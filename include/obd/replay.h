/* Reading a tools/obdlog.py capture back, for obdd --replay (handout §5: a replay harness
 * is a legitimate systems artifact).
 *
 * A capture is JSONL, one flat object per line: a header (src, adapter, protocol, ...),
 * then samples (pid, raw, t_mono, ...), events and a footer. This reads only the fields
 * obdlog writes, as flat key-value pairs: it is not a general JSON parser, and a line it
 * cannot read is counted and skipped, never guessed at.
 *
 * Labels (CLAUDE.md §2.7): a live capture replayed is replay, and so is a replay of a
 * replay; a synth capture stays synth. A capture without a label is refused.
 */
#ifndef CARWATCH_REPLAY_H
#define CARWATCH_REPLAY_H

#include "obd/pid.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define REPLAY_LINE_MAX 4096u   /* an obdlog header with 43 PIDs is about 1 KB */

/* The string value of "key" in a flat JSON object line: true, with out NUL-terminated.
 * Escapes other than \" and \\ are refused (obdlog writes none in these fields). */
bool replay_str(const char *line, const char *key, char *out, size_t cap);

/* The number value of "key": true if present and numeric (null is false). */
bool replay_num(const char *line, const char *key, double *out);

/* The label a replay of a capture labeled src carries, or 0 (SRC_INVALID) to refuse it. */
uint8_t replay_label(const char *src);

/* "0A 10" (obdlog's raw) as one reply line's data. False if it is not hex pairs. */
bool replay_raw(const char *hex, obd_line_t *out);

#endif
