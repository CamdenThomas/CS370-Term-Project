/* On-disk record format — mechanism D (D-034, with D-037 and D-040; extends 🔒 C.1, C.2).
 *
 * The constraint: power can disappear at any instant. The box shuts down cleanly when
 * its supercapacitor allows (D-024), but a cold-start crank, a pulled cable or an empty
 * bank can still cut a write, so recovery must find the torn tail and truncate it
 * without losing anything before it.
 *
 * Therefore every record is (a) self-framing: a magic number to resynchronize on and a
 * length; and (b) CRC'd over everything before the CRC, so a partly written record is
 * detectable rather than merely wrong. Records are written in committed batches (D-025):
 * a batch with no COMMIT on disk is discarded at recovery.
 *
 * Layout (little-endian, the Pi's order): a 40-byte header, a body whose size depends
 * on its kind, then a CRC32 of header and body.
 */
#ifndef CARWATCH_RECORD_H
#define CARWATCH_RECORD_H

#include <float.h>
#include <stddef.h>
#include <stdint.h>

#define REC_MAGIC          0x31525743u   /* the bytes "CWR1" */
#define REC_HDR_SIZE       40u
#define REC_CRC_SIZE       4u
#define REC_EVENT_TEXT_MAX 200u
#define REC_MAX_BODY       (4u + REC_EVENT_TEXT_MAX)
#define REC_MAX_SIZE       (REC_HDR_SIZE + REC_MAX_BODY + REC_CRC_SIZE)

typedef enum {
    REC_SAMPLE   = 1,
    REC_BEGIN    = 2,
    REC_COMMIT   = 3,
    REC_BOOT     = 4,
    REC_SHUTDOWN = 5,
    REC_EVENT    = 6
} rec_kind_t;

#define REC_FLAG_WALL_UNTRUSTED 0x01u   /* the clock lost its backup this boot (D-030) */

typedef struct {
    uint32_t magic;      /* REC_MAGIC — resync anchor for the recovery scan */
    uint16_t kind;       /* rec_kind_t */
    uint16_t body_len;   /* bytes of body; each kind allows only its own size */
    uint64_t seq;        /* per log, never reused; a gap is a detected loss, not a hidden one */
    uint64_t mono_ns;    /* CLOCK_MONOTONIC: at capture for samples, at write otherwise */
    uint64_t wall_ns;    /* Unix time from the real-time clock (D-030) */
    uint8_t  src;        /* data_src_t: 1 live, 2 replay, 3 synth; 0 is invalid (D-040) */
    uint8_t  flags;      /* REC_FLAG_* */
    uint16_t reserved;   /* zero */
    uint32_t boot_id;    /* increments every boot; mono_ns compares only within one */
} rec_hdr_t;

/* Mode 0x00 is never sent to a car, so it names the box's own sensors (D-037). */
#define REC_MODE_BOX  0x00u
#define REC_PID_PROBE 0x01u   /* the DS18B20, value in °C */
#define REC_RAW_MAX   5u      /* the most data bytes a single-frame Mode 01 reply carries */

typedef struct {
    uint8_t mode;
    uint8_t pid;
    uint8_t raw_len;           /* 0 to REC_RAW_MAX */
    uint8_t raw[REC_RAW_MAX];  /* the reply's data bytes, kept beside the decoded value */
    float   value;
} rec_sample_t;

typedef struct { uint64_t txn_id; } rec_begin_t;

typedef struct {
    uint64_t txn_id;
    uint32_t records;    /* samples in the batch */
    uint32_t reserved;
} rec_commit_t;

typedef enum { REC_PREV_CLEAN = 0, REC_PREV_RECOVERED = 1, REC_PREV_FIRST = 2 } rec_prev_end_t;

typedef struct {
    uint8_t  prev_end;   /* rec_prev_end_t */
    uint8_t  reserved[3];
    uint32_t bytes_truncated;
    uint32_t records_lost;
} rec_boot_t;

typedef enum {
    REC_SHUT_SOURCE_LOST = 1,
    REC_SHUT_SIGNAL      = 2,
    REC_SHUT_OPERATOR    = 3
} rec_shutdown_reason_t;

typedef struct { uint8_t reason; uint8_t reserved[7]; } rec_shutdown_t;

/* On disk an event is 4 + len bytes: code, len, then len bytes of key=value text. */
typedef struct {
    uint16_t code;
    uint16_t len;
    char     text[REC_EVENT_TEXT_MAX];
} rec_event_t;

/* The layout is the file format: a change here is a format change (D-034). */
_Static_assert(__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__, "the log is little-endian");
_Static_assert(sizeof(float) == 4 && FLT_MANT_DIG == 24, "sample values are IEEE f32");
_Static_assert(sizeof(rec_hdr_t) == REC_HDR_SIZE, "header is 40 bytes");
_Static_assert(offsetof(rec_hdr_t, kind) == 4 && offsetof(rec_hdr_t, body_len) == 6 &&
               offsetof(rec_hdr_t, seq) == 8 && offsetof(rec_hdr_t, mono_ns) == 16 &&
               offsetof(rec_hdr_t, wall_ns) == 24 && offsetof(rec_hdr_t, src) == 32 &&
               offsetof(rec_hdr_t, flags) == 33 && offsetof(rec_hdr_t, boot_id) == 36,
               "header offsets are D-034's");
_Static_assert(sizeof(rec_sample_t) == 12 && offsetof(rec_sample_t, value) == 8,
               "sample body is 12 bytes");
_Static_assert(sizeof(rec_begin_t) == 8, "begin body is 8 bytes");
_Static_assert(sizeof(rec_commit_t) == 16, "commit body is 16 bytes");
_Static_assert(sizeof(rec_boot_t) == 12, "boot body is 12 bytes");
_Static_assert(sizeof(rec_shutdown_t) == 8, "shutdown body is 8 bytes");
_Static_assert(offsetof(rec_event_t, text) == 4, "event text follows code and len");

typedef enum {
    REC_OK        =  0,
    REC_SHORT     = -1,   /* fewer bytes than the record needs: a torn tail, or not yet written */
    REC_BAD_MAGIC = -2,
    REC_BAD_KIND  = -3,
    REC_BAD_LEN   = -4,   /* body_len, or an event's len, that its kind does not allow */
    REC_BAD_CRC   = -5,
    REC_BAD_SRC   = -6    /* a CRC-valid record with no valid label: a writer bug */
} rec_status_t;

uint32_t rec_crc32(const void *buf, uint32_t len);

/* Writes magic, the header (kind, body_len and the rest from *hdr), the body and the CRC
 * into out. Returns the record's total size, or 0 if the header or body is invalid or
 * the record does not fit in cap. */
size_t rec_encode(uint8_t *out, size_t cap, const rec_hdr_t *hdr, const void *body);

/* Parses the record at buf[0, len). On REC_OK, fills *hdr, points *body into buf and sets
 * *size to the record's total length. Never reads past len. */
rec_status_t rec_decode(const uint8_t *buf, size_t len, rec_hdr_t *hdr,
                        const uint8_t **body, size_t *size);

const char *rec_status_name(rec_status_t st);

#endif
