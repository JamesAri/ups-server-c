#include "sock_utils.h"
#include "log.h"
#include "../model/player.h"

#include <sys/socket.h>
#include <poll.h>
#include <sys/time.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>

//#include <netinet/in.h>


// ======================================================================= //
//                            SOCKET UTILS                                 //
// ======================================================================= //

int recvall(int s, void *buf, int *len) {
    int total = 0;        // how many bytes we've received.
    int bytes_left = *len; // how many bytes we have left to receive
    int n;

    while (total < *len) {
        n = (int) recv(s, buf + total, bytes_left, 0);
        if (n == -1 || n == 0) { break; }
        total += n;
        bytes_left -= n;
    }

    *len = total; // return number actually received here

    return n;
}

int sendall(int s, void *buf, int *len) {
    int total = 0;        // how many bytes we've sent
    int bytes_left = *len; // how many we have left to send
    int n;

    while (total < *len) {
        n = (int) send(s, buf + total, bytes_left, 0);
        if (n == -1 || n == 0) { break; }
        total += n;
        bytes_left -= n;
    }

    *len = total; // return number actually sent

    return n;
}

void *get_in_addr(struct sockaddr *sa) {
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in *) sa)->sin_addr);
    }

    return &(((struct sockaddr_in6 *) sa)->sin6_addr);
}

int get_listener_socket(char *port, int backlog) {
    int listener, rv, yes = 1;

    // hints - settings for *ai creation
    // *ai - list of available addrinfo structures
    // *p - temp addrinfo to loop over with
    struct addrinfo hints, *ai, *p;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE; // use with NULL as first param in getaddrinfo for auto-IP detection.

    if ((rv = getaddrinfo(NULL, port, &hints, &ai)) != 0) {
        log_error("selecting server error: %s\n", gai_strerror(rv));
        exit(EXIT_FAILURE);
    }

    for (p = ai; p != NULL; p = p->ai_next) {
        listener = socket(p->ai_family, p->ai_socktype, p->ai_protocol);

        if (listener < 0) continue;

        // Reuse address on socket-level (if possible)
        setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));

        if (bind(listener, p->ai_addr, p->ai_addrlen) < 0) {
            close(listener);
            continue;
        }

        break;
    }

    freeaddrinfo(ai);

    if (p == NULL) return -1;

    if (listen(listener, backlog) == -1) return -1;

    return listener;
}

int set_socket_timeout(int fd, long timeout_sec) {
#ifdef _WIN32
#include <winsock2.h>
    DWORD tv = timeout_sec;
#else
    struct timeval tv;
    tv.tv_sec = timeout_sec;
    tv.tv_usec = 0;
#endif
    if (setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, (const char *) &tv, sizeof tv) < 0)
        return -1;
    if (setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, (const char *) &tv, sizeof tv) < 0)
        return -1;
    return 0;
}


// ======================================================================= //
//                              POLL UTILS                                 //
// ======================================================================= //

void add_to_pfds(struct pollfd *pfds[], int new_fd, int *fd_count, int *fd_size) {
    if (*fd_count == *fd_size) {
        *fd_size *= 2;

        *pfds = realloc(*pfds, sizeof(*(*pfds)) * (*fd_size));
    }

    (*pfds)[*fd_count].fd = new_fd;
    (*pfds)[*fd_count].events = POLLIN; // Check ready-to-read

    (*fd_count)++;
}

void del_from_pfds(struct pollfd pfds[], int i, int *fd_count) {
    pfds[i] = pfds[*fd_count - 1];
    pfds[*fd_count - 1].fd = 0;
    pfds[*fd_count - 1].events = 0;
    pfds[*fd_count - 1].revents = 0;

    (*fd_count)--;
}

void del_from_pfds_by_fd(struct pollfd pfds[], int fd, int *fd_count) {
    for (int i = 0; i < *fd_count; i++) {
        if (pfds[i].fd == fd) {
            del_from_pfds(pfds, i, fd_count);
            return;
        }
    }
}

void disconnect_fd(struct pollfd pfds[], int fd, int *fd_count) {
    close(fd);
    del_from_pfds_by_fd(pfds, fd, fd_count);
}

void free_pfds(struct pollfd **pfds) {
    free(*pfds);
    (*pfds) = NULL;
}

void print_pfds(struct pollfd *pfds, int fd_size) {
    for (int i = 0; i < fd_size; i++) {
        fprintf(stderr, "%d, ", pfds[i].fd);
    }
    fprintf(stderr, "\n");
}

