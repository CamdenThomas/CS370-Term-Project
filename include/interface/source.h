/* Where statusd gets its data (D-043, D-045): one function, one request line in, one reply
 * out. A source answers in the control socket's reply format (kv.h), so statusd cannot tell
 * the synthetic source from the real one, a client of /run/carwatch/ctl.sock. Replacing it
 * is the one line in statusd that builds the source_t.
 */
#ifndef CARWATCH_SOURCE_H
#define CARWATCH_SOURCE_H

#include <stddef.h>
#include <sys/types.h>

typedef struct {
    /* request is one D-043 request line without its newline, in the typed grammar of kv.h:
     * series pid="0C" from="-5m" to="now". Writes a whole reply to out[cap], NUL-terminated
     * and ending in "ok" or "err <code> <reason>", and returns its length. A refused request
     * is still a reply. Returns -1, never half a reply, only if the reply does not fit. */
    ssize_t (*query)(void *ctx, const char *request, char *out, size_t cap);
    void     *ctx;
} source_t;

static inline ssize_t source_query(const source_t *s, const char *request, char *out,
                                   size_t cap)
{
    return s->query(s->ctx, request, out, cap);
}

#endif
