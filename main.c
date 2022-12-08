#include "server.h"
#include "model/player.h"
#include "utils/log.h"
#include "model/word_generator.h"

#include <stdlib.h>

#include <stdio.h>
#include <execinfo.h>
#include <signal.h>
#include <unistd.h>

/**
 * <pre>
 * LOG_TRACE - detailed application flow - usually no need to log <br><br>
 * LOG_DEBUG - application flow that is worth logging for future debugging<br>
 *             (helpful to people more than just developers (IT, sysadmins, etc.))<br><br>
 * LOG_INFO  - basic logging info - logs current state of server <br><br>
 * LOG_WARN  - server warnings - usually recv/send errors/hangups <br><br>
 * LOG_ERROR - server/client errors - application keeps running, but this problem should be further investigated <br><br>
 * LOG_FATAL - server fatal error - server down <br><br>
 * </pre>
 */
void setup_logger() {
    log_set_level(LOG_TRACE); // default setting (all -> stderr)
    FILE *fp = fopen("./server.log", "a+");
    if (fp == NULL) {
        fprintf(stderr, "couldn't open log file - logging off\n");
    } else {
        log_add_fp(fp, LOG_DEBUG);
    }
}

// argv = [<application_path>, port, ip_address, lobby_size, game_size]

void handler(int sig) {
    void *array[10];
    size_t size;

    // get void*'s for all entries on the stack
    size = backtrace(array, 10);

    // print out all the frames to stderr
    fprintf(stderr, "Error: signal %d:\n", sig);
    backtrace_symbols_fd(array, (int) size, STDERR_FILENO);
    exit(1);
}

int main(int argc, char *argv[]) {
    signal(SIGSEGV, handler);
    fprintf(stderr, "%s\n", argv[0]);

    setup_logger();
    read_words("./resources/words.txt");
    start();
    return 0;
}
