#include "serialization.h"
#include "sock_header.h"

#include <stdlib.h>
#include <string.h>

struct Buffer *new_buffer() {
    struct Buffer *b = malloc(sizeof(struct Buffer));

    b->data = malloc(INITIAL_SIZE);
    b->size = INITIAL_SIZE;
    b->next = 0;

    return b;
}

void reserve_space(struct Buffer *b, int bytes) {
    if ((b->next + bytes) > b->size) {
        b->data = realloc(b->data, b->size * 2);
        b->size *= 2;
    }
}

void free_buffer(struct Buffer **buffer) {
    free((*buffer)->data);
    free((*buffer));
    (*buffer) = NULL;
}

void serialize_sock_header(int flag, struct Buffer *b) {
    struct SocketHeader *sock_header = (struct SocketHeader *) malloc(sizeof(struct SocketHeader));
    reserve_space(b, sizeof(struct SocketHeader));
    memcpy(((char *) b->data) + b->next, sock_header, sizeof(struct SocketHeader));
    b->next += sizeof(struct SocketHeader);
    free(sock_header);
}

void serialize_int(int x, struct Buffer *b) {
    x = htonl(x);

    reserve_space(b, sizeof(int));
    memcpy(((char *) b->data) + b->next, &x, sizeof(int));
    b->next += sizeof(int);
}

void serialize_string(char *string, struct Buffer *b) {
    int str_len = (int) strlen(string);

    reserve_space(b, str_len);
    memcpy(((char *) b->data) + b->next, string, str_len);
    b->next += str_len;
}