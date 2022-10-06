#include "server.h"
#include "model/player.h"
#include "utils/sock_header.h"
#include "utils/sock_utils.h"
#include "utils/serialization.h"
#include "utils/debug.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <poll.h>


int recv_buffer(int fd, struct Buffer *buffer, int size) {
    int res_recv, temp = size;
    reserve_space(buffer, size);
    res_recv = recvall(fd, buffer->data + buffer->next, &temp);
    buffer->next += size;
    return res_recv;
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
    int temp = buffer->next;
    return sendall(dest_fd, buffer->data, &temp);
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
    serialize_sock_header(flag, buffer);
    serialize_int((int) strlen(msg), buffer);
    serialize_string(msg, buffer);
    res_send = send_buffer(fd, buffer);
    free_buffer(&buffer);
    return res_send;
}

int broadcast(struct pollfd *pfds, int fd_count, int listener, int sender_fd, struct Buffer *buffer) {

    for (int j = 0; j < fd_count; j++) {
        int dest_fd = pfds[j].fd;
        if (dest_fd != listener && dest_fd != sender_fd) {
            if (send_buffer(dest_fd, buffer) <= 0) {
                fprintf(stderr, "broadcast error\n");
                return -1;
            }
        }
    }
    return 0;
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

int send_game_guess_start(struct pollfd *pfds, int fd_count, int listener, int drawing_fd) {
    int send_res;
    struct Buffer *buffer = new_buffer();
    serialize_sock_header(WRONG_GUESS, buffer);
    send_res = broadcast(pfds, fd_count, listener, drawing_fd, buffer);
    free_buffer(&buffer);
    return send_res;
}

int manage_drawing_player(struct SocketHeader *sock_header, int sender_fd, struct Buffer *buffer) {
    if (sock_header->flag != CANVAS) {
        fprintf(stderr, "invalid header: drawing player error - removing from game\n");
        return -1;
    }

    serialize_sock_header(CANVAS, buffer);
    return recv_buffer(sender_fd, buffer, CANVAS_BUF_SIZE);
}

int manage_guessing_player(struct SocketHeader *sock_header, int sender_fd, struct Buffer *buffer, char *guess_word) {
    int guess_str_len, recv_res, *buf_int_ptr;
    char *buf_str_ptr;
    if (sock_header->flag != CHAT) {
        fprintf(stderr, "invalid header: guessing player error - removing from game\n");
        return -1;
    }

    // buffer = [SOCK_HEADER]
    // buffer = [SOCK_HEADER][INT]
    // buffer = [SOCK_HEADER][INT][STRING(len: guess_str_len)]
    //               1B       4B              var B

    serialize_sock_header(CHAT, buffer);

    buf_int_ptr = buffer->data + buffer->next;
    if ((recv_res = recv_buffer_int(sender_fd, buffer)) <= 0) return recv_res;

    guess_str_len = ntohl(*buf_int_ptr);

    buf_str_ptr = buffer->data + buffer->next;
    if ((recv_res = recv_buffer_string(sender_fd, buffer, guess_str_len)) <= 0) return recv_res;

    if (!strcmp(guess_word, buf_str_ptr)) send_correct_guess(sender_fd);
    else send_wrong_guess(sender_fd);
    return recv_res;
}


int broadcast_received_data(struct pollfd *pfds, int *fd_count, int pfds_i,
                            struct Players *players, int drawing_fd, int listener) {
    char *guess_word = "coffee";
    struct Player *player;
    int temp, recv_res, sender_fd = pfds[pfds_i].fd;
    if ((player = get_player_by_fd(players, sender_fd)) == NULL) {
        fprintf(stderr, "unknown player\n");
        return -1;
    }
    struct SocketHeader *sock_header = new_sock_header(EMPTY);
    struct Buffer *buffer = new_buffer();

    temp = sizeof(struct SocketHeader);
    recvall(sender_fd, sock_header, &temp);

    if (drawing_fd == player->fd)
        recv_res = manage_drawing_player(sock_header, sender_fd, buffer);
    else
        recv_res = manage_guessing_player(sock_header, sender_fd, buffer, guess_word);

    if (recv_res <= 0) {
        if (recv_res == 0) fprintf(stderr, "pollserver: socket %d hung up\n", sender_fd);
        else fprintf(stderr, "recv: socket %d is invalid, closing\n", sender_fd);
        del_from_pfds(pfds, pfds_i, fd_count);
        close(sender_fd);
        player->is_online = 0;
    } else {
        broadcast(pfds, *fd_count, listener, sender_fd, buffer);
    }

    free_sock_header(&sock_header);
    free_buffer(&buffer);
    return 0;
}


int manage_new_player(int newfd, struct Players *players) {
    struct Buffer *buffer = new_buffer();
    int username_len, flag, recv_res, *buf_int_ptr;
    char *buf_string_ptr;

    if ((recv_res = recv_buffer_sock_header(newfd, buffer)) <= 0) {
        free_buffer(&buffer);
        fprintf(stderr, "received incomplete data from socket %d\n", newfd);
        return recv_res;
    }

    flag = ((struct SocketHeader *) buffer->data)->flag;
    if (flag != INPUT_USERNAME) {
        free_buffer(&buffer);
        fprintf(stderr, "invalid register-username header (received flag: 0x%02X)\n", flag);
        return -1;
    }


    buf_int_ptr = buffer->data + buffer->next;
    if ((recv_res = recv_buffer_int(newfd, buffer)) <= 0) return recv_res;
    username_len = ntohl(*buf_int_ptr);

    buf_string_ptr = buffer->data + buffer->next;
    if ((recv_res = recv_buffer_string(newfd, buffer, username_len)) <= 0) return recv_res;

    if (update_players(players, buf_string_ptr, newfd) == NULL) {
        free_buffer(&buffer);
        send_invalid_username(newfd);
        fprintf(stderr, "invalid username - player already logged in\n");
        return -1;
    }

    free_buffer(&buffer);
    return recv_res;
}

void start() {
    int listener, newfd, drawing_fd = -1;
    struct sockaddr_storage remoteaddr; // Client address
    socklen_t addrlen;

    char remoteIP[INET6_ADDRSTRLEN];

    int fd_count = 0;
    int fd_size = BACKLOG;
    struct pollfd *pfds = (struct pollfd *) malloc(sizeof *pfds * fd_size);

    struct Players *players = new_players();

    if ((listener = get_listener_socket()) == -1) {
        fprintf(stderr, "error getting listening socket\n");
        exit(1);
    }

    fprintf(stderr, "server listening on port %s\n", PORT);

    add_to_pfds(&pfds, listener, &fd_count, &fd_size);

    for (;;) {
        int poll_res = poll(pfds, fd_count, POLL_TIMEOUT);
        if (poll_res == -1) {
            fprintf(stderr, "poll-error\n");
            exit(1);
        } else if (poll_res == 0) {
            fprintf(stderr, "poll-timeout\n");
        }

        for (int i = 0; i < fd_count; i++) {

            if (!(pfds[i].revents & POLLIN)) continue;

            if (pfds[i].fd == listener) {

                addrlen = sizeof remoteaddr;
                newfd = accept(listener, (struct sockaddr *) &remoteaddr, &addrlen);

                if (newfd == -1) {
                    fprintf(stderr, "accept\n");
                } else {

                    if (manage_new_player(newfd, players) == -1) {
                        fprintf(stderr, "closing connection for socket %d\n", newfd);
                        close(newfd);
                        continue;
                    }

                    add_to_pfds(&pfds, newfd, &fd_count, &fd_size);

                    fprintf(stdout, "pollserver: new connection from %s on socket %d (user: %s)\n"
                                    "fd_count: %d\n",
                            inet_ntop(remoteaddr.ss_family,
                                      get_in_addr((struct sockaddr *) &remoteaddr),
                                      remoteIP, INET6_ADDRSTRLEN),
                            newfd,
                            get_player_by_fd(players, newfd)->username,
                            fd_count);
                }
            } else {
                broadcast_received_data(pfds, &fd_count, i, players, drawing_fd, listener);
            }
        }
    }
}