/* storaged as a process, against the real rings: samples pushed while storaged is
 * kill -9'd mid-stream and restarted must all reach the log exactly once (D-025, D-039),
 * and a SIGTERM must end the log cleanly. Mechanism D and E's promise, end to end. An
 * event sent while storaged is down must reach the log after it restarts, with its label
 * (D-042). */
#include "common/events.h"
#include "common/log.h"
#include "ipc/ring.h"
#include "ipc/uds.h"
#include "store/wal.h"

#include <dirent.h>
#include <fcntl.h>
#include <libgen.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

static int failures;

#define CHECK(cond, msg) do { \
    if (!(cond)) { fprintf(stderr, "  FAIL %s:%d: %s\n", __FILE__, __LINE__, msg); \
                   failures++; } } while (0)

#define TOTAL   1000u
#define KILL_AT 300u

static char g_bin[512];
static char g_events[EV_PATH_MAX];

static pid_t start_storaged(const char *dir)
{
    pid_t pid = fork();
    if (pid == 0) {
        int null = open("/dev/null", O_WRONLY);
        if (null >= 0) { (void)dup2(null, STDOUT_FILENO); (void)close(null); }
        execl(g_bin, "storaged", "--dir", dir, "--src", "synth", "--events", g_events,
              (char *)NULL);
        _exit(127);
    }
    return pid;
}

/* The event records in a segment with this code: how many, and whether all were synth. */
static unsigned count_events(const uint8_t *buf, size_t len, uint16_t code, bool *all_synth)
{
    unsigned n = 0;
    *all_synth = true;
    size_t off = 0;
    rec_hdr_t h;
    const uint8_t *body;
    size_t size;
    while (off < len && rec_decode(buf + off, len - off, &h, &body, &size) == REC_OK) {
        if (h.kind == REC_EVENT && (uint16_t)(body[0] | body[1] << 8) == code) {
            n++;
            if (h.src != SRC_SYNTH) *all_synth = false;
        }
        off += size;
    }
    return n;
}

static void sleep_ms(long ms)
{
    struct timespec t = { ms / 1000, (ms % 1000) * 1000000L };
    (void)nanosleep(&t, NULL);
}

static int wait_drained(ring_t *r)
{
    for (int i = 0; i < 1000; i++) {   /* up to 10 s */
        if (ring_depth(r) == 0) return 0;
        sleep_ms(10);
    }
    return -1;
}

static void push(ring_t *r, uint32_t i)
{
    ring_slot_t s;
    memset(&s, 0, sizeof s);
    s.mode = 0x01; s.pid = 0x0C; s.len = 2;
    s.src = SRC_SYNTH;   /* test data is synthesized; label it (CLAUDE.md §2.7) */
    s.mono_ns = wal_now_mono();
    s.value = (float)i;
    memcpy(s.data, &i, 2);
    while (!ring_push(r, &s)) sleep_ms(1);
}

static void remove_dir(const char *dir)
{
    DIR *d = opendir(dir);
    if (d == NULL) return;
    struct dirent *e;
    char path[512];
    while ((e = readdir(d)) != NULL) {
        if (e->d_name[0] == '.') continue;
        snprintf(path, sizeof path, "%s/%s", dir, e->d_name);
        (void)unlink(path);
    }
    (void)closedir(d);
    (void)rmdir(dir);
}

int main(int argc, char **argv)
{
    (void)argc;
    log_init("test_storaged", LOG_FATAL);
    char self[512];
    snprintf(self, sizeof self, "%s", argv[0]);
    snprintf(g_bin, sizeof g_bin, "%s/storaged", dirname(self));

    char dir[256];
    const char *base = getenv("TMPDIR");
    snprintf(dir, sizeof dir, "%s/carwatch-storaged.XXXXXX", base != NULL ? base : "/tmp");
    if (mkdtemp(dir) == NULL) { perror("mkdtemp"); return 2; }
    if (snprintf(g_events, sizeof g_events, "%s/events.sock", dir) >= (int)sizeof g_events) {
        fprintf(stderr, "  TMPDIR too long for a socket path\n");
        return 2;
    }
    ev_sender_t ev;
    ev_sender_init(&ev, g_events);

    ring_t *obd = ring_create(RING_NAME_OBD);
    ring_t *probe = ring_create(RING_NAME_PROBE);
    CHECK(obd != NULL && probe != NULL, "create rings");
    if (obd == NULL || probe == NULL) return 1;

    pid_t pid = start_storaged(dir);
    CHECK(pid > 0, "start storaged");

    for (uint32_t i = 0; i < TOTAL; i++) {
        push(obd, i);
        if (i == KILL_AT) {
            /* Mid-stream, with a batch likely open: the supervisor's view of a crash. */
            CHECK(kill(pid, SIGKILL) == 0, "kill -9 storaged");
            int st;
            CHECK(waitpid(pid, &st, 0) == pid && WIFSIGNALED(st), "storaged died");
            /* obdd's view: an event while storaged is down is queued, not lost. */
            ev_send(&ev, EV_OBD_MULTI_ECU, SRC_SYNTH, "pid=05 n_ecu=2");
            CHECK(ev.n == 1, "the event is queued while storaged is down");
            pid = start_storaged(dir);
            CHECK(pid > 0, "restart storaged");
        }
        if (i % 4 == 0) sleep_ms(1);
    }

    for (int i = 0; i < 500 && ev_flush(&ev) > 0; i++) sleep_ms(10);
    CHECK(ev.n == 0 && ev.dropped == 0, "the queued event reached the restarted storaged");
    CHECK(wait_drained(obd) == 0, "storaged drained the ring");
    CHECK(kill(pid, SIGTERM) == 0, "SIGTERM storaged");
    int st = 0;
    CHECK(waitpid(pid, &st, 0) == pid && WIFEXITED(st) && WEXITSTATUS(st) == 0,
          "storaged stopped cleanly with status 0");

    /* Read the only segment back and count. */
    char path[512];
    snprintf(path, sizeof path, "%s/log-%016x.cwr", dir, 0u);
    FILE *f = fopen(path, "rb");
    CHECK(f != NULL, "segment exists");
    if (f != NULL) {
        uint8_t *buf = malloc(1u << 22);
        size_t len = buf != NULL ? fread(buf, 1, 1u << 22, f) : 0;
        (void)fclose(f);
        wal_scan_t sc;
        wal_scan(buf, len, &sc);
        CHECK(sc.committed_len == len, "the whole log is committed");
        CHECK(sc.ended_clean, "the log ends with a shutdown record");
        if (sc.samples_committed != TOTAL)
            fprintf(stderr, "  samples in log: %u, pushed: %u\n", sc.samples_committed, TOTAL);
        CHECK(sc.samples_committed == TOTAL, "every sample once: none lost, none twice");
        bool all_synth = false;
        unsigned n_ev = count_events(buf, len, EV_OBD_MULTI_ECU, &all_synth);
        if (n_ev != 1) fprintf(stderr, "  multi_ecu events in log: %u, sent: 1\n", n_ev);
        CHECK(n_ev == 1, "the event sent while storaged was down is in the log once");
        CHECK(all_synth, "the event keeps its sender's label");
        free(buf);
    }

    ev_sender_close(&ev);
    CHECK(ring_overruns(obd) == 0 && ring_rejected(obd) == 0, "no refusals");
    ring_detach(obd);
    ring_detach(probe);
    (void)ring_destroy(RING_NAME_OBD);
    (void)ring_destroy(RING_NAME_PROBE);
    remove_dir(dir);
    return failures == 0 ? 0 : 1;
}
