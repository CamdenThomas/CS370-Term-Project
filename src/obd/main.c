/* obdd — capture daemon: Mode 01 requests over the Bluetooth OBD2 adapter (D-012, D-019).
 *
 * OWNER: Camden.  M3 target: real Mode 01 replies from the CR-V into the ring.
 *
 * Design intent (see docs/DESIGN.md §1): open /dev/obd, a udev symlink to the RFCOMM
 * tty the kernel binds to the adapter (/dev/rfcomm0); initialise the adapter (ATZ, ATE0, ATSP0); then round-robin the PID list at a fixed
 * request budget, one request outstanding at a time. Every timeout and every unanswered
 * PID is logged — a PID the car stops answering is a fault the analysis must see, not a
 * gap to fill.
 */
#include "common/log.h"

int main(void)
{
    log_init("obdd", LOG_INFO);
    LOG_I("start");

    /* TODO(M2): open /dev/obd (O_RDWR | O_NOCTTY); raw line discipline. The baud is
     *           ignored over RFCOMM, so any cfsetspeed() value works.
     * TODO(M2): treat a hangup (read() == 0, EIO, POLLHUP) as "link down": log it,
     *           mark the adapter absent, and reopen with backoff (DESIGN.md §3). The
     *           Bluetooth link drops; that is not a crash.
     * TODO(M3): adapter init; round-robin Mode 01 requests; parse each reply into a
     *           ring_slot_t and push it into the shm ring with src = live.
     * TODO(M3): per-request timeout; log unanswered PIDs; ATZ reset, then back off.
     * TODO(M4): instrument reply-arrival -> push latency into a histogram. */

    LOG_W("not implemented — M0 scaffold");
    return 0;
}
