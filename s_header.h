//
// Created by jakub.slechta on 03.10.2022.
//

#ifndef UPS_SERVER_C_S_HEADER_H
#define UPS_SERVER_C_S_HEADER_H

#define OK 1
#define CANVAS 2
#define CHAT 3
#define DRAW_NEXT 4
#define INPUT_USERNAME 5
#define INVALID_USERNAME 6


struct SocketHeader {
   unsigned char flag;
};


#endif //UPS_SERVER_C_S_HEADER_H
