#include "store/wal.h"

#include "common/log.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

uint64_t wal_now_mono(void)
{
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) return 0;
    return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
}

uint64_t wal_now_wall(void)
{
    struct timespec ts;
    if (clock_gettime(CLOCK_REALTIME, &ts) != 0) return 0;
    return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
}

/* ---------------------------------------------------------------- scan (recovery) */

static unsigned ns_of(const rec_sample_t *s)
{
    return s->mode == REC_MODE_BOX ? 1u : 0u;
}

void wal_scan(const uint8_t *buf, size_t len, wal_scan_t *out)
{
    memset(out, 0, sizeof *out);

    /* What the open batch would contribute if its COMMIT turns up. */
    bool     in_batch = false;
    uint64_t txn = 0;
    uint32_t samples = 0;
    uint64_t mono[WAL_NAMESPACES] = { 0 }, gen[WAL_NAMESPACES] = { 0 };
    uint16_t last_kind = 0;

    size_t off = 0;
    while (off < len) {
        rec_hdr_t h;
        const uint8_t *body;
        size_t size;
        if (rec_decode(buf + off, len - off, &h, &body, &size) != REC_OK) break;

        if (h.kind == REC_BEGIN) {
            if (in_batch) break;                      /* a BEGIN inside a batch: damage */
            rec_begin_t b;
            memcpy(&b, body, sizeof b);
            in_batch = true; txn = b.txn_id; samples = 0;
            memcpy(mono, out->last_mono, sizeof mono);
            memcpy(gen, out->ring_gen, sizeof gen);
        } else if (!in_batch) {
            break;                                    /* every record belongs to a batch */
        } else if (h.kind == REC_COMMIT) {
            rec_commit_t c;
            memcpy(&c, body, sizeof c);
            if (c.txn_id != txn) break;
            in_batch = false;
            out->committed_len = off + size;
            out->any_committed = true;
            out->ended_clean = last_kind == REC_SHUTDOWN;
            out->last_seq = h.seq;
            out->last_txn = txn;
            out->last_boot_id = h.boot_id;
            out->samples_committed += samples;
            memcpy(out->last_mono, mono, sizeof mono);
            memcpy(out->ring_gen, gen, sizeof gen);
            samples = 0;
        } else if (h.kind == REC_SAMPLE) {
            rec_sample_t s;
            memcpy(&s, body, sizeof s);
            samples++;
            mono[ns_of(&s)] = h.mono_ns;
        } else if (h.kind == REC_EVENT) {
            rec_event_t e;
            memset(&e, 0, sizeof e);
            memcpy(&e, body, h.body_len);
            unsigned ns;
            uint64_t g;
            char text[REC_EVENT_TEXT_MAX + 1];
            memcpy(text, e.text, e.len);
            text[e.len] = '\0';
            if (e.code == WAL_EV_RING_GEN &&
                sscanf(text, "ns=%u gen=%" SCNx64, &ns, &g) == 2 && ns < WAL_NAMESPACES)
                gen[ns] = g;
        }
        last_kind = h.kind;
        off += size;
    }
    if (in_batch) out->records_lost = samples;
}

/* ---------------------------------------------------------------- files */

/* The newest segment's first seq, or -1 if the directory holds none. */
static int newest_segment(const char *dir, uint64_t *first_seq)
{
    DIR *d = opendir(dir);
    if (d == NULL) {
        LOG_E("wal_opendir_failed dir=%s errno=%d (%s)", dir, errno, strerror(errno));
        return -2;
    }
    int found = -1;
    struct dirent *e;
    while ((e = readdir(d)) != NULL) {
        uint64_t s;
        char tail[8];
        if (sscanf(e->d_name, "log-%16" SCNx64 ".%7s", &s, tail) == 2 &&
            strcmp(tail, "cwr") == 0 && (found < 0 || s > *first_seq)) {
            *first_seq = s;
            found = 0;
        }
    }
    if (closedir(d) != 0)
        LOG_W("wal_closedir_failed dir=%s errno=%d (%s)", dir, errno, strerror(errno));
    return found;
}

static void seg_path(char *out, size_t n, const char *dir, uint64_t first_seq)
{
    snprintf(out, n, "%s/log-%016" PRIx64 ".cwr", dir, first_seq);
}

/* A new file's directory entry is durable only once the directory is fsync'd. */
static int sync_dir(const char *dir)
{
    int fd = open(dir, O_RDONLY);
    if (fd < 0) {
        LOG_E("wal_dir_open_failed dir=%s errno=%d (%s)", dir, errno, strerror(errno));
        return -1;
    }
    int rc = fsync(fd);
    if (rc != 0) LOG_E("wal_dir_fsync_failed dir=%s errno=%d (%s)", dir, errno, strerror(errno));
    if (close(fd) != 0) LOG_W("wal_dir_close_failed errno=%d (%s)", errno, strerror(errno));
    return rc;
}

