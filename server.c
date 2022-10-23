#include "server.h"
#include "lobby.h"
#include "model/word_generator.h"
#include "shared/sock_header.h"
#include "utils/sock_utils.h"
#include "utils/serialization.h"
#include "utils/debug.h"
#include "utils/log.h"

#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <search.h>

// ======================================================================= //
//                                MISC                                     //
// ======================================================================= //

void add_to_lobby_pfds(int new_fd) {
    add_to_pfds(&(lobby.pfds), new_fd, &(lobby.fd_count), &(lobby.fd_size));
}

void disconnect_fd(int fd) {
    if (fd < 0) {
        log_error("trying to disconnect negative fd");
        return;
    }
    close(fd);
    del_from_pfds_by_fd(lobby.pfds, fd, &(lobby.fd_count));
}

void remove_player_from_server(struct Player *player) {
    if (player == NULL) {
        log_error("internal err: player is NULL");
        return;
    }
    log_debug("removing player %s (fd: %d) from game", player->username, player->fd);
    disconnect_fd(player->fd);
    player->game->cur_capacity--;
    player->is_online = false;
    player->fd = -1;
}

// ======================================================================= //
//                            LOGGING UTILS                                //
// ======================================================================= //

void log_sigterm() {
    log_info("terminating server: SIGTERM (%d) (┛ಠ_ಠ)┛彡┻━┻", SIGTERM);
    exit(SIGTERM);
}

void setup_signal_handling() {
    struct sigaction action_term;
    memset(&action_term, 0, sizeof(struct sigaction));
    action_term.sa_handler = log_sigterm;
    sigaction(SIGTERM, &action_term, NULL);
}

void log_game_start(time_t *start, time_t *end) {
    char s1[MAX_LOG_MSG_LEN], s2[MAX_LOG_MSG_LEN];
    memset(s1, 0, sizeof(s1));
    memset(s2, 0, sizeof(s2));
    strftime(s1, sizeof(s1), "%c", localtime(start));
    strftime(s2, sizeof(s2), "%c", localtime(end));
    log_trace("round starts at %s and ends at %s (%d sec rounds, %d sec counter)",
              s1, s2, GAME_DURATION_SEC, TIME_BEFORE_START_SEC);
}

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
            log_warn("err(%d): received buffer (from fd: %d): %s", recv_res, fd, hex_buf_str, recv_res);
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

