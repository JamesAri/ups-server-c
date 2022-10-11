#include "server.h"
#include "model/player.h"
#include "model/word_generator.h"
#include "utils/sock_header.h"
#include "utils/sock_utils.h"
#include "utils/serialization.h"
#include "utils/debug.h"
#include "utils/log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <poll.h>
#include <errno.h>
#include <sys/time.h>
#include <signal.h>

// ======================================================================= //
//                              MISC UTILS                                 //
// ======================================================================= //

void remove_player_from_game(struct Game *game, int player_fd) {
    struct Player *player;

    player = get_player_by_fd(game->players, player_fd);
    if (player != NULL) { // if player exits, set to offline
        log_trace("removing player %s (fd: %d) from game", player->username, player_fd);
        player->is_online = false;
        player->fd = -1;
    }

    disconnect_fd(game->pfds, player_fd, &game->fd_count);
}

void free_game(struct Game **game) {
    free_words();
    free_players(&((*game)->players));
    free_pfds(&((*game)->pfds));
    (*game) = NULL;
}

void set_socket_timeout(int fd) {
#ifdef _WIN32
#include <winsock2.h>
    DWORD tv = SOCKOPT_TIMEOUT_SEC;
#else
    struct timeval tv;
    tv.tv_sec = SOCKOPT_TIMEOUT_SEC;
    tv.tv_usec = 0;
#endif
    if (setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, (const char *) &tv, sizeof tv) < 0)
        log_error("setsockopt error: couldn't set receive timeout for socket with fd d%", fd);
    if (setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, (const char *) &tv, sizeof tv) < 0)
        log_error("setsockopt error: couldn't set send timeout for socket with fd d%", fd);
}


void log_sigterm() {
    log_warn("terminating server: SIGTERM (%d)", SIGTERM);
    exit(SIGTERM);
}

void setup_signal_handling() {
    struct sigaction action_term;
    memset(&action_term, 0, sizeof(struct sigaction));
    action_term.sa_handler = log_sigterm;
    sigaction(SIGTERM, &action_term, NULL);
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
        if (recv_res < 0)
            log_warn("error(%d): received buffer (from fd: %d): %s", recv_res, fd, hex_buf_str, recv_res);
    } else
        log_trace("received buffer (from fd: %d): %s", fd, hex_buf_str);

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

