/** ======================================================================= //
//         FD (SERVER) - PLAYER (CLIENT) Mapping + Game State               //
// ======================================================================= **/

#include "game.h"
#include "utils/log.h"
#include "utils/sock_utils.h"
#include "model/canvas.h"
#include "model/word_generator.h"

#include <string.h>

int get_active_players(struct Game *game) {
    struct PlayerList *pl = game->players->player_list;
    int counter = 0;
    while (pl != NULL) {
        if (pl->player->is_online) counter++;
        pl = pl->next;
    }
    return counter;
}

/** returns 0 if successfully updated, else -1 if failed to get next drawing fd */
int get_next_drawing_fd(struct Game *game) {
    if (game == NULL || game->players == NULL) return -1;

    int iterations = 0;

    if (game->drawing_player_list == NULL) {
        if (!check_player_list_validity(game->players->player_list)) return -1;
        game->drawing_player_list = game->players->player_list;
        return 0;
    } else {
        struct PlayerList *player_list = game->drawing_player_list->next;
        while (check_player_list_validity(player_list)) {
            iterations++;
            if (player_list->player->is_online) {
                game->drawing_player_list = player_list;
                return 0;
            }
            player_list = game->drawing_player_list->next;
        }
        // player_list is NULL here, we start over
        player_list = game->players->player_list;
        while (check_player_list_validity(player_list)) {
            iterations++;
            if (iterations >= game->players->count /*same player...*/) return -1;
            if (player_list->player->is_online) {
                game->drawing_player_list = player_list;
                return 0;
            }
            player_list = game->drawing_player_list->next;
        }
    }
    return -1;
}

struct Game *new_game(int listener) {
    struct Game *game = (struct Game *) malloc(sizeof(struct Game));

    if (game == NULL) {
        log_fatal("err: couldn't malloc game");
        exit(EXIT_FAILURE);
    }

    game->cur_capacity = 0;
    game->listener = listener;
    game->in_progress = false;
    game->drawing_player_list = NULL;
    game->start_sec = 0;
    game->end_sec = 0;
    memset(game->guess_word, 0, sizeof(game->guess_word));

    game->players = new_players();
    if (game->players == NULL) {
        log_fatal("err: couldn't malloc players");
        free(game);
        exit(EXIT_FAILURE);
    }

    game->canvas = new_canvas();
    if (game->canvas == NULL) {
        log_fatal("err: couldn't malloc canvas");
        free_players(&game->players);
        free(game);
        exit(EXIT_FAILURE);
    }

    return game;
}

void free_game(struct Game *game) {
    free_words();
    free_players(&(game->players));
    free_canvas(&(game->canvas));
}

