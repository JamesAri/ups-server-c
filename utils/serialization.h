#ifndef UPS_SERVER_C_SERIALIZATION_H
#define UPS_SERVER_C_SERIALIZATION_H

#define INITIAL_SIZE 10

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

#endif //UPS_SERVER_C_SERIALIZATION_H
