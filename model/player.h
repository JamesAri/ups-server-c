#ifndef UPS_SERVER_C_PLAYER_H
#define UPS_SERVER_C_PLAYER_H

#include <stdbool.h>
#include "../game.h"

#define PLAYER_CREATION_ERROR (-1)
#define PLAYER_CREATED 0
#define PLAYER_ALREADY_ONLINE 1
#define PLAYER_RECONNECTED 2

/* User data */
struct Player {
    int fd;
    char username[256];
    int score;
    bool is_online;
    struct Game *game;
};

/* Linked list node */
struct PlayerList {
    struct PlayerList *next;
    struct Player *player;
};

/* Linked list data structure */
struct Players {
    struct PlayerList *player_list;
    int count;
};


// ADD
int add_player(struct Players *players, struct Player *player);

// UPDATE
/**
 * Returns -1 on ERROR, 0 on SUCCESS, 1 if player is already "logged in" (ONLINE)
 */
int update_players(struct Players *players, char *username, int fd);

// REMOVE
int remove_player(struct Players *players, const char *username);

// GET
struct Player *get_player_by_fd(struct Players *players, int fd);

// PRINT
void print_players(struct Players *players);

bool check_player_list_validity(struct PlayerList *player_list);


// ======================================================================= //
//                         ALLOCATION & FREEING                            //
// ======================================================================= //

struct Players *new_players();

void free_player(struct Player **player);

void free_player_list(struct PlayerList **player_list);

void free_player_list_shallow(struct PlayerList **player_list);

void remove_offline_player_lists(struct Game *game);

void free_players(struct Players **players);

void free_players_shallow(struct Players **players);

#endif //UPS_SERVER_C_PLAYER_H
