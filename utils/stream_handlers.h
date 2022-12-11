#ifndef UPS_SERVER_C_STREAM_HANDLERS_H
#define UPS_SERVER_C_STREAM_HANDLERS_H

#include "serialization.h"
#include "../model/player.h"

// ======================================================================= //
//                             RECEIVING DATA                              //
// ======================================================================= //

int recv_buffer(int fd, struct Buffer *buffer, int size);

int recv_buffer_sock_header(int fd, struct Buffer *buffer);

int recv_buffer_int(int fd, struct Buffer *buffer);

int recv_buffer_string(int fd, struct Buffer *buffer, int str_len);


// ======================================================================= //
//                             SENDING DATA                                //
// ======================================================================= //

/* SEND CORE */
int send_buffer(int dest_fd, struct Buffer *buffer);

int send_header_only(int fd, int flag);

int send_ok(int fd);

int send_server_full(int fd);

int send_header_with_msg(int fd, int flag, char *msg);

int send_wrong_guess(int fd);

int send_correct_guess(int fd);

int send_invalid_username(int fd);

int send_server_error(int fd);

int send_game_draw_start(int drawing_fd, char *guess_word, time_t time_round_end);

int send_game_in_progress(struct Player *player);

int send_player_list(int fd, struct Game *game);

/* BROADCAST CORE */
void broadcast_buffer(struct Game *game, int sender_fd, struct Buffer *buffer);

void broadcast_waiting_for_players(struct Game *game, int connected_clients);

void broadcast_game_guess_start(struct Game *game, int drawing_fd);

void broadcast_round_ends(struct Game *game);

void broadcast_player_list(struct Game *game, int sender_fd);

void broadcast_heartbeat(struct Game *game);

void broadcast_player_list_change(struct Game *game, struct Player *player);

#endif //UPS_SERVER_C_STREAM_HANDLERS_H
