/**
 * Both server and client share socket header defined in "sock_header.h"
 * Both server and client share definitions defined in this header file.
 *
 * Future application can use settings (json-xml like) file or describe
 * some of these definitions in a protocol.
 */
#ifndef UPS_SERVER_C_DEFINITIONS_H
#define UPS_SERVER_C_DEFINITIONS_H

#define GAME_DURATION_SEC 20

#define MAX_USERNAME_LEN 14
#define MAX_GUESS_LEN 15

#define ROWS 100
#define COLS 100
#define CANVAS_SIZE (ROWS * COLS)

#define BITARRAY_SIZE(x) (x/8+(!!(x%8)))

#endif //UPS_SERVER_C_DEFINITIONS_H
