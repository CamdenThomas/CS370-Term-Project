/* The synthetic source: every reply is well-formed and labeled synth, the numbers repeat
 * exactly for a given time, and every refusal and size limit is checked. */
#include "interface/synth_source.h"
#include "interface/kv.h"
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
#define OUT_SIZE 65536u

static synth_t  syn;
static source_t src;
static char     reply[OUT_SIZE];
static kv_line_t recs[400];

static void setup(uint64_t now)
{
    synth_init(&syn, NOW);
    synth_set_time(&syn, now);
    src = synth_source(&syn);
}

static ssize_t ask(const char *request)
{
    return source_query(&src, request, reply, sizeof reply);
}

static const kv_field_t *field(const kv_line_t *l, const char *key)
{
    for (size_t i = 0; i < l->nfields; i++)
        if (strcmp(l->f[i].key, key) == 0) return &l->f[i];
    return NULL;
}

static unsigned long long num(const kv_line_t *l, const char *key)
{
    const kv_field_t *f = field(l, key);
    CHECK(f != NULL && f->type == KV_NUM, key);
    return f != NULL ? strtoull(f->val, NULL, 10) : 0;
}

static bool is(const kv_line_t *l, const char *key, const char *want)
{
    const kv_field_t *f = field(l, key);
    return f != NULL && f->type == KV_STR && strcmp(f->val, want) == 0;
}

/* Collects the records tagged `tag` (all of them if tag is NULL) and checks that EVERY
 * record in the reply is labeled src="synth". Returns how many were collected. */
static size_t scan(size_t n, const char *tag)
{
    const char *cur = reply, *end = reply + n;
    kv_line_t l;
    size_t count = 0;

    while (kv_reply_next(&cur, end, &l) == KV_RECORD) {
        CHECK(is(&l, "src", "synth"), "every record is labeled src=\"synth\"");
        if (tag == NULL || strcmp(l.tag, tag) == 0) {
            if (count < sizeof recs / sizeof recs[0]) recs[count] = l;
            count++;
        }
    }
    return count;
}

/* The reply is refused with this code. */
static void refused(const char *request, const char *code)
{
    ssize_t n = ask(request);
    kv_line_t f;
    CHECK(n > 0 && kv_reply_check(reply, (size_t)n, &f) == KV_ERR &&
          strcmp(f.err_code, code) == 0, request);
}

static void test_every_reply_is_valid_and_labeled(void)
{
    static const char *requests[] = {
        "status", "verdicts", "faults",
        "series pid=\"0C\"",
        "series pid=\"probe\" from=\"-1h\" to=\"now\"",
        "series pid=\"05\" from=1799999700 to=1800000000",
    };

    setup(NOW);
    for (size_t i = 0; i < sizeof requests / sizeof requests[0]; i++) {
        ssize_t n = ask(requests[i]);
        CHECK(n > 0 && (size_t)n == strlen(reply), "the length returned is the reply's length");
        CHECK(n > 0 && kv_reply_check(reply, (size_t)n, NULL) == KV_OK, requests[i]);
        CHECK(n > 0 && scan((size_t)n, NULL) > 0, "and it has records");
    }
}

static void test_status(void)
{
    static const char *names[] = { "supervisor", "obdd", "ambientd", "storaged", "analyzed",
                                   "statusd" };
    setup(NOW + 100);
    ssize_t n = ask("status");

    CHECK(scan((size_t)n, "proc") == 6, "six processes");
    for (size_t i = 0; i < 6; i++) {
        CHECK(is(&recs[i], "name", names[i]), names[i]);
        CHECK(is(&recs[i], "state", "up"), "up");
        CHECK(num(&recs[i], "rss_kb") > 0 && num(&recs[i], "pid") > 0, "numbers are numbers");
        CHECK(num(&recs[i], "uptime_s") >= 3600 + 100 || strcmp(names[i], "analyzed") == 0,
              "uptime counts from an hour before the start");
        CHECK(field(&recs[i], "cpu_pct") != NULL && field(&recs[i], "events") != NULL &&
              field(&recs[i], "drops") != NULL, "cpu, events and drops");
    }
    CHECK(num(&recs[4], "restarts") == 2, "analyzed has restarted twice");
    CHECK(num(&recs[1], "restarts") == 0, "obdd has not");

    CHECK(scan((size_t)n, "ring") == 2, "two rings");
    CHECK(is(&recs[0], "name", "obd") && is(&recs[1], "name", "ambient"), "ring names");
    CHECK(num(&recs[0], "depth") <= num(&recs[0], "capacity") && num(&recs[0], "overruns") == 0,
          "ring depth within capacity, no overruns");

    CHECK(scan((size_t)n, "link") == 1, "one link record");
    CHECK(is(&recs[0], "dongle", "up") && is(&recs[0], "probe", "present") &&
          is(&recs[0], "light", "green") && num(&recs[0], "time_trusted") == 1, "link fields");

    CHECK(scan((size_t)n, "srcmix") == 1, "one source mix");
    CHECK(num(&recs[0], "live") == 0 && num(&recs[0], "replay") == 0 &&
          num(&recs[0], "synth") > 0, "all of it synth");
}

