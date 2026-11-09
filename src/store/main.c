/* storaged — append-only, crash-consistent log. Mechanism D lives here.
 *
 * Drains the rings (obdd's, and ambientd's when it exists; D-031, D-037) into the
 * write-ahead log in batches: BEGIN, samples, COMMIT, one fsync, at 64 samples or one
 * second (D-025, D-035). A ring slot is released only after the batch holding it is
 * durable, so a kill -9 at any instant loses nothing (D-039).
 *
 * Every other daemon's events arrive on one Unix socket, D-042's, and go into the same
 * batches, ordered with the samples by seq.
 *
 *   storaged [--dir DIR] [--src live|replay|synth] [--events PATH]
 *
 * --src labels storaged's own records (boot, shutdown, events); every sample and every
 * received event keeps the label its sender gave it. Without the event socket storaged
 * still records samples: it logs why and runs on. Exit codes: 0 after a clean stop, 1 if
 * it cannot start.
 */
#include "common/log.h"
#include "ipc/ring.h"
#include "ipc/uds.h"
#include "store/record.h"
#include "store/wal.h"

#include <errno.h>
#include <inttypes.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define HEARTBEAT_NS (3600ull * 1000000000ull)   /* hourly (handout §3.4) */
#define IDLE_NS      20000000L                   /* poll the rings every 20 ms when idle */
/* Events per batch. Room for these beside 64 samples is in WAL_BUF_BYTES' slack of 32
 * records, so events can never crowd samples out of a batch. A few arrive per drive. */
#define EVENTS_PER_BATCH 16u

static volatile sig_atomic_t g_stop;

static void on_signal(int sig) { (void)sig; g_stop = 1; }

static int parse_src(const char *s, uint8_t *out)
{
    if (strcmp(s, "live") == 0)   { *out = SRC_LIVE;   return 0; }
    if (strcmp(s, "replay") == 0) { *out = SRC_REPLAY; return 0; }
    if (strcmp(s, "synth") == 0)  { *out = SRC_SYNTH;  return 0; }
    return -1;
}

typedef struct {
    const char *name;
    ring_t     *ring;
    size_t      taken;      /* slots in the open batch, released after it commits */
    bool        added;      /* the open batch holds a sample from this ring */
    uint64_t    last_seq;   /* ring seq of the newest slot taken */
    bool        skip;       /* D-039: slots up to skip_seq were committed before a restart */
    uint64_t    skip_seq;
} source_t;

/* Records which ring instance this run drains, so the next run can tell a ring that
 * outlived storaged from one made fresh at boot (D-039). */
static int note_generation(wal_t *w, unsigned ns, const ring_t *r)
{
    rec_event_t e;
    memset(&e, 0, sizeof e);
    e.code = WAL_EV_RING_GEN;
    int n = snprintf(e.text, sizeof e.text, "ns=%u gen=%" PRIx64, ns, r->generation);
    e.len = (uint16_t)n;
    return wal_add(w, REC_EVENT, w->src, wal_now_mono(), wal_now_wall(), &e,
                   (uint16_t)(4u + e.len));
}

/* Records, inside the batch, the newest ring slot it holds, so a run after a crash between
 * this batch's fsync and the ring release skips exactly what is durable (D-039). */
static int note_ring_seq(wal_t *w, unsigned ns, uint64_t seq)
{
    rec_event_t e;
    memset(&e, 0, sizeof e);
    e.code = WAL_EV_RING_SEQ;
    int n = snprintf(e.text, sizeof e.text, "ns=%u seq=%" PRIx64, ns, seq);
    e.len = (uint16_t)n;
    return wal_add(w, REC_EVENT, w->src, wal_now_mono(), wal_now_wall(), &e,
                   (uint16_t)(4u + e.len));
}

