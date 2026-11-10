/* cspell:words benchring */
/* obdd — capture daemon: Mode 01 requests over the Bluetooth OBD2 adapter (D-012, D-019).
 *
 * Opens /dev/obd (a udev link to the RFCOMM tty the kernel binds to the dongle), resets
 * and configures the adapter, then asks for every supported PID in turn, one request
 * outstanding, rpm and load one request in four (D-044), and pushes each reply into the
 * shared-memory ring for storaged. It runs tools/obdlog.py's loop, which made the
 * Forester's first capture, with events in place of a JSONL file (D-042):
 *
 *   - a dropped link (read 0, EIO, POLLHUP) is not a crash: link_lost, then reconnect
 *     with backoff from 1 s to 30 s (DESIGN.md §4);
 *   - the bus asleep (ignition off, key in ACC) is not an adapter fault: ecu_silent once,
 *     then retry less and less often; no reset;
 *   - 3 bad replies in a row: adapter_reset (ATZ and the settings), and if that fails,
 *     close and reopen the port;
 *   - 5 NO DATA in a row: pid_absent, retried once a minute: a PID the car stops
 *     answering is a fault the analysis must see, not a gap to fill.
 *
 * At each connection it records what it found: session (adapter, protocol, voltage, the
 * VIN's hash), supported (the PID list), static_pids (13, 1C, 1D, 51, read once). Every
 * sample carries src=live, or synth when the adapter's banner says SIM: tools/obdsim.py
 * (CLAUDE.md §2.7).
 *
 *   obdd [--port PATH] [--events PATH] [--create-ring] [--max-seconds N]
 *   obdd --replay CAPTURE [--speed X] [--events PATH] [--create-ring]
 *
 * --replay reads a tools/obdlog.py capture instead of the adapter and pushes its samples
 * through the same ring at their recorded spacing, divided by --speed (Camden,
 * 2026-11-08: the replay harness is C). A live capture replayed is labeled replay; a
 * synth one stays synth (include/obd/replay.h). Values are decoded again from the raw
 * bytes, by the same code as live replies. Events in the capture are not replayed.
 *
 * --create-ring makes the ring itself: a bench stand-in until the supervisor does (board
 * benchring). Exit codes: 0 after a clean stop (SIGTERM, SIGINT, --max-seconds, the end
 * of a replay), 1 if it cannot start or the capture is refused.
 */
#include "common/events.h"
#include "common/log.h"
#include "ipc/ring.h"
#include "ipc/uds.h"
#include "obd/elm.h"
#include "obd/pid.h"
#include "obd/replay.h"
#include "obd/sched.h"

#include <errno.h>
#include <inttypes.h>
#include <math.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define HEARTBEAT_MS     (3600 * 1000)   /* hourly (handout §3.4) */
#define BACKOFF_MIN_MS   1000
#define BACKOFF_MAX_MS   30000
#define BUS_RETRY_MAX_MS 30000            /* obdlog's BUS_RETRY_MAX */
#define NO_DATA_LIMIT    5                /* consecutive NO DATA before a PID is absent */
#define ABSENT_RETRY_MS  60000            /* how often an absent PID is tried again */
#define ERROR_LIMIT      3                /* consecutive bad replies before a reset */
#define WAIT_STEP_MS     50

static const uint8_t k_fast[] = { 0x0C, 0x04 };   /* rpm, load: the binning axes */
/* PIDs whose answer cannot change while the engine runs: O2 sensors present (two forms),
 * OBD standard, fuel type. 01 is not one: it carries the check-engine light. */
static const uint8_t k_static[] = { 0x13, 0x1C, 0x1D, 0x51 };

static volatile sig_atomic_t g_stop;
static void on_signal(int sig) { (void)sig; g_stop = 1; }

typedef struct {
    const char *port;
    ring_t     *ring;
    ev_sender_t ev;
    uint8_t     src;           /* this session's label */
    int64_t     deadline_ms;   /* --max-seconds, 0 for none */
    int64_t     next_beat_ms;
    uint64_t    samples, ring_full;
    bool        ring_full_said;
} obdd_t;

