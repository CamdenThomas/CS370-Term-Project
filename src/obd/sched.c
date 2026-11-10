/* The request schedule: tools/obdlog.py's Schedule, line for line (include/obd/sched.h). */
#include "obd/sched.h"

#include <string.h>

void sched_init(sched_t *s, const obd_pidset_t *pids, const uint8_t *fast, size_t n_fast)
{
    memset(s, 0, sizeof *s);
    for (size_t i = 0; i < n_fast && s->n_fast < SCHED_FAST_MAX; i++)
        if (obd_pidset_has(pids, fast[i])) s->fast[s->n_fast++] = fast[i];
    for (unsigned p = 0; p < 256; p++) {
        if (!obd_pidset_has(pids, (uint8_t)p)) continue;
        bool is_fast = false;
        for (size_t i = 0; i < s->n_fast; i++) is_fast |= s->fast[i] == p;
        if (!is_fast) s->slow[s->n_slow++] = (uint8_t)p;
    }
}

/* Schedule._take: the group's next PID not skipped, one lap at most. */
static int take(const uint8_t *group, size_t n, size_t *cursor, sched_skip_fn skip, void *ctx)
{
    for (size_t i = 0; i < n; i++) {
        uint8_t pid = group[*cursor % n];
        (*cursor)++;
        if (skip == NULL || !skip(pid, ctx)) return pid;
    }
    return -1;
}

int sched_next(sched_t *s, sched_skip_fn skip, void *ctx)
{
    int pid;
    if (s->n_fast > 0 && s->since_fast >= SCHED_FAST_EVERY - 1) {
        pid = take(s->fast, s->n_fast, &s->c_fast, skip, ctx);
        if (pid >= 0) {
            s->since_fast = 0;
            return pid;
        }
    }
    pid = take(s->slow, s->n_slow, &s->c_slow, skip, ctx);
    if (pid >= 0) {
        s->since_fast++;
        return pid;
    }
    pid = take(s->fast, s->n_fast, &s->c_fast, skip, ctx);
    if (pid >= 0) s->since_fast = 0;
    return pid;
}
