#ifndef SPX_COMMON_H
#define SPX_COMMON_H

#define _POSIX_C_SOURCE 200809L

#define _DEFAULT_SOURCE

#include "functions.h"
#include "utility.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <math.h>

#define FIFO_EXCHANGE "/tmp/spx_exchange_%d"
#define FIFO_TRADER "/tmp/spx_trader_%d"
#define FEE_PERCENTAGE 0.01
#define PRODUCT_CHAR_LIMIT 16
#define FIFO_LIMIT 80
#define MAX_ORDER 4
#define VALUE 0
#define QUANTITY 1
#define BUYBOOK 0
#define SELLBOOK 1
#define ACCEPTED "ACCEPTED %d;"
#define AMENDED "AMENDED %d;"
#define CANCELLED "CANCELLED %d;"
#define MARKET "MARKET %s %s %d %d;"
#define FILL "FILL %d %d;"

int SPX_print(const char *restrict format, ...);

#endif
