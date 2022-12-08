#include "player.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

bool check_player_list_validity(struct PlayerList *player_list) {
    if (player_list == NULL || player_list->player == NULL) return false;
    return true;
}

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
    new_player->game = NULL;

    if (add_player(players, new_player) < 0) return PLAYER_CREATION_ERROR;

    return PLAYER_CREATED;
}

/**
 * Removes player and frees only PlayerList structure, i.e. shallow free
 */
int remove_player(struct Players *players, const char *username) {
    if (players == NULL || !check_player_list_validity(players->player_list)) return -1;

    if (!strcmp(username, players->player_list->player->username)) {
        players->player_list = players->player_list->next;
        players->count--;
        return 0;
    }

    struct PlayerList *l_node = players->player_list;
    struct PlayerList *r_node = players->player_list->next;
    while (r_node != NULL) {
        if (!strcmp(username, r_node->player->username)) {
            l_node->next = r_node->next;
            players->count--;
            return 0;
        }
        l_node = r_node;
        r_node = r_node->next;
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


void remove_offline_player_lists(struct Game *game) {
    struct Players *players = game->players;
    if (players == NULL || !check_player_list_validity(players->player_list)) return;

    struct PlayerList *l_node = players->player_list;
    struct PlayerList *r_node = players->player_list->next;

    int drawing_fd = game->drawing_player_list->player->fd;

    while (l_node != NULL) { // first node is online or NULL (empty game)
        r_node = l_node->next;
        if (!l_node->player->is_online) {
            players->player_list = r_node;
            players->count--;
            if (l_node->player->fd == drawing_fd) {
                game->drawing_player_list = NULL;
            }
            free_player_list_shallow(&l_node);
        } else {
            break;
        }
        l_node = r_node;
    }
    if (players->player_list == NULL) return;

    l_node = players->player_list;
    r_node = players->player_list->next;

    while (r_node) {
        if (!r_node->player->is_online) {
            l_node->next = r_node->next;
            players->count--;
            if (r_node->player->fd == drawing_fd) {
                game->drawing_player_list = NULL;
            }
            free_player_list_shallow(&r_node);
            r_node = l_node->next;
        } else {
            l_node = r_node;
            r_node = r_node->next;
        }
    }
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

void free_player_list_shallow(struct PlayerList **player_list) {
    if (player_list == NULL || (*player_list) == NULL) return;

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

void free_players_shallow(struct Players **players) {
    if (players == NULL || (*players) == NULL) return;

    struct PlayerList *curr_node = (*players)->player_list;
    struct PlayerList *next_node = NULL;
    while (curr_node != NULL) {
        next_node = curr_node->next;
        free_player_list_shallow(&curr_node);
        curr_node = next_node;
    }
    free((*players));
    (*players) = NULL;
}