static int create_segment(wal_t *w, uint64_t first_seq)
{
    char path[256];
    seg_path(path, sizeof path, w->dir, first_seq);
    int fd = open(path, O_WRONLY | O_CREAT | O_EXCL | O_APPEND | O_CLOEXEC, 0644);
    if (fd < 0) {
        LOG_E("wal_segment_create_failed path=%s errno=%d (%s)", path, errno, strerror(errno));
        return -1;
    }
    if (sync_dir(w->dir) != 0) {
        if (close(fd) != 0) LOG_W("wal_close_failed errno=%d (%s)", errno, strerror(errno));
        return -1;
    }
    w->fd = fd;
    w->seg_bytes = 0;
    LOG_I("wal_segment_created path=%s", path);
    return 0;
}

/* Reads the segment, scans it, cuts it back to its last COMMIT and leaves it open for
 * appending. */
static int recover_segment(wal_t *w, uint64_t first_seq)
{
    char path[256];
    seg_path(path, sizeof path, w->dir, first_seq);
    uint8_t *data = NULL;
    int rc = -1;
    int fd = open(path, O_RDWR | O_APPEND | O_CLOEXEC);
    if (fd < 0) {
        LOG_E("wal_segment_open_failed path=%s errno=%d (%s)", path, errno, strerror(errno));
        return -1;
    }
    struct stat st;
    if (fstat(fd, &st) != 0) {
        LOG_E("wal_fstat_failed path=%s errno=%d (%s)", path, errno, strerror(errno));
        goto out;
    }
    size_t size = (size_t)st.st_size;
    data = malloc(size > 0 ? size : 1);
    if (data == NULL) {
        LOG_E("wal_alloc_failed bytes=%zu", size);
        goto out;
    }
    for (size_t got = 0; got < size; ) {
        ssize_t n = pread(fd, data + got, size - got, (off_t)got);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) {
            LOG_E("wal_read_failed path=%s errno=%d (%s)", path, errno, strerror(errno));
            goto out;
        }
        got += (size_t)n;
    }

    wal_scan(data, size, &w->recovered);
    size_t keep = w->recovered.committed_len;
    if (keep < size) {
        if (ftruncate(fd, (off_t)keep) != 0 || fsync(fd) != 0) {
            LOG_E("wal_truncate_failed path=%s errno=%d (%s)", path, errno, strerror(errno));
            goto out;
        }
    }
    LOG_I("recovered path=%s bytes=%zu kept=%zu truncated=%zu samples_lost=%u clean=%d",
          path, size, keep, size - keep, w->recovered.records_lost,
          (int)w->recovered.ended_clean);
    w->prev_end = (w->recovered.ended_clean && keep == size) ? REC_PREV_CLEAN
                                                             : REC_PREV_RECOVERED;
    w->truncated = size - keep;
    w->fd = fd;
    w->seg_bytes = keep;
    fd = -1;
    rc = 0;

out:
    free(data);
    if (fd >= 0 && close(fd) != 0)
        LOG_W("wal_close_failed path=%s errno=%d (%s)", path, errno, strerror(errno));
    return rc;
}

/* ---------------------------------------------------------------- batches */

static int append(wal_t *w, uint16_t kind, uint8_t src, uint64_t mono_ns, uint64_t wall_ns,
                  const void *body, uint16_t body_len)
{
    rec_hdr_t h;
    memset(&h, 0, sizeof h);
    h.kind = kind; h.body_len = body_len; h.seq = w->next_seq;
    h.mono_ns = mono_ns; h.wall_ns = wall_ns; h.src = src; h.flags = w->flags;
    h.boot_id = w->boot_id;
    size_t n = rec_encode(w->buf + w->len, sizeof w->buf - w->len, &h, body);
    if (n == 0) return -1;
    w->len += n;
    w->next_seq++;
    return 0;
}

int wal_add(wal_t *w, uint16_t kind, uint8_t src, uint64_t mono_ns, uint64_t wall_ns,
            const void *body, uint16_t body_len)
{
    /* Keep room for the COMMIT that must close the batch. */
    if (w->len + REC_HDR_SIZE + body_len + REC_CRC_SIZE +
        REC_HDR_SIZE + sizeof(rec_commit_t) + REC_CRC_SIZE > sizeof w->buf)
        return -1;
    if (!w->open_batch) {
        w->batch_seq0 = w->next_seq;
        rec_begin_t b = { .txn_id = w->next_txn };
        if (append(w, REC_BEGIN, w->src, wal_now_mono(), wal_now_wall(), &b, sizeof b) != 0)
            return -1;
        w->open_batch = true;
        w->batch_mono = wal_now_mono();
        w->batch_samples = 0;
    }
    if (append(w, kind, src, mono_ns, wall_ns, body, body_len) != 0) return -1;
    if (kind == REC_SAMPLE) w->batch_samples++;
    return 0;
}

bool wal_due(const wal_t *w, uint64_t now_mono_ns)
{
    return w->open_batch && (w->batch_samples >= WAL_BATCH_SAMPLES ||
                             now_mono_ns - w->batch_mono >= WAL_BATCH_NS);
}

