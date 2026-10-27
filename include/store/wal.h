/* storaged's write-ahead log: mechanism D (D-025, D-034, D-035, D-041).
 *
 * Records go to the newest segment file, log-<first seq>.cwr, in batches: BEGIN, the
 * records, COMMIT, then one fsync. A batch counts only once its COMMIT is on disk. At
 * open, recovery scans the newest segment, keeps everything up to the last COMMIT and
 * truncates the rest (undo is truncation: nothing is ever overwritten, C.1), then writes
 * a boot batch saying whether the last session ended cleanly.
 */
#ifndef CARWATCH_WAL_H
#define CARWATCH_WAL_H

#include "store/record.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define WAL_SEGMENT_BYTES   (16u * 1024u * 1024u)   /* D-034 */
#define WAL_BATCH_SAMPLES   64u                     /* D-035 */
#define WAL_BATCH_NS        1000000000ull           /* D-035: one second */
#define WAL_BUF_BYTES       (WAL_BATCH_SAMPLES * 56u + 32u * REC_MAX_SIZE)
#define WAL_NAMESPACES      2u    /* sample namespaces: 0 the car (Mode 01), 1 the box (D-037) */

/* Event codes storaged writes itself (D-042: sender byte 0x05). */
#define WAL_EV_RING_GEN     0x0501u   /* "ns=<0|1> gen=<hex>": which ring instance fed us */

/* What a scan of one segment found. */
typedef struct {
    size_t   committed_len;       /* bytes up to and including the last COMMIT */
    bool     any_committed;
    bool     ended_clean;         /* the last committed batch ended with a SHUTDOWN */
    uint64_t last_seq, last_txn;
    uint32_t last_boot_id;
    uint32_t samples_committed;   /* in the whole segment */
    uint32_t records_lost;        /* decodable samples after the last COMMIT */
    uint64_t last_mono[WAL_NAMESPACES];   /* last committed sample's mono_ns, per namespace */
    uint64_t ring_gen[WAL_NAMESPACES];    /* last WAL_EV_RING_GEN, per namespace (0: none) */
} wal_scan_t;

typedef struct {
    char       dir[200];
    int        fd;
    uint64_t   seg_bytes;         /* committed length of the open segment */
    uint64_t   next_seq, next_txn;
    uint32_t   boot_id;
    uint8_t    src;               /* src of storaged's own records */
    uint8_t    flags;             /* REC_FLAG_* for every record this boot */
    wal_scan_t recovered;         /* what the scan at open found */
    rec_prev_end_t prev_end;
    uint64_t   truncated;         /* bytes the recovery at open cut off */
    bool       open_batch;
    uint64_t   batch_seq0;        /* next_seq before the open batch's BEGIN */
    uint64_t   batch_mono;        /* when the open batch began */
    uint32_t   batch_samples;
    size_t     len;
    uint8_t    buf[WAL_BUF_BYTES];
} wal_t;

/* Scans buf[0, len) as one segment. Never reads past len. */
void wal_scan(const uint8_t *buf, size_t len, wal_scan_t *out);

/* Opens (creating dir if needed) and recovers the newest segment, then commits a boot
 * batch. src labels storaged's own records. Returns 0, or -1 having logged why. */
int  wal_open(wal_t *w, const char *dir, uint8_t src, uint8_t flags);

/* Adds one record to the open batch, opening it with a BEGIN if needed. Returns 0, or -1
 * if the record is invalid or the batch is full (commit first). */
int  wal_add(wal_t *w, uint16_t kind, uint8_t src, uint64_t mono_ns, uint64_t wall_ns,
             const void *body, uint16_t body_len);

/* True when the open batch holds 64 samples or is a second old (D-035). */
bool wal_due(const wal_t *w, uint64_t now_mono_ns);

/* Appends COMMIT, writes the batch and fsyncs. Returns 0 once it is durable, or -1 having
 * logged why and cut the file back to its last commit; the batch is then discarded and
 * the caller must not release its ring slots. */
int  wal_commit(wal_t *w);

/* Commits any open batch, then a batch holding a SHUTDOWN record, and closes. */
int  wal_close(wal_t *w, rec_shutdown_reason_t reason);

uint64_t wal_now_mono(void);
uint64_t wal_now_wall(void);

#endif
