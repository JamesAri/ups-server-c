#include "serialization.h"
#include "sock_header.h"

#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <stdio.h>


// ======================================================================= //
//                              BUFFER                                     //
// ======================================================================= //

struct Buffer *new_buffer() {
    struct Buffer *buffer = malloc(sizeof(struct Buffer));

    buffer->data = calloc(INITIAL_SIZE, sizeof(char));
    buffer->size = INITIAL_SIZE * sizeof(char);
    buffer->next = 0;

    return buffer;
}

void reserve_space(struct Buffer *buffer, int bytes) {
    int new_size, min_size = buffer->next + bytes;
    if (min_size > buffer->size) {
        new_size = (min_size > buffer->size * 2) ? min_size : buffer->size * 2;
        buffer->data = realloc(buffer->data, new_size);
        if (buffer->data == NULL) {
            fprintf(stderr, "realloc error - reserving space for buffer");
            exit(1); // TODO
        }
        buffer->size = new_size;
    }
    memset(buffer->data + buffer->next, 0, buffer->size - buffer->next); // let's make it clean ^^
}

void clear_buffer(struct Buffer *buffer) {
    memset(buffer->data, 0, buffer->size);
    buffer->next = 0;
}

void free_buffer(struct Buffer **buffer) {
    free((*buffer)->data);
    free((*buffer));
    (*buffer) = NULL;
}


// ======================================================================= //
//                        SERIALIZING PROCEDURES                           //
// ======================================================================= //

void serialize_sock_header(int flag, struct Buffer *buffer) {
    struct SocketHeader *sock_header = new_sock_header(flag);
    reserve_space(buffer, sizeof(struct SocketHeader));
    memcpy(((char *) buffer->data) + buffer->next, sock_header, sizeof(struct SocketHeader));
    buffer->next += sizeof(struct SocketHeader);
    free_sock_header(&sock_header);
}

void serialize_int(int x, struct Buffer *buffer) {
    x = htonl(x);

    reserve_space(buffer, sizeof(x));
    memcpy(((char *) buffer->data) + buffer->next, &x, sizeof(x));
    buffer->next += sizeof(x);
}

void serialize_string(char *string, struct Buffer *buffer) {
    int str_len = (int) strlen(string);

    reserve_space(buffer, str_len);
    memcpy(((char *) buffer->data) + buffer->next, string, str_len);
    buffer->next += str_len;
}

void serialize_time_t(time_t time, struct Buffer *buffer) {
    time = htonll(time);
    reserve_space(buffer, sizeof(time_t));
    memcpy(((char *) buffer->data) + buffer->next, &time, sizeof(time_t));
    buffer->next += sizeof(time_t);
}

void serialize_canvas(struct Canvas *canvas, struct Buffer *buffer) {
    reserve_space(buffer, CANVAS_BYTES_TO_SEND);
    memcpy(((char *) buffer->data) + buffer->next, canvas->bitarray_grid, CANVAS_BYTES_TO_SEND);
    buffer->next += CANVAS_BYTES_TO_SEND;
}

void serialize_his(int flag, char *string, struct Buffer *buffer) {
    serialize_sock_header(flag, buffer);
    serialize_int((int) strlen(string), buffer);
    serialize_string(string, buffer);
}


// ======================================================================= //
//                         UNPACKING PROCEDURES                            //
// ======================================================================= //

// [SOCK_HEADER][unpacking INT]
void unpack_int(struct Buffer *buffer, int *res) {
    *res = ntohl(*(int *) (buffer->data + INT_OFFSET));
}

// [---offset---][unpacking INT]
void unpack_int_var(struct Buffer *buffer, int *res, int offset) {
    *res = ntohl(*(int *) (buffer->data + offset));
}

// [SOCK_HEADER][INT][unpacking STRING]
void unpack_string(struct Buffer *buffer, char *res) {
    strcpy(res, (buffer->data + STRING_OFFSET));
}