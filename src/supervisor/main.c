/* supervisor — mechanism E. Shared ownership.
 *
 * A diagnostic recorder that dies silently has actively harmed its user, who believes
 * it is on duty. Any child may be kill -9'd at any time; the system degrades, logs,
 * and recovers.
 *
 * The subtle requirement: a PHYSICALLY UNPLUGGED sensor is not a crash. Restarting a
 * child in a tight loop because its hardware is gone is a restart storm, and it is the
 * failure mode most likely to appear at hour 31 of the soak.
 */
#include "common/log.h"

#include <stdio.h>

int main(void)
{
    log_init("supervisor", LOG_INFO);
    LOG_I("start");

    /* TODO(M3): fork/exec children; fd hygiene across fork; waitpid loop.
     * TODO(M3): restart policy with exponential backoff + a restart-storm guard.
     * TODO(M3): hourly heartbeat per child (log_heartbeat) — the soak plot needs it.
     * TODO(M4): degrade-not-die when a sensor is absent; distinguish absent from
     *           crashed and log the distinction. Demo day will unplug something. */

    LOG_W("not implemented — M0 scaffold");
    return 0;
}
