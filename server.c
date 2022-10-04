#include "server.h"
#include "player.h"
#include "s_header.h"
#include "ser_buffer.h"
#include "socket_utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <poll.h>

int broadcast(struct pollfd *pfds, int fd_count, int listener,
        int sender_fd, void *buffer, int length) {

    for(int j = 0; j < fd_count; j++) {
        int dest_fd = pfds[j].fd;
        if (dest_fd != listener && dest_fd != sender_fd) {
            int len = length; // the amount of bytes we want to send from buffer.
            if (sendall(dest_fd, buffer, &len) == -1) {
                perror("send");
                return -1;
            }
            if(len != length){
                perror("sent invalid data");
                return -1;
            }
        }
    }
    return  0;
}

int broadcast_by_header(struct pollfd *pfds, int fd_count, int listener, int sender_fd,
        struct SocketHeader *sock_header, void *data, int data_len) {
    int sock_header_size = sizeof(struct SocketHeader);
    int padding = sock_header_size;
    void *buffer;
    switch(sock_header->flag) {
        case CHAT:
            padding += sizeof(int);
            buffer = malloc(data_len + padding);
            memcpy(buffer, sock_header, sock_header_size);
            unsigned int htonl_int = htonl(data_len);
            memcpy(buffer + sock_header_size, &htonl_int, sizeof(htonl_int));
            break;
        default:
            buffer = malloc (data_len + padding);
            memcpy(buffer, sock_header, sock_header_size);
    }
    memcpy(buffer + padding, data, data_len);
    int buff_size = data_len + padding;
    int res = broadcast(pfds, fd_count, listener, sender_fd, buffer, buff_size);
    free(buffer);
    return res;
}

int broadcast_received_data(struct pollfd *pfds, int *fd_count, int pfds_i,
        struct Players *players, int drawing_fd, int listener) {
    struct Player *player;
    int sender_fd = pfds[pfds_i].fd;
    if ((player = get_player_by_fd(players, sender_fd)) == NULL) {
        fprintf(stderr, "unknown player");
        return -1;
    }
    struct SocketHeader *sock_header = (struct SocketHeader *)malloc(sizeof(struct SocketHeader));
    int data_len, padding, temp;

    temp = sizeof(struct SocketHeader);
    recvall(sender_fd, sock_header, &temp);
    struct Buffer *buffer = (struct Buffer *) malloc(sizeof(struct Buffer));

    reserve_space(buffer, sizeof(struct SocketHeader));
    memcpy(buffer->data + buffer->next, sock_header, sizeof(struct SocketHeader));
    buffer->next += sizeof(struct SocketHeader);

    if (drawing_fd == player->fd) {
        if(sock_header->flag != CANVAS) {
            fprintf(stderr, "drawing player error - removing from game");
            del_from_pfds(pfds, drawing_fd, fd_count);
            player->is_online = 0;
            free(sock_header);
            free_buffer(&buffer);
            return -1;
        }
        data_len = CANVAS_BUF_SIZE;
        padding = sizeof(struct SocketHeader);
        reserve_space(buffer, padding + data_len);

    } else {
        if(sock_header->flag != CHAT) {
            fprintf(stderr, "guessing player error - removing from game");
            del_from_pfds(pfds, sender_fd, fd_count);
            player->is_online = 0;
            free(sock_header);
            free_buffer(&buffer);
            return -1;
        }
        temp = sizeof(int);
        if (recvall(sender_fd, &data_len, &temp) <= 0) {
            free(sock_header);
            free_buffer(&buffer);
            return -1;
        }
        data_len = ntohl(data_len);
        padding = sizeof(struct SocketHeader) + sizeof(data_len);
        reserve_space(buffer, padding + data_len);
        memcpy(buffer->data + buffer->next, &data_len, sizeof(data_len));
        buffer->next += sizeof(data_len);
    }
    temp = data_len;
    int recv_res = recvall(sender_fd, buffer->data, &temp);
    buffer->next += data_len;

    if (recv_res <= 0) {
        if (recv_res == 0) fprintf(stderr, "pollserver: socket %d hung up\n", sender_fd);
        else fprintf(stderr, "recv: socket %d lost\n", sender_fd);
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

int send_correct_guess(int fd) {
    return 0;
}

int send_wrong_guess(int fd) {
    return 0;
}

int send_game_guess_start(struct pollfd *pfds, int *fd_count, int listener, int drawing_fd) {
    return 0;
}

int send_game_draw_start(int drawing_fd) {
    return 0;
}

int send_invalid_username(int fd) {
    struct SocketHeader *s_header = (struct SocketHeader *)malloc(sizeof(struct SocketHeader));
    s_header->flag = INVALID_USERNAME;
    int temp = sizeof(struct SocketHeader);
    int res = (int)sendall(fd, s_header, &temp);
    free(s_header);
    if (res <= 0) return -1;
    return 0;
}

int manage_new_player(int newfd, struct Players *players) {
    struct SocketHeader *sock_header = (struct SocketHeader *)malloc(sizeof(struct SocketHeader));
    if (sock_header == NULL) return -1;
    char username[WORD_BUF_SIZE];
    int username_len, temp;

    temp = sizeof(struct SocketHeader);
    if(recvall(newfd, sock_header, &temp) <= 0) {
        free(sock_header);
        return -1;
    }
    if (sock_header->flag != INPUT_USERNAME) {
        fprintf(stderr, "invalid register-username header\n");
        close(newfd);
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

int start() {
    int listener, drawing_fd = -1; // file descriptors - sockets
    int newfd;        // Newly accept()ed socket descriptor
    struct sockaddr_storage remoteaddr; // Client address
    socklen_t addrlen;

    struct Players *players = (struct Players *)malloc(sizeof(struct Players));
    players->playerList = NULL;
    players->count = 0;

    char remoteIP[INET6_ADDRSTRLEN];

    int fd_count = 0;
    int fd_size = BACKLOG;
    struct pollfd *pfds = (struct pollfd *)malloc(sizeof *pfds * fd_size);

    listener = get_listener_socket();

    if (listener == -1) {
        fprintf(stderr, "error getting listening socket\n");
        exit(1);
    }
    fprintf(stderr, "server listening on port %s", PORT);
    add_to_pfds(&pfds, listener, &fd_count, &fd_size);

    for(;;) {
        if (poll(pfds, fd_count, -1) == -1) {
            fprintf(stderr, "poll");
            exit(1);
        }

        for(int i = 0; i < fd_count; i++) {

            if (!(pfds[i].revents & POLLIN)) {
                continue;
            }
            if (pfds[i].fd == listener) {

                addrlen = sizeof remoteaddr;
                newfd = accept(listener, (struct sockaddr *)&remoteaddr, &addrlen);

                if (newfd == -1) {
                    fprintf(stderr, "accept");
                } else {

                    if (manage_new_player(newfd, players) == -1) continue;

                    add_to_pfds(&pfds, newfd, &fd_count, &fd_size);

                    fprintf(stdout, "pollserver: new connection from %s on socket %d.\n"
                                    "fd_count: %d\n",
                            inet_ntop(remoteaddr.ss_family,
                                      get_in_addr((struct sockaddr *) &remoteaddr),
                                      remoteIP, INET6_ADDRSTRLEN),
                            newfd,
                            fd_count);
                }
            } else {
                broadcast_received_data(pfds, &fd_count, i, players, drawing_fd, listener);
            }
        }
    }
    return 0;
}