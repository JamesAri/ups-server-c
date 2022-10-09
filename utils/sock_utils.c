#include "sock_utils.h"
#include "../server.h"
#include "log.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <poll.h>

#ifdef _WIN32

#else

#endif

int recvall(int s, void *buf, int *len) {
    int total = 0;        // how many bytes we've received.
    int bytesleft = *len; // how many bytes we have left to receive
    int n;

    while (total < *len) {
        n = (int) recv(s, buf + total, bytesleft, 0);
        if (n == -1 || n == 0) { break; }
        total += n;
        bytesleft -= n;
    }

    *len = total; // return number actually received here

    return n;
}

int sendall(int s, void *buf, int *len) {
    int total = 0;        // how many bytes we've sent
    int bytesleft = *len; // how many we have left to send
    int n;

    while (total < *len) {
        n = (int) send(s, buf + total, bytesleft, 0);
        if (n == -1 || n == 0) { break; }
        total += n;
        bytesleft -= n;
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

int get_listener_socket() {
    int listener, rv, yes = 1;

    // hints - settings for *ai creation
    // *ai - list of available addrinfo structures
    // *p - temp addrinfo to loop over with
    struct addrinfo hints, *ai, *p;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE; // use with NULL as first param in getaddrinfo for auto-IP detection.

    if ((rv = getaddrinfo(NULL, PORT, &hints, &ai)) != 0) {
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

    if (listen(listener, BACKLOG) == -1) return -1;

    return listener;
}

void add_to_pfds(struct pollfd *pfds[], int newfd, int *fd_count, int *fd_size) {
    if (*fd_count == *fd_size) {
        *fd_size *= 2;

        *pfds = realloc(*pfds, sizeof(**pfds) * (*fd_size));
    }

    (*pfds)[*fd_count].fd = newfd;
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

void disconnect_fd(struct pollfd pfds[], int i, int *fd_count) {
    close(pfds[i].fd);
    del_from_pfds(pfds, i, fd_count);
}

