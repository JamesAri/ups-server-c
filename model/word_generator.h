#ifndef UPS_SERVER_C_WORD_GENERATOR_H
#define UPS_SERVER_C_WORD_GENERATOR_H

#define INIT_SIZE 100
#define INCREMENT 10


struct Words {
    int size;
    int word_count;
    char **words;
};

struct Words *read_words(char *file_name);

void free_words(struct Words *words);

void get_random_word(char *string_bfr, struct Words *);

#endif //UPS_SERVER_C_WORD_GENERATOR_H
