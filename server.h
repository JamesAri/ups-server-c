#ifndef UPS_SERVER_C_SERVER_H
#define UPS_SERVER_C_SERVER_H

#include "model/player.h"


// server settings
#define BACKLOG 5
#define POLL_TIMEOUT_SEC 60
#define SOCKOPT_TIMEOUT_SEC 2

// buffer sizes
#define MAX_LOG_MSG_LEN 512

// game settings
#define MIN_PLAYERS 2
#define TIME_BEFORE_START_SEC 5


void start();

void remove_player_from_server(struct Player *player);

#endif //UPS_SERVER_C_SERVER_H
