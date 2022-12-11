#ifndef UPS_SERVER_C_SOCK_HEADER_H
#define UPS_SERVER_C_SOCK_HEADER_H

//                                      // ============================================================================
//                                      // ||        SERVER -> CLIENT               ||        CLIENT -> SERVER       ||
//                                      // ============================================================================
#define DISCONNECT 0                    // || ------------------------------------- || ----------------------------- ||
#define OK 1                            // || [SOCK_HEADER]                         || ----------------------------- ||
#define CANVAS 2                        // || [SOCK_HEADER][CANVAS]                 || [SOCK_HEADER][CANVAS]         ||
#define CHAT 3                          // || [SOCK_HEADER][INT][STR]               || [SOCK_HEADER][INT][STR]       ||
#define START_AND_GUESS 4               // || [SOCK_HEADER][TIME]                   || ----------------------------- ||
#define START_AND_DRAW 5                // || [SOCK_HEADER][INT][STR][TIME]         || ----------------------------- ||
#define LOGIN 6                         // || ------------------------------------- || [SOCK_HEADER][INT][STR]       ||
#define CORRECT_GUESS 7                 // || [SOCK_HEADER]                         || ----------------------------- ||
#define WRONG_GUESS 8                   // || [SOCK_HEADER]                         || ----------------------------- ||
#define CORRECT_GUESS_ANNOUNCEMENT 9    // || [SOCK_HEADER][INT][STR]               || ----------------------------- ||
#define INVALID_USERNAME 10             // || [SOCK_HEADER]                         || ----------------------------- ||
#define WAITING_FOR_PLAYERS 11          // || [SOCK_HEADER][INT][INT]               || ----------------------------- ||
#define GAME_ENDS 12                    // || [SOCK_HEADER]                         || ----------------------------- ||
#define GAME_IN_PROGRESS 13             // || [SOCK_HEADER][TIME]                   || ----------------------------- ||
#define SERVER_ERROR 14                 // || [SOCK_HEADER]                         || ----------------------------- ||
#define PLAYER_LIST 15                  // || [S_H][INT]array([INT][STR][ON/OFF])   || ----------------------------- ||
#define PLAYER_LIST_CHANGE 16           // || [SOCK_HEADER][INT][STR][ON/OFF]       || ----------------------------- ||
#define SERVER_FULL 17                  // || [SOCK_HEADER]                         || ----------------------------- ||
#define HEART_BEAT 18                   // || [SOCK_HEADER]                         || [SOCK_HEADER]                 ||
//                                      // ============================================================================

struct SocketHeader {
    unsigned char flag;
};

void free_sock_header(struct SocketHeader **sock_header);

struct SocketHeader *new_sock_header(int flag);

#endif //UPS_SERVER_C_SOCK_HEADER_H
