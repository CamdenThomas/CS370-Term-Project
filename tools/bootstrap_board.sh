#!/usr/bin/env bash
#
# bootstrap_board.sh — create the GitHub labels, milestones, issues and blocked-by
# dependencies for the whole carwatch project, from docs/PLAN.md.
#
# Run ONCE, by a human, from the repo root, after `gh auth login`:
#
#     ./tools/bootstrap_board.sh --dry-run     # print what it would do
#     ./tools/bootstrap_board.sh               # actually create everything
#
# Idempotency: labels and milestones are safe to re-run. ISSUES ARE NOT — running twice
# creates duplicates. The script refuses to proceed if open issues already exist unless
# you pass --force.
#
# Requires: gh >= 2.80 (issue dependency support landed 2026-06). Check with:
#     gh --version && gh issue dependency --help
set -euo pipefail

DRY=0; FORCE=0
for a in "$@"; do
  case "$a" in
    --dry-run) DRY=1 ;;
    --force)   FORCE=1 ;;
    *) echo "usage: $0 [--dry-run] [--force]" >&2; exit 2 ;;
  esac
done

command -v gh >/dev/null || { echo "gh CLI not found. https://cli.github.com" >&2; exit 1; }
gh auth status >/dev/null 2>&1 || { echo "Run 'gh auth login' first." >&2; exit 1; }

REPO="$(gh repo view --json nameWithOwner -q .nameWithOwner)"
echo "repo: $REPO"

if [[ $DRY -eq 0 && $FORCE -eq 0 ]]; then
  existing="$(gh issue list --state open --limit 1 --json number -q 'length')"
  if [[ "$existing" != "0" ]]; then
    echo "Open issues already exist. Re-running would create duplicates." >&2
    echo "Pass --force if you are sure." >&2
    exit 1
  fi
fi

run() { if [[ $DRY -eq 1 ]]; then echo "  DRY: $*"; else "$@"; fi; }

# ---------------------------------------------------------------- labels

echo "== labels =="
mklabel() {  # name, color, description
  if [[ $DRY -eq 1 ]]; then echo "  DRY: label $1"; return; fi
  gh label create "$1" --color "$2" --description "$3" --force >/dev/null
  echo "  $1"
}
mklabel question  d876e3 "Needs a human answer. Claude may never close this."
mklabel decision  8250df "A design choice that must be made. Claude may never close this."
mklabel task      0e8a16 "Work to be done."
mklabel bug       d73a4a "Something is wrong."
mklabel hardware  e99695 "Physical world: order it, wire it, measure it, drive it."
mklabel blocked   000000 "Waiting on a dependency."
mklabel critical-path b60205 "Slipping this slips the project."

# ------------------------------------------------------------ milestones

echo "== milestones =="
# Dates assume an Aug 24 2026 semester start — see PLAN.md / decision D-008.
# Correct them here if T04 says otherwise, BEFORE running for real.
mkmilestone() {  # title, due (YYYY-MM-DD), description
  if [[ $DRY -eq 1 ]]; then echo "  DRY: milestone $1 due $2"; return; fi
  gh api "repos/$REPO/milestones" -f title="$1" -f due_on="${2}T23:59:59Z" \
        -f description="$3" >/dev/null 2>&1 || echo "  ($1 exists, skipping)"
  echo "  $1"
}
mkmilestone M0 2026-09-20 "Team formed, hardware ordered, repo and CLAUDE.md initialized"
mkmilestone M1 2026-09-27 "Problem memo; transcripts copied out"
mkmilestone M2 2026-10-11 "Design document; both sensors alive; BASELINE COLLECTION STARTS"
mkmilestone M3 2026-11-01 "Checkpoint demo: pipeline end to end, one mechanism live"
mkmilestone M4 2026-11-29 "Feature freeze; 48-hour soak begins"
mkmilestone M5 2026-12-06 "Final submission"
mkmilestone M6 2026-12-18 "Demo day"

# ---------------------------------------------------------------- issues

echo "== issues =="
declare -A NUM   # slug -> issue number

mkissue() {  # slug, milestone, labels(csv), assignee, title, body
  local slug="$1" ms="$2" labels="$3" who="$4" title="$5" body="$6"
  if [[ $DRY -eq 1 ]]; then
    echo "  DRY: [$ms] $title  ($labels, ${who:-unassigned})"
    NUM[$slug]=0
    return
  fi
  local url
  url="$(gh issue create --title "$title" --body "$body" \
          --label "$labels" --milestone "$ms" \
          ${who:+--assignee "$who"} 2>/dev/null)" \
    || { echo "  FAILED: $title" >&2; return; }
  NUM[$slug]="${url##*/}"
  echo "  #${NUM[$slug]}  $title"
}

