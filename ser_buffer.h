//
// Created by jakub.slechta on 04.10.2022.
//

#ifndef UPS_SERVER_C_SER_BUFFER_H
#define UPS_SERVER_C_SER_BUFFER_H

#define INITIAL_SIZE 10

struct Buffer {
    void *data;
    int next;
    int size;
};

void reserve_space(struct Buffer *b, int bytes);
void free_buffer(struct Buffer **buffer);

#endif //UPS_SERVER_C_SER_BUFFER_H
