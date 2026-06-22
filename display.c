#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <sys/mman.h>
#include <GL/glut.h>
#include "display.h"

/* ── Globals ─────────────────────────────────────────────────── */
static DisplayState *g_ds          = NULL;
static float         g_anim[NUM_TEAMS]; /* smooth animated piece position */
static int           g_end_ticks   = 0; /* countdown after competition ends */

/* ── Low-level draw helpers ──────────────────────────────────── */

static void quad(float x, float y, float w, float h) {
    glBegin(GL_QUADS);
      glVertex2f(x,   y);    glVertex2f(x+w, y);
      glVertex2f(x+w, y+h);  glVertex2f(x,   y+h);
    glEnd();
}

static void rect_border(float x, float y, float w, float h, float lw) {
    glLineWidth(lw);
    glBegin(GL_LINE_LOOP);
      glVertex2f(x,   y);    glVertex2f(x+w, y);
      glVertex2f(x+w, y+h);  glVertex2f(x,   y+h);
    glEnd();
}

static void circle(float cx, float cy, float r) {
    glBegin(GL_TRIANGLE_FAN);
      glVertex2f(cx, cy);
      for (int i = 0; i <= 28; i++) {
          float a = (float)i * 2.0f * 3.14159265f / 28.0f;
          glVertex2f(cx + r * cosf(a), cy + r * sinf(a));
      }
    glEnd();
}

static void hline(float x1, float x2, float y) {
    glBegin(GL_LINES);
      glVertex2f(x1, y);
      glVertex2f(x2, y);
    glEnd();
}

static void str12(float x, float y, const char *s) {
    glRasterPos2f(x, y);
    for (; *s; s++) glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, (int)*s);
}

static void str18(float x, float y, const char *s) {
    glRasterPos2f(x, y);
    for (; *s; s++) glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, (int)*s);
}

static void str24(float x, float y, const char *s) {
    glRasterPos2f(x, y);
    for (; *s; s++) glutBitmapCharacter(GLUT_BITMAP_TIMES_ROMAN_24, (int)*s);
}

/* ── Layout helpers ──────────────────────────────────────────── */

/* X centre of member[i] within the row range [left..right] */
static float mx(int i, int N, float left, float right) {
    if (N <= 1) return (left + right) * 0.5f;
    return left + (float)i * (right - left) / (float)(N - 1);
}

/* ── Draw one team's full row ────────────────────────────────── */

static void draw_team_row(int t, float row_y) {
    int   N  = g_ds->num_members;
    int   NP = g_ds->num_pieces;
    char  tc = (t == 0) ? 'A' : 'B';

    /* Team colour */
    float cr = (t == 0) ? 0.22f : 0.35f;
    float cg = (t == 0) ? 0.72f : 0.58f;
    float cb = (t == 0) ? 0.56f : 0.92f;

    float left  = 140.0f;
    float right = 820.0f;
    float bw = 64.0f, bh = 46.0f;  /* member box dimensions */
    float mid_y = row_y + bh * 0.5f;

    /* ── Team label ── */
    glColor3f(cr, cg, cb);
    char lbl[16];
    snprintf(lbl, sizeof lbl, "TEAM %c", tc);
    str18(12.0f, row_y + 12.0f, lbl);

    /* ── Connector lines between boxes ── */
    glColor3f(0.28f, 0.28f, 0.36f);
    glLineWidth(1.5f);
    for (int i = 0; i < N - 1; i++) {
        float x1 = mx(i,   N, left, right) + bw * 0.5f + 2;
        float x2 = mx(i+1, N, left, right) - bw * 0.5f - 2;
        hline(x1, x2, mid_y);
    }

    /* ── Member boxes ── */
    for (int i = 0; i < N; i++) {
        float bx = mx(i, N, left, right) - bw * 0.5f;
        float by = row_y;
        int   active = (g_ds->active_member[t] == i);

        /* Box fill */
        glColor3f(active ? 0.26f : 0.14f,
                  active ? 0.27f : 0.14f,
                  active ? 0.36f : 0.20f);
        quad(bx, by, bw, bh);

        /* Box border */
        if (active) {
            glColor3f(cr, cg, cb);
            rect_border(bx, by, bw, bh, 2.0f);
        } else {
            glColor3f(0.30f, 0.30f, 0.40f);
            rect_border(bx, by, bw, bh, 1.0f);
        }

        /* Box label */
        glColor3f(active ? 0.95f : 0.55f,
                  active ? 0.95f : 0.55f,
                  active ? 0.98f : 0.60f);
        char ml[16];
        if      (i == 0)   snprintf(ml, sizeof ml, "Source");
        else if (i == N-1) snprintf(ml, sizeof ml, "Sink");
        else               snprintf(ml, sizeof ml, "M%d", i);
        str12(bx + 5.0f, by + 17.0f, ml);
    }

    /* ── Animated piece ── */
    if (g_ds->active_member[t] >= 0) {
        float px = mx(0, N, left, right)
                   + g_anim[t] * (mx(N-1, N, left, right) - mx(0, N, left, right))
                                / (float)(N > 1 ? N - 1 : 1);
        float py = mid_y;

        /* Drop shadow */
        glColor3f(0.04f, 0.04f, 0.06f);
        circle(px + 2, py - 2, 12.0f);

        /* Piece: yellow = forward, orange-red = bounce */
        if (g_ds->direction[t] >= 0)
            glColor3f(1.00f, 0.85f, 0.12f);   /* forward  */
        else
            glColor3f(0.95f, 0.38f, 0.18f);   /* bouncing */
        circle(px, py, 12.0f);

        /* Piece ID */
        glColor3f(0.08f, 0.08f, 0.10f);
        char pid[8];
        snprintf(pid, sizeof pid, "%d", g_ds->piece_id[t]);
        str12(px - (int)strlen(pid) * 3.5f, py - 4.0f, pid);
    }

    /* ── Progress bar ── */
    float pbx = left, pby = row_y - 24.0f, pbw = right - left, pbh = 11.0f;

    glColor3f(0.14f, 0.14f, 0.20f);
    quad(pbx, pby, pbw, pbh);

    if (NP > 0) {
        float fill = (float)g_ds->placed[t] / (float)NP;
        glColor3f(cr * 0.70f, cg * 0.70f, cb * 0.70f);
        quad(pbx, pby, pbw * fill, pbh);
    }

    glColor3f(0.36f, 0.36f, 0.46f);
    rect_border(pbx, pby, pbw, pbh, 1.0f);

    glColor3f(0.62f, 0.62f, 0.68f);
    char prog[32];
    snprintf(prog, sizeof prog, "%d / %d pieces", g_ds->placed[t], NP);
    str12(right + 10.0f, pby + 1.0f, prog);
}

