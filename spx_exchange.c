/**
 * comp2017 - assignment 3
 * <your name>
 * <your unikey>
 */

#include "spx_exchange.h"

int SPX_print(const char *restrict format, ...);
char *get_message(char *input);
int number_orders(char *line);
char *read_from_trader(int trader_id);
void write_to_trader(int trader_id, char *message);

int num_products;
int num_traders;
char **products;
order **buybook = NULL;   // buy orderbook
order **sellbook = NULL;  // sell orderbook
int ***matchbook = NULL;  // stores matched orders for each trader

char received_msg[ORDER_SIZE_LIMIT];
int sigchld;

char **exchange_fifo;
char **trader_fifo;

int *exchange_fd;
int *trader_fd;

pid_t children[FIFO_LIMIT];

void print_positions() {
    SPX_print("\t--POSITIONS--\n");
    for (int tdx = 0; tdx < num_traders; tdx++) {
        SPX_print("\t Trader %d: ", tdx);
        for (int pidx = 0, count = num_products; pidx < num_products; ++pidx, --count) {
            printf("%s %d ($%lf)", products[pidx], 0, 0.01);
            if (count > 1)
                printf(", ");
            else
                printf("\n");
        }
    }
}

void remove_order(int pidx, order *product, int flag) {
    order **book;

    if (flag == 1)
        book = buybook;
    else
        book = sellbook;

    order *head = book[pidx];

    while (book[pidx]) {
        if (product == head) {
            // order found at head position
            book[pidx] = head->next;
            free(head);  // free memory allocated
        } else {
            // order found somewhere else
            if ((book[pidx])->next == product) {
                order *temp = (book[pidx])->next;  // points to order that needs to be removed
                (book[pidx])->next = temp->next;
                free(temp);
            }
        }
        book[pidx] = (book[pidx])->next;
    }
}

void match_positions() {
    // Match the highest buy with the lowest sell
    for (int i = 0; i < num_products; i++) {
        // No orders to match for this product
        if (!buybook[i] || !(sellbook[i]))
            break;

        // matchbook = {trader 0 array, trader 1 array}

        // Trader 0 array = {{value, qty}, {value, qty}, ...}
        // Trader 1 array = {{value, qty}, {value, qty}, ...}

        int buy_price = (buybook[i])->price;
        int sell_price = (sellbook[i])->price;
        int buy_qty = (buybook[i])->quantity;
        int sell_qty = (sellbook[i])->quantity;

        while (buybook[i] && sellbook[i]) {
            int count = 0;  // counds the number of matched orders
            // int qty_bought = 0;

            double value = 0;
            double fee = 0;
            if (buy_price > sell_price) {
                // execute order

                if (buy_qty == sell_qty) {
                    // calculate the transaction value and fee
                    value = (buybook[i])->price * (sellbook[i])->quantity;
                    fee = value * FEE_PERCENTAGE;
                    count++;

                    // store order in matched orders for buyer and seller
                    // store_order(i, (buybook[i])->trader_id, (sellbook[i])->quantity, -value, fee);
                    // store_order(i, (sellbook[i])->trader_id, -(buybook[i])->quantity, value, fee);

                    // remove the order from orderbook as it is fulfilled
                    remove_order(i, buybook[i], BUYBOOK);
                    remove_order(i, sellbook[i], SELLBOOK);
                }
                if (buy_qty > sell_qty) {
                    // delete the sell order and modify buy order
                    // int qty_bought = (sellbook[i])->quantity;
                    (buybook[i])->quantity =
                        (buybook[i])->quantity - (sellbook[i])->quantity;

                    value = (buybook[i])->price * (sellbook[i])->quantity;
                    fee = value * FEE_PERCENTAGE;
                    count++;

                    // store_order(i, (buybook[i])->trader_id, qty_bought, -value, fee);
                    // store_order(i, (sellbook[i])->trader_id, -qty_bought, value, fee);

                    remove_order(i, sellbook[i], SELLBOOK);
                }
                if (buy_qty < sell_qty) {
                    // delete the buy order and modify sell order
                    (sellbook[i])->quantity =
                        (sellbook[i])->quantity - (buybook[i])->quantity;

                    value = (buybook[i])->price * (buybook[i])->quantity;
                    fee = value * FEE_PERCENTAGE;
                    count++;

                    // store_order(i, (buybook[i])->trader_id, qty_bought, -value, fee);
                    // store_order(i, (sellbook[i])->trader_id, -qty_bought, value, fee);

                    remove_order(i, buybook[i], BUYBOOK);
                }
            }

            SPX_print("Match: Order %d [T%d], New order: %d [T%d] value: $%d, fee: %.2f.\n",
                      (buybook[i])->order_id, (buybook[i])->trader_id,
                      (sellbook[i])->order_id, (sellbook[i])->trader_id,
                      value, round(fee));

            buybook[i] = (buybook[i])->next;
            sellbook[i] = (sellbook[i])->next;
        }
    }
}

