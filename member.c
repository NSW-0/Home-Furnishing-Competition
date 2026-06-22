#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include "simulation.h"
#include "member.h"

/* ── Helpers ─────────────────────────────────────────────────── */

static void sleep_ms(int ms) {
    if (ms <= 0) return;
    struct timespec ts;
    ts.tv_sec  =  ms / 1000;
    ts.tv_nsec = (ms % 1000) * 1000000L;
    nanosleep(&ts, NULL);
}

static int compute_delay(int min_ms, int max_ms, int pieces_moved) {
    int range = max_ms - min_ms;
    int base  = (range > 0) ? min_ms + rand() % range : min_ms;
    double mult = 1.0 + 0.10 * (pieces_moved / 10.0);
    return (int)(base * mult);
}

/* Shorthand: update display field only if display pointer exists */
#define DS_SET(field, val) \
    do { if (a->display) a->display->field = (val); } while(0)

/* ── SOURCE (member_idx == 0) ────────────────────────────────── */
static void run_source(const MemberArgs *a) {
    int  N    = a->num_pieces;
    int  t    = a->team_id;
    char team = (t == 0) ? 'A' : 'B';

    int placed [MAX_PIECES];
    int blocked[MAX_PIECES];
    memset(placed,  0, N * sizeof(int));
    memset(blocked, 0, N * sizeof(int));

    int num_placed  = 0;
    int total_moved = 0;

    srand((unsigned)(time(NULL) ^ ((unsigned long)getpid() << 3)));

    printf("[Team %c | Source   ] Started – managing %d pieces.\n", team, N);
    fflush(stdout);

    while (num_placed < N) {
        int cand[MAX_PIECES];
        int nc = 0;
        for (int i = 0; i < N; i++)
            if (!placed[i] && !blocked[i])
                cand[nc++] = i;

        if (nc == 0) {
            printf("[Team %c | Source   ] Warning: all blocked – force unblock.\n", team);
            fflush(stdout);
            for (int i = 0; i < N; i++)
                if (!placed[i]) blocked[i] = 0;
            continue;
        }

        int idx      = cand[rand() % nc];
        int piece_id = a->piece_ids[idx];
        int delay    = compute_delay(a->min_delay_ms, a->max_delay_ms, total_moved);

        sleep_ms(delay);

        /* Tell display: source is sending this piece */
        DS_SET(active_member[t], 0);
        DS_SET(direction[t],     1);
        DS_SET(piece_id[t],      piece_id);

        if (send_msg(a->fwd_out, MSG_FORWARD, piece_id) < 0) break;

        printf("[Team %c | Source   ] >> Sent   piece #%4d  (delay=%dms)\n",
               team, piece_id, delay);
        fflush(stdout);

        Message resp;
        if (recv_msg(a->bwd_in, &resp) < 0) break;

        if (resp.type == MSG_SUCCESS) {
            placed[idx] = 1;
            num_placed++;
            memset(blocked, 0, N * sizeof(int));
            printf("[Team %c | Source   ] << PLACED  piece #%4d  (%d/%d done)\n",
                   team, piece_id, num_placed, N);
        } else {
            blocked[idx] = 1;
            printf("[Team %c | Source   ] << BLOCKED piece #%4d\n", team, piece_id);
        }

        /* Piece is back at source — hide from display */
        DS_SET(active_member[t], -1);

        fflush(stdout);
        total_moved++;
    }

    printf("[Team %c | Source   ] All pieces delivered. Exiting.\n", team);
    fflush(stdout);
}

/* ── MIDDLE MEMBER (0 < member_idx < num_members-1) ─────────── */
static void run_middle(const MemberArgs *a) {
    int  total_moved = 0;
    int  t           = a->team_id;
    char team        = (t == 0) ? 'A' : 'B';

    srand((unsigned)(time(NULL) ^ ((unsigned long)getpid() << 3)));

    printf("[Team %c | Member %2d] Started.\n", team, a->member_idx);
    fflush(stdout);

    while (1) {
        Message msg;

        /* Forward leg */
        if (recv_msg(a->fwd_in, &msg) < 0) break;

        int delay = compute_delay(a->min_delay_ms, a->max_delay_ms, total_moved);

        DS_SET(active_member[t], a->member_idx);
        DS_SET(direction[t],     1);
        DS_SET(piece_id[t],      msg.piece_id);

        sleep_ms(delay);
        if (send_msg(a->fwd_out, msg.type, msg.piece_id) < 0) break;

        printf("[Team %c | Member %2d] >> #%4d forward   (delay=%dms)\n",
               team, a->member_idx, msg.piece_id, delay);
        fflush(stdout);

        /* Backward leg */
        if (recv_msg(a->bwd_in, &msg) < 0) break;

        DS_SET(active_member[t], a->member_idx);
        DS_SET(direction[t],     -1);

        sleep_ms(delay / 2);
        if (send_msg(a->bwd_out, msg.type, msg.piece_id) < 0) break;

        printf("[Team %c | Member %2d] << #%4d %-8s\n",
               team, a->member_idx, msg.piece_id,
               (msg.type == MSG_SUCCESS) ? "SUCCESS" : "BOUNCE");
        fflush(stdout);

        total_moved++;
    }

    printf("[Team %c | Member %2d] Pipe closed – exiting.\n", team, a->member_idx);
    fflush(stdout);
}

/* ── SINK (member_idx == num_members-1) ─────────────────────── */
static void run_sink(const MemberArgs *a) {
    int  *order = a->sorted_ids;
    int   N     = a->num_pieces;
    int   next  = 0;
    int   t     = a->team_id;
    char  team  = (t == 0) ? 'A' : 'B';

    printf("[Team %c | Sink     ] Started – first expected: #%d.\n",
           team, order[0]);
    fflush(stdout);

    while (next < N) {
        Message msg;
        if (recv_msg(a->fwd_in, &msg) < 0) break;

        DS_SET(active_member[t], a->member_idx);
        DS_SET(piece_id[t],      msg.piece_id);

        printf("[Team %c | Sink     ]    Received #%4d  (want #%4d) → ",
               team, msg.piece_id, order[next]);
        fflush(stdout);

        if (msg.piece_id == order[next]) {
            next++;
            DS_SET(direction[t], 1);
            if (a->display) a->display->placed[t]++;
            send_msg(a->bwd_out, MSG_SUCCESS, msg.piece_id);
            printf("PLACED  (%d/%d)\n", next, N);
        } else {
            DS_SET(direction[t], -1);
            send_msg(a->bwd_out, MSG_BOUNCE, msg.piece_id);
            printf("REJECTED\n");
        }
        fflush(stdout);
    }

    send_msg(a->result_fd, MSG_DONE, 0);
    printf("[Team %c | Sink     ] *** ROUND WON – all %d pieces placed! ***\n",
           team, N);
    fflush(stdout);
}

/* ── Entry point ─────────────────────────────────────────────── */
void run_member(const MemberArgs *a) {
    signal(SIGPIPE, SIG_IGN);

    if      (a->member_idx == 0)
        run_source(a);
    else if (a->member_idx == a->num_members - 1)
        run_sink(a);
    else
        run_middle(a);
}
