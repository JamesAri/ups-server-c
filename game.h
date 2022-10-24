/** ======================================================================= //
//         FD (SERVER) - PLAYER (CLIENT) Mapping + Game State               //
// ======================================================================= **/

#ifndef UPS_SERVER_C_GAME_H
#define UPS_SERVER_C_GAME_H

#include "shared/definitions.h"

#include <arpa/inet.h>

#define GAME_CAPACITY 2

struct Game {
    int listener;
    int cur_capacity;
    // game status
    struct Players *players;
    struct PlayerList *drawing_player_list;
    struct Canvas *canvas;
    bool in_progress;
    char guess_word[MAX_GUESS_LEN];
    time_t start_sec;
    time_t end_sec;
};

int get_active_players(struct Game *game);

int get_next_drawing_fd(struct Game *game);

struct Game *new_game(int listener);

void free_game(struct Game *game);

#endif //UPS_SERVER_C_GAME_H
