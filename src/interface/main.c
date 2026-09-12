/* obdctl — query CLI over a Unix domain socket. Shared ownership.
 *
 * The handout is explicit that a crisp CLI with exact semantics beats a pretty
 * dashboard, and it is far more extensible at the live-modification station on demo
 * day. Resist the web app.
 *
 * This is the ONLY directory in src/ permitted to contain network code, and only for
 * the LAN interface (CLAUDE.md §2.1). `make boundary` enforces that.
 */
#include "common/log.h"

#include <stdio.h>
#include <string.h>

static int usage(void)
{
    fputs("usage: obdctl <verb>\n"
          "  status                     liveness, per-process RSS, event counts\n"
          "  verdicts [--since WHEN]    current and historical diagnoses\n"
          "  series PID --from --to     raw or binned history\n"
          "  baseline PID               learned model for an operating-point bin\n"
          "  faults                     detection / degradation / recovery events\n",
          stderr);
    return 2;
}

int main(int argc, char **argv)
{
    if (argc < 2) return usage();

    log_init("obdctl", LOG_WARN);

    static const char *verbs[] = { "status", "verdicts", "series", "baseline", "faults" };
    for (size_t i = 0; i < sizeof verbs / sizeof verbs[0]; i++) {
        if (strcmp(argv[1], verbs[i]) == 0) {
            /* TODO(M3): connect to the supervisor's UDS and issue the request. */
            printf("%s: not implemented — M0 scaffold\n", verbs[i]);
            return 0;
        }
    }
    return usage();
}
