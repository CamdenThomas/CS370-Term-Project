/* statusd's JSON writer: see include/interface/json.h. No allocation. */
#include "interface/json.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void fail(json_t *j)
{
    j->failed = true;
}

/* Appends n bytes, keeping room for the NUL. While the writer has not failed, len < cap. */
static void put(json_t *j, const char *s, size_t n)
{
    if (j->failed) return;
    if (n >= j->cap - j->len) { fail(j); return; }
    memcpy(j->buf + j->len, s, n);
    j->len += n;
    j->buf[j->len] = '\0';
}

static void put_char(json_t *j, char c)
{
    put(j, &c, 1);
}

/* Called before any value: checks that a value may go here and writes the comma. */
static bool begin_value(json_t *j)
{
    if (j->failed) return false;
    if (j->depth == 0) {
        if (j->done) { fail(j); return false; }
        return true;
    }
    unsigned top = j->depth - 1;
    if (j->is_object[top]) {
        if (!j->after_key) { fail(j); return false; }   /* an object wants a key first */
        j->after_key = false;
        return true;
    }
    if (j->has_item[top]) put_char(j, ',');
    j->has_item[top] = true;
    return !j->failed;
}

/* Called after a value is complete. */
static void end_value(json_t *j)
{
    if (j->depth == 0) j->done = true;
}

static void put_scalar(json_t *j, const char *text, size_t n)
{
    if (!begin_value(j)) return;
    put(j, text, n);
    end_value(j);
}

static void put_quoted(json_t *j, const char *s)
{
    if (s == NULL) { fail(j); return; }
    put_char(j, '"');
    for (const unsigned char *p = (const unsigned char *)s; *p != '\0' && !j->failed; p++) {
        unsigned char c = *p;
        if (c >= 0x80) { fail(j); return; }
        switch (c) {
        case '"':  put(j, "\\\"", 2); break;
        case '\\': put(j, "\\\\", 2); break;
        case '\b': put(j, "\\b", 2);  break;
        case '\f': put(j, "\\f", 2);  break;
        case '\n': put(j, "\\n", 2);  break;
        case '\r': put(j, "\\r", 2);  break;
        case '\t': put(j, "\\t", 2);  break;
        default:
            if (c < 0x20 || c == 0x7f) {
                char esc[8];
                int n = snprintf(esc, sizeof esc, "\\u%04x", c);
                put(j, esc, (size_t)n);
            } else {
                put_char(j, (char)c);
            }
        }
    }
    put_char(j, '"');
}

void json_init(json_t *j, char *buf, size_t cap)
{
    memset(j, 0, sizeof *j);
    j->buf = buf;
    j->cap = cap;
    if (cap == 0) { fail(j); return; }
    buf[0] = '\0';
}

static void begin_container(json_t *j, bool object)
{
    if (!begin_value(j)) return;
    if (j->depth >= JSON_MAX_DEPTH) { fail(j); return; }
    j->is_object[j->depth] = object;
    j->has_item[j->depth] = false;
    j->depth++;
    put_char(j, object ? '{' : '[');
}

static void end_container(json_t *j, bool object)
{
    if (j->failed) return;
    if (j->depth == 0 || j->is_object[j->depth - 1] != object || j->after_key) {
        fail(j);
        return;
    }
    j->depth--;
    put_char(j, object ? '}' : ']');
    end_value(j);
}

void json_begin_object(json_t *j) { begin_container(j, true); }
void json_end_object(json_t *j)   { end_container(j, true); }
void json_begin_array(json_t *j)  { begin_container(j, false); }
void json_end_array(json_t *j)    { end_container(j, false); }

void json_key(json_t *j, const char *key)
{
    if (j->failed) return;
    if (key == NULL || j->depth == 0 || !j->is_object[j->depth - 1] || j->after_key) {
        fail(j);
        return;
    }
    unsigned top = j->depth - 1;
    if (j->has_item[top]) put_char(j, ',');
    j->has_item[top] = true;
    put_quoted(j, key);
    put_char(j, ':');
    j->after_key = true;
}

void json_string(json_t *j, const char *s)
{
    if (!begin_value(j)) return;
    put_quoted(j, s);
    end_value(j);
}

void json_int(json_t *j, long long v)
{
    char tmp[32];
    int n = snprintf(tmp, sizeof tmp, "%lld", v);
    put_scalar(j, tmp, (size_t)n);
}

void json_uint(json_t *j, unsigned long long v)
{
    char tmp[32];
    int n = snprintf(tmp, sizeof tmp, "%llu", v);
    put_scalar(j, tmp, (size_t)n);
}

/* The shortest of 15, 16 and 17 significant digits that reads back as the same double. */
void json_double(json_t *j, double v)
{
    char tmp[40];
    int n = 0;

    if (!isfinite(v)) { fail(j); return; }
    for (int prec = 15; prec <= 17; prec++) {
        n = snprintf(tmp, sizeof tmp, "%.*g", prec, v);
        if (strtod(tmp, NULL) == v) break;
    }
    put_scalar(j, tmp, (size_t)n);
}

void json_bool(json_t *j, bool v)
{
    put_scalar(j, v ? "true" : "false", v ? 4 : 5);
}

void json_null(json_t *j)
{
    put_scalar(j, "null", 4);
}

static bool is_digit(char c)
{
    return c >= '0' && c <= '9';
}

bool json_is_number(const char *text)
{
    if (text == NULL) return false;
    const char *p = text;

    if (*p == '-') p++;
    if (*p == '0') p++;
    else if (*p >= '1' && *p <= '9') while (is_digit(*p)) p++;
    else return false;

    if (*p == '.') {
        p++;
        if (!is_digit(*p)) return false;
        while (is_digit(*p)) p++;
    }
    if (*p == 'e' || *p == 'E') {
        p++;
        if (*p == '+' || *p == '-') p++;
        if (!is_digit(*p)) return false;
        while (is_digit(*p)) p++;
    }
    return *p == '\0';
}

void json_number(json_t *j, const char *text)
{
    if (!json_is_number(text)) { fail(j); return; }
    put_scalar(j, text, strlen(text));
}

ssize_t json_finish(json_t *j)
{
    if (j->failed || j->depth != 0 || !j->done) return -1;
    return (ssize_t)j->len;
}
