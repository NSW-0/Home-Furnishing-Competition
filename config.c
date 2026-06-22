#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "config.h"

/* ── Defaults (used if a key is absent from the file) ───────── */
static void set_defaults(Config *c) {
    c->num_members  = 4;
    c->num_pieces   = 20;
    c->min_delay_ms = 100;
    c->max_delay_ms = 500;
    c->wins_needed  = 3;
    c->random_ids   = 1;
}

/*
 * Parse a simple "key = value" text file.
 * Lines starting with '#' or blank lines are ignored.
 */
int load_config(const char *path, Config *cfg) {
    set_defaults(cfg);

    FILE *f = fopen(path, "r");
    if (!f) { perror("load_config: fopen"); return -1; }

    char line[256], key[64], val[64];
    while (fgets(line, sizeof line, f)) {
        if (line[0] == '#' || line[0] == '\n') continue;
        /* accept both "key=val" and "key = val" */
        int matched =
            (sscanf(line, " %63[^= \t] = %63s", key, val) == 2) ||
            (sscanf(line, " %63[^=]=%63s",       key, val) == 2);
        if (!matched) continue;

        if      (!strcmp(key, "num_members"))  cfg->num_members  = atoi(val);
        else if (!strcmp(key, "num_pieces"))   cfg->num_pieces   = atoi(val);
        else if (!strcmp(key, "min_delay_ms")) cfg->min_delay_ms = atoi(val);
        else if (!strcmp(key, "max_delay_ms")) cfg->max_delay_ms = atoi(val);
        else if (!strcmp(key, "wins_needed"))  cfg->wins_needed  = atoi(val);
        else if (!strcmp(key, "random_ids"))   cfg->random_ids   = atoi(val);
    }
    fclose(f);
    return 0;
}

void print_config(const Config *cfg) {
    printf("┌──────────────────────────────┐\n");
    printf("│  Configuration               │\n");
    printf("├──────────────────────────────┤\n");
    printf("│  members per team : %-8d │\n", cfg->num_members);
    printf("│  furniture pieces : %-8d │\n", cfg->num_pieces);
    printf("│  delay min   (ms) : %-8d │\n", cfg->min_delay_ms);
    printf("│  delay max   (ms) : %-8d │\n", cfg->max_delay_ms);
    printf("│  wins needed      : %-8d │\n", cfg->wins_needed);
    printf("│  random IDs       : %-8s │\n", cfg->random_ids ? "yes" : "no");
    printf("└──────────────────────────────┘\n\n");
}