# Set these to the real GitHub usernames before running.
CAMDEN="${CARWATCH_CAMDEN:-@me}"
LANCE="${CARWATCH_LANCE:-}"          # leave empty until Lance's username is known

B() { printf '%s\n' "$@"; }   # body helper: one arg per line

## ---- M0
mkissue team M0 task "$CAMDEN" \
"M0 admin: register the team as Camden Thomas + Lance Baron" \
"$(B "Formal team registration for the CS370 term project." "" "Done when: both names are on record with the course.")"

mkissue order M0 "hardware,critical-path" "$CAMDEN" \
"M0 hardware: order the core BOM (and the ELM327 in the same cart)" \
"$(B "See docs/hardware/BOM.md — Pi 4, 32GB A2 microSD x2, MCP2515+TJA1050, OBD2 pigtail," \
     "12V->5V automotive buck converter, jumpers. ~\$90." "" \
     "**Also order the \$12 ELM327 now.** It unblocks the PID survey weeks before the" \
     "MCP2515 matters, and the PID survey answers the project's named risk." "" \
     "Critical path B. The handout names shipping time as the most common silent" \
     "schedule-killer, twice. Blocks every piece of physical work in the project." "" \
     "Done when: order confirmations are in the thread and ETAs are noted here.")"

mkissue honda M0 "question,critical-path" "$LANCE" \
"M0 question: confirm the Honda's year, model, engine and transmission" \
"$(B "Blocks the PID survey (and therefore diagnostic #1) and the CAN bitrate choice." "" \
     "Need: year, model, engine, transmission." "" \
     "Answer here in one line and this closes. Decision D-007 in docs/DECISIONS.md.")"

mkissue dates M0 question "$CAMDEN" \
"M0 question: verify M0-M6 dates against the syllabus" \
"$(B "docs/milestones.md assumes an Aug 24 2026 semester start, which puts M0 due" \
     "2026-09-20. The handout gives week numbers, not dates." "" \
     "If the assumption is off by even one week, the M4 soak start moves — and M4" \
     "already lands on Thanksgiving week." "" \
     "Answer: paste the real milestone dates. Then correct docs/milestones.md and the" \
     "GitHub milestone due dates. Decision D-008.")"

mkissue soakq M0 "decision,critical-path" "$CAMDEN" \
"M0 decision: 48-hour soak on a synthesized CAN source — ask Pallickara" \
"$(B "Handout §5: 'The 48-hour soak and the live demonstration run on live sensors.'" \
     "Our PC-side CAN generator is synthesized data by that definition." "" \
     "Draft email ready at docs/professor-email-draft.md with all three options." "" \
     "**Build as if the answer is no.** Fallback is adding an MPU-6050 + DS18B20 (~\$8)" \
     "so the soak runs on live sensors while CAN is replayed and labeled." "" \
     "Decision D-006. Must close before M4.")"

mkissue ghsetup M0 task "$CAMDEN" \
"M0 admin: configure branch protection, labels, milestones and the board" \
"$(B "See .github/pull_request_template_note.md for the exact settings:" \
     "require PR, 1 approval, dismiss stale approvals, require conversation resolution," \
     "block force pushes, block deletions." "" \
     "Prefer merge commits over squash — our granular history is a graded deliverable" \
     "and squashing destroys it." "" \
     "Board columns: Backlog / Blocked / Ready / In progress / In review / Done.")"

mkissue cleanup M0 task "$CAMDEN" \
"M0 admin: raise cleanupPeriodDays on both machines" \
"$(B "Transcripts purge after 30 days by default; this project runs 15 weeks." \
     "Losing Week-5 transcripts in Week 15 is a foreseeable loss and is not excused." "" \
     "Add to .claude/settings.json on BOTH machines:" \
     '    {"cleanupPeriodDays": 180}' "" \
     "Then verify ~/.claude/projects/ path and that the copy-out command works.")"

mkissue lancesign M0 task "$LANCE" \
"M0 review: Lance reads and signs the decision log" \
"$(B "docs/DECISIONS.md — at minimum §A.1 (MCP2515 over Bluetooth), §B.1 (mechanism" \
     "commitments), §C.1 (append-only storage) and §D.1 (self-trained model)." \
     "Those four bind your subsystems." "" \
     "Sign by changing 'Lance ⬜' to 'Lance ✅' on entries you agree with, in a commit" \
     "you author. Open an issue for anything you want to contest.")"