static int64_t mono_ns(void)
{
    struct timespec t;
    if (clock_gettime(CLOCK_MONOTONIC, &t) != 0) {
        LOG_E("clock_gettime_failed errno=%d (%s)", errno, strerror(errno));
        return 0;
    }
    return (int64_t)t.tv_sec * 1000000000 + t.tv_nsec;
}

static int64_t mono_ms(void) { return mono_ns() / 1000000; }

static bool stopping(const obdd_t *o)
{
    return g_stop || (o->deadline_ms != 0 && mono_ms() >= o->deadline_ms);
}

static void heartbeat(obdd_t *o)
{
    if (mono_ms() < o->next_beat_ms) return;
    log_heartbeat(o->samples, o->ring_full);
    o->next_beat_ms += HEARTBEAT_MS;
}

/* Sleeps up to ms, waking for a stop, and keeps events flowing meanwhile. */
static void wait_ms(obdd_t *o, int ms)
{
    int64_t end = mono_ms() + ms;
    while (!stopping(o) && mono_ms() < end) {
        struct timespec t = { 0, WAIT_STEP_MS * 1000000L };
        (void)nanosleep(&t, NULL);
        (void)ev_flush(&o->ev);
        heartbeat(o);
    }
}

static const char *status_name(elm_status_t st)
{
    switch (st) {
    case ELM_OK:        return "ok";
    case ELM_NODATA:    return "nodata";
    case ELM_ASLEEP:    return "asleep";
    case ELM_ERROR:     return "error";
    case ELM_TIMEOUT:   return "timeout";
    case ELM_LINK_LOST: return "link_lost";
    }
    return "?";
}

/* A value for key=value text: as is, or in double quotes if it holds a space. */
static const char *quoted(char *buf, size_t cap, const char *v)
{
    if (v[0] != '\0' && strchr(v, ' ') == NULL && strchr(v, '"') == NULL) return v;
    size_t n = 0;
    if (cap < 3) return "?";
    buf[n++] = '"';
    for (; *v != '\0' && n < cap - 2; v++) buf[n++] = *v == '"' ? '\'' : *v;
    buf[n++] = '"';
    buf[n] = '\0';
    return buf;
}

/* "AA BB" for one reply line's data. */
static void hex(const obd_line_t *l, char *out, size_t cap)
{
    size_t n = 0;
    out[0] = '\0';
    for (size_t i = 0; i < l->len && n + 4 <= cap; i++)
        n += (size_t)snprintf(out + n, cap - n, "%s%02X", i > 0 ? " " : "", l->data[i]);
}

static void push(obdd_t *o, uint8_t pid, const obd_line_t *l, int64_t at_ns)
{
    ring_slot_t s;
    memset(&s, 0, sizeof s);
    s.mono_ns = (uint64_t)at_ns;
    s.mode = 0x01;
    s.pid = pid;
    s.len = l->len < RING_PAYLOAD ? l->len : (uint8_t)RING_PAYLOAD;
    memcpy(s.data, l->data, s.len);
    s.src = o->src;
    double v;
    /* A PID with no formula (01, 03, 41, ...) keeps its raw bytes; its value is NaN. */
    s.value = obd_decode(pid, l->data, l->len, &v) ? (float)v : NAN;
    if (!ring_push(o->ring, &s)) {
        /* Refused, never overwritten (ring.h): storaged is behind or gone. Said once per
         * episode; the count is in every heartbeat. */
        o->ring_full++;
        if (!o->ring_full_said) LOG_W("ring_full depth=%zu", ring_depth(o->ring));
        o->ring_full_said = true;
        return;
    }
    o->ring_full_said = false;
    o->samples++;
}

