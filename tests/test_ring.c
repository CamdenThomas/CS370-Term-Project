/* The ring's contract is a no-drop guarantee, so the tests are about
 * what must NOT happen: an unread slot must never be overwritten, and a refusal must
 * always be counted. */
#include "ipc/ring.h"
#include "common/log.h"

#include <pthread.h>
#include <stdatomic.h>
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
    ring_t *r = aligned_alloc(RING_LINE, sizeof *r);   /* ring_t is cache-line aligned */
    if (r == NULL) { failures++; return; }
    ring_init(r);

    ring_slot_t out;
    CHECK(!ring_pop(r, &out), "pop on empty ring must fail");
    CHECK(ring_depth(r) == 0, "empty depth");

    for (uint64_t i = 0; i < 100; i++)
        CHECK(ring_push(r, &(ring_slot_t){ .seq = i, .src = SRC_SYNTH }), "push should succeed");

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
    ring_t *r = aligned_alloc(RING_LINE, sizeof *r);   /* ring_t is cache-line aligned */
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
    ring_t *r = aligned_alloc(RING_LINE, sizeof *r);   /* ring_t is cache-line aligned */
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

/* A producer, a consumer and a third reader running at once: the consumer must see
 * every sequence number in order, and the observer (the supervisor or `obdctl status`
 * asking how full the ring is) must never see a depth above capacity. */
#define CONCURRENT_N 400000u

static ring_t     *g_ring;
static atomic_int  g_done;
static unsigned    g_bad_depth;

static void *producer(void *arg)
{
    (void)arg;
    for (uint64_t i = 0; i < CONCURRENT_N; ) {
        ring_slot_t s = mk(i);
        memcpy(s.data, &i, sizeof i);
        if (ring_push(g_ring, &s)) i++;
    }
    return NULL;
}

static void *observer(void *arg)
{
    (void)arg;
    while (!atomic_load(&g_done))
        if (ring_depth(g_ring) > RING_CAPACITY) g_bad_depth++;
    return NULL;
}

static void test_concurrent_order_and_depth(void)
{
    g_ring = aligned_alloc(RING_LINE, sizeof *g_ring);
    if (g_ring == NULL) { failures++; return; }
    ring_init(g_ring);
    atomic_store(&g_done, 0);
    g_bad_depth = 0;

    pthread_t p, o;
    if (pthread_create(&p, NULL, producer, NULL) != 0) { failures++; free(g_ring); return; }
    if (pthread_create(&o, NULL, observer, NULL) != 0) {
        failures++; pthread_join(p, NULL); free(g_ring); return;
    }

    unsigned out_of_order = 0;
    ring_slot_t out;
    for (uint64_t want = 0; want < CONCURRENT_N; ) {
        if (!ring_pop(g_ring, &out)) continue;
        uint64_t v;
        memcpy(&v, out.data, sizeof v);
        if (out.seq != want || v != want) out_of_order++;
        want++;
    }
    pthread_join(p, NULL);
    atomic_store(&g_done, 1);
    pthread_join(o, NULL);

    CHECK(out_of_order == 0, "concurrent consumer saw a gap, a repeat or a torn slot");
    CHECK(g_bad_depth == 0, "an observer saw a depth above capacity");
    free(g_ring);
}

static void test_unlabeled_slot_is_refused(void)
{
    ring_t *r = aligned_alloc(RING_LINE, sizeof *r);   /* ring_t is cache-line aligned */
    if (r == NULL) { failures++; return; }
    ring_init(r);

    ring_slot_t s = mk(0);
    s.src = SRC_INVALID;
    CHECK(!ring_push(r, &s), "an unlabeled slot must be refused (D-040)");
    CHECK(ring_rejected(r) == 1, "the refusal must be counted");
    CHECK(ring_overruns(r) == 0, "a rejection is not an overrun");
    CHECK(ring_depth(r) == 0, "nothing was queued");

    s.src = SRC_SYNTH + 1;
    CHECK(!ring_push(r, &s), "an out-of-range label must be refused");
    free(r);
}

static void test_ring_numbers_its_slots(void)
{
    ring_t *r = aligned_alloc(RING_LINE, sizeof *r);   /* ring_t is cache-line aligned */
    if (r == NULL) { failures++; return; }
    ring_init(r);

    ring_slot_t s = mk(12345), out;   /* whatever seq the producer sets is replaced */
    CHECK(ring_push(r, &s) && ring_push(r, &s), "two pushes");
    CHECK(ring_pop(r, &out) && out.seq == 0, "first slot is seq 0");
    CHECK(ring_pop(r, &out) && out.seq == 1, "second slot is seq 1");
    free(r);
}

/* storaged's pattern (D-039): look at a batch, write it, fsync, and only then free it.
 * Until the release, the slots stay put and the producer cannot reuse them. */
static void test_peek_then_release(void)
{
    ring_t *r = aligned_alloc(RING_LINE, sizeof *r);   /* ring_t is cache-line aligned */
    if (r == NULL) { failures++; return; }
    ring_init(r);

    for (uint64_t i = 0; i < 5; i++) {
        ring_slot_t s = mk(i);
        CHECK(ring_push(r, &s), "push");
    }

    ring_slot_t out;
    CHECK(ring_peek(r, 0, &out) && out.seq == 0, "peek oldest");
    CHECK(ring_peek(r, 4, &out) && out.seq == 4, "peek newest");
    CHECK(!ring_peek(r, 5, &out), "peek past the newest");
    CHECK(ring_depth(r) == 5, "peeking consumes nothing");

    CHECK(!ring_release(r, 6), "release more than is unread");
    CHECK(ring_depth(r) == 5, "a refused release consumes nothing");
    CHECK(ring_release(r, 3), "release a batch of three");
    CHECK(ring_depth(r) == 2, "depth after release");
    CHECK(ring_peek(r, 0, &out) && out.seq == 3, "next unread after release");

    /* A full ring whose slots are peeked but not released still refuses the producer. */
    for (uint64_t i = 0; ring_depth(r) < RING_CAPACITY; i++) {
        ring_slot_t s = mk(i);
        CHECK(ring_push(r, &s), "fill");
    }
    ring_slot_t extra = mk(0);
    CHECK(ring_peek(r, 0, &out), "peek on full ring");
    CHECK(!ring_push(r, &extra), "peeked but unreleased slots must not be overwritten");
    free(r);
}

int main(void)
{
    test_peek_then_release();
    test_ring_numbers_its_slots();
    test_unlabeled_slot_is_refused();
    test_concurrent_order_and_depth();
    test_empty_then_fifo();
    test_full_refuses_never_overwrites();
    test_wraparound();
    return failures == 0 ? 0 : 1;
}
