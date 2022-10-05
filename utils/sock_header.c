#include "sock_header.h"

#include <stdlib.h>

struct SocketHeader *new_sock_header(int flag) {
    struct SocketHeader *sock_header = (struct SocketHeader *) malloc(sizeof(struct SocketHeader));
    sock_header->flag = flag;
    return sock_header;
}

void free_sock_header(struct SocketHeader **sock_header) {
    free(*sock_header);
    *sock_header = NULL;
}