/* Open and init the adapter, retrying with backoff until it answers or obdd stops. */
static elm_status_t connect_adapter(obdd_t *o, elm_t *e, unsigned *attempts)
{
    int backoff = BACKOFF_MIN_MS;
    bool silent_said = false;
    for (*attempts = 1; !stopping(o); (*attempts)++) {
        elm_status_t st = elm_open(e, o->port);
        if (st == ELM_OK) st = elm_init(e);
        uint8_t src = elm_is_sim(e) ? SRC_SYNTH : SRC_LIVE;
        if (st == ELM_OK) {
            if (silent_said) ev_send(&o->ev, EV_OBD_ECU_AWAKE, src, "during=connect");
            return ELM_OK;
        }
        elm_close(e);
        if (st == ELM_ASLEEP) {
            /* Ignition off: the adapter is fine and a reset will not help; wait. */
            if (!silent_said) ev_send(&o->ev, EV_OBD_ECU_SILENT, src, "during=connect");
            silent_said = true;
        } else {
            LOG_W("connect_failed attempt=%u status=%s retry_ms=%d", *attempts,
                  status_name(st), backoff);
        }
        wait_ms(o, backoff);
        backoff = backoff * 2 < BACKOFF_MAX_MS ? backoff * 2 : BACKOFF_MAX_MS;
    }
    return ELM_LINK_LOST;
}

/* What this connection found, as three events; and the PIDs to poll. */
static elm_status_t setup(obdd_t *o, elm_t *e, sched_t *sched)
{
    o->src = elm_is_sim(e) ? SRC_SYNTH : SRC_LIVE;
    log_set_src((data_src_t)o->src);

    obd_pidset_t sup;
    elm_status_t st = elm_supported(e, &sup);
    if (st != ELM_OK) return st;

    char proto[64], volts[32], vin[OBD_VIN_MAX + 1], hash[OBD_HASH_LEN + 1] = "none";
    if ((st = elm_info(e, "ATDP", proto, sizeof proto)) == ELM_LINK_LOST) return st;
    if (st != ELM_OK) snprintf(proto, sizeof proto, "?");
    if ((st = elm_info(e, "ATRV", volts, sizeof volts)) == ELM_LINK_LOST) return st;
    if (st != ELM_OK) snprintf(volts, sizeof volts, "?");
    if ((st = elm_vin(e, vin)) == ELM_LINK_LOST) return st;
    if (st == ELM_OK) obd_vin_hash(vin, hash);   /* the VIN itself is never written */

    char qa[ELM_BANNER_MAX + 2], qp[sizeof proto + 2];
    ev_send(&o->ev, EV_OBD_SESSION, o->src, "adapter=%s protocol=%s voltage=%s vin_hash=%s",
            quoted(qa, sizeof qa, e->banner), quoted(qp, sizeof qp, proto), volts, hash);

    char list[EV_TEXT_MAX];
    size_t n = 0;
    unsigned count = 0;
    list[0] = '\0';
    for (unsigned p = 0; p < 256; p++) {
        if (!obd_pidset_has(&sup, (uint8_t)p)) continue;
        count++;
        if (n + 4 < sizeof list)
            n += (size_t)snprintf(list + n, sizeof list - n, "%s%02X", n > 0 ? "," : "", p);
    }
    ev_send(&o->ev, EV_OBD_SUPPORTED, o->src, "n=%u pids=%s", count, list);

    /* The static PIDs: read once, kept out of the rotation. */
    char stat[EV_TEXT_MAX];
    n = 0;
    stat[0] = '\0';
    obd_pidset_t poll = sup;
    for (size_t i = 0; i < sizeof k_static; i++) {
        uint8_t pid = k_static[i];
        if (!obd_pidset_has(&sup, pid)) continue;
        poll.bits[pid >> 3] = (uint8_t)(poll.bits[pid >> 3] & ~(1u << (pid & 7u)));
        obd_answer_t a;
        st = elm_query(e, 0x01, pid, &a);
        if (st == ELM_LINK_LOST) return st;
        char h[3 * OBD_MAX_DATA + 1];
        if (st == ELM_OK) hex(&a.ecu[0], h, sizeof h);
        else snprintf(h, sizeof h, "%s", status_name(st));
        for (char *c = h; *c != '\0'; c++) if (*c == ' ') *c = '.';
        if (n + 8 + strlen(h) < sizeof stat)
            n += (size_t)snprintf(stat + n, sizeof stat - n, "%s%02X=%s", n > 0 ? " " : "",
                                  pid, h);
    }
    ev_send(&o->ev, EV_OBD_STATIC_PIDS, o->src, "%s", n > 0 ? stat : "none");

    sched_init(sched, &poll, k_fast, sizeof k_fast);
    if (sched->n_fast + sched->n_slow == 0) {
        LOG_W("no_pids_to_poll supported=%u", count);
        return ELM_NODATA;
    }
    LOG_I("polling pids=%zu fast=%zu src=%s", sched->n_fast + sched->n_slow, sched->n_fast,
          log_src_name((data_src_t)o->src));
    return ELM_OK;
}

