/* statusd's HTTP layer: see include/interface/http.h. No sockets and no allocation. */
#include "interface/http.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

static int hexval(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

/* Percent-decodes in[0, n) into out[cap], NUL-terminated; '+' becomes a space if asked.
 * Returns the decoded length, or -1 for a bad escape, an encoded NUL, or no room. */
static ssize_t decode(const char *in, size_t n, char *out, size_t cap, bool plus_is_space)
{
    size_t o = 0;

    if (cap == 0) return -1;
    for (size_t i = 0; i < n; i++) {
        char c = in[i];
        if (c == '%') {
            if (i + 2 >= n) return -1;
            int hi = hexval(in[i + 1]), lo = hexval(in[i + 2]);
            if (hi < 0 || lo < 0) return -1;
            c = (char)(hi * 16 + lo);
            if (c == '\0') return -1;
            i += 2;
        } else if (c == '+' && plus_is_space) {
            c = ' ';
        }
        if (o + 1 >= cap) return -1;
        out[o++] = c;
    }
    out[o] = '\0';
    return (ssize_t)o;
}

/* True if the decoded path has a ".." segment or a byte no path should carry. Checking
 * the decoded form is what stops "%2e%2e" from walking past a plain ".." test. */
static bool path_is_unsafe(const char *path, size_t n)
{
    char dec[HTTP_PATH_MAX];

    ssize_t d = decode(path, n, dec, sizeof dec, false);
    if (d < 0) return true;

    size_t seg = 0;
    for (size_t i = 0; i <= (size_t)d; i++) {
        unsigned char c = (unsigned char)dec[i];
        if (i < (size_t)d && (c < 0x20 || c == 0x7f || c == '\\')) return true;
        if (i == (size_t)d || c == '/') {
            if (i - seg == 2 && dec[seg] == '.' && dec[seg + 1] == '.') return true;
            seg = i + 1;
        }
    }
    return false;
}

http_parse_t http_parse_request(const char *buf, size_t len, http_request_t *out)
{
    const char *end = memmem(buf, len, "\r\n\r\n", 4);

    if (end == NULL) return len >= HTTP_MAX_REQUEST ? HTTP_TOO_LARGE : HTTP_INCOMPLETE;
    size_t head = (size_t)(end - buf) + 4;
    if (head > HTTP_MAX_REQUEST) return HTTP_TOO_LARGE;

    /* Tab is legal in a header; every other control byte, NUL and DEL included, is not. */
    for (size_t i = 0; i < head; i++) {
        unsigned char c = (unsigned char)buf[i];
        if ((c < 0x20 && c != '\r' && c != '\n' && c != '\t') || c == 0x7f) return HTTP_BAD;
    }

    const char *line = buf;
    const char *eol = memmem(buf, head, "\r\n", 2);   /* always found: the head ends in one */
    size_t line_len = (size_t)(eol - line);
    if (memchr(line, '\n', line_len) != NULL || memchr(line, '\r', line_len) != NULL)
        return HTTP_BAD;

    const char *sp1 = memchr(line, ' ', line_len);
    if (sp1 == NULL) return HTTP_BAD;
    if (sp1 - line != 3 || memcmp(line, "GET", 3) != 0) return HTTP_BAD_METHOD;

    const char *target = sp1 + 1;
    const char *sp2 = memchr(target, ' ', line_len - (size_t)(target - line));
    if (sp2 == NULL) return HTTP_BAD;

    size_t target_len = (size_t)(sp2 - target);
    const char *ver = sp2 + 1;
    size_t ver_len = line_len - (size_t)(ver - line);
    if (ver_len != 8 || (memcmp(ver, "HTTP/1.1", 8) != 0 && memcmp(ver, "HTTP/1.0", 8) != 0))
        return HTTP_BAD;

    if (target_len == 0 || target[0] != '/') return HTTP_BAD;
    for (size_t i = 0; i < target_len; i++) {
        unsigned char c = (unsigned char)target[i];
        if (c <= 0x20 || c >= 0x7f || c == '\\') return HTTP_BAD;
    }

    const char *q = memchr(target, '?', target_len);
    size_t path_len = q != NULL ? (size_t)(q - target) : target_len;
    size_t query_len = q != NULL ? target_len - path_len - 1 : 0;
    if (path_len >= HTTP_PATH_MAX || query_len >= HTTP_QUERY_MAX) return HTTP_BAD;
    if (path_is_unsafe(target, path_len)) return HTTP_BAD;

    memcpy(out->path, target, path_len);
    out->path[path_len] = '\0';
    if (query_len > 0) memcpy(out->query, q + 1, query_len);
    out->query[query_len] = '\0';
    return HTTP_OK;
}

int http_query_get(const char *query, const char *key, char *out, size_t n)
{
    size_t klen = strlen(key);
    const char *p = query;

    while (*p != '\0') {
        size_t pair = strcspn(p, "&");
        size_t name = strcspn(p, "=&");
        if (name == klen && memcmp(p, key, klen) == 0) {
            const char *val = p + name;
            size_t vlen = pair - name;
            if (vlen > 0) { val++; vlen--; }   /* skip the '=' */
            return decode(val, vlen, out, n, true) < 0 ? -1 : 0;
        }
        p += pair;
        if (*p == '&') p++;
    }
    return -1;
}

static const char *status_text(int status)
{
    switch (status) {
    case HTTP_STATUS_OK:                 return "OK";
    case HTTP_STATUS_BAD_REQUEST:        return "Bad Request";
    case HTTP_STATUS_NOT_FOUND:          return "Not Found";
    case HTTP_STATUS_METHOD_NOT_ALLOWED: return "Method Not Allowed";
    case HTTP_STATUS_HEADERS_TOO_LARGE:  return "Request Header Fields Too Large";
    case HTTP_STATUS_INTERNAL_ERROR:     return "Internal Server Error";
    default:                             return NULL;
    }
}

ssize_t http_write_head(char *out, size_t cap, int status, const char *ctype,
                        size_t body_len)
{
    const char *text = status_text(status);

    if (text == NULL || ctype == NULL) return -1;
    int n = snprintf(out, cap,
                     "HTTP/1.1 %d %s\r\n"
                     "Content-Type: %s\r\n"
                     "Content-Length: %zu\r\n"
                     "Connection: close\r\n"
                     "Cache-Control: no-store\r\n"
                     "X-Content-Type-Options: nosniff\r\n"
                     "\r\n",
                     status, text, ctype, body_len);
    if (n < 0 || (size_t)n >= cap) return -1;
    return n;
}
