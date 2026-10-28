/* statusd's HTTP layer without a socket: every refusal has a test, because the parser is
 * the one part of the product that reads bytes a stranger on the Wi-Fi chose. */
#include "interface/http.h"
#include "common/log.h"

#include <stdio.h>
#include <string.h>

static int failures;

#define CHECK(cond, msg) do { \
    if (!(cond)) { fprintf(stderr, "  FAIL %s:%d: %s\n", __FILE__, __LINE__, msg); \
                   failures++; } } while (0)

static http_parse_t parse(const char *s, http_request_t *r)
{
    return http_parse_request(s, strlen(s), r);
}

static void test_good_requests(void)
{
    http_request_t r;

    CHECK(parse("GET / HTTP/1.1\r\nHost: box\r\n\r\n", &r) == HTTP_OK, "plain GET");
    CHECK(strcmp(r.path, "/") == 0 && r.query[0] == '\0', "path / and no query");

    CHECK(parse("GET /api/series?pid=0C&from=-2h HTTP/1.0\r\n\r\n", &r) == HTTP_OK,
          "GET with a query, HTTP/1.0, no headers");
    CHECK(strcmp(r.path, "/api/series") == 0, "path stops at '?'");
    CHECK(strcmp(r.query, "pid=0C&from=-2h") == 0, "query is what follows '?'");

    CHECK(parse("GET /a? HTTP/1.1\r\n\r\n", &r) == HTTP_OK && r.query[0] == '\0',
          "empty query");
    CHECK(parse("GET /a..b HTTP/1.1\r\n\r\n", &r) == HTTP_OK,
          "'..' inside a name is not a path segment");
}

static void test_every_split_is_incomplete_until_whole(void)
{
    const char *req = "GET /api/status HTTP/1.1\r\nHost: box\r\n\r\n";
    size_t n = strlen(req);
    http_request_t r;

    for (size_t cut = 0; cut < n; cut++)
        CHECK(http_parse_request(req, cut, &r) == HTTP_INCOMPLETE, "a prefix is incomplete");
    CHECK(http_parse_request(req, n, &r) == HTTP_OK, "the whole request parses");
    CHECK(strcmp(r.path, "/api/status") == 0, "path after a split read");
}

static void test_refusals(void)
{
    http_request_t r;

    CHECK(parse("POST / HTTP/1.1\r\n\r\n", &r) == HTTP_BAD_METHOD, "POST");
    CHECK(parse("get / HTTP/1.1\r\n\r\n", &r) == HTTP_BAD_METHOD, "methods are case-sensitive");
    CHECK(parse("DELETE /a HTTP/1.1\r\n\r\n", &r) == HTTP_BAD_METHOD, "DELETE");

    CHECK(parse("GARBAGE\r\n\r\n", &r) == HTTP_BAD, "no spaces at all");
    CHECK(parse("GET /\r\n\r\n", &r) == HTTP_BAD, "no version");
    CHECK(parse("GET / HTTP/2.0\r\n\r\n", &r) == HTTP_BAD, "HTTP/2.0");
    CHECK(parse("GET / FTP/1.1\r\n\r\n", &r) == HTTP_BAD, "not HTTP");
    CHECK(parse("GET  / HTTP/1.1\r\n\r\n", &r) == HTTP_BAD, "two spaces");
    CHECK(parse("GET index.html HTTP/1.1\r\n\r\n", &r) == HTTP_BAD, "target without a slash");
    CHECK(parse("GET http://x/ HTTP/1.1\r\n\r\n", &r) == HTTP_BAD, "absolute-form target");
    CHECK(parse("\r\n\r\n", &r) == HTTP_BAD, "empty request line");

    CHECK(parse("GET /a/../b HTTP/1.1\r\n\r\n", &r) == HTTP_BAD, "'..' segment");
    CHECK(parse("GET /.. HTTP/1.1\r\n\r\n", &r) == HTTP_BAD, "trailing '..'");
    CHECK(parse("GET /../etc HTTP/1.1\r\n\r\n", &r) == HTTP_BAD, "leading '..'");
    CHECK(parse("GET /%2e%2e/x HTTP/1.1\r\n\r\n", &r) == HTTP_BAD, "encoded '..'");
    CHECK(parse("GET /%2E./x HTTP/1.1\r\n\r\n", &r) == HTTP_BAD, "half-encoded '..'");
    CHECK(parse("GET /a\\b HTTP/1.1\r\n\r\n", &r) == HTTP_BAD, "backslash");

    CHECK(parse("GET /a\x01 HTTP/1.1\r\n\r\n", &r) == HTTP_BAD, "control byte in target");
    CHECK(parse("GET / HTTP/1.1\r\nX: a\x7f\r\n\r\n", &r) == HTTP_BAD, "DEL in a header");
    CHECK(parse("GET / HTTP/1.1\r\nX: a\x01\r\n\r\n", &r) == HTTP_BAD, "control byte in a header");
    CHECK(parse("GET /a HTTP/1.1\nHost: x\n\n", &r) == HTTP_INCOMPLETE,
          "bare LF is not a line ending, so the request never finishes");

    CHECK(http_parse_request("GET /a\0b HTTP/1.1\r\n\r\n", 21, &r) == HTTP_BAD,
          "NUL in the target");
    CHECK(http_parse_request("GET / HTTP/1.1\r\nX: \0\r\n\r\n", 24, &r) == HTTP_BAD,
          "NUL in a header");
}

