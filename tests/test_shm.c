/* The ring across a real process boundary (D-039): a forked producer attaches by name
 * and pushes; the parent consumes in storaged's peek-then-release pattern. Every slot
 * must arrive once, in order, intact. Attach must refuse what it cannot trust. */
#include "ipc/ring.h"
#include "common/log.h"

#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

static int failures;

#define CHECK(cond, msg) do { \
    if (!(cond)) { fprintf(stderr, "  FAIL %s:%d: %s\n", __FILE__, __LINE__, msg); \
                   failures++; } } while (0)

#define CROSS_N 200000u
#define BATCH   64u

static void name_for(char *buf, size_t n, const char *tag)
{
    snprintf(buf, n, "/cw-%s-%d", tag, (int)getpid());
}

static void produce(const char *name)
{
    ring_t *r = ring_attach(name);
    if (r == NULL) _exit(2);
    for (uint64_t i = 0; i < CROSS_N; ) {
        ring_slot_t s;
        memset(&s, 0, sizeof s);
        s.src = SRC_SYNTH;   /* test data is synthesized; label it (CLAUDE.md §2.7) */
        s.len = sizeof i;
        memcpy(s.data, &i, sizeof i);
        if (ring_push(r, &s)) i++;
    }
    ring_detach(r);
    _exit(0);
}

static void test_cross_process_order(void)
{
    char name[64];
    name_for(name, sizeof name, "x");
    ring_t *r = ring_create(name);
    CHECK(r != NULL, "create");
    if (r == NULL) return;

    pid_t pid = fork();
    if (pid == 0) produce(name);
    CHECK(pid > 0, "fork");

    unsigned bad = 0;
    uint64_t want = 0;
    while (pid > 0 && want < CROSS_N) {
        size_t n = 0;
        ring_slot_t s;
        while (n < BATCH && ring_peek(r, n, &s)) {
            uint64_t v;
            memcpy(&v, s.data, sizeof v);
            if (s.seq != want || v != want || s.src != SRC_SYNTH) bad++;
            want++;
            n++;
        }
        /* storaged would write and fsync the batch here, then release it (D-039). */
        if (n > 0 && !ring_release(r, n)) bad++;
    }

    int status = 0;
    CHECK(pid > 0 && waitpid(pid, &status, 0) == pid, "waitpid");
    CHECK(WIFEXITED(status) && WEXITSTATUS(status) == 0, "producer attached and finished");
    CHECK(bad == 0, "a slot arrived out of order, twice or torn");
    CHECK(ring_depth(r) == 0, "everything consumed");
    CHECK(ring_rejected(r) == 0, "no slot was unlabeled");

    ring_detach(r);
    CHECK(ring_destroy(name) == 0, "destroy");
}

static void test_attach_refuses_what_it_cannot_trust(void)
{
    char name[64];
    name_for(name, sizeof name, "a");
    CHECK(ring_attach(name) == NULL, "attach to a missing ring");

    ring_t *r = ring_create(name);
    CHECK(r != NULL, "create");
    if (r == NULL) return;
    r->version = RING_VERSION + 1;   /* as if written by another build */
    CHECK(ring_attach(name) == NULL, "attach to a ring of another layout");
    r->version = RING_VERSION;
    ring_t *again = ring_attach(name);
    CHECK(again != NULL, "attach to a good ring");
    ring_detach(again);

    /* A second create replaces a stale ring rather than failing on it. */
    ring_slot_t s;
    memset(&s, 0, sizeof s);
    s.src = SRC_SYNTH;
    CHECK(ring_push(r, &s), "push into the old ring");
    ring_detach(r);
    r = ring_create(name);
    CHECK(r != NULL && ring_depth(r) == 0, "re-create starts empty");
    ring_detach(r);
    CHECK(ring_destroy(name) == 0, "destroy");
}

int main(void)
{
    log_init("test_shm", LOG_FATAL);   /* the refusals above log errors by design */
    test_cross_process_order();
    test_attach_refuses_what_it_cannot_trust();
    return failures == 0 ? 0 : 1;
}
