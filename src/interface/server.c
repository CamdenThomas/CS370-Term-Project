/* statusd's connection loop: see include/interface/server.h. One thread, no allocation. */
#include "interface/server.h"
#include "common/log.h"

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <poll.h>
#include <stdbool.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#define LISTEN_BACKLOG      8
#define ACCEPTS_PER_STEP    16        /* so a flood of connects cannot starve the others */
#define DRAIN_MAX           65536u    /* unread request bytes thrown away before a close */
#define BUSY_LOG_EVERY_MS   1000u

typedef unsigned long long ull;

static const char internal_error_body[] = "internal error\n";

static uint64_t mono_ms(void *ctx)
{
    struct timespec ts;

    (void)ctx;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
        LOG_E("clock_gettime: %s", strerror(errno));
        return 0;
    }
    return (uint64_t)ts.tv_sec * 1000u + (uint64_t)ts.tv_nsec / 1000000u;
}

static uint64_t now_ms(const server_t *s)
{
    return s->clock(s->clock_ctx);
}

/* --------------------------------------------------------------- open, close */

int server_open(server_t *s, const char *addr, uint16_t port, server_handler_t h, void *ctx)
{
    struct sockaddr_in sa;
    socklen_t len = sizeof sa;
    int one = 1;

    memset(s, 0, sizeof *s);
    s->listen_fd = -1;
    for (size_t i = 0; i < SERVER_MAX_CONNS; i++) s->conns[i].fd = -1;
    s->handler = h;
    s->handler_ctx = ctx;
    s->clock = mono_ms;

    memset(&sa, 0, sizeof sa);
    sa.sin_family = AF_INET;
    sa.sin_port = htons(port);
    if (inet_pton(AF_INET, addr, &sa.sin_addr) != 1) {
        LOG_E("not an IPv4 address: %s", addr);
        return -1;
    }

    s->listen_fd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
    if (s->listen_fd < 0) {
        LOG_E("socket: %s", strerror(errno));
        return -1;
    }
    if (setsockopt(s->listen_fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one) < 0) {
        LOG_E("setsockopt SO_REUSEADDR: %s", strerror(errno));
        goto fail;
    }
    if (bind(s->listen_fd, (struct sockaddr *)&sa, sizeof sa) < 0) {
        LOG_E("bind %s:%u: %s", addr, (unsigned)port, strerror(errno));
        goto fail;
    }
    if (listen(s->listen_fd, LISTEN_BACKLOG) < 0) {
        LOG_E("listen: %s", strerror(errno));
        goto fail;
    }
    if (getsockname(s->listen_fd, (struct sockaddr *)&sa, &len) < 0) {
        LOG_E("getsockname: %s", strerror(errno));
        goto fail;
    }
    s->port = ntohs(sa.sin_port);
    LOG_I("listening on %s:%u", addr, (unsigned)s->port);
    return 0;

fail:
    if (close(s->listen_fd) < 0) LOG_W("close listener: %s", strerror(errno));
    s->listen_fd = -1;
    return -1;
}

void server_set_clock(server_t *s, uint64_t (*fn)(void *), void *ctx)
{
    s->clock = fn;
    s->clock_ctx = ctx;
}

static void close_conn(server_conn_t *c)
{
    if (c->fd >= 0 && close(c->fd) < 0 && errno != EINTR)
        LOG_W("close connection: %s", strerror(errno));
    c->fd = -1;
    c->state = CONN_FREE;
    c->req_len = 0;
}

void server_close(server_t *s)
{
    for (size_t i = 0; i < SERVER_MAX_CONNS; i++)
        if (s->conns[i].state != CONN_FREE) close_conn(&s->conns[i]);
    if (s->listen_fd >= 0) {
        if (close(s->listen_fd) < 0) LOG_W("close listener: %s", strerror(errno));
        s->listen_fd = -1;
        LOG_I("stop served=%llu drops=%llu timeouts=%llu", (ull)s->served, (ull)s->drops,
              (ull)s->timeouts);
    }
}

uint16_t server_port(const server_t *s)     { return s->port; }
uint64_t server_served(const server_t *s)   { return s->served; }
uint64_t server_drops(const server_t *s)    { return s->drops; }
uint64_t server_timeouts(const server_t *s) { return s->timeouts; }

/* ------------------------------------------------------------------- writing */

/* Lays the response out in c->buf. The body is already at SERVER_HEAD_MAX; the head goes
 * right-aligned just before it, so the whole response is one run of bytes to send. */
