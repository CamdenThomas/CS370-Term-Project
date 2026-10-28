/* statusd's connection loop over real loopback sockets, in one thread: the test steps the
 * server and reads the client in turn, and a fake clock moves time without sleeping. */
#include "interface/server.h"
#include "common/log.h"

#include <arpa/inet.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <unistd.h>

static int failures;

#define CHECK(cond, msg) do { \
    if (!(cond)) { fprintf(stderr, "  FAIL %s:%d: %s\n", __FILE__, __LINE__, msg); \
                   failures++; } } while (0)

static server_t srv;
static uint16_t port;
static uint64_t fake_now;

static uint64_t fake_clock(void *ctx) { (void)ctx; return fake_now; }

/* ------------------------------------------------------------------ handler */

static void fill(server_response_t *r, int status, const char *text)
{
    r->status = status;
    r->ctype = "text/plain";
    r->body_len = (size_t)snprintf(r->body, r->cap, "%s", text);
}

static void stub(void *ctx, const http_request_t *req, server_response_t *r)
{
    (void)ctx;
    if (strcmp(req->path, "/missing") == 0) {
        fill(r, HTTP_STATUS_NOT_FOUND, "nope\n");
    } else if (strcmp(req->path, "/big") == 0) {
        r->status = HTTP_STATUS_OK;
        r->ctype = "application/octet-stream";
        for (size_t i = 0; i < r->cap; i++) r->body[i] = (char)('a' + i % 26);
        r->body_len = r->cap;
    } else if (strcmp(req->path, "/bad") == 0) {
        fill(r, 418, "teapot\n");
    } else if (strcmp(req->path, "/noctype") == 0) {
        fill(r, HTTP_STATUS_OK, "x\n");
        r->ctype = NULL;
    } else if (strcmp(req->path, "/crlf") == 0) {
        fill(r, HTTP_STATUS_OK, "x\n");
        r->ctype = "text/plain\r\nX-Injected: yes";
    } else if (strcmp(req->path, "/over") == 0) {
        fill(r, HTTP_STATUS_OK, "x\n");
        r->body_len = r->cap + 1;
    } else if (strcmp(req->path, "/untouched") == 0) {
        return;
    } else {
        char text[HTTP_PATH_MAX + HTTP_QUERY_MAX + 32];
        snprintf(text, sizeof text, "path=%s query=%s\n", req->path, req->query);
        fill(r, HTTP_STATUS_OK, text);
    }
}

/* ------------------------------------------------------------------- client */

static int dial(int rcvbuf)
{
    int fd = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
    struct sockaddr_in a = { .sin_family = AF_INET, .sin_port = htons(port) };

    if (fd < 0) { perror("socket"); exit(2); }
    if (rcvbuf > 0) (void)setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &rcvbuf, sizeof rcvbuf);
    inet_pton(AF_INET, "127.0.0.1", &a.sin_addr);
    if (connect(fd, (struct sockaddr *)&a, sizeof a) < 0) { perror("connect"); exit(2); }
    (void)fcntl(fd, F_SETFL, O_NONBLOCK);
    return fd;
}

static void send_n(int fd, const char *s, size_t n)
{
    CHECK(send(fd, s, n, MSG_NOSIGNAL) == (ssize_t)n, "client send");
}

static void send_str(int fd, const char *s) { send_n(fd, s, strlen(s)); }

/* Steps the server and reads fd, up to `steps` rounds or until the server closes it. */
static size_t slurp(int fd, char *out, size_t cap, int steps, bool *closed)
{
    size_t len = 0;

    *closed = false;
    for (int i = 0; i < steps && !*closed; i++) {
        CHECK(server_step(&srv, 0) >= 0, "server_step");
        for (;;) {
            ssize_t r = recv(fd, out + len, cap - 1 - len, 0);
            if (r > 0) { len += (size_t)r; if (len == cap - 1) break; continue; }
            if (r == 0 || (errno != EAGAIN && errno != EWOULDBLOCK)) *closed = true;
            break;
        }
    }
    out[len] = '\0';
    return len;
}

