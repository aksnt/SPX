#include <math.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "../functions.h"
#include "../spx_common.h"
#include "../spx_exchange.h"
#include "../utility.h"
#include "cmocka.h"

#define TESTING
#include "../spx_exchange.c"
#include "../spx_trader.h"

static void test_get_PID(void **state) {
    int PID = 1;
    int result = get_PID(PID);
    assert_int_equal(result, 0);
}

static void test_get_PID_neg(void **state) {
    int PID = -1;
    int result = get_PID(PID);
    assert_int_equal(result, 0);
}

static void test_get_pidx(void **state) {
    char *product = "GPU";
    int result = get_pidx(product);
    assert_int_equal(result, 0);
}

static void test_get_pidx_invalid(void **state) {
    char *product = "xyz";
    int result = get_pidx(product);
    assert_int_equal(result, -1);
}

static void test_count_levels(void **state) {
    int flag = 0;
    int pidx = 0;
    int result = count_levels(pidx, flag);
    assert_int_equal(result, 0);
}

static void test_get_message_e(void **state) {
    char *input = "BUY 0 GPU 30 500; something useless";
    char *buf = get_message_e(input);
    assert_string_equal(input, "BUY 0 GPU 30 500");
}

static void test_get_message_t(void **state) {
    char *input = "SELL 0 GPU 30 500; something useless";
    char *buf = get_message_e(input);
    assert_string_equal(input, "SELL 0 GPU 30 500");
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_get_PID),
        cmocka_unit_test(test_get_PID_neg),
        cmocka_unit_test(test_get_pidx),
        cmocka_unit_test(test_get_pidx_invalid),
        cmocka_unit_test(test_count_levels),
        cmocka_unit_test(test_get_message_e),
        cmocka_unit_test(test_get_message_t)};
    return cmocka_run_group_tests(tests, NULL, NULL);
}
