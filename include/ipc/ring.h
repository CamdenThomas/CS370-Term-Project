/* Single-producer / single-consumer lock-free ring from capture (obdd) to storage
 * (storaged), its only consumer; analysis reads the committed log instead (D-031).
 * It is the handoff mechanism E's process boundary needs. It is not claimed as
 * mechanism F: Mode 01 rates never stress it, and no Pi-side sensor is added (D-029).
 *
 * The no-drop guarantee is the point. A gap in the record is a gap in the diagnosis,
 * so a drop must be impossible to hide: every slot carries a sequence number and the
 * consumer accounts for every one of them. If the ring is full the producer must
 * report the overrun rather than silently overwrite an unread sample (DECISIONS B.3).
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
#define RING_PAYLOAD    16u   /* bytes; a Mode 01 reply's data (<= 4 bytes) fits with room */

typedef struct {
    uint64_t seq;                    /* set by ring_push: the slot's position, never reused */
    uint64_t mono_ns;                /* CLOCK_MONOTONIC at capture */
    uint32_t pid;                    /* (mode << 16) | PID — room for 16-bit Mode 22 PIDs */
    uint8_t  len;
    uint8_t  src;                    /* data_src_t — live | replay | synth */
    uint8_t  _pad[2];
    uint8_t  data[RING_PAYLOAD];
} ring_slot_t;

#define RING_LINE    64u          /* cache line on the Pi 4's Cortex-A72 */
#define RING_MAGIC   0x474E5243u  /* the bytes "CRNG" */
#define RING_VERSION 1u           /* bump whenever ring_t or ring_slot_t changes layout */

/* Shared-memory names (D-039): obdd's ring and ambientd's (D-037). */
#define RING_NAME_OBD   "/carwatch-obd"
#define RING_NAME_PROBE "/carwatch-probe"

/* The header lets an attaching process refuse a ring written by a different build.
 * head and tail each own a cache line, so the producer's stores never invalidate the
 * line the consumer is spinning on, and the reverse. */
typedef struct {
    uint32_t magic;          /* RING_MAGIC */
    uint16_t version;        /* RING_VERSION */
    uint16_t slot_size;      /* sizeof(ring_slot_t) */
    uint32_t capacity;       /* RING_CAPACITY */
    _Alignas(RING_LINE) _Atomic uint64_t head;      /* producer writes */
    _Alignas(RING_LINE) _Atomic uint64_t tail;      /* consumer writes */
    _Alignas(RING_LINE) _Atomic uint64_t overruns;  /* refused pushes — must stay zero */
    _Atomic uint64_t                     rejected;  /* unlabeled slots refused (D-040) */
    _Alignas(RING_LINE) ring_slot_t      slot[RING_CAPACITY];
} ring_t;

/* Initializes an unshared ring. Only the creator calls it (ring_create does). */
void     ring_init(ring_t *r);

/* True when r's header matches this build's layout. */
bool     ring_layout_ok(const ring_t *r);

/* Shared memory (src/ipc/shm.c, D-039). ring_create removes any stale object of that
 * name, makes a new one and initializes it: the supervisor calls it once at start.
 * ring_attach maps an existing ring and returns NULL, having logged why, if it is
 * missing, too small or of another layout: producers and storaged call it, and never
 * initialize. Every failure is logged with errno. */
ring_t  *ring_create(const char *name);
ring_t  *ring_attach(const char *name);
void     ring_detach(ring_t *r);
int      ring_destroy(const char *name);

/* Returns false when full (never overwriting an unread slot; counts an overrun) or when
 * the slot carries no valid src (counts a rejection, D-040). */
bool     ring_push(ring_t *r, const ring_slot_t *in);

/* Copies the i-th unread slot (0 is the oldest) without consuming it. Returns false when
 * fewer than i + 1 slots are unread. Consumer only. */
bool     ring_peek(const ring_t *r, size_t i, ring_slot_t *out);

/* Consumes the n oldest unread slots, freeing them for the producer. Returns false, and
 * consumes nothing, if fewer than n are unread. storaged calls it only after the batch
 * holding those slots is fsync'd, so a crash before that loses nothing (D-039).
 * Consumer only. */
bool     ring_release(ring_t *r, size_t n);

/* ring_peek(r, 0, out) then ring_release(r, 1). Returns false when empty. */
bool     ring_pop(ring_t *r, ring_slot_t *out);

uint64_t ring_overruns(const ring_t *r);
uint64_t ring_rejected(const ring_t *r);
size_t   ring_depth(const ring_t *r);

#endif