static char resp[SERVER_BODY_MAX + 2048];

/* One request on a fresh connection; the response lands in resp. */
static size_t exchange(const char *request)
{
    bool closed;
    int fd = dial(0);

    send_str(fd, request);
    size_t n = slurp(fd, resp, sizeof resp, 300, &closed);
    CHECK(closed, "the server closed the connection after the response");
    close(fd);
    return n;
}

static int status_of(const char *r) { return strncmp(r, "HTTP/1.1 ", 9) == 0 ? atoi(r + 9) : -1; }

static const char *body_of(const char *r)
{
    const char *p = strstr(r, "\r\n\r\n");
    return p != NULL ? p + 4 : "";
}

/* True once the client's socket reads end-of-file (the server closed it). */
static bool is_closed(int fd)
{
    char c;
    ssize_t r = recv(fd, &c, 1, MSG_PEEK);
    return r == 0 || (r < 0 && errno != EAGAIN && errno != EWOULDBLOCK);
}

static void open_server(void)
{
    CHECK(server_open(&srv, "127.0.0.1", 0, stub, NULL) == 0, "server_open");
    port = server_port(&srv);
    CHECK(port != 0, "an ephemeral port was chosen");
}

/* ------------------------------------------------------------- fd bookkeeping */

#define MAX_FD 1024

static void snapshot(bool set[MAX_FD])
{
    DIR *d = opendir("/proc/self/fd");
    struct dirent *e;

    memset(set, 0, MAX_FD);
    if (d == NULL) { perror("opendir"); exit(2); }
    int self = dirfd(d);
    while ((e = readdir(d)) != NULL) {
        int fd = atoi(e->d_name);
        if (e->d_name[0] != '.' && fd != self && fd < MAX_FD) set[fd] = true;
    }
    closedir(d);
}

/* ------------------------------------------------------------------- tests */

static void test_open_close(void)
{
    static bool before[MAX_FD], after[MAX_FD];
    server_t other;

    snapshot(before);
    CHECK(server_open(&srv, "999.1.1.1", 0, stub, NULL) == -1, "a bad address");
    CHECK(server_open(&srv, "localhost", 0, stub, NULL) == -1, "a name is not an address");

    open_server();
    CHECK(server_open(&other, "127.0.0.1", port, stub, NULL) == -1, "the port is taken");
    server_close(&srv);
    server_close(&srv);
    CHECK(server_step(&srv, 0) == -1, "a closed server has nothing to step");
    snapshot(after);
    CHECK(memcmp(before, after, MAX_FD) == 0, "no descriptor survives server_close");
}

static void test_descriptors_are_close_on_exec_and_not_leaked(void)
{
    static bool before[MAX_FD], during[MAX_FD], after[MAX_FD];

    snapshot(before);
    open_server();
    int a = dial(0), b = dial(0);
    CHECK(server_step(&srv, 0) >= 0 && server_step(&srv, 0) >= 0, "accepted two");
    snapshot(during);
    int fresh = 0;
    for (int fd = 3; fd < MAX_FD; fd++) {
        if (!during[fd] || before[fd]) continue;
        fresh++;
        int fl = fcntl(fd, F_GETFD);
        CHECK(fl >= 0 && (fl & FD_CLOEXEC), "every new descriptor is close-on-exec");
    }
    CHECK(fresh == 5, "a listener, two accepted and two client descriptors");

    close(a); close(b);
    for (int i = 0; i < 4; i++) server_step(&srv, 0);
    server_close(&srv);
    snapshot(after);
    CHECK(memcmp(before, after, MAX_FD) == 0, "nothing leaks");
}

