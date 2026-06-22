#ifndef DISPLAY_H
#define DISPLAY_H

#include <sys/mman.h>
#include "simulation.h"

/*
 * DisplayState lives in mmap(MAP_SHARED | MAP_ANONYMOUS).
 * This means every forked process (source, middle, sink, display child)
 * sees the SAME memory — no extra pipes or IPC needed for visualization.
 *
 * Writers  : parent (round/wins/done),  source/middle/sink (piece position)
 * Reader   : display child (OpenGL window)
 */
typedef struct {
    /* Competition-level info — written by parent */
    int round;
    int wins[NUM_TEAMS];
    int num_members;
    int num_pieces;
    int done;        /* 1 = competition finished               */
    int champion;    /* winning team index                     */

    /* Per-team per-round info — written by simulation members */
    int placed       [NUM_TEAMS]; /* pieces placed this round           */
    int active_member[NUM_TEAMS]; /* which member holds the piece (-1=none) */
    int direction    [NUM_TEAMS]; /* +1 = forward,  -1 = backward       */
    int piece_id     [NUM_TEAMS]; /* ID of piece currently in motion     */
} DisplayState;

/* Allocate shared state in anonymous shared memory */
DisplayState *create_display_state(void);

/* Entry point for the display child process (never returns) */
void run_display(DisplayState *ds);

#endif /* DISPLAY_H */
