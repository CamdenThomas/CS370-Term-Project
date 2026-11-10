/* The event socket (D-042): events sent before storaged listens are queued and arrive in
 * order; past EV_QUEUE the oldest are dropped and counted; a receiver that restarts is
 * found again; a packet with no valid src is refused. */
#include "common/log.h"
#include "ipc/uds.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

static int failures;

#define CHECK(cond, msg) do { \
    if (!(cond)) { fprintf(stderr, "  FAIL %s:%d: %s\n", __FILE__, __LINE__, msg); \
                   failures++; } } while (0)

static char g_dir[64], g_path[EV_PATH_MAX];

static size_t receive_all(ev_receiver_t *r, ev_t *out, size_t max)
{
    size_t got = 0;
    for (int i = 0; i < 50 && got < max; i++) {   /* the kernel delivers at once; be kind */
        got += ev_receive(r, out + got, max - got);
        if (got < max) usleep(1000);
    }
    return got;
}

static bool text_is(const ev_t *e, const char *want)
{
    return e->len == strlen(want) && memcmp(e->text, want, e->len) == 0;
}

static void test_queued_then_delivered_in_order(void)
{
    ev_sender_t s;
    ev_sender_init(&s, g_path);
    ev_send(&s, 0x0201, SRC_SYNTH, "n=%d", 1);
    ev_send(&s, 0x0202, SRC_SYNTH, "n=%d", 2);
    ev_send(&s, 0x0203, SRC_LIVE, "n=%d", 3);
    CHECK(s.n == 3 && s.sent == 0, "no receiver yet: all three queued");

    ev_receiver_t r;
    CHECK(ev_receiver_open(&r, g_path) == 0, "receiver opens");
    struct stat st;
    CHECK(stat(g_path, &st) == 0 && (st.st_mode & 0777) == 0600, "socket is mode 0600");

    CHECK(ev_flush(&s) == 0 && s.sent == 3, "a flush delivers the backlog");
    ev_t got[8];
    size_t n = receive_all(&r, got, 3);
    CHECK(n == 3, "three received");
    if (n == 3) {
        CHECK(got[0].code == 0x0201 && text_is(&got[0], "n=1") && got[0].src == SRC_SYNTH,
              "first, with its code, text and src");
        CHECK(got[1].code == 0x0202 && text_is(&got[1], "n=2"), "second");
        CHECK(got[2].code == 0x0203 && got[2].src == SRC_LIVE, "third keeps its own src");
    }
    ev_sender_close(&s);
    ev_receiver_close(&r);
    CHECK(access(g_path, F_OK) != 0, "close removes the socket file");
}

static void test_overflow_drops_the_oldest(void)
{
    ev_sender_t s;
    ev_sender_init(&s, g_path);
    for (int i = 0; i < (int)EV_QUEUE + 6; i++) ev_send(&s, 0x0205, SRC_SYNTH, "i=%d", i);
    CHECK(s.n == EV_QUEUE && s.dropped == 6, "64 kept, 6 dropped and counted");

    ev_receiver_t r;
    CHECK(ev_receiver_open(&r, g_path) == 0, "receiver opens");
    (void)ev_flush(&s);
    ev_t got[EV_QUEUE];
    size_t n = receive_all(&r, got, EV_QUEUE);
    CHECK(n == EV_QUEUE, "all 64 kept events arrive");
    CHECK(n > 0 && text_is(&got[0], "i=6"), "the oldest six were the ones dropped");
    ev_sender_close(&s);
    ev_receiver_close(&r);
}

