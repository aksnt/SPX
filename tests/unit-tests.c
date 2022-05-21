#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <math.h>

#include "../spx_common.h"
#include "cmocka.h"

#define TESTING
#include "../spx_exchange.c"
#include "../spx_exchange.h"
#include "../spx_trader.h"

static void test_get_PID(void **state) {
    int PID = 1;
    int result = get_PID(PID);
    assert_int_equal(result, 0);
}