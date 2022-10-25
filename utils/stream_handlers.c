#include "stream_handlers.h"
#include "sock_utils.h"
#include "debug.h"
#include "string.h"
#include "log.h"
#include "../game.h"
#include "../server.h"
#include "../shared/sock_header.h"


// ======================================================================= //
//                             RECEIVING DATA                              //
// ======================================================================= //

int recv_buffer(int fd, struct Buffer *buffer, int size) {
    int recv_res, temp = size;
    reserve_space(buffer, size);

    recv_res = recvall(fd, buffer->data + buffer->next, &temp);
    buffer->next += size;
    // logging
    char hex_buf_str[HEX_STRING_MAX_SIZE];
    memset(hex_buf_str, 0, sizeof(hex_buf_str));
    buf_to_hex_string(buffer->data, buffer->next, hex_buf_str);
    if (recv_res <= 0) {
        if (recv_res == 0)
            log_trace("socket (fd: %d) hung up, received bytes: %s", fd, hex_buf_str);
        if (recv_res < 0)
            log_warn("err (%d): received buffer (from fd: %d): %s", recv_res, fd, hex_buf_str, recv_res);
    } else
        log_trace("received bytes (from fd: %d): %s", fd, hex_buf_str);

    return recv_res;
}

int recv_buffer_sock_header(int fd, struct Buffer *buffer) {
    return recv_buffer(fd, buffer, sizeof(struct SocketHeader));
}

int recv_buffer_int(int fd, struct Buffer *buffer) {
    return recv_buffer(fd, buffer, sizeof(int));
}

int recv_buffer_string(int fd, struct Buffer *buffer, int str_len) {
    return recv_buffer(fd, buffer, str_len);
}


// ======================================================================= //
//                             SENDING DATA                                //
// ======================================================================= //

/* SEND CORE */
int send_buffer(int dest_fd, struct Buffer *buffer) {
    int temp = buffer->next, send_res;

    send_res = sendall(dest_fd, buffer->data, &temp);

    // logging
    char hex_buf_str[HEX_STRING_MAX_SIZE];
    memset(hex_buf_str, 0, sizeof(hex_buf_str));
    buf_to_hex_string(buffer->data, buffer->next, hex_buf_str);
    if (send_res <= 0) {
        if (send_res < 0)
            log_warn("err(%d): couldn't send buffer to fd: %d, buffer: %s", send_res, dest_fd, hex_buf_str);
    } else {
        log_trace("sent buffer (to fd: %d): %s", dest_fd, hex_buf_str);
    }

    return send_res;
}

int send_header_only(int fd, int flag) {
    struct Buffer *buffer = new_buffer();
    int send_res;
    serialize_sock_header(flag, buffer);
    send_res = send_buffer(fd, buffer);
    free_buffer(&buffer);
    return send_res;
}

int send_ok(int fd) {
    return send_header_only(fd, OK);
}

int send_server_full(int fd) {
    return send_header_only(fd, SERVER_FULL);
}

int send_header_with_msg(int fd, int flag, char *msg) {
    struct Buffer *buffer = new_buffer();
    int send_res;
    serialize_his(flag, msg, buffer);
    send_res = send_buffer(fd, buffer);
    free_buffer(&buffer);
    return send_res;
}

int send_wrong_guess(int fd) {
    return send_header_only(fd, WRONG_GUESS);
}

int send_correct_guess(int fd) {
    return send_header_only(fd, CORRECT_GUESS);
}

int send_invalid_username(int fd) {
    return send_header_only(fd, INVALID_USERNAME);
}

int send_server_error(int fd) {
    return send_header_only(fd, SERVER_ERROR);
}

int send_game_draw_start(int drawing_fd, char *guess_word, time_t time_round_end) {
    struct Buffer *buffer = new_buffer();
    int send_res;
    serialize_his(START_AND_DRAW, guess_word, buffer);
    serialize_time_t(time_round_end, buffer);
    send_res = send_buffer(drawing_fd, buffer);
    free_buffer(&buffer);
    return send_res;
}

