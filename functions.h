#ifndef FUNCTIONS_H
#define FUNCTIONS_H

#include "spx_exchange.h"
#include <sys/types.h>

/**
 * @brief Prints the positions of traders currently trading
 * 
 */
void print_positions();

/**
 * @brief Attempts to match positions each time an order comes through
 * 
 */
void match_positions();

/**
 * @brief Used to find an order based on some input, and trader_id and remove it
 * 
 * @param order_line 
 * @param trader_id 
 * @param returns 
 * @return int 0 if removed successfully, with product index and which book in results array
 */
int find_and_remove(char *order_line, int trader_id, int returns[]);

/**
 * @brief Signals ACCPETED <order_id> to the sending trader and Market <order> to the rest
 * 
 * @param trader_id 
 * @param order_id 
 * @param order_type 
 * @param pidx 
 * @param qty 
 * @param price 
 */
void signal_accepted(int trader_id, int order_id, char *order_type, int pidx, int qty, int price);

/**
 * @brief Adds the order to the head of the buybook or sellbook. For buybook
 * it is added from highest to lowest price (head to tail) and reverse for sellbook
 * 
 * @param order_line 
 * @param trader_id 
 * @return int 
 */
int add_order(char *order_line, int trader_id);

/**
 * @brief Prints the orderbook after a successful order
 * 
 */
void print_orderbook();

/**
 * @brief Used to find out which trader sent a signal using its process ID
 * 
 * @param pid 
 * @return Returns the index of the process ID of the process which sent the signal 
 */
int get_PID(pid_t pid);

#endif