static int finish(server_conn_t *c, int status, const char *ctype, size_t body_len)
{
    char head[SERVER_HEAD_MAX];
    ssize_t h = http_write_head(head, sizeof head, status, ctype, body_len);

    if (h < 0) return -1;
    c->out_pos = SERVER_HEAD_MAX - (size_t)h;
    memcpy(c->buf + c->out_pos, head, (size_t)h);
    c->out_end = SERVER_HEAD_MAX + body_len;
    return 0;
}

/* Throws away whatever the client sent after its request, so that closing does not reset
 * the connection and take our response with it. Best effort, and bounded. */
static void drain(int fd)
{
    char scratch[1024];
    size_t total = 0;

    while (total < DRAIN_MAX) {
        ssize_t r = recv(fd, scratch, sizeof scratch, 0);
        if (r <= 0) break;
        total += (size_t)r;
    }
}

static void conn_write(server_t *s, server_conn_t *c)
{
    while (c->out_pos < c->out_end) {
        ssize_t r = send(c->fd, c->buf + c->out_pos, c->out_end - c->out_pos, MSG_NOSIGNAL);

        if (r > 0) { c->out_pos += (size_t)r; continue; }
        if (r < 0 && errno == EINTR) continue;
        if (r < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) return;   /* wait for POLLOUT */
        LOG_D("send: %s", r < 0 ? strerror(errno) : "wrote nothing");
        close_conn(c);
        return;
    }
    s->served++;
    drain(c->fd);
    if (shutdown(c->fd, SHUT_WR) < 0 && errno != ENOTCONN)
        LOG_D("shutdown: %s", strerror(errno));
    close_conn(c);
}

static void begin_write(server_t *s, server_conn_t *c, uint64_t now)
{
    c->state = CONN_WRITING;
    c->deadline_ms = now + SERVER_DEADLINE_MS;   /* a fresh allowance to receive it */
    conn_write(s, c);
}

static void fail_500(server_conn_t *c)
{
    memcpy(c->buf + SERVER_HEAD_MAX, internal_error_body, sizeof internal_error_body - 1);
    (void)finish(c, HTTP_STATUS_INTERNAL_ERROR, "text/plain", sizeof internal_error_body - 1);
}

static const char *refusal_text(http_parse_t p)
{
    switch (p) {
    case HTTP_BAD_METHOD: return "method not allowed\n";
    case HTTP_TOO_LARGE:  return "request headers too large\n";
    case HTTP_BAD:
    case HTTP_OK:
    case HTTP_INCOMPLETE: break;
    }
    return "bad request\n";
}

static void respond_error(server_t *s, server_conn_t *c, http_parse_t refusal, uint64_t now)
{
    const char *text = refusal_text(refusal);
    size_t n = strlen(text);

    memcpy(c->buf + SERVER_HEAD_MAX, text, n);
    if (finish(c, (int)refusal, "text/plain", n) < 0) fail_500(c);
    begin_write(s, c, now);
}

/* A content type is one header value: no control byte, or a handler could split the
 * response in two. */
static bool ctype_ok(const char *ctype)
{
    if (ctype == NULL) return false;
    for (const unsigned char *p = (const unsigned char *)ctype; *p != '\0'; p++)
        if (*p < 0x20 || *p == 0x7f) return false;
    return true;
}

static void dispatch(server_t *s, server_conn_t *c, const http_request_t *req, uint64_t now)
{
    server_response_t r = { HTTP_STATUS_INTERNAL_ERROR, "text/plain",
                            c->buf + SERVER_HEAD_MAX, SERVER_BODY_MAX, 0 };

    s->handler(s->handler_ctx, req, &r);

    /* The handler is not trusted (server.h). The body is read from our own buffer and
     * length, never from the pointer and cap the handler was handed back. */
    if (r.body_len > SERVER_BODY_MAX || !ctype_ok(r.ctype) ||
        finish(c, r.status, r.ctype, r.body_len) < 0) {
        LOG_E("handler for %s returned an invalid response: status=%d body_len=%zu",
              req->path, r.status, r.body_len);
        fail_500(c);
    }
    begin_write(s, c, now);
}

/* ------------------------------------------------------------------- reading */

static void conn_read(server_t *s, server_conn_t *c, uint64_t now)
{
    size_t room = sizeof c->req - c->req_len;
    http_request_t req;

    if (room == 0) { respond_error(s, c, HTTP_TOO_LARGE, now); return; }
    ssize_t r = recv(c->fd, c->req + c->req_len, room, 0);
    if (r < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) return;
        LOG_D("recv: %s", strerror(errno));
        close_conn(c);
        return;
    }
    if (r == 0) { close_conn(c); return; }    /* the client gave up */

    c->req_len += (size_t)r;
    http_parse_t p = http_parse_request(c->req, c->req_len, &req);
    if (p == HTTP_INCOMPLETE) return;         /* the deadline is not extended */
    if (p == HTTP_OK) dispatch(s, c, &req, now);
    else respond_error(s, c, p, now);
}

