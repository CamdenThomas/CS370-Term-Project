/* statusd's routes without a socket: a URL goes in, a status and a body come out. A scripted
 * fake source shows what request line was built (and that hostile input never reaches it),
 * and the synthetic source shows the whole path end to end. */
#include "interface/route.h"
#include "interface/synth_source.h"
#include "common/log.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures;

#define CHECK(cond, msg) do { \
    if (!(cond)) { fprintf(stderr, "  FAIL %s:%d: %s\n", __FILE__, __LINE__, msg); \
                   failures++; } } while (0)

#define NOW 1800000000ULL

/* ---------------------------------------------------------------- fake source */

typedef struct {
    const char *reply;    /* what the source answers with */
    ssize_t     force;    /* if not 0, returned instead of a reply */
    char        last[256];
    int         calls;
} fake_t;

static fake_t fake;

static ssize_t fake_query(void *ctx, const char *request, char *out, size_t cap)
{
    fake_t *f = ctx;
    size_t n = strlen(f->reply);

    f->calls++;
    snprintf(f->last, sizeof f->last, "%s", request);
    if (f->force != 0) return f->force;
    if (n + 1 > cap) return -1;
    memcpy(out, f->reply, n + 1);
    return (ssize_t)n;
}

static source_t fake_src = { fake_query, &fake };
static synth_t  syn;
static source_t synth_src;
static route_ctx_t rc;

static void use_fake(const char *reply)
{
    memset(&fake, 0, sizeof fake);
    fake.reply = reply;
    rc.src = &fake_src;
}

static void use_synth(void)
{
    synth_init(&syn, NOW);
    synth_src = synth_source(&syn);
    rc.src = &synth_src;
}

/* ------------------------------------------------------------------- the call */

#define CAP_MAX  SERVER_BODY_MAX
static char area[CAP_MAX + 16];
static char text[CAP_MAX + 1];
static server_response_t resp;

/* Routes GET target with a response buffer of cap bytes; the body lands in text. */
static int call_cap(const char *target, size_t cap)
{
    char raw[600];
    http_request_t req;

    snprintf(raw, sizeof raw, "GET %s HTTP/1.1\r\n\r\n", target);
    http_parse_t p = http_parse_request(raw, strlen(raw), &req);
    CHECK(p == HTTP_OK, target);   /* a test URL must get past the HTTP layer first */
    if (p != HTTP_OK) return -1;

    memset(area, 0xAA, sizeof area);
    resp = (server_response_t){ HTTP_STATUS_INTERNAL_ERROR, "text/plain", area, cap, 0 };
    route_handle(&rc, &req, &resp);

    for (size_t i = cap; i < sizeof area; i++)
        if ((unsigned char)area[i] != 0xAA) { CHECK(false, "wrote past the response buffer"); break; }
    CHECK(resp.body_len <= cap, "the length fits the buffer");
    if (resp.body_len > cap) resp.body_len = cap;
    memcpy(text, area, resp.body_len);
    text[resp.body_len] = '\0';
    return resp.status;
}

static int call(const char *target) { return call_cap(target, CAP_MAX); }

static bool starts(const char *s, const char *prefix) { return strncmp(s, prefix, strlen(prefix)) == 0; }
static bool has(const char *needle) { return strstr(text, needle) != NULL; }

static size_t count(const char *hay, const char *needle)
{
    size_t n = 0;
    for (const char *p = hay; (p = strstr(p, needle)) != NULL; p += strlen(needle)) n++;
    return n;
}

static bool json_error(const char *code)
{
    char want[96];
    snprintf(want, sizeof want, "{\"ok\":false,\"code\":\"%s\",\"reason\":\"", code);
    return starts(text, want);
}

/* -------------------------------------------------------------------- tests */

static void test_paths(void)
{
    use_fake("ok\n");

    CHECK(call("/") == 200 && strcmp(resp.ctype, "text/html; charset=utf-8") == 0 &&
          has("statusd"), "the placeholder page");
    CHECK(call("/?x=1") == 400, "the page takes no query");
    CHECK(call("/nope") == 404 && strcmp(resp.ctype, "text/plain") == 0 &&
          strcmp(text, "not found\n") == 0, "an unknown path is a text 404");
    CHECK(call("/API/status") == 404 && strcmp(text, "not found\n") == 0, "paths are case-sensitive");
    CHECK(call("/api/nope") == 404 && json_error("not_found") &&
          strcmp(resp.ctype, "application/json") == 0, "an unknown /api/ path is a JSON 404");
    CHECK(call("/api/status/") == 404 && json_error("not_found"), "a trailing slash is another path");
    CHECK(call("/api/") == 404 && json_error("not_found"), "the bare prefix");
    CHECK(fake.calls == 0, "none of them reached the source");
}