static void test_size_caps(void)
{
    static char big[HTTP_MAX_REQUEST + 64];
    http_request_t r;

    memset(big, 'a', sizeof big);
    memcpy(big, "GET / HTTP/1.1\r\nX: ", 19);
    CHECK(http_parse_request(big, HTTP_MAX_REQUEST - 1, &r) == HTTP_INCOMPLETE,
          "just under the cap, no end yet");
    CHECK(http_parse_request(big, HTTP_MAX_REQUEST, &r) == HTTP_TOO_LARGE,
          "at the cap with no end of headers");
    CHECK(http_parse_request(big, sizeof big, &r) == HTTP_TOO_LARGE, "far over the cap");

    char path[HTTP_PATH_MAX + 32];
    int n = snprintf(path, sizeof path, "GET /");
    for (size_t i = 0; i < HTTP_PATH_MAX; i++) path[n++] = 'p';
    n += snprintf(path + n, sizeof path - (size_t)n, " HTTP/1.1\r\n\r\n");
    CHECK(http_parse_request(path, (size_t)n, &r) == HTTP_BAD, "path over HTTP_PATH_MAX");

    char q[HTTP_QUERY_MAX + 64];
    n = snprintf(q, sizeof q, "GET /a?");
    for (size_t i = 0; i < HTTP_QUERY_MAX; i++) q[n++] = 'q';
    n += snprintf(q + n, sizeof q - (size_t)n, " HTTP/1.1\r\n\r\n");
    CHECK(http_parse_request(q, (size_t)n, &r) == HTTP_BAD, "query over HTTP_QUERY_MAX");
}

/* HEAD is refused on purpose: nothing we serve is asked for with it, and "GET only" is a
 * rule that fits in one sentence. A 405 must say what is allowed (RFC 9110 15.5.6). */
static void test_head_is_rejected(void)
{
    http_request_t r;
    char out[256];

    CHECK(parse("HEAD / HTTP/1.1\r\n\r\n", &r) == HTTP_BAD_METHOD, "HEAD /");
    CHECK(parse("HEAD /api/status HTTP/1.1\r\nHost: box\r\n\r\n", &r) == HTTP_BAD_METHOD,
          "HEAD on an API route");
    CHECK(parse("head / HTTP/1.1\r\n\r\n", &r) == HTTP_BAD_METHOD, "lowercase head");
    CHECK(parse("GETS / HTTP/1.1\r\n\r\n", &r) == HTTP_BAD_METHOD, "GET plus a letter");
    CHECK(parse("GE / HTTP/1.1\r\n\r\n", &r) == HTTP_BAD_METHOD, "GET minus a letter");

    ssize_t n = http_write_head(out, sizeof out, (int)HTTP_BAD_METHOD, "text/plain", 0);
    CHECK(n > 0 && strstr(out, "\r\nAllow: GET\r\n") != NULL, "a 405 carries Allow: GET");

    n = http_write_head(out, sizeof out, HTTP_STATUS_OK, "text/plain", 0);
    CHECK(n > 0 && strstr(out, "Allow:") == NULL, "only a 405 carries Allow");
}

/* The enum values are the statuses, so a refusal can go straight into a response head. */
static void test_refusals_are_statuses(void)
{
    char out[256];
    static const http_parse_t refusals[] = { HTTP_BAD, HTTP_BAD_METHOD, HTTP_TOO_LARGE };

    CHECK(HTTP_BAD == 400 && HTTP_BAD_METHOD == 405 && HTTP_TOO_LARGE == 431,
          "refusals are their HTTP statuses");
    CHECK((int)HTTP_OK != (int)HTTP_STATUS_OK && (int)HTTP_OK < 400, "OK is not a status");
    CHECK(HTTP_INCOMPLETE < 0, "INCOMPLETE is not a status");
    for (size_t i = 0; i < sizeof refusals / sizeof refusals[0]; i++)
        CHECK(http_write_head(out, sizeof out, (int)refusals[i], "text/plain", 0) > 0,
              "a refusal is a status the writer knows");
}

