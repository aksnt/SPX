#include "spx_trader.h"

int trader_id;
int exchange_fd, trader_fd;
char exchange_fifo[FIFO_LIMIT], trader_fifo[FIFO_LIMIT];
char received_msg[FIFO_LIMIT];
char *sent_msg;
int market_open;
int order_id;
volatile sig_atomic_t sigusr1;

char *read_from_exchange();
void send_to_exchange(char *msg);

void do_order(char *order_line, int flag) {
    char *mkt = strtok(order_line, " ");
    char* order_type = strtok(NULL, " ");
    char* product_type = strtok(NULL, " ");
    int qty = strtok(NULL, " ");
    if (qty >= 1000) {
        return 0;
    }
    int price = strtok(NULL, " ");

    char auto_order[FIFO_LIMIT];

    if (flag == 1) 
        sprintf(auto_order, BUY, order_id++, product_type, qty, price);
    else 
        sprintf(auto_order, SELL, order_id++, product_type, qty, price);

    send_to_exchange(auto_order);
}

int *parse_message(char *order) {
    char *mkt = strtok(order, " ");
    if (!strcmp(mkt, "MARKET") == 0) {
        return 0;
    }
    char *order_type = strtok(NULL, " ");
    if (strcmp(order_type, "BUY") == 0) {
        return 1;
    }
    if (strcmp(order_type, "SELL") == 0) {
        return 2;
    }
}

char *get_message1(char *input) {
    int delimiter = 0;
    for (int i = 0; i < strlen(input) + 1; ++i)
        if (input[i] == ';') {
            delimiter = i;
            break;
        }

    input[delimiter] = '\0';
    return input;
}

void sig_handler(int sig) {
    if (sig == SIGUSR1) {
        if (strcmp(read_from_exchange(), "MARKET OPEN") == 0) {
            market_open = 1;
            sigusr1 = 1;
        }

        sigusr1 = 1;
    }
}

void send_to_exchange(char *msg) {
    write(trader_fd, msg, strlen(msg));
    kill(getppid(), SIGUSR1);
}

int main(int argc, char **argv) {
    if (argc < 2) {
        printf("Not enough arguments\n");
        return 1;
    }

    // connect to named pipes
    trader_id = atoi(argv[1]);
    sprintf(exchange_fifo, FIFO_EXCHANGE, trader_id);
    sprintf(trader_fifo, FIFO_TRADER, trader_id);

    exchange_fd = open(exchange_fifo, O_RDONLY);
    trader_fd = open(trader_fifo, O_WRONLY);

    // register signal handler
    struct sigaction sa;
    memset(&sa, 0, sizeof(struct sigaction));
    sa.sa_sigaction = &sig_handler;
    sa.sa_flags = SA_SIGINFO;

    // wait for exchange update (MARKET message)
    while (market_open) {
        // event loop:
        if (!sigusr1) {
            pause();
        } else {
            char *buf = read_from_exchange();
            char buf2[FIFO_LIMIT];
            strcpy(buf2, buf);
            int res = parse_message(buf);
            char buf3[FIFO_LIMIT];
            sprintf(buf3, ACCEPTED, order_id);
            if (res == 1) {
                do_order(buf2, 1);
            } else {
                do_order(buf2, 2);
            }
        }
    }
}

char *read_from_exchange() {
    read(exchange_fd, received_msg, FIFO_LIMIT);
    return get_message1(received_msg);
}