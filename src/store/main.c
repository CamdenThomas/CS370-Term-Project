/* storaged — append-only, crash-consistent log. Mechanism D lives here.
 *
 * OWNER: Lance.  M3 target: samples from the ring durably on disk, recoverable after a
 * power cut taken at an arbitrary instant.
 */
#include "common/log.h"
#include "store/record.h"

#include <stdio.h>

int main(void)
{
    log_init("storaged", LOG_INFO);
    LOG_I("start rec_version=%u", REC_VERSION);

    /* TODO(M2): decide and document the fsync discipline in docs/DESIGN.md §4 before
     *           writing it. Batch size and interval are a user-requirements argument,
     *           not a default to inherit.
     * TODO(M3): open-or-recover: scan from the last known-good offset, verify CRCs,
     *           truncate the torn tail, log exactly what was lost.
     * TODO(M3): drain the ring; sequence-account every slot.
     * TODO(M4): persist REC_BASELINE so learned statistics survive key-off. */

    LOG_W("not implemented — M0 scaffold");
    return 0;
}