/* ---------------------------------------------------------------- the loop */

static server_conn_t *free_slot(server_t *s)
{
    for (size_t i = 0; i < SERVER_MAX_CONNS; i++)
        if (s->conns[i].state == CONN_FREE) return &s->conns[i];
    return NULL;
}

static void accept_new(server_t *s, uint64_t now)
{
    for (int i = 0; i < ACCEPTS_PER_STEP; i++) {
        int fd = accept4(s->listen_fd, NULL, NULL, SOCK_NONBLOCK | SOCK_CLOEXEC);

        if (fd < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) return;
            if (errno == EINTR || errno == ECONNABORTED) continue;
            /* Out of descriptors or memory: the connection stays queued, so polling the
             * listener again at once would spin. Leave it alone for a moment. */
            LOG_W("accept: %s; pausing %u ms", strerror(errno), SERVER_ACCEPT_PAUSE_MS);
            s->accept_resume_ms = now + SERVER_ACCEPT_PAUSE_MS;
            return;
        }

        server_conn_t *c = free_slot(s);
        if (c == NULL) {
            s->drops++;
            if (s->drops == 1 || now >= s->last_busy_log_ms + BUSY_LOG_EVERY_MS) {
                LOG_W("all %u slots busy; refusing connections (%llu so far)", SERVER_MAX_CONNS,
                      (ull)s->drops);
                s->last_busy_log_ms = now;
            }
            if (close(fd) < 0) LOG_W("close refused connection: %s", strerror(errno));
            continue;
        }
        c->fd = fd;
        c->state = CONN_READING;
        c->deadline_ms = now + SERVER_DEADLINE_MS;
        c->req_len = 0;
        conn_read(s, c, now);                 /* the request may already be here */
    }
}

static void reap(server_t *s, uint64_t now)
{
    for (size_t i = 0; i < SERVER_MAX_CONNS; i++) {
        server_conn_t *c = &s->conns[i];

        if (c->state == CONN_FREE || now < c->deadline_ms) continue;
        LOG_W("timeout in the %s phase", c->state == CONN_READING ? "read" : "write");
        s->timeouts++;
        close_conn(c);
    }
}

int server_step(server_t *s, int max_wait_ms)
{
    struct pollfd pfd[1 + SERVER_MAX_CONNS];
    server_conn_t *who[1 + SERVER_MAX_CONNS];       /* NULL for the listener */
    nfds_t n = 0;
    uint64_t now = now_ms(s);
    uint64_t wait = max_wait_ms > 0 ? (uint64_t)max_wait_ms : 0;
    int handled = 0;

    if (s->listen_fd < 0) return -1;
    if (now >= s->accept_resume_ms) {
        pfd[n] = (struct pollfd){ .fd = s->listen_fd, .events = POLLIN };
        who[n++] = NULL;
    } else if (s->accept_resume_ms - now < wait) {
        wait = s->accept_resume_ms - now;
    }
    for (size_t i = 0; i < SERVER_MAX_CONNS; i++) {
        server_conn_t *c = &s->conns[i];
        if (c->state == CONN_FREE) continue;

        pfd[n] = (struct pollfd){ .fd = c->fd,
                                  .events = c->state == CONN_READING ? POLLIN : POLLOUT };
        who[n++] = c;
        uint64_t left = c->deadline_ms > now ? c->deadline_ms - now : 0;
        if (left < wait) wait = left;
    }

    if (poll(pfd, n, (int)wait) < 0) {
        if (errno == EINTR) return 0;               /* the caller checks its stop flag */
        LOG_E("poll: %s", strerror(errno));
        return -1;
    }

    now = now_ms(s);                                /* after the wait, not before it */
    reap(s, now);
    for (nfds_t i = 0; i < n; i++) {
        short ev = pfd[i].revents;
        server_conn_t *c = who[i];

        if (ev == 0) continue;
        if (c == NULL) {
            if (ev & (POLLERR | POLLNVAL)) {
                LOG_E("the listening socket failed (revents=%#x)", (unsigned)ev);
                return -1;
            }
            accept_new(s, now);
            handled++;
            continue;
        }
        if (c->state == CONN_FREE) continue;        /* reaped just now */
        handled++;
        if (c->state == CONN_READING) conn_read(s, c, now);
        else conn_write(s, c);
    }
    return handled;
}