## ---- M1
mkissue pidsub M1 "hardware,critical-path" "$CAMDEN" \
"M1 hardware: supported-PID survey on the Outback" \
"$(B "Mode 01 PIDs 0x00 / 0x20 / 0x40 / 0x60 return bitmaps of supported PIDs." \
     "Ten minutes with the ELM327. No custom hardware needed." "" \
     "**Most important question: is oil pressure published as an analog value, or only" \
     "as a binary low-pressure switch?**" "" \
     "Record results in docs/hardware/pid-survey.md.")"

mkissue pidhonda M1 "hardware,critical-path" "$LANCE" \
"M1 hardware: supported-PID survey on the Honda" \
"$(B "Same procedure as the Outback survey. Record in docs/hardware/pid-survey.md.")"

mkissue oilq M1 "decision,critical-path" "" \
"M1 decision: is analog oil pressure available? If not, what replaces diagnostic #1?" \
"$(B "This is the project's named risk (PROBLEM.md)." "" \
     "If neither vehicle publishes analog oil pressure, options in preference order:" \
     "  1. Sniff manufacturer-specific frames for a pressure value" \
     "  2. Add a physical sender at the sender port (real work on a daily driver —" \
     "     do not commit to this after Week 10)" \
     "  3. Replace diagnostic #1 with an oil-temperature thermal-load metric, and" \
     "     declare the substitution in DESIGN.md §5" "" \
     "Record the outcome in docs/DECISIONS.md §E.1.")"

mkissue nameuser M1 question "$CAMDEN" \
"M1 question: name the real user and vehicle for PROBLEM.md" \
"$(B "The handout rejects personas: 'people who might want to monitor things' is not a" \
     "user, and neither is 'a cautious driver'." "" \
     "Need a real person and a real car — ideally two, one work truck and one commuter," \
     "since that contrast IS the thesis: the same sticker says 5,000 miles to both cars" \
     "and the cars disagree." "" \
     "Memos that fail this are returned for revision, costing a week.")"

mkissue memo M1 task "$CAMDEN" \
"M1 docs: draft PROBLEM.md, all six required headings" \
"$(B "user / problem / why a device / sensors / mechanisms / risk." \
     "One page. Must survive a skeptical stranger. Template already in PROBLEM.md.")"

mkissue memorev M1 task "$LANCE" \
"M1 review: Lance reviews PROBLEM.md against the phone-app test" \
"$(B "Two questions to answer honestly:" \
     "  1. Why must this be physically present and always awake?" \
     "  2. Why doesn't a phone app already solve it?" "" \
     "If either answer is weak, say so now — a returned memo costs a week.")"

mkissue tx1 M1 task "" \
"M1 process: transcript copy-out, both partners" \
"$(B '    cp ~/.claude/projects/*CS370*/*.jsonl partners/<name>/transcripts/' "" \
     "Raw. Not summarized, not exported, not retyped. Both partners, separately.")"

## ---- M2
mkissue bench M2 "hardware,critical-path" "$CAMDEN" \
"M2 hardware: MCP2515 bench bring-up on the Pi, no car" \
"$(B "1. Meter the module VCC and MISO with the Pi DISCONNECTED — many cheap boards" \
     "   drive 5V onto MISO and will silently kill the Pi." \
     "2. Note the crystal: 8 MHz vs 16 MHz changes the overlay and a wrong value" \
     "   produces a silently dead bus." \
     "3. dtoverlay + dtparam in config.txt, reboot, 'ip link show can0'." "" \
     "See docs/hardware/wiring.md. Wire with the board powered off.")"

mkissue twonode M2 hardware "$CAMDEN" \
"M2 hardware: two-node bench CAN bus — prove frames move before a car is attached" \
"$(B "MCP2515 module + USB-to-CAN adapter on the same bench bus." "" \
     "Do not skip this. Debugging a silent bus with a car attached is debugging two" \
     "unknowns at once.")"

mkissue firstcap M2 "hardware,critical-path" "$CAMDEN" \
"M2 hardware: first live capture from a real car" \
"$(B "Key to accessory first, then running. 'candump can0 > capture.log'." "" \
     "This is the M2 exit criterion: both sensors electrically alive.")"

