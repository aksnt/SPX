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
char *read_from_trader(int trader_id);

int num_products;
int num_traders;

char **products;
int *exchange_fd;
int *trader_fd;
char **exchange_fifo;
char **trader_fifo;
char received_msg[FIFO_LIMIT];
char *sent_msg;
char rec_order[ORDER_SIZE_LIMIT];
pid_t children[FIFO_LIMIT];
char **words;
int num_words;

void check_order(int trader_id) {
    words = get_order(read_from_trader(trader_id), &num_words);
    printf("[T%d] Parsing command: <%s>", trader_id, read_from_trader(trader_id));
    // if (strcmp(words[0], "BUY") == 0) {
    // }
}

void sig_handle(int sig) {
    // get message/order
    // READ till ;
    // Parse input and print it (logic happens here)

    if (sig == SIGUSR1) {
        for (int i = 0; i < num_traders; i++) {
            printf("caught\n");
            // check_order(i);
            // sent_msg = "ACCEPTED;";
            // write(exchange_fd[i], sent_msg, strlen(sent_msg) + 1);
            // kill(children[i], SIGUSR1);
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
    num_traders = argc - 2;
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
        // write_to_trader(children[i], "MARKET OPEN;");
        sent_msg = "MARKET OPEN;";
        write(exchange_fd[i], sent_msg, strlen(sent_msg) + 1);
        kill(children[i], SIGUSR1);
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
