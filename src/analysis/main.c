/* analyzed — the intelligence. All of it ours (handout §3.3, D-004).
 *
 * OWNER: Lance.
 *
 * Pipeline:
 *   operating-point bin (RPM x load x coolant temp)
 *     -> per-bin running mean/variance
 *     -> multivariate residual (Mahalanobis)
 *     -> robust trend (Theil-Sen) over oil age / fuel-trim drift
 *     -> classifier (our weights, our C inference)
 *     -> state machine with hysteresis and dwell
 *
 * The restraint rule: a monitor that cries wolf twice gets unplugged, and an unplugged
 * monitor is worse than none. False-positive rate is a first-class metric.
 */
#include "common/log.h"

#include <stdio.h>

int main(void)
{
    log_init("analyzed", LOG_INFO);
    LOG_I("start");

    /* TODO(M3): operating-point binning + running statistics (Welford).
     * TODO(M4): Mahalanobis residual; Theil-Sen trend; state machine.
     * TODO(M4): load weights from models/; inference in C. NO network, ever. */

    LOG_W("not implemented — M0 scaffold");
    return 0;
}
