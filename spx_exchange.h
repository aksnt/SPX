#ifndef SPX_EXCHANGE_H
#define SPX_EXCHANGE_H

#include "spx_common.h"
// #include "utility.h"

#define LOG_PREFIX "[SPX]"

int read_products(char* path, int* num_products, char*** products);

#endif
