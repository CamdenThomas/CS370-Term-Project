/* candaemon — MCP2515 receive path. Mechanism B lives here.
 *
 * OWNER: Camden.  M3 target: real frames from a real car into the ring.
 *
 * Design intent (see docs/DESIGN.md §2): the MCP2515's INT line is wired to a GPIO and
 * watched with epoll on the gpio chardev / SocketCAN fd. The polling variant is kept
 * behind --poll so the head-to-head comparison docs/EVALUATION.md §6.1 requires is a flag,
 * not a rewrite.
 */
#include "common/log.h"

#include <stdio.h>
#include <string.h>

int main(int argc, char **argv)
{
    int poll_mode = 0;
    for (int i = 1; i < argc; i++)
        if (strcmp(argv[i], "--poll") == 0) poll_mode = 1;

    log_init("candaemon", LOG_INFO);
    LOG_I("start mode=%s", poll_mode ? "poll" : "irq");

    /* TODO(M2): open SocketCAN can0; bring up via scripts/provision_pi.sh overlay.
     * TODO(M3): epoll on the CAN fd; push ring_slot_t into the shm ring.
     * TODO(M3): --poll variant with a configurable interval for the B comparison.
     * TODO(M4): instrument arrival->push latency into a histogram. */

    LOG_W("not implemented — M0 scaffold");
    return 0;
}