typedef struct {
    bool    absent[256];
    int64_t absent_at[256];   /* when it was last tried while absent */
    int64_t now;
} absent_t;

static bool skip_absent(uint8_t pid, void *ctx)
{
    const absent_t *a = ctx;
    return a->absent[pid] && a->now - a->absent_at[pid] < ABSENT_RETRY_MS;
}

/* One connection's polling, until the link goes or obdd stops. */
static elm_status_t poll_loop(obdd_t *o, elm_t *e, sched_t *sched)
{
    static absent_t ab;              /* 2 KB: kept off the stack */
    memset(&ab, 0, sizeof ab);
    uint8_t no_data[256] = { 0 };
    bool multi_said[256] = { false };
    bool mil = false;
    int errors = 0, asleep_ms = 0;

    while (!stopping(o)) {
        (void)ev_flush(&o->ev);
        heartbeat(o);
        ab.now = mono_ms();
        int next = sched_next(sched, skip_absent, &ab);
        if (next < 0) {
            /* Every PID set aside (engine off, ECU quiet): wait for the next retry. */
            wait_ms(o, 200);
            continue;
        }
        uint8_t pid = (uint8_t)next;
        if (ab.absent[pid]) ab.absent_at[pid] = ab.now;   /* its retry; the next in a minute */

        obd_answer_t a;
        elm_status_t st = elm_query(e, 0x01, pid, &a);
        int64_t at_ns = mono_ns();
        switch (st) {
        case ELM_NODATA:
            if (no_data[pid] < 255) no_data[pid]++;
            if (no_data[pid] == NO_DATA_LIMIT && !ab.absent[pid]) {
                ab.absent[pid] = true;
                ab.absent_at[pid] = ab.now;
                ev_send(&o->ev, EV_OBD_PID_ABSENT, o->src, "pid=%02X after=%d", pid,
                        NO_DATA_LIMIT);
            }
            continue;
        case ELM_ASLEEP:
            errors = 0;
            if (asleep_ms == 0) {
                ev_send(&o->ev, EV_OBD_ECU_SILENT, o->src, "pid=%02X", pid);
                asleep_ms = BACKOFF_MIN_MS;
            }
            wait_ms(o, asleep_ms);
            asleep_ms = asleep_ms * 2 < BUS_RETRY_MAX_MS ? asleep_ms * 2 : BUS_RETRY_MAX_MS;
            continue;
        case ELM_LINK_LOST:
            ev_send(&o->ev, EV_OBD_LINK_LOST, o->src, "pid=%02X", pid);
            return ELM_LINK_LOST;
        case ELM_TIMEOUT:
        case ELM_ERROR: {
            char qt[OBD_TEXT_MAX + 2];
            errors++;
            ev_send(&o->ev, EV_OBD_REPLY_BAD, o->src, "pid=%02X kind=%s text=%s", pid,
                    status_name(st), quoted(qt, sizeof qt, st == ELM_ERROR ? a.text : "-"));
            if (errors < ERROR_LIMIT) continue;
            errors = 0;
            st = elm_init(e);
            ev_send(&o->ev, EV_OBD_ADAPTER_RESET, o->src, "after=%d result=%s", ERROR_LIMIT,
                    status_name(st));
            if (st == ELM_OK) continue;
            /* Connected but deaf: close the port and open it again, as for a drop. */
            ev_send(&o->ev, EV_OBD_LINK_LOST, o->src, "reason=adapter_deaf");
            return ELM_LINK_LOST;
        }
        case ELM_OK:
            break;
        }

        errors = 0;
        if (asleep_ms != 0) {
            asleep_ms = 0;
            ev_send(&o->ev, EV_OBD_ECU_AWAKE, o->src, "pid=%02X", pid);
        }
        no_data[pid] = 0;
        if (ab.absent[pid]) {
            ab.absent[pid] = false;
            ev_send(&o->ev, EV_OBD_PID_PRESENT, o->src, "pid=%02X", pid);
        }
        if (a.n > 1 && !multi_said[pid]) {
            /* With headers off (ATH0) a line does not say which ECU sent it: keep the
             * first, as obdlog decodes it, and say once that there were more. */
            multi_said[pid] = true;
            char h0[3 * OBD_MAX_DATA + 1], h1[3 * OBD_MAX_DATA + 1];
            hex(&a.ecu[0], h0, sizeof h0);
            hex(&a.ecu[1], h1, sizeof h1);
            ev_send(&o->ev, EV_OBD_MULTI_ECU, o->src, "pid=%02X n_ecu=%zu first=\"%s\" "
                    "second=\"%s\"", pid, a.seen, h0, h1);
        }
        if (pid == 0x01 && a.ecu[0].len >= 1) {
            bool on = (a.ecu[0].data[0] & 0x80u) != 0;   /* PID 01, byte A, bit 7: MIL */
            if (on && !mil)
                ev_send(&o->ev, EV_OBD_MIL_ON, o->src, "dtc_count=%u",
                        (unsigned)(a.ecu[0].data[0] & 0x7Fu));
            mil = on;
        }
        push(o, pid, &a.ecu[0], at_ns);
    }
    return ELM_OK;
}