int validate_order(char *order_type, order *new_order, int pidx) {
    order **buyptr = buybook;
    order **sellptr = sellbook;

    if (strcmp(order_type, "BUY") == 0) {
        while (buyptr[pidx]) {
            if (new_order->order_id == (buyptr[pidx])->order_id) {
                return 1;
            }
            buyptr[pidx] = (buyptr[pidx])->next;
        }
    }

    if (strcmp(order_type, "SELL") == 0) {
        while (sellptr[pidx]) {
            if (new_order->order_id == (sellptr[pidx])->order_id) {
                return 1;
            }
            sellptr[pidx] = (sellptr[pidx])->next;
        }
    }
    // order doesn't exist hence, valid
    return 0;
}

int get_product(char *product) {
    for (int i = 0; i < num_products; i++) {
        if (strcmp(products[i], product) == 0) {
            return i;
        }
    }
    return -1;
}

int add_order(char *order_line, int trader_id) {
    int do_buy = 0;
    int do_sell = 0;

    // buybook = {{GPU orders}, {Router orders}}
    order *new_order = (order *)malloc(sizeof(order));
    char *order_type = strtok(order_line, " ");

    if (strcmp(order_type, "BUY") == 0)
        do_buy = 1;

    if (strcmp(order_type, "SELL") == 0)
        do_sell = 1;

    // if (strcmp(order_type, "AMEND") == 0) {
    //     //TODO some amend logic

    // }
    // if (strcmp(order_type, "CANCEL") == 0) {
    //     //TODO some cancel logic
    //     new_order->type = CANCEL;
    // }

    new_order->order_id = atoi(strtok(NULL, " "));

    char *product_type = strtok(NULL, " ");
    int pidx = get_product(product_type);
    if (pidx != -1) {
        new_order->product_idx = pidx;
    } else {
        SPX_print("Invalid.\n");  // just for testing
        return 0;
    }

    if (validate_order(order_type, new_order, new_order->product_idx))
        return 0;

    new_order->quantity = atoi(strtok(NULL, " "));
    new_order->price = atoi(strtok(NULL, " "));
    new_order->trader_id = trader_id;

    // Insert in order of price -> according to buy or sell

    if (do_buy) {
        // Insert into buy book from highest to lowest
        if (!buybook[pidx] || (buybook[pidx])->price > new_order->price) {
            new_order->next = buybook[pidx];
            buybook[pidx] = new_order;
        } else {
            while ((buybook[pidx])->next && ((buybook[pidx])->next->price < new_order->price)) {
                buybook[pidx] = (buybook[pidx])->next;
            }
            new_order->next = (buybook[pidx])->next;
            (buybook[pidx])->next = new_order;
        }

    } else if (do_sell) {
        // Insert into sell book from lowest to highest order
        if (!sellbook[pidx] || (sellbook[pidx])->price < new_order->price) {
            new_order->next = sellbook[pidx];
            sellbook[pidx] = new_order;

        } else {
            while ((sellbook[pidx])->next && (sellbook[pidx])->next->price > new_order->price) {
                sellbook[pidx] = (sellbook[pidx])->next;
            }
            new_order->next = (sellbook[pidx])->next;
            (sellbook[pidx])->next = new_order;
        }
    }

    return 1;
}

int count_levels(int pidx, int flag) {
    int count = 0;
    int duplicate = 0;

    order *cursor;
    if (flag == 0)
        cursor = buybook[pidx];
    else
        cursor = sellbook[pidx];

    while (cursor) {
        if (cursor->next) {
            if (cursor->price == cursor->next->price)
                duplicate++;
        }
        count++;
        cursor = cursor->next;
    }
    return count - duplicate;
}

