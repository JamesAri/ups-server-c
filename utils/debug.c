#include "debug.h"
#include <stdio.h>
#include <string.h>

void buf_to_hex_string(void *buffer, int len, char *hex_string) {
    int i;
    int print_size = sizeof(char) * 3;
    for (i = 0; i < len - 1; i++) {
        snprintf(hex_string + i * print_size, sizeof(hex_string), "%02X:", ((char *) buffer)[i]);
    }
    print_size = sizeof(char) * 2;
    snprintf(hex_string + i * print_size, sizeof(hex_string), "%02X", ((char *) buffer)[i]);
}