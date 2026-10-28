/* statusd's JSON writer: exact output, escaping, numbers that survive a round trip, and
 * every way to misuse it or run out of room. */
#include "interface/json.h"
#include "common/log.h"

#include <float.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures;

#define CHECK(cond, msg) do { \
    if (!(cond)) { fprintf(stderr, "  FAIL %s:%d: %s\n", __FILE__, __LINE__, msg); \
                   failures++; } } while (0)

/* out[] is followed by a guard, so a write past the end is caught here instead of
 * corrupting the test's own counters. */
#define OUT_SIZE 512u
#define GUARD_SIZE 16u
static char   area[OUT_SIZE + GUARD_SIZE];
static char  *const out = area;
static json_t j;

static void start(void)
{
    memset(area + OUT_SIZE, 0xAA, GUARD_SIZE);
    json_init(&j, out, OUT_SIZE);
}

static void check_guard(void)
{
    for (size_t i = 0; i < GUARD_SIZE; i++)
        if ((unsigned char)area[OUT_SIZE + i] != 0xAA) { CHECK(false, "wrote past the buffer"); return; }
}

/* The document is complete and reads exactly `want`. */
static void expect(const char *want, const char *msg)
{
    check_guard();
    ssize_t n = json_finish(&j);
    if (n != (ssize_t)strlen(want) || strcmp(out, want) != 0)
        fprintf(stderr, "  got  %s (%zd)\n  want %s\n", n < 0 ? "<failed>" : out, n, want);
    CHECK(n == (ssize_t)strlen(want) && strcmp(out, want) == 0, msg);
}

static void expect_failed(const char *msg)
{
    check_guard();
    CHECK(json_finish(&j) == -1, msg);
}

static void test_containers(void)
{
    start(); json_begin_object(&j); json_end_object(&j);
    expect("{}", "empty object");
    start(); json_begin_array(&j); json_end_array(&j);
    expect("[]", "empty array");

    for (int n = 0; n <= 3; n++) {
        char want[16] = "[";
        start(); json_begin_array(&j);
        for (int i = 1; i <= n; i++) {
            json_uint(&j, (unsigned)i);
            snprintf(want + strlen(want), sizeof want - strlen(want), "%s%d", i > 1 ? "," : "", i);
        }
        json_end_array(&j);
        strcat(want, "]");
        expect(want, "commas in an array of 0 to 3 items");
    }

    start(); json_begin_object(&j);
    json_key(&j, "a"); json_uint(&j, 1);
    json_key(&j, "b"); json_uint(&j, 2);
    json_key(&j, "c"); json_uint(&j, 3);
    json_end_object(&j);
    expect("{\"a\":1,\"b\":2,\"c\":3}", "commas in an object");

    start(); json_begin_array(&j);
    json_begin_array(&j); json_end_array(&j);
    json_begin_array(&j); json_end_array(&j);
    json_end_array(&j);
    expect("[[],[]]", "a container after a container");

    start(); json_begin_object(&j);
    json_key(&j, "a"); json_begin_object(&j); json_end_object(&j);
    json_key(&j, "b"); json_begin_array(&j); json_end_array(&j);
    json_end_object(&j);
    expect("{\"a\":{},\"b\":[]}", "nested containers as object values");

    /* The shape statusd will send. */
    start(); json_begin_object(&j);
    json_key(&j, "ok"); json_bool(&j, true);
    json_key(&j, "records"); json_begin_array(&j);
    json_begin_object(&j);
    json_key(&j, "tag"); json_string(&j, "proc");
    json_key(&j, "up"); json_uint(&j, 1);
    json_end_object(&j);
    json_begin_object(&j); json_key(&j, "tag"); json_string(&j, "idle"); json_end_object(&j);
    json_end_array(&j);
    json_end_object(&j);
    expect("{\"ok\":true,\"records\":[{\"tag\":\"proc\",\"up\":1},{\"tag\":\"idle\"}]}",
           "a reply document");
}

static void test_top_level_scalars(void)
{
    start(); json_bool(&j, true);   expect("true", "top-level true");
    start(); json_null(&j);         expect("null", "top-level null");
    start(); json_int(&j, -5);      expect("-5", "top-level number");
    start(); json_string(&j, "x");  expect("\"x\"", "top-level string");
}

static void test_strings(void)
{
    start(); json_string(&j, "");
    expect("\"\"", "empty string");

    start(); json_string(&j, "\"\\\b\f\n\r\t\x01\x1f\x7f/<");
    expect("\"\\\"\\\\\\b\\f\\n\\r\\t\\u0001\\u001f\\u007f/<\"", "every escape");

    start(); json_begin_object(&j); json_key(&j, "a\"b"); json_uint(&j, 1); json_end_object(&j);
    expect("{\"a\\\"b\":1}", "keys are escaped too");

    start(); json_string(&j, "plain text 123 ~");
    expect("\"plain text 123 ~\"", "printable ASCII passes through");
}