static int usage(void)
{
    fprintf(stderr, "usage: obdd [--port PATH] [--events PATH] [--create-ring] "
                    "[--max-seconds N]\n"
                    "       obdd --replay CAPTURE [--speed X] [--events PATH] "
                    "[--create-ring]\n");
    return 1;
}

/* The header's description of the recorded session, as live obdd would have sent it. */
static void replay_header(obdd_t *o, const char *line, const char *path, double speed,
                          const char *src_in)
{
    char adapter[ELM_BANNER_MAX] = "?", proto[64] = "?", volts[32] = "?", hash[32] = "none";
    (void)replay_str(line, "adapter", adapter, sizeof adapter);
    (void)replay_str(line, "protocol", proto, sizeof proto);
    (void)replay_str(line, "voltage", volts, sizeof volts);
    (void)replay_str(line, "vin_hash", hash, sizeof hash);
    const char *base = strrchr(path, '/') != NULL ? strrchr(path, '/') + 1 : path;
    char qf[128], qa[ELM_BANNER_MAX + 2], qp[sizeof proto + 2];
    ev_send(&o->ev, EV_OBD_REPLAY, o->src, "file=%s speed=%g src_in=%s",
            quoted(qf, sizeof qf, base), speed, src_in);
    ev_send(&o->ev, EV_OBD_SESSION, o->src, "adapter=%s protocol=%s voltage=%s vin_hash=%s",
            quoted(qa, sizeof qa, adapter), quoted(qp, sizeof qp, proto), volts, hash);
}