static void test_get(void)
{
    open_server();
    size_t n = exchange("GET /hello?x=1 HTTP/1.1\r\nHost: box\r\n\r\n");

    CHECK(n > 0 && status_of(resp) == 200, "200");
    CHECK(strcmp(body_of(resp), "path=/hello query=x=1\n") == 0, "the handler saw the request");
    CHECK(strstr(resp, "\r\nContent-Length: 22\r\n") != NULL, "Content-Length is the body's");
    CHECK(strstr(resp, "\r\nConnection: close\r\n") != NULL, "Connection: close");
    CHECK(strstr(resp, "\r\nCache-Control: no-store\r\n") != NULL, "no-store");
    CHECK(strstr(resp, "\r\nContent-Type: text/plain\r\n") != NULL, "the handler's type");
    CHECK(server_served(&srv) == 1, "one response served");

    exchange("GET /missing HTTP/1.1\r\n\r\n");
    CHECK(status_of(resp) == 404 && strcmp(body_of(resp), "nope\n") == 0, "the handler's 404");

    for (int i = 0; i < 20; i++) {
        exchange("GET /again HTTP/1.1\r\n\r\n");
        CHECK(status_of(resp) == 200, "repeat");
    }
    CHECK(server_served(&srv) == 22, "twenty-two served, slots reused");
    server_close(&srv);
}

static void test_framing(void)
{
    bool closed;
    open_server();

    int fd = dial(0);
    send_str(fd, "GET /sp");
    server_step(&srv, 0);
    send_str(fd, "lit HTTP/1.1\r\n");
    server_step(&srv, 0);
    CHECK(!is_closed(fd), "still waiting for the rest");
    send_str(fd, "\r\n");
    slurp(fd, resp, sizeof resp, 100, &closed);
    CHECK(status_of(resp) == 200 && strstr(resp, "path=/split") != NULL,
          "a request split across three sends");
    close(fd);

    exchange("GET /a HTTP/1.1\r\n\r\nGET /b HTTP/1.1\r\n\r\n");
    CHECK(strstr(resp, "path=/a") != NULL && strstr(resp, "path=/b") == NULL,
          "a second request on the same connection is not served");
    CHECK(strstr(resp + 9, "HTTP/1.1") == NULL, "and there is only one response");
    server_close(&srv);
}

static void test_refusals(void)
{
    static char big[7000];

    open_server();
    exchange("POST / HTTP/1.1\r\n\r\n");
    CHECK(status_of(resp) == 405 && strstr(resp, "\r\nAllow: GET\r\n") != NULL, "POST");
    exchange("HEAD / HTTP/1.1\r\n\r\n");
    CHECK(status_of(resp) == 405 && strstr(resp, "\r\nAllow: GET\r\n") != NULL, "HEAD");
    exchange("GARBAGE\r\n\r\n");
    CHECK(status_of(resp) == 400, "garbage");
    exchange("GET /a/../b HTTP/1.1\r\n\r\n");
    CHECK(status_of(resp) == 400, "a path that climbs");

    memset(big, 'a', sizeof big - 1);
    memcpy(big, "GET / HTTP/1.1\r\nX: ", 19);
    exchange(big);
    CHECK(status_of(resp) == 431, "a header block with no end");
    CHECK(server_served(&srv) == 5, "refusals are responses too");
    server_close(&srv);
}

static void test_handler_is_not_trusted(void)
{
    open_server();
    static const char *paths[] = { "/bad", "/noctype", "/crlf", "/over" };
    for (size_t i = 0; i < sizeof paths / sizeof paths[0]; i++) {
        char req[64];
        snprintf(req, sizeof req, "GET %s HTTP/1.1\r\n\r\n", paths[i]);
        exchange(req);
        CHECK(status_of(resp) == 500, paths[i]);
        CHECK(strcmp(body_of(resp), "internal error\n") == 0, "a fixed body, not the handler's");
        CHECK(strstr(resp, "X-Injected") == NULL, "no injected header");
    }
    exchange("GET /untouched HTTP/1.1\r\n\r\n");
    CHECK(status_of(resp) == 500 && body_of(resp)[0] == '\0',
          "a handler that writes nothing sends the preset empty 500");
    server_close(&srv);
}

