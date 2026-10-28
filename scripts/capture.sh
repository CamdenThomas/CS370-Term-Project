#!/bin/bash
# carwatch capture: survey the car's sensors, record them to disk, show what was stored,
# and commit everything to the repository under captures/<car>/ (D-046).
#
#   scripts/capture.sh /dev/cu.<adapter> [seconds, default 60]   # a real car: committed
#   scripts/capture.sh /dev/rfcomm0 [seconds]                    # the same, on the Pi
#   scripts/capture.sh sim [seconds]                             # rehearsal: never committed
#   CAR=crv scripts/capture.sh ...                               # name the car (default forester)
#
# Run it from a branch, not main; on main it switches to camden/m2/captures first.
# A capture is committed only when every record says src=live (CLAUDE.md §2.7).
set -u -o pipefail
PORT=${1:?usage: scripts/capture.sh /dev/cu.<adapter>|/dev/rfcomm0|sim [seconds]}
SECS=${2:-60}
CAR=${CAR:-forester}
cd "$(dirname "$0")/.." || exit 1
export PYTHONPATH=tools
PY=.venv/bin/python
[ -x "$PY" ] || { echo "capture: no .venv; run 'make toolcheck' once first" >&2; exit 1; }
mkdir -p data
START=data/.capture-start
touch "$START"
AWAKE=""; command -v caffeinate >/dev/null && AWAKE="caffeinate -i"   # macOS only

step() { printf '\n\033[1;36m== %s\033[0m\n\033[0;33m   say: %s\033[0m\n\n' "$1" "$2"; sleep 2; }
newest() { find data -maxdepth 1 -name "$1" -newer "$START" | sort | tail -1; }

if [ "$PORT" = sim ]; then
    $PY tools/obdsim.py --link data/obd-sim --latency 0.04 >/dev/null 2>&1 &
    SIM=$!
    trap 'kill $SIM 2>/dev/null' EXIT
    for _ in 1 2 3 4 5 6 7 8 9 10; do [ -e data/obd-sim ] && break; sleep 0.5; done
    PORT=data/obd-sim
fi

step "1/4  Is the car talking? Which sensors does it publish?" \
     "The adapter in the diagnostic port talks to this computer over Bluetooth. First we ask the engine computer which sensors it has."
$PY tools/pidscan.py --port "$PORT" | grep -v '^VIN' || exit 1   # keep the VIN off the video

step "2/4  Recording every sensor for ${SECS} s" \
     "Now we record all of them, about 20 readings a second. Each reading is written to disk the moment it arrives, labeled live. (Rev the engine now.)"
$AWAKE $PY tools/obdlog.py --port "$PORT" --max-seconds "$SECS"
LOG=$(newest 'drive-*.jsonl')
[ -n "$LOG" ] || { echo "capture: obdlog wrote no log" >&2; exit 1; }
NAME=$(basename "$LOG" .jsonl)

step "3/4  What is on disk" \
     "This is the file it wrote: one line per reading, each with its source, a timestamp, the raw reply from the car and the decoded value. Then a summary per sensor."
ls -lh "$LOG"
echo "$(wc -l <"$LOG") records; the last few:"
tail -n 6 "$LOG" | cut -c1-150
echo
$PY tools/obdsum.py "$LOG" | tee "data/summary-$NAME.txt"
$PY - "$LOG" <<'EOF' | tee -a "data/summary-$NAME.txt"
import json, sys
t = []
for line in open(sys.argv[1]):
    try: r = json.loads(line)
    except ValueError: continue
    if r.get("type") == "sample": t.append(r["t_mono"])
g = sorted(b - a for a, b in zip(t, t[1:]))
if g:
    print(f"request cycle: p50 {g[len(g)//2]*1000:.0f} ms, p99 {g[int(len(g)*0.99)]*1000:.0f} ms")
EOF

case "$NAME" in
    *-live) ;;
    *) printf '\n\033[1;33m%s is not live data: kept in data/, not committed.\033[0m\n' "$NAME"
       exit 0 ;;
esac

step "4/4  Saved to the project" \
     "Everything is now committed to our git repository: the raw recording, the sensor survey and the summary."
DIR=captures/$CAR/${NAME#drive-}
DIR=${DIR%-live}
mkdir -p "$DIR"
cp "$LOG" "$DIR/drive.jsonl"
cp "data/summary-$NAME.txt" "$DIR/summary.txt"
SCAN=$(newest 'pidscan-*-live.md')
[ -n "$SCAN" ] && cp "$SCAN" "$DIR/pidscan.md" && cp "${SCAN%.md}.json" "$DIR/pidscan.json"

[ "$(git rev-parse --abbrev-ref HEAD)" != main ] ||
    git switch camden/m2/captures 2>/dev/null || git switch -c camden/m2/captures
STATS=$(sed -n 4p "$DIR/summary.txt")
git add -- "$DIR"
git commit -q -F - <<EOF
M2 obd: record a live $CAR capture, ${DIR##*/}

WHAT:   Adds the raw obdlog session, the pidscan survey and the obdsum
        summary, as scripts/capture.sh wrote them, in
        $DIR/.
WHY:    D-046: every real-car capture is kept in the repository.
        $STATS.
REVIEW: Raw and unedited; the VIN appears only as its hash (D-033).
VERIFY: scripts/capture.sh $1 $SECS; summary.txt is obdsum's output.
EOF
git log -1 --stat --format='%h %s' | cat
if git push -q -u origin HEAD 2>/dev/null; then
    echo "pushed to GitHub"
else
    echo "committed here; not pushed (no network?): run 'git push' later"
fi