static void test_verdicts(void)
{
    setup(NOW);
    ssize_t n = ask("verdicts");

    CHECK(scan((size_t)n, "verdict") == 2, "two verdicts");
    CHECK(is(&recs[0], "state", "active") && is(&recs[0], "area", "cooling"), "the active one");
    CHECK(field(&recs[0], "end_s") != NULL && field(&recs[0], "end_s")->type == KV_NULL,
          "an active verdict has no end");
    CHECK(is(&recs[1], "state", "ended") && is(&recs[1], "area", "air_fuel"), "the ended one");
    CHECK(num(&recs[1], "end_s") >= num(&recs[1], "start_s"), "it ended after it began");
    for (size_t i = 0; i < 2; i++) {
        const kv_field_t *s = field(&recs[i], "sensors");
        CHECK(s != NULL && s->type == KV_STR && strchr(s->val, ',') != NULL,
              "a verdict names two or more cooperating sensors (D.4)");
        CHECK(field(&recs[i], "residual") != NULL && field(&recs[i], "residual")->type == KV_NUM,
              "and a residual");
    }
}

static void test_faults(void)
{
    setup(NOW);
    ssize_t n = ask("faults");
    size_t count = scan((size_t)n, "fault");

    CHECK(count >= 3, "at least three events");
    unsigned long long last = 0;
    for (size_t i = 0; i < count; i++) {
        CHECK(is(&recs[i], "kind", "detection") || is(&recs[i], "kind", "degradation") ||
              is(&recs[i], "kind", "recovery"), "a kind D-042 names");
        CHECK(field(&recs[i], "code") != NULL && field(&recs[i], "detail") != NULL, "code, detail");
        CHECK(num(&recs[i], "t_s") >= last, "oldest first");
        last = num(&recs[i], "t_s");
    }
}

static void test_series(void)
{
    setup(NOW);
    ssize_t n = ask("series pid=\"0C\" from=\"-5m\" to=\"now\"");
    size_t count = scan((size_t)n, "sample");

    CHECK(count >= 1 && count <= 300, "between 1 and 300 samples");
    unsigned long long prev = 0;
    for (size_t i = 0; i < count; i++) {
        unsigned long long t = num(&recs[i], "t_s");
        CHECK(t >= NOW - 300 && t <= NOW, "inside the window");
        CHECK(i == 0 || t > prev, "strictly increasing");
        CHECK(is(&recs[i], "pid", "0C") && field(&recs[i], "value")->type == KV_NUM, "a number");
        prev = t;
    }

    /* The same window, as epoch seconds, is the same reply. */
    char first[OUT_SIZE];
    memcpy(first, reply, (size_t)n + 1);
    ssize_t m = ask("series pid=\"0C\" from=1799999700 to=1800000000");
    CHECK(m == n && memcmp(first, reply, (size_t)n) == 0, "epoch and relative windows agree");

    n = ask("series pid=\"0C\"");
    CHECK(n > 0 && scan((size_t)n, "sample") >= 1, "from and to default to the last 5 minutes");

    n = ask("series pid=\"0D\" from=\"-1h\" to=\"now\"");
    count = scan((size_t)n, "sample");
    CHECK(n > 0 && count >= 100 && count <= 300, "an hour is thinned to at most 300 samples");

    n = ask("series pid=\"probe\" from=\"-1h\" to=\"now\"");
    count = scan((size_t)n, "sample");
    size_t nulls = 0, nums = 0;
    for (size_t i = 0; i < count; i++) {
        const kv_field_t *v = field(&recs[i], "value");
        if (v->type == KV_NULL) nulls++; else if (v->type == KV_NUM) nums++;
    }
    CHECK(nulls >= 1 && nums >= 1, "the probe drops out for a while, then answers");

    static const char *pids[] = { "0C", "0D", "05", "04", "06", "07", "probe" };
    for (size_t i = 0; i < sizeof pids / sizeof pids[0]; i++) {
        char req[64];
        snprintf(req, sizeof req, "series pid=\"%s\"", pids[i]);
        n = ask(req);
        CHECK(n > 0 && scan((size_t)n, "sample") >= 1, pids[i]);
    }
}

