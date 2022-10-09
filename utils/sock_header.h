#ifndef UPS_SERVER_C_SOCK_HEADER_H
#define UPS_SERVER_C_SOCK_HEADER_H

#define EMPTY 0
#define OK 1
#define CANVAS 2
#define CHAT 3
#define START_AND_GUESS 4
#define START_AND_DRAW 5
#define INPUT_USERNAME 6
#define CORRECT_GUESS 7
#define WRONG_GUESS 8
#define CORRECT_GUESS_ANNOUNCEMENT 9
#define INVALID_USERNAME 10


struct SocketHeader {
    unsigned char flag;
};

void free_sock_header(struct SocketHeader **sock_header);

struct SocketHeader *new_sock_header(int flag);

#endif //UPS_SERVER_C_SOCK_HEADER_H
