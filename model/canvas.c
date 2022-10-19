#include "canvas.h"

void toggle_bit(char *bitarray, int index) {
    bitarray[index / 8] ^= 1 << (index % 8);
}

char get_bit(const char *bitarray, int index) {
    return (char) (1 & (bitarray[index / 8] >> (index % 8)));
}

struct Canvas *new_canvas() {
    struct Canvas *canvas = (struct Canvas *) malloc(sizeof(struct Canvas));
    return canvas;
}

void free_canvas(struct Canvas **canvas) {
    free(*canvas);
    (*canvas) = NULL;
}