mkissue baseline M2 "critical-path" "" \
"M2 CRITICAL PATH: start baseline collection on BOTH vehicles" \
"$(B "**This is the single most schedule-critical task in the project.**" "" \
     "Diagnostic #1 claims oil pressure degrades measurably across an oil-change" \
     "interval. That needs a real interval to develop — 5-7 weeks of driving — and" \
     "calendar time cannot be bought back by working harder in Week 14." "" \
     "Crude is fine. 'candump' to a file on a timer is fine. **On time is not optional.**" \
     "A crude capture that starts now beats an elegant one that starts three weeks late." "" \
     "If this has not started by the M2 deadline, diagnostic #1 is dead — decide that" \
     "consciously, record it in DECISIONS.md, and narrow the claim in writing.")"

mkissue recfmt M2 decision "$LANCE" \
"M2 decision: finalize the on-disk record format" \
"$(B "Fields, framing, CRC placement, and the src=live|replay|synth provenance tag." \
     "Draft already in include/store/record.h." "" \
     "Changing this after M3 means migrating captured baseline data. Decide carefully.")"

mkissue fsync M2 decision "$LANCE" \
"M2 decision: fsync discipline — batch size and interval" \
"$(B "There is no correct value, only one you can justify from user requirements." "" \
     "The trade: larger batches are faster and easier on the SD card, but lose more" \
     "when the key turns off mid-write — which happens several times a day, by design." "" \
     "Write the argument in DESIGN.md §4 BEFORE writing the code. The defense will ask.")"

mkissue archdiag M2 task "$CAMDEN" \
"M2 docs: architecture diagram with a rate on every arrow" \
"$(B "In the spirit of the handout's Figure 1. The rates are not decoration — they are" \
     "what makes the mechanism justifications checkable." "" \
     "Skeleton already in DESIGN.md §1; fill every [N].")"

mkissue failtable M2 task "" \
"M2 docs: failure-mode table" \
"$(B "Per component: how it fails, how it is detected, what the system does, what the" \
     "log shows." "" \
     "**The soak grades this table's honesty.** Write the modes you are afraid of, not" \
     "the ones you have already handled.")"

mkissue evalplan M2 task "$LANCE" \
"M2 docs: evaluation plan with committed target numbers" \
"$(B "Each measurement with its method and its target. Numbers committed now are twice" \
     "as credible when hit later — and instructive either way.")"

mkissue substit M2 task "$CAMDEN" \
"M2 docs: constraints and substitutions section" \
"$(B "What the ideal build would use, what we are actually using, what the gap costs." "" \
     "'A design document with nothing to report here has usually not met its hardware" \
     "yet.' This section is where the limitations section pays off in M5.")"

mkissue designdoc M2 task "" \
"M2 docs: assemble DESIGN.md and run the three-isolated-workers check" \
"$(B "A design-only reviewer, a parts-only buyer who never sees the design, and a" \
     "builder who only follows instructions must each succeed from their section alone." "" \
     "3-5 pages. The most leveraged hours of the whole project.")"

mkissue symlib M2 task "$LANCE" \
"M2 hw: populate CS370_Project_Library.kicad_sym" \
"$(B "sym-lib-table referenced this file and it did not exist — KiCad errors on project" \
     "open. An empty library is committed to resolve that; populate it as parts are drawn." "" \
     "Verify pin numbering against the datasheet with your own eyes. A wrong pin number in" \
     "a symbol is the class of error that survives every review and kills a board.")"

mkissue schpower M2 task "$LANCE" \
"M2 hw: draw the Power block" \
"$(B "OBD2 pin 16 (+12V, LIVE WITH THE KEY OFF), fuse, 12V->5V automotive buck, bulk and" \
     "bypass caps, the Pi 5V rail." "" \
     "Annotate every power net with its voltage domain. See" \
     "docs/hardware/schematic-conventions.md.")"

mkissue schcan M2 task "$LANCE" \
"M2 hw: draw the CAN block" \
"$(B "OBD2 pin 6 / pin 14, TJA1050, MCP2515, crystal + load caps, termination decision," \
     "INT to GPIO." "" \
     "**Put the crystal frequency in a value field, not a comment.** 8 MHz vs 16 MHz" \
     "changes the device-tree overlay and a wrong value produces a silently dead bus.")"

mkissue schpi M2 task "$LANCE" \
"M2 hw: draw the Pi interface block" \
"$(B "40-pin header, SPI0 (CE0/MISO/MOSI/SCLK), the interrupt GPIO, grounds." "" \
     "The Pi has no 5V-tolerant inputs and no over-voltage protection.")"

