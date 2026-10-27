#!/usr/bin/env bash
# Soak runner. --mini runs a 1-hour bench rehearsal (make soakcheck). The default opens
# the graded 48-hour key-cycle window (D-023): the box runs in the car as its installed
# service, and this script records the manifest and collects the session logs.
set -euo pipefail

DURATION=$((48 * 3600))
LABEL="soak"
if [[ "${1:-}" == "--mini" ]]; then DURATION=3600; LABEL="mini"; fi

STAMP="$(date -u +%Y%m%dT%H%M%SZ)"
LOGDIR="soak/logs/${LABEL}-${STAMP}"
mkdir -p "$LOGDIR"

echo "soak: label=${LABEL} duration=${DURATION}s logdir=${LOGDIR}"
echo "soak: THE LOGS ARE THE DELIVERABLE. Do not edit them afterward."

# TODO(M3): --mini: launch build/bin/supervisor with its log to $LOGDIR/system.log
# TODO(M3): record uname -a, git rev-parse HEAD, build flags, the car, and the data
#           source (live | replay | synth) into $LOGDIR/manifest.txt BEFORE starting.
# TODO(M4): default: at the end of the window, copy the box's raw session logs into
#           $LOGDIR and list every session (boot, end, clean or recovered) in
#           $LOGDIR/sessions.txt.
# TODO(M4): record what fault was injected, when, and in which session, into
#           $LOGDIR/faults.txt
# TODO(M4): orderly end: the last session's final heartbeat; exit status recorded.

echo "not implemented — M0 scaffold" >&2
exit 1
