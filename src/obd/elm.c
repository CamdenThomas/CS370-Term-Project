/* One ELM327 session over a tty: the C port of tools/elm.py (see include/obd/elm.h). */
#include "obd/elm.h"

#include "common/log.h"

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <string.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

/* Settings after reset: echo off, no linefeeds, spaces on, headers off, auto protocol. */
static const char *const k_init[] = { "ATE0", "ATL0", "ATS1", "ATH0", "ATSP0" };

#define ATZ_MS     5000   /* elm.py: ATZ timeout */
#define SEARCH_MS 15000   /* elm.py: the first 0100 searches for the protocol */
#define VIN_MS     5000

static int64_t now_ms(void)
{
    struct timespec t;
    if (clock_gettime(CLOCK_MONOTONIC, &t) != 0) {
        LOG_E("clock_gettime_failed errno=%d (%s)", errno, strerror(errno));
        return 0;
    }
    return (int64_t)t.tv_sec * 1000 + t.tv_nsec / 1000000;
}

elm_status_t elm_open(elm_t *e, const char *path)
{
    memset(e, 0, sizeof *e);
    e->fd = -1;
    e->timeout_ms = ELM_TIMEOUT_MS;
    e->resync_ms = ELM_RESYNC_MS;

    int fd = open(path, O_RDWR | O_NOCTTY | O_NONBLOCK | O_CLOEXEC);
    if (fd < 0) {
        LOG_W("tty_open_failed path=%s errno=%d (%s)", path, errno, strerror(errno));
        return ELM_LINK_LOST;
    }
    struct termios t;
    if (tcgetattr(fd, &t) != 0) {
        LOG_W("tty_getattr_failed path=%s errno=%d (%s)", path, errno, strerror(errno));
        goto fail;
    }
    /* Raw: no echo, no line editing, no CR translation. The baud is ignored by RFCOMM
     * and by a pty; set one anyway so a USB adapter (D-015's fallback) also works. */
    cfmakeraw(&t);
    t.c_cflag |= CLOCAL | CREAD;
    if (cfsetspeed(&t, B115200) != 0 || tcsetattr(fd, TCSANOW, &t) != 0) {
        LOG_W("tty_setattr_failed path=%s errno=%d (%s)", path, errno, strerror(errno));
        goto fail;
    }
    e->fd = fd;
    return ELM_OK;

fail:
    if (close(fd) != 0) LOG_W("tty_close_failed errno=%d (%s)", errno, strerror(errno));
    return ELM_LINK_LOST;
}

void elm_close(elm_t *e)
{
    if (e->fd >= 0 && close(e->fd) != 0)
        LOG_W("tty_close_failed errno=%d (%s)", errno, strerror(errno));
    e->fd = -1;
}

/* Reads until the last byte that is not whitespace (or NUL) is the `>` prompt.
 * With keep, the bytes go into e->reply. ELM_OK, ELM_TIMEOUT or ELM_LINK_LOST. */
static elm_status_t read_to_prompt(elm_t *e, int64_t deadline, bool keep)
{
    char last = 0;
    if (keep) {
        e->reply_len = 0;
        e->reply_cut = false;
        e->reply[0] = '\0';
    }
    for (;;) {
        int64_t left = deadline - now_ms();
        if (left <= 0) return ELM_TIMEOUT;
        struct pollfd p = { .fd = e->fd, .events = POLLIN };
        int r = poll(&p, 1, (int)left);
        if (r < 0) {
            if (errno == EINTR) continue;
            LOG_W("tty_poll_failed errno=%d (%s)", errno, strerror(errno));
            return ELM_LINK_LOST;
        }
        if (r == 0) return ELM_TIMEOUT;
        if (p.revents & POLLNVAL) {
            LOG_W("tty_poll_invalid fd=%d", e->fd);
            return ELM_LINK_LOST;
        }
        if (p.revents & POLLIN) {
            char buf[256];
            ssize_t n = read(e->fd, buf, sizeof buf);
            if (n < 0) {
                if (errno == EINTR || errno == EAGAIN) continue;
                /* EIO: the far end of the tty is gone (RFCOMM dropped, pty closed). */
                LOG_W("tty_read_failed errno=%d (%s)", errno, strerror(errno));
                return ELM_LINK_LOST;
            }
            if (n == 0) {
                LOG_W("tty_read_eof");
                return ELM_LINK_LOST;
            }
            for (ssize_t i = 0; i < n; i++) {
                char c = buf[i];
                if (keep) {
                    if (e->reply_len < ELM_REPLY_MAX) e->reply[e->reply_len++] = c;
                    else e->reply_cut = true;
                }
                /* elm.py strips whitespace before looking for `>`; NUL too, here, since
                 * some clones pad after the prompt. */
                if (c != '\0' && !isspace((unsigned char)c)) last = c;
            }
            if (keep) e->reply[e->reply_len] = '\0';
            if (last == '>') return ELM_OK;
            continue;
        }
        if (p.revents & (POLLHUP | POLLERR)) {
            LOG_W("tty_hangup revents=0x%x", (unsigned)p.revents);
            return ELM_LINK_LOST;
        }
    }
}