static void drop_batch(wal_t *w)
{
    w->len = 0;
    w->open_batch = false;
    w->batch_samples = 0;
    w->next_seq = w->batch_seq0;   /* the dropped batch's seqs were never durable */
}

int wal_commit(wal_t *w)
{
    if (!w->open_batch) return 0;
    rec_commit_t c = { .txn_id = w->next_txn, .records = w->batch_samples, .reserved = 0 };
    if (append(w, REC_COMMIT, w->src, wal_now_mono(), wal_now_wall(), &c, sizeof c) != 0) {
        LOG_E("wal_commit_encode_failed txn=%" PRIu64, w->next_txn);
        drop_batch(w);
        return -1;
    }

    for (size_t done = 0; done < w->len; ) {
        ssize_t n = write(w->fd, w->buf + done, w->len - done);
        if (n < 0 && errno == EINTR) continue;
        if (n < 0) {
            LOG_E("wal_write_failed txn=%" PRIu64 " errno=%d (%s)", w->next_txn, errno,
                  strerror(errno));
            goto undo;
        }
        done += (size_t)n;
    }
    if (fsync(w->fd) != 0) {
        LOG_E("fsync_failed txn=%" PRIu64 " errno=%d (%s)", w->next_txn, errno,
              strerror(errno));
        goto undo;
    }

    w->seg_bytes += w->len;
    w->len = 0;
    w->open_batch = false;
    w->batch_samples = 0;
    w->next_txn++;

    if (w->seg_bytes >= WAL_SEGMENT_BYTES) {
        if (close(w->fd) != 0) LOG_W("wal_close_failed errno=%d (%s)", errno, strerror(errno));
        w->fd = -1;
        if (create_segment(w, w->next_seq) != 0) return -1;
    }
    return 0;

undo:
    /* Leave the file ending at its last COMMIT; the caller keeps the ring slots. */
    if (ftruncate(w->fd, (off_t)w->seg_bytes) != 0)
        LOG_E("wal_undo_truncate_failed errno=%d (%s)", errno, strerror(errno));
    drop_batch(w);
    return -1;
}

/* ---------------------------------------------------------------- open, close */

int wal_open(wal_t *w, const char *dir, uint8_t src, uint8_t flags)
{
    memset(w, 0, sizeof *w);
    w->fd = -1;
    w->src = src;
    w->flags = flags;
    if (snprintf(w->dir, sizeof w->dir, "%s", dir) >= (int)sizeof w->dir) {
        LOG_E("wal_dir_too_long dir=%s", dir);
        return -1;
    }
    if (mkdir(dir, 0755) != 0 && errno != EEXIST) {
        LOG_E("wal_mkdir_failed dir=%s errno=%d (%s)", dir, errno, strerror(errno));
        return -1;
    }

    uint64_t first = 0;
    int found = newest_segment(dir, &first);
    if (found == -2) return -1;
    if (found < 0) {
        w->prev_end = REC_PREV_FIRST;
        w->next_seq = 0;
        w->next_txn = 1;
        w->boot_id = 1;
        if (create_segment(w, 0) != 0) return -1;
    } else {
        if (recover_segment(w, first) != 0) return -1;
        w->next_seq = w->recovered.any_committed ? w->recovered.last_seq + 1 : first;
        w->next_txn = w->recovered.last_txn + 1;
        w->boot_id = w->recovered.last_boot_id + 1;
    }

    rec_boot_t b;
    memset(&b, 0, sizeof b);
    b.prev_end = (uint8_t)w->prev_end;
    b.records_lost = w->recovered.records_lost;
    b.bytes_truncated = w->truncated > UINT32_MAX ? UINT32_MAX : (uint32_t)w->truncated;
    if (wal_add(w, REC_BOOT, src, wal_now_mono(), wal_now_wall(), &b, sizeof b) != 0 ||
        wal_commit(w) != 0) {
        LOG_E("wal_boot_record_failed dir=%s", dir);
        if (w->fd >= 0 && close(w->fd) != 0)
            LOG_W("wal_close_failed errno=%d (%s)", errno, strerror(errno));
        w->fd = -1;
        return -1;
    }
    LOG_I("wal_open dir=%s boot_id=%u prev_end=%u next_seq=%" PRIu64, dir, w->boot_id,
          (unsigned)w->prev_end, w->next_seq);
    return 0;
}

int wal_close(wal_t *w, rec_shutdown_reason_t reason)
{
    int rc = wal_commit(w);
    rec_shutdown_t s;
    memset(&s, 0, sizeof s);
    s.reason = (uint8_t)reason;
    if (rc == 0 &&
        (wal_add(w, REC_SHUTDOWN, w->src, wal_now_mono(), wal_now_wall(), &s, sizeof s) != 0 ||
         wal_commit(w) != 0))
        rc = -1;
    if (w->fd >= 0 && close(w->fd) != 0) {
        LOG_E("wal_close_failed errno=%d (%s)", errno, strerror(errno));
        rc = -1;
    }
    w->fd = -1;
    return rc;
}