static void test_ints(void)
{
    start(); json_int(&j, 0);          expect("0", "int 0");
    start(); json_int(&j, -1);         expect("-1", "int -1");
    start(); json_int(&j, LLONG_MIN);  expect("-9223372036854775808", "LLONG_MIN");
    start(); json_int(&j, LLONG_MAX);  expect("9223372036854775807", "LLONG_MAX");
    start(); json_uint(&j, 0);         expect("0", "uint 0");
    start(); json_uint(&j, ULLONG_MAX); expect("18446744073709551615", "ULLONG_MAX");
}

static void test_doubles(void)
{
    static const struct { double v; const char *text; } exact[] = {
        { 0.0, "0" }, { -0.0, "-0" }, { 0.5, "0.5" }, { 0.1, "0.1" }, { 100.0, "100" },
        { 1e21, "1e+21" }, { 1e-7, "1e-07" }, { -1750.25, "-1750.25" },
    };
    for (size_t i = 0; i < sizeof exact / sizeof exact[0]; i++) {
        start(); json_double(&j, exact[i].v);
        expect(exact[i].text, "the shortest text that reads back");
    }

    static const double round_trip[] = {
        DBL_MAX, -DBL_MAX, DBL_MIN, DBL_EPSILON, 5e-324, 1.0 / 3.0, 123456789.123456789,
        0.30000000000000004, 9007199254740993.0, 1e308, 1e-308, M_PI, -M_E,
    };
    for (size_t i = 0; i < sizeof round_trip / sizeof round_trip[0]; i++) {
        start(); json_double(&j, round_trip[i]);
        CHECK(json_finish(&j) > 0, "a finite double is written");
        CHECK(json_is_number(out), "and is a valid JSON number");
        CHECK(strtod(out, NULL) == round_trip[i], "and reads back as the same double");
    }

    start(); json_double(&j, 0.5);
    CHECK(json_finish(&j) > 0 && strchr(out, '.') != NULL, "the decimal point is a dot");

    start(); json_double(&j, NAN);       expect_failed("NaN fails");
    start(); json_double(&j, INFINITY);  expect_failed("+infinity fails");
    start(); json_double(&j, -INFINITY); expect_failed("-infinity fails");
}

static void test_null_and_bool(void)
{
    start(); json_begin_array(&j);
    json_bool(&j, true); json_bool(&j, false); json_null(&j);
    json_end_array(&j);
    expect("[true,false,null]", "bool and null");

    start(); json_begin_object(&j);
    json_key(&j, "a"); json_uint(&j, 1);
    json_key(&j, "b"); json_null(&j);
    json_end_object(&j);
    expect("{\"a\":1,\"b\":null}", "null as an object value");
}

static void test_is_number(void)
{
    static const char *good[] = { "0", "-0", "1", "-1", "10", "1.5", "-1.5", "0.5", "1e5",
                                  "1E5", "1e+5", "1E-5", "-1.5e-3", "0e0", "123456789012345678901" };
    static const char *bad[] = { "", "-", "+1", "01", "-01", ".5", "5.", "-.5", "1e", "1e+",
                                 "1.e5", "0x10", "NaN", "nan", "Infinity", "inf", " 1", "1 ",
                                 "--1", "1.2.3", "1e5.5", "1,5", "null", "true", "\"1\"", "1a" };

    for (size_t i = 0; i < sizeof good / sizeof good[0]; i++)
        CHECK(json_is_number(good[i]), good[i]);
    for (size_t i = 0; i < sizeof bad / sizeof bad[0]; i++)
        CHECK(!json_is_number(bad[i]), bad[i]);
    CHECK(!json_is_number(NULL), "NULL is not a number");

    start(); json_begin_array(&j); json_number(&j, "1.5e3"); json_number(&j, "-0"); json_end_array(&j);
    expect("[1.5e3,-0]", "json_number writes the text as given");

    start(); json_number(&j, "0C");  expect_failed("json_number refuses 0C");
    start(); json_number(&j, "");    expect_failed("json_number refuses empty text");
    start(); json_number(&j, NULL);  expect_failed("json_number refuses NULL");
}

