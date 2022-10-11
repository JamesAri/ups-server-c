#ifndef UPS_SERVER_C_SERIALIZATION_H
#define UPS_SERVER_C_SERIALIZATION_H

#include <sys/time.h>

#define INITIAL_SIZE 10

#define INT_OFFSET sizeof(struct SocketHeader)
#define STRING_OFFSET (INT_OFFSET + sizeof(int))

struct Buffer {
    void *data;
    int next;
    int size;
};

struct Buffer *new_buffer();

void reserve_space(struct Buffer *buffer, int bytes);

void clear_buffer(struct Buffer *buffer);

void free_buffer(struct Buffer **buffer);

void serialize_sock_header(int flag, struct Buffer *buffer);

void serialize_int(int x, struct Buffer *buffer);

void serialize_string(char *string, struct Buffer *buffer);

void serialize_his(int flag, char *string, struct Buffer *buffer);

void serialize_time_t(time_t time, struct Buffer *buffer);

void unpack_int(struct Buffer *buffer, int *res);

void unpack_int_var(struct Buffer *buffer, int *res, int offset);

void unpack_string(struct Buffer *buffer, char *res);

#endif //UPS_SERVER_C_SERIALIZATION_H