/* obdd --replay: a capture's samples into the ring at their recorded spacing / speed. */
static int run_replay(obdd_t *o, const char *path, double speed)
{
    FILE *f = fopen(path, "re");
    if (f == NULL) {
        LOG_E("replay_open_failed path=%s errno=%d (%s)", path, errno, strerror(errno));
        return 1;
    }
    static char line[REPLAY_LINE_MAX];   /* 4 KB, off the stack */
    uint64_t lines = 0, skipped = 0, bad = 0;
    bool have_header = false, first = true;
    double t0 = 0.0;
    int64_t start = 0;
    int rc = 1;

    while (!stopping(o) && fgets(line, sizeof line, f) != NULL) {
        lines++;
        size_t n = strlen(line);
        if (n == sizeof line - 1 && line[n - 1] != '\n') {
            int c;
            while ((c = fgetc(f)) != EOF && c != '\n') {}   /* the rest of the long line */
            LOG_W("replay_line_too_long line=%" PRIu64, lines);
            bad++;
            continue;
        }
        char type[16], src[16];
        if (!replay_str(line, "type", type, sizeof type)) {
            bad++;
            continue;
        }
        if (!have_header) {
            /* The first record must be a labeled header: data nobody labeled is refused,
             * never guessed live (CLAUDE.md §2.7). */
            if (strcmp(type, "header") != 0 || !replay_str(line, "src", src, sizeof src) ||
                (o->src = replay_label(src)) == SRC_INVALID) {
                LOG_E("replay_refused path=%s line=%" PRIu64 " reason=no_labeled_header",
                      path, lines);
                goto out;
            }
            have_header = true;
            log_set_src((data_src_t)o->src);
            replay_header(o, line, path, speed, src);
            continue;
        }
        if (strcmp(type, "sample") != 0) {   /* events, gps, footer */
            skipped++;
            continue;
        }
        char pid_txt[8], raw[3 * OBD_MAX_DATA + 8];
        double t;
        obd_line_t l;
        char *end;
        if (!replay_str(line, "pid", pid_txt, sizeof pid_txt) ||
            !replay_str(line, "raw", raw, sizeof raw) || !replay_num(line, "t_mono", &t) ||
            !replay_raw(raw, &l)) {
            bad++;
            continue;
        }
        unsigned long pid = strtoul(pid_txt, &end, 16);
        if (*end != '\0' || pid > 0xFF) {
            bad++;
            continue;
        }
        /* A sample whose own label disagrees with the header is not this capture's. */
        if (replay_str(line, "src", src, sizeof src) && replay_label(src) != o->src) {
            LOG_W("replay_label_mismatch line=%" PRIu64 " src=%s", lines, src);
            bad++;
            continue;
        }

        if (first) {
            t0 = t;
            start = mono_ns();
            first = false;
        }
        int64_t due = start + (int64_t)((t - t0) / speed * 1e9);
        for (int64_t now = mono_ns(); now < due && !stopping(o); now = mono_ns()) {
            int64_t left = due - now < 50000000 ? due - now : 50000000;
            struct timespec ts = { 0, (long)left };
            (void)nanosleep(&ts, NULL);
            (void)ev_flush(&o->ev);
        }
        push(o, (uint8_t)pid, &l, mono_ns());
        heartbeat(o);
    }
    if (ferror(f)) {
        LOG_E("replay_read_failed path=%s line=%" PRIu64, path, lines);
        goto out;
    }
    if (!have_header) {
        LOG_E("replay_refused path=%s reason=empty", path);
        goto out;
    }
    rc = 0;
    LOG_I("replay_done lines=%" PRIu64 " samples=%" PRIu64 " skipped=%" PRIu64
          " unreadable=%" PRIu64, lines, o->samples, skipped, bad);
out:
    if (fclose(f) != 0) LOG_W("replay_close_failed errno=%d (%s)", errno, strerror(errno));
    return rc;
}

