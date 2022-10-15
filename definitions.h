/**
 * Both server and client share socket header defined in "sock_header.h"
 * Both server and client share definitions defined in this header file.
 *
 * Future application can use setting's file or describe these definitions in a protocol.
 */
#ifndef UPS_SERVER_C_DEFINITIONS_H
#define UPS_SERVER_C_DEFINITIONS_H

#define PORT "9034"

#define GAME_DURATION_SEC 60

#define MAX_USERNAME_LEN 20

#define ROWS 100
#define COLS 100
#define CANVAS_BUF_SIZE ((ROWS * COLS) / 8 + (ROWS * COLS) % 8)

#endif //UPS_SERVER_C_DEFINITIONS_H
