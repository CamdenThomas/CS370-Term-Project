#!/usr/bin/env bash
# A labeled capture through the real pipeline: obdd --replay feeds the ring, storaged
# writes the log (make replay FIX=..., handout §5). Bench mode: obdd makes the ring
# (board item benchring), so nothing else may be using it.  cspell:words benchring
#
#   scripts/replay.sh CAPTURE.jsonl [SPEED] [OUT_DIR]
#
# SPEED divides the recorded spacing (1 = real time). OUT_DIR must be new or empty; the
# default is build/replay/<capture>-<time>/. A live capture comes out labeled replay; a
# synth one stays synth (CLAUDE.md §2.7).
set -euo pipefail

CAP=${1:?usage: scripts/replay.sh CAPTURE.jsonl [speed] [out dir]}
SPEED=${2:-1}
BIN=${CARWATCH_BIN:-build/bin}
RING=/dev/shm/carwatch-obd

die() { echo "replay: $*" >&2; exit 1; }

[ -f "$CAP" ] || die "no capture at $CAP"
if [ ! -x "$BIN/obdd" ] || [ ! -x "$BIN/storaged" ]; then die "build first: make"; fi
[ ! -e "$RING" ] || die "$RING exists: is obdd running? stop it first"
case "$(grep -m1 -o '"src":"[a-z]*"' "$CAP" || true)" in
    '"src":"live"' | '"src":"replay"') LABEL=replay ;;
    '"src":"synth"') LABEL=synth ;;
    *) die "$CAP has no live, replay or synth label: refused" ;;
esac
OUT=${3:-build/replay/$(basename "$CAP" .jsonl)-$(date +%Y%m%dT%H%M%S)}
mkdir -p "$OUT"
[ -z "$(ls -A "$OUT")" ] || die "$OUT is not empty"
SOCK=$(mktemp -u "${TMPDIR:-/tmp}/carwatch-replay.XXXXXX")

cleanup() {
    if [ -n "${OBDD:-}" ]; then kill "$OBDD" 2>/dev/null || true; fi
    if [ -n "${STORE:-}" ]; then kill "$STORE" 2>/dev/null || true; fi
    rm -f "$RING" "$SOCK"
}
trap cleanup EXIT

"$BIN/obdd" --replay "$CAP" --speed "$SPEED" --create-ring --events "$SOCK" \
    >"$OUT/obdd.log" 2>&1 &
OBDD=$!
for _ in $(seq 100); do [ -e "$RING" ] && break; sleep 0.05; done
[ -e "$RING" ] || die "obdd did not make the ring: see $OUT/obdd.log"
"$BIN/storaged" --dir "$OUT" --src "$LABEL" --events "$SOCK" >"$OUT/storaged.log" 2>&1 &
STORE=$!

OBDD_RC=0
wait "$OBDD" || OBDD_RC=$?
OBDD=
sleep 1.5                       # one batch (D-035): storaged drains what is left
kill -TERM "$STORE"
STORE_RC=0
wait "$STORE" || STORE_RC=$?
STORE=

grep -h -E "replay_done|replay_refused|stop samples" "$OUT/obdd.log" "$OUT/storaged.log" |
    sed 's/^.* pid=[0-9]* //'
echo "replay: label=$LABEL obdd=$OBDD_RC storaged=$STORE_RC log in $OUT"
[ "$OBDD_RC" -eq 0 ] && [ "$STORE_RC" -eq 0 ]
