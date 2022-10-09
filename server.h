#ifndef UPS_SERVER_C_SERVER_H
#define UPS_SERVER_C_SERVER_H

#include "utils/sock_header.h"

#include <poll.h>


#define PORT "9034"
#define BACKLOG 5
#define POLL_TIMEOUT (-1)

#define MAX_GUESS_LEN 256
#define MAX_STRING_LEN 256
#define CANVAS_BUF_SIZE 1024

struct Game {
    struct Players *players;
    struct pollfd *pfds;
    int fd_size;
    int fd_count;
};

void start();

#endif //UPS_SERVER_C_SERVER_H