void print_orderbook() {
    SPX_print("\t--ORDERBOOK--\n");

    order **buyptr = buybook;
    order **sellptr = sellbook;

    int buy = 0;
    int sell = 0;

    for (int i = 0; i < num_products; ++i) {
        buy = count_levels(i, BUYBOOK);
        sell = count_levels(i, SELLBOOK);

        SPX_print("\tProduct: %s; Buy levels: %d; Sell levels: %d\n",
                  products[i], buy, sell);

        /* The following declares needed variables and then prints the orderbook */

        int buy_qty = 0;
        int sell_qty = 0;
        int num_buy = 1;
        int num_sell = 1;

        //?Pointer questions

        /*
        Buybook = {{GPU Order 1 -> GPU Order 2}, {Router Order 1 -> Router Order 2}}

        order **buyptr = buybook;
        order *buyptr = buybook[i];
        order buyptr = buybook[i][j];
        */

        while ((buyptr[i]) && (sellptr[i])) {
            if ((buyptr[i])->price == (buyptr[i]->next)->price) {
                buy_qty += (buyptr[i])->quantity + (buyptr[i]->next)->quantity;
                num_buy++;
                (buyptr[i])->flag = 1;  // flag as duplicate as to not reprint below
            }

            if ((sellptr[i])->price == (sellptr[i]->next)->price) {
                sell_qty += (sellptr[i])->quantity + (sellptr[i]->next)->quantity;
                num_sell++;
                (sellptr[i])->flag = 1;
            }

            if ((buyptr[i])->price > (sellptr[i])->price) {
                // print buy orders that are not flagged
                if (!(buyptr[i])->flag) {
                    if (num_buy > 1) {
                        SPX_print("\t\tBUY %d @ %d (%d orders)\n", buy_qty,
                                  (buyptr[i])->price, num_buy);
                        buy_qty = 0;
                        num_buy = 1;
                    } else if (num_buy == 1) {
                        SPX_print("\t\tBUY %d @ %d (%d order)\n", buy_qty,
                                  (buyptr[i])->price, num_buy);
                        buy_qty = 0;
                    }
                }
            }

            if ((buyptr[i])->price < (sellptr[i])->price) {
                // print sell orders that are not flagged
                if (!(sellptr[i])->flag) {
                    if (num_sell > 1) {
                        SPX_print("\t\tSELL %d @ %d (%d orders)\n", sell_qty,
                                  (sellptr[i])->price, num_sell);
                        sell_qty = 0;
                        num_sell = 1;
                    } else if (num_sell == 1) {
                        SPX_print("\t\tSELL %d @ %d (%d order)\n", sell_qty,
                                  (sellptr[i])->price, num_sell);
                        sell_qty = 0;
                    }
                }
            }

            buyptr[i] = (buyptr[i])->next;
            sellptr[i] = (sellptr[i])->next;
        }

        if (buyptr[i] && !sellptr[i]) {
            // print remaining buy orders
            while (buyptr[i]) {
                buy_qty = 0;
                num_buy = 1;
                if (!(buyptr[i])->next) {
                    SPX_print("\t\tBUY %d @ %d (%d order)\n", (buyptr[i])->quantity,
                              (buyptr[i])->price, num_buy);
                    buy_qty = 0;
                } else {
                    if ((buyptr[i])->price == (buyptr[i]->next)->price) {
                        buy_qty += (buyptr[i])->quantity + (buyptr[i]->next)->quantity;
                        num_buy++;
                        (buyptr[i])->flag = 1;
                    }
                    if ((buyptr[i])->price > (sellptr[i])->price) {
                        if (!(buyptr[i])->flag) {
                            if (num_buy > 1) {
                                SPX_print("\t\tBUY %d @ %d (%d orders)\n", buy_qty,
                                          (buyptr[i])->price, num_buy);
                                buy_qty = 0;
                                num_buy = 1;
                            } else if (num_buy == 1) {
                                SPX_print("\t\tBUY %d @ %d (%d order)\n", buy_qty,
                                          (buyptr[i])->price, num_buy);
                                buy_qty = 0;
                            }
                        }
                    }
                }
                buyptr[i] = (buyptr[i])->next;
            }
        }

        if (!(buyptr[i]) && sellptr[i]) {
            // print remaining sell orders
            while (sellptr[i]) {
                sell_qty = 0;
                num_sell = 1;
                if (!(sellptr[i])->next) {
                    SPX_print("\t\tSELL %d @ %d (%d order)\n", sell_qty,
                              (sellptr[i])->price, num_sell);
                    buy_qty = 0;
                } else {
                    if ((sellptr[i])->price == (sellptr[i]->next)->price) {
                        sell_qty += (sellptr[i])->quantity + (sellptr[i]->next)->quantity;
                        num_sell++;
                        (sellptr[i])->flag = 1;
                    }
                    if (!(sellptr[i])->flag) {
                        if (num_sell > 1) {
                            SPX_print("\t\tSELL %d @ %d (%d orders)\n", sell_qty,
                                      (sellptr[i])->price, num_sell);
                            sell_qty = 0;
                            num_sell = 1;
                        } else if (num_sell == 1) {
                            SPX_print("\t\tSELL %d @ %d (%d order)\n", sell_qty,
                                      (sellptr[i])->price, num_sell);
                            sell_qty = 0;
                        }
                    }
                }
                sellptr[i] = (sellptr[i])->next;
            }
        }
    }
}

