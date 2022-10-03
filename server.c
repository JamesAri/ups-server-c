#include "server.h"
#include "player.h"
#include "s_header.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <poll.h>

int recvall(int s, void *buf, int *len) {
    int total = 0;        // how many bytes we've received.
    int bytesleft = *len; // how many bytes we have left to receive
    int n;

    while(total < *len) {
        n = (int)recv(s, buf+total, bytesleft, 0);
        if (n == -1 || n == 0) { break; }
        total += n;
        bytesleft -= n;
    }

    *len = total; // return number actually received here

    return n;
}

int sendall(int s, char *buf, int *len) {
    int total = 0;        // how many bytes we've sent
    int bytesleft = *len; // how many we have left to send
    int n;

    while(total < *len) {
        n = (int)send(s, buf+total, bytesleft, 0);
        if (n == -1 || n == 0) { break; }
        total += n;
        bytesleft -= n;
    }

    *len = total; // return number actually sent here

    return n;
}

/*
 * Gets address based on family of the passed sockaddr struct.
 */
void *get_in_addr(struct sockaddr *sa) {
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

/*
 * Returns server listening socket.
 */
int get_listener_socket() {
    int listener;     // Listening socket descriptor
    int yes=1;        // For setsockopt() SO_REUSEADDR, below
    int rv;

    // *ai - list of available addrinfo structures
    // *p - temp addrinfo to loop over with
    // hints - settings for *ai creation
    struct addrinfo hints, *ai, *p;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE; // use with NULL as first param in getaddrinfo for auto-IP detection.
    if ((rv = getaddrinfo(NULL, PORT, &hints, &ai)) != 0) {
        fprintf(stderr, "selectserver: %s\n", gai_strerror(rv));
        exit(1);
    }

    for(p = ai; p != NULL; p = p->ai_next) {
        listener = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (listener < 0) {
            continue;
        }

        // Reuse address on socket-level (if possible)
        setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));

        if (bind(listener, p->ai_addr, p->ai_addrlen) < 0) {
            close(listener);
            continue;
        }

        break;
    }

    freeaddrinfo(ai);

    if (p == NULL) {
        return -1;
    }

    if (listen(listener, BACKLOG) == -1) {
        return -1;
    }

    return listener;
}


void add_to_pfds(struct pollfd *pfds[], int newfd, int *fd_count, int *fd_size) {
    // If we don't have room, add more space in the pfds array
    if (*fd_count == *fd_size) {
        *fd_size *= 2;

        *pfds = realloc(*pfds, sizeof(**pfds) * (*fd_size));
    }

    (*pfds)[*fd_count].fd = newfd;
    (*pfds)[*fd_count].events = POLLIN; // Check ready-to-read

    (*fd_count)++;
}

// Remove an index from the set
void del_from_pfds(struct pollfd pfds[], int i, int *fd_count) {
    // Copy the one from the end over this one
    pfds[i] = pfds[*fd_count-1];

    (*fd_count)--;
}

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

int send_invalid_username(int fd) {
    struct SocketHeader *s_header = (struct SocketHeader *)malloc(sizeof(struct SocketHeader));
    s_header->flag = INVALID_USERNAME;
    int res = (int)send(fd, s_header, sizeof(struct SocketHeader), 0);
    if (res - sizeof(struct SocketHeader)) return -1;
    return 0;
}

