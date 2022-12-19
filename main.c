#include "server.h"
#include "utils/log.h"
#include "model/word_generator.h"

#include <stdlib.h>
#include <stdio.h>
#include <execinfo.h>
#include <signal.h>
#include <unistd.h>

// usage: <executable> hostname port lobby_size game_size
#define ARGUMENT_COUNT 5
#define WORDS_RESOURCES "./resources/words.txt"

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

void setup_signals() {
    signal(SIGSEGV, handler);
}


int handle_arguments(int argc, char *argv[], int *lobby_size, int *game_size) {
    if (argc != ARGUMENT_COUNT) {
        fprintf(stderr, "usage: <executable> hostname port lobby_size game_size\n");
        return EXIT_FAILURE;
    }

    *lobby_size = (int) strtol(argv[3], (char **) NULL, 10);
    *game_size = (int) strtol(argv[4], (char **) NULL, 10);

    if (lobby_size <= 0)
        fprintf(stderr, "invalid lobby size\n");
    else if (game_size <= 0)
        fprintf(stderr, "invalid game size\n");

    log_info("lobby size set to %d, game size set to %d", *lobby_size, *game_size);
    return EXIT_SUCCESS;
}

int main(int argc, char *argv[]) {
    char *addr = argv[1], *port = argv[2];
    int lobby_size, game_size;

    if (handle_arguments(argc, argv, &lobby_size, &game_size) != EXIT_SUCCESS)
        return EXIT_FAILURE;

    setup_signals();
    setup_logger();
    struct Words *words = read_words(WORDS_RESOURCES);
    start(addr, port, lobby_size, game_size, words);
    return 0;
}