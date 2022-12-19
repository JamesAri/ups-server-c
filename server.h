#ifndef UPS_SERVER_C_SERVER_H
#define UPS_SERVER_C_SERVER_H

#include "model/player.h"
#include "lobby.h"
#include "model/word_generator.h"


// server settings
#define SOCKOPT_TIMEOUT_SEC 3

// buffer sizes
#define STD_STRING_BFR_LEN 256
#define MAX_LOG_MSG_LEN 512

// game settings
#define MIN_PLAYERS 2
#define TIME_BEFORE_START_SEC 5


void start(char *addr, char *port, int lobby_size, int game_size, struct Words *words);

void remove_player_from_server(struct Player *player);

#endif //UPS_SERVER_C_SERVER_H
