/* src/obd/sched.c against tests/tools/test_obdlog.py's Schedule cases, same sequences. */
#include "obd/sched.h"

#include <stdio.h>
#include <string.h>

static int failures;

#define CHECK(cond, msg) do { \
    if (!(cond)) { fprintf(stderr, "  FAIL %s:%d: %s\n", __FILE__, __LINE__, msg); \
                   failures++; } } while (0)

static const uint8_t k_fast[] = { 0x0C, 0x04 };

static void init(sched_t *s, const uint8_t *pids, size_t n, const uint8_t *fast, size_t nf)
{
    obd_pidset_t set;
    memset(&set, 0, sizeof set);
    for (size_t i = 0; i < n; i++) obd_pidset_add(&set, pids[i]);
    sched_init(s, &set, fast, nf);
}

static bool skip_one(uint8_t pid, void *ctx) { return pid == *(const uint8_t *)ctx; }
static bool skip_all(uint8_t pid, void *ctx) { (void)pid; (void)ctx; return true; }

static bool picks(sched_t *s, const int *want, size_t n, sched_skip_fn skip, void *ctx)
{
    for (size_t i = 0; i < n; i++) {
        int got = sched_next(s, skip, ctx);
        if (got != want[i]) {
            fprintf(stderr, "  pick %zu: got %02X, want %02X\n", i, (unsigned)got,
                    (unsigned)want[i]);
            return false;
        }
    }
    return true;
}

int main(void)
{
    sched_t s;

    static const uint8_t six[] = { 0x04, 0x05, 0x06, 0x0C, 0x0D, 0x0F };
    init(&s, six, 6, k_fast, 2);
    static const int w1[] = { 0x05, 0x06, 0x0D, 0x0C, 0x0F, 0x05, 0x06, 0x04,
                              0x0D, 0x0F, 0x05, 0x0C };
    CHECK(picks(&s, w1, 12, NULL, NULL), "fast PIDs get one request in four, in turn");

    static const uint8_t four[] = { 0x04, 0x05, 0x06, 0x0C };
    init(&s, four, 4, k_fast, 2);
    uint8_t skip06 = 0x06;
    static const int w2[] = { 0x05, 0x05, 0x05, 0x0C };
    CHECK(picks(&s, w2, 4, skip_one, &skip06), "skipped PIDs cost no turn");

    static const uint8_t three[] = { 0x04, 0x05, 0x0C };
    init(&s, three, 3, k_fast, 2);
    uint8_t skip05 = 0x05;
    static const int w3[] = { 0x0C, 0x04, 0x0C, 0x04 };
    CHECK(picks(&s, w3, 4, skip_one, &skip05), "only fast PIDs left are asked every time");

    static const uint8_t two[] = { 0x05, 0x0C };
    static const uint8_t f0c[] = { 0x0C };
    init(&s, two, 2, f0c, 1);
    CHECK(sched_next(&s, skip_all, NULL) == -1, "everything skipped is -1");

    init(&s, three, 3, NULL, 0);
    static const int w5[] = { 0x04, 0x05, 0x0C, 0x04, 0x05, 0x0C };
    CHECK(picks(&s, w5, 6, NULL, NULL), "no fast PIDs is plain round robin");

    static const uint8_t no_fast[] = { 0x05, 0x0D };
    init(&s, no_fast, 2, k_fast, 2);
    static const int w6[] = { 0x05, 0x0D, 0x05, 0x0D };
    CHECK(s.n_fast == 0 && picks(&s, w6, 4, NULL, NULL),
          "a fast PID that is not polled is dropped");

    return failures == 0 ? 0 : 1;
}
