#include "server.h"
#include "player.h"
#include "s_header.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <poll.h>


/*
 * Sends all possibly remaining (pending) data.
 */
int sendall(int s, char *buf, int *len)
{
    int total = 0;        // how many bytes we've sent
    int bytesleft = *len; // how many we have left to send
    int n;

    while(total < *len) {
        n = (int)send(s, buf+total, bytesleft, 0);
        if (n == -1) { break; }
        total += n;
        bytesleft -= n;
    }

    *len = total; // return number actually sent here

    return n==-1?-1:0; // return -1 on failure, 0 on success
}

/*
 * Gets address based on family of the passed sockaddr struct.
 */
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

/*
 * Returns server listening socket.
 */
int get_listener_socket()
{
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


void add_to_pfds(struct pollfd *pfds[], int newfd, int *fd_count, int *fd_size)
{
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
void del_from_pfds(struct pollfd pfds[], int i, int *fd_count)
{
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
    int res = send(fd, s_header, sizeof(struct SocketHeader), 0);
    if (res - sizeof(struct SocketHeader)) return -1;
    return 0;
}

int start()
{
    int listener, drawing_fd; // file descriptors - sockets
    int poll_count; // Number of currently waiting fds.
    int newfd;        // Newly accept()ed socket descriptor
    struct sockaddr_storage remoteaddr; // Client address
    socklen_t addrlen;

    struct Players *players = (struct Players *)malloc(sizeof(struct Players));
    players->playerList = NULL;
    players->count = 0;

    struct SocketHeader *s_header = (struct SocketHeader *)malloc(sizeof(struct SocketHeader));

    char canvas_buf[CANVAS_BUF_SIZE];
    char word_buf[WORD_BUF_SIZE];

    char remoteIP[INET6_ADDRSTRLEN];

    // Start off with room for 5 connections
    int fd_count = 0;
    int fd_size = 5;
    struct pollfd *pfds = malloc(sizeof *pfds * fd_size);

    // Set up and get a listening socket
    listener = get_listener_socket();

    if (listener == -1) {
        fprintf(stderr, "error getting listening socket\n");
        exit(1);
    }
    fprintf(stderr, "server listening on port %s", PORT);

    // Register listener
    add_to_pfds(&pfds, listener, &fd_count, &fd_size);

//    pfds[0].fd = listener;
//    pfds[0].events = POLLIN; // Report ready to read on incoming connection
//    fd_count = 1; // For the listener
    // Main loop
    for(;;) {
        // -1 as timeout means infinitely.
        poll_count = poll(pfds, fd_count, -1);


        if (poll_count == -1) {
            fprintf(stderr, "poll");
            exit(1);
        }

        // Run through the existing connections looking for data to read
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

                    add_to_pfds(&pfds, newfd, &fd_count, &fd_size);

                    char username[WORD_BUF_SIZE];
                    recv(newfd, username, WORD_BUF_SIZE, 0);
                    struct Player *player = update_players(players, username, newfd);
                    fprintf(stderr, "\npfds[i]->%d, listener->%d, newfd->%d\n", pfds[i].fd, listener, newfd);

                    if (!players->count) {
                        drawing_fd = newfd;
                    }

                    if (player == NULL) { // User with this username is already online
                        send_invalid_username(newfd);
                        del_from_pfds(pfds, newfd, &fd_count);
                        continue;
                    }

                    char msg[WORD_BUF_SIZE];

                    sprintf(msg, "User %s joined the game!", username);
                    fprintf(stderr, "%s\n", msg);

                    if (broadcast(pfds, fd_count, listener, -1, msg, WORD_BUF_SIZE) == -1) {
                        perror("broadcast failed");
                    }

                    fprintf(stderr, "pollserver: new connection from %s on socket %d (user: %s)\n",
                           inet_ntop(remoteaddr.ss_family,
                                     get_in_addr((struct sockaddr *) &remoteaddr),
                                     remoteIP, INET6_ADDRSTRLEN),
                           newfd,
                           username);
                }
            } else {
                // If not the listener, we're just a regular client
                int sender_fd = pfds[i].fd;

                struct Player *player = get_player_by_fd(players, sender_fd);
                if (player == NULL) {
                    fprintf(stderr, "unknown player");
                    continue;
                }

                int nbytes, expected_nbytes;
                char *used_buffer;
                drawing_fd = -1; // TODO
                if (drawing_fd == player->fd) {
                    expected_nbytes = sizeof canvas_buf;
                    used_buffer = canvas_buf;
                } else {
                    expected_nbytes = sizeof word_buf;
                    used_buffer = word_buf;
                }
                nbytes = recv(sender_fd, used_buffer, expected_nbytes, 0);

                if (nbytes <= 0) {
                    if (nbytes == 0) {
                        // Connection closed
                        fprintf(stderr, "pollserver: socket %d hung up\n", sender_fd);
                    } else {
                        // Got error
                        fprintf(stderr, "recv: socket %d lost\n", sender_fd);
                    }

                    close(pfds[i].fd);
                    player->is_online = 0;

                    del_from_pfds(pfds, i, &fd_count);

                } else {
                    if (nbytes != expected_nbytes) {
                        fprintf(stderr, "recv - invalid bytes");
                    }
                    // Broadcast
                    for(int j = 0; j < fd_count; j++) {
                        int dest_fd = pfds[j].fd;

                        if (dest_fd != listener && dest_fd != sender_fd) {
                            int len = expected_nbytes; // the amount of bytes we want to send from buffer.
                            if (sendall(dest_fd, used_buffer, &len) == -1) {
                                fprintf(stderr, "send");
                            }
                            if(len != expected_nbytes) perror("sent invalid data");
//                            if (send(dest_fd, word_buf, nbytes, 0) == -1) {
//                                perror("send");
//                            }
                        }
                    }
                }
            }
        }
    }

    return 0;
}