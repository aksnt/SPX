#ifndef SPX_TRADER_H
#define SPX_TRADER_H

#include "spx_common.h"
#include "utility.h"


char *get_message_t(char *input);
char *read_from_exchange();
void send_to_exchange(char *msg);

#endif
