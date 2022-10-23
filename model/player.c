#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "player.h"


int add_player(struct Players *players, struct Player *player) {
    if (players == NULL || player == NULL) return -1;

    struct PlayerList *new_player_node = (struct PlayerList *) calloc(1, sizeof(struct PlayerList));

    if (new_player_node == NULL) return -1;

    new_player_node->player = player;

    // first player
    if (!players->count || players->player_list == NULL) {
        new_player_node->next = NULL;
    } else {
        new_player_node->next = players->player_list;
    }
    players->player_list = new_player_node;
    players->count++;

    return 0;
}

/**
 * Returns  -1 on ERROR, 0 on SUCCESS, 1 if player is already "logged in" (ONLINE),
 *          2 if player reconnected.
 */
int update_players(struct Players *players, char *username, int fd) {
    if (players == NULL || username == NULL) return PLAYER_CREATION_ERROR;

    struct PlayerList *curr_node = players->player_list;
    while (curr_node != NULL) {
        if (!strcmp(curr_node->player->username, username)) {
            if (curr_node->player->is_online) return PLAYER_ALREADY_ONLINE;
            else {
                curr_node->player->fd = fd;
                curr_node->player->is_online = true;
                return PLAYER_RECONNECTED;
            }
        }
        curr_node = curr_node->next;
    }

    struct Player *new_player = (struct Player *) malloc(sizeof(struct Player));

    if (new_player == NULL) return PLAYER_CREATION_ERROR;

    strcpy(new_player->username, username);
    new_player->fd = fd;
    new_player->is_online = true;

    if (add_player(players, new_player) < 0) return PLAYER_CREATION_ERROR;

    return PLAYER_CREATED;
}

// we might remove players after some time offline, just implemented feature, not using yet
int remove_player(struct Players *players, int fd) {
    if (players == NULL) return -1;

    struct PlayerList *prev_player = players->player_list;
    struct PlayerList *curr_player = players->player_list->next;

    if (fd == players->player_list->player->fd) {
        free_player_list(&prev_player);
        players->player_list = curr_player;
        players->count--;
        return 0;
    }

    while (curr_player != NULL) {
        if (fd == curr_player->player->fd) {
            prev_player->next = curr_player->next;
            free_player_list(&curr_player);
            players->count--;
            return 0;
        }
        prev_player = curr_player;
        curr_player = curr_player->next;
    }
    return -1;
}

struct Player *get_player_by_fd(struct Players *players, int fd) {
    if (players == NULL) return NULL;

    struct PlayerList *curr_node = players->player_list;

    while (curr_node != NULL) {
        if (curr_node->player == NULL) return NULL;
        if (curr_node->player->fd == fd) return curr_node->player;
        curr_node = curr_node->next;
    }
    return NULL;
}

void print_players(struct Players *players) {
    if (players == NULL) return;
    struct PlayerList *curr_node = players->player_list;
    while (curr_node != NULL) {
        fprintf(stderr, "Player: fd=%d, username=%s\n", curr_node->player->fd, curr_node->player->username);
        curr_node = curr_node->next;
    }
}

bool check_player_list_validity(struct PlayerList *player_list) {
    if (player_list == NULL || player_list->player == NULL) return false;
    else return true;
}


// ======================================================================= //
//                         ALLOCATION & FREEING                            //
// ======================================================================= //

struct Players *new_players() {
    struct Players *players = (struct Players *) malloc(sizeof(struct Players));
    if (players == NULL) return NULL;
    players->player_list = NULL;
    players->count = 0;
    return players;
}

void free_player(struct Player **player) {
    if (player == NULL || (*player) == NULL) return;

    free((*player));
    (*player) = NULL;
}

void free_player_list(struct PlayerList **player_list) {
    if (player_list == NULL || (*player_list) == NULL) return;

    free_player(&(*player_list)->player);
    free((*player_list));
    (*player_list) = NULL;
}

void free_players(struct Players **players) {
    if (players == NULL || (*players) == NULL) return;

    struct PlayerList *curr_node = (*players)->player_list;
    struct PlayerList *next_node = NULL;
    while (curr_node != NULL) {
        next_node = curr_node->next;
        free_player_list(&curr_node);
        curr_node = next_node;
    }
    free((*players));
    (*players) = NULL;
}