static void test_end_to_end(void)
{
    static const char *routes[] = { "/api/status", "/api/verdicts", "/api/faults",
                                    "/api/series?pid=0C", "/api/series?pid=probe&from=-1h" };
    static const char *verbs[] = { "status", "verdicts", "faults", "series pid=\"0C\"",
                                   "series pid=\"probe\" from=\"-1h\"" };
    static char reply[ROUTE_REPLY_MAX];

    use_synth();
    synth_set_time(&syn, NOW);
    for (size_t i = 0; i < sizeof routes / sizeof routes[0]; i++) {
        CHECK(call(routes[i]) == 200, routes[i]);
        CHECK(strcmp(resp.ctype, "application/json") == 0, "JSON");
        CHECK(starts(text, "{\"ok\":true,\"records\":[") && strcmp(text + strlen(text) - 2, "]}") == 0,
              "the envelope");
        CHECK(count(text, "{") == count(text, "}") && count(text, "[") == count(text, "]"),
              "braces balance");

        ssize_t n = source_query(&synth_src, verbs[i], reply, sizeof reply);
        size_t records = 0;
        for (const char *p = reply; (p = strchr(p, '\n')) != NULL; p++) records++;
        records--;   /* the terminator */
        CHECK(n > 0 && count(text, "{\"tag\":") == records, "one object per record");
        CHECK(count(text, "\"src\":\"synth\"") == records, "and every one says synth");
    }

    call("/api/status");
    CHECK(has("\"tag\":\"proc\",\"name\":\"obdd\",\"state\":\"up\",\"pid\":102"),
          "strings quoted, numbers not");
    CHECK(has("\"restarts\":2") && has("\"time_trusted\":1"), "numbers");
    call("/api/verdicts");
    CHECK(has("\"end_s\":null") && has("\"sensors\":\"coolant,intake\""), "null, and a list string");
    call("/api/series?pid=0C");
    CHECK(has("\"pid\":\"0C\"") && has("\"t_s\":18") && !has("\"t_s\":\""), "a PID is a string, a time a number");
    call("/api/series?pid=probe&from=-1h");
    CHECK(has("\"value\":null") && has("\"value\":2"), "the probe drops out, then answers");
}

static void test_conversion(void)
{
    use_fake("rec a=\"x\" n=1.5 z=null\nok\n");
    CHECK(call("/api/status") == 200, "ok");
    CHECK(strcmp(text, "{\"ok\":true,\"records\":[{\"tag\":\"rec\",\"a\":\"x\",\"n\":1.5,\"z\":null}]}") == 0,
          "each field keeps the type the source gave it");

    use_fake("ok\n");
    CHECK(call("/api/status") == 200 && strcmp(text, "{\"ok\":true,\"records\":[]}") == 0, "no records");

    use_fake("a n=1\nb n=2\nidle\nok\n");
    call("/api/status");
    CHECK(strcmp(text, "{\"ok\":true,\"records\":[{\"tag\":\"a\",\"n\":1},{\"tag\":\"b\",\"n\":2},"
                       "{\"tag\":\"idle\"}]}") == 0, "records in order, one with no fields");

    use_fake("a pid=\"46\" v=46\nok\n");
    call("/api/status");
    CHECK(has("\"pid\":\"46\",\"v\":46"), "a string that looks like a number stays a string");
}

static void test_error_mapping(void)
{
    static const struct { const char *reply; int status; const char *body; } cases[] = {
        { "err usage bad window\n", 400,
          "{\"ok\":false,\"code\":\"usage\",\"reason\":\"bad window\"}" },
        { "err unknown_pid no such pid\n", 404,
          "{\"ok\":false,\"code\":\"unknown_pid\",\"reason\":\"no such pid\"}" },
        { "err unknown_verb no such verb\n", 404,
          "{\"ok\":false,\"code\":\"unknown_verb\",\"reason\":\"no such verb\"}" },
        { "err unavailable storaged is down\n", 503,
          "{\"ok\":false,\"code\":\"unavailable\",\"reason\":\"storaged is down\"}" },
        { "err weird something odd\n", 500,
          "{\"ok\":false,\"code\":\"weird\",\"reason\":\"something odd\"}" },
        { "err usage\n", 400, "{\"ok\":false,\"code\":\"usage\",\"reason\":\"\"}" },
        { "rec a=1\nerr unavailable gone\n", 503,
          "{\"ok\":false,\"code\":\"unavailable\",\"reason\":\"gone\"}" },
    };
    for (size_t i = 0; i < sizeof cases / sizeof cases[0]; i++) {
        use_fake(cases[i].reply);
        CHECK(call("/api/status") == cases[i].status, cases[i].reply);
        CHECK(strcmp(text, cases[i].body) == 0, "the error body");
        CHECK(strcmp(resp.ctype, "application/json") == 0, "JSON");
    }

    static const char *malformed[] = { "rec a=1\nbogus line\nok\n", "rec a=1\n", "", "ok\nrec a=1\n",
                                       "rec a=1\nok", "rec a=hello\nok\n" };
    for (size_t i = 0; i < sizeof malformed / sizeof malformed[0]; i++) {
        use_fake(malformed[i]);
        CHECK(call("/api/status") == 500 && json_error("internal"), malformed[i]);
    }

    use_fake("ok\n");
    fake.force = -1;
    CHECK(call("/api/status") == 500 && json_error("internal"), "a reply that did not fit");

    use_fake("rec tag=\"x\" a=1\nok\n");
    CHECK(call("/api/status") == 500 && json_error("internal"), "a field called tag collides with the record's");
}

