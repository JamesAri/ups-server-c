#ifndef UPS_SERVER_C_SERVER_H
#define UPS_SERVER_C_SERVER_H

#include "utils/sock_header.h"

#include <poll.h>
#include <stdbool.h>


#define PORT "9034"
#define BACKLOG 5
#define POLL_TIMEOUT_MS 60000
#define SOCKOPT_TIMEOUT_SEC 2

#define MAX_GUESS_LEN 256
#define MAX_STRING_LEN 256
#define CANVAS_BUF_SIZE 1024

#define MIN_PLAYERS 2
#define TIME_BEFORE_START_SEC 5
#define GAME_DURATION_SEC 60

struct Game {
    struct Players *players;
    struct pollfd *pfds;
    int fd_size;
    int fd_count;
    bool in_progress;
};

void start();

#endif //UPS_SERVER_C_SERVER_H
