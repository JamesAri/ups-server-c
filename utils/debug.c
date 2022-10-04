#include "debug.h"
#include <stdio.h>

void print_buf_to_hex(void *buffer, int len) {
    int i;
    for (i = 0; i < len; i++) {
        if (i > 0) printf(":");
        printf("%02X", ((char *) buffer)[i]);
    }
    printf("\n");
}