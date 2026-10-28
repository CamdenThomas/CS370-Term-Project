/* statusd's routes: see include/interface/route.h. No sockets, no allocation. */
#include "interface/route.h"
#include "interface/json.h"
#include "interface/kv.h"
#include "interface/page.h"
#include "common/log.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#define ROUTE_VALUE_MAX   24u    /* a query value, after decoding */
#define ROUTE_LINE_MAX    160u   /* the request line built from a URL */

static const struct { const char *path, *verb; } api[] = {
    { "/api/status",   "status" },
    { "/api/verdicts", "verdicts" },
    { "/api/faults",   "faults" },
    { "/api/series",   "series" },
};
#define NAPI (sizeof api / sizeof api[0])

/* The query keys /api/series takes, in the order they go into the request line. */
static const char *const series_keys[] = { "pid", "from", "to" };
#define NSERIES_KEYS (sizeof series_keys / sizeof series_keys[0])

/* --------------------------------------------------------------- responses */

static void set_empty_500(server_response_t *r)
{
    r->status = HTTP_STATUS_INTERNAL_ERROR;
    r->ctype = "text/plain";
    r->body_len = 0;
}

static void set_text(server_response_t *r, int status, const char *ctype, const char *text)
{
    size_t n = strlen(text);

    if (n > r->cap) { set_empty_500(r); return; }
    memcpy(r->body, text, n);
    r->status = status;
    r->ctype = ctype;
    r->body_len = n;
}

/* {"ok":false,"code":"...","reason":"..."} */
static void set_error(server_response_t *r, int status, const char *code, const char *reason)
{
    json_t j;

    json_init(&j, r->body, r->cap);
    json_begin_object(&j);
    json_key(&j, "ok");
    json_bool(&j, false);
    json_key(&j, "code");
    json_string(&j, code);
    json_key(&j, "reason");
    json_string(&j, reason);
    json_end_object(&j);
    ssize_t n = json_finish(&j);
    if (n < 0) { set_empty_500(r); return; }
    r->status = status;
    r->ctype = "application/json";
    r->body_len = (size_t)n;
}

static int status_for_code(const char *code)
{
    if (strcmp(code, "usage") == 0) return HTTP_STATUS_BAD_REQUEST;
    if (strcmp(code, "unknown_pid") == 0 || strcmp(code, "unknown_verb") == 0)
        return HTTP_STATUS_NOT_FOUND;
    if (strcmp(code, "unavailable") == 0) return HTTP_STATUS_UNAVAILABLE;
    return HTTP_STATUS_INTERNAL_ERROR;
}

/* The source's records as {"ok":true,"records":[{"tag":...,<fields>}]}. Returns false, with
 * the reason in *why, if the reply cannot be shown. */
static bool records_to_json(server_response_t *r, const char *reply, size_t n, const char **why)
{
    const char *cur = reply, *end = reply + n;
    kv_line_t rec;
    json_t j;

    json_init(&j, r->body, r->cap);
    json_begin_object(&j);
    json_key(&j, "ok");
    json_bool(&j, true);
    json_key(&j, "records");
    json_begin_array(&j);
    while (kv_reply_next(&cur, end, &rec) == KV_RECORD) {
        if (kv_get(&rec, "tag") != NULL) {
            LOG_E("source record %s has a field called tag", rec.tag);
            *why = "source sent a field named tag";
            return false;
        }
        json_begin_object(&j);
        json_key(&j, "tag");
        json_string(&j, rec.tag);
        for (size_t i = 0; i < rec.nfields; i++) {
            json_key(&j, rec.f[i].key);
            switch (rec.f[i].type) {
            case KV_STR:  json_string(&j, rec.f[i].val); break;
            case KV_NUM:  json_number(&j, rec.f[i].val); break;
            case KV_NULL: json_null(&j); break;
            }
        }
        json_end_object(&j);
    }
    json_end_array(&j);
    json_end_object(&j);

    ssize_t len = json_finish(&j);
    if (len < 0) {
        LOG_E("a reply of %zu bytes does not fit a %zu-byte response", n, r->cap);
        *why = "response did not fit";
        return false;
    }
    r->status = HTTP_STATUS_OK;
    r->ctype = "application/json";
    r->body_len = (size_t)len;
    return true;
}

/* ------------------------------------------------------------ the request line */

/* A query value is one token of a line sent to the source: nothing that could end a
 * string, start a field or a line, or surprise a parser. */
static bool safe_value(const char *v)
{
    size_t n = strlen(v);

    if (n == 0 || n > ROUTE_VALUE_MAX) return false;
    for (size_t i = 0; i < n; i++) {
        char c = v[i];
        bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
                  strchr(":+.,_/-", c) != NULL;
        if (!ok) return false;
    }
    return true;
}

