#include "model/word_generator.h"
#include "server.h"
#include "model/player.h"
#include "utils/sock_header.h"
#include "ser_buffer.h"
#include "utils/debug.h"

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

int test_players() {
    struct Players *players = malloc(sizeof(struct Players));
    update_players(players, "James", 0);
    update_players(players, "Jake", 1);
    update_players(players, "Annie", 1); // this will pass
    update_players(players, "James", 2); // wont be added
    print_players(players);
    free_players(&players);
    return 0;
}


void test_something() {
    struct Words *words = read_words("./resources/words.txt");
    char *guess_word = malloc(256);
    get_random_word(words, guess_word);
    free_words(&words);
    printf("GUESS WORD: %s", guess_word);

    struct SocketHeader *sock_header = (struct SocketHeader *) malloc(sizeof(struct SocketHeader));
    struct Buffer *buffer = new_buffer();
    sock_header->flag = START_AND_DRAW;
    int str_len = (int) strlen(guess_word);
    int temp = (int) sizeof(struct SocketHeader) + str_len + (int) sizeof(int);
    reserve_space(buffer, temp);

    memcpy(buffer->data, sock_header, sizeof(struct SocketHeader));
    buffer->next += sizeof(struct SocketHeader);

    uint32_t htonl_str_len = htonl(str_len);
    memcpy(buffer->data + buffer->next, &htonl_str_len, sizeof(int));
    buffer->next += sizeof(int);

    memcpy(buffer->data + buffer->next, guess_word, str_len);
    buffer->next += str_len;
    print_buf_to_hex(buffer->data, buffer->next);
    free(sock_header);
    free_buffer(&buffer);
}


int main() {
    start();
    return 0;
}
