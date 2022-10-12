#ifndef UPS_SERVER_C_SERVER_H
#define UPS_SERVER_C_SERVER_H

#include "utils/sock_header.h"

#include <poll.h>
#include <stdbool.h>
#include <sys/time.h>

// server settings
#define BACKLOG 5
#define POLL_TIMEOUT_SEC 60
#define SOCKOPT_TIMEOUT_SEC 2

// buffer sizes
#define MAX_GUESS_LEN 256
#define MAX_STRING_LEN 256
#define CANVAS_BUF_SIZE 1024

// game settings
#define MIN_PLAYERS 2
#define GAME_DURATION_SEC 60

struct Game {
    // file descriptors
    struct pollfd *pfds;
    int fd_size;
    int fd_count;
    int listener;
    // game
    struct Players *players;
    struct Player *drawing_player;
    bool in_progress;
    char guess_word[MAX_GUESS_LEN];
    time_t start_sec;
    time_t end_sec;
};

void start();

#endif //UPS_SERVER_C_SERVER_H
