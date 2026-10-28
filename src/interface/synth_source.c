/* A source of made-up numbers: see include/interface/synth_source.h. Pure functions of the
 * request and synth_t's clock; every record it writes says src="synth". */
#include "interface/synth_source.h"
#include "interface/kv.h"

#include <math.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_WINDOW_S   3600u   /* the longest series window */
#define MAX_SAMPLES    300u    /* a series reply holds at most this many samples */
#define BOOT_AGO_S     3600u   /* uptimes count from an hour before synth_init */
#define PI             3.14159265358979323846

typedef unsigned long long ull;

typedef struct {
    char  *buf;
    size_t cap, len;
    bool   over;   /* a line did not fit: the reply is not whole */
} out_t;

static void outf(out_t *o, const char *fmt, ...) __attribute__((format(printf, 2, 3)));

static void outf(out_t *o, const char *fmt, ...)
{
    if (o->over) return;
    size_t room = o->cap - o->len;
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(o->buf + o->len, room, fmt, ap);
    va_end(ap);
    if (n < 0 || (size_t)n >= room) { o->over = true; return; }
    o->len += (size_t)n;
}

static void reply_err(out_t *o, const char *code, const char *reason)
{
    outf(o, "err %s %s\n", code, reason);
}

/* A repeatable stand-in for noise: the same input gives the same bits on every machine. */
static uint32_t mix(uint64_t x)
{
    x ^= x >> 33;
    x *= 0xff51afd7ed558ccdULL;
    x ^= x >> 33;
    x *= 0xc4ceb9fe1a85ec53ULL;
    x ^= x >> 33;
    return (uint32_t)x;
}

static double noise(uint64_t t, uint64_t k)   /* -1.0 to 1.0 */
{
    return (double)(mix(t * 31u + k) % 2001u) / 1000.0 - 1.0;
}

static uint64_t ago(uint64_t now, uint64_t secs)
{
    return now >= secs ? now - secs : 0;
}

static const kv_field_t *find(const kv_line_t *l, const char *key)
{
    for (size_t i = 0; i < l->nfields; i++)
        if (strcmp(l->f[i].key, key) == 0) return &l->f[i];
    return NULL;
}

/* ---------------------------------------------------------------- status */

static const struct {
    const char *name;
    unsigned    pid, rss_kb, restarts, events_per_s;
} procs[] = {
    { "supervisor", 101, 1400, 0, 1 },  { "obdd",    102, 2100, 0, 15 },
    { "ambientd",   103, 1500, 0, 1 },  { "storaged", 104, 3200, 0, 16 },
    { "analyzed",   105, 4800, 2, 16 }, { "statusd",  106, 2600, 0, 1 },
};
#define NPROCS (sizeof procs / sizeof procs[0])

static void reply_status(const synth_t *s, out_t *o)
{
    uint64_t up = s->now_s - s->t0 + BOOT_AGO_S;

    for (size_t i = 0; i < NPROCS; i++) {
        /* analyzed restarted fifteen minutes ago, so its uptime is shorter. */
        uint64_t u = procs[i].restarts > 0 ? s->now_s - s->t0 + 900 : up;
        outf(o, "proc name=\"%s\" state=\"up\" pid=%u uptime_s=%llu restarts=%u rss_kb=%u "
                "cpu_pct=%.1f events=%llu drops=0 src=\"synth\"\n",
             procs[i].name, procs[i].pid, (ull)u, procs[i].restarts,
             procs[i].rss_kb + mix(s->now_s / 10 + i) % 64u,
             0.2 + (double)(mix(s->now_s / 5 + 7 * i) % 40u) / 10.0,
             (ull)(u * procs[i].events_per_s));
    }
    outf(o, "ring name=\"obd\" depth=%u capacity=4096 overruns=0 src=\"synth\"\n",
         mix(s->now_s + 1) % 40u);
    outf(o, "ring name=\"ambient\" depth=%u capacity=4096 overruns=0 src=\"synth\"\n",
         mix(s->now_s + 2) % 3u);
    outf(o, "link dongle=\"up\" probe=\"present\" light=\"green\" time_trusted=1 "
            "src=\"synth\"\n");
    outf(o, "srcmix live=0 replay=0 synth=%llu src=\"synth\"\n", (ull)(up * 16));
    outf(o, "ok\n");
}

/* -------------------------------------------------------------- verdicts */

static void reply_verdicts(const synth_t *s, out_t *o)
{
    outf(o, "verdict id=1 area=\"cooling\" sensors=\"coolant,intake\" residual=3.4 "
            "state=\"active\" start_s=%llu end_s=null src=\"synth\"\n",
         (ull)ago(s->now_s, 420));
    outf(o, "verdict id=2 area=\"air_fuel\" sensors=\"ltft,maf\" residual=2.6 "
            "state=\"ended\" start_s=%llu end_s=%llu src=\"synth\"\n",
         (ull)ago(s->now_s, 5400), (ull)ago(s->now_s, 4200));
    outf(o, "ok\n");
}

/* ---------------------------------------------------------------- faults */

static void reply_faults(const synth_t *s, out_t *o)
{
    static const struct {
        uint64_t    secs_ago;
        const char *kind, *source, *code, *detail;
    } events[] = {
        { 3000, "detection",   "ambientd", "sensor_absent",    "probe not answering" },
        { 2990, "degradation", "ambientd", "capture_continues", "engine capture unaffected" },
        { 2400, "recovery",    "ambientd", "sensor_present",   "probe answering again" },
        {  420, "detection",   "analyzed", "verdict_surfaced", "cooling residual above trip level" },
    };

    for (size_t i = 0; i < sizeof events / sizeof events[0]; i++)
        outf(o, "fault t_s=%llu kind=\"%s\" source=\"%s\" code=\"%s\" detail=\"%s\" "
                "src=\"synth\"\n",
             (ull)ago(s->now_s, events[i].secs_ago), events[i].kind, events[i].source,
             events[i].code, events[i].detail);
    outf(o, "ok\n");
}

