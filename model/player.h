#ifndef UPS_SERVER_C_PLAYER_H
#define UPS_SERVER_C_PLAYER_H

#include <stdbool.h>

struct Player {
    int fd;
    char username[256];
    int score;
    bool is_online;
};

struct PlayerList {
    struct PlayerList *next;
    struct Player *player;
};

struct Players {
    struct PlayerList *player_list;
    int count;
};


struct Players *new_players();

int add_player(struct Players *players, struct Player *player);

int remove_player(struct Players *players, int fd);

int update_players(struct Players *players, char *username, int fd);

struct Player *get_player_by_fd(struct Players *players, int fd);

void print_players(struct Players *players);

void free_player(struct Player **player);

void free_player_list(struct PlayerList **player_list);

void free_players(struct Players **players);

#endif //UPS_SERVER_C_PLAYER_H
