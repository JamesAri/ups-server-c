#include "word_generator.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>


struct Words *read_words(char *file_name) {
    FILE *fp;
    char *line = NULL;
    size_t len = 0;
    ssize_t read;
    struct Words *words_array;
    words_array = (struct Words *) malloc(sizeof(struct Words));
    words_array->words = (char **) malloc(INIT_SIZE * sizeof(char *));
    words_array->size = INIT_SIZE;
    words_array->word_count = 0;

    fp = fopen(file_name, "r");
    if (fp == NULL) {
        exit(EXIT_FAILURE);
    }

    while ((read = getline(&line, &len, fp)) != -1) {
        if (words_array->word_count >= words_array->size) {
            words_array->size += INCREMENT;
            words_array->words = (char **) realloc(words_array->words, words_array->size * sizeof(char *));
        }
        words_array->words[words_array->word_count] = (char *) malloc(read * sizeof(char));
        strcpy(words_array->words[words_array->word_count], line);
        words_array->word_count++;
    }
    fclose(fp);
    if (line)
        free(line);

    return words_array;
}


void get_random_word(struct Words *words, char *in_bfr) {
    srand(time(NULL));
    strcpy(in_bfr, words->words[rand() % words->word_count]);
}


void free_words(struct Words **words) {
    for (int i = 0; i < (*words)->size; i++) {
        free((*words)->words[i]);
    }
    (*words)->size = (*words)->word_count = 0;
    free((*words));
    *words = NULL;
}


