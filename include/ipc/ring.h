/* Single-producer / single-consumer lock-free ring — mechanism F.
 *
 * The no-drop guarantee is the point. A gap in the record is a gap in the diagnosis,
 * so a drop must be impossible to hide: every slot carries a sequence number and the
 * consumer accounts for every one of them. If the ring is full the producer must
 * report the overrun rather than silently overwrite an unread sample (D-002).
 *
 * Layout is fixed so the ring can live in shared memory across the process boundary
 * that mechanism E requires.
 */
#ifndef CARWATCH_RING_H
#define CARWATCH_RING_H

#include <stdatomic.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define RING_CAPACITY 4096u   /* power of two — the mask depends on it */
#define RING_PAYLOAD    16u   /* bytes; one CAN frame's data field fits with room */

typedef struct {
    uint64_t seq;                    /* monotonic, never reused */
    uint64_t mono_ns;                /* CLOCK_MONOTONIC at capture */
    uint32_t can_id;
    uint8_t  len;
    uint8_t  src;                    /* data_src_t — live | replay | synth */
    uint8_t  _pad[2];
    uint8_t  data[RING_PAYLOAD];
} ring_slot_t;

typedef struct {
    _Atomic uint64_t head;           /* producer writes */
    _Atomic uint64_t tail;           /* consumer writes */
    _Atomic uint64_t overruns;       /* refused pushes — must stay zero */
    uint64_t         _pad[5];        /* keep head/tail off one cache line */
    ring_slot_t      slot[RING_CAPACITY];
} ring_t;

void     ring_init(ring_t *r);

/* Returns false when full. Never overwrites an unread slot; increments overruns. */
bool     ring_push(ring_t *r, const ring_slot_t *in);

/* Returns false when empty. */
bool     ring_pop(ring_t *r, ring_slot_t *out);

uint64_t ring_overruns(const ring_t *r);
size_t   ring_depth(const ring_t *r);

#endif
