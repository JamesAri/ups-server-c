#include <stdlib.h>
#include "lobby.h"
#include "utils/log.h"
#include "utils/sock_utils.h"


void initialize_lobby(int listener) {
    lobby.fd_count = 0;
    lobby.fd_size = PFD_INIT_SIZE;

    lobby.pfds = (struct pollfd *) malloc(sizeof(struct pollfd *) * lobby.fd_size);
    if (lobby.pfds == NULL) {
        log_fatal("err: couldn't malloc pfds");
        exit(EXIT_FAILURE);
    }

    for (int i = 0; i < LOBBY_SIZE; i++) {
        lobby.games[i] = new_game(listener);
    }
}

void free_lobby() {
    free_pfds(&(lobby.pfds));
    for (int i = 0; i < LOBBY_SIZE; i++)
        free_game(lobby.games[i]);
}