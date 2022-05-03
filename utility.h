#ifndef UTILITY_H
#define UTILITY_H

#include <stdio.h>
#include <stdarg.h>

#define FIFO_LIMIT 80

int SPX_print(const char *restrict format, ...);
char *get_message(char *input);
int number_orders(char *line);
char **get_order(char *line, int *num_words);


#endif