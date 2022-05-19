#ifndef UTILITY_H
#define UTILITY_H

char *get_message_e(char *input);

int num_words(char *line);

int read_products(char *path, int *num_products, char ***products);

int get_pidx(char *product);

int count_levels(int pidx, int flag);

char *read_from_trader(int trader_id);

void write_to_trader(int trader_id, char *message);

#endif