/* BROADCAST CORE */
void broadcast_buffer(struct Game *game, int sender_fd, struct Buffer *buffer) {
    int dest_fd;

    char hex_buf_str[HEX_STRING_MAX_SIZE];
    memset(hex_buf_str, 0, sizeof(hex_buf_str));
    buf_to_hex_string(buffer->data, buffer->next, hex_buf_str);
    log_trace("broadcasting buffer (sender: %d): %s", sender_fd, hex_buf_str);

    struct PlayerList *pl = game->players->player_list;
    while (pl != NULL) {
        if (!pl->player->is_online) continue;
        dest_fd = pl->player->fd;
        if (dest_fd != game->listener && dest_fd != sender_fd) {
            if ((send_buffer(dest_fd, buffer)) <= 0) {
                log_warn("broadcasting err: destination fd %d failure, removing player", dest_fd);
                remove_player_from_server(pl->player);
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
    broadcast_buffer(game, -1, buffer);
    free_buffer(&buffer);
}


// ======================================================================= //
//               SERVER LOGIC AND CLIENT (PLAYER) MANAGEMENT               //
// ======================================================================= //

// client is trying to log in
int manage_logging_player(int new_fd) {
    struct Player *player;
    struct Game *game;
    struct Buffer *buffer = new_buffer();
    int username_len, flag, res, res_update;
    char username[MAX_USERNAME_LEN];

    if ((res = recv_buffer_sock_header(new_fd, buffer)) <= 0) {
        free_buffer(&buffer);
        log_warn("received incomplete data from socket %d", new_fd);
        return res;
    }

    flag = ((struct SocketHeader *) buffer->data)->flag;

    if (flag != LOGIN) {
        free_buffer(&buffer);
        log_warn("invalid register-username header (received flag: 0x%02X)", flag);
        return -1;
    }

    if ((res = recv_buffer_int(new_fd, buffer)) <= 0) return res;

    unpack_int(buffer, &username_len);

    if (username_len > MAX_USERNAME_LEN) {
        free_buffer(&buffer);
        log_warn("username too long (fd: %d)", new_fd);
        return -1;
    }

    if ((res = recv_buffer_string(new_fd, buffer, username_len)) <= 0) return res;

    strcpy(username, buffer->data + STRING_OFFSET);

    free_buffer(&buffer);

    res_update = update_players(lobby.all_players, username, new_fd);

    switch (res_update) {
        case PLAYER_ALREADY_ONLINE:
            send_invalid_username(new_fd);
            log_warn("invalid username, player %s already logged in", username);
            return -1;
        case PLAYER_CREATION_ERROR:
            send_server_error(new_fd);
            log_error("failed to create or update a player with username %s", username);
            return -1;
        case PLAYER_RECONNECTED:
            log_debug("player %s reconnected", username);
            send_ok(new_fd);
            // TODO check game capacity
            break;
        case PLAYER_CREATED:
            player = get_player_by_fd(lobby.all_players, new_fd);
            for (int i = 0; i < LOBBY_CAPACITY; i++) {
                game = lobby.games[i];
                if (game->cur_capacity < GAME_CAPACITY) {
                    player->game = game;
                    game->cur_capacity++;
                    if (add_player(game->players, player) < 0) return -1;
                    log_debug("created new player %s", username);
                    send_ok(new_fd);
                    return res;
                }
            }
            log_warn("lobby is full, player %s has to wait", player->username);
            return -1;
        default:
            return -1;
    }
    return res;
}

// client is trying to send canvas changes
int manage_drawing_player(struct Player *player, struct Buffer *buffer) {
    int drawing_fd = player->fd;
    int recv_res, number_of_diffs, temp;
    int serialized_canvas_offset = sizeof(struct SocketHeader) + sizeof(int); // header + num of diffs

    if (((struct SocketHeader *) buffer->data)->flag != CANVAS) {
        log_warn("received invalid header, drawing_fd: %d", drawing_fd);
        return -1;
    }

    if ((recv_res = recv_buffer_int(drawing_fd, buffer)) <= 0) {
        log_debug("couldn't receive canvas buffer size (fd: %d)", drawing_fd);
        return recv_res;
    }
    unpack_int(buffer, &number_of_diffs);
    if ((recv_res = recv_buffer(drawing_fd, buffer, number_of_diffs * (int) sizeof(int))) <= 0) {
        log_debug("couldn't receive canvas buffer (fd: %d)", drawing_fd);
        return recv_res;
    }

    for (int i = 0; i < number_of_diffs; i++) {
        unpack_int_var(buffer, &temp, serialized_canvas_offset + (int) sizeof(int) * i);
        if (temp < 0 || temp >= CANVAS_SIZE) {
            log_debug("received canvas index is out of range: %d (fd: %d)", temp, drawing_fd);
            return -1;
        }
        toggle_bit(player->game->canvas->bitarray_grid, temp);
    }

    return recv_res;
}

// todo dont allow chat if player correct guessed
// client is trying to send a guess
int manage_guessing_player(struct Player *player, struct Buffer *buffer) {
    int guess_str_len, recv_res, sender_fd = player->fd;
    char *guess_word = player->game->guess_word;
    char buff_client_guess[MAX_GUESS_LEN + 1], broadcast_response_msg[MAX_GUESS_LEN]; // TODO use MAX_MSG_LEN

    if (((struct SocketHeader *) buffer->data)->flag != CHAT) {
        log_warn("invalid header: guessing player err");
        return -1;
    }

    memset(broadcast_response_msg, 0, sizeof(broadcast_response_msg));
    memset(buff_client_guess, 0, sizeof(buff_client_guess));

    if ((recv_res = recv_buffer_int(sender_fd, buffer)) <= 0) return recv_res;

    guess_str_len = ntohl(*(int *) (buffer->data + INT_OFFSET));
    if ((recv_res = recv_buffer_string(sender_fd, buffer, guess_str_len)) <= 0) return recv_res;

    strcpy(buff_client_guess, buffer->data + STRING_OFFSET);
    buff_client_guess[MAX_GUESS_LEN] = '\0';

    log_trace("client guess word: %s", buff_client_guess);

    int correct_guess = !strcmp(guess_word, buff_client_guess);

    if (correct_guess) {
        sprintf(broadcast_response_msg, "Player %s got the answer!", player->username);
        clear_buffer(buffer);
        serialize_his(CORRECT_GUESS_ANNOUNCEMENT, broadcast_response_msg, buffer);
    } else {
        sprintf(broadcast_response_msg, "%s: %s", player->username, buff_client_guess);
        clear_buffer(buffer);
        serialize_his(CHAT, broadcast_response_msg, buffer);
    }

    if (correct_guess) return send_correct_guess(sender_fd);
    else return send_wrong_guess(sender_fd);
}

int manage_player(struct Player *player) {
    struct Game *game = player->game;
    struct Buffer *buffer = new_buffer();
    int recv_res, sender_fd = player->fd;

    if ((recv_res = recv_buffer_sock_header(sender_fd, buffer)) <= 0) return recv_res;

    if (game->drawing_player_list->player->fd == sender_fd)
        recv_res = manage_drawing_player(player, buffer);
    else
        recv_res = manage_guessing_player(player, buffer);

    if (recv_res > 0)
        broadcast_buffer(game, sender_fd, buffer);
    else {
        log_warn("couldn't broadcast, because sender_fd %d is invalid", sender_fd);
    }

    free_buffer(&buffer);
    return recv_res;
}


// ======================================================================= //
//                          ROUNDS MANAGEMENT                              //
// ======================================================================= //

void start_round(struct Game *game) {
    game->in_progress = true;

    if (get_next_drawing_fd(game)) {
        log_fatal("internal err: there should be available players for drawing");
        free_lobby();
        exit(EXIT_FAILURE);
    }

    struct Player *drawing_player = game->drawing_player_list->player;
    int drawing_fd = drawing_player->fd;

    log_debug("next drawing player is %s (fd: %d)", drawing_player->username, drawing_fd);

    if (send_game_draw_start(drawing_fd, game->guess_word, game->end_sec) <= 0) {
        log_warn("drawing fd %d err, closing", drawing_fd);
        remove_player_from_server(drawing_player);
    }

    // we don't care if drawing player is offline, he might reconnect
    broadcast_game_guess_start(game, drawing_fd);

    log_debug("new round started");
}

// client is trying to join waiting_for_players/start/in_progress round
void validate_round(struct Game *game) {
    int connected_players = get_active_players(game);

    if (connected_players < MIN_PLAYERS) {
        log_trace("waiting for players to join (%d/%d), broadcasting", connected_players, MIN_PLAYERS);
        broadcast_waiting_for_players(game, connected_players);
    } else {
        get_random_word(game->guess_word);
        log_trace("generated new guess word: %s", game->guess_word);

        game->start_sec = time(NULL) + TIME_BEFORE_START_SEC;
        game->end_sec = game->start_sec + GAME_DURATION_SEC;

        start_round(game);

        log_game_start(&game->start_sec, &game->end_sec);
    }
}

void end_round(struct Game *game) {
    game->in_progress = false;

    broadcast_round_ends(game);

    memset(game->canvas->bitarray_grid, 0, sizeof(game->canvas->bitarray_grid));

    validate_round(game);

    log_debug("round ended");
}

// ======================================================================= //
//                              MAIN LOOP                                  //
// ======================================================================= //

void start() {
    setup_signal_handling();

    int listener, timeout_sec = POLL_TIMEOUT_SEC, timeout_sec_new, new_fd, sender_fd, recv_res, poll_res;

    struct sockaddr_storage remote_addr; // Client address
    socklen_t addr_len;
    char remote_ip[INET6_ADDRSTRLEN];

    struct Player *cur_player;
    struct Game *game;

    if ((listener = get_listener_socket(PORT, BACKLOG)) == -1) {
        log_fatal("err getting listening socket: %s", strerror(errno));
        exit(EXIT_FAILURE);
    }
    log_info("server (fd %d) listening on port %s ( ˘ ɜ˘) ♬♪♫", listener, PORT);

    initialize_lobby(listener);

    add_to_lobby_pfds(listener);

    for (;;) {

        for (int i = 0; i < LOBBY_CAPACITY; i++) {
            game = lobby.games[i];
            timeout_sec_new = (game->in_progress) ? (int) (game->end_sec - time(NULL)) : POLL_TIMEOUT_SEC;
            timeout_sec = (timeout_sec_new < timeout_sec) ? timeout_sec_new : timeout_sec;
        }

        log_trace("timeout set to %d second(s)", timeout_sec);
        poll_res = poll(lobby.pfds, lobby.fd_count, timeout_sec * 1000);

        if (poll_res == -1) {
            log_fatal("poll-err: %s", strerror(errno));
            free_lobby();
            exit(EXIT_FAILURE);
        } else if (poll_res == 0) {
            log_trace("poll-timeout");
        }

        for (int i = 0; i < LOBBY_CAPACITY; i++) {
            if (lobby.games[i]->in_progress && time(NULL) + SOCKOPT_TIMEOUT_SEC >= lobby.games[i]->end_sec)
                end_round(lobby.games[i]);
        }

        for (int i = 0; i < lobby.fd_count; i++) {

            if (!(lobby.pfds[i].revents & POLLIN)) continue;

            if (lobby.pfds[i].fd == listener) {

                addr_len = sizeof remote_addr;
                new_fd = accept(listener, (struct sockaddr *) &remote_addr, &addr_len);

                if (new_fd == -1) {
                    log_error("listener: accept err");
                } else {
                    log_debug("poll-server: new connection (fd: %d), processing...", new_fd);

                    if (set_socket_timeout(new_fd, SOCKOPT_TIMEOUT_SEC) < 0) {
                        log_error("setsockopt err: couldn't set timeout for socket with fd d%", new_fd);
                        close(new_fd);
                        continue;
                    }

                    if (manage_logging_player(new_fd) <= 0) {
                        log_warn("login err: closing connection for socket %d", new_fd);
                        close(new_fd);
                        continue;
                    }

                    if ((cur_player = get_player_by_fd(lobby.all_players, new_fd)) == NULL) {
                        log_error("internal err: created player missing in lobby list, server might not work properly");
                        disconnect_fd(new_fd);
                        continue;
                    }

                    add_to_lobby_pfds(cur_player->fd);

                    log_debug("poll-server: new connection from %s on socket %d (user: %s) was successful",
                              inet_ntop(remote_addr.ss_family, get_in_addr((struct sockaddr *) &remote_addr),
                                        remote_ip, sizeof(remote_ip)),
                              new_fd, cur_player->username);
                    log_debug("poll-server: %d connected client(s)", lobby.fd_count - 1); // -1 for listener

                    if (cur_player->game->in_progress) {
                        log_trace("fd %d joining game in progress", cur_player->fd);
                        if (send_game_in_progress(cur_player) <= 0) {
                            log_warn("fd %d unable to join current round, closing", cur_player->fd);
                            remove_player_from_server(cur_player);
                        }
                    } else {
                        validate_round(cur_player->game);
                    }
                }
            } else {
                sender_fd = lobby.pfds[i].fd;

                // Unknown player
                if ((cur_player = get_player_by_fd(lobby.all_players, sender_fd)) == NULL) {
                    log_error("internal err: unknown player connected - disconnecting malicious client");
                    disconnect_fd(sender_fd);
                    continue;
                }

                // No game in progress or client not waiting for start
                if (!(cur_player->game->in_progress) || time(NULL) < cur_player->game->start_sec) {
                    log_debug("socket %d (%s) trying to communicate outside the game loop, closing",
                              sender_fd, cur_player->username);
                    remove_player_from_server(cur_player);
                    continue;
                }

                recv_res = manage_player(cur_player);

                // Known player error
                if (recv_res <= 0) {
                    if (recv_res == 0)
                        log_debug("poll-server: socket %d hung up (user: %s)", sender_fd, cur_player->username);
                    else
                        log_warn("recv err (%d): socket: %d, user: %s, closing", recv_res, sender_fd,
                                 cur_player->username);
                    remove_player_from_server(cur_player);
                }
            }
        }
    }
}