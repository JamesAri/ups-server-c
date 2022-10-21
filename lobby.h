#ifndef UPS_SERVER_C_LOBBY_H
#define UPS_SERVER_C_LOBBY_H

#include "model/player.h"
#include "game.h"

#define PFD_INIT_SIZE 5
#define LOBBY_SIZE 3

struct Lobby {
    struct Game *games[LOBBY_SIZE];
    struct Players *all_players;
    struct pollfd *pfds;
    int fd_size;
    int fd_count;
} lobby;


void initialize_lobby(int listener);

void free_lobby();

#endif //UPS_SERVER_C_LOBBY_H