mkissue meterv M2 "hardware,critical-path" "$CAMDEN" \
"M2 hw: measure MCP2515 module VCC and MISO with the Pi DISCONNECTED" \
"$(B "Cheap MCP2515 breakouts vary. Some run the TJA1050 at 5V and level-shift the" \
     "controller side; some do not, and those drive **5V onto MISO**, which kills a Pi" \
     "silently — sometimes taking the whole SoC with it." "" \
     "This cannot be answered from a datasheet, a product page, or by Claude. Meter it." \
     "Record the measured voltages and the date in docs/hardware/wiring.md.")"

mkissue hwgate M2 task "$LANCE" \
"M2 hw: make hwcheck green; regenerate wiring.md and BOM.md from the schematic" \
"$(B "ERC violations are build failures (decision D-010). Once green, 'make hwdocs'" \
     "regenerates the schematic SVG, bom.csv and the netlist." "" \
     "wiring.md pin tables and BOM.md become DERIVED from the schematic — a hand-kept pin" \
     "table disagrees with reality by Week 9; a generated one cannot.")"

mkissue tx2 M2 task "" "M2 process: transcript copy-out" "$(B "Both partners. Raw .jsonl.")"

## ---- M3
mkissue canirq M3 task "$CAMDEN" \
"M3 can: SocketCAN open + epoll on the interrupt-driven RX path" \
"$(B "Mechanism B. Instrument arrival->push latency into a histogram from the start —" \
     "retrofitting measurement is how evaluations end up with means and no distributions.")"

mkissue canpoll M3 task "$CAMDEN" \
"M3 can: --poll variant behind a flag for the mechanism-B comparison" \
"$(B "A flag, not a rewrite. The head-to-head comparison is a graded deliverable, so the" \
     "two paths must be switchable at runtime on identical hardware.")"

mkissue ringshm M3 task "$CAMDEN" \
"M3 ipc: move the ring into shared memory, prove cross-process" \
"$(B "src/ipc/ring.c is implemented and unit-tested single-process. This wires it into" \
     "shm so the producer and consumer are genuinely separate processes (mechanism E).")"

mkissue storewrite M3 task "$LANCE" \
"M3 store: record write path with the chosen fsync policy" \
"$(B "Drain the ring, sequence-account every slot, write CRC'd self-framing records.")"

mkissue storerecover M3 task "$LANCE" \
"M3 store: recovery scan, CRC verify, torn-tail truncation" \
"$(B "Mechanism D, the part that is actually graded. On open: scan from the last" \
     "known-good offset, verify CRCs, truncate the torn tail, and **log exactly what was" \
     "lost**. Silent recovery is as bad as no recovery.")"

mkissue supfork M3 task "" \
"M3 supervisor: fork/exec, waitpid, fd hygiene across restart" \
"$(B "No orphaned fds across restart. 'What happens to this fd when the child dies?' is" \
     "a demo-day question, verbatim.")"

mkissue supbackoff M3 task "" \
"M3 supervisor: backoff and restart-storm guard" \
"$(B "**The subtle part.** A physically unplugged sensor is NOT a crash. Restarting a" \
     "child in a tight loop because its hardware is gone is a restart storm, and it is" \
     "the failure most likely to appear at hour 31 of the soak." "" \
     "Distinguish 'crashed' from 'absent' and log the distinction.")"

mkissue heartbeat M3 task "" \
"M3 common: wire hourly heartbeats through every daemon" \
"$(B "Liveness, RSS, cumulative event counts. Required by the soak, and the source of" \
     "the RSS-vs-time plot in EVALUATION.md §3. log_heartbeat() already exists.")"

mkissue obdstatus M3 task "" \
"M3 interface: obdctl UDS transport + status verb" \
"$(B "First of five verbs. Resist the web app — a crisp CLI is more defensible and far" \
     "more extensible at the live-modification station on demo day.")"

mkissue replay M3 task "$LANCE" \
"M3 tools: replay harness preserving inter-sample timing" \
"$(B "Feeds a recorded capture through the REAL pipeline. A replay that collapses the" \
     "gaps is not testing the system that will run in the car." "" \
     "Everything it emits is labeled src=replay. Not optional.")"

