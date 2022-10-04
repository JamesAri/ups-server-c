//
// Created by jakub.slechta on 03.10.2022.
//

#ifndef UPS_SERVER_C_CLIENT_H
#define UPS_SERVER_C_CLIENT_H

#define PORT "9034"
#define BACKLOG 5
#define WORD_BUF_SIZE 256
#define CANVAS_BUF_SIZE 1024

struct Buffer {
    void *data;
    int next;
    int size;
};

#endif //UPS_SERVER_C_CLIENT_H
