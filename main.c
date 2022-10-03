#include "word_generator.h"
#include "server.h"
#include "player.h"

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

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

int main() {
//    struct Words *words = read_words("./resources/words.txt");
//    char rnd_word[256];
//    get_random_word(words, rnd_word);
//    free_words(&words);
//
//    printf("%s", rnd_word);

//    test_players();

    start();
    return 0;
}