/* ── OpenGL display callback ─────────────────────────────────── */

static void on_display(void) {
    int W = glutGet(GLUT_WINDOW_WIDTH);
    int H = glutGet(GLUT_WINDOW_HEIGHT);

    glClear(GL_COLOR_BUFFER_BIT);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, W, 0, H, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    /* Background */
    glColor3f(0.10f, 0.10f, 0.14f);
    quad(0, 0, W, H);

    /* ── Competition-over screen ── */
    if (g_ds->done) {
        glColor3f(1.00f, 0.85f, 0.12f);
        str24(W/2 - 155, H/2 + 60, "COMPETITION  OVER!");

        char msg[64];
        snprintf(msg, sizeof msg, "Champion :  Team %c",
                 g_ds->champion == 0 ? 'A' : 'B');
        glColor3f(0.25f, 0.90f, 0.42f);
        str24(W/2 - 105, H/2 + 10, msg);

        snprintf(msg, sizeof msg, "Final Score   A: %d      B: %d",
                 g_ds->wins[0], g_ds->wins[1]);
        glColor3f(0.70f, 0.70f, 0.75f);
        str18(W/2 - 110, H/2 - 38, msg);

        glutSwapBuffers();
        return;
    }

    /* ── Header ── */
    glColor3f(0.86f, 0.86f, 0.90f);
    str24(W/2 - 190, H - 30, "HOME FURNISHING COMPETITION");

    char hdr[80];
    snprintf(hdr, sizeof hdr,
             "Round %d          A: %d wins          B: %d wins",
             g_ds->round + 1, g_ds->wins[0], g_ds->wins[1]);
    glColor3f(0.50f, 0.50f, 0.58f);
    str18(W/2 - 175, H - 56, hdr);

    /* Header separator */
    glColor3f(0.20f, 0.20f, 0.28f);
    glLineWidth(1.0f);
    hline(20, W - 20, H - 66);

    /* ── Team A row (upper half) ── */
    draw_team_row(0, (float)H * 0.58f);

    /* Mid separator */
    glColor3f(0.20f, 0.20f, 0.28f);
    hline(20, W - 20, (float)H * 0.46f);

    /* ── Team B row (lower half) ── */
    draw_team_row(1, (float)H * 0.25f);

    /* ── Footer legend ── */
    glColor3f(0.32f, 0.32f, 0.40f);
    str12(20, 8,
          "  Yellow circle = piece going forward      "
          "Red circle = piece bouncing back");

    glutSwapBuffers();
}

/* ── Timer: smooth animation + redraw ───────────────────────── */

static void on_timer(int v) {
    (void)v;

    /* Ease animated position toward the current active member index */
    for (int t = 0; t < NUM_TEAMS; t++) {
        int tgt = g_ds->active_member[t];
        if (tgt < 0) tgt = 0;
        g_anim[t] += ((float)tgt - g_anim[t]) * 0.16f;
    }

    glutPostRedisplay();

    if (g_ds->done) {
        if (++g_end_ticks > 250)   /* ~250 × 50 ms ≈ 12 s then auto-close */
            exit(0);
        glutTimerFunc(50, on_timer, 0);
    } else {
        glutTimerFunc(33, on_timer, 0);  /* ~30 fps */
    }
}

/* ── Public API ──────────────────────────────────────────────── */

DisplayState *create_display_state(void) {
    DisplayState *ds = mmap(NULL, sizeof *ds,
                             PROT_READ | PROT_WRITE,
                             MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (ds == MAP_FAILED) { perror("mmap display_state"); return NULL; }
    memset(ds, 0, sizeof *ds);
    ds->active_member[0] = -1;
    ds->active_member[1] = -1;
    return ds;
}

void run_display(DisplayState *ds) {
    g_ds       = ds;
    g_anim[0]  = 0.0f;
    g_anim[1]  = 0.0f;

    int argc = 0;
    glutInit(&argc, NULL);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB);
    glutInitWindowSize(920, 530);
    glutInitWindowPosition(60, 60);
    glutCreateWindow("Home Furnishing Competition");

    glClearColor(0.10f, 0.10f, 0.14f, 1.0f);

    glutDisplayFunc(on_display);
    glutTimerFunc(33, on_timer, 0);

    glutMainLoop();   /* never returns */
}