/* Moves unread slots of one ring into the open batch, up to the batch's sample cap. */
static uint64_t drain(wal_t *w, source_t *s)
{
    uint64_t added = 0;
    ring_slot_t slot;
    while (w->batch_samples < WAL_BATCH_SAMPLES && ring_peek(s->ring, s->taken, &slot)) {
        s->taken++;
        s->last_seq = slot.seq;
        if (s->skip && slot.seq <= s->skip_seq) continue;   /* durable before a restart */

        rec_sample_t r;
        memset(&r, 0, sizeof r);
        r.mode = slot.mode;
        r.pid = slot.pid;
        r.raw_len = slot.len < REC_RAW_MAX ? slot.len : (uint8_t)REC_RAW_MAX;
        memcpy(r.raw, slot.data, r.raw_len);
        r.value = slot.value;
        uint64_t now_mono = wal_now_mono(), now_wall = wal_now_wall();
        uint64_t age = now_mono > slot.mono_ns ? now_mono - slot.mono_ns : 0;
        if (wal_add(w, REC_SAMPLE, slot.src, slot.mono_ns, now_wall - age, &r, sizeof r) != 0) {
            /* Full or invalid: leave the slot for after the commit. */
            s->taken--;
            s->last_seq = slot.seq - 1;
            LOG_W("sample_not_added ring=%s seq=%" PRIu64 " src=%u", s->name, slot.seq,
                  (unsigned)slot.src);
            break;
        }
        added++;
        s->added = true;
    }
    return added;
}

/* Moves waiting events into the open batch, each as an event record with its sender's
 * src, while the batch has room (D-042). An event is read off the socket only when it can
 * be added, so none is taken and then dropped for want of space. */
static uint32_t take_events(wal_t *w, ev_receiver_t *rx, uint32_t *in_batch)
{
    uint32_t added = 0;
    ev_t ev;
    while (*in_batch < EVENTS_PER_BATCH && w->len + 3u * REC_MAX_SIZE <= sizeof w->buf &&
           ev_receive(rx, &ev, 1) == 1) {
        rec_event_t e;
        memset(&e, 0, sizeof e);
        e.code = ev.code;
        e.len = ev.len;
        memcpy(e.text, ev.text, ev.len);
        if (wal_add(w, REC_EVENT, ev.src, wal_now_mono(), wal_now_wall(), &e,
                    (uint16_t)(4u + e.len)) != 0) {
            /* Only an invalid record gets here; the sender's journal still has it. */
            LOG_E("event_not_added code=0x%04x src=%u len=%u", (unsigned)ev.code,
                  (unsigned)ev.src, (unsigned)ev.len);
            continue;
        }
        (*in_batch)++;
        added++;
    }
    return added;
}

