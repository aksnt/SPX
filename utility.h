#ifndef UTILITY_H
#define UTILITY_H

/**
 * @brief Gets the message from an input up until ;
 * 
 * @param input 
 * @return The string representation of the message
 */
char *get_message_e(char *input);

/**
 * @brief Reads products from the text file into an array
 * 
 * @param path 
 * @param num_products 
 * @param products 
 * @return char** array storing products 
 */
int read_products(char *path, int *num_products, char ***products);

/**
 * @brief Get the product index within products array of a product
 * 
 * @param product 
 * @return int 
 */
int get_pidx(char *product);

/**
 * @brief Counts the levels of orders in an orderbook and ignores duplicates
 * Can be used for both buy and sell book
 * @param pidx 
 * @param flag 
 * @return the number of levels
 */
int count_levels(int pidx, int flag);

/**
 * @brief Reads the message from trader_fd and parses it into get_message_e
 * 
 * @param trader_id 
 * @return char* 
 */
char *read_from_trader(int trader_id);

/**
 * @brief Writes a message to a trader and then sends SIGUSR1
 * 
 * @param trader_id 
 * @param message 
 */
void write_to_trader(int trader_id, char *message);

#endif