int main(int argc, char **argv)
{
    obdd_t o;
    memset(&o, 0, sizeof o);
    o.port = "/dev/obd";
    const char *events = EV_SOCK_DEFAULT;
    const char *replay = NULL;
    double speed = 1.0;
    bool create = false;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--port") == 0 && i + 1 < argc) {
            o.port = argv[++i];
        } else if (strcmp(argv[i], "--replay") == 0 && i + 1 < argc) {
            replay = argv[++i];
        } else if (strcmp(argv[i], "--speed") == 0 && i + 1 < argc) {
            char *end;
            speed = strtod(argv[++i], &end);
            if (*end != '\0' || !(speed > 0.0 && speed <= 10000.0)) return usage();
        } else if (strcmp(argv[i], "--events") == 0 && i + 1 < argc) {
            events = argv[++i];
        } else if (strcmp(argv[i], "--create-ring") == 0) {
            create = true;
        } else if (strcmp(argv[i], "--max-seconds") == 0 && i + 1 < argc) {
            char *end;
            long s = strtol(argv[++i], &end, 10);
            if (*end != '\0' || s <= 0 || s > 86400 * 30) return usage();
            o.deadline_ms = mono_ms() + s * 1000;
        } else {
            return usage();
        }
    }

    log_init("obdd", LOG_INFO);
    if (replay != NULL) LOG_I("start replay=%s speed=%g events=%s", replay, speed, events);
    else LOG_I("start port=%s events=%s", o.port, events);

    struct sigaction sa;
    memset(&sa, 0, sizeof sa);
    sa.sa_handler = on_signal;
    if (sigemptyset(&sa.sa_mask) != 0 || sigaction(SIGTERM, &sa, NULL) != 0 ||
        sigaction(SIGINT, &sa, NULL) != 0) {
        LOG_E("sigaction_failed errno=%d (%s)", errno, strerror(errno));
        return 1;
    }

    o.ring = create ? ring_create(RING_NAME_OBD) : ring_attach(RING_NAME_OBD);
    if (o.ring == NULL) {
        LOG_E("exit status=1 reason=no_ring create=%d", (int)create);   /* shm.c said why */
        return 1;
    }
    if (create) LOG_W("bench_ring_created name=%s: until the supervisor makes it (benchring)",
                      RING_NAME_OBD);
    ev_sender_init(&o.ev, events);
    o.next_beat_ms = mono_ms();

    int rc = 0;
    elm_t e;
    static sched_t sched;            /* 300 bytes of PID lists, off the stack */
    bool lost = false;
    if (replay != NULL) rc = run_replay(&o, replay, speed);
    while (replay == NULL && !stopping(&o)) {
        unsigned attempts = 0;
        if (connect_adapter(&o, &e, &attempts) != ELM_OK) break;   /* only when stopping */
        if (lost) ev_send(&o.ev, EV_OBD_RECONNECTED, o.src, "attempts=%u", attempts);

        elm_status_t st = setup(&o, &e, &sched);
        if (st == ELM_OK) {
            st = poll_loop(&o, &e, &sched);
        } else {
            LOG_W("session_setup_failed status=%s", status_name(st));
            if (st == ELM_LINK_LOST)
                ev_send(&o.ev, EV_OBD_LINK_LOST, o.src, "during=setup");
        }
        elm_close(&e);
        lost = true;
        if (!stopping(&o) && st != ELM_LINK_LOST) wait_ms(&o, BACKOFF_MIN_MS);
    }

    /* Give queued events a moment to reach storaged: a replay can end before storaged
     * has started listening. What is still queued after 2 s is in the journal only. */
    for (int i = 0; i < 40 && ev_flush(&o.ev) > 0; i++) {
        struct timespec t = { 0, WAIT_STEP_MS * 1000000L };
        (void)nanosleep(&t, NULL);
    }
    ev_sender_close(&o.ev);
    LOG_I("stop samples=%" PRIu64 " ring_full=%" PRIu64 " events_sent=%" PRIu64
          " events_dropped=%" PRIu64, o.samples, o.ring_full, o.ev.sent, o.ev.dropped);
    ring_detach(o.ring);   /* never destroyed here: storaged may still be draining it */
    return rc;
}
