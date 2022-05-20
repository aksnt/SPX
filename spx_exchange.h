#ifndef SPX_EXCHANGE_H
#define SPX_EXCHANGE_H

#include "spx_common.h"

#define LOG_PREFIX "[SPX]"

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

int invalid_order(char *order_type, order *new_order, int pidx);

#endif
