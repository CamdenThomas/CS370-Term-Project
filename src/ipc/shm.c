/* The rings in POSIX shared memory (D-039). The supervisor creates each ring once at
 * start; obdd, ambientd and storaged attach to it and never initialize it, so a child
 * restarted by the supervisor cannot reset the indices the other side is using. */
#include "ipc/ring.h"

#include "common/log.h"

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

ring_t *ring_create(const char *name)
{
    ring_t *r  = NULL;
    int     fd = -1;

    /* A ring left by a previous boot or a crashed supervisor holds stale indices. */
    if (shm_unlink(name) != 0 && errno != ENOENT)
        LOG_W("ring_stale_unlink_failed name=%s errno=%d (%s)", name, errno, strerror(errno));

    fd = shm_open(name, O_RDWR | O_CREAT | O_EXCL, 0600);
    if (fd < 0) {
        LOG_E("ring_create_failed name=%s step=shm_open errno=%d (%s)",
              name, errno, strerror(errno));
        goto out;
    }
    if (ftruncate(fd, (off_t)sizeof *r) != 0) {
        LOG_E("ring_create_failed name=%s step=ftruncate errno=%d (%s)",
              name, errno, strerror(errno));
        goto unlink;
    }
    void *p = mmap(NULL, sizeof *r, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (p == MAP_FAILED) {
        LOG_E("ring_create_failed name=%s step=mmap errno=%d (%s)",
              name, errno, strerror(errno));
        goto unlink;
    }
    r = p;
    ring_init(r);
    LOG_I("ring_created name=%s bytes=%zu capacity=%u", name, sizeof *r, RING_CAPACITY);
    goto out;

unlink:
    if (shm_unlink(name) != 0)
        LOG_W("ring_unlink_failed name=%s errno=%d (%s)", name, errno, strerror(errno));
out:
    if (fd >= 0 && close(fd) != 0)
        LOG_W("ring_close_failed name=%s errno=%d (%s)", name, errno, strerror(errno));
    return r;
}

ring_t *ring_attach(const char *name)
{
    ring_t     *r  = NULL;
    int         fd = shm_open(name, O_RDWR, 0);
    struct stat st;

    if (fd < 0) {
        LOG_E("ring_attach_failed name=%s step=shm_open errno=%d (%s)",
              name, errno, strerror(errno));
        return NULL;
    }
    if (fstat(fd, &st) != 0) {
        LOG_E("ring_attach_failed name=%s step=fstat errno=%d (%s)",
              name, errno, strerror(errno));
        goto out;
    }
    if ((size_t)st.st_size < sizeof *r) {
        LOG_E("ring_attach_failed name=%s step=size have=%lld want=%zu",
              name, (long long)st.st_size, sizeof *r);
        goto out;
    }
    void *p = mmap(NULL, sizeof *r, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (p == MAP_FAILED) {
        LOG_E("ring_attach_failed name=%s step=mmap errno=%d (%s)",
              name, errno, strerror(errno));
        goto out;
    }
    if (!ring_layout_ok(p)) {
        const ring_t *bad = p;
        LOG_E("ring_attach_failed name=%s step=layout magic=0x%08X version=%u "
              "slot_size=%u capacity=%u", name, bad->magic, bad->version,
              bad->slot_size, bad->capacity);
        if (munmap(p, sizeof *r) != 0)
            LOG_W("ring_munmap_failed name=%s errno=%d (%s)", name, errno, strerror(errno));
        goto out;
    }
    r = p;

out:
    if (close(fd) != 0)
        LOG_W("ring_close_failed name=%s errno=%d (%s)", name, errno, strerror(errno));
    return r;
}

void ring_detach(ring_t *r)
{
    if (r != NULL && munmap(r, sizeof *r) != 0)
        LOG_W("ring_munmap_failed errno=%d (%s)", errno, strerror(errno));
}

int ring_destroy(const char *name)
{
    if (shm_unlink(name) != 0 && errno != ENOENT) {
        LOG_E("ring_destroy_failed name=%s errno=%d (%s)", name, errno, strerror(errno));
        return -1;
    }
    return 0;
}
