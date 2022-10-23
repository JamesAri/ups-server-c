#include "canvas.h"

void toggle_bit(char *bitarray, int index) {
    bitarray[index / 8] ^= 1 << (7 - index % 8);
}

struct Canvas *new_canvas() {
    struct Canvas *canvas = (struct Canvas *) calloc(1, sizeof(struct Canvas));
    return canvas;
}

void free_canvas(struct Canvas **canvas) {
    free(*canvas);
    (*canvas) = NULL;
}