mkissue nightly M3 "task,critical-path" "" \
"M3 soak: begin overnight small-scale runs NOW" \
"$(B "The handout is explicit: 'start soak-style overnight runs now, at small scale;" \
     "every leak found in Week 9 is a crisis avoided in Week 14.'" "" \
     "'make soakcheck' runs a one-hour miniature. Do not wait for M4 to discover the" \
     "system cannot survive an hour.")"

mkissue m3demo M3 task "" \
"M3 demo: checkpoint demonstration rehearsal (graded live, 10 points)" \
"$(B "Both sensors through the real pipeline into real storage, with at least one menu" \
     "mechanism working and instrumented.")"

mkissue tx3 M3 task "" "M3 process: transcript copy-out" "$(B "Both partners. Raw .jsonl.")"

## ---- M4 (abbreviated bodies — detail lives in PLAN.md §3)
mkissue bin M4 task "$LANCE" "M4 analysis: operating-point binning (RPM x load x coolant temp)" \
"$(B "Comparing a cold idle to a warm cruise is a false-alarm generator. This one step" \
     "removes more false positives than anything downstream. See DECISIONS.md §D.2.")"
mkissue welford M4 task "$LANCE" "M4 analysis: per-bin running mean and variance (Welford)" \
"$(B "Numerically stable online statistics. Per bin, per PID.")"
mkissue persist M4 task "$LANCE" "M4 analysis: persist learned baselines through a power cut" \
"$(B "Weeks of learned baseline must survive the key turning off. This requirement is" \
     "why mechanism D is non-optional.")"
mkissue mahal M4 task "$LANCE" "M4 analysis: multivariate residual (Mahalanobis distance)" \
"$(B "The signal is correlation between sensors, not any single value. Oil pressure is" \
     "SUPPOSED to rise with RPM. This is the 'an if-statement could not do this' proof.")"
mkissue trend M4 task "$LANCE" "M4 analysis: robust trend estimation (Theil-Sen)" \
"$(B "Honest slope through noisy data, resistant to the glitches every car log contains.")"
mkissue fsm M4 task "$LANCE" "M4 analysis: state machine with hysteresis and dwell" \
"$(B "A monitor that cries wolf twice gets unplugged, and an unplugged monitor is worse" \
     "than none. False-positive rate is a first-class metric.")"
mkissue train M4 task "$LANCE" "M4 tools: training pipeline and weights format with provenance header" \
"$(B "Our architecture, our training loop, our data, our weights. Header records the" \
     "training commit and the fixture set. No third-party pretrained weights.")"
mkissue infer M4 task "$LANCE" "M4 analysis: C inference over our own weights" \
"$(B "No interpreter in the runtime path — boundary legibility, and 30 MB of RSS we" \
     "cannot afford in a memory-stability test we are graded on.")"
mkissue obdrest M4 task "" "M4 interface: remaining four obdctl verbs" \
"$(B "verdicts, series, baseline, faults — each with exact, documented semantics.")"

mkissue expirq M4 "task,critical-path" "$CAMDEN" \
"M4 experiment: interrupt vs polling — latency distribution and CPU, idle and loaded" \
"$(B "**Required by mechanism B. Cannot be cut.** Present as an experiment: method," \
     "data, conclusion. Include the CPU cost of the polling design at the rate needed to" \
     "match the interrupt design's drop rate — that comparison is the point.")"
mkissue expdrop M4 "task,critical-path" "$CAMDEN" \
"M4 experiment: no-drop under CPU contention, proven by sequence accounting" \
"$(B "**Required by mechanism F.** Sustained rate, zero drops, under stress-ng.")"
mkissue expcrash M4 "task,critical-path" "$LANCE" \
"M4 experiment: crash consistency — N power-cut trials" \
"$(B "**Required by mechanism D.** Pull power mid-write, repeatedly. Record recovery" \
     "outcome and bytes lost per trial.")"

mkissue keyoff M4 hardware "$CAMDEN" "M4 hardware: measure key-off current draw" \
"$(B "OBD2 pin 16 is live with the key off — that is what makes the unattended soak" \
     "possible and a dead battery possible. Fuse it. Measure before leaving it overnight." \
     "Record the number in EVALUATION.md.")"
mkissue fvac M4 hardware "" "M4 ground truth: induce and label a vacuum leak, both vehicles" \
"$(B "Briefly disconnect a small vacuum line. Reversible, safe, moves LTFT within a" \
     "minute. Label the recording with exactly what was done and when.")"
mkissue fcool M4 hardware "" "M4 ground truth: induce and label a cooling anomaly" \
"$(B "Partially block radiator airflow with cardboard to force an abnormal thermal" \
     "profile.")"
