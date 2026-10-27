#include "ipc/ring.h"

#include <stddef.h>
#include <string.h>

#define MASK (RING_CAPACITY - 1u)

_Static_assert(RING_CAPACITY != 0u && (RING_CAPACITY & MASK) == 0u,
               "RING_CAPACITY must be a non-zero power of two");
_Static_assert(offsetof(ring_t, tail) - offsetof(ring_t, head) >= RING_LINE &&
               offsetof(ring_t, overruns) - offsetof(ring_t, tail) >= RING_LINE,
               "head, tail and the counter must not share a cache line");

/* The ring will live in shared memory between processes (mechanism E). An atomic that
 * is not lock-free may be implemented with a lock private to one process, which is no
 * lock at all across two; on a 32-bit Pi OS a 64-bit atomic could be exactly that. */
_Static_assert(ATOMIC_LLONG_LOCK_FREE == 2, "the ring needs lock-free 64-bit atomics");

void ring_init(ring_t *r)
{
    memset(r->slot, 0, sizeof r->slot);
    atomic_init(&r->head, 0);
    atomic_init(&r->tail, 0);
    atomic_init(&r->overruns, 0);
}

bool ring_push(ring_t *r, const ring_slot_t *in)
{
    uint64_t head = atomic_load_explicit(&r->head, memory_order_relaxed);
    uint64_t tail = atomic_load_explicit(&r->tail, memory_order_acquire);

    if (head - tail >= RING_CAPACITY) {
        /* Full. Refuse rather than overwrite: an unread sample is data the diagnosis
         * needs, and a silent overwrite is exactly the hidden loss DECISIONS B.3
         * forbids. */
        atomic_fetch_add_explicit(&r->overruns, 1, memory_order_relaxed);
        return false;
    }

    r->slot[head & MASK] = *in;
    atomic_store_explicit(&r->head, head + 1, memory_order_release);
    return true;
}

bool ring_pop(ring_t *r, ring_slot_t *out)
{
    uint64_t tail = atomic_load_explicit(&r->tail, memory_order_relaxed);
    uint64_t head = atomic_load_explicit(&r->head, memory_order_acquire);

    if (tail == head) return false;

    *out = r->slot[tail & MASK];
    atomic_store_explicit(&r->tail, tail + 1, memory_order_release);
    return true;
}

uint64_t ring_overruns(const ring_t *r)
{
    return atomic_load_explicit(&r->overruns, memory_order_relaxed);
}

size_t ring_depth(const ring_t *r)
{
    /* tail first: head only grows, so a head read after the tail is never behind it.
     * Read the other way round, a third party (the supervisor, `obdctl status`) could see
     * the consumer pass a stale head and get a depth near 2^64. */
    uint64_t tail = atomic_load_explicit(&r->tail, memory_order_acquire);
    uint64_t head = atomic_load_explicit(&r->head, memory_order_acquire);
    return (size_t)(head - tail);
}
