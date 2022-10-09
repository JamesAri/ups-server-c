#include "server.h"
#include "model/player.h"
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

void remove_player_from_game(struct Game *game, int player_fd, int i) {
    struct Player *player;

    if (player_fd != game->pfds[i].fd)
        log_error("internal error: player's fd doesn't match poll pfds (%d != %d)", player_fd, game->pfds[i].fd);

    player = get_player_by_fd(game->players, player_fd);
    if (player != NULL) {
        player->is_online = 0;
    }

    disconnect_fd(game->pfds, i, &game->fd_count);
}

int recv_buffer(int fd, struct Buffer *buffer, int size) {
    int recv_res, temp = size;
    reserve_space(buffer, size);
    recv_res = recvall(fd, buffer->data + buffer->next, &temp);
    buffer->next += size;

    if (recv_res <= 0) {
        char hex_buf_str[HEX_STRING_MAX_SIZE];
        buf_to_hex_string(buffer->data, buffer->next, hex_buf_str);
        if (recv_res < 0)
            log_error("error(%d): received buffer (from fd: %d): %s", recv_res, fd, hex_buf_str, recv_res);
    }

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

int send_buffer(int dest_fd, struct Buffer *buffer) {
    int temp = buffer->next, send_res;

    send_res = sendall(dest_fd, buffer->data, &temp);

    if (send_res <= 0) {
        char hex_buf_str[HEX_STRING_MAX_SIZE];
        buf_to_hex_string(buffer->data, buffer->next, hex_buf_str);
        log_info("error(%d): sent buffer (to fd: %d): %s", send_res, dest_fd, hex_buf_str);
    }

    return send_res;
}

int send_header_only(int fd, int flag) {
    struct Buffer *buffer = new_buffer();
    int res_send;
    serialize_sock_header(flag, buffer);
    res_send = send_buffer(fd, buffer);
    free_buffer(&buffer);
    return res_send;
}

int send_header_with_msg(int fd, int flag, char *msg) {
    struct Buffer *buffer = new_buffer();
    int res_send;
    serialize_his(flag, msg, buffer);
    res_send = send_buffer(fd, buffer);
    free_buffer(&buffer);
    return res_send;
}

void broadcast(struct Game *game, int listener, int sender_fd, struct Buffer *buffer) {
    int dest_fd, j;

    for (j = 0; j < game->fd_count; j++) {
        dest_fd = game->pfds[j].fd;
        if (dest_fd != listener && dest_fd != sender_fd) {
            if ((send_buffer(dest_fd, buffer)) <= 0) {
                log_error("broadcast error: fd %d failure, closing socket", dest_fd);
                remove_player_from_game(game, dest_fd, j);
            }
        }
    }
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

int send_game_draw_start(int drawing_fd, char *guess_word) {
    return send_header_with_msg(drawing_fd, START_AND_DRAW, guess_word);
}

void send_game_guess_start(struct Game *game, int listener, int drawing_fd) {
    struct Buffer *buffer = new_buffer();
    serialize_sock_header(START_AND_GUESS, buffer);
    broadcast(game, listener, drawing_fd, buffer);
    free_buffer(&buffer);
}

int manage_drawing_player(int sender_fd, struct Buffer *buffer) {
    if (((struct SocketHeader *) buffer->data)->flag != CANVAS) {
        log_error("invalid header: drawing player error");
        return -1;
    }

    return recv_buffer(sender_fd, buffer, CANVAS_BUF_SIZE);
}

int manage_guessing_player(int sender_fd, struct Buffer *buffer, char *guess_word,
                           struct Player *player) {
    int guess_str_len, recv_res;
    char buff_client_guess[MAX_GUESS_LEN], broadcast_response_msg[MAX_STRING_LEN];

    if (((struct SocketHeader *) buffer->data)->flag != CHAT) {
        log_error("invalid header: guessing player error");
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


int manage_client_data(struct Game *game, int drawing_fd, int sender_fd, int listener, struct Player *player) {
    char *guess_word = "coffee";
    struct Buffer *buffer = new_buffer();
    int recv_res;

    if ((recv_res = recv_buffer_sock_header(sender_fd, buffer)) <= 0) return recv_res;

    if (drawing_fd == sender_fd)
        recv_res = manage_drawing_player(sender_fd, buffer);
    else
        recv_res = manage_guessing_player(sender_fd, buffer, guess_word, player);

    if (recv_res > 0)
        broadcast(game, listener, sender_fd, buffer);

    free_buffer(&buffer);
    return recv_res;
}


int manage_logging_player(int new_fd, struct Players *players) {
    struct Buffer *buffer = new_buffer();
    int username_len, flag, recv_res;

    if ((recv_res = recv_buffer_sock_header(new_fd, buffer)) <= 0) {
        free_buffer(&buffer);
        log_error("received incomplete data from socket %d", new_fd);
        return recv_res;
    }

    flag = ((struct SocketHeader *) buffer->data)->flag;

    if (flag != INPUT_USERNAME) {
        free_buffer(&buffer);
        log_error("invalid register-username header (received flag: 0x%02X)", flag);
        return -1;
    }

    if ((recv_res = recv_buffer_int(new_fd, buffer)) <= 0) return recv_res;

    unpack_int(buffer, &username_len);

    if ((recv_res = recv_buffer_string(new_fd, buffer, username_len)) <= 0) return recv_res;

    if (update_players(players, buffer->data + STRING_OFFSET, new_fd) == NULL) {
        free_buffer(&buffer);
        send_invalid_username(new_fd);
        log_error("invalid username - player already logged in");
        return -1;
    }

    free_buffer(&buffer);
    return recv_res;
}

void start() {
#ifdef _WIN32
    DWORD tv = RECV_TIMEOUT_SEC;
#else
    struct timeval tv;
    tv.tv_sec = RECV_TIMEOUT_SEC;
    tv.tv_usec = 0;
#endif

    int listener, new_fd, sender_fd, drawing_fd = -1, recv_res, poll_res;

    struct sockaddr_storage remote_addr; // Client address
    socklen_t addr_len;

    char remote_ip[INET6_ADDRSTRLEN];

    struct Game *game = (struct Game *) malloc(sizeof(struct Game));

    game->fd_count = 0;
    game->fd_size = BACKLOG;
    game->pfds = (struct pollfd *) malloc(sizeof(struct pollfd *) * game->fd_size);
    game->players = new_players();

    struct Player *cur_player;

    if ((listener = get_listener_socket()) == -1) {
        log_error("error getting listening socket");
        exit(EXIT_FAILURE);
    }

    log_info("server listening on port %s", PORT);

    add_to_pfds(&game->pfds, listener, &game->fd_count, &game->fd_size);

    for (;;) {
        poll_res = poll(game->pfds, game->fd_count, POLL_TIMEOUT);
        if (poll_res == -1) {
            log_error("poll-error");
            exit(EXIT_FAILURE);
        } else if (poll_res == 0) {
            log_error("poll-timeout");
        }

        for (int i = 0; i < game->fd_count; i++) {

            if (!(game->pfds[i].revents & POLLIN)) continue;

            if (game->pfds[i].fd == listener) {

                addr_len = sizeof remote_addr;
                new_fd = accept(listener, (struct sockaddr *) &remote_addr, &addr_len);

                if (new_fd == -1) {
                    log_error("accept");
                } else {
                    setsockopt(new_fd, SOL_SOCKET, SO_RCVTIMEO, (const char *) &tv, sizeof tv);

                    if (manage_logging_player(new_fd, game->players) == -1) {
                        log_error("closing connection for socket %d", new_fd);
                        close(new_fd);
                        continue;
                    }

                    add_to_pfds(&game->pfds, new_fd, &game->fd_count, &game->fd_size);

                    log_info("poll-server: new connection from %s on socket %d (user: %s)",
                             inet_ntop(remote_addr.ss_family,
                                       get_in_addr((struct sockaddr *) &remote_addr),
                                       remote_ip, INET6_ADDRSTRLEN),
                             new_fd,
                             get_player_by_fd(game->players, new_fd)->username);

                    log_info("connected clients: %d", game->fd_count - 1);
                }
            } else {
                sender_fd = game->pfds[i].fd;

                // Unknown player
                if ((cur_player = get_player_by_fd(game->players, sender_fd)) == NULL) {
                    log_error("internal error: unknown player connected - disconnecting malicious client");
                    disconnect_fd(game->pfds, i, &game->fd_count);
                    continue;
                }

                recv_res = manage_client_data(game, drawing_fd, sender_fd, listener, cur_player);

                // Known player
                if (recv_res <= 0) {
                    if (recv_res == 0)
                        log_info("poll-server: socket %d hung up (user: %s)",
                                 sender_fd, cur_player->username);
                    else
                        log_error("recv: socket %d is invalid, closing (user: %s), err_code: %d",
                                  sender_fd, cur_player->username, recv_res);
                    remove_player_from_game(game, sender_fd, i);
                }
            }
        }
    }
}