/**
 * comp2017 - assignment 3
 * <your name>
 * <your unikey>
 */

#include "spx_exchange.h"

#include "spx_common.h"

int SPX_print(const char *restrict format, ...);
char *get_message(char *input);
int number_orders(char *line);
char *read_from_trader(int trader_id);
void write_to_trader(int trader_id, char *message);

// Utility variables storing total products, traders and all products
int num_products;
int num_traders;
char **products;
int trading_fees;

// Data structures used are two linked lists and a multidimensional array
order **buybook = NULL;   // buy orderbook
order **sellbook = NULL;  // sell orderbook
int ***matchbook;         // stores matched orders for each trader

// Global signal variables
volatile sig_atomic_t sigusr1;         // flag variable for logic
volatile sig_atomic_t sigchld;         // counts children
volatile sig_atomic_t trader_idx;      // stores trader index given by sending process
volatile sig_atomic_t child_idx = -1;  // stores trader id of child that sent SIGCHLD

char received_msg[ORDER_SIZE_LIMIT];
int *exchange_fd;
int *trader_fd;

pid_t *children;

int get_PID(pid_t pid) {
    for (int i = 0; i < num_traders; i++) {
        if (pid == children[i])
            return i;
    }
    return 0;
}

void signal_handler(int sig, siginfo_t *sinfo, void *context) {
    if (sig == SIGUSR1) {
        sigusr1 = 1;  // set flag to true
        trader_idx = get_PID(sinfo->si_pid);
    }
    if (sig == SIGCHLD) {
        sigchld++;  // increment to count traders disconnected
        child_idx = get_PID(sinfo->si_pid);
    }
}

int main(int argc, char **argv) {
    SPX_print(" Starting\n");
    read_products(argv[1], &num_products, &products);

    // initialise total traders
    num_traders = argc - 2;

    // allocate memory for all 3 books
    buybook = (order **)calloc(num_products, sizeof(order *));
    sellbook = (order **)calloc(num_products, sizeof(order *));
    matchbook = (int ***)malloc(sizeof(int **) * num_traders);

    // allocate memory for all dimensions of matchbook
    for (int i = 0; i < num_traders; i++) {
        matchbook[i] = (int **)calloc(num_products, sizeof(int *));
        for (int j = 0; j < num_products; j++)
            matchbook[i][j] = (int *)calloc(2, sizeof(int));
    }

    SPX_print(" Trading %d products:", num_products);
    for (int i = 0; i < num_products; ++i) {
        printf(" %s", products[i]);
    }
    printf("\n");

    // FIFO: Begin
    exchange_fd = malloc(sizeof(int) * num_traders);
    trader_fd = malloc(sizeof(int) * num_traders);
    char **exchange_fifo = malloc(sizeof(char *) * num_traders);
    char **trader_fifo = malloc(sizeof(char *) * num_traders);

    // Signal handlers
    struct sigaction sa;
    memset(&sa, 0, sizeof(struct sigaction));
    sa.sa_sigaction = &signal_handler;
    sa.sa_flags = SA_SIGINFO;

    sigaction(SIGUSR1, &sa, NULL);  // handle SIGUSR1
    sigaction(SIGCHLD, &sa, NULL);  // handle SIGCHLD

    children = malloc(sizeof(pid_t) * num_traders);

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
        SPX_print(" Created FIFO %s\n", exchange_fifo[i]);

        unlink(trader_fifo[i]);
        if (mkfifo(trader_fifo[i], 0666) == -1) {
            printf("ERROR: Failed to create FIFO /tmp/spx_trader_%d", i);
        }
        SPX_print(" Created FIFO %s\n", trader_fifo[i]);
        usleep(1);
        SPX_print(" Starting trader %d (%s)\n", i, argv[2 + i]);

        // Start each trader as a child process
        pid_t res = fork();
        if (res == 0) {
            // Inside child process
            char trader_id[FIFO_LIMIT];
            sprintf(trader_id, "%d", i);
            char *args[] = {argv[2 + i], trader_id, NULL};

            execvp(argv[2 + i], args);
            exit(0);
        } else {
            // Inside parent process
            children[i] = res;  // stores the process ID of each trader

            exchange_fd[i] = open(exchange_fifo[i], O_WRONLY);
            SPX_print(" Connected to %s\n", exchange_fifo[i]);

            trader_fd[i] = open(trader_fifo[i], O_RDONLY);
            SPX_print(" Connected to %s\n", trader_fifo[i]);

            usleep(1);  // ensures each trader fd has time to setup properly
        }
    }

    // send market open to all traders
    for (int i = 0; i < num_traders; i++)
        write_to_trader(i, "MARKET OPEN;");

    // event loop
    while (sigchld < num_traders) {
        if (child_idx != -1) {
            SPX_print(" Trader %d disconnected\n", child_idx);
            child_idx = -1;  // reset flag
        }
        if (!sigusr1)
            pause();
        else {
            // Parse in the order
            char *buf = read_from_trader(trader_idx);
            SPX_print(" [T%d] Parsing command: <%s>\n", trader_idx, buf);

            // Add the order to the orderbook, if invalid then break
            if (!add_order(buf, trader_idx))
                break;

            // Attempt to match positions
            match_positions();

            // Print orderbook
            print_orderbook();

            // Print positions
            print_positions();

            // reset flag
            sigusr1 = 0;
        }
    }

    // Trading is now compelete
    if (sigchld == 1) {  // only one trader thus first if condition does not run
        if (child_idx != -1)
            SPX_print(" Trader %d disconnected\n", child_idx);
    }
    SPX_print(" Trading completed\n");
    SPX_print(" Exchange fees collected: $%d\n", trading_fees);

    /* TEARDOWN OPERATIONS */

    //  Free products and books memory
    for (int i = 0; i < num_products; i++) {
        free(products[i]);
        free(buybook[i]);
        free(sellbook[i]);
    }
    free(products);
    free(buybook);
    free(sellbook);

    // Free matchbook memory
    for (int i = 0; i < num_traders; i++) {
        for (int j = 0; j < num_products; j++) {
            free(matchbook[i][j]);
        }
        free(matchbook[i]);
    }
    free(matchbook);

    // Safetely cleanup and free memory for FIFOs
    for (int i = 0; i < num_traders; i++) {
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
    usleep(1);
    free(children);

    return 0;
}

