//
// Created by jakub.slechta on 03.10.2022.
//

#ifndef UPS_SERVER_C_S_HEADER_H
#define UPS_SERVER_C_S_HEADER_H

#define OK 1
#define CANVAS 2
#define CHAT 3
#define START_AND_GUESS 4
#define START_AND_DRAW 5
#define INPUT_USERNAME 6
#define CORRECT_GUESS 7
#define WRONG_GUESS 8
#define INVALID_USERNAME 10


struct SocketHeader {
   unsigned char flag;
};


#endif //UPS_SERVER_C_S_HEADER_H