mkissue funplug M4 hardware "$CAMDEN" "M4 ground truth: sensor unplug — and demo-day rehearsal" \
"$(B "They WILL unplug a sensor mid-demo, at a moment of their choosing. Announced in" \
     "the handout so nobody can call it unfair. Rehearse it until it is boring.")"
mkissue foil M4 "hardware,critical-path" "" \
"M4 CRITICAL PATH: capture across a real oil change on both vehicles" \
"$(B "Critical path A. This is the one that cannot be faked and cannot be rushed." \
     "Depends entirely on baseline collection having started at M2.")"
mkissue encl M4 hardware "$CAMDEN" "M4 hardware: enclosure and in-vehicle mounting" \
"$(B "The handout is explicit that a food container is an acceptable enclosure.")"

mkissue freeze M4 "task,critical-path" "" "M4 FEATURE FREEZE" \
"$(B "No new features after this line. Bugs only." "" \
     "Everything downstream — the soak, the evaluation, the demo — runs on what exists" \
     "at this moment.")"
mkissue soak1 M4 "task,critical-path" "" "M4 soak: 48-hour run #1, with a scheduled injected fault" \
"$(B "Must show: timestamped startup, hourly heartbeats with RSS and event counts, at" \
     "least one injected fault with detection/degradation/recovery visible, and an" \
     "orderly end state." "" \
     "**Raw, unedited logs are the deliverable. Do not tidy them.**")"
mkissue soak2 M4 task "" "M4 soak: 48-hour run #2 (the margin)" \
"$(B "'Begin the graded 48-hour soak with margin to run it twice. You will want to run" \
     "it twice.' — handout §16.")"
mkissue sanitize M4 "task,critical-path" "" "M4 quality: make asan and make memcheck clean everywhere" \
"$(B "ASan/valgrind findings cap that component's final-system contribution at 50%." "" \
     "'A device that leaks for 48 hours and happens not to die has not passed the soak." \
     "It has outrun it.'")"
mkissue tx4 M4 task "" "M4 process: transcript copy-out" "$(B "Both partners. Raw .jsonl.")"

## ---- M5
mkissue plots M5 task "$LANCE" "M5 tools: soak figures — RSS, CPU, event counts over 48h" \
"$(B "The heartbeats give you this series for free. Plot it. A flat RSS line is the" \
     "whole point.")"
mkissue evalreport M5 "task,critical-path" "" "M5 docs: write EVALUATION.md" \
"$(B "Latency distributions (idle + loaded), per-process CPU/RSS over the full soak, one" \
     "domain metric, fault-injection results with log excerpts, the required mechanism" \
     "comparisons, and an honest limitations section." "" \
     "'An honest limitations section is worth more at the defense than a suspiciously" \
     "perfect results section, and we notice which one we are reading.'")"
mkissue designfinal M5 task "" "M5 docs: DESIGN.md as-built, with a changelog of what M2 got wrong" \
"$(B "Graded. An empty changelog is not credible.")"
mkissue readmecheck M5 "task,critical-path" "" "M5 docs: verify README on a clean Pi, by someone who has never built it" \
"$(B "'If the TA can't boot it, the TA can't grade it.' Find a third party.")"
mkissue promptlog M5 "task,critical-path" "" "M5 process: PROMPTLOG.md complete, both partners" \
"$(B "6-10 annotated episodes each, including: one revised plan, one rejected diff, one" \
     "tool-output debugging loop with real hardware evidence, and one review of the" \
     "partner's work." "" \
     "One of these is defended out loud on demo day.")"
mkissue reflection M5 task "" "M5 process: REFLECTION.md, both partners" \
"$(B "One page each: where the agent was most and least reliable against real hardware," \
     "one bug it introduced that you caught, and the distance between what you wanted to" \
     "build and what you built.")"
mkissue commitaudit M5 "task,critical-path" "$CAMDEN" "M5 process: commit audit" \
"$(B "git shortlog -sn --all" "" \
     ">= 40 meaningful commits, both partners well represented. A partner under a" \
     "quarter of substantive commits is probed hard at the individual defense.")"
mkissue tarball M5 task "" "M5 submit: assemble the tarball and M5 transcript copy-out" \
"$(B "Source + Makefile + README, PROBLEM/DESIGN/EVALUATION, raw soak logs, CLAUDE.md," \
     "per-partner PROMPTLOG + REFLECTION + raw .jsonl, full git history.")"