/* ---------------------------------------------------------------- series */

static const struct {
    const char *pid;
    double      base, amp, period_s, wobble;
} signals[] = {
    { "0C", 1800, 700, 90, 25 },    /* engine speed, rpm */
    { "0D", 55, 25, 140, 1 },       /* vehicle speed, km/h */
    { "05", 88, 4, 600, 0.3 },      /* coolant, deg C */
    { "04", 35, 20, 90, 2 },        /* engine load, % */
    { "06", 0.5, 2, 60, 0.3 },      /* short-term fuel trim, % */
    { "07", 1.5, 0.5, 3000, 0.1 },  /* long-term fuel trim, % */
    { "probe", 21, 2, 1800, 0.1 },  /* cabin air, deg C */
};
#define NSIGNALS (sizeof signals / sizeof signals[0])
#define PROBE (NSIGNALS - 1)

/* "now", "-<1 to 5 digits><s|m|h>", or Unix seconds as a bare number. */
static bool parse_when(const kv_line_t *req, const char *key, const char *dflt, uint64_t now,
                       uint64_t *out)
{
    const kv_field_t *f = find(req, key);
    const char *text = f != NULL ? f->val : dflt;
    kv_type_t type = f != NULL ? f->type : KV_STR;

    if (type == KV_NULL) return false;
    if (type == KV_NUM) {
        size_t n = strlen(text);
        if (n > 12 || strspn(text, "0123456789") != n) return false;
        *out = strtoull(text, NULL, 10);
        return true;
    }
    if (strcmp(text, "now") == 0) { *out = now; return true; }

    size_t d = text[0] == '-' ? strspn(text + 1, "0123456789") : 0;
    if (d < 1 || d > 5) return false;
    char unit = text[1 + d];
    if ((unit != 's' && unit != 'm' && unit != 'h') || text[2 + d] != '\0') return false;
    uint64_t secs = strtoull(text + 1, NULL, 10) * (unit == 's' ? 1u : unit == 'm' ? 60u : 3600u);
    if (secs > now) return false;
    *out = now - secs;
    return true;
}

static void reply_series(const synth_t *s, const kv_line_t *req, out_t *o)
{
    const kv_field_t *pid = find(req, "pid");
    if (pid == NULL || pid->type != KV_STR) { reply_err(o, "usage", "series needs a pid"); return; }

    size_t k = 0;
    while (k < NSIGNALS && strcmp(signals[k].pid, pid->val) != 0) k++;
    if (k == NSIGNALS) { reply_err(o, "unknown_pid", "no such pid"); return; }

    uint64_t from, to;
    if (!parse_when(req, "from", "-5m", s->now_s, &from) ||
        !parse_when(req, "to", "now", s->now_s, &to)) {
        reply_err(o, "usage", "bad from or to");
        return;
    }
    if (from >= to || to - from > MAX_WINDOW_S) {
        reply_err(o, "usage", "window must be 1 to 3600 seconds");
        return;
    }

    /* Samples sit on multiples of step, so two polls of one window agree on their times. */
    uint64_t step = (to - from) / MAX_SAMPLES + 1;
    for (uint64_t t = (from + step - 1) / step * step; t <= to; t += step) {
        if (k == PROBE && t % 900 < 60) {   /* the probe drops out for a minute */
            outf(o, "sample pid=\"%s\" t_s=%llu value=null src=\"synth\"\n", signals[k].pid,
                 (ull)t);
            continue;
        }
        double v = signals[k].base +
                   signals[k].amp * sin(2 * PI * (double)t / signals[k].period_s + (double)k) +
                   signals[k].wobble * noise(t, k);
        outf(o, "sample pid=\"%s\" t_s=%llu value=%.1f src=\"synth\"\n", signals[k].pid,
             (ull)t, v);
    }
    outf(o, "ok\n");
}

/* ------------------------------------------------------------------ seam */

static ssize_t synth_query(void *ctx, const char *request, char *out, size_t cap)
{
    const synth_t *s = ctx;
    out_t o = { out, cap, 0, false };
    kv_line_t req;

    if (kv_parse_line(request, strlen(request), &req) != KV_RECORD)
        reply_err(&o, "usage", "malformed request");
    else if (strcmp(req.tag, "status") == 0)
        reply_status(s, &o);
    else if (strcmp(req.tag, "verdicts") == 0)
        reply_verdicts(s, &o);
    else if (strcmp(req.tag, "faults") == 0)
        reply_faults(s, &o);
    else if (strcmp(req.tag, "series") == 0)
        reply_series(s, &req, &o);
    else if (strcmp(req.tag, "baseline") == 0)
        reply_err(&o, "unavailable", "baseline is not served yet");
    else
        reply_err(&o, "unknown_verb", "no such verb");

    return o.over ? -1 : (ssize_t)o.len;
}

void synth_init(synth_t *s, uint64_t now_s)
{
    s->t0 = now_s;
    s->now_s = now_s;
}

void synth_set_time(synth_t *s, uint64_t now_s)
{
    s->now_s = now_s;
}

source_t synth_source(synth_t *s)
{
    return (source_t){ .query = synth_query, .ctx = s };
}