static void test_request_lines(void)
{
    static const struct { const char *target, *line; } cases[] = {
        { "/api/status", "status" },
        { "/api/verdicts", "verdicts" },
        { "/api/faults", "faults" },
        { "/api/series", "series" },
        { "/api/series?pid=0C&from=-5m&to=now", "series pid=\"0C\" from=\"-5m\" to=\"now\"" },
        { "/api/series?to=now&from=-5m&pid=0C", "series pid=\"0C\" from=\"-5m\" to=\"now\"" },
        { "/api/series?pid=0C&from=1799999700&to=1800000000",
          "series pid=\"0C\" from=1799999700 to=1800000000" },
        { "/api/series?pid=04", "series pid=\"04\"" },
        { "/api/series?pid=46", "series pid=\"46\"" },
        { "/api/series?pid=probe&from=-1h", "series pid=\"probe\" from=\"-1h\"" },
        { "/api/series?from=10%3A30&pid=0C", "series pid=\"0C\" from=\"10:30\"" },
        { "/api/series?pid=0%43", "series pid=\"0C\"" },
        { "/api/series?pid=a%2Bb", "series pid=\"a+b\"" },
        { "/api/series?pid=aaaaaaaaaaaaaaaaaaaaaaaa", "series pid=\"aaaaaaaaaaaaaaaaaaaaaaaa\"" },
    };
    for (size_t i = 0; i < sizeof cases / sizeof cases[0]; i++) {
        use_fake("ok\n");
        CHECK(call(cases[i].target) == 200, cases[i].target);
        CHECK(fake.calls == 1 && strcmp(fake.last, cases[i].line) == 0, cases[i].line);
        if (strcmp(fake.last, cases[i].line) != 0) fprintf(stderr, "  got: %s\n", fake.last);
    }
}

static void test_hostile_input_never_reaches_the_source(void)
{
    static const char *bad[] = {
        "/api/series?pid=0C%22", "/api/series?pid=0C\"x", "/api/series?pid=a%20b",
        "/api/series?pid=a+b", "/api/series?pid=%0A", "/api/series?pid=%09", "/api/series?pid=%00",
        "/api/series?pid=a%5Cb", "/api/series?pid=%FF", "/api/series?pid=",
        "/api/series?pid", "/api/series?pid=0C%22%20from%3D%221%22",
        "/api/series?pid=%3Cscript%3E", "/api/series?pid=a;b", "/api/series?pid=a%3Db",
        "/api/series?pid=%7B", "/api/series?pid=0C&to=a%26b", "/api/series?pid=aaaaaaaaaaaaaaaaaaaaaaaaa",
        "/api/series?pid=%zz", "/api/series?pid=%4", "/api/series?pid=0C&from=%zz",
        "/api/series?foo=1", "/api/series?pid=1&pid=2", "/api/series?PID=1", "/api/series?pid=1&",
        "/api/series?&pid=1", "/api/series?pid=1&&to=2", "/api/series?=1", "/api/series?pi=1",
        "/api/series?pids=1", "/api/series?pid=1&from=1&from=2", "/api/series?pid=1&to=1&to=2",
        "/api/status?x=1", "/api/verdicts?a", "/api/faults?since=-1h",
    };
    for (size_t i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        use_fake("ok\n");
        CHECK(call(bad[i]) == 400 && json_error("usage"), bad[i]);
        CHECK(fake.calls == 0, "and the source was never asked");
    }

    use_fake("ok\n");
    CHECK(call("/api/status?") == 200 && fake.calls == 1, "an empty query is no query");
}

static void test_capacity(void)
{
    static char big[ROUTE_REPLY_MAX];
    size_t len = 0;

    /* A reply that fits the scratch buffer but whose JSON does not fit the response. */
    for (int i = 0; i < 2000 && len + 40 < sizeof big; i++)
        len += (size_t)snprintf(big + len, sizeof big - len, "r a=%d b=2 c=\"xxxxxxxx\"\n", i % 1000);
    snprintf(big + len, sizeof big - len, "ok\n");
    use_fake(big);
    CHECK(call("/api/status") == 500 && json_error("internal"), "a response too big for the body");

    use_fake("a n=1\nb n=2\nc n=3\nok\n");
    CHECK(call_cap("/api/status", 20) == 500, "a 20-byte body cannot hold the records");
    CHECK(call_cap("/api/status", 200) == 200, "200 bytes can");
    use_fake("err usage a long enough reason to overflow a tiny response buffer\n");
    CHECK(call_cap("/api/status", 30) == 500 && resp.body_len == 0, "even the error does not fit");
    CHECK(call_cap("/api/nope", 10) == 500, "nor the 404");
    CHECK(call_cap("/", 10) == 500, "nor the page");
}

int main(void)
{
    log_init("test_route", LOG_FATAL);
    test_paths();
    test_end_to_end();
    test_conversion();
    test_error_mapping();
    test_request_lines();
    test_hostile_input_never_reaches_the_source();
    test_capacity();
    return failures == 0 ? 0 : 1;
}
