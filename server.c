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

int broadcast(struct pollfd *pfds, int fd_count, int listener,
              int sender_fd, void *buffer, int length) {
//TODO:  make with Buffer struct
    for (int j = 0; j < fd_count; j++) {
        int dest_fd = pfds[j].fd;
        if (dest_fd != listener && dest_fd != sender_fd) {
            int len = length; // the amount of bytes we want to send from buffer.
            if (sendall(dest_fd, buffer, &len) == -1) {
                perror("send");
                return -1;
            }
            if (len != length) {
                perror("sent invalid data");
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
    struct SocketHeader *sock_header = new_sock_header(WRONG_GUESS);
    int send_res;
    send_res = broadcast(pfds, fd_count, listener, drawing_fd, sock_header, sizeof(struct SocketHeader));
    free_sock_header(&sock_header);
    return send_res;
}

int manage_drawing_player(struct SocketHeader *sock_header, int sender_fd, struct Buffer *buffer) {
    int temp, recv_res;
    if (sock_header->flag != CANVAS) {
        fprintf(stderr, "invalid header: drawing player error - removing from game\n");
        return -1;
    }
    serialize_sock_header(CANVAS, buffer);

    reserve_space(buffer, CANVAS_BUF_SIZE);
    temp = CANVAS_BUF_SIZE;
    recv_res = recvall(sender_fd, buffer->data + buffer->next, &temp);
    buffer->next += CANVAS_BUF_SIZE;

    return recv_res;
}

int manage_guessing_player(struct SocketHeader *sock_header, int sender_fd, struct Buffer *buffer, char *guess_word) {
    int guess_str_len, recv_res, temp;
    if (sock_header->flag != CHAT) {
        fprintf(stderr, "invalid header: guessing player error - removing from game\n");
        return -1;
    }

    temp = sizeof(int);
    if (recvall(sender_fd, &guess_str_len, &temp) <= 0) return -1;

    char str_buf[guess_str_len];
    temp = guess_str_len;
    recv_res = recvall(sender_fd, str_buf, &temp);

    if (recv_res <= 0) {
        return recv_res;
    }

    serialize_sock_header(CHAT, buffer);
    serialize_int(guess_str_len, buffer);
    serialize_string(str_buf, buffer);

    if (!strcmp(guess_word, str_buf)) send_correct_guess(sender_fd);
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
        broadcast(pfds, *fd_count, listener, sender_fd, buffer->data, buffer->next);
    }

    free(sock_header);
    free_buffer(&buffer);
    return 0;
}


int manage_new_player(int newfd, struct Players *players) {
    struct SocketHeader *sock_header = new_sock_header(EMPTY);
    if (sock_header == NULL) return -1;
    char username[WORD_BUF_SIZE];
    int username_len, temp;

    temp = sizeof(struct SocketHeader);
    if (recvall(newfd, sock_header, &temp) <= 0) {
        free(sock_header);
        fprintf(stderr, "received incomplete data from socket %d\n", newfd);
        return -1;
    }

    if (sock_header->flag != INPUT_USERNAME) {
        fprintf(stderr, "invalid register-username header (recieved flag: 0x%02X)\n", sock_header->flag);
        return -1;
    }
    free(sock_header);
    temp = sizeof(int);
    if (recvall(newfd, &username_len, &temp) <= 0) return -1;
    username_len = ntohl(username_len);
    if (recvall(newfd, username, &username_len) <= 0) return -1;

    if (update_players(players, username, newfd) == NULL) {
        send_invalid_username(newfd);
        fprintf(stderr, "invalid username - player already logged in\n");
        return -1;
    }
    return 0;
}

void start() {
    int listener, drawing_fd = -1; // file descriptors - sockets
    int newfd;        // Newly accept()ed socket descriptor
    struct sockaddr_storage remoteaddr; // Client address
    socklen_t addrlen;

    struct Players *players = (struct Players *) malloc(sizeof(struct Players));
    players->playerList = NULL;
    players->count = 0;

    char remoteIP[INET6_ADDRSTRLEN];

    int fd_count = 0;
    int fd_size = BACKLOG;
    struct pollfd *pfds = (struct pollfd *) malloc(sizeof *pfds * fd_size);

    listener = get_listener_socket();

    if (listener == -1) {
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

            if (!(pfds[i].revents & POLLIN)) {
                continue;
            }
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