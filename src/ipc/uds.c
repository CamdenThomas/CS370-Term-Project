/* The event socket (D-042; include/ipc/uds.h). The only file outside src/interface/ that
 * may call socket(): `make boundary` checks it. AF_UNIX only. */
#include "ipc/uds.h"

#include "common/log.h"

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

_Static_assert(EV_PATH_MAX == sizeof(((struct sockaddr_un *)0)->sun_path),
               "EV_PATH_MAX is sun_path's size");

static void close_fd(int fd, const char *what)
{
    if (fd >= 0 && close(fd) != 0)
        LOG_W("event_close_failed what=%s errno=%d (%s)", what, errno, strerror(errno));
}

static bool make_addr(struct sockaddr_un *a, const char *path)
{
    memset(a, 0, sizeof *a);
    a->sun_family = AF_UNIX;
    size_t n = strlen(path);
    if (n == 0 || n >= sizeof a->sun_path) return false;
    memcpy(a->sun_path, path, n);
    return true;
}

/* --- sender ----------------------------------------------------------------------------- */

void ev_sender_init(ev_sender_t *s, const char *path)
{
    memset(s, 0, sizeof *s);
    s->fd = -1;
    snprintf(s->path, sizeof s->path, "%s", path);
}

void ev_sender_close(ev_sender_t *s)
{
    if (s->n > 0) LOG_W("event_unsent count=%zu dropped=%llu", s->n,
                        (unsigned long long)s->dropped);
    close_fd(s->fd, "sender");
    s->fd = -1;
}

