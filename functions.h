#ifndef FUNCTIONS_H
#define FUNCTIONS_H

#include "spx_exchange.h"
#include <sys/types.h>

void print_positions();
void remove_buybook(int pidx);
void remove_sellbook(int pidx);
void match_positions();
int find_and_remove(char *order_line, int trader_id, int returns[]);
void signal_accepted(int trader_id, int order_id, char *order_type, int pidx, int qty, int price);
int add_order(char *order_line, int trader_id);
void print_orderbook();
int get_PID(pid_t pid);

#endif