/* The server's own send buffer is shrunk through the slot's fd, so a 48 KB response cannot
 * leave in one send. */
static int dial_with_small_sndbuf(void)
{
    int fd = dial(2048);
    int small = 2048;

    server_step(&srv, 0);
    CHECK(srv.conns[0].fd >= 0, "the server accepted it");
    (void)setsockopt(srv.conns[0].fd, SOL_SOCKET, SO_SNDBUF, &small, sizeof small);
    return fd;
}

static void test_big_body_in_pieces(void)
{
    open_server();
    int fd = dial_with_small_sndbuf();

    send_str(fd, "GET /big HTTP/1.1\r\n\r\n");
    server_step(&srv, 0);
    CHECK(srv.conns[0].state == CONN_WRITING, "the response did not fit in one send");

    bool closed;
    size_t n = slurp(fd, resp, sizeof resp, 100000, &closed);
    CHECK(closed && status_of(resp) == 200, "it arrived");
    const char *body = body_of(resp);
    CHECK(n - (size_t)(body - resp) == SERVER_BODY_MAX, "the whole body");
    bool intact = true;
    for (size_t i = 0; i < SERVER_BODY_MAX; i++)
        if (body[i] != (char)('a' + i % 26)) { intact = false; break; }
    CHECK(intact, "and every byte of it");
    close(fd);
    server_close(&srv);
}

static void test_timeouts(void)
{
    open_server();
    server_set_clock(&srv, fake_clock, NULL);

    /* A client that sends nothing. */
    fake_now = 1000;
    int idle = dial(0);
    server_step(&srv, 0);
    fake_now = 1000 + SERVER_DEADLINE_MS - 1;
    server_step(&srv, 0);
    CHECK(!is_closed(idle), "open just before the deadline");
    fake_now = 1000 + SERVER_DEADLINE_MS;
    server_step(&srv, 0);
    CHECK(is_closed(idle), "closed at the deadline");
    CHECK(server_timeouts(&srv) == 1, "counted");
    close(idle);

    /* Half a request, then a trickle: neither extends the deadline. */
    fake_now = 20000;
    int slow = dial(0);
    server_step(&srv, 0);
    send_str(slow, "GET / HT");
    server_step(&srv, 0);
    fake_now = 24000;
    send_str(slow, "T");
    server_step(&srv, 0);
    CHECK(!is_closed(slow), "still open at 4 s");
    fake_now = 25000;
    send_str(slow, "P");
    server_step(&srv, 0);
    CHECK(is_closed(slow), "closed 5 s after it connected, bytes or not");
    CHECK(server_timeouts(&srv) == 2, "counted again");
    close(slow);

    /* A client that never reads its response. The request arrives 4 s after it connected,
     * and the response still gets a whole new allowance, counted from there. */
    fake_now = 40000;
    int deaf = dial_with_small_sndbuf();
    fake_now = 44000;
    send_str(deaf, "GET /big HTTP/1.1\r\n\r\n");
    server_step(&srv, 0);
    CHECK(srv.conns[0].state == CONN_WRITING, "stuck writing");
    fake_now = 44000 + SERVER_DEADLINE_MS - 1;
    server_step(&srv, 0);
    CHECK(srv.conns[0].state == CONN_WRITING, "still writing just before the deadline");
    fake_now = 44000 + SERVER_DEADLINE_MS;
    server_step(&srv, 0);
    CHECK(srv.conns[0].state == CONN_FREE, "reaped in the write phase");
    CHECK(server_timeouts(&srv) == 3, "counted a third time");
    close(deaf);
    server_close(&srv);
}

