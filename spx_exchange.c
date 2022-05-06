/**
 * comp2017 - assignment 3
 * <your name>
 * <your unikey>
 */

#include "spx_exchange.h"

int SPX_print(const char *restrict format, ...);
char *get_message(char *input);
int number_orders(char *line);
char **get_order(char *line, int *num_words);

int num_products;
char **products;
int *exchange_fd;
int *trader_fd;
char **exchange_fifo;
char **trader_fifo;
char received_msg[FIFO_LIMIT];
char *sent_msg;
char rec_order[ORDER_SIZE_LIMIT];
pid_t children[FIFO_LIMIT];

char *read_from_trader(int trader_id);

void sig_handle(int sig) {
    // get message/order
    // READ till ;
    // Parse input and print it (logic happens here)
    if (sig == SIGUSR1) {
        printf("caught-exchange\n");
        for (int i = 0; i < num_products; i++) {
            if (strcmp(read_from_trader(i), "BUY 0 GPU 30 500") == 0) {
                printf("caught-exchange\n");
            }
        }
    }
}

char *read_from_trader(int trader_id) {
    trader_fd[trader_id] = open(trader_fifo[trader_id], O_RDONLY);
    read(trader_fd[trader_id], received_msg, FIFO_LIMIT);
    close(trader_fd[trader_id]);
    return get_message(received_msg);
}

void write_to_trader(int trader_id, char *message) {
    write(exchange_fd[trader_id], message, strlen(message) + 1);
    kill(children[trader_id], SIGUSR1);
}

int main(int argc, char **argv) {
    SPX_print("Starting\n");
    read_products(argv[1], &num_products, &products);

    SPX_print("Trading %d products:", num_products);
    for (int i = 0; i < num_products; ++i) {
        printf(" %s", products[i]);
    }
    printf("\n");

    // FIFO: Begin
    int num_traders = argc - 2;
    exchange_fd = malloc(sizeof(int) * num_traders);
    trader_fd = malloc(sizeof(int) * num_traders);
    exchange_fifo = malloc(sizeof(char *) * num_traders);
    trader_fifo = malloc(sizeof(char *) * num_traders);

    for (int i = 0; i < num_traders; ++i) {
        exchange_fifo[i] = malloc(sizeof(char) * FIFO_LIMIT);
        trader_fifo[i] = malloc(sizeof(char) * FIFO_LIMIT);
        sprintf(exchange_fifo[i], FIFO_EXCHANGE, i);
        sprintf(trader_fifo[i], FIFO_TRADER, i);

        // Make FIFOs
        SPX_print("Created FIFO /tmp/spx_exchange_%d\n", i);
        mkfifo(exchange_fifo[i], 0666);

        SPX_print("Created FIFO /tmp/spx_trader_%d\n", i);
        mkfifo(trader_fifo[i], 0666);

        SPX_print("Starting trader %d (%s)\n", i, argv[2 + i]);

        // Start each child process
        pid_t res = fork();
        if (res == 0) {
            // child process
            char trader_id[FIFO_LIMIT];
            sprintf(trader_id, "%d", i);
            char *args[] = {argv[2 + i], trader_id, NULL};

            // printf("[CHILD]: Exchange is %d. Child is %d. Parent is %d.\n", getpid(), res, getppid());
            execvp(argv[2 + i], args);
            exit(0);
        } else {
            // parent process
            signal(SIGUSR1, sig_handle);
            children[i] = res;  // stores the process ID of each trader

            // printf("[PARENT]: Trader is %d. Parent is %d. VSCODE is %d.\n", getpid(), res, getppid());
            SPX_print("Connected to /tmp/spx_exchange_%d\n", i);
            exchange_fd[i] = open(exchange_fifo[i], O_WRONLY);

            SPX_print("Connected to /tmp/spx_trader_%d\n", i);
            trader_fd[i] = open(trader_fifo[i], O_RDONLY);
        }
    }

    for (int i = 0; i < num_traders; i++) {
        // write_to_trader(i, "MARKET OPEN;");
        kill(children[i], SIGUSR1);
        // sent_msg = "MARKET OPEN;";
        // write(exchange_fd[i], sent_msg, strlen(sent_msg) + 1);     
    }

    // While loop checking how many SIGCHLDs signals received, if == num_traders -> calc fees
    // If any SIGCHLD received --> disconnect the child and trader that sent it

    // Free products memory
    for (int i = 0; i < num_products; i++) {
        free(products[i]);
    }

    free(products);

    for (int i = 0; i < num_traders; i++) {
        // Free memory for FIFOs
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
