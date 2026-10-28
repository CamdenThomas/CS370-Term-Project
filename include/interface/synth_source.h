/* A source of made-up numbers, so the page can be built before the first drive (D-045).
 *
 * Every record it writes carries src="synth" (CLAUDE.md section 2.7). Its output is a pure
 * function of the request and the time it was given: no rand(), no clock reads, so a test
 * pins the time and compares exact bytes. The scenario is fixed: an active cooling verdict,
 * one process that has restarted, a finished air/fuel verdict, and a probe that drops out
 * for a minute every quarter hour.
 */
#ifndef CARWATCH_SYNTH_SOURCE_H
#define CARWATCH_SYNTH_SOURCE_H

#include "interface/source.h"

#include <stdint.h>

typedef struct {
    uint64_t t0;      /* when the fake box "booted"; uptimes count from an hour before it */
    uint64_t now_s;   /* Unix seconds, set by the caller for each request */
} synth_t;

void synth_init(synth_t *s, uint64_t now_s);
void synth_set_time(synth_t *s, uint64_t now_s);

/* The seam for s; s must outlive the returned source. */
source_t synth_source(synth_t *s);

#endif
