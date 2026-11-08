/* src/obd/elm.c against a scripted adapter on a real pty, as tests/tools/test_elm.py does
 * against obdsim: init and the banner, a reply that arrives after its timeout (and must
 * not be taken for the next one), a busy adapter stopped by a bare CR, a bus that is
 * asleep, the supported-PID walk, and a hangup. Real syscalls: poll, read, tcflush. */
#include "common/log.h"
#include "obd/elm.h"

#include <fcntl.h>
#include <poll.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static int failures;

#define CHECK(cond, msg) do { \
    if (!(cond)) { fprintf(stderr, "  FAIL %s:%d: %s\n", __FILE__, __LINE__, msg); \
                   failures++; } } while (0)

/* --- the fake adapter ------------------------------------------------------------------- */

typedef struct {
    const char *cmd;       /* the command, without its CR; "" is a bare CR */
    const char *reply;     /* written back in full, prompt included */
    int         delay_ms;  /* before the reply */
    bool        busy;      /* say nothing until a bare CR, then STOPPED */
} rule_t;

typedef struct {
    int            master;
    const rule_t  *rules;
    size_t         n;
    bool           ignore_cr;   /* a busy adapter that never stops */
    atomic_int     stop;
    pthread_t      thread;
} fake_t;

static void sleep_ms(int ms)
{
    struct timespec t = { ms / 1000, (long)(ms % 1000) * 1000000L };
    (void)nanosleep(&t, NULL);
}

static void say(int fd, const char *s)
{
    size_t len = strlen(s);
    while (len > 0) {
        ssize_t w = write(fd, s, len);
        if (w <= 0) return;
        s += w;
        len -= (size_t)w;
    }
}

static void *fake_main(void *arg)
{
    fake_t *f = arg;
    char cmd[64];
    size_t n = 0;
    bool busy = false;
    while (!atomic_load(&f->stop)) {
        struct pollfd p = { .fd = f->master, .events = POLLIN };
        if (poll(&p, 1, 20) <= 0 || !(p.revents & POLLIN)) continue;
        char c;
        if (read(f->master, &c, 1) != 1) return NULL;
        if (c != '\r') {
            if (n < sizeof cmd - 1) cmd[n++] = c;
            continue;
        }
        cmd[n] = '\0';
        n = 0;
        if (busy) {
            if (cmd[0] == '\0' && !f->ignore_cr) {
                say(f->master, "STOPPED\r\r>");
                busy = false;
            }
            continue;
        }
        const rule_t *r = NULL;
        for (size_t i = 0; i < f->n; i++)
            if (strcmp(f->rules[i].cmd, cmd) == 0) r = &f->rules[i];
        if (r == NULL) { say(f->master, "?\r\r>"); continue; }
        if (r->busy) { busy = true; continue; }
        if (r->delay_ms > 0) sleep_ms(r->delay_ms);
        say(f->master, r->reply);
    }
    return NULL;
}

/* Opens a pty, starts the fake on its master, and opens the slave as the adapter. */
static bool start(fake_t *f, elm_t *e, const rule_t *rules, size_t n)
{
    memset(f, 0, sizeof *f);
    f->rules = rules;
    f->n = n;
    f->master = posix_openpt(O_RDWR | O_NOCTTY);
    char name[128];
    if (f->master < 0 || grantpt(f->master) != 0 || unlockpt(f->master) != 0 ||
        ptsname_r(f->master, name, sizeof name) != 0) {
        fprintf(stderr, "  FAIL cannot make a pty\n");
        failures++;
        return false;
    }
    if (elm_open(e, name) != ELM_OK) {
        fprintf(stderr, "  FAIL elm_open(%s)\n", name);
        failures++;
        close(f->master);
        return false;
    }
    if (pthread_create(&f->thread, NULL, fake_main, f) != 0) {
        failures++;
        elm_close(e);
        close(f->master);
        return false;
    }
    return true;
}

static void stop(fake_t *f, elm_t *e)
{
    atomic_store(&f->stop, 1);
    pthread_join(f->thread, NULL);
    elm_close(e);
    if (f->master >= 0) close(f->master);
}

#define N(a) (sizeof (a) / sizeof (a)[0])

/* An adapter that answers the init sequence; 0100 is the last rule so a test can swap it. */
#define INIT_RULES(banner) \
    { "ATZ", "ATZ\r\r\r" banner "\r\r>", 0, false }, \
    { "ATE0", "ATE0\rOK\r\r>", 0, false }, \
    { "ATL0", "OK\r\r>", 0, false }, \
    { "ATS1", "OK\r\r>", 0, false }, \
    { "ATH0", "OK\r\r>", 0, false }, \
    { "ATSP0", "OK\r\r>", 0, false }

