/* Mechanism D's evidence without hardware (D-025): cut a real log at every byte offset,
 * as a power cut could, and recovery must keep exactly the committed batches before the
 * cut, drop everything after, and never accept a damaged record. */
#include "store/wal.h"
#include "common/log.h"

#include <dirent.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static int failures;

#define CHECK(cond, msg) do { \
    if (!(cond)) { fprintf(stderr, "  FAIL %s:%d: %s\n", __FILE__, __LINE__, msg); \
                   failures++; } } while (0)

static void temp_dir(char *buf, size_t n)
{
    const char *base = getenv("TMPDIR");
    snprintf(buf, n, "%s/carwatch-wal.XXXXXX", base != NULL ? base : "/tmp");
    if (mkdtemp(buf) == NULL) { perror("mkdtemp"); exit(2); }
}

static void remove_dir(const char *dir)
{
    DIR *d = opendir(dir);
    if (d == NULL) return;
    struct dirent *e;
    char path[512];
    while ((e = readdir(d)) != NULL) {
        if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0) continue;
        snprintf(path, sizeof path, "%s/%s", dir, e->d_name);
        (void)unlink(path);
    }
    (void)closedir(d);
    (void)rmdir(dir);
}

static uint8_t *read_segment(const char *dir, size_t *len)
{
    char path[512];
    snprintf(path, sizeof path, "%s/log-%016x.cwr", dir, 0u);
    FILE *f = fopen(path, "rb");
    if (f == NULL) return NULL;
    uint8_t *buf = malloc(1u << 20);
    *len = buf != NULL ? fread(buf, 1, 1u << 20, f) : 0;
    (void)fclose(f);
    return buf;
}

static int add_sample(wal_t *w, uint64_t i)
{
    rec_sample_t s;
    memset(&s, 0, sizeof s);
    s.mode = 0x01; s.pid = 0x0C; s.raw_len = 2; s.raw[0] = (uint8_t)i; s.value = (float)i;
    return wal_add(w, REC_SAMPLE, SRC_SYNTH, 1000u + i, 0, &s, sizeof s);
}

/* A log of known shape: the boot batch, then batches of 10 samples, then a clean
 * shutdown. Returns the byte offset after each COMMIT in commit_end[]. */
static void test_write_reopen_clean(void)
{
    char dir[256];
    temp_dir(dir, sizeof dir);
    wal_t *w = malloc(sizeof *w);
    if (w == NULL) { failures++; return; }

    CHECK(wal_open(w, dir, SRC_SYNTH, 0) == 0, "first open");
    CHECK(w->prev_end == REC_PREV_FIRST && w->boot_id == 1, "first boot");
    for (uint64_t i = 0; i < 150; i++) {
        CHECK(add_sample(w, i) == 0, "add");
        if (wal_due(w, wal_now_mono())) CHECK(wal_commit(w) == 0, "commit when due");
    }
    CHECK(w->batch_samples == 150 % WAL_BATCH_SAMPLES, "batches of 64 committed as due");
    CHECK(wal_close(w, REC_SHUT_OPERATOR) == 0, "close");

    CHECK(wal_open(w, dir, SRC_SYNTH, 0) == 0, "reopen");
    CHECK(w->prev_end == REC_PREV_CLEAN, "a clean shutdown is recognised");
    CHECK(w->boot_id == 2, "boot id increments");
    CHECK(w->recovered.samples_committed == 150, "every sample durable");
    CHECK(w->recovered.records_lost == 0 && w->truncated == 0, "nothing lost");
    CHECK(wal_close(w, REC_SHUT_OPERATOR) == 0, "close again");
    free(w);
    remove_dir(dir);
}

