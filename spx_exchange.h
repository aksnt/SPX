#ifndef SPX_EXCHANGE_H
#define SPX_EXCHANGE_H

#include "spx_common.h"

#define LOG_PREFIX "[SPX]"
#define VALID_ORDER_SIZE 5

// move to seperate header later
int read_products(char* path, int* num_products, char*** products);

typedef struct order order;

struct order {
    int order_id; 
    int product_idx; //stores which product is being ordered
    int price;
    int quantity;
    int trader_id; //trader which made the order
    int flag; //For orderbook printing purposes, if flag = 1 ignore when printing

    order* next;
};

#endif