## ---- M6
mkissue demorehearse M6 task "" "M6 demo: full rehearsal with surprise fault injection" \
"$(B "Each partner injects a fault the other did not schedule. Graded behaviors:" \
     "graceful degradation, honest reporting through our own interface, and the log line" \
     "proving the system noticed.")"
mkissue livemod M6 "task,critical-path" "" "M6 demo: live-modification practice" \
"$(B "Each partner, alone at the keyboard, makes one small previously-unseen change to" \
     "their OWN subsystem from the submitted source: a new heartbeat field, a changed" \
     "threshold semantic, a new CLI flag." "" \
     "Small for an author. Revealing for anyone else. Practice on each other.")"
mkissue crossquiz M6 "task,critical-path" "" "M6 demo: cross-boundary quiz" \
"$(B "Demo day guarantees each partner one question from across the ownership boundary." \
     "Camden must be able to explain the logbook; Lance must be able to explain the ring." "" \
     "Quiz each other until neither hesitates.")"

# ---------------------------------------------------- dependencies

echo "== dependencies =="
dep() {  # blocked_slug, blocker_slug
  local a="${NUM[$1]:-}" b="${NUM[$2]:-}"
  [[ -z "$a" || -z "$b" || "$a" == "0" || "$b" == "0" ]] && { echo "  DRY/skip: $1 <- $2"; return; }
  if gh issue dependency add "$a" --blocked-by "$b" >/dev/null 2>&1; then
    echo "  #$a blocked by #$b"
  else
    gh issue comment "$a" --body "Blocked by #$b" >/dev/null 2>&1 \
      && echo "  #$a blocked by #$b (as a comment — 'gh issue dependency' unavailable;" \
              "link them in the web UI)"
  fi
}

dep pidsub order;      dep pidhonda order;    dep pidhonda honda
dep oilq pidsub;       dep oilq pidhonda
dep memo nameuser;     dep memo oilq;         dep memorev memo
dep bench order;       dep twonode bench;     dep firstcap twonode
dep baseline firstcap
dep fsync recfmt
dep failtable archdiag; dep evalplan archdiag; dep substit oilq
dep designdoc archdiag; dep designdoc failtable; dep designdoc evalplan
dep designdoc substit;  dep designdoc fsync
dep schcan bench;      dep schpi bench;       dep meterv order
dep hwgate schpower;   dep hwgate schcan;     dep hwgate schpi
dep hwgate symlib;     dep hwgate meterv
dep substit hwgate
dep canirq firstcap;   dep canirq designdoc;  dep canpoll canirq
dep ringshm designdoc
dep storewrite recfmt; dep storewrite fsync;  dep storerecover storewrite
dep supfork designdoc; dep supbackoff supfork; dep heartbeat supfork
dep obdstatus supfork; dep replay storewrite; dep nightly heartbeat
dep m3demo canirq;     dep m3demo storewrite; dep m3demo heartbeat
dep bin storewrite;    dep welford bin;       dep persist welford
dep persist storerecover
dep mahal welford;     dep trend welford
dep fsm mahal;         dep fsm trend
dep train mahal;       dep infer train;       dep infer fsm
dep obdrest obdstatus; dep obdrest fsm
dep expirq canpoll;    dep expdrop ringshm;   dep expcrash storerecover
dep keyoff firstcap
dep fvac bin;          dep fcool bin;         dep funplug supbackoff
dep foil baseline;     dep encl keyoff
dep freeze infer;      dep freeze obdrest;    dep freeze expirq
dep freeze expdrop;    dep freeze expcrash
dep soak1 freeze;      dep soak1 funplug;     dep soak2 soak1
dep sanitize freeze
dep plots soak2;       dep evalreport plots
dep evalreport expirq; dep evalreport expdrop; dep evalreport expcrash
dep evalreport fvac;   dep evalreport fcool
dep designfinal freeze; dep readmecheck freeze; dep reflection evalreport
dep tarball evalreport; dep tarball designfinal
dep tarball promptlog;  dep tarball reflection
dep demorehearse soak2; dep livemod readmecheck; dep crossquiz readmecheck

echo
echo "Done."
[[ $DRY -eq 1 ]] && echo "(dry run — nothing was created)"
echo
echo "Next:"
echo "  1. Add all issues to the project board and set up the columns."
echo "  2. Set CARWATCH_LANCE to Lance's GitHub username and reassign his issues."
echo "  3. Answer the M0 questions — they unblock most of M1."