void parse_command(char *order, int trader_id) {
    SPX_print("[T%d] Parsing command: <%s>\n", trader_id, order);
}

// void signal_all_traders(int trader_id, char *order_line) {
//     order *first_order = current_orders;
//     for (int j = 0; j < num_traders; j++) {
//         if (j == trader_id) {
//             // TODO change to sprintf later
//             write(exchange_fd[j], "ACCEPTED ", strlen("ACCEPTED ") + 1);
//             write(exchange_fd[j], &(first_order->id), sizeof(int));
//             write(exchange_fd[j], ";", strlen(";") + 1);
//         } else {
//             write(exchange_fd[j], "MARKET ", strlen("MARKET ") + 1);
//             write(exchange_fd[j], order_line, strlen(order_line) + 1);
//         }
//         // signal traders in both cases with their respective message
//         kill(children[j], SIGUSR1);
//     }
// }

void sig_handle(int sig) {
    // TODO change some global to true and do logic outside
    if (sig == SIGUSR1) {
        for (int i = 0; i < num_traders; i++) {
            // Parse in the order
            char *buf = read_from_trader(i);
            parse_command(buf, i);

            // Add the order to the orderbook, send invalid otherwise
            if (!add_order(buf, i)) {
                write_to_trader(i, "INVALID;");
                break;
            }

            // attempt to match positions
            match_positions();

            // print ORDERBOOK
            print_orderbook();

            // Print positions
            print_positions();

            // signal traders
            kill(children[i], SIGUSR1);
        }
    }
}

void sig_chld(int sig) {
    sigchld = 1;
}

char *read_from_trader(int trader_id) {
    read(trader_fd[trader_id], received_msg, sizeof(received_msg));
    return get_message(received_msg);
}

void write_to_trader(int trader_id, char *message) {
    write(exchange_fd[trader_id], message, strlen(message) + 1);
    kill(children[trader_id], SIGUSR1);
}

