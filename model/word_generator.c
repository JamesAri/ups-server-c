#include "word_generator.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>

int read_words(char *file_name) {
    FILE *fp;
    char *line = NULL;
    size_t len = 0;
    ssize_t read;
    words.words = (char **) malloc(INIT_SIZE * sizeof(char *));
    words.size = INIT_SIZE;
    words.word_count = 0;

    fp = fopen(file_name, "r");
    if (fp == NULL) {
        return -1;
    }

    while ((read = getline(&line, &len, fp)) != -1) {
        if (words.word_count >= words.size) {
            words.size += INCREMENT;
            words.words = (char **) realloc(words.words, words.size * sizeof(char *));
        }
        words.words[words.word_count] = (char *) malloc(read * sizeof(char));
        strcpy(words.words[words.word_count], line);
        words.word_count++;
    }
    fclose(fp);

    return 0;
}


void get_random_word(char *string_bfr) {
    srand(time(NULL));
    strcpy(string_bfr, words.words[rand() % words.word_count]);
    string_bfr[strcspn(string_bfr, "\r\n")] = 0;
    for (; *string_bfr; ++string_bfr) *string_bfr = (char) tolower(*string_bfr);
}


void free_words() {
    for (int i = 0; i < words.size; i++) {
        free(words.words[i]);
    }
    free(words.words);
    words.size = words.word_count = 0;
}