int SPX_print(const char *restrict format, ...) {
    int res1, res2;
    va_list ap;
    res1 = printf("[SPX]");
    if (res1 < 0)
        return res1;

    va_start(ap, format);
    res2 = vprintf(format, ap);
    va_end(ap);
    return res2 < 0 ? res2 : res1 + res2;
}

char *get_message_e(char *input) {
    int delimiter = 0;
    for (int i = 0; i < strlen(input) + 1; ++i)
        if (input[i] == ';') {
            delimiter = i;
            break;
        }

    input[delimiter] = '\0';
    return input;
}

int num_words(char *line) {
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

void print_positions() {
    SPX_print("\t--POSITIONS--\n");

    for (int tdx = 0; tdx < num_traders; tdx++) {
        SPX_print("\tTrader %d: ", tdx);

        for (int pidx = 0, count = num_products; pidx < num_products; ++pidx, --count) {
            printf("%s ", products[pidx]);
            int value = matchbook[tdx][pidx][VALUE];
            int qty = matchbook[tdx][pidx][QUANTITY];
            printf("%d ($%d)", qty, value);

            if (count > 1)
                printf(", ");
            else
                printf("\n");
        }
    }
}

void remove_buybook(int pidx) {
    order *head = buybook[pidx];
    buybook[pidx] = (buybook[pidx])->next;
    free(head);  // free memory allocated
}

void remove_sellbook(int pidx) {
    order *head = sellbook[pidx];
    sellbook[pidx] = (sellbook[pidx])->next;
    free(head);  // free memory allocated
}

void signal_fill(int order_BID, int order_SID, int buy_qty, int sell_qty, int BID, int SID) {
    char buyer_fill[FIFO_LIMIT];
    char seller_fill[FIFO_LIMIT];

    sprintf(buyer_fill, FILL, order_BID, sell_qty);
    sprintf(seller_fill, FILL, order_SID, buy_qty);

    write_to_trader(BID, buyer_fill);
    write_to_trader(SID, seller_fill);
}

void match_positions() {
    // Match the highest buy with the lowest sell
    for (int i = 0; i < num_products; i++) {
        order *buyptr = buybook[i];
        order *sellptr = sellbook[i];

        // Insufficient orders to match for this product
        if (!buybook[i] || !sellbook[i]) {
            break;
        }

        int buy_price = (buybook[i])->price;    // head of buybook for a product
        int sell_price = (sellbook[i])->price;  // head of sellbook for a product
        int buy_qty = (buybook[i])->quantity;
        int sell_qty = (sellbook[i])->quantity;

        int BID = 0;
        int order_BID = 0;
        int SID = 0;
        int order_SID = 0;
        // While both books have orders, we attempt to match
        while (buyptr && sellptr) {
            int value = 0;
            double fee = 0;

            if (buy_price >= sell_price) {
                // execute order
                if (buy_qty == sell_qty) {
                    // calculate the transaction value and fee
                    value = buyptr->price * sellptr->quantity;
                    fee = value * FEE_PERCENTAGE;

                    order_BID = buyptr->order_id;
                    order_SID = sellptr->order_id;
                    BID = buyptr->trader_id;
                    SID = sellptr->trader_id;

                    // store order in matched orders for buyer and seller

                    matchbook[BID][i][VALUE] += -value;
                    matchbook[BID][i][QUANTITY] += buy_qty;

                    matchbook[SID][i][VALUE] += value;
                    matchbook[SID][i][QUANTITY] += -sell_qty;

                    // remove the order from orderbook as it is fulfilled
                    order *buy_head = buybook[i];
                    buybook[i] = (buybook[i])->next;
                    free(buy_head);  // free memory allocated
                    buyptr = buybook[i];

                    order *sell_head = sellbook[i];
                    sellbook[i] = (sellbook[i])->next;
                    free(sell_head);
                    sellptr = sellbook[i];
                }
                if (buy_qty > sell_qty) {
                    // delete the sell order and modify buy order
                    (buybook[i])->quantity =
                        (buybook[i])->quantity - (sellbook[i])->quantity;

                    value = buyptr->price * (sellbook[i])->quantity;
                    fee = value * FEE_PERCENTAGE;

                    order_BID = buyptr->order_id;
                    order_SID = sellptr->order_id;
                    BID = buyptr->trader_id;
                    SID = sellptr->trader_id;

                    // store order in matched orders for buyer and seller
                    matchbook[BID][i][VALUE] += -value;
                    matchbook[BID][i][QUANTITY] += sell_qty;

                    matchbook[SID][i][VALUE] += value;
                    matchbook[SID][i][QUANTITY] += -sell_qty;

                    order *sell_head = sellbook[i];
                    sellbook[i] = (sellbook[i])->next;
                    free(sell_head);
                    sellptr = sellbook[i];
                }
                if (buy_qty < sell_qty) {
                    // delete the buy order and modify sell order
                    (sellbook[i])->quantity =
                        (sellbook[i])->quantity - (buybook[i])->quantity;

                    value = (buybook[i])->price * (buybook[i])->quantity;
                    fee = value * FEE_PERCENTAGE;

                    order_BID = buyptr->order_id;
                    order_SID = sellptr->order_id;
                    BID = buyptr->trader_id;
                    SID = sellptr->trader_id;

                    matchbook[BID][i][VALUE] += -value;
                    matchbook[BID][i][QUANTITY] += buy_qty;

                    matchbook[SID][i][VALUE] += value;
                    matchbook[SID][i][QUANTITY] += -buy_qty;

                    order *buy_head = buybook[i];
                    buybook[i] = (buybook[i])->next;
                    free(buy_head);  // free memory allocated
                    buyptr = buybook[i];
                }

                SPX_print(" Match: Order %d [T%d], New Order %d [T%d], value: $%d, fee: $%.0f.\n",
                          order_BID, BID, order_SID, SID, value, round(fee));

                signal_fill(order_BID, order_SID, buy_qty, sell_qty, BID, SID);

                if (buyptr) {
                    buy_qty = buyptr->quantity;
                    buy_price = buyptr->price;
                }
                if (sellptr) {
                    sell_qty = sellptr->quantity;
                    sell_price = sellptr->price;
                }

                trading_fees += round(fee);
            } else {
                break;
            }
        }
    }
}

int invalid_order(char *order_type, order *new_order, int pidx) {
    order *buyptr = buybook[pidx];
    order *sellptr = sellbook[pidx];

    if (strcmp(order_type, "BUY") == 0) {
        while (buyptr) {
            if (new_order->order_id == buyptr->order_id &&
                new_order->trader_id == buyptr->trader_id) {
                return 1;  // order already exists, invalid
            }
            buyptr = buyptr->next;
        }
    }

    if (strcmp(order_type, "SELL") == 0) {
        while (sellptr) {
            if (new_order->order_id == sellptr->order_id &&
                new_order->trader_id == sellptr->trader_id) {
                return 1;  // order already exists, invalid
            }
            sellptr = sellptr->next;
        }
    }
    // order doesn't exist hence, valid
    return 0;
}

int get_pidx(char *product) {
    for (int i = 0; i < num_products; i++) {
        if (strcmp(products[i], product) == 0) {
            return i;
        }
    }
    return -1;
}

int find_and_remove(char *order_line, int trader_id, int returns[]) {
    char *order_type = strtok(order_line, " ");
    if (strcmp(order_type, "AMEND") != 0) {
        exit(1);
    }

    int OID = atoi(strtok(NULL, " "));
    int TID = trader_id;

    // search both books for a match
    for (int i = 0; i < num_products; i++) {
        order *buyptr = buybook[i];
        order *head = buybook[i];
        order *prev = buyptr;

        while (buyptr) {
            if (buyptr->order_id == OID && buyptr->trader_id == TID) {
                // order found, remove it and return 1
                if (buyptr == head) {
                    // remove order at head
                    buybook[i] = buybook[i]->next;
                    free(head);
                    returns[0] = i;
                    returns[1] = 1;
                    return 0;
                } else {
                    // not at head, remove in middle or tail
                    prev->next = buyptr->next;
                    free(buyptr);
                    returns[0] = i;
                    returns[1] = 1;
                    return 0;
                }
            }
            prev = buyptr;
            buyptr = buyptr->next;
        }
    }

    for (int j = 0; j < num_products; j++) {
        order *sellptr = sellbook[j];
        order *prev1 = sellptr;
        while (sellptr) {
            if (sellptr->order_id == OID && sellptr->trader_id == TID) {
                // order found, remove it and return 2
                if (sellptr == sellbook[j]) {
                    // remove order at head
                    sellbook[j] = sellbook[j]->next;
                    free(sellptr);
                    returns[0] = j;
                    returns[1] = 2;
                    return 0;
                } else {
                    // not at head, remove in middle or tail
                    prev1->next = sellptr->next;
                    free(sellptr);
                    returns[0] = j;
                    returns[1] = 2;
                    return 0;
                }
            }
            prev1 = sellptr;
            sellptr = sellptr->next;
        }
    }
    return 1;
}

void signal_accepted(int trader_id, int order_id, char *order_type, int pidx, int qty, int price) {
    for (int j = 0; j < num_traders; j++) {
        if (j == trader_id) {
            char msg[FIFO_LIMIT];
            sprintf(msg, ACCEPTED, order_id);
            write(exchange_fd[j], msg, strlen(msg));
        } else {
            char msg2[FIFO_LIMIT];
            sprintf(msg2, MARKET, order_type, products[pidx], qty, price);
            write(exchange_fd[j], msg2, strlen(msg2));
        }
        // signal traders in both cases with their respective message
        kill(children[j], SIGUSR1);
    }
}

int add_order(char *order_line, int trader_id) {
    int do_buy = 0;
    int do_sell = 0;

    int amended = 0;
    int results[2];  // stores results returned by amend logic

    char *copy = (char *)malloc(sizeof(char) * strlen(order_line) + 1);
    strcpy(copy, order_line);
    char *order_type = strtok(order_line, " ");
    order *new_order = (order *)malloc(sizeof(order));
    new_order->order_id = atoi(strtok(NULL, " "));

    if (strcmp(order_type, "BUY") == 0)
        do_buy = 1;

    if (strcmp(order_type, "SELL") == 0)
        do_sell = 1;

    if (strcmp(order_type, "AMEND") == 0) {
        // find out which book the order is in
        find_and_remove(copy, trader_id, results);
        if (results[1] == 1)
            do_buy = 1;
        else if (results[1] == 2)
            do_sell = 1;
        amended = 1;
    }

    if (strcmp(order_type, "CANCEL") == 0) {
        find_and_remove(copy, trader_id, results);
        char msg[FIFO_LIMIT];
        sprintf(msg, CANCELLED, new_order->order_id);
        write_to_trader(trader_id, msg);
        return 2;
    }

    free(copy);

    int pidx = 0;
    if (!amended) {
        char *product_type = strtok(NULL, " ");
        pidx = get_pidx(product_type);
        if (pidx != -1)
            new_order->product_idx = pidx;
        else
            return 0;
    }
    if (amended)
        pidx = results[0];

    new_order->quantity = atoi(strtok(NULL, " "));
    new_order->price = atoi(strtok(NULL, " "));
    new_order->trader_id = trader_id;
    new_order->flag = 0;

    if (invalid_order(order_type, new_order, pidx)) {
        // write to trader that order is invalid
        write_to_trader(trader_id, "INVALID");
        return 0;
    }

    // If the order was amended, send amended
    if (amended) {
        char msg[FIFO_LIMIT];
        sprintf(msg, AMENDED, new_order->order_id);
        write_to_trader(trader_id, msg);

    } else {
        // send accepeted to trader and market <order details> to others
        signal_accepted(trader_id, new_order->order_id,
                        order_type, pidx, new_order->quantity, new_order->price);
    }

    // Insert in order of price -> according to buy or sell
    if (do_buy) {
        // Insert into buy book from highest to lowest
        if (!buybook[pidx] || (buybook[pidx])->price < new_order->price) {
            new_order->next = buybook[pidx];
            buybook[pidx] = new_order;
        } else {
            order *cursor = buybook[pidx];
            while (cursor->next && (cursor->next->price >= new_order->price)) {
                cursor = cursor->next;
            }
            new_order->next = cursor->next;
            cursor->next = new_order;
        }

    } else if (do_sell) {
        // Insert into sell book from lowest to highest order
        if (!sellbook[pidx] || (sellbook[pidx])->price > new_order->price) {
            new_order->next = sellbook[pidx];
            sellbook[pidx] = new_order;

        } else {
            order *cursor = sellbook[pidx];
            while (cursor->next && cursor->next->price <= new_order->price) {
                cursor = cursor->next;
            }
            new_order->next = cursor->next;
            cursor->next = new_order;
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

    int buy = 0;
    int sell = 0;

    for (int i = 0; i < num_products; ++i) {
        order *buyptr = buybook[i];
        order *sellptr = sellbook[i];
        buy = count_levels(i, BUYBOOK);
        sell = count_levels(i, SELLBOOK);

        SPX_print("\tProduct: %s; Buy levels: %d; Sell levels: %d\n",
                  products[i], buy, sell);

        /* The following declares needed variables and then prints the orderbook */
        int num_buy = 1;
        int num_sell = 1;
        int buy_qty = 0;
        int sell_qty = 0;

        if (buyptr)
            buy_qty = buyptr->quantity;
        if (sellptr)
            sell_qty = sellptr->quantity;

        // Go through all orders and mark flags
        if (buyptr && !sellptr) {  // only have buy orders
            while (buyptr) {
                if (buyptr->next) {  // If there is a next order, check it
                    order *cursor = buyptr;
                    while (cursor->next) {
                        if (cursor->price == cursor->next->price) {
                            cursor->next->flag = 1;  // it is a duplicate
                            buy_qty += cursor->next->quantity;
                            num_buy++;
                            cursor = cursor->next;
                        } else {
                            break;
                        }
                    }
                }

                if (num_buy == 1) {
                    if (!buyptr->flag) {
                        SPX_print("\t\tBUY %d @ $%d (1 order)\n", buy_qty, buyptr->price);
                    }
                } else {
                    if (!buyptr->flag) {
                        SPX_print("\t\tBUY %d @ $%d (%d orders)\n", buy_qty, buyptr->price, num_buy);
                        num_buy = 1;
                    }
                }
                buyptr = buyptr->next;
                if (buyptr)
                    buy_qty = buyptr->quantity;
            }
        }

        if (!buyptr && sellptr) {  // only have sell orders
            while (sellptr) {
                if (sellptr->next) {  // If there is a next order, check it
                    order *cursor = sellptr;
                    while (cursor->next) {
                        if (cursor->price == cursor->next->price) {
                            cursor->next->flag = 1;  // it is a duplicate
                            sell_qty += cursor->next->quantity;
                            num_sell++;
                            cursor = cursor->next;
                        } else {
                            break;
                        }
                    }
                }
                if (num_sell == 1) {
                    if (!sellptr->flag) {
                        SPX_print("\t\tSELL %d @ $%d (1 order)\n", sell_qty, sellptr->price);
                    }
                } else {
                    if (!sellptr->flag) {
                        SPX_print("\t\tSELL %d @ $%d (%d orders)\n", sell_qty, sellptr->price, num_sell);
                        num_sell = 1;
                    }
                }
                sellptr = sellptr->next;
                if (sellptr)
                    sell_qty = sellptr->quantity;
            }
        }

        if (buyptr && sellptr) {  // print sell first
            while (sellptr) {
                if (sellptr->next) {  // If there is a next order, check it
                    order *cursor = sellptr;
                    while (cursor->next) {
                        if (cursor->price == cursor->next->price) {
                            cursor->next->flag = 1;  // it is a duplicate
                            sell_qty += cursor->next->quantity;
                            num_sell++;
                            cursor = cursor->next;
                        } else {
                            break;
                        }
                    }
                }
                if (num_sell == 1) {
                    if (!sellptr->flag) {
                        SPX_print("\t\tSELL %d @ $%d (1 order)\n", sell_qty, sellptr->price);
                    }
                } else {
                    if (!sellptr->flag) {
                        SPX_print("\t\tSELL %d @ $%d (%d orders)\n", sell_qty, sellptr->price, num_sell);
                        num_sell = 1;
                    }
                }
                sellptr = sellptr->next;
                if (sellptr)
                    sell_qty = sellptr->quantity;
            }

            while (buyptr) {
                if (buyptr->next) {  // If there is a next order, check it
                    order *cursor = buyptr;
                    while (cursor->next) {
                        if (cursor->price == cursor->next->price) {
                            cursor->next->flag = 1;  // it is a duplicate
                            buy_qty += cursor->next->quantity;
                            num_buy++;
                            cursor = cursor->next;
                        } else {
                            break;
                        }
                    }
                }

                if (num_buy == 1) {
                    if (!buyptr->flag) {
                        SPX_print("\t\tBUY %d @ $%d (1 order)\n", buy_qty, buyptr->price);
                    }
                } else {
                    if (!buyptr->flag) {
                        SPX_print("\t\tBUY %d @ $%d (%d orders)\n", buy_qty, buyptr->price, num_buy);
                        num_buy = 1;
                    }
                }
                buyptr = buyptr->next;
                if (buyptr)
                    buy_qty = buyptr->quantity;
            }
        }
    }
}

char *read_from_trader(int trader_id) {
    read(trader_fd[trader_id], received_msg, sizeof(received_msg));
    return get_message_e(received_msg);
}

void write_to_trader(int trader_id, char *message) {
    write(exchange_fd[trader_id], message, strlen(message));
    kill(children[trader_id], SIGUSR1);
}
