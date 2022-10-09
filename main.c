#include "server.h"
#include "model/player.h"
#include "utils/log.h"

#include <stdlib.h>

int test_players() {
    struct Players *players = malloc(sizeof(struct Players));
    update_players(players, "James", 0);
    update_players(players, "Jake", 1);
    update_players(players, "Annie", 1); // this will pass
    update_players(players, "James", 2); // wont be added
    print_players(players);
    free_players(&players);
    return 0;
}

void setup_logger() {
    log_set_level(LOG_TRACE); // default setting (all -> stderr)
    FILE *fp = fopen("./server.log", "a+");
    if (fp == NULL) {
        fprintf(stderr, "couldn't open log file\n");
    } else {
        log_add_fp(fp, LOG_INFO);
    }
}

int main() {
    setup_logger();
    start();
    return 0;
}