int main(int argc, char **argv) {
    SPX_print("Starting\n");
    read_products(argv[1], &num_products, &products);

    buybook = (order **)malloc(sizeof(order *) * num_products);
    sellbook = (order **)malloc(sizeof(order *) * num_products);
    matchbook = (int ***)malloc(sizeof(int **) * num_products * num_traders);

    SPX_print("Trading %d products:", num_products);
    for (int i = 0; i < num_products; ++i) {
        printf(" %s", products[i]);
    }
    printf("\n");

    // FIFO: Begin
    num_traders = argc - 2;
    exchange_fd = malloc(sizeof(int) * num_traders);
    trader_fd = malloc(sizeof(int) * num_traders);
    exchange_fifo = malloc(sizeof(char *) * num_traders);
    trader_fifo = malloc(sizeof(char *) * num_traders);

    // Signal handler
    signal(SIGUSR1, sig_handle);
    signal(SIGCHLD, sig_chld);

    for (int i = 0; i < num_traders; ++i) {
        exchange_fifo[i] = malloc(sizeof(char) * FIFO_LIMIT);
        trader_fifo[i] = malloc(sizeof(char) * FIFO_LIMIT);
        sprintf(exchange_fifo[i], FIFO_EXCHANGE, i);
        sprintf(trader_fifo[i], FIFO_TRADER, i);

        // Make FIFOs
        unlink(exchange_fifo[i]);  // deletes any prior FIFOs
        if (mkfifo(exchange_fifo[i], 0666) == -1) {
            printf("ERROR: Failed to create FIFO /tmp/spx_exchange_%d", i);
        }
        SPX_print("Created FIFO %s\n", exchange_fifo[i]);

        unlink(trader_fifo[i]);
        if (mkfifo(trader_fifo[i], 0666) == -1) {
            printf("ERROR: Failed to create FIFO /tmp/spx_trader_%d", i);
        }
        SPX_print("Created FIFO %s\n", trader_fifo[i]);

        SPX_print("Starting trader %d (%s)\n", i, argv[2 + i]);

        // Start each trader as a child process
        pid_t res = fork();
        if (res == 0) {
            // Inside child process
            // printf("[CHILD]: Trader is %d. Child is %d. Parent is %d.\n", getpid(), res, getppid());
            char trader_id[FIFO_LIMIT];
            sprintf(trader_id, "%d", i);
            char *args[] = {argv[2 + i], trader_id, NULL};

            execvp(argv[2 + i], args);
            exit(0);
        } else {
            // Inside parent process
            children[i] = res;  // stores the process ID of each trader

            // printf("[PARENT]: Exchange is %d. Child is %d. VSCODE is %d.\n", getpid(), res, getppid());
            exchange_fd[i] = open(exchange_fifo[i], O_WRONLY);
            SPX_print("Connected to %s\n", exchange_fifo[i]);

            trader_fd[i] = open(trader_fifo[i], O_RDONLY);
            SPX_print("Connected to %s\n", trader_fifo[i]);
            usleep(1);  // ensures each trader is setup properly
        }
    }

    for (int i = 0; i < num_traders; i++) {
        // sends market open to all traders
        write_to_trader(i, "MARKET OPEN;");
    }
    usleep(1);
    // While loop checking how many SIGCHLDs signals received, if == num_traders -> calc fees
    // If any SIGCHLD received --> disconnect the child and trader that sent it
    while (sigchld) {
        SPX_print("Trader [0] disconnected\n");
        SPX_print("Trading completed\n");
        SPX_print("Exchange fees collected: $0\n");
        sigchld = 0;
    }

    // Free products memory
    for (int i = 0; i < num_products; i++) {
        free(products[i]);
    }

    free(products);


    for (int i = 0; i < num_traders; i++) {
        // Free memory for FIFOs
        unlink(exchange_fifo[i]);
        unlink(trader_fifo[i]);

        free(exchange_fifo[i]);
        free(trader_fifo[i]);

        // Close all fd
        close(exchange_fd[i]);
        close(trader_fd[i]);
    }
    free(exchange_fd);
    free(trader_fd);
    free(exchange_fifo);
    free(trader_fifo);
    

    return 0;
}

int read_products(char *path, int *num_products, char ***products) {
    FILE *file;

    file = fopen(path, "r");
    if (file == NULL) {
        return -1;
    }

    char line[PRODUCT_CHAR_LIMIT];
    // Read the first line
    fgets(line, PRODUCT_CHAR_LIMIT, file);
    // Parse string line -> integer
    *num_products = atoi(line);
    *products = malloc(sizeof(char *) * (*num_products));
    for (int i = 0; i < *num_products; i++) {
        fgets(line, PRODUCT_CHAR_LIMIT, file);
        (*products)[i] = malloc(sizeof(char) * (PRODUCT_CHAR_LIMIT + 1));
        line[strcspn(line, "\n")] = '\0';
        strcpy((*products)[i], line);
    }

    fclose(file);
    return 0;
}

int read_products(char *path, int *num_products, char ***products) {
    FILE *file;

    file = fopen(path, "r");
    if (file == NULL) {
        return -1;
    }

    char line[PRODUCT_CHAR_LIMIT];
    // Read the first line
    fgets(line, PRODUCT_CHAR_LIMIT, file);
    // Parse string line -> integer
    *num_products = atoi(line);
    *products = malloc(sizeof(char *) * (*num_products));
    for (int i = 0; i < *num_products; i++) {
        fgets(line, PRODUCT_CHAR_LIMIT, file);
        (*products)[i] = malloc(sizeof(char) * (PRODUCT_CHAR_LIMIT + 1));
        line[strcspn(line, "\n")] = '\0';
        strcpy((*products)[i], line);
    }

    fclose(file);
    return 0;
}

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
    char **words = (char **)malloc(*num_orders * sizeof(char *));
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