static void test_slots(void)
{
    int held[SERVER_MAX_CONNS];

    open_server();
    for (unsigned i = 0; i < SERVER_MAX_CONNS; i++) held[i] = dial(0);
    server_step(&srv, 0);

    int extra = dial(0);
    server_step(&srv, 0);
    CHECK(is_closed(extra), "a fifth connection is closed at once");
    CHECK(server_drops(&srv) == 1, "and counted");
    close(extra);

    close(held[0]);
    server_step(&srv, 0);
    server_step(&srv, 0);
    exchange("GET /after HTTP/1.1\r\n\r\n");
    CHECK(status_of(resp) == 200, "a slot freed by a departure is reused");
    for (unsigned i = 1; i < SERVER_MAX_CONNS; i++) close(held[i]);
    server_close(&srv);
}

/* With no descriptor to give, accept fails and the connection stays queued, so the
 * listener stays readable. Without a pause the loop would spin on it. */
static void test_accept_pause(void)
{
    struct rlimit saved, low;
    bool closed;

    open_server();
    server_set_clock(&srv, fake_clock, NULL);
    fake_now = 50000;

    int fd = dial(0);
    int probe = dup(0);                 /* the number accept4 would be given */
    CHECK(probe >= 0, "dup");
    close(probe);
    CHECK(getrlimit(RLIMIT_NOFILE, &saved) == 0, "getrlimit");
    low = saved;
    low.rlim_cur = (rlim_t)probe;
    CHECK(setrlimit(RLIMIT_NOFILE, &low) == 0, "lower the descriptor limit");

    send_str(fd, "GET /late HTTP/1.1\r\n\r\n");
    CHECK(server_step(&srv, 0) == 1, "accept fails for lack of a descriptor");
    CHECK(server_step(&srv, 0) == 0, "and the listener is then left alone");
    CHECK(setrlimit(RLIMIT_NOFILE, &saved) == 0, "restore the limit");

    fake_now += SERVER_ACCEPT_PAUSE_MS - 1;
    CHECK(server_step(&srv, 0) == 0, "still paused one millisecond early");
    fake_now += 1;
    slurp(fd, resp, sizeof resp, 100, &closed);
    CHECK(status_of(resp) == 200 && strstr(resp, "path=/late") != NULL,
          "the queued connection is served once accepting resumes");
    close(fd);
    server_close(&srv);
}

static void test_clients_that_vanish(void)
{
    bool closed;

    open_server();
    for (int i = 0; i < 12; i++) {
        int fd = dial(0);
        send_str(fd, "GET /x HTT");
        server_step(&srv, 0);
        close(fd);
        server_step(&srv, 0);
        server_step(&srv, 0);
    }
    for (int i = 0; i < 6; i++) {
        int fd = dial(2048);
        send_str(fd, "GET /big HTTP/1.1\r\n\r\n");
        server_step(&srv, 0);
        close(fd);
        for (int k = 0; k < 20; k++) server_step(&srv, 0);
    }
    for (unsigned i = 0; i < SERVER_MAX_CONNS; i++)
        CHECK(srv.conns[i].state == CONN_FREE, "no slot is stuck");

    int fd = dial(0);
    send_str(fd, "GET /ok HTTP/1.1\r\n\r\n");
    slurp(fd, resp, sizeof resp, 100, &closed);
    CHECK(status_of(resp) == 200, "and the server still answers");
    close(fd);
    server_close(&srv);
}

int main(void)
{
    log_init("test_server", LOG_FATAL);
    signal(SIGPIPE, SIG_IGN);
    test_open_close();
    test_descriptors_are_close_on_exec_and_not_leaked();
    test_get();
    test_framing();
    test_refusals();
    test_handler_is_not_trusted();
    test_big_body_in_pieces();
    test_timeouts();
    test_slots();
    test_accept_pause();
    test_clients_that_vanish();
    return failures == 0 ? 0 : 1;
}
