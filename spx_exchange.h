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

/**
 * @brief Checks whether an order is invalid based on the full order and struct representation
 * 
 * @param order_type 
 * @param new_order 
 * @param pidx 
 * @return 1 if invalid, 0 otherwise
 */
int invalid_order(char *order_type, order *new_order, int pidx);

#endif
