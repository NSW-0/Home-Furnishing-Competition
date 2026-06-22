#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/select.h>
#include <omp.h>

#include "simulation.h"
#include "config.h"
#include "member.h"
#include "display.h"

/* ── Global piece arrays (copy-on-write after fork) ─────────── */
static int g_pieces[NUM_TEAMS][MAX_PIECES];
static int g_sorted[NUM_TEAMS][MAX_PIECES];

static int cmp_int(const void *a, const void *b) {
    return *(const int *)a - *(const int *)b;
}

/* Generate piece IDs for both teams in parallel using OpenMP */
static void generate_pieces(const Config *cfg, int round) {
    int N = cfg->num_pieces;

    #pragma omp parallel for num_threads(2) schedule(static,1)
    for (int t = 0; t < NUM_TEAMS; t++) {
        unsigned int seed = (unsigned)time(NULL)
                          ^ (unsigned)(t * 997u)
                          ^ (unsigned)(round * 31337u);

        if (cfg->random_ids) {
            int pool_size = N * 5;
            int *pool = malloc((size_t)pool_size * sizeof(int));
            if (!pool) { perror("malloc"); exit(1); }
            for (int k = 0; k < pool_size; k++) pool[k] = k + 1;
            /* Fisher-Yates shuffle */
            for (int k = pool_size - 1; k > 0; k--) {
                int j = (int)(rand_r(&seed) % (unsigned)(k + 1));
                int tmp = pool[k]; pool[k] = pool[j]; pool[j] = tmp;
            }
            for (int k = 0; k < N; k++) {
                g_pieces[t][k] = pool[k];
                g_sorted[t][k] = pool[k];
            }
            free(pool);
        } else {
            for (int k = 0; k < N; k++) {
                g_pieces[t][k] = k + 1;
                g_sorted[t][k] = k + 1;
            }
        }
        qsort(g_sorted[t], N, sizeof(int), cmp_int);
    }
}

/* ── run_round() ─────────────────────────────────────────────── */
static int run_round(const Config *cfg, DisplayState *ds) {
    int N  = cfg->num_members;
    int NP = cfg->num_pieces;

    int fwd[NUM_TEAMS][MAX_MEMBERS][2];
    int bwd[NUM_TEAMS][MAX_MEMBERS][2];
    int res[NUM_TEAMS][2];

    for (int t = 0; t < NUM_TEAMS; t++) {
        for (int i = 0; i < N - 1; i++) {
            if (pipe(fwd[t][i]) < 0 || pipe(bwd[t][i]) < 0) {
                perror("pipe"); return -1;
            }
        }
        if (pipe(res[t]) < 0) { perror("pipe res"); return -1; }
    }

    /* Flat list of every pipe fd (for child cleanup) */
    int all_fds[NUM_TEAMS * MAX_MEMBERS * 4 + NUM_TEAMS * 2 + 4];
    int n_all = 0;
    for (int t = 0; t < NUM_TEAMS; t++) {
        for (int i = 0; i < N - 1; i++) {
            all_fds[n_all++] = fwd[t][i][READ_END];
            all_fds[n_all++] = fwd[t][i][WRITE_END];
            all_fds[n_all++] = bwd[t][i][READ_END];
            all_fds[n_all++] = bwd[t][i][WRITE_END];
        }
        all_fds[n_all++] = res[t][READ_END];
        all_fds[n_all++] = res[t][WRITE_END];
    }

    pid_t pids[NUM_TEAMS][MAX_MEMBERS];

    for (int t = 0; t < NUM_TEAMS; t++) {
        for (int i = 0; i < N; i++) {
            pid_t pid = fork();
            if (pid < 0) { perror("fork"); return -1; }

            if (pid == 0) {
                /* ── CHILD ── */
                MemberArgs args;
                args.team_id      = t;
                args.member_idx   = i;
                args.num_members  = N;
                args.num_pieces   = NP;
                args.piece_ids    = g_pieces[t];
                args.sorted_ids   = g_sorted[t];
                args.min_delay_ms = cfg->min_delay_ms;
                args.max_delay_ms = cfg->max_delay_ms;
                args.display      = ds;   /* shared mmap — same address after fork */

                args.fwd_in    = (i > 0)    ? fwd[t][i-1][READ_END]  : -1;
                args.fwd_out   = (i < N-1)  ? fwd[t][i  ][WRITE_END] : -1;
                args.bwd_in    = (i < N-1)  ? bwd[t][i  ][READ_END]  : -1;
                args.bwd_out   = (i > 0)    ? bwd[t][i-1][WRITE_END] : -1;
                args.result_fd = (i == N-1) ? res[t][WRITE_END]      : -1;

                /* Close every pipe fd this member doesn't own */
                int keep[5] = {
                    args.fwd_in, args.fwd_out,
                    args.bwd_in, args.bwd_out,
                    args.result_fd
                };
                for (int k = 0; k < n_all; k++) {
                    int fd = all_fds[k];
                    if (fd < 0) continue;
                    int owned = 0;
                    for (int j = 0; j < 5; j++)
                        if (fd == keep[j]) { owned = 1; break; }
                    if (!owned) close(fd);
                }

                run_member(&args);
                exit(0);
            }

            pids[t][i] = pid;
        }
    }

    /* Parent: close all pipe fds except result read-ends */
    for (int k = 0; k < n_all; k++) {
        int fd = all_fds[k];
        if (fd == res[0][READ_END] || fd == res[1][READ_END]) continue;
        if (fd >= 0) close(fd);
    }

    /* Wait for the first team to finish */
    int winner = -1;
    while (winner == -1) {
        fd_set rfds;
        FD_ZERO(&rfds);
        int maxfd = -1;
        for (int t = 0; t < NUM_TEAMS; t++) {
            if (res[t][READ_END] >= 0) {
                FD_SET(res[t][READ_END], &rfds);
                if (res[t][READ_END] > maxfd) maxfd = res[t][READ_END];
            }
        }
        if (maxfd < 0) break;
        if (select(maxfd + 1, &rfds, NULL, NULL, NULL) < 0) break;

        for (int t = 0; t < NUM_TEAMS; t++) {
            if (res[t][READ_END] >= 0 &&
                FD_ISSET(res[t][READ_END], &rfds)) {
                Message m;
                if (recv_msg(res[t][READ_END], &m) == 0 && m.type == MSG_DONE) {
                    winner = t;
                    break;
                }
            }
        }
    }

    close(res[0][READ_END]);
    close(res[1][READ_END]);

    /* Kill and reap all simulation children */
    for (int t = 0; t < NUM_TEAMS; t++)
        for (int i = 0; i < N; i++)
            kill(pids[t][i], SIGTERM);

    for (int t = 0; t < NUM_TEAMS; t++)
        for (int i = 0; i < N; i++)
            waitpid(pids[t][i], NULL, 0);

    return winner;
}

