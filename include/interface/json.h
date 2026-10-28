/* statusd's JSON writer (D-045): builds a document in a buffer the caller owns.
 *
 *     json_t j;  json_init(&j, buf, sizeof buf);
 *     json_begin_object(&j);  json_key(&j, "ok");  json_bool(&j, true);  json_end_object(&j);
 *     ssize_t n = json_finish(&j);                  -> {"ok":true}
 *
 * Errors are sticky: a call that runs out of room or is out of order (a value where a key
 * belongs, an end that matches no begin, nesting past JSON_MAX_DEPTH) marks the writer
 * failed, every later call does nothing, and json_finish returns -1. The caller checks
 * once at the end. The writer never writes past cap, and keeps the buffer NUL-terminated.
 *
 * Strings are ASCII: a byte of 0x80 or more fails the writer, because the control-socket
 * text it is made from is ASCII (kv.h) and a byte we cannot vouch for could make invalid
 * UTF-8. Integers are written exactly. A JavaScript page reads them as doubles, so a field
 * that can pass 2^53 (nanosecond clocks, sequence numbers) must be sent as a string.
 * json_double formats with printf, so it depends on the C locale; nothing here calls
 * setlocale.
 */
#ifndef CARWATCH_JSON_H
#define CARWATCH_JSON_H

#include <stdbool.h>
#include <stddef.h>
#include <sys/types.h>

#define JSON_MAX_DEPTH  8u

typedef struct {
    char    *buf;
    size_t   cap, len;
    unsigned depth;                       /* open containers */
    bool     is_object[JSON_MAX_DEPTH];   /* per open container: object or array */
    bool     has_item[JSON_MAX_DEPTH];    /* per open container: wrote an item already */
    bool     after_key;                   /* a key was written; its value must follow */
    bool     done;                        /* the one top-level value is complete */
    bool     failed;
} json_t;

void json_init(json_t *j, char *buf, size_t cap);

void json_begin_object(json_t *j);
void json_end_object(json_t *j);
void json_begin_array(json_t *j);
void json_end_array(json_t *j);

/* Only directly inside an object, and followed by exactly one value. */
void json_key(json_t *j, const char *key);

void json_string(json_t *j, const char *s);
void json_int(json_t *j, long long v);
void json_uint(json_t *j, unsigned long long v);
void json_double(json_t *j, double v);   /* NaN and infinity fail: a gap is json_null */
void json_bool(json_t *j, bool v);
void json_null(json_t *j);

/* Writes text as a number, exactly as given, if json_is_number accepts it; else fails. */
void json_number(json_t *j, const char *text);

/* True if text is wholly one number in JSON's grammar: -?(0|[1-9][0-9]*)(\.[0-9]+)?
 * ([eE][+-]?[0-9]+)?. Not "+1", "01", ".5", "5.", "1e", "0x10", "NaN" or padded text. */
bool json_is_number(const char *text);

/* Returns the document's length, in a NUL-terminated buffer, or -1 if the writer failed
 * or the document is not exactly one complete value. */
ssize_t json_finish(json_t *j);

#endif
