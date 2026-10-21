/* On-disk record format — mechanism D.
 *
 * The constraint: power disappears every time the key turns off. The box shuts down
 * cleanly when its supercapacitor allows (D-024), but a cold-start crank, a pulled
 * cable or an empty bank can still cut a write, so the format must let recovery find
 * the torn tail and truncate it without losing anything before it.
 *
 * Therefore every record is (a) self-framing, so a scan can resynchronize, and
 * (b) CRC'd over its whole body, so a partially-flushed record is detectable rather
 * than merely wrong. Records are written in committed batches (D-025): a batch with
 * no COMMIT on disk is discarded at recovery.
 */
#ifndef CARWATCH_RECORD_H
#define CARWATCH_RECORD_H

#include <stdint.h>

#define REC_MAGIC   0x43574B31u   /* "CWK1" */
#define REC_VERSION 1u

typedef struct {
    uint32_t magic;      /* REC_MAGIC — resync anchor for the recovery scan */
    uint16_t version;
    uint16_t len;        /* bytes of payload following this header */
    uint64_t seq;        /* monotonic; a gap here is a detected loss, not a hidden one */
    uint64_t mono_ns;    /* CLOCK_MONOTONIC — for intervals, survives wall-clock jumps */
    uint64_t wall_ns;    /* CLOCK_REALTIME, set from the RTC at boot (D-030); untrusted if
                          * the RTC reported invalid time */
    uint8_t  src;        /* data_src_t — live | replay | synth. Never optional. */
    uint8_t  kind;       /* rec_kind_t */
    uint16_t _pad;
    uint32_t crc32;      /* over header-with-crc-zeroed, then payload */
} rec_hdr_t;

typedef enum {
    REC_OBD_RAW    = 1,   /* raw adapter reply line, kept for replay (D-015) */
    REC_SAMPLE     = 2,   /* decoded PID sample */
    REC_BASELINE   = 3,   /* persisted per-bin statistics — must survive power loss */
    REC_VERDICT    = 4,
    REC_FAULT      = 5,   /* detection / degradation / recovery event */
    REC_HEARTBEAT  = 6
} rec_kind_t;

uint32_t rec_crc32(const void *buf, uint32_t len);

#endif
