//
// Created by jakub.slechta on 03.10.2022.
//

#ifndef UPS_SERVER_C_S_HEADER_H
#define UPS_SERVER_C_S_HEADER_H

#define OK 1
#define CANVAS 2
#define CHAT 3
#define DRAW 4
#define INVALID_USERNAME 5


struct SocketHeader {
   unsigned char flag;
};


#endif //UPS_SERVER_C_S_HEADER_H