static void test_every_cut_recovers_the_committed_prefix(void)
{
    char dir[256];
    temp_dir(dir, sizeof dir);
    wal_t *w = malloc(sizeof *w);
    if (w == NULL) { failures++; return; }

    /* Build the log, noting where each COMMIT ends and how many samples precede it. */
    size_t ends[16];
    uint32_t samples_at[16];
    size_t n_ends = 0;
    CHECK(wal_open(w, dir, SRC_SYNTH, 0) == 0, "open");
    ends[n_ends] = (size_t)w->seg_bytes; samples_at[n_ends++] = 0;   /* the boot batch */
    for (uint64_t b = 0; b < 5; b++) {
        for (uint64_t i = 0; i < 10; i++) CHECK(add_sample(w, b * 10 + i) == 0, "add");
        CHECK(wal_commit(w) == 0, "commit");
        ends[n_ends] = (size_t)w->seg_bytes; samples_at[n_ends++] = (uint32_t)((b + 1) * 10);
    }
    CHECK(wal_close(w, REC_SHUT_OPERATOR) == 0, "close");
    ends[n_ends] = (size_t)w->seg_bytes; samples_at[n_ends++] = 50;   /* the shutdown batch */
    remove_dir(dir);   /* keep only the bytes, below */

    size_t len = 0;
    temp_dir(dir, sizeof dir);
    CHECK(wal_open(w, dir, SRC_SYNTH, 0) == 0, "rebuild open");
    for (uint64_t b = 0; b < 5; b++) {
        for (uint64_t i = 0; i < 10; i++) CHECK(add_sample(w, b * 10 + i) == 0, "add");
        CHECK(wal_commit(w) == 0, "commit");
    }
    CHECK(wal_close(w, REC_SHUT_OPERATOR) == 0, "close");
    uint8_t *full = read_segment(dir, &len);
    remove_dir(dir);
    CHECK(full != NULL && len == ends[n_ends - 1], "the rebuilt log has the same shape");
    if (full == NULL) { free(w); return; }

    for (size_t cut = 0; cut <= len; cut++) {
        wal_scan_t sc;
        wal_scan(full, cut, &sc);
        size_t want_len = 0;
        uint32_t want_samples = 0;
        for (size_t k = 0; k < n_ends; k++)
            if (ends[k] <= cut) { want_len = ends[k]; want_samples = samples_at[k]; }
        if (sc.committed_len != want_len || sc.samples_committed != want_samples) {
            fprintf(stderr, "  FAIL cut=%zu kept=%zu want=%zu samples=%u want=%u\n",
                    cut, sc.committed_len, want_len, sc.samples_committed, want_samples);
            failures++;
            break;
        }
        CHECK(sc.ended_clean == (cut == len), "only the whole log ends clean");

        /* Every 13th cut, run the real open on the cut file: it must truncate to the
         * committed prefix and append a boot record saying it recovered. */
        if (cut % 13 == 0 || cut == len) {
            temp_dir(dir, sizeof dir);
            char path[512];
            snprintf(path, sizeof path, "%s/log-%016x.cwr", dir, 0u);
            int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            CHECK(fd >= 0 && write(fd, full, cut) == (ssize_t)cut && close(fd) == 0,
                  "write the cut file");
            CHECK(wal_open(w, dir, SRC_SYNTH, 0) == 0, "open after a cut");
            CHECK(w->seg_bytes > want_len, "boot batch appended after the kept prefix");
            CHECK(w->truncated == cut - want_len, "truncated byte count");
            CHECK(w->prev_end == (cut == len ? REC_PREV_CLEAN :
                                  cut == 0 ? REC_PREV_RECOVERED : REC_PREV_RECOVERED),
                  "prev_end says recovered unless the log ended clean");
            CHECK(wal_close(w, REC_SHUT_OPERATOR) == 0, "close after a cut");
            remove_dir(dir);
        }
    }
    free(full);
    free(w);
}

int main(void)
{
    log_init("test_wal", LOG_FATAL);
    test_write_reopen_clean();
    test_every_cut_recovers_the_committed_prefix();
    return failures == 0 ? 0 : 1;
}