int start() {
    int listener, drawing_fd = -1; // file descriptors - sockets
    int poll_count; // Number of currently waiting fds.
    int newfd;        // Newly accept()ed socket descriptor
    struct sockaddr_storage remoteaddr; // Client address
    socklen_t addrlen;

    struct Players *players = (struct Players *)malloc(sizeof(struct Players));
    players->playerList = NULL;
    players->count = 0;

    struct SocketHeader *s_header = (struct SocketHeader *)malloc(sizeof(struct SocketHeader));

    char remoteIP[INET6_ADDRSTRLEN];

    int fd_count = 0;
    int fd_size = BACKLOG;
    struct pollfd *pfds = (struct pollfd *)malloc(sizeof *pfds * fd_size);
    struct SocketHeader *sock_header = (struct SocketHeader *)malloc(sizeof(struct SocketHeader));

    listener = get_listener_socket();

    if (listener == -1) {
        fprintf(stderr, "error getting listening socket\n");
        exit(1);
    }
    fprintf(stderr, "server listening on port %s", PORT);
    add_to_pfds(&pfds, listener, &fd_count, &fd_size);

    for(;;) {
        poll_count = poll(pfds, fd_count, -1);

        if (poll_count == -1) {
            fprintf(stderr, "poll");
            exit(1);
        }

        for(int i = 0; i < fd_count; i++) {

            if (!(pfds[i].revents & POLLIN)) {
                continue;
            }

            if (pfds[i].fd == listener) {
                addrlen = sizeof remoteaddr;
                newfd = accept(listener,
                               (struct sockaddr *)&remoteaddr,
                               &addrlen);

                if (newfd == -1) {
                    fprintf(stderr, "accept");
                } else {
                    char username[WORD_BUF_SIZE];
                    struct Player *player;
                    int len;

                    recv(newfd, sock_header, sizeof(struct SocketHeader), 0);
                    if (sock_header->flag != INPUT_USERNAME) {
                        fprintf(stderr, "invalid username protocol");
                        close(newfd);
                        continue;
                    }

                    recv(newfd, &len, sizeof(int), 0);
                    len = ntohs(len);
                    recvall(newfd, username, &len);
                    player = update_players(players, username, newfd);

                    if (player == NULL) { // User with this username is already online
                        send_invalid_username(newfd);
                        continue;
                    }

                    if (players->count == 1) {
                        // TODO
                        // drawing_fd = newfd;
                    }
                    fprintf(stdout, "\nListener: newfd->%d, fd_size->%d\n", newfd, fd_size);
                    add_to_pfds(&pfds, newfd, &fd_count, &fd_size);

                    fprintf(stdout, "pollserver: new connection from %s on socket %d (user: %s)\n",
                           inet_ntop(remoteaddr.ss_family,
                                     get_in_addr((struct sockaddr *) &remoteaddr),
                                     remoteIP, INET6_ADDRSTRLEN),
                           newfd,
                           username);

//                    char msg[WORD_BUF_SIZE];
//                    int raw_data_size = sizeof msg + sizeof(struct SocketHeader);
//                    unsigned char raw_data[raw_data_size];
//                    raw_data[0] = OK;
//                    sprintf(&((char *)raw_data)[1], "User %s joined the game!", username);
//                    fprintf(stdout, "%s\n", &raw_data[1]);
//                    if (broadcast(pfds, fd_count, listener, -1, raw_data, raw_data_size) == -1) {
//                        fprintf(stderr, "broadcast failed");
//                    }
                }
            } else {
                int sender_fd = pfds[i].fd;
                int expected_bytes, recv_res, len, padding;
                struct Player *player;

                if ((player = get_player_by_fd(players, sender_fd)) == NULL) {
                    fprintf(stderr, "unknown player");
                    continue;
                }
                recv(sender_fd, sock_header, sizeof(struct SocketHeader), 0);
                // TODO FIX THE MESS BELOW

                if (drawing_fd == player->fd) {
                    if(sock_header->flag != CANVAS) {
                        fprintf(stderr, "drawing player error - removing from game");
                        del_from_pfds(pfds, drawing_fd, &fd_count);
                        player->is_online = 0;
                        continue;
                    }
                    len = CANVAS_BUF_SIZE;
                    padding = sizeof(struct SocketHeader);
                } else {
                    if(sock_header->flag != CHAT) {
                        fprintf(stderr, "guessing player error - removing from game");
                        del_from_pfds(pfds, sender_fd, &fd_count);
                        player->is_online = 0;
                        continue;
                    }
                    recv(sender_fd, &len, sizeof(int), 0);
                    len = ntohs(len);
                    padding = sizeof(struct SocketHeader) + sizeof(int);
                }
                char buffer[padding + len];
                recv_res = recvall(sender_fd, buffer + padding, &len);
                // TODO:    construct new buffer with data -> [SOCKET_HEADER][USED_BUFFER] -> 1 (B) + EXPECTED_TYPES (B)
                //          and broadcast it.
                buffer[0] = (char)sock_header->flag;
                if (sock_header->flag == CHAT) buffer[1] = htons(len);

                if (recv_res <= 0) {
                    if (recv_res == 0) fprintf(stderr, "pollserver: socket %d hung up\n", sender_fd);
                    else fprintf(stderr, "recv: socket %d lost\n", sender_fd);
                    player->is_online = 0;
                    del_from_pfds(pfds, i, &fd_count);
                    close(sender_fd);
                } else {
                    broadcast(pfds, fd_count, listener, sender_fd, buffer, padding + len);
                }
            }
        }
    }
    return 0;
}