/* A write blocked on a stalled Bluetooth link must time out, as elm.py's write_timeout. */
static elm_status_t write_all(elm_t *e, const char *s, size_t len, int64_t deadline)
{
    while (len > 0) {
        ssize_t n = write(e->fd, s, len);
        if (n > 0) {
            s += n;
            len -= (size_t)n;
            continue;
        }
        if (n < 0 && errno == EINTR) continue;
        if (n < 0 && errno != EAGAIN) {
            LOG_W("tty_write_failed errno=%d (%s)", errno, strerror(errno));
            return ELM_LINK_LOST;
        }
        int64_t left = deadline - now_ms();
        if (left <= 0) {
            LOG_W("tty_write_timeout left_bytes=%zu", len);
            return ELM_LINK_LOST;
        }
        struct pollfd p = { .fd = e->fd, .events = POLLOUT };
        if (poll(&p, 1, (int)left) < 0 && errno != EINTR) {
            LOG_W("tty_poll_failed errno=%d (%s)", errno, strerror(errno));
            return ELM_LINK_LOST;
        }
    }
    return ELM_OK;
}

/* elm.py _resync: wait out the reply to a request that timed out. */
static elm_status_t resync(elm_t *e)
{
    int first = e->resync_ms < e->timeout_ms ? e->resync_ms : e->timeout_ms;
    elm_status_t st = read_to_prompt(e, now_ms() + first, false);
    if (st == ELM_TIMEOUT) {
        st = write_all(e, "\r", 1, now_ms() + e->timeout_ms);
        if (st != ELM_OK) return st;
        st = read_to_prompt(e, now_ms() + e->timeout_ms, false);
        if (st == ELM_TIMEOUT) {
            LOG_W("adapter_busy after_ms=%d", first + e->timeout_ms);
            return ELM_TIMEOUT;   /* still stale */
        }
    }
    if (st != ELM_OK) return st;
    e->stale = false;
    return ELM_OK;
}

elm_status_t elm_command(elm_t *e, const char *cmd, int timeout_ms)
{
    if (e->fd < 0) return ELM_LINK_LOST;
    int t = timeout_ms > 0 ? timeout_ms : e->timeout_ms;
    if (e->stale) {
        elm_status_t st = resync(e);
        if (st != ELM_OK) return st;
    }

    char line[32];
    int n = snprintf(line, sizeof line, "%s\r", cmd);
    if (n < 0 || (size_t)n >= sizeof line) {
        LOG_E("command_too_long len=%zu", strlen(cmd));
        return ELM_ERROR;
    }
    int64_t deadline = now_ms() + t;
    /* Drop anything unread, so a stray byte is never taken for this reply. */
    if (tcflush(e->fd, TCIFLUSH) != 0) {
        LOG_W("tty_flush_failed errno=%d (%s)", errno, strerror(errno));
        return ELM_LINK_LOST;
    }
    elm_status_t st = write_all(e, line, (size_t)n, deadline);
    if (st != ELM_OK) return st;
    st = read_to_prompt(e, deadline, true);
    if (st == ELM_TIMEOUT) e->stale = true;
    return st;
}