/* Every key must be one of series_keys, at most once, and no pair may be empty. On success
 * sets bit i of *seen for each series_keys[i] present and returns NULL; else the reason. */
static const char *check_keys(const char *query, unsigned *seen)
{
    *seen = 0;
    for (const char *p = query; *p != '\0'; ) {
        size_t pair = strcspn(p, "&");
        size_t klen = strcspn(p, "=&");
        size_t k = 0;

        while (k < NSERIES_KEYS && !(strlen(series_keys[k]) == klen &&
                                     memcmp(p, series_keys[k], klen) == 0)) k++;
        if (k == NSERIES_KEYS) return "unknown query key";
        if (*seen & (1u << k)) return "repeated query key";
        *seen |= 1u << k;

        p += pair;
        if (*p == '&' && *++p == '\0') return "empty query pair";
    }
    return NULL;
}

/* series pid="0C" from="-5m" to=1800000000. pid is always a string (PID 04 has a leading
 * zero, and is no number); from and to are bare when they are all digits, epoch seconds.
 * Returns NULL, or the reason the URL was refused. */
static const char *build_series(const char *query, char *line, size_t cap)
{
    unsigned seen;
    const char *why = check_keys(query, &seen);
    size_t len = (size_t)snprintf(line, cap, "series");

    if (why != NULL) return why;
    for (size_t k = 0; k < NSERIES_KEYS; k++) {
        char v[HTTP_QUERY_MAX];

        if (!(seen & (1u << k))) continue;
        if (http_query_get(query, series_keys[k], v, sizeof v) != 0) return "bad query value";
        if (!safe_value(v)) return "unsafe query value";

        bool bare = k != 0 && strspn(v, "0123456789") == strlen(v);
        int w = snprintf(line + len, cap - len, bare ? " %s=%s" : " %s=\"%s\"", series_keys[k], v);
        if (w < 0 || (size_t)w >= cap - len) return "query too long";
        len += (size_t)w;
    }
    return NULL;
}

/* ------------------------------------------------------------------- handler */

void route_handle(void *ctx, const http_request_t *req, server_response_t *resp)
{
    route_ctx_t *rc = ctx;
    char line[ROUTE_LINE_MAX];
    const char *why = NULL;
    size_t found = NAPI;

    if (strcmp(req->path, "/") == 0) {
        if (req->query[0] != '\0') set_text(resp, HTTP_STATUS_BAD_REQUEST, "text/plain", "bad request\n");
        else set_text(resp, HTTP_STATUS_OK, "text/html; charset=utf-8", page_html);
        return;
    }
    if (strncmp(req->path, "/api/", 5) != 0) {
        set_text(resp, HTTP_STATUS_NOT_FOUND, "text/plain", "not found\n");
        return;
    }
    for (size_t i = 0; i < NAPI; i++)
        if (strcmp(req->path, api[i].path) == 0) found = i;
    if (found == NAPI) {
        set_error(resp, HTTP_STATUS_NOT_FOUND, "not_found", "no such route");
        return;
    }

    if (strcmp(api[found].verb, "series") == 0) {
        why = build_series(req->query, line, sizeof line);
    } else if (req->query[0] != '\0') {
        why = "this route takes no query";
    } else {
        snprintf(line, sizeof line, "%s", api[found].verb);
    }
    if (why != NULL) {
        set_error(resp, HTTP_STATUS_BAD_REQUEST, "usage", why);
        return;
    }

    ssize_t n = source_query(rc->src, line, rc->reply, sizeof rc->reply);
    if (n < 0) {
        LOG_E("the source's reply to \"%s\" did not fit %u bytes", line, ROUTE_REPLY_MAX);
        set_error(resp, HTTP_STATUS_INTERNAL_ERROR, "internal", "reply did not fit");
        return;
    }

    kv_line_t final;
    switch (kv_reply_check(rc->reply, (size_t)n, &final)) {
    case KV_OK:
        if (!records_to_json(resp, rc->reply, (size_t)n, &why))
            set_error(resp, HTTP_STATUS_INTERNAL_ERROR, "internal", why);
        return;
    case KV_ERR:
        set_error(resp, status_for_code(final.err_code), final.err_code, final.err_reason);
        return;
    case KV_RECORD:
    case KV_BAD:
    case KV_INCOMPLETE:
        break;
    }
    LOG_E("the source's reply to \"%s\" is malformed", line);
    set_error(resp, HTTP_STATUS_INTERNAL_ERROR, "internal", "malformed source reply");
}
