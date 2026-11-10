/* Which PID obdd asks next: the C port of tools/obdlog.py's Schedule (D-044).
 *
 * The fast PIDs (rpm and load, 0C and 04: the axes every reading is binned by) get one
 * request in every SCHED_FAST_EVERY, taken in turn; every other PID shares the rest,
 * round-robin. A PID the caller says to skip (set aside as absent) costs no turn. On the
 * Forester this gave rpm and load 2.6 Hz and each other PID 0.4 Hz (DESIGN.md §1).
 */
#ifndef CARWATCH_SCHED_H
#define CARWATCH_SCHED_H

#include "obd/pid.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define SCHED_FAST_EVERY 4u
#define SCHED_FAST_MAX   4u

typedef struct {
    uint8_t  fast[SCHED_FAST_MAX], slow[256];
    size_t   n_fast, n_slow;
    size_t   c_fast, c_slow;      /* cursors */
    unsigned since_fast;        /* slow requests since the last fast one */
} sched_t;

/* True when pid should not be asked this turn. */
typedef bool (*sched_skip_fn)(uint8_t pid, void *ctx);

/* The PIDs to poll, in ascending order; fast ones that are not in pids are ignored. */
void sched_init(sched_t *s, const obd_pidset_t *pids, const uint8_t *fast, size_t n_fast);

/* The next PID to request, or -1 when every PID is skipped. skip may be NULL. */
int  sched_next(sched_t *s, sched_skip_fn skip, void *ctx);

#endif