/* --- the cases -------------------------------------------------------------------------- */

static void test_init_and_query(void)
{
    static const rule_t rules[] = {
        INIT_RULES("ELM327 v1.5 (carwatch SIM)"),
        { "0100", "SEARCHING...\r41 00 BE 1F A8 13 \r\r>", 0, false },
        { "010C", "41 0C 1A F8 \r\r>", 0, false },
        { "ATDP", "AUTO, ISO 15765-4 (CAN 11/500)\r\r>", 0, false },
        { "ATRV", "13.7V\r\r>", 0, false },
        { "0902", "014\r0: 49 02 01 31 48 47\r1: 43 4D 38 32 36 33 33\r"
                  "2: 41 30 30 34 33 35 32\r\r>", 0, false },
    };
    fake_t f;
    elm_t e;
    if (!start(&f, &e, rules, N(rules))) return;

    CHECK(elm_init(&e) == ELM_OK, "init succeeds");
    CHECK(strcmp(e.banner, "ELM327 v1.5 (carwatch SIM)") == 0, "banner without the echo");
    CHECK(elm_is_sim(&e), "the simulator's banner says SIM");

    obd_answer_t a;
    CHECK(elm_query(&e, 0x01, 0x0C, &a) == ELM_OK && a.n == 1 && a.ecu[0].len == 2 &&
          a.ecu[0].data[0] == 0x1A && a.ecu[0].data[1] == 0xF8, "a query returns its data");
    CHECK(elm_query(&e, 0x01, 0x5C, &a) == ELM_ERROR, "an unknown command fails (?)");

    char info[64];
    CHECK(elm_info(&e, "ATDP", info, sizeof info) == ELM_OK &&
          strcmp(info, "AUTO, ISO 15765-4 (CAN 11/500)") == 0, "protocol");
    CHECK(elm_info(&e, "ATRV", info, sizeof info) == ELM_OK && strcmp(info, "13.7V") == 0,
          "voltage");
    char vin[OBD_VIN_MAX + 1];
    CHECK(elm_vin(&e, vin) == ELM_OK && strcmp(vin, "1HGCM82633A004352") == 0, "VIN");
    stop(&f, &e);
}

static void test_real_banner_is_not_sim(void)
{
    static const rule_t rules[] = {
        INIT_RULES("ELM327 v1.4b"),
        { "0100", "41 00 BE 1F A8 13 \r\r>", 0, false },
    };
    fake_t f;
    elm_t e;
    if (!start(&f, &e, rules, N(rules))) return;
    CHECK(elm_init(&e) == ELM_OK && !elm_is_sim(&e), "a real adapter is live, not synth");
    stop(&f, &e);
}

static void test_bus_asleep(void)
{
    static const rule_t rules[] = {
        INIT_RULES("ELM327 v1.4b"),
        { "0100", "SEARCHING...\rUNABLE TO CONNECT\r\r>", 0, false },
    };
    fake_t f;
    elm_t e;
    if (!start(&f, &e, rules, N(rules))) return;
    CHECK(elm_init(&e) == ELM_ASLEEP, "ignition off: the bus is asleep, not an error");
    stop(&f, &e);
}

static void test_late_reply_is_not_taken_for_the_next(void)
{
    static const rule_t rules[] = {
        { "010D", "41 0D 3C \r\r>", 600, false },   /* after the 300 ms timeout */
        { "010C", "41 0C 1A F8 \r\r>", 0, false },
    };
    fake_t f;
    elm_t e;
    if (!start(&f, &e, rules, N(rules))) return;
    e.timeout_ms = 300;

    obd_answer_t a;
    CHECK(elm_query(&e, 0x01, 0x0D, &a) == ELM_TIMEOUT && e.stale, "times out, stale");
    CHECK(elm_query(&e, 0x01, 0x0C, &a) == ELM_OK && a.n == 1 && a.ecu[0].data[0] == 0x1A,
          "the next request gets its own reply, not the late one");
    CHECK(!e.stale, "back in step after the late prompt");
    stop(&f, &e);
}

