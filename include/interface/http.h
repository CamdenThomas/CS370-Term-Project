/* statusd's HTTP layer (D-043): bytes in, bytes out, no sockets.
 *
 * Only GET is served. The parser takes whatever has been read so far and says whether it is
 * a whole request, part of one, or one to refuse; the writer makes the head of a response
 * and leaves the body to the caller, so the embedded page is never copied. Every buffer is
 * fixed-size: a request cannot make this layer allocate.
 */
#ifndef CARWATCH_HTTP_H
#define CARWATCH_HTTP_H

#include <stddef.h>
#include <sys/types.h>

#define HTTP_MAX_REQUEST   4096u   /* request line and headers together */
#define HTTP_PATH_MAX      128u
#define HTTP_QUERY_MAX     256u

/* The statuses this layer can write. http_write_head refuses any other. */
typedef enum {
    HTTP_STATUS_OK                  = 200,
    HTTP_STATUS_BAD_REQUEST         = 400,
    HTTP_STATUS_NOT_FOUND           = 404,
    HTTP_STATUS_METHOD_NOT_ALLOWED  = 405,
    HTTP_STATUS_HEADERS_TOO_LARGE   = 431,
    HTTP_STATUS_INTERNAL_ERROR      = 500
} http_status_t;

/* A refusal is its own HTTP status, so a caller answers with the value it was given. The
 * two non-refusals are outside the status range: HTTP_OK does not say which status the
 * route will answer with, and HTTP_INCOMPLETE is not an answer yet. */
typedef enum {
    HTTP_INCOMPLETE = -1,   /* read more and call again */
    HTTP_OK = 0,            /* a whole, acceptable request */
    HTTP_BAD = HTTP_STATUS_BAD_REQUEST,                /* malformed, or a target we refuse */
    HTTP_BAD_METHOD = HTTP_STATUS_METHOD_NOT_ALLOWED,  /* anything but GET */
    HTTP_TOO_LARGE = HTTP_STATUS_HEADERS_TOO_LARGE     /* no end of headers in the cap */
} http_parse_t;

typedef struct {
    char path[HTTP_PATH_MAX];     /* starts with '/', still percent-encoded */
    char query[HTTP_QUERY_MAX];   /* after the '?', still percent-encoded; "" if none */
} http_request_t;

/* Parses buf[0, len), which need not be NUL-terminated. Fills *out only on HTTP_OK.
 * Headers are checked for stray control bytes and then ignored. */
http_parse_t http_parse_request(const char *buf, size_t len, http_request_t *out);

/* Finds key in an "a=1&b=2" query, percent-decodes its value into out[n] ('+' is a
 * space), and returns 0. Returns -1 if the key is absent, the encoding is bad, or the
 * value does not fit. */
int http_query_get(const char *query, const char *key, char *out, size_t n);

/* Writes the status line and headers, ending with the blank line, for a response whose
 * body is body_len bytes of ctype. The head is NUL-terminated, so cap must be its length
 * plus one. Returns the length written (not counting the NUL), or -1 if cap is too small
 * or status is not an http_status_t this layer knows. */
ssize_t http_write_head(char *out, size_t cap, int status, const char *ctype,
                        size_t body_len);

#endif