static void test_capacity(void)
{
    /* {"a":1} is 7 bytes; the buffer also needs a NUL. Bytes past cap must never change. */
    for (size_t cap = 0; cap <= 9; cap++) {
        char b[24];
        memset(b, 0x55, sizeof b);
        json_init(&j, b, cap);
        json_begin_object(&j); json_key(&j, "a"); json_uint(&j, 1); json_end_object(&j);
        ssize_t n = json_finish(&j);
        CHECK((cap >= 8) == (n == 7), "fits exactly when cap is length plus one");
        if (cap >= 8) CHECK(strcmp(b, "{\"a\":1}") == 0, "the document is NUL-terminated");
        for (size_t i = cap; i < sizeof b; i++)
            if (b[i] != 0x55) { CHECK(false, "wrote past cap"); break; }
    }

    char b[16];
    json_init(&j, b, 0);
    CHECK(json_finish(&j) == -1, "a zero-size buffer fails");

    /* A long string must fail cleanly, not overrun. */
    char big[600];
    memset(big, 'x', sizeof big - 1); big[sizeof big - 1] = '\0';
    start(); json_string(&j, big);
    expect_failed("a string longer than the buffer");
}

static void test_misuse(void)
{
    /* Each case below is otherwise complete and closed, so only the misuse can fail it. */
    start(); json_begin_object(&j); json_uint(&j, 1); json_end_object(&j);
    expect_failed("a value with no key inside an object");
    start(); json_begin_array(&j); json_key(&j, "a"); json_uint(&j, 1); json_end_array(&j);
    expect_failed("a key inside an array");
    start(); json_key(&j, "a"); json_uint(&j, 1);
    expect_failed("a key at the top level");
    start(); json_begin_object(&j); json_key(&j, "a"); json_key(&j, "b"); json_uint(&j, 1);
    json_end_object(&j);
    expect_failed("two keys in a row");
    start(); json_begin_object(&j); json_key(&j, "a"); json_end_object(&j);
    expect_failed("an object ended after a key with no value");
    start(); json_end_object(&j);
    expect_failed("an end with no begin");
    start(); json_end_array(&j);
    expect_failed("an array end with no begin");
    start(); json_begin_object(&j); json_end_array(&j);
    expect_failed("an object closed as an array");
    start(); json_begin_array(&j); json_end_object(&j);
    expect_failed("an array closed as an object");

    start(); json_uint(&j, 1); json_uint(&j, 2);
    expect_failed("two top-level values");
    start(); json_begin_object(&j); json_end_object(&j); json_begin_object(&j);
    json_end_object(&j);
    expect_failed("a second top-level container");
    start(); json_begin_object(&j);
    expect_failed("a container never closed");
    start(); json_begin_array(&j); json_uint(&j, 1);
    expect_failed("an array never closed");
    start();
    expect_failed("nothing written");

    start();
    for (unsigned i = 0; i < JSON_MAX_DEPTH; i++) json_begin_array(&j);
    for (unsigned i = 0; i < JSON_MAX_DEPTH; i++) json_end_array(&j);
    CHECK(json_finish(&j) == (ssize_t)(2 * JSON_MAX_DEPTH), "nesting to the cap is fine");
    start();
    for (unsigned i = 0; i <= JSON_MAX_DEPTH; i++) json_begin_array(&j);
    for (unsigned i = 0; i <= JSON_MAX_DEPTH; i++) json_end_array(&j);
    expect_failed("nesting one past the cap");

    start(); json_string(&j, NULL);
    expect_failed("a NULL string");
    start(); json_begin_object(&j); json_key(&j, NULL); json_uint(&j, 1); json_end_object(&j);
    expect_failed("a NULL key");
    start(); json_string(&j, "caf\xc3\xa9");
    expect_failed("a non-ASCII string");
    start(); json_begin_object(&j); json_key(&j, "\x80"); json_uint(&j, 1); json_end_object(&j);
    expect_failed("a non-ASCII key");
}

static void test_failure_is_sticky(void)
{
    start(); json_begin_object(&j); json_end_array(&j);   /* fails here */
    size_t len = j.len;
    json_key(&j, "a"); json_uint(&j, 1); json_end_object(&j);
    CHECK(j.len == len, "nothing is written after a failure");
    expect_failed("the writer stays failed");

    start(); json_double(&j, NAN);
    json_begin_array(&j); json_end_array(&j);
    expect_failed("a good document after a bad value is still failed");
}

int main(void)
{
    log_init("test_json", LOG_FATAL);
    test_containers();
    test_top_level_scalars();
    test_strings();
    test_ints();
    test_doubles();
    test_null_and_bool();
    test_is_number();
    test_capacity();
    test_misuse();
    test_failure_is_sticky();
    return failures == 0 ? 0 : 1;
}