int main(int argc, char **argv)
{
    const char *dir = "/var/lib/carwatch/log";
    const char *events_path = EV_SOCK_DEFAULT;
    uint8_t src = SRC_LIVE;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--dir") == 0 && i + 1 < argc) {
            dir = argv[++i];
        } else if (strcmp(argv[i], "--events") == 0 && i + 1 < argc) {
            events_path = argv[++i];
        } else if (strcmp(argv[i], "--src") == 0 && i + 1 < argc &&
                   parse_src(argv[i + 1], &src) == 0) {
            i++;
        } else {
            fprintf(stderr, "usage: storaged [--dir DIR] [--src live|replay|synth] "
                            "[--events PATH]\n");
            return 1;
        }
    }

    log_init("storaged", LOG_INFO);
    log_set_src((data_src_t)src);
    LOG_I("start format=CWR1 dir=%s", dir);

    struct sigaction sa;
    memset(&sa, 0, sizeof sa);
    sa.sa_handler = on_signal;
    if (sigemptyset(&sa.sa_mask) != 0 || sigaction(SIGTERM, &sa, NULL) != 0 ||
        sigaction(SIGINT, &sa, NULL) != 0) {
        LOG_E("sigaction_failed errno=%d (%s)", errno, strerror(errno));
        return 1;
    }

    /* Events are worth less than samples: without the socket, record samples anyway. */
    ev_receiver_t rx;
    if (ev_receiver_open(&rx, events_path) != 0)
        LOG_W("events_disabled path=%s: other daemons' events reach the journal only",
              events_path);

    int rc = 1;
    wal_t *w = NULL;
    source_t srcs[WAL_NAMESPACES] = {
        { .name = RING_NAME_OBD },
        { .name = RING_NAME_PROBE },
    };

    srcs[0].ring = ring_attach(RING_NAME_OBD);
    if (srcs[0].ring == NULL) goto out;        /* the supervisor makes it; it retries us */
    srcs[1].ring = ring_attach(RING_NAME_PROBE);
    if (srcs[1].ring == NULL) LOG_W("probe_ring_absent name=%s", RING_NAME_PROBE);

    w = malloc(sizeof *w);
    if (w == NULL) {
        LOG_E("alloc_failed bytes=%zu", sizeof *w);
        goto out;
    }
    if (wal_open(w, dir, src, 0) != 0) goto out;

    for (unsigned ns = 0; ns < WAL_NAMESPACES; ns++) {
        if (srcs[ns].ring == NULL) continue;
        if (w->recovered.ring_gen[ns] == srcs[ns].ring->generation &&
            w->recovered.has_ring_seq[ns]) {
            srcs[ns].skip = true;
            srcs[ns].skip_seq = w->recovered.ring_seq[ns];
        }
        if (note_generation(w, ns, srcs[ns].ring) != 0 || wal_commit(w) != 0) goto out;
    }
    LOG_I("draining prev_end=%u skip_obd=%d/%" PRIu64 " skip_probe=%d/%" PRIu64,
          (unsigned)w->prev_end, (int)srcs[0].skip, srcs[0].skip_seq, (int)srcs[1].skip,
          srcs[1].skip_seq);

    uint64_t samples = 0, events = 0;
    uint32_t ev_in_batch = 0;
    uint64_t next_beat = wal_now_mono();
    while (!g_stop) {
        uint64_t added = 0;
        for (unsigned ns = 0; ns < WAL_NAMESPACES; ns++)
            if (srcs[ns].ring != NULL) added += drain(w, &srcs[ns]);
        uint32_t ev_added = take_events(w, &rx, &ev_in_batch);
        events += ev_added;

        bool any_taken = srcs[0].taken > 0 || srcs[1].taken > 0;
        if (wal_due(w, wal_now_mono()) || (any_taken && !w->open_batch)) {
            int noted = 0;
            for (unsigned ns = 0; ns < WAL_NAMESPACES; ns++)
                if (srcs[ns].added) noted |= note_ring_seq(w, ns, srcs[ns].last_seq);
            if (noted != 0 || wal_commit(w) != 0) {
                /* The batch is gone from the log but not from the rings: start again.
                 * Its events are not in a ring: they survive in their senders' journals. */
                if (ev_in_batch > 0) LOG_E("events_lost count=%u", ev_in_batch);
                ev_in_batch = 0;
                srcs[0].taken = srcs[1].taken = 0;
                srcs[0].added = srcs[1].added = false;
                struct timespec back = { 1, 0 };
                (void)nanosleep(&back, NULL);
                continue;
            }
            ev_in_batch = 0;
            for (unsigned ns = 0; ns < WAL_NAMESPACES; ns++) {
                if (srcs[ns].ring != NULL && srcs[ns].taken > 0 &&
                    !ring_release(srcs[ns].ring, srcs[ns].taken))
                    LOG_E("ring_release_failed ring=%s n=%zu", srcs[ns].name, srcs[ns].taken);
                srcs[ns].taken = 0;
                srcs[ns].added = false;
            }
        }
        samples += added;

        uint64_t now = wal_now_mono();
        if (now >= next_beat) {
            uint64_t over = ring_overruns(srcs[0].ring) +
                            (srcs[1].ring != NULL ? ring_overruns(srcs[1].ring) : 0);
            log_heartbeat(samples, over);
            next_beat = now + HEARTBEAT_NS;
        }
        if (added == 0 && ev_added == 0) {
            struct timespec idle = { 0, IDLE_NS };
            (void)nanosleep(&idle, NULL);
        }
    }

    /* Clean stop: take the last events, make the open batch durable, free its slots, and
     * mark the end. */
    events += take_events(w, &rx, &ev_in_batch);
    for (unsigned ns = 0; ns < WAL_NAMESPACES; ns++)
        if (srcs[ns].added && note_ring_seq(w, ns, srcs[ns].last_seq) != 0)
            LOG_E("ring_seq_note_failed ring=%s", srcs[ns].name);
    if (wal_commit(w) == 0)
        for (unsigned ns = 0; ns < WAL_NAMESPACES; ns++)
            if (srcs[ns].ring != NULL && srcs[ns].taken > 0)
                (void)ring_release(srcs[ns].ring, srcs[ns].taken);
    rc = wal_close(w, REC_SHUT_SIGNAL) == 0 ? 0 : 1;
    LOG_I("stop samples=%" PRIu64 " events=%" PRIu64 " events_refused=%" PRIu64 " clean=%d",
          samples, events, rx.refused, rc == 0);

out:
    if (rc != 0) LOG_E("exit status=1");
    ev_receiver_close(&rx);
    free(w);
    for (unsigned ns = 0; ns < WAL_NAMESPACES; ns++) ring_detach(srcs[ns].ring);
    return rc;
}
