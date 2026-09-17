#!/usr/bin/env bash
# Soak runner. --mini runs a 1-hour rehearsal (make soakcheck); default is the real 48h.
set -euo pipefail

DURATION=$((48 * 3600))
LABEL="soak"
if [[ "${1:-}" == "--mini" ]]; then DURATION=3600; LABEL="mini"; fi

STAMP="$(date -u +%Y%m%dT%H%M%SZ)"
LOGDIR="soak/logs/${LABEL}-${STAMP}"
mkdir -p "$LOGDIR"

echo "soak: label=${LABEL} duration=${DURATION}s logdir=${LOGDIR}"
echo "soak: THE LOGS ARE THE DELIVERABLE. Do not edit them afterward."

# TODO(M3): launch build/bin/supervisor with its log to $LOGDIR/system.log
# TODO(M3): record uname -a, git rev-parse HEAD, build flags, and the data source
#           (live | replay | synth) into $LOGDIR/manifest.txt BEFORE starting.
# TODO(M4): schedule the injected fault at a random point in the middle third and
#           record what was injected and when into $LOGDIR/faults.txt
# TODO(M4): orderly shutdown at the end; final heartbeat; exit status recorded.

echo "not implemented — M0 scaffold" >&2
exit 1
