#ifndef MEMBER_H
#define MEMBER_H

#include "display.h"   /* for DisplayState */

/*
 * Everything a forked member process needs to know about itself.
 * The parent fills this struct before each fork(); the child receives
 * its own copy via the normal copy-on-write semantics.
 */
typedef struct {
    int  team_id;       /* 0 = Team A,  1 = Team B              */
    int  member_idx;    /* 0 = source,  num_members-1 = sink    */
    int  num_members;   /* total members in this team           */
    int  num_pieces;    /* furniture pieces this round          */

    int *piece_ids;     /* all piece IDs (source uses this)     */
    int *sorted_ids;    /* sorted IDs = expected placement order (sink uses this) */

    int  min_delay_ms;
    int  max_delay_ms;

    /*
     * Pipe file descriptors.
     * Set to -1 when the role doesn't use that end.
     *
     *   fwd_in  – read: piece arriving from the previous member
     *   fwd_out – write: piece going to the next member
     *   bwd_in  – read: response arriving from the next member
     *   bwd_out – write: response going to the previous member
     *   result_fd – write: sink notifies parent of round win
     */
    int  fwd_in;
    int  fwd_out;
    int  bwd_in;
    int  bwd_out;
    int  result_fd;

    /*
     * Shared display state (mmap MAP_SHARED).
     * Members write their current position here so the
     * OpenGL window can animate the piece in real time.
     * NULL = no display (simulation still works fine).
     */
    DisplayState *display;
} MemberArgs;

/* Dispatch to source / middle / sink depending on member_idx */
void run_member(const MemberArgs *args);

#endif /* MEMBER_H */