static void test_refusals(void)
{
    setup(NOW);
    refused("series", "usage");
    refused("series pid=0C", "usage");                          /* unquoted: not the grammar */
    refused("series pid=\"ZZ\"", "unknown_pid");
    refused("series pid=\"0c\"", "unknown_pid");
    refused("series pid=5", "usage");                           /* a number is not a PID */
    refused("series pid=\"0C\" from=\"junk\"", "usage");
    refused("series pid=\"0C\" from=\"-5\"", "usage");
    refused("series pid=\"0C\" from=\"-5x\"", "usage");
    refused("series pid=\"0C\" from=\"-1x\"", "usage");                /* only s, m and h */
    refused("series pid=\"0C\" from=\"-1d\"", "usage");
    refused("series pid=\"0C\" from=-5", "usage");
    refused("series pid=\"0C\" from=1.5", "usage");
    refused("series pid=\"0C\" from=\"-5m\" to=\"-10m\"", "usage");   /* backwards */
    refused("series pid=\"0C\" from=\"now\" to=\"now\"", "usage");     /* empty */
    refused("series pid=\"0C\" from=\"-2h\"", "usage");                /* over an hour */
    refused("series pid=\"0C\" from=\"-100000s\"", "usage");
    refused("reboot", "unknown_verb");
    refused("baseline pid=\"0C\"", "unavailable");
    refused("", "usage");
    refused("ok", "usage");
    refused("err a b", "usage");
    refused("status extra", "usage");
    refused("STATUS", "usage");

    /* A clock that has not been running five minutes cannot look five minutes back. */
    synth_init(&syn, 100);
    refused("series pid=\"0C\"", "usage");
    refused("series pid=\"0C\" from=\"-101s\" to=\"now\"", "usage");
    ssize_t n = ask("series pid=\"0C\" from=\"-100s\" to=\"now\"");
    CHECK(n > 0 && scan((size_t)n, "sample") >= 1, "looking back exactly to zero is fine");
}

static void test_deterministic(void)
{
    static char a[OUT_SIZE], b[OUT_SIZE];
    static const char *requests[] = { "status", "verdicts", "faults",
                                      "series pid=\"0C\" from=\"-1h\"" };

    for (size_t i = 0; i < sizeof requests / sizeof requests[0]; i++) {
        setup(NOW + 5000);
        ssize_t n = ask(requests[i]);
        memcpy(a, reply, (size_t)n + 1);
        setup(NOW + 5000);
        ssize_t m = ask(requests[i]);
        memcpy(b, reply, (size_t)m + 1);
        CHECK(n == m && memcmp(a, b, (size_t)n) == 0, "the same time gives the same bytes");

        /* Not one second later: series samples sit on multiples of their step, so a window
         * that slid by less than a step can hold exactly the same samples. */
        setup(NOW + 5100);
        m = ask(requests[i]);
        CHECK(m != n || memcmp(a, reply, (size_t)n) != 0, "a later time gives a different reply");
    }
}

static void test_capacity(void)
{
    static char area[OUT_SIZE];
    setup(NOW);
    ssize_t n = ask("status");
    CHECK(n > 0, "a status reply");
    size_t len = (size_t)n;

    static const size_t caps[] = { 0, 1, 2, 50 };
    for (size_t i = 0; i <= sizeof caps / sizeof caps[0] + 1; i++) {
        size_t cap = i < sizeof caps / sizeof caps[0] ? caps[i] : (i == 4 ? len - 1 : len);
        memset(area, 0xAA, sizeof area);
        CHECK(source_query(&src, "status", area, cap) == -1, "a reply that does not fit is -1");
        for (size_t k = cap; k < sizeof area; k++)
            if ((unsigned char)area[k] != 0xAA) { CHECK(false, "wrote past cap"); break; }
    }

    memset(area, 0xAA, sizeof area);
    CHECK(source_query(&src, "status", area, len + 1) == (ssize_t)len, "length plus one fits");
    CHECK(strlen(area) == len && (unsigned char)area[len + 1] == 0xAA, "NUL-terminated, no more");

    /* A refusal that does not fit is -1 as well. */
    memset(area, 0xAA, sizeof area);
    CHECK(source_query(&src, "nonsense", area, 8) == -1, "a short err reply does not fit in 8");
    CHECK((unsigned char)area[8] == 0xAA, "and stays inside cap");
}

int main(void)
{
    log_init("test_synth_source", LOG_FATAL);
    test_every_reply_is_valid_and_labeled();
    test_status();
    test_verdicts();
    test_faults();
    test_series();
    test_refusals();
    test_deterministic();
    test_capacity();
    return failures == 0 ? 0 : 1;
}
