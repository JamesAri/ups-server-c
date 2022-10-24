#ifndef UPS_SERVER_C_CANVAS_H
#define UPS_SERVER_C_CANVAS_H

#include "../shared/definitions.h"

#define BITARRAY_SIZE(x) (x/8+(!!(x%8)))
#define CANVAS_BYTES_TO_SEND BITARRAY_SIZE(CANVAS_SIZE)

struct Canvas {
    char bitarray_grid[BITARRAY_SIZE(CANVAS_SIZE)];
};

void toggle_bit(char *bitarray, int index);

struct Canvas *new_canvas();

void free_canvas(struct Canvas **canvas);

#endif //UPS_SERVER_C_CANVAS_H