int send_buffer(int dest_fd, struct Buffer *buffer) {
    int temp = buffer->next, send_res;

    send_res = sendall(dest_fd, buffer->data, &temp);

    // logging
    char hex_buf_str[HEX_STRING_MAX_SIZE];
    memset(hex_buf_str, 0, sizeof(hex_buf_str));
    buf_to_hex_string(buffer->data, buffer->next, hex_buf_str);
    if (send_res <= 0) {
        if (send_res < 0)
            log_info("error(%d): sent buffer (to fd: %d): %s", send_res, dest_fd, hex_buf_str);
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

int send_game_draw_start(int drawing_fd, char *guess_word, time_t time_round_end) {
    struct Buffer *buffer = new_buffer();
    int send_res;
    serialize_his(START_AND_DRAW, guess_word, buffer);
    serialize_time_t(time_round_end, buffer);
    send_res = send_buffer(drawing_fd, buffer);
    free_buffer(&buffer);
    return send_res;
}

int send_game_in_progress(int fd, time_t time_round_end) {
    int send_res;
    struct Buffer *buffer = new_buffer();

    serialize_sock_header(GAME_IN_PROGRESS, buffer);
    serialize_time_t(time_round_end, buffer);

    send_res = send_buffer(fd, buffer);
    free_buffer(&buffer);
    return send_res;
}

void broadcast_buffer(struct Game *game, int listener, int sender_fd, struct Buffer *buffer) {
    int dest_fd, j;

    for (j = 0; j < game->fd_count; j++) {
        dest_fd = game->pfds[j].fd;
        if (dest_fd != listener && dest_fd != sender_fd) {
            if ((send_buffer(dest_fd, buffer)) <= 0) {
                log_warn("broadcasting error: destination fd %d failure, closing socket", dest_fd);
                remove_player_from_game(game, dest_fd);
            }
        }
    }
}

void broadcast_game_guess_start(struct Game *game, int listener, int drawing_fd, time_t time_round_end) {
    struct Buffer *buffer = new_buffer();
    serialize_sock_header(START_AND_GUESS, buffer);
    serialize_time_t(time_round_end, buffer);
    broadcast_buffer(game, listener, drawing_fd, buffer);
    free_buffer(&buffer);
}

void broadcast_waiting_for_players(struct Game *game, int listener, int connected_clients) {
    struct Buffer *buffer = new_buffer();
    serialize_sock_header(WAITING_FOR_PLAYERS, buffer);
    serialize_int(connected_clients, buffer);
    serialize_int(MIN_PLAYERS, buffer);
    broadcast_buffer(game, listener, listener, buffer);
    free_buffer(&buffer);
}


// ======================================================================= //
//                         GAME SPECIFIC UTILS                             //
// ======================================================================= //

struct Game *new_game() {
    struct Game *game = (struct Game *) malloc(sizeof(struct Game));
    game->fd_count = 0;
    game->fd_size = BACKLOG;
    game->pfds = (struct pollfd *) malloc(sizeof(struct pollfd *) * game->fd_size);
    game->players = new_players();
    game->in_progress = false;
    return game;
}

void start_round(struct Game *game, int drawing_fd, int listener, char *guess_word,
                 time_t time_round_end) {

    game->in_progress = true;

    if (send_game_draw_start(drawing_fd, guess_word, time_round_end) <= 0) {
        log_info("drawing fd %d error, closing", drawing_fd);
        remove_player_from_game(game, drawing_fd);
    }

    // we don't care if drawing player is offline, he has time to reconnect
    broadcast_game_guess_start(game, listener, drawing_fd, time_round_end);

    log_debug("new round started");
}

void end_round(struct Game *game) {
    game->in_progress = false;

    //TODO: broadcast round ended maybe

    log_debug("round ended");
}


// ======================================================================= //
//               SERVER LOGIC AND CLIENT/PLAYER MANAGEMENT                 //
// ======================================================================= //

// client is trying to login...
int manage_logging_player(int new_fd, struct Players *players) {
    struct Buffer *buffer = new_buffer();
    int username_len, flag, recv_res;

    if ((recv_res = recv_buffer_sock_header(new_fd, buffer)) <= 0) {
        free_buffer(&buffer);
        log_warn("received incomplete data from socket %d", new_fd);
        return recv_res;
    }

    flag = ((struct SocketHeader *) buffer->data)->flag;

    if (flag != INPUT_USERNAME) {
        free_buffer(&buffer);
        log_warn("invalid register-username header (received flag: 0x%02X)", flag);
        return -1;
    }

    if ((recv_res = recv_buffer_int(new_fd, buffer)) <= 0) return recv_res;

    unpack_int(buffer, &username_len);

    if ((recv_res = recv_buffer_string(new_fd, buffer, username_len)) <= 0) return recv_res;

    if (update_players(players, buffer->data + STRING_OFFSET, new_fd) == NULL) {
        free_buffer(&buffer);
        send_invalid_username(new_fd);
        log_warn("invalid username - player already logged in");
        return -1;
    }

    free_buffer(&buffer);
    return recv_res;
}

// client is trying to send drawn canvas
int manage_drawing_player(int drawing_fd, struct Buffer *buffer) {
    if (((struct SocketHeader *) buffer->data)->flag != CANVAS) {
        log_warn("invalid header: drawing player error (drawing_fd: %d)", drawing_fd);
        return -1;
    }

    return recv_buffer(drawing_fd, buffer, CANVAS_BUF_SIZE);
}

// client is trying to send a guess
int manage_guessing_player(int sender_fd, struct Buffer *buffer, char *guess_word, struct Player *player) {
    int guess_str_len, recv_res;
    char buff_client_guess[MAX_GUESS_LEN], broadcast_response_msg[MAX_STRING_LEN];

    if (((struct SocketHeader *) buffer->data)->flag != CHAT) {
        log_warn("invalid header: guessing player error");
        return -1;
    }

    memset(broadcast_response_msg, 0, sizeof(broadcast_response_msg));
    memset(buff_client_guess, 0, sizeof(buff_client_guess));

    if ((recv_res = recv_buffer_int(sender_fd, buffer)) <= 0) return recv_res;

    guess_str_len = ntohl(*(int *) (buffer->data + INT_OFFSET));
    if ((recv_res = recv_buffer_string(sender_fd, buffer, guess_str_len)) <= 0) return recv_res;

    strcpy(buff_client_guess, buffer->data + STRING_OFFSET);
    buff_client_guess[MAX_GUESS_LEN - 1] = '\0';

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

// client is trying to join waiting_for_players/new/in_progress round
void manage_current_round(struct Game *game, int new_fd, int drawing_fd, int listener, int connected_clients,
                          char *guess_word, time_t *time_round_start, time_t *time_round_end) {
    if (game->in_progress) {
        log_trace("fd %d joining game in progress", new_fd);
        if (send_game_in_progress(new_fd, *time_round_end) <= 0) {
            log_info("fd %d unable to join current round, closing", new_fd);
            remove_player_from_game(game, new_fd);
        }
    } else if (connected_clients < MIN_PLAYERS) {
        log_info("waiting for players to join (%d/%d), broadcasting", connected_clients, MIN_PLAYERS);
        broadcast_waiting_for_players(game, listener, connected_clients);
    } else {
        get_random_word(guess_word);
        log_debug("generated new guess word: %s, sending to drawing fd: %d", guess_word, drawing_fd);

        *time_round_start = time(NULL);
        *time_round_end = *time_round_start + GAME_DURATION_SEC;

        char s1[1000], s2[1000];
        strftime(s1, 1000, "%c", localtime(time_round_start));
        strftime(s2, 1000, "%c", localtime(time_round_end));
        log_trace("round started at %s and ends at %s (%d second rounds, %d second start count)",
                  s1, s2, GAME_DURATION_SEC, TIME_BEFORE_START_SEC);
        log_trace("time_round_end: %lu", time_round_end);


        start_round(game, drawing_fd, listener, guess_word, *time_round_end);
    }
}

int manage_client(struct Game *game, int drawing_fd, int sender_fd, int listener, struct Player *player,
                  char *guess_word) {
    struct Buffer *buffer = new_buffer();
    int recv_res;

    if ((recv_res = recv_buffer_sock_header(sender_fd, buffer)) <= 0) return recv_res;

    if (drawing_fd == sender_fd)
        recv_res = manage_drawing_player(sender_fd, buffer);
    else
        recv_res = manage_guessing_player(sender_fd, buffer, guess_word, player);

    if (recv_res > 0)
        broadcast_buffer(game, listener, sender_fd, buffer);

    free_buffer(&buffer);
    return recv_res;
}

// ======================================================================= //
//                              MAIN LOOP                                  //
// ======================================================================= //

void start() {
    setup_signal_handling();

    int listener, new_fd, sender_fd, drawing_fd = 6, recv_res, poll_res, connected_clients;

    struct sockaddr_storage remote_addr; // Client address
    socklen_t addr_len;
    char remote_ip[INET6_ADDRSTRLEN];

    time_t time_round_end = 0, time_round_start = 0;
    struct Game *game = new_game();
    struct Player *cur_player, *drawing_player;

    char guess_word[MAX_GUESS_LEN];

    if ((listener = get_listener_socket()) == -1) {
        log_fatal("error getting listening socket: %s", strerror(errno));
        free_game(&game);
        exit(EXIT_FAILURE);
    }

    log_info("server listening on port %s", PORT);

    add_to_pfds(&game->pfds, listener, &game->fd_count, &game->fd_size);

    for (;;) {
        poll_res = poll(game->pfds, game->fd_count, POLL_TIMEOUT_MS);

        if (game->in_progress && time(NULL) + SOCKOPT_TIMEOUT_SEC >= time_round_end)
            end_round(game);

        if (poll_res == -1) {
            log_fatal("poll-error: %s", strerror(errno));
            free_game(&game);
            exit(EXIT_FAILURE);
        } else if (poll_res == 0) {
            log_warn("poll-timeout");
        }

        for (int i = 0; i < game->fd_count; i++) {

            if (!(game->pfds[i].revents & POLLIN)) continue;

            if (game->pfds[i].fd == listener) {

                addr_len = sizeof remote_addr;
                new_fd = accept(listener, (struct sockaddr *) &remote_addr, &addr_len);

                if (new_fd == -1) {
                    log_error("listener: accept error");
                } else {
                    set_socket_timeout(new_fd);

                    if (manage_logging_player(new_fd, game->players) == -1) {
                        log_warn("login error: closing connection for socket %d", new_fd);
                        close(new_fd);
                        continue;
                    }

                    add_to_pfds(&game->pfds, new_fd, &game->fd_count, &game->fd_size);

                    connected_clients = game->fd_count - 1; // -1 for listener

                    log_info("poll-server: new connection from %s on socket %d (user: %s)",
                             inet_ntop(remote_addr.ss_family, get_in_addr((struct sockaddr *) &remote_addr),
                                       remote_ip, sizeof(remote_ip)),
                             new_fd,
                             get_player_by_fd(game->players, new_fd)->username);
                    log_info("poll-server: %d connected client(s)", connected_clients);

                    manage_current_round(game, new_fd, drawing_fd, listener, connected_clients, guess_word,
                                         &time_round_start, &time_round_end);
                }
            } else {
                sender_fd = game->pfds[i].fd;

                // Unknown player
                if ((cur_player = get_player_by_fd(game->players, sender_fd)) == NULL) {
                    log_error("internal error: unknown player connected - disconnecting malicious client");
                    disconnect_fd(game->pfds, sender_fd, &game->fd_count);
                    continue;
                }

                // No game in progress or client didn't wait for round start
                if (!game->in_progress || time(NULL) < time_round_start + TIME_BEFORE_START_SEC) {
                    log_warn("socket %d trying to communicate outside the game loop, closing", sender_fd);
                    remove_player_from_game(game, sender_fd);
                    continue;
                }

                recv_res = manage_client(game, drawing_fd, sender_fd, listener, cur_player, guess_word);

                // Known player error
                if (recv_res <= 0) {
                    if (recv_res == 0)
                        log_info("poll-server: socket %d hung up (user: %s)", sender_fd,
                                 cur_player->username);
                    else
                        log_warn("recv error (%d): socket: %d, user: %s, closing", recv_res, sender_fd,
                                 cur_player->username);
                    remove_player_from_game(game, sender_fd);
                }
            }
        }
    }
}