#ifndef UPS_SERVER_C_SERVER_H
#define UPS_SERVER_C_SERVER_H

#include <stdbool.h>
#include <sys/time.h>

// server settings
#define LOBBY_GAMES 3
#define BACKLOG 5
#define POLL_TIMEOUT_SEC 60
#define SOCKOPT_TIMEOUT_SEC 2

// buffer sizes
#define MAX_LOG_MSG_LEN 512

// game settings
#define MIN_PLAYERS 2
#define TIME_BEFORE_START_SEC 5


void start();

#endif //UPS_SERVER_C_SERVER_H
