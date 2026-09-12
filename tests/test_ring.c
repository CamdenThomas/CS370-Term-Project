/* Mechanism F is a no-drop guarantee, so the tests are about what must NOT happen:
 * an unread slot must never be overwritten, and a refusal must always be counted. */
#include "ipc/ring.h"
#include "common/log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures;

#define CHECK(cond, msg) do { \
    if (!(cond)) { fprintf(stderr, "  FAIL %s:%d: %s\n", __FILE__, __LINE__, msg); \
                   failures++; } } while (0)

static ring_slot_t mk(uint64_t seq)
{
    ring_slot_t s;
    memset(&s, 0, sizeof s);
    s.seq = seq;
    s.src = (uint8_t)SRC_SYNTH;   /* test data is synthesized; label it (CLAUDE.md §2.7) */
    s.len = 8;
    return s;
}

static void test_empty_then_fifo(void)
{
    ring_t *r = malloc(sizeof *r);
    if (r == NULL) { failures++; return; }
    ring_init(r);

    ring_slot_t out;
    CHECK(!ring_pop(r, &out), "pop on empty ring must fail");
    CHECK(ring_depth(r) == 0, "empty depth");

    for (uint64_t i = 0; i < 100; i++)
        CHECK(ring_push(r, &(ring_slot_t){ .seq = i }), "push should succeed");

    CHECK(ring_depth(r) == 100, "depth after 100 pushes");

    for (uint64_t i = 0; i < 100; i++) {
        CHECK(ring_pop(r, &out), "pop should succeed");
        CHECK(out.seq == i, "FIFO order violated");
    }
    CHECK(ring_depth(r) == 0, "drained depth");
    free(r);
}

static void test_full_refuses_never_overwrites(void)
{
    ring_t *r = malloc(sizeof *r);
    if (r == NULL) { failures++; return; }
    ring_init(r);

    for (uint64_t i = 0; i < RING_CAPACITY; i++) {
        ring_slot_t s = mk(i);
        CHECK(ring_push(r, &s), "push within capacity");
    }

    ring_slot_t extra = mk(999999);
    CHECK(!ring_push(r, &extra), "push on full ring must be refused");
    CHECK(ring_overruns(r) == 1, "refusal must be counted as an overrun");

    /* The oldest unread slot must still be intact — this is the whole guarantee. */
    ring_slot_t out;
    CHECK(ring_pop(r, &out), "pop after refused push");
    CHECK(out.seq == 0, "oldest unread slot was overwritten");
    free(r);
}

static void test_wraparound(void)
{
    ring_t *r = malloc(sizeof *r);
    if (r == NULL) { failures++; return; }
    ring_init(r);

    ring_slot_t out;
    for (uint64_t i = 0; i < RING_CAPACITY * 3; i++) {
        ring_slot_t s = mk(i);
        CHECK(ring_push(r, &s), "push during wraparound");
        CHECK(ring_pop(r, &out), "pop during wraparound");
        CHECK(out.seq == i, "sequence broken across wraparound");
    }
    CHECK(ring_overruns(r) == 0, "no overruns expected in lockstep push/pop");
    free(r);
}

int main(void)
{
    test_empty_then_fifo();
    test_full_refuses_never_overwrites();
    test_wraparound();
    return failures == 0 ? 0 : 1;
}
