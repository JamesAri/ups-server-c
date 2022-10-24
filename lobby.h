#ifndef UPS_SERVER_C_LOBBY_H
#define UPS_SERVER_C_LOBBY_H

#define PFDS_INIT_SIZE 5
#define LOBBY_CAPACITY 3

struct Lobby {
    struct Game *games[LOBBY_CAPACITY];
    struct Players *all_players;
    struct pollfd *pfds;
    int fd_size;
    int fd_count;
} lobby;


void initialize_lobby(int listener);

void free_lobby();

#endif //UPS_SERVER_C_LOBBY_H
