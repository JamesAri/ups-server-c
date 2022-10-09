#include "debug.h"
#include <stdio.h>
#include <string.h>

void buf_to_hex_string(void *buffer, int len, char *hex_string) {
    int i;
    char temp[255];
    memset(temp, 0, sizeof(temp));
    for (i = 0; i < len; i++) {
        if (i == 0)
            sprintf(temp, "%02X", ((char *) buffer)[i]);
        else
            sprintf(temp, ":%02X", ((char *) buffer)[i]);
        strcat(hex_string, temp);
    }
}