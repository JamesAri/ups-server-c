#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "player.h"

struct Players *new_players() {
    struct Players *players = (struct Players *) malloc(sizeof(struct Players));
    players->playerList = NULL;
    players->count = 0;
    return players;
}

struct Player *get_player_by_fd(struct Players *players, int fd) {
    struct PlayerList *curr_node = players->playerList;
    while (curr_node != NULL) {
        if (curr_node->player->fd == fd) {
            return curr_node->player;
        }
        curr_node = curr_node->next;
    }
    return NULL;
}

/*
 * Return NULL if players already "logged in" (online).
 * Otherwise, return the updated/added player.
 */
struct Player *update_players(struct Players *players, char *username, int fd) {
    struct PlayerList *curr_node = players->playerList;
    while (curr_node != NULL) {
        if (!strcmp(curr_node->player->username, username)) {
            if (curr_node->player->is_online) return NULL;
            else {
                curr_node->player->fd = fd;
                curr_node->player->is_online = 1;
                return curr_node->player;
            }
        }
        curr_node = curr_node->next;
    }

    struct Player *new_player = (struct Player *) malloc(sizeof(struct Player));
    strcpy(new_player->username, username);
    new_player->fd = fd;
    new_player->is_online = 1;

    add_player(players, new_player);

    return new_player;
}

int add_player(struct Players *players, struct Player *player) {
    if (players == NULL) {
        return -1;
    }

    struct PlayerList *new_player_node = (struct PlayerList *) malloc(sizeof(struct PlayerList));
    new_player_node->player = player;

    if (!players->count || players->playerList == NULL) {
        new_player_node->next = NULL;
    } else {
        new_player_node->next = players->playerList;
    }
    players->playerList = new_player_node;
    players->count++;

    return 0;
}

int remove_player(struct Players *players, int fd) {
    struct PlayerList *prev_player = players->playerList;
    struct PlayerList *curr_player = players->playerList->next;

    if (fd == players->playerList->player->fd) {
        free_player_list(&prev_player);
        players->playerList = curr_player;
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

void free_player(struct Player **player) {
    free((*player));
    (*player) = NULL;
}


void free_player_list(struct PlayerList **player_list) {
    free_player(&(*player_list)->player);
    free((*player_list));
    (*player_list) = NULL;
}

void free_players(struct Players **players) {
    struct PlayerList *curr_node = (*players)->playerList;
    struct PlayerList *next_node = NULL;
    while (curr_node != NULL) {
        next_node = curr_node->next;
        free_player_list(&curr_node);
        curr_node = next_node;
    }
    free((*players));
    (*players) = NULL;
}

void print_players(struct Players *players) {
    struct PlayerList *curr_node = players->playerList;
    while (curr_node != NULL) {
        fprintf(stderr, "Player: fd=%d, username=%s\n", curr_node->player->fd, curr_node->player->username);
        curr_node = curr_node->next;
    }
}

