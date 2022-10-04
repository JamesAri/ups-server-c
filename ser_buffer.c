#include <stdlib.h>
#include "ser_buffer.h"

struct Buffer *new_buffer() {
    struct Buffer *b = malloc(sizeof(struct Buffer));

    b->data = malloc(INITIAL_SIZE);
    b->size = INITIAL_SIZE;
    b->next = 0;

    return b;
}

void reserve_space(struct Buffer *b, int bytes) {
    if((b->next + bytes) > b->size) {
        b->data = realloc(b->data, b->size * 2);
        b->size *= 2;
    }
}

void free_buffer(struct Buffer **buffer) {
    free((*buffer)->data);
    free((*buffer));
    (*buffer) = NULL;
}