/* Structured logging. Every line is machine-parseable, because the 48-hour soak log
 * is a graded deliverable and we will have to plot it. (handout §3.4) */
#ifndef CARWATCH_LOG_H
#define CARWATCH_LOG_H

#include <stdint.h>

typedef enum { LOG_DEBUG, LOG_INFO, LOG_WARN, LOG_ERROR, LOG_FATAL } log_level_t;

/* Data provenance. CLAUDE.md §2.7: replayed or synthesized data is labeled as such
 * EVERYWHERE it appears. This field exists from day one so that it is impossible to
 * forget later. */
typedef enum { SRC_LIVE = 0, SRC_REPLAY = 1, SRC_SYNTH = 2 } data_src_t;

void        log_init(const char *component, log_level_t min);
void        log_set_src(data_src_t src);
data_src_t  log_get_src(void);
const char *log_src_name(data_src_t src);

void log_emit(log_level_t lvl, const char *fmt, ...)
    __attribute__((format(printf, 2, 3)));

/* Hourly heartbeat: process liveness, RSS, cumulative event counts. Required by the
 * soak (handout §3.4) and the source of the RSS-vs-time plot in docs/EVALUATION.md §3. */
void log_heartbeat(uint64_t events, uint64_t drops);

#define LOG_D(...) log_emit(LOG_DEBUG, __VA_ARGS__)
#define LOG_I(...) log_emit(LOG_INFO,  __VA_ARGS__)
#define LOG_W(...) log_emit(LOG_WARN,  __VA_ARGS__)
#define LOG_E(...) log_emit(LOG_ERROR, __VA_ARGS__)

#endif
