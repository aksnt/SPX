#ifndef SPX_TRADER_H
#define SPX_TRADER_H

#include "spx_common.h"
#include "utility.h"

#define BUY "BUY %d %s %d %d;"
#define SELL "SELL %d %s %d %d;"

char *get_message_t(char *input);
char *read_from_exchange();
void send_to_exchange(char *msg);

#endif