int send_game_in_progress(struct Player *player) {
    int connecting_fd = player->fd, send_res, second_flag = START_AND_GUESS;
    struct Game *game = player->game;
    struct Buffer *buffer = new_buffer();

    serialize_sock_header(GAME_IN_PROGRESS, buffer);

    if (game->drawing_player_list->player->fd == connecting_fd) second_flag = START_AND_DRAW;

    serialize_sock_header(second_flag, buffer);

    if (second_flag == START_AND_DRAW) {
        serialize_int((int) strlen(game->guess_word), buffer);
        serialize_string(game->guess_word, buffer);
    }

    serialize_time_t(game->end_sec, buffer);
    serialize_canvas(game->canvas, buffer);

    send_res = send_buffer(connecting_fd, buffer);
    free_buffer(&buffer);
    return send_res;
}

int send_player_list(int fd, struct Game *game) {
    int send_res;
    struct Buffer *buffer = new_buffer();
    serialize_sock_header(PLAYER_LIST, buffer);
    serialize_int(game->players->count, buffer);
    serialize_players(game->players, buffer);
    send_res = send_buffer(fd, buffer);
    free_buffer(&buffer);
    return send_res;
}

/* BROADCAST CORE */
void broadcast_buffer(struct Game *game, int sender_fd, struct Buffer *buffer) {
    int dest_fd;

    char hex_buf_str[HEX_STRING_MAX_SIZE];
    memset(hex_buf_str, 0, sizeof(hex_buf_str));
    buf_to_hex_string(buffer->data, buffer->next, hex_buf_str);
    log_trace("broadcasting buffer (sender: %d): %s", sender_fd, hex_buf_str);

    struct PlayerList *pl = game->players->player_list;
    while (pl != NULL) {
        if (pl->player->is_online) {
            dest_fd = pl->player->fd;
            if (dest_fd != game->listener && dest_fd != sender_fd) {
                if ((send_buffer(dest_fd, buffer)) <= 0) {
                    log_warn("broadcasting err: destination fd %d failure, removing player", dest_fd);
                    remove_player_from_server(pl->player);
                }
            }
        }
        pl = pl->next;
    }
}

void broadcast_waiting_for_players(struct Game *game, int connected_clients) {
    struct Buffer *buffer = new_buffer();
    serialize_sock_header(WAITING_FOR_PLAYERS, buffer);
    serialize_int(connected_clients, buffer);
    serialize_int(MIN_PLAYERS, buffer);
    broadcast_buffer(game, game->listener, buffer);
    free_buffer(&buffer);
}

void broadcast_game_guess_start(struct Game *game, int drawing_fd) {
    struct Buffer *buffer = new_buffer();
    serialize_sock_header(START_AND_GUESS, buffer);
    serialize_time_t(game->end_sec, buffer);
    broadcast_buffer(game, drawing_fd, buffer);
    free_buffer(&buffer);
}

void broadcast_round_ends(struct Game *game) {
    struct Buffer *buffer = new_buffer();
    serialize_sock_header(GAME_ENDS, buffer);
    broadcast_buffer(game, game->listener, buffer);
    free_buffer(&buffer);
}

void broadcast_player_list(struct Game *game, int sender_fd) {
    struct Buffer *buffer = new_buffer();
    serialize_sock_header(PLAYER_LIST, buffer);
    serialize_int(game->players->count, buffer);
    serialize_players(game->players, buffer);
    broadcast_buffer(game, sender_fd, buffer);
    free_buffer(&buffer);
}

void broadcast_player_list_change(struct Game *game, struct Player *player) {
    struct Buffer *buffer = new_buffer();
    serialize_sock_header(PLAYER_LIST_CHANGE, buffer);
    serialize_player(player, buffer);
    broadcast_buffer(game, player->fd, buffer);
    free_buffer(&buffer);
}