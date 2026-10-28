/* The control socket's reply lines: see include/interface/kv.h. No allocation. */
#include "interface/kv.h"

#include <stdbool.h>
#include <string.h>

static bool is_ident(char c)
{
    return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_';
}

/* True if s[0, n) is 1 to max-1 bytes of [a-z0-9_]. */
static bool is_name(const char *s, size_t n, size_t max)
{
    if (n == 0 || n >= max) return false;
    for (size_t i = 0; i < n; i++)
        if (!is_ident(s[i])) return false;
    return true;
}

static void copy(char *dst, const char *src, size_t n)
{
    memcpy(dst, src, n);
    dst[n] = '\0';
}

/* "err <code>" or "err <code> <reason>". s is the whole line; sp is its first space. */
static kv_kind_t parse_err(const char *s, size_t len, const char *sp, kv_line_t *out)
{
    const char *code = sp + 1;
    const char *end = s + len;
    const char *csp = memchr(code, ' ', (size_t)(end - code));
    size_t code_len = (size_t)((csp != NULL ? csp : end) - code);

    if (!is_name(code, code_len, KV_MAX_CODE)) return KV_BAD;
    copy(out->err_code, code, code_len);
    if (csp == NULL) return KV_ERR;

    const char *reason = csp + 1;
    size_t reason_len = (size_t)(end - reason);
    if (reason_len == 0 || reason_len >= KV_MAX_REASON) return KV_BAD;
    if (reason[0] == ' ' || reason[reason_len - 1] == ' ') return KV_BAD;
    copy(out->err_reason, reason, reason_len);
    return KV_ERR;
}

/* One "key=value" token, t[0, n), appended to *out. */
static bool add_field(const char *t, size_t n, kv_line_t *out)
{
    const char *eq = memchr(t, '=', n);

    if (eq == NULL || out->nfields >= KV_MAX_FIELDS) return false;
    size_t klen = (size_t)(eq - t);
    const char *val = eq + 1;
    size_t vlen = n - klen - 1;

    if (!is_name(t, klen, KV_MAX_KEY) || vlen >= KV_MAX_VALUE) return false;
    if (memchr(val, '=', vlen) != NULL) return false;
    for (size_t i = 0; i < out->nfields; i++)
        if (strlen(out->f[i].key) == klen && memcmp(out->f[i].key, t, klen) == 0)
            return false;

    kv_field_t *f = &out->f[out->nfields++];
    copy(f->key, t, klen);
    copy(f->val, val, vlen);
    return true;
}

kv_kind_t kv_parse_line(const char *s, size_t len, kv_line_t *out)
{
    memset(out, 0, sizeof *out);
    if (len == 0 || len > KV_MAX_LINE) return KV_BAD;
    for (size_t i = 0; i < len; i++) {
        unsigned char c = (unsigned char)s[i];
        if (c < 0x20 || c >= 0x7f) return KV_BAD;
    }
    if (len == 2 && memcmp(s, "ok", 2) == 0) return KV_OK;

    const char *sp = memchr(s, ' ', len);
    size_t head = sp != NULL ? (size_t)(sp - s) : len;

    if (head == 3 && memcmp(s, "err", 3) == 0)
        return sp != NULL ? parse_err(s, len, sp, out) : KV_BAD;

    /* "ok" and "err" end a reply, so neither can also tag a record. */
    if (!is_name(s, head, KV_MAX_TAG) || (head == 2 && memcmp(s, "ok", 2) == 0))
        return KV_BAD;
    copy(out->tag, s, head);

    size_t pos = head;
    while (pos < len) {
        const char *t = s + pos + 1;                 /* s[pos] is the separating space */
        size_t room = len - pos - 1;
        const char *next = memchr(t, ' ', room);
        size_t n = next != NULL ? (size_t)(next - t) : room;

        if (n == 0 || !add_field(t, n, out)) return KV_BAD;   /* n == 0: doubled or trailing space */
        pos += 1 + n;
    }
    return KV_RECORD;
}

kv_kind_t kv_reply_next(const char **cursor, const char *end, kv_line_t *out)
{
    if (*cursor >= end) return KV_INCOMPLETE;
    const char *nl = memchr(*cursor, '\n', (size_t)(end - *cursor));
    if (nl == NULL) return KV_INCOMPLETE;

    size_t len = (size_t)(nl - *cursor);
    if (len > 0 && nl[-1] == '\r') len--;
    kv_kind_t k = kv_parse_line(*cursor, len, out);
    *cursor = nl + 1;
    return k;
}

kv_kind_t kv_reply_check(const char *buf, size_t len, kv_line_t *final)
{
    const char *cur = buf, *end = buf + len;
    kv_line_t line;

    for (;;) {
        kv_kind_t k = kv_reply_next(&cur, end, &line);
        if (k == KV_RECORD) continue;
        if (k == KV_OK || k == KV_ERR) {
            if (cur != end) return KV_BAD;          /* something follows the terminator */
            if (final != NULL) *final = line;
            return k;
        }
        return k;                                   /* KV_BAD or KV_INCOMPLETE */
    }
}

const char *kv_get(const kv_line_t *line, const char *key)
{
    for (size_t i = 0; i < line->nfields; i++)
        if (strcmp(line->f[i].key, key) == 0) return line->f[i].val;
    return NULL;
}
