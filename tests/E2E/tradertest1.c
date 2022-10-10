#include "tradertest.h"

int trader_id;
int exchange_fd, trader_fd;
char exchange_fifo[FIFO_LIMIT], trader_fifo[FIFO_LIMIT];
char received_msg[FIFO_LIMIT];
char *sent_msg;
int market_open;

void sig_handler(int sig) {
    if (sig == SIGUSR1) {
        if (strcmp(read_from_exchange(), "MARKET OPEN") == 0) {
            market_open = 1;
        }
    }
}

void send_to_exchange(char *msg) {
    write(trader_fd, msg, strlen(msg) + 1);
    kill(getppid(), SIGUSR1);
    pause();
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
    signal(SIGUSR1, sig_handler);

    while (1) {
        pause();
        if (market_open) {
            /****************************************/
            // write some orders here

            send_to_exchange("SELL 0 GPU 30 500;");
            // pause();

            /****************************************/

            // disconnect
            market_open = 0;
            
        }
    }
    kill(getppid(), SIGCHLD);
}

char *read_from_exchange() {
    read(exchange_fd, received_msg, FIFO_LIMIT);
    return get_message_t(received_msg);
}

char *get_message_t(char *input) {
    int delimiter = 0;
    if (strcmp(input, "DISCONNECT") == 0)
        return 0;
    for (int i = 0; i < strlen(input) + 1; ++i)
        if (input[i] == ';') {
            delimiter = i;
            break;
        }

    input[delimiter] = '\0';
    return input;
}