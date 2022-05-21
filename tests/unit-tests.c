#include "../spx_exchange.h"
#include "../spx_common.h"
#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <math.h>

#include "cmocka.h"

#define TESTING
// #include "../spx_exchange.c"    
#include "../spx_trader.h"

int main(void) {
    const struct CMUnitTest tests [] = {
        cmocka_unit_test(test_get_PID)
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}



static void test_get_PID(void **state) {
    int PID = 1;
    int result = get_PID(PID);
    assert_int_equal(result, 0);
}
