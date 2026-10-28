/* statusd's routes (D-043, D-045): the handler that server.c calls for each request. It
 * maps a URL to one request line for the source, and turns the source's reply into JSON.
 * No sockets and no allocation; the 64 KB scratch buffer is the caller's.
 *
 *   /                      the status page (page.c)
 *   /api/status            status
 *   /api/verdicts          verdicts
 *   /api/faults            faults
 *   /api/series?pid=&from=&to=
 *                          series pid="..." from=... to=...
 *
 * Success is 200 with {"ok":true,"records":[{"tag":"proc","name":"obdd","pid":102,...}]},
 * each field typed as the source typed it (kv.h). Every refusal under /api/ is JSON too,
 * {"ok":false,"code":"usage","reason":"..."}, so the page can always parse a body.
 *
 * Whatever the URL carries is untrusted, and a request line is built from it. So query
 * keys are checked against a fixed list, each value must be 1 to 24 bytes of
 * [A-Za-z0-9:+.,_/-] after decoding, and anything else is a 400 that never reaches the
 * source. The routes take no other query: an unexpected key, a repeated one, or any query
 * on a route that takes none is a 400. A "+" in a URL decodes to a space, so a client that
 * means a plus sends %2B.
 */
#ifndef CARWATCH_ROUTE_H
#define CARWATCH_ROUTE_H

#include "interface/http.h"
#include "interface/server.h"
#include "interface/source.h"

#define ROUTE_REPLY_MAX  65536u

typedef struct {
    const source_t *src;
    char            reply[ROUTE_REPLY_MAX];   /* the source's text, before it becomes JSON */
} route_ctx_t;

/* A server_handler_t: ctx is a route_ctx_t whose src is set. */
void route_handle(void *ctx, const http_request_t *req, server_response_t *resp);

#endif
