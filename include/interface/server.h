/* statusd's connection loop (D-043, D-045): a poll() loop that reads one GET request per
 * connection, asks a handler for the response, and writes it. No routes live here; route.c
 * is the handler. One thread, no allocation, no call that blocks.
 *
 * The handler is trusted to be fast and to fill in only the response it is given. The
 * server does not trust what it returns: before anything is sent, the status must be one
 * http.h can write, the content type must exist and be free of control bytes (no response
 * splitting), and the body length must fit the buffer. Otherwise the client gets a fixed
 * 500 and an ERROR line is logged. That cannot stop a handler that overruns the buffer
 * while writing; json_t and make asan are for that.
 *
 * A handler that talks to something slow (the ctl.sock client, later) must bound its own
 * wait: while it waits, every connection waits.
 *
 * Each connection gets SERVER_DEADLINE_MS to deliver its request and the same again to
 * receive the response; trickling bytes does not extend either, so a slow client cannot
 * hold one of the SERVER_MAX_CONNS slots. A connection that arrives when all are busy is
 * closed at once and counted in server_drops.
 */
#ifndef CARWATCH_SERVER_H
#define CARWATCH_SERVER_H

#include "interface/http.h"

#include <stddef.h>
#include <stdint.h>

#define SERVER_MAX_CONNS        4u
#define SERVER_HEAD_MAX         256u     /* room before the body for the response head */
#define SERVER_BODY_MAX         49152u
#define SERVER_DEADLINE_MS      5000u
#define SERVER_ACCEPT_PAUSE_MS  100u     /* after accept fails for lack of fds or memory */

/* What the handler fills in. It arrives preset to an empty 500 text/plain, so a handler
 * that returns without writing anything sends that. body points at SERVER_BODY_MAX bytes
 * the server owns; the handler sets body_len to the bytes it wrote there. */
typedef struct {
    int         status;      /* an http_status_t value */
    const char *ctype;
    char       *body;
    size_t      cap, body_len;
} server_response_t;

typedef void (*server_handler_t)(void *ctx, const http_request_t *req, server_response_t *resp);

typedef enum { CONN_FREE, CONN_READING, CONN_WRITING } server_conn_state_t;

/* Internal to server.c, but visible so a caller can allocate a server_t statically
 * (about 200 KB) and a test can look at the slots. */
typedef struct {
    int                 fd;           /* -1 when free */
    server_conn_state_t state;
    uint64_t            deadline_ms;
    size_t              req_len;
    size_t              out_pos, out_end;   /* the response is buf[out_pos, out_end) */
    char                req[HTTP_MAX_REQUEST];
    char                buf[SERVER_HEAD_MAX + SERVER_BODY_MAX];   /* head, right-aligned, then body */
} server_conn_t;

typedef struct {
    int               listen_fd;
    uint16_t          port;
    server_handler_t  handler;
    void             *handler_ctx;
    uint64_t        (*clock)(void *);   /* milliseconds, monotonic */
    void             *clock_ctx;
    uint64_t          accept_resume_ms;
    uint64_t          last_busy_log_ms;
    uint64_t          served, drops, timeouts;
    server_conn_t     conns[SERVER_MAX_CONNS];
} server_t;

/* Listens on addr:port (port 0 picks one; see server_port). Returns 0, or -1 having logged
 * why. addr is a dotted IPv4 address, never a name. */
int server_open(server_t *s, const char *addr, uint16_t port, server_handler_t h, void *ctx);

/* Replaces the clock, which starts as CLOCK_MONOTONIC. A test installs a fake one so that
 * timeouts need no sleeping; so could a caller that sets the system clock later. */
void server_set_clock(server_t *s, uint64_t (*fn)(void *), void *ctx);

/* One round: waits up to max_wait_ms (less if a deadline is nearer) for something to do,
 * then reaps expired connections, accepts new ones and moves each ready one forward.
 * Returns the number of events handled (0 after a signal interrupted the wait), or -1 if
 * the listening socket failed. */
int server_step(server_t *s, int max_wait_ms);

/* Closes every connection and the listener. Safe to call twice. */
void server_close(server_t *s);

uint16_t server_port(const server_t *s);
uint64_t server_served(const server_t *s);     /* responses sent in full */
uint64_t server_drops(const server_t *s);      /* connections refused for lack of a slot */
uint64_t server_timeouts(const server_t *s);   /* connections closed for being too slow */

#endif
