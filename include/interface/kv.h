/* The control socket's reply lines (D-043, D-045): what statusd reads from a source.
 *
 * A reply is zero or more record lines, then one terminator, each line ending in "\n"
 * (a "\r\n" is accepted):
 *
 *     proc name="obdd" up=1 pid=412 src="synth"  a record: a tag, then key=value fields
 *     ok                                         the end of a good reply
 *     err unavailable storaged is down           the end of a refused one: a code, then text
 *
 * Tags, keys and codes are [a-z0-9_]. A value carries its type: in double quotes it is a
 * string (printable ASCII, no '"' and no '\', so nothing is unescaped), bare it is a JSON
 * number (json.h) or null, and anything else bare is refused. So pid="46" and pid=46 are
 * different things, and forgetting the quotes is an error, not a silent change of type.
 * Parsing copies into fixed-size fields: no allocation, and the caller's buffer can go
 * away afterward.
 */
#ifndef CARWATCH_KV_H
#define CARWATCH_KV_H

#include <stddef.h>

#define KV_MAX_LINE     512u   /* bytes in a line, not counting its newline */
#define KV_MAX_FIELDS   16u
#define KV_MAX_TAG      24u    /* these sizes count the NUL */
#define KV_MAX_KEY      24u
#define KV_MAX_VALUE    96u
#define KV_MAX_CODE     24u
#define KV_MAX_REASON   96u

typedef enum {
    KV_RECORD,       /* a tag and its fields */
    KV_OK,           /* the end of a good reply */
    KV_ERR,          /* the end of a refused reply: err_code and err_reason are set */
    KV_BAD,          /* malformed, or over a limit */
    KV_INCOMPLETE    /* no whole line yet, or no terminator yet */
} kv_kind_t;

typedef enum { KV_STR, KV_NUM, KV_NULL } kv_type_t;

typedef struct {
    char      key[KV_MAX_KEY];
    char      val[KV_MAX_VALUE];   /* a string without its quotes; a number or "null" as typed */
    kv_type_t type;
} kv_field_t;

typedef struct {
    char       tag[KV_MAX_TAG];
    kv_field_t f[KV_MAX_FIELDS];
    size_t     nfields;
    char       err_code[KV_MAX_CODE];
    char       err_reason[KV_MAX_REASON];   /* "" if the line gave none */
} kv_line_t;

/* Parses one line, s[0, len), without its newline. s need not be NUL-terminated. *out is
 * cleared first and is meaningful only for KV_RECORD and KV_ERR. */
kv_kind_t kv_parse_line(const char *s, size_t len, kv_line_t *out);

/* Takes the next line from [*cursor, end) and parses it. Advances *cursor past the line's
 * newline, even when the line is KV_BAD, so a caller can skip it. Returns KV_INCOMPLETE,
 * leaving *cursor alone, if no newline remains. */
kv_kind_t kv_reply_next(const char **cursor, const char *end, kv_line_t *out);

/* Walks a whole reply, buf[0, len), and says how it ended: KV_OK, or KV_ERR with *final
 * set (final may be NULL). KV_BAD if any line is bad or anything follows the terminator;
 * KV_INCOMPLETE if the reply stops before its terminator and that terminator's newline. */
kv_kind_t kv_reply_check(const char *buf, size_t len, kv_line_t *final);

/* The value of key on a record line, or NULL if it has none. */
const char *kv_get(const kv_line_t *line, const char *key);

#endif
