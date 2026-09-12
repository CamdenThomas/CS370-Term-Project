#include "ipc/ring.h"

#include <string.h>

#define MASK (RING_CAPACITY - 1u)

_Static_assert((RING_CAPACITY & MASK) == 0u, "RING_CAPACITY must be a power of two");

void ring_init(ring_t *r)
{
    memset(r, 0, sizeof *r);
    atomic_store_explicit(&r->head, 0, memory_order_relaxed);
    atomic_store_explicit(&r->tail, 0, memory_order_relaxed);
    atomic_store_explicit(&r->overruns, 0, memory_order_relaxed);
}

bool ring_push(ring_t *r, const ring_slot_t *in)
{
    uint64_t head = atomic_load_explicit(&r->head, memory_order_relaxed);
    uint64_t tail = atomic_load_explicit(&r->tail, memory_order_acquire);

    if (head - tail >= RING_CAPACITY) {
        /* Full. Refuse rather than overwrite: an unread sample is data the diagnosis
         * needs, and a silent overwrite is exactly the failure mechanism F exists to
         * make impossible. */
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
    uint64_t head = atomic_load_explicit(&r->head, memory_order_acquire);
    uint64_t tail = atomic_load_explicit(&r->tail, memory_order_acquire);
    return (size_t)(head - tail);
}
