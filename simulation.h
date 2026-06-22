#ifndef SIMULATION_H
#define SIMULATION_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <sys/wait.h>

/* ── Limits ─────────────────────────────────────────────────── */
#define NUM_TEAMS    2
#define MAX_MEMBERS  50
#define MAX_PIECES   10000

/* ── Message types sent through pipes ───────────────────────── */
#define MSG_FORWARD  1   /* piece moving  source  ──►  sink   */
#define MSG_BOUNCE   2   /* piece rejected, going sink ──► source */
#define MSG_SUCCESS  3   /* piece accepted, notify travels back  */
#define MSG_DONE     4   /* all pieces placed – team wins round  */

/* ── Pipe-end indices ────────────────────────────────────────── */
#define READ_END   0
#define WRITE_END  1

/* ── The only data structure that travels through pipes ─────── */
typedef struct {
    int type;      /* one of MSG_* above          */
    int piece_id;  /* furniture serial number      */
} Message;

/* ── Atomic pipe helpers ────────────────────────────────────── */
static inline int send_msg(int fd, int type, int piece_id) {
    Message m = { type, piece_id };
    return (write(fd, &m, sizeof m) == (ssize_t)sizeof m) ? 0 : -1;
}

static inline int recv_msg(int fd, Message *m) {
    return (read(fd, m, sizeof *m) == (ssize_t)sizeof *m) ? 0 : -1;
}

#endif /* SIMULATION_H */
