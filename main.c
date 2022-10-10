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

/**
 * <pre>
 * LOG_TRACE - detailed application flow - usually no need to log <br><br>
 * LOG_DEBUG - application flow that is worth logging for future debugging (slightly more important trace) <br><br>
 * LOG_INFO  - basic logging info - logs current state of server <br><br>
 * LOG_WARN  - server warnings - usually recv/send errors <br><br>
 * LOG_ERROR - server/client errors - application keeps running, but this problem should be further investigated <br><br>
 * LOG_FATAL - server fatal error - server down <br><br>
 * </pre>
 */
void setup_logger() {
    log_set_level(LOG_TRACE); // default setting (all -> stderr)
    FILE *fp = fopen("./server.log", "a+");
    if (fp == NULL) {
        fprintf(stderr, "couldn't open log file\n");
    } else {
        log_add_fp(fp, LOG_TRACE); // TODO: set to LOG_INFO or LOG_DEBUG
    }
}

int main() {
    setup_logger();
    start();
    return 0;
}
