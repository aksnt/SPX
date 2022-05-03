/**
 * comp2017 - assignment 3
 * <your name>
 * <your unikey>
 */

#include "spx_exchange.h"

#include "spx_common.h"

int num_products;
char **products;
int *exchange_fd;
int *trader_fd;
char **exchange_fifo;
char **trader_fifo;
char received_msg[FIFO_LIMIT];
char *sent_msg;
char rec_order[ORDER_SIZE_LIMIT];

void sig_handle(int sig) {
    if (sig == SIGUSR1) {
        printf("caught-exchange\n");
    }
}

char *read_from_trader(int trader_id) {
    trader_fd[trader_id] = open(trader_fifo[trader_id], O_RDONLY);
    read(trader_fd[trader_id], received_msg, FIFO_LIMIT);
    close(trader_fd[trader_id]);
    return get_message(received_msg);
}

int main(int argc, char **argv) {
    SPX_print("Starting\n");
    read_products(argv[1], &num_products, &products);

    SPX_print("Trading %d products: ", num_products);
    for (int i = 0; i < num_products; ++i) {
        printf("%s ", products[i]);
    }

    printf("\n");

    // FIFO: Begin
    int num_traders = argc - 2;
    exchange_fd = malloc(sizeof(int) * num_traders);
    trader_fd = malloc(sizeof(int) * num_traders);
    exchange_fifo = malloc(sizeof(char *) * num_traders);
    trader_fifo = malloc(sizeof(char *) * num_traders);

    pid_t children[FIFO_LIMIT];  // add children to this array

    for (int i = 0; i < num_traders; ++i) {
        exchange_fifo[i] = malloc(sizeof(char) * FIFO_LIMIT);
        trader_fifo[i] = malloc(sizeof(char) * FIFO_LIMIT);
        sprintf(exchange_fifo[i], FIFO_EXCHANGE, i);
        sprintf(trader_fifo[i], FIFO_TRADER, i);

        // Make FIFOs
        mkfifo(exchange_fifo[i], 0666);
        SPX_print("Created FIFO /tmp/spx_exchange_%d\n", i);

        mkfifo(trader_fifo[i], 0666);
        SPX_print("Created FIFO /tmp/spx_trader_%d\n", i);

        pid_t res = fork();
        pid_t cur_child = getpid();
        children[i] = cur_child;

        if (res == 0) {
            // child process
            char trader_id[FIFO_LIMIT];
            sprintf(trader_id, "%d", i);
            char *args[] = {argv[2 + i], trader_id, NULL};
            SPX_print("Starting trader %d (%s)\n", i, argv[2 + i]);
            printf("[CHILD]: Trader is %d. Child is %d. Parent is %d.\n", getpid(), children[i], getppid());
            execvp(argv[2 + i], args);
            exit(0);
        } else {
            // parent process
            //  Send "CONNECT" to trader (child process)

            exchange_fd[i] = open(exchange_fifo[i], O_WRONLY);
            sent_msg = "CONNECT;";
            write(exchange_fd[i], sent_msg, strlen(sent_msg) + 1);
            close(exchange_fd[i]);

            SPX_print("Connected to /tmp/spx_exchange_%d\n", i);
            SPX_print("Connected to /tmp/spx_trader_%d\n", i);
            printf("[PARENT]: Trader is %d. Child is %d. Parent is %d.\n", getpid(), children[i], getppid());

            // Wait for signal

            kill(getpid(), SIGUSR1);
            pause();

            if (strcmp(read_from_trader(i), "BUY 0 GPU 30 500") == 0) {
                strcpy(rec_order, read_from_trader(i));
                // rec_order = get_order(trade, &num_orders);
                SPX_print("[T%d] Parsing command: <%s>\n", i, read_from_trader(i));
            }

        }
    }


    sleep(1);
    // Free products memory
    for (int i = 0; i < num_products; i++) {
        free(products[i]);
    }

    free(products);

    for (int i = 0; i < num_traders; i++) {
        free(exchange_fifo[i]);
        free(trader_fifo[i]);
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
