#ifndef UPS_SERVER_C_SOCK_HEADER_H
#define UPS_SERVER_C_SOCK_HEADER_H

//                                      // =======================================================================
//                                      // ||        SERVER -> CLIENT          ||        CLIENT -> SERVER       ||
//                                      // =======================================================================
#define EMPTY 0                         // || -------------------------------- || ----------------------------- ||
#define GAME_IN_PROGRESS 1              // || [SOCK_HEADER][TIME]              || ----------------------------- ||
#define CANVAS 2                        // || [SOCK_HEADER][CANVAS]            || [SOCK_HEADER][CANVAS]         ||
#define CHAT 3                          // || [SOCK_HEADER][INT][STRING]       || [SOCK_HEADER][INT][STRING]    ||
#define START_AND_GUESS 4               // || [SOCK_HEADER][TIME]              || ----------------------------- ||
#define START_AND_DRAW 5                // || [SOCK_HEADER][INT][STRING][TIME] || ----------------------------- ||
#define INPUT_USERNAME 6                // || [SOCK_HEADER]                    || [SOCK_HEADER][INT][STRING]    ||
#define CORRECT_GUESS 7                 // || [SOCK_HEADER]                    || ----------------------------- ||
#define WRONG_GUESS 8                   // || [SOCK_HEADER]                    || ----------------------------- ||
#define CORRECT_GUESS_ANNOUNCEMENT 9    // || [SOCK_HEADER][INT][STRING]       || ----------------------------- ||
#define INVALID_USERNAME 10             // || [SOCK_HEADER]                    || ----------------------------- ||
#define WAITING_FOR_PLAYERS 11          // || [SOCK_HEADER][INT][INT]          || ----------------------------- ||
#define GAME_ENDS 12                    // || [SOCK_HEADER]                    || ----------------------------- ||
//                                      // =======================================================================

struct SocketHeader {
    unsigned char flag;
};

void free_sock_header(struct SocketHeader **sock_header);

struct SocketHeader *new_sock_header(int flag);

#endif //UPS_SERVER_C_SOCK_HEADER_H