static bool try_connect(ev_sender_t *s)
{
    struct sockaddr_un a;
    if (!make_addr(&a, s->path)) {
        if (!s->down_logged) LOG_E("event_socket_path_invalid path=%s", s->path);
        s->down_logged = true;
        return false;
    }
    int fd = socket(AF_UNIX, SOCK_SEQPACKET | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
    if (fd < 0) {
        if (!s->down_logged)
            LOG_E("event_socket_failed errno=%d (%s)", errno, strerror(errno));
        s->down_logged = true;
        return false;
    }
    if (connect(fd, (const struct sockaddr *)&a, sizeof a) != 0) {
        /* storaged down or restarting: queue and try at the next send. Said once. */
        if (!s->down_logged)
            LOG_W("event_socket_down path=%s errno=%d (%s) queued=%zu", s->path, errno,
                  strerror(errno), s->n);
        s->down_logged = true;
        close_fd(fd, "connect");
        return false;
    }
    if (s->down_logged) LOG_I("event_socket_up path=%s queued=%zu", s->path, s->n);
    s->down_logged = false;
    s->fd = fd;
    return true;
}

size_t ev_flush(ev_sender_t *s)
{
    int reconnects = 0;
    while (s->n > 0) {
        if (s->fd < 0 && (reconnects++ >= 2 || !try_connect(s))) break;
        const ev_t *e = &s->q[s->head];
        uint8_t pkt[EV_PACKET_MAX];
        pkt[0] = (uint8_t)(e->code & 0xFFu);
        pkt[1] = (uint8_t)(e->code >> 8);
        pkt[2] = e->src;
        memcpy(pkt + 3, e->text, e->len);
        size_t len = 3u + e->len;

        ssize_t w = send(s->fd, pkt, len, MSG_DONTWAIT | MSG_NOSIGNAL);
        if (w == (ssize_t)len) {
            s->head = (s->head + 1) % EV_QUEUE;
            s->n--;
            s->sent++;
            continue;
        }
        if (w < 0 && errno == EINTR) continue;
        if (w < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) break;   /* storaged busy */
        /* EPIPE, ECONNRESET: storaged went away. Reconnect, and keep the event. */
        LOG_W("event_send_failed errno=%d (%s)", w < 0 ? errno : 0,
              w < 0 ? strerror(errno) : "short send");
        close_fd(s->fd, "sender");
        s->fd = -1;
    }
    return s->n;
}

void ev_send(ev_sender_t *s, uint16_t code, uint8_t src, const char *fmt, ...)
{
    char text[EV_TEXT_MAX + 1];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(text, sizeof text, fmt, ap);
    va_end(ap);
    if (n < 0) n = 0;
    size_t len = (size_t)n < EV_TEXT_MAX ? (size_t)n : EV_TEXT_MAX;

    /* The journal first: it holds the event even if the socket never does (D-042). */
    LOG_I("event code=0x%04x src=%s %.*s", (unsigned)code, log_src_name((data_src_t)src),
          (int)len, text);

    if (s->n == EV_QUEUE) {          /* full: drop the oldest, and count it */
        s->head = (s->head + 1) % EV_QUEUE;
        s->n--;
        s->dropped++;
    }
    ev_t *e = &s->q[(s->head + s->n) % EV_QUEUE];
    e->code = code;
    e->src = src;
    e->len = (uint8_t)len;
    memcpy(e->text, text, len);
    s->n++;
    (void)ev_flush(s);
}

/* --- receiver --------------------------------------------------------------------------- */

int ev_receiver_open(ev_receiver_t *r, const char *path)
{
    memset(r, 0, sizeof *r);
    r->listen_fd = -1;
    for (size_t i = 0; i < EV_MAX_CLIENTS; i++) r->client[i] = -1;

    struct sockaddr_un a;
    if (!make_addr(&a, path)) {
        LOG_E("event_socket_path_invalid path=%s", path);
        return -1;
    }
    snprintf(r->path, sizeof r->path, "%s", path);
    /* A socket file left by a storaged that was kill -9'd would make bind fail. */
    if (unlink(path) != 0 && errno != ENOENT)
        LOG_W("event_socket_stale_unlink_failed path=%s errno=%d (%s)", path, errno,
              strerror(errno));

    int fd = socket(AF_UNIX, SOCK_SEQPACKET | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
    if (fd < 0) {
        LOG_E("event_socket_failed errno=%d (%s)", errno, strerror(errno));
        return -1;
    }
    const char *step = "bind";
    if (bind(fd, (const struct sockaddr *)&a, sizeof a) != 0) goto fail;
    step = "chmod";
    if (chmod(path, 0600) != 0) goto fail;
    step = "listen";
    if (listen(fd, (int)EV_MAX_CLIENTS) != 0) goto fail;
    r->listen_fd = fd;
    LOG_I("event_socket_listening path=%s", path);
    return 0;

fail:
    LOG_E("event_socket_open_failed path=%s step=%s errno=%d (%s)", path, step, errno,
          strerror(errno));
    close_fd(fd, "listen");
    return -1;
}

void ev_receiver_close(ev_receiver_t *r)
{
    for (size_t i = 0; i < EV_MAX_CLIENTS; i++) {
        close_fd(r->client[i], "client");
        r->client[i] = -1;
    }
    if (r->listen_fd >= 0) {
        close_fd(r->listen_fd, "listen");
        r->listen_fd = -1;
        if (unlink(r->path) != 0 && errno != ENOENT)
            LOG_W("event_socket_unlink_failed path=%s errno=%d (%s)", r->path, errno,
                  strerror(errno));
    }
}

static void accept_waiting(ev_receiver_t *r)
{
    for (;;) {
        int c = accept4(r->listen_fd, NULL, NULL, SOCK_NONBLOCK | SOCK_CLOEXEC);
        if (c < 0) {
            if (errno == EINTR || errno == ECONNABORTED) continue;
            if (errno != EAGAIN && errno != EWOULDBLOCK)
                LOG_W("event_accept_failed errno=%d (%s)", errno, strerror(errno));
            return;
        }
        size_t i = 0;
        while (i < EV_MAX_CLIENTS && r->client[i] >= 0) i++;
        if (i == EV_MAX_CLIENTS) {
            LOG_W("event_client_refused reason=full max=%u", EV_MAX_CLIENTS);
            close_fd(c, "refused");
            continue;
        }
        r->client[i] = c;
        LOG_I("event_client_connected slot=%zu", i);
    }
}

size_t ev_receive(ev_receiver_t *r, ev_t *out, size_t max)
{
    if (r->listen_fd < 0) return 0;
    accept_waiting(r);
    size_t got = 0;
    for (size_t i = 0; i < EV_MAX_CLIENTS && got < max; i++) {
        while (r->client[i] >= 0 && got < max) {
            uint8_t pkt[EV_PACKET_MAX + 1];   /* one more: an oversized packet is seen */
            ssize_t n = recv(r->client[i], pkt, sizeof pkt, MSG_DONTWAIT);
            if (n < 0 && errno == EINTR) continue;
            if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) break;
            if (n <= 0) {
                /* 0: the sender exited or restarted; it reconnects. <0: a real error. */
                if (n < 0) LOG_W("event_recv_failed slot=%zu errno=%d (%s)", i, errno,
                                 strerror(errno));
                close_fd(r->client[i], "client");
                r->client[i] = -1;
                break;
            }
            if (n < 3 || (size_t)n > EV_PACKET_MAX || !SRC_IS_VALID(pkt[2])) {
                r->refused++;
                LOG_W("event_refused slot=%zu bytes=%zd src=%u", i, n,
                      n >= 3 ? (unsigned)pkt[2] : 0u);
                continue;
            }
            ev_t *e = &out[got++];
            e->code = (uint16_t)(pkt[0] | pkt[1] << 8);
            e->src = pkt[2];
            e->len = (uint8_t)(n - 3);
            memcpy(e->text, pkt + 3, e->len);
            r->received++;
        }
    }
    return got;
}