/* ── main ────────────────────────────────────────────────────── */
int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <config_file>\n", argv[0]);
        return 1;
    }

    Config cfg;
    if (load_config(argv[1], &cfg) < 0) return 1;
    print_config(&cfg);

    if (cfg.num_members < 2) {
        fprintf(stderr, "Error: num_members must be >= 2\n"); return 1;
    }

    /* ── Create shared display state ── */
    DisplayState *ds = create_display_state();
    if (ds) {
        ds->num_members = cfg.num_members;
        ds->num_pieces  = cfg.num_pieces;
    }

    /* ── Fork the OpenGL display child ── */
    pid_t display_pid = -1;
    if (ds) {
        display_pid = fork();
        if (display_pid == 0) {
            /* Display child: opens GLUT window and loops forever */
            run_display(ds);
            exit(0);
        }
        /* Give the window a moment to appear */
        usleep(400000);
    }

    int wins[NUM_TEAMS] = {0, 0};
    int round = 0;

    printf("\n═══════════════════════════════════\n");
    printf("   HOME FURNISHING COMPETITION\n");
    printf("═══════════════════════════════════\n\n");

    while (wins[0] < cfg.wins_needed && wins[1] < cfg.wins_needed) {

        double t_start = omp_get_wtime();

        printf("───── Round %d ─────────────────────\n", round + 1);

        generate_pieces(&cfg, round);

        /* Reset display state for new round */
        if (ds) {
            ds->round             = round;
            ds->placed[0]         = 0;
            ds->placed[1]         = 0;
            ds->active_member[0]  = -1;
            ds->active_member[1]  = -1;
            ds->direction[0]      = 1;
            ds->direction[1]      = 1;
        }

        for (int t = 0; t < NUM_TEAMS; t++) {
            int show = (cfg.num_pieces < 10) ? cfg.num_pieces : 10;
            printf("[Round %d] Team %c order: ", round+1, t==0?'A':'B');
            for (int k = 0; k < show; k++) printf("%d ", g_sorted[t][k]);
            if (cfg.num_pieces > 10) printf("...");
            printf("\n");
        }
        fflush(stdout);

        int winner = run_round(&cfg, ds);

        double elapsed = omp_get_wtime() - t_start;

        if (winner >= 0) {
            wins[winner]++;
            if (ds) ds->wins[winner] = wins[winner];

            printf("\n  ★  Round %d winner: Team %c  (%.2f s)\n",
                   round + 1, winner == 0 ? 'A' : 'B', elapsed);
            printf("     Score →  A: %d   B: %d\n\n", wins[0], wins[1]);
        }
        fflush(stdout);
        round++;
    }

    int champion = (wins[0] >= cfg.wins_needed) ? 0 : 1;

    /* Tell display the competition is over */
    if (ds) {
        ds->done     = 1;
        ds->champion = champion;
    }

    printf("═══════════════════════════════════\n");
    printf("  COMPETITION OVER\n");
    printf("  Champion : Team %c\n", champion == 0 ? 'A' : 'B');
    printf("  Score    : A=%d  B=%d\n", wins[0], wins[1]);
    printf("═══════════════════════════════════\n");

    /* Let the display window show the result for a few seconds, then clean up */
    if (display_pid > 0) {
        sleep(12);
        kill(display_pid, SIGTERM);
        waitpid(display_pid, NULL, 0);
    }

    return 0;
}
