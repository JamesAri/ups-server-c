#include <sys/socket.h>
#include <poll.h>
#include <sys/time.h>
#include "../model/player.h"

#ifndef UPS_SERVER_C_SOCKET_UTILS_H
#define UPS_SERVER_C_SOCKET_UTILS_H

// ======================================================================= //
//                            SOCKET UTILS                                 //
// ======================================================================= //

int recvall(int s, void *buf, int *len);

int sendall(int s, void *buf, int *len);

void *get_in_addr(struct sockaddr *sa);

int get_listener_socket(char *port, int backlog);

int set_socket_timeout(int fd, long timeout_sec);


// ======================================================================= //
//                              POLL UTILS                                 //
// ======================================================================= //

void add_to_pfds(struct pollfd *pfds[], int new_fd, int *fd_count, int *fd_size);

void del_from_pfds(struct pollfd pfds[], int i, int *fd_count);

void del_from_pfds_by_fd(struct pollfd pfds[], int fd, int *fd_count);

void disconnect_fd(struct pollfd pfds[], int fd, int *fd_count);

void print_pfds(struct pollfd *pfds, int fd_size);

void free_pfds(struct pollfd **pfds);

#endif //UPS_SERVER_C_SOCKET_UTILS_H
