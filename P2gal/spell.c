#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/stat.h>

#define BUF_SIZE 4096
#define MAX_WORDS 200000
#define MAX_WORD_LEN 128

char **dictionary = NULL;
size_t dict_size = 0;

// ----------------- Utility -----------------

// Normalize word to lowercase
void normalize(char *word) {
    for (int i = 0; word[i]; i++) {
        word[i] = tolower((unsigned char)word[i]);
    }
}

// Compare for qsort and bsearch
int cmp_words(const void *a, const void *b) {
    return strcmp(*(char **)a, *(char **)b);
}

// Check if word has only symbols (no letters)
int only_symbols(const char *s) {
    for (; *s; s++) {
        if (isalpha((unsigned char)*s))
            return 0;
    }
    return 1;
}

// ----------------- Dictionary -----------------

void load_dictionary(const char *path) {
    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        perror("open dictionary");
        exit(EXIT_FAILURE);
    }

    dictionary = malloc(MAX_WORDS * sizeof(char *));
    if (!dictionary) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }

    char buf[BUF_SIZE];
    ssize_t bytes;
    char word[MAX_WORD_LEN];
    int wlen = 0;

    while ((bytes = read(fd, buf, BUF_SIZE)) > 0) {
        for (ssize_t i = 0; i < bytes; i++) {
            if (buf[i] == '\n' || buf[i] == '\r') {
                if (wlen > 0) {
                    word[wlen] = '\0';
                    normalize(word);
                    dictionary[dict_size++] = strdup(word);
                    wlen = 0;
                }
            } else if (wlen < MAX_WORD_LEN - 1) {
                word[wlen++] = buf[i];
            }
        }
    }

    if (wlen > 0) {
        word[wlen] = '\0';
        normalize(word);
        dictionary[dict_size++] = strdup(word);
    }

    close(fd);

    qsort(dictionary, dict_size, sizeof(char *), cmp_words);
}

// ----------------- Spell Checking -----------------

int in_dictionary(char *word) {
    void *res = bsearch(&word, dictionary, dict_size, sizeof(char *), cmp_words);
    return res != NULL;
}

void check_file(const char *filename) {
    int fd = open(filename, O_RDONLY);
    if (fd < 0) {
        printf( "cannot open file: %s\n", filename);
        return;
    }

    char buf[BUF_SIZE];
    ssize_t bytes;
    char word[MAX_WORD_LEN];
    int wlen = 0;
    int line = 1, col = 1, start_col = 1;

    while ((bytes = read(fd, buf, BUF_SIZE)) > 0) {
        for (ssize_t i = 0; i < bytes; i++) {
            char c = buf[i];

            if (isalpha((unsigned char)c) || c == '-') {
                if (wlen == 0) start_col = col;
                if (wlen < MAX_WORD_LEN - 1)
                    word[wlen++] = c;
            } else {
                if (wlen > 0) {
                    word[wlen] = '\0';
                    char temp[MAX_WORD_LEN];
                    strcpy(temp, word);
                    normalize(temp);

                    if (!in_dictionary(temp) && !only_symbols(temp)) {
                        char msg[512];
                        int len = snprintf(msg, sizeof(msg),
                                           "%s:%d:%d %s\n",
                                           filename, line, start_col, word);
                        write(STDOUT_FILENO, msg, len);
                    }
                    wlen = 0;
                }
                if (c == '\n') {
                    line++;
                    col = 0;
                }
            }
            col++;
        }
    }

    if (wlen > 0) {  // Last word if no trailing non-letter
        word[wlen] = '\0';
        char temp[MAX_WORD_LEN];
        strcpy(temp, word);
        normalize(temp);
        if (!in_dictionary(temp) && !only_symbols(temp)) {
            char msg[512];
            int len = snprintf(msg, sizeof(msg),
                               "%s:%d:%d %s\n",
                               filename, line, start_col, word);
            write(STDOUT_FILENO, msg, len);
        }
    }

    close(fd);
}

// ----------------- Main -----------------

int main(int argc, char *argv[]) {
    if (argc < 2) {
        write(STDERR_FILENO, "Usage: spell dict [files...]\n", 29);
        exit(EXIT_FAILURE);
    }

    const char *dict_file = argv[1];
    load_dictionary(dict_file);

    if (argc == 2) {
        // read from stdin
        char temp_name[] = "stdin_input.txt";
        int temp_fd = open(temp_name, O_WRONLY | O_CREAT | O_TRUNC, 0600);
        if (temp_fd < 0) {
            perror("open temp");
            exit(EXIT_FAILURE);
        }
        char buf[BUF_SIZE];
        ssize_t bytes;
        while ((bytes = read(STDIN_FILENO, buf, BUF_SIZE)) > 0)
            write(temp_fd, buf, bytes);
        close(temp_fd);
        check_file(temp_name);
        unlink(temp_name);
    } else {
        for (int i = 2; i < argc; i++) {
            check_file(argv[i]);
        }
    }

    for (size_t i = 0; i < dict_size; i++)
        free(dictionary[i]);
    free(dictionary);

    return 0;
}
