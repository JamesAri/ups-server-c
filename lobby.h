#ifndef UPS_SERVER_C_LOBBY_H
#define UPS_SERVER_C_LOBBY_H

#define PFDS_INIT_SIZE 5

struct Lobby {
    struct Game **games;
    struct Players *all_players;
    struct pollfd *pfds;
    int fd_size;
    int fd_count;
    int capacity;
} lobby;


void initialize_lobby(int listener, int capacity);

void free_lobby();

#endif //UPS_SERVER_C_LOBBY_H
