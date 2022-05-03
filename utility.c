#include "utility.h"
#include "spx_common.h"

int SPX_print(const char *restrict format, ...) {
    int res1, res2;
    va_list ap;
    res1 = printf("[SPX] ");
    if (res1 < 0)
        return res1;

    va_start(ap, format);
    res2 = vprintf(format, ap);
    va_end(ap);
    return res2 < 0 ? res2 : res1 + res2;
}

char *get_message(char *input) {
    int delimiter;
    for (int i = 0; i < FIFO_LIMIT; ++i)
        if (input[i] == ';') {
            delimiter = i;
            break;
        }

    input[delimiter] = '\0';
    return input;
}

int number_orders(char *line) {
    int len_line = strlen(line);
    int num_orders = 0;

    char is_last_char_space = 1;
    
    // for each word
    for (int line_i = 0; line_i < len_line - 1; line_i++) {
        if (line[line_i] == ';')
            is_last_char_space = 1;
        else {
            if (is_last_char_space)
                num_orders++;

            is_last_char_space = 0;
        }
    }
    return num_orders;
}

char **get_order(char *line, int *num_orders) {
    int len_line = strlen(line);
    if (len_line == 0) {
        return NULL;
    }

    *num_orders = number_orders(line);
    char **words = (char **)malloc(*num_orders* sizeof(char *));
    char is_last_char_space = 1;

    int word_i = 0;
    int first = 0;
    int last = 0;

    // for each word
    for (int line_i = 0; line_i < len_line; line_i++) {
        if (line[line_i] == ';') {
            if (!is_last_char_space) {
                last = line_i - 1;
                words[word_i] = (char *)malloc((last - first + 2) * sizeof(char));
                for (int i = first, j = 0; i <= last; i++, j++)
                    words[word_i][j] = line[i];

                words[word_i][last - first + 1] = '\0';
                word_i++;
            }

            is_last_char_space = 1;
        } else {
            if (is_last_char_space) {
                first = line_i;
            }

            is_last_char_space = 0;
        }
    }

    return words;
}