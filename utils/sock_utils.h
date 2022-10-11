#include <sys/socket.h>
#include <poll.h>

#ifndef UPS_SERVER_C_SOCKET_UTILS_H
#define UPS_SERVER_C_SOCKET_UTILS_H


int recvall(int s, void *buf, int *len);

int sendall(int s, void *buf, int *len);

void *get_in_addr(struct sockaddr *sa);

int get_listener_socket();

void add_to_pfds(struct pollfd *pfds[], int newfd, int *fd_count, int *fd_size);

void del_from_pfds(struct pollfd pfds[], int i, int *fd_count);

void del_from_pfds_by_fd(struct pollfd pfds[], int fd, int *fd_count);

void disconnect_fd(struct pollfd pfds[], int fd, int *fd_count);

void print_pfds(struct pollfd *pfds);

void free_pfds(struct pollfd **pfds);

#endif //UPS_SERVER_C_SOCKET_UTILS_H