static void test_receiver_restart_is_found_again(void)
{
    ev_receiver_t r;
    CHECK(ev_receiver_open(&r, g_path) == 0, "receiver opens");
    ev_sender_t s;
    ev_sender_init(&s, g_path);
    ev_send(&s, 0x0201, SRC_SYNTH, "before");
    ev_t got[4];
    CHECK(receive_all(&r, got, 1) == 1, "delivered before the restart");

    ev_receiver_close(&r);              /* storaged kill -9'd and restarted */
    CHECK(ev_receiver_open(&r, g_path) == 0, "receiver reopens");
    ev_send(&s, 0x0201, SRC_SYNTH, "after");
    if (s.n > 0) (void)ev_flush(&s);    /* the first send may only notice the old peer */
    size_t n = receive_all(&r, got, 1);
    CHECK(n == 1 && text_is(&got[0], "after"), "the sender reconnects; nothing is lost");
    CHECK(s.n == 0, "nothing left queued");
    ev_sender_close(&s);
    ev_receiver_close(&r);
}

static void test_unlabeled_packet_is_refused(void)
{
    ev_receiver_t r;
    CHECK(ev_receiver_open(&r, g_path) == 0, "receiver opens");
    int fd = socket(AF_UNIX, SOCK_SEQPACKET, 0);
    struct sockaddr_un a;
    memset(&a, 0, sizeof a);
    a.sun_family = AF_UNIX;
    memcpy(a.sun_path, g_path, sizeof a.sun_path);   /* both EV_PATH_MAX, NUL-padded */
    CHECK(fd >= 0 && connect(fd, (struct sockaddr *)&a, sizeof a) == 0, "raw connect");
    const unsigned char bad[] = { 0x01, 0x02, 0x00, 'x' };   /* src 0: nobody vouched */
    const unsigned char tiny[] = { 0x01 };
    const unsigned char good[] = { 0x01, 0x02, SRC_REPLAY, 'o', 'k' };
    CHECK(send(fd, bad, sizeof bad, 0) == (ssize_t)sizeof bad &&
          send(fd, tiny, sizeof tiny, 0) == (ssize_t)sizeof tiny &&
          send(fd, good, sizeof good, 0) == (ssize_t)sizeof good, "raw sends");
    ev_t got[4];
    size_t n = receive_all(&r, got, 1);
    CHECK(n == 1 && got[0].code == 0x0201 && got[0].src == SRC_REPLAY && text_is(&got[0], "ok"),
          "only the labeled packet is returned");
    CHECK(r.refused == 2, "the unlabeled and the short packet are counted");
    close(fd);
    ev_receiver_close(&r);
}

static void test_long_text_is_cut(void)
{
    ev_receiver_t r;
    CHECK(ev_receiver_open(&r, g_path) == 0, "receiver opens");
    ev_sender_t s;
    ev_sender_init(&s, g_path);
    char big[400];
    memset(big, 'k', sizeof big - 1);
    big[sizeof big - 1] = '\0';
    ev_send(&s, 0x0201, SRC_SYNTH, "%s", big);
    ev_t got[1];
    CHECK(receive_all(&r, got, 1) == 1 && got[0].len == EV_TEXT_MAX, "cut to 200 bytes");
    ev_sender_close(&s);
    ev_receiver_close(&r);
}

int main(void)
{
    log_init("test_uds", LOG_FATAL);
    const char *base = getenv("TMPDIR");
    /* sun_path holds 108 bytes, so a long TMPDIR (macOS gives one) falls back to /tmp. */
    if (base == NULL || strlen(base) > 32) base = "/tmp";
    snprintf(g_dir, sizeof g_dir, "%s/carwatch-uds.XXXXXX", base);
    if (mkdtemp(g_dir) == NULL) { perror("mkdtemp"); return 2; }
    if (snprintf(g_path, sizeof g_path, "%s/events.sock", g_dir) >= (int)sizeof g_path)
        return 2;

    test_queued_then_delivered_in_order();
    test_overflow_drops_the_oldest();
    test_receiver_restart_is_found_again();
    test_unlabeled_packet_is_refused();
    test_long_text_is_cut();

    (void)unlink(g_path);
    (void)rmdir(g_dir);
    return failures == 0 ? 0 : 1;
}