void elm_join_lines(const char *raw, size_t len, const char *skip, char *out, size_t cap)
{
    size_t o = 0;
    if (cap == 0) return;
    out[0] = '\0';
    size_t i = 0;
    while (i < len) {
        char line[128];
        size_t n = 0;
        while (i < len && raw[i] != '\r' && raw[i] != '\n') {
            char c = raw[i++];
            if (c != '>' && c != '\0' && n < sizeof line - 1) line[n++] = c;
        }
        while (i < len && (raw[i] == '\r' || raw[i] == '\n')) i++;
        size_t s = 0;
        while (s < n && isspace((unsigned char)line[s])) s++;
        while (n > s && isspace((unsigned char)line[n - 1])) n--;
        if (n == s) continue;
        line[n] = '\0';
        if (skip != NULL && strcmp(line + s, skip) == 0) continue;
        int w = snprintf(out + o, cap - o, "%s%s", o > 0 ? " " : "", line + s);
        if (w < 0 || (size_t)w >= cap - o) {
            out[cap - 1] = '\0';
            return;
        }
        o += (size_t)w;
    }
}

elm_status_t elm_init(elm_t *e)
{
    elm_status_t st = elm_command(e, "ATZ", ATZ_MS);
    if (st != ELM_OK) return st;
    elm_join_lines(e->reply, e->reply_len, "ATZ", e->banner, sizeof e->banner);
    for (size_t i = 0; i < sizeof k_init / sizeof k_init[0]; i++) {
        st = elm_command(e, k_init[i], 0);
        if (st != ELM_OK) return st;
    }
    /* With the ignition off the protocol search fails: the caller waits, then retries. */
    st = elm_command(e, "0100", SEARCH_MS);
    if (st != ELM_OK) return st;
    if (obd_check_bus(e->reply, e->reply_len) == OBD_ASLEEP) return ELM_ASLEEP;
    return ELM_OK;
}

bool elm_is_sim(const elm_t *e)
{
    for (const char *p = e->banner; p[0] != '\0'; p++)
        if (toupper((unsigned char)p[0]) == 'S' && toupper((unsigned char)p[1]) == 'I' &&
            toupper((unsigned char)p[2]) == 'M')
            return true;
    return false;
}

elm_status_t elm_query(elm_t *e, uint8_t mode, uint8_t pid, obd_answer_t *out)
{
    char req[5];
    obd_request(req, mode, pid);
    elm_status_t st = elm_command(e, req, 0);
    if (st != ELM_OK) {
        memset(out, 0, sizeof *out);
        return st;
    }
    return (elm_status_t)obd_parse_all(e->reply, e->reply_len, mode, pid, out);
}

elm_status_t elm_supported(elm_t *e, obd_pidset_t *out)
{
    memset(out, 0, sizeof *out);
    for (unsigned base = 0x00; base <= 0xE0; base += 0x20) {
        char req[5];
        obd_request(req, 0x01, (uint8_t)base);
        elm_status_t st = elm_command(e, req, 0);
        if (st != ELM_OK) return st;

        obd_pidset_t block;
        memset(&block, 0, sizeof block);
        /* Every ECU that answers contributes; the engine may not answer first. */
        obd_status_t ps = obd_support_from_reply(e->reply, e->reply_len, (uint8_t)base,
                                                 &block);
        if (ps == OBD_NODATA) {
            if (base == 0x00) return ELM_NODATA;   /* nothing answers at all */
            break;          /* a block the previous one chained to, unanswered */
        }
        if (ps != OBD_OK) return (elm_status_t)ps;
        for (unsigned p = base + 1; p < base + 0x20 && p <= 0xFF; p++)
            if (obd_pidset_has(&block, (uint8_t)p)) obd_pidset_add(out, (uint8_t)p);
        if (base + 0x20 > 0xFF || !obd_pidset_has(&block, (uint8_t)(base + 0x20))) break;
    }
    return ELM_OK;
}

elm_status_t elm_info(elm_t *e, const char *cmd, char *out, size_t cap)
{
    if (cap > 0) out[0] = '\0';
    elm_status_t st = elm_command(e, cmd, 0);
    if (st != ELM_OK) return st;
    elm_join_lines(e->reply, e->reply_len, NULL, out, cap);
    return ELM_OK;
}

elm_status_t elm_vin(elm_t *e, char vin[OBD_VIN_MAX + 1])
{
    vin[0] = '\0';
    elm_status_t st = elm_command(e, "0902", VIN_MS);
    if (st != ELM_OK) return st;
    return (elm_status_t)obd_decode_vin(e->reply, e->reply_len, vin);
}