static void test_query_get(void)
{
    char v[16];

    CHECK(http_query_get("pid=0C&from=-2h", "pid", v, sizeof v) == 0 && strcmp(v, "0C") == 0,
          "first key");
    CHECK(http_query_get("pid=0C&from=-2h", "from", v, sizeof v) == 0 &&
          strcmp(v, "-2h") == 0, "second key");
    CHECK(http_query_get("pid=0C", "to", v, sizeof v) == -1, "absent key");
    CHECK(http_query_get("pids=1", "pid", v, sizeof v) == -1, "key must match whole name");
    CHECK(http_query_get("pid=1", "pi", v, sizeof v) == -1, "prefix of a key is not the key");
    CHECK(http_query_get("a=&b=2", "a", v, sizeof v) == 0 && v[0] == '\0', "empty value");
    CHECK(http_query_get("t=10%3A30%2B01", "t", v, sizeof v) == 0 &&
          strcmp(v, "10:30+01") == 0, "percent-decoding");
    CHECK(http_query_get("t=a+b", "t", v, sizeof v) == 0 && strcmp(v, "a b") == 0,
          "'+' is a space");
    CHECK(http_query_get("t=%4", "t", v, sizeof v) == -1, "truncated escape");
    CHECK(http_query_get("t=%zz", "t", v, sizeof v) == -1, "non-hex escape");
    CHECK(http_query_get("t=%00", "t", v, sizeof v) == -1, "an encoded NUL is refused");
    CHECK(http_query_get("t=0123456789abcdef", "t", v, sizeof v) == -1,
          "value of 16 bytes does not fit in 16 with its NUL");
    CHECK(http_query_get("t=0123456789abcde", "t", v, sizeof v) == 0, "value that just fits");
    CHECK(http_query_get("", "t", v, sizeof v) == -1, "empty query");
}

static void test_write_head(void)
{
    char out[256];

    ssize_t n = http_write_head(out, sizeof out, HTTP_STATUS_OK, "text/html; charset=utf-8",
                                1234);
    const char *want =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/html; charset=utf-8\r\n"
        "Content-Length: 1234\r\n"
        "Connection: close\r\n"
        "Cache-Control: no-store\r\n"
        "X-Content-Type-Options: nosniff\r\n"
        "\r\n";
    CHECK(n == (ssize_t)strlen(want), "length of the head");
    CHECK(n > 0 && memcmp(out, want, (size_t)n) == 0, "exact bytes of a 200 head");

    n = http_write_head(out, sizeof out, HTTP_STATUS_NOT_FOUND, "text/plain", 0);
    CHECK(n > 0 && strncmp(out, "HTTP/1.1 404 Not Found\r\n", 24) == 0, "404 status line");

    static const http_status_t known[] = {
        HTTP_STATUS_OK, HTTP_STATUS_BAD_REQUEST, HTTP_STATUS_NOT_FOUND,
        HTTP_STATUS_METHOD_NOT_ALLOWED, HTTP_STATUS_HEADERS_TOO_LARGE,
        HTTP_STATUS_INTERNAL_ERROR
    };
    for (size_t i = 0; i < sizeof known / sizeof known[0]; i++)
        CHECK(http_write_head(out, sizeof out, (int)known[i], "text/plain", 0) > 0,
              "known status");
    CHECK(http_write_head(out, sizeof out, 418, "text/plain", 0) == -1, "unknown status");

    /* The head is NUL-terminated, so it needs its length plus one. Short of that it must
     * fail, never truncate. */
    n = http_write_head(out, sizeof out, HTTP_STATUS_OK, "text/plain", 5);
    CHECK(n > 0 && http_write_head(out, (size_t)n, HTTP_STATUS_OK, "text/plain", 5) == -1,
          "no room for the terminating NUL");
    CHECK(http_write_head(out, (size_t)n + 1, HTTP_STATUS_OK, "text/plain", 5) == n,
          "buffer exactly big enough");
    CHECK(http_write_head(out, 0, HTTP_STATUS_OK, "text/plain", 5) == -1, "zero-size buffer");
}

int main(void)
{
    log_init("test_http", LOG_FATAL);
    test_good_requests();
    test_every_split_is_incomplete_until_whole();
    test_refusals();
    test_size_caps();
    test_head_is_rejected();
    test_refusals_are_statuses();
    test_query_get();
    test_write_head();
    return failures == 0 ? 0 : 1;
}
