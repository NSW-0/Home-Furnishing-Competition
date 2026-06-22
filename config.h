#ifndef CONFIG_H
#define CONFIG_H

typedef struct {
    int num_members;   /* members per team  (must be >= 2)     */
    int num_pieces;    /* furniture pieces per team             */
    int min_delay_ms;  /* minimum per-member delay  (ms)       */
    int max_delay_ms;  /* maximum per-member delay  (ms)       */
    int wins_needed;   /* rounds to win to end competition     */
    int random_ids;    /* 1 = random serials, 0 = sequential   */
} Config;

int  load_config(const char *path, Config *cfg);
void print_config(const Config *cfg);

#endif /* CONFIG_H */