static void test_busy_adapter_is_stopped_by_a_cr(void)
{
    static const rule_t rules[] = {
        { "0105", "", 0, true },
        { "010C", "41 0C 1A F8 \r\r>", 0, false },
    };
    fake_t f;
    elm_t e;
    if (!start(&f, &e, rules, N(rules))) return;
    e.timeout_ms = 300;

    obd_answer_t a;
    CHECK(elm_query(&e, 0x01, 0x05, &a) == ELM_TIMEOUT, "a busy adapter times out");
    CHECK(elm_query(&e, 0x01, 0x0C, &a) == ELM_OK && a.ecu[0].data[1] == 0xF8,
          "a bare CR stops it and the next request is answered");
    stop(&f, &e);
}

static void test_adapter_that_never_stops(void)
{
    static const rule_t rules[] = {
        { "0105", "", 0, true },
        { "010C", "41 0C 1A F8 \r\r>", 0, false },
    };
    fake_t f;
    elm_t e;
    if (!start(&f, &e, rules, N(rules))) return;
    f.ignore_cr = true;
    e.timeout_ms = 200;

    obd_answer_t a;
    CHECK(elm_query(&e, 0x01, 0x05, &a) == ELM_TIMEOUT, "times out");
    CHECK(elm_query(&e, 0x01, 0x0C, &a) == ELM_TIMEOUT && e.stale,
          "still busy after the CR: a timeout, and still stale");
    stop(&f, &e);
}

static void test_supported_walk(void)
{
    static const rule_t rules[] = {
        { "0100", "41 00 BE 1F A8 13 \r\r>", 0, false },   /* chains to 0x20 */
        { "0120", "41 20 80 00 00 01 \r\r>", 0, false },   /* 0x21; chains to 0x40 */
        { "0140", "NO DATA\r\r>", 0, false },               /* chained to, unanswered */
    };
    fake_t f;
    elm_t e;
    if (!start(&f, &e, rules, N(rules))) return;

    obd_pidset_t s;
    CHECK(elm_supported(&e, &s) == ELM_OK, "the walk ends at an unanswered block");
    CHECK(obd_pidset_has(&s, 0x01) && obd_pidset_has(&s, 0x0C) && obd_pidset_has(&s, 0x21),
          "PIDs of both blocks");
    CHECK(!obd_pidset_has(&s, 0x20) && !obd_pidset_has(&s, 0x40),
          "next-block bits are not PIDs to poll");
    stop(&f, &e);
}

static void test_nothing_answers_0100(void)
{
    static const rule_t rules[] = { { "0100", "NO DATA\r\r>", 0, false } };
    fake_t f;
    elm_t e;
    if (!start(&f, &e, rules, N(rules))) return;
    obd_pidset_t s;
    CHECK(elm_supported(&e, &s) == ELM_NODATA, "nothing answers: the caller must know");
    stop(&f, &e);
}

static void test_hangup_is_link_lost(void)
{
    static const rule_t rules[] = { { "010C", "41 0C 1A F8 \r\r>", 0, false } };
    fake_t f;
    elm_t e;
    if (!start(&f, &e, rules, N(rules))) return;
    atomic_store(&f.stop, 1);
    pthread_join(f.thread, NULL);
    close(f.master);            /* the far end goes away: Bluetooth dropped */
    f.master = -1;

    obd_answer_t a;
    CHECK(elm_query(&e, 0x01, 0x0C, &a) == ELM_LINK_LOST, "a hangup is link lost");
    elm_close(&e);
    CHECK(elm_query(&e, 0x01, 0x0C, &a) == ELM_LINK_LOST, "a closed session is link lost");
}

static void test_join_lines(void)
{
    char out[64];
    const char *raw = "ATZ\r\r\rELM327 v1.5\r\r>";
    elm_join_lines(raw, strlen(raw), "ATZ", out, sizeof out);
    CHECK(strcmp(out, "ELM327 v1.5") == 0, "the echo is skipped");
    raw = " a \n\nb\r>";
    elm_join_lines(raw, strlen(raw), NULL, out, sizeof out);
    CHECK(strcmp(out, "a b") == 0, "lines stripped, joined by a space");
}

int main(void)
{
    log_init("test_elm", LOG_FATAL);   /* the hangup case logs on purpose */
    test_join_lines();
    test_init_and_query();
    test_real_banner_is_not_sim();
    test_bus_asleep();
    test_late_reply_is_not_taken_for_the_next();
    test_busy_adapter_is_stopped_by_a_cr();
    test_adapter_that_never_stops();
    test_supported_walk();
    test_nothing_answers_0100();
    test_hangup_is_link_lost();
    return failures == 0 ? 0 : 1;
}
