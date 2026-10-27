/* storaged — append-only, crash-consistent log. Mechanism D lives here.
 *
 * M3 target: samples from the ring durably on disk, recoverable after a
 * power cut taken at an arbitrary instant.
 */
#include "common/log.h"
#include "store/record.h"

#include <stdio.h>

int main(void)
{
    log_init("storaged", LOG_INFO);
    LOG_I("start format=CWR1 magic=0x%08X", REC_MAGIC);

    /* TODO(M2): decide and document the fsync discipline in docs/DESIGN.md §4 before
     *           writing it. Batch size and interval are a user-requirements argument,
     *           not a default to inherit.
     * TODO(M3): open-or-recover: scan from the last known-committed offset, verify
     *           CRCs, truncate the torn tail and any batch without a COMMIT (D-025),
     *           log exactly what was lost.
     * TODO(M3): drain the ring into batches: BEGIN, records, COMMIT, one fsync;
     *           sequence-account every slot.
     * TODO(M3): write a boot record saying whether the last session ended cleanly,
     *           judged by a shutdown record after the last COMMIT (D-025). */

    LOG_W("not implemented — M0 scaffold");
    return 0;
}
