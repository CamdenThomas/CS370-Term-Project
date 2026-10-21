# Revision plan: the box redesign and the road to M2

> **STATUS: PROPOSAL, written 2026-10-21 in Camden's session. Nothing in this file is a
> decision.** A decision exists only when it is recorded in `docs/DECISIONS.md` as
> ⚠️ UNREVIEWED, in the same commit as whatever enacts it, and signed later by a human.
> Claude never marks a decision reviewed or 🔒 LOCKED (`CLAUDE.md` §0).
>
> **Audience: a fresh Claude session with no memory of the conversation that produced this
> file.** Everything it needs is here or named by path. Where this file says "Camden
> decided", that is a statement about the 2026-10-21 conversation, and §1 says exactly
> which statements those are. Everything else is Claude's recommendation and must be
> confirmed with Camden before it is written down as settled.

## 0. How to use this file

1. Do the session-start steps in `CLAUDE.md` §0 first (read `docs/milestones.md`, the
   review queue at the top of `docs/DECISIONS.md`, run the board status, check `git
   status` is clean, `git pull`, create a branch **before** the first edit).
2. Read §1 (provenance), then §2 and §3 (the product and its engineering numbers), then
   §4 (what is wrong in the repo today).
3. Work through §8 in order. Each stage is one pull request; each commit inside it is one
   responsibility (`CLAUDE.md` §7.3). Do not skip ahead: later stages assume the earlier
   decisions exist.
4. Stop and ask Camden at every item in §11. Do not guess at them.
5. Obey §12 (the do-not list) at all times.
6. At the end of the session do the `CLAUDE.md` §0 end steps, including the transcript
   copy-out (`partners/camden/transcripts/README.md` now has the macOS command).

This plan edits documents first and code later. No money is spent by anything in it
(§10). Camden's standing instruction: be very sure of the paper plan before any part is
bought.

## 1. Provenance: what Camden decided, and what Claude only recommended

### 1.1 Decided by Camden in the 2026-10-21 conversation

| # | Statement |
| --- | --- |
| P1 | The team's 2008 Subaru Forester (non-turbo, Camden's car) is a testbed. Lance's 2015 Honda CR-V remains Lance's to use. **Neither car is required of anyone, and no document may restrict where either partner tests.** Camden's instruction, paraphrased: do not set limitations or restrictions for him, and especially not for Lance while the two of them are not communicating. |
| P2 | Tasks have no pre-assigned owners. Any open task is for whoever is available. For the handout's per-subsystem map, Camden chose **claim on first commit**: whoever lands a subsystem's first substantive commit is recorded as its owner. |
| P3 | The professor said the 48-hour soak means **48 hours of regular use** (not 48 hours powered): the car starts and stops, the product handles every cycle, is on and working every time the car runs, shuts down gracefully when the car shuts off, and auto-boots on the next start. Camden chose the reading "48 hours wall-clock of normal use". This has **not** been received in writing. |
| P4 | Camden approved **extending locked decision C.1** (append-only, CRC'd records) with write-ahead-log style transactions and checkpoints: a batch is `BEGIN`, records, `COMMIT`; recovery redoes committed work from the latest checkpoint and undoes uncommitted work. Camden's reason: it ties the project to the logging and checkpoint techniques taught in class. |
| P5 | `pisensor` (an MPU-6050 or similar on the Pi for mechanisms B and F) is **declined**. Mechanisms D and E stay the committed pair. |
| P6 | The Raspberry Pi 4 stays (both partners already own one). |
| P7 | **The product is one box.** The box has a single clean USB-C port (like a computer's) as its only power input. The user plugs in whatever USB-C charger and cable they already own. Camden: "no need to reinvent the charger." The team designs the box; the team buys only the OBD2 Bluetooth dongle and a cable. The box limits what any source can push, and supplements weak power from a small stored-energy element. |
| P8 | The stored-energy element is **small and short-lived by design**: its jobs are supplementing weak input power, bridging the dip at engine start, and giving the Pi enough time for a clean shutdown. It is **never** an always-on source and is not to be designed as one; about one minute of stored runtime is the ceiling and a few seconds to tens of seconds is the goal. The Pi is **intentionally off at rest**, which is what makes the 48-hour soak definition (P3) valid. |
| P9 | The Bluetooth OBD2 dongle is assumed to work as expected (Bluetooth Classic serial profile, **not BLE**). The box reads it, logs, and processes the data. |
| P10 | **Wake button: dropped.** The box has one port and one light and no buttons. |
| P11 | **One tri-color light.** Red means an issue, and "issue" means either a problem with the box or a problem with the car; the report says which. Yellow means starting or stopping on stored energy. Green means ready, all systems go. For pulse versus steady, Camden said whichever is safer, so §2.3 specifies pulsing. |
| P12 | The phone connects to the box (its own Wi-Fi) to view data and trends and to **download a report**. An app is "maybe": a stretch, not a commitment. |
| P13 | The box is "super small, sleek", with vents if needed. |
| P14 | **No shopping for now.** The paper plan comes first. |

### 1.2 Recommended by Claude, not confirmed by Camden (confirm before recording)

| # | Recommendation | Why it is still open |
| --- | --- | --- |
| R1 | The stored-energy element is a **supercapacitor bank**, not a lithium cell. | Camden said "small rechargeable battery" first, then described the seconds-scale role. He never chose the chemistry. §3.1 has the reasoning (no thermal runaway, no charge-temperature cutoff to design, works in a hot or frozen car). |
| R2 | A **short grace period** after the source disappears, longer than a crank dip and shorter than the stored runtime, before shutdown starts. | Needs the Forester's real supply-dip measurement. |
| R3 | **Report as one HTML file plus a CSV**, served by a C program in `src/interface/`. | Camden said "report" and "maybe an app". Format is unchosen. |
| R4 | Steady yellow while booting (a not-yet-running state), pulses everywhere once alive. | See §2.3 and §11. |
| R5 | The phone page may set the Pi's clock (the Pi has no battery-backed clock). | Conflicts with D-013's "writes nothing". §11. |
| R6 | Record the Forester as the car the first baseline and fault captures run on. | Follows from P1 but is not itself a decision about Lance. |

### 1.3 Unknown, must be measured or asked

| # | Unknown | How to resolve |
| --- | --- | --- |
| U1 | Which Mode 01 PIDs the Forester answers, and its OBD protocol. A 2008 US car is expected to speak CAN (ISO 15765-4) but this is **not verified**. MAP may be absent. | PID survey (§8, stage 3). Do not assume; the sensor list and fault areas depend on it. |
| U2 | The Forester's supply behavior at crank, and whether its sockets and ports switch with the key. | Meter. Free first test: a USB charger with a light in the socket, key off. |
| U3 | Real Pi 4 boot and shutdown time and power draw on this image. | USB-C power meter and `systemd-analyze`. |
| U4 | The professor's written confirmation of the soak definition (P3). | Email, drafted in §9. Camden sends it. |
| U5 | Which deliverable is due "next Wednesday", 2026-10-28. The board has no such date: M2 was due 2026-10-11 and M3 is due 2026-11-01. | Camden checks the syllabus. |
| U6 | Whether Lance agrees with any of this. Lance has reviewed **no** decision in `docs/DECISIONS.md`. | Camden talks to Lance. Until then everything is ⚠️ UNREVIEWED. |

## 2. The product, as revised

### 2.1 In one paragraph

carwatch is a small plastic box with one USB-C port and one three-color light. It lives in
a car, plugged into whatever USB-C charger the owner already has. When the car powers
the charger the box boots, listens to the car's engine sensors through a Bluetooth OBD2
dongle, records everything to a crash-safe log, learns what *this car's* normal looks like
at each operating point, and flags behavior that leaves it, naming the likely fault area.
When the car shuts off, power disappears; the box notices, finishes its writes on a small
amount of stored energy, and halts cleanly, then sits off until the next start. A phone
joins the box's own Wi-Fi to see status and trends and to download a report that says
what is wrong with the box or the car. The claim we defend (D-021, to be reworded to "the
car it is installed in" by D-022): *it learns that car's normal across its Mode 01 engine
sensors, flags behavior outside it, and names the likely fault area, with measured catch
and false-alarm rates on faults we induce.* Not "predicts failure."

### 2.2 The physical product

| Item | Specification |
| --- | --- |
| Shell | Heat-resistant plastic (a cabin sits well above 60 °C parked in sun; ordinary PLA softens near 60 °C, so use ABS, ASA or similar, to be confirmed against the shell's datasheet). **No metal around the Pi's antenna**: Bluetooth and Wi-Fi share one radio. Vent slots on the top and bottom for passive airflow. Target envelope on the order of 95 × 65 × 30 mm for a Pi 4 plus the power board (estimate, to be confirmed against real parts). A food-container prototype is acceptable per the handout; the sleek shell is the product intent. |
| Power port | One USB-C receptacle. The only power input. Wired as a plain **sink** (5.1 kΩ resistors on both CC pins) so any source offers 5 V only. |
| Light | One tri-color LED visible through the shell. See §2.3. |
| Buttons | None. |
| Other ports | None. The OBD2 dongle is a separate item in the car's OBD2 port and reaches the box over Bluetooth, so no wire of ours touches it. |
| Install | Plug the dongle into the OBD2 port, plug the box into any USB-C charger in the car. Nothing is cut, spliced, pierced or clamped. |
| Inside | Raspberry Pi 4, microSD, a power block (§3.1), the light, and nothing else on the header. |
| Not supported | A power source that stays live with the key off. The box is designed for power that disappears when the car turns off; on an always-live port it would simply stay on. This is an installation requirement, written down as a limitation, not a mode to engineer around. |

### 2.3 The light

One tri-color LED on three GPIOs (each through its own resistor; value to be set by the
schematic against the chosen LED's datasheet). Written **only by the supervisor** (the
rule from D-014, kept).

| Color and pattern | Meaning |
| --- | --- |
| Green, slow pulse (about 1 Hz) | Ready: booted, recording, all systems go. |
| Yellow, medium blink (about 2 Hz) | Running on stored energy: starting up through a supply dip, or shutting down. |
| Red, fast blink (about 4 Hz) | An issue, in the box **or** the car. The report says which. |
| Steady any color, or dark while the car is on | The device is hung or not running. Never a valid live state. |
| Dark while the car is off | Normal. The box is powered down. |

Priority when more than one applies: red over yellow over green.

**Why every live state pulses.** A GPIO keeps its last level after the process that drove
it dies. A steady green would freeze into a false all-clear if the supervisor crashed. If
every live state blinks or pulses, a frozen light of any color can only mean "not
running." This is D-014's reasoning, applied to three colors. Camden said to choose
whichever is safer, and this is.

**Open (§11):** what the light shows during the roughly 20 to 40 seconds the Pi spends
booting, when no software can drive it yet. Recommendation R4: hold the yellow LED on in
hardware from power-up until the supervisor takes over the GPIOs, so *steady yellow* means
"booting, not yet running," consistent with the rule above.

**What counts as red** (the supervisor sets it; the report explains it):

- Box: a child process is down or restart-limited; the adapter has been silent while the
  engine is running; storage is failing or the disk is nearly full; undervoltage
  (`vcgencmd get_throttled` non-zero) or thermal throttling; the stored-energy element
  failed its self-check.
- Car: a verdict has surfaced after hysteresis and dwell (D.3): a fault
  area was named. A sensor the car stopped answering is also a car-side event.

### 2.4 What the box does in a drive (behavior, end to end)

1. The owner starts the car. The charger energizes. The box begins charging its stored
   energy and boots the Pi (light per §2.3).
2. `supervisor` starts `obdd`, `storaged`, `analyzed` and the status-page server, each
   supervised (mechanism E). A heartbeat appears within 60 seconds.
3. `obdd` finds the Bluetooth dongle at `/dev/obd`, initializes it, and round-robins Mode
   01 requests (D-012). Samples go through the shared-memory ring (B.3, refuse-don't-
   overwrite) to `storaged`, which writes them in committed batches (D-025).
4. `analyzed` bins by operating point, updates per-bin statistics, computes Mahalanobis
   residuals and Theil–Sen trends, classifies, and runs the state machine with hysteresis
   and dwell. Verdicts surface only after dwell.
5. The light goes green once everything is up.
6. The car turns off. Power disappears. The box detects it, waits out the grace period
   (R2), then: stops capture, commits the final batch, writes a checkpoint, syncs, and
   halts the Pi (light yellow). The power stage then removes the Pi's supply and the box
   is dark.
7. If anything goes wrong in step 6 (stored energy empty, cable pulled, source returns
   mid-shutdown), the crash-consistent log still recovers at the next boot (D-025). The
   clean shutdown is the normal path; crash consistency is the guarantee.
8. At the next start the cycle repeats and the box reports in its log whether the previous
   session ended cleanly or was recovered.

### 2.5 The phone

- The Pi runs its own Wi-Fi access point with WPA2, a per-device passphrase, and **no
  upstream and no internet** (D-013). The phone joins it. The product has no cloud.
- A read-only status page shows status, current verdicts and trends. A **Download report**
  button returns the report (R3). Nothing on the page can change the device, except the
  clock question in §11 (R5).
- A native app is a stretch and is not planned for M2 to M5. The page works on any phone.
- **The box is off when the car is off, so the phone can reach it only while the car
  is running or while the box is plugged into any charger at home.** This is a property
  of the product (P8), and it should be written in the README as a feature: plug it into
  a phone charger at home and it boots and serves the same page.
- `obdctl` remains the primary, exact interface with its five verbs (D-013 and the
  handout's "crisp CLI" guidance). Any graded behavior must be demonstrable through
  `obdctl` alone.

**Report contents (proposal, to be finalized in board item `reportspec`):**

| Section | Content |
| --- | --- |
| Header | Box id, report time, whether wall-clock time is trustworthy (the Pi has no battery-backed clock), `src` mix of the data (live, replay, synth: always labeled) |
| Sessions | Each key cycle: boot id, start, end, duration, ended cleanly or recovered, records lost on recovery |
| Box health | Process liveness and restarts, ring overruns (must be zero), undervoltage and throttle events, temperature, stored-energy self-check, storage free space |
| Car findings | Each surfaced verdict: fault area, which sensors cooperated to call it, residual and trend evidence, when it began and ended, confidence |
| Baseline maturity | Hours of normal driving learned per operating-point bin, so the owner knows how far to trust the verdicts |
| Absent signals | Any PID the car stopped answering, and for how long |

### 2.6 The 48-hour soak, under the professor's definition (P3)

Not 48 continuous powered hours. A **48-hour wall-clock window of normal use** in which:

- the box powers on at every key-on, records, and shuts down at every key-off;
- every drive shows a timestamped boot, hourly heartbeats while running (the Pi is off
  while parked, so the log shows deliberate gaps; this needs the professor's written okay,
  U4), and an orderly end of each session;
- **at least one injected fault** (unplug the OBD2 dongle for ten minutes while the engine
  is running, or `kill -9` a child) with detection, degradation and recovery visible;
- the key-off power loss counts as each session's end, and the log proves whether it was a
  clean shutdown or a recovery.

The raw logs are the deliverable and are never tidied (`soak/README.md`). Because the soak
now needs a real car, the fake OBD2 port (`obdsim`) is a development tool only, never the
soak's data source. Leave margin to run the soak twice.

## 3. Engineering specification

All numbers marked **(E)** are Claude's estimates from general knowledge, **not
measurements**, and must be replaced by measured values before they appear in
`docs/DESIGN.md` as anything but "estimate". `CLAUDE.md` §8.4: a measured voltage beats an
opinion.

### 3.1 The power block

```text
 any USB-C source ─▶ [CC sink, 5.1 kΩ x2] ─▶ [input protection] ─▶ [current limit,
                                                                    inrush control]
                                                                           │
                                  ┌────────────────────────────────────────┤
                                  ▼                                        ▼
                      [stored-energy element        [power-path: source first, stored
                       + balancing + fuse]  ◀──────  energy fills only the gap]
                                                                           │
                                                          [5 V, 3 A converter] ─▶ Pi 4
   "source present" ─▶ GPIO (shutdown trigger)        "halt done" ◀── GPIO (cut the converter)
```

**Requirements** (each becomes a line in `docs/hardware/wiring.md` with a measurement):

1. **Sink only, 5 V only.** A dumb sink never negotiates above 5 V, so no source can
   offer the Pi more than 5 V. Add transient and reverse-polarity protection and a fuse.
2. **Input current limit** below the source's rating, and an **inrush limiter**: an empty
   stored-energy element looks like a short circuit and would trip a modest source.
3. **Power-path behavior:** the source feeds the Pi first; the stored energy covers only
   the shortfall (weak source) or the gap (crank dip, source loss).
4. **Source-present signal** on a GPIO, so the supervisor can detect power loss.
5. **Halt-done signal:** when the Pi has finished halting, the power stage removes the
   converter output so the box goes fully dark and the stored energy is not wasted. The Pi
   can drive a GPIO low at poweroff (the `gpio-poweroff` device-tree overlay); confirm on
   the board.
6. **Auto power-on:** when the source returns, the converter must enable again without a
   button. This is how "auto boots on the next start" is satisfied.
7. **Stored energy bounded by design:** the element is sized so that stored runtime is
   on the order of tens of seconds and never approaches a minute at the Pi's load (P8).

**Sizing sketch (E, to be replaced by measurement):**

| Quantity | Estimate |
| --- | --- |
| Pi 4 draw during boot and shutdown | about 3 to 5 W |
| Clean-shutdown energy | 10 to 15 s × 4 W ÷ 0.85 converter efficiency ≈ 50 to 70 J |
| Crank-bridge energy | 1 to 3 s × 5 W ≈ 5 to 15 J |
| A bank of two series 2.7 V supercapacitor cells, 10 F total, charged to about 5 V | ½·C·V² ≈ 125 J stored, about 105 J usable above a 2 V converter floor |
| Charging time from empty at a 1 A budget | C·ΔV ÷ I ≈ 10 F × 5 V ÷ 1 A ≈ 50 s |

**Consequence the next session must not forget:** a 10 F bank needs on the order of a
minute to charge at 1 A. A smaller bank charges faster; a weaker source charges slower.
At a **cold start the bank is empty**, so it cannot bridge the crank dip on the first
start of a session, and the Pi may reset during cranking. That is survivable only because
of the crash-consistent log (D-025). The clean-shutdown path at key-off is reliable (the
bank has had a whole drive to charge); the crank-bridge path at key-on is best-effort and
must be described honestly in the design document. Measure it before claiming it.

**Why a supercapacitor (R1) rather than a lithium cell.** Lithium cells must not charge
above about 45 °C or below 0 °C, and a car cabin leaves that window on both ends (a
Colorado winter and a parked summer afternoon). Many cheap Pi UPS boards have no
temperature cutoff. A supercapacitor has no thermal-runaway failure mode, tolerates a wide
temperature range (confirm against the part's datasheet), and fits a seconds-scale job
exactly. Its costs are low voltage per cell (so two in series, with balancing), very low
internal resistance (so it needs a fuse), and a fast self-discharge that does not matter
for a box that is off at rest. If Camden prefers a cell anyway: use LiFePO4 or a charger
with a temperature-sense input that inhibits charging outside the safe window, a protected
cell, and a physical position away from the Pi's heat. Whichever is chosen, **a part
number goes into the BOM only with its datasheet linked and a human having checked it**
(`CLAUDE.md` §8.4; Claude cannot see the board).

**Mechanical and thermal:** a Pi 4 throttles near 80 °C. Cabin heat plus a sealed shell
is the likeliest way to hit that. Specify a heatsink, vents, and a measured temperature
in a hot parked car (board item `thermal`). Supercapacitor and SD-card temperature ratings
must cover the cabin's range.

### 3.2 Power-event state machine (the supervisor's, mechanism E)

| State | Entered when | The supervisor does | Light |
| --- | --- | --- | --- |
| `BOOTING` | Linux up, supervisor starts | Starts children, waits for first heartbeats | steady yellow until it owns the GPIOs (R4), then yellow blink |
| `RUNNING` | All children healthy, source present | Normal operation | green pulse |
| `DEGRADED` | A child is down or restart-limited, adapter silent, storage or power fault | Keeps recording what it can, logs the distinction between absent and crashed | red blink |
| `GRACE` | Source-present signal lost | Starts a timer. Keeps recording. If the source returns, return to `RUNNING` | yellow blink |
| `SHUTTING_DOWN` | Grace timer expired with no source | Stops `obdd`, tells `storaged` to commit and checkpoint, `sync`, `shutdown -h` | yellow blink |
| `OFF` | Pi halted, converter cut | Nothing runs | dark |

Open edge cases to settle in `shutfsm` before any code: the grace period's length (longer
than a crank dip, shorter than the stored runtime); what happens if the source returns
mid-`SHUTTING_DOWN` (recommended: finish the clean shutdown, then let the box reboot,
which is safe if annoying); a false trigger from a supply dip while running; and what the
supervisor does when the stored-energy self-check fails.

### 3.3 Storage: write-ahead transactions and checkpoints (mechanism D, D-025)

This extends 🔒 C.1; it does not remove any of its rules. Records stay append-only,
self-framing (magic), and CRC'd over the whole record, and every record keeps its `src`
tag (C.2).

- **Transaction.** `storaged` drains the ring into a batch: `BEGIN(txn_id)`, the sample
  records, `COMMIT(txn_id)`, then one `fsync`. A batch is durable only once its `COMMIT`
  is on disk.
- **Checkpoint.** Periodically (rarely, to keep the write load and memory low), write a
  snapshot of the learned baselines plus the log offset and last committed `txn_id` it
  covers. Use **two alternating slots (A/B)**, each CRC'd and carrying a generation
  counter, so a checkpoint torn by a power cut can never destroy the previous good one.
- **Recovery at boot.** Load the newest valid checkpoint. Scan the log forward from its
  offset, verifying every CRC. **Redo**: re-apply every committed transaction to the
  baselines. **Undo**: any transaction with no `COMMIT` is truncated at its `BEGIN`; the
  scan stops at the first bad CRC and truncates the torn tail. Log exactly what was
  recovered and what was lost (counts and offsets).
- **Why undo is only truncation.** Nothing is ever overwritten in place (that is C.1's
  point), so there is no before-image to restore. State this plainly in the design
  document and at the defense; do not claim an undo log the design does not need.
- **Record format proposal (decision `recfmt`, to be confirmed):** add kinds
  `REC_TXN_BEGIN`, `REC_TXN_COMMIT`, `REC_CHECKPOINT` and `REC_BOOT` to `rec_kind_t` in
  `include/store/record.h`; add a `boot_id` to the header (there is a spare 16-bit field
  today; a 32-bit `boot_id` needs a layout change, so bump `REC_VERSION` to 2 and keep the
  known-answer CRC test unchanged); every session starts with a `REC_BOOT` record that
  states whether the previous session ended cleanly.
- **fsync policy (provisional, `fsync`):** one commit and one `fsync` per batch of about
  one second. Records are about 48 bytes (a 40-byte header plus a small payload, E), the
  request budget is about 10 to 20 per second (D-012), so about 1 KB/s, about 3.5 MB per
  hour of running, and on the order of 170 MB over 48 *running* hours (the 48 hour
  *wall-clock* soak records far less because the box is off while parked). The worst loss
  from a power cut that beats the clean shutdown is about one second of samples, which
  holds nothing a user would miss. The argument for the batch size must be written in
  `docs/DESIGN.md` §4 before the code is written (C.1's standing instruction).
- **Baseline persistence:** learned per-bin statistics are the model's state, so they must
  survive every key cycle. Checkpoint plus replay since the checkpoint is exactly how.
- **Clock:** the Pi has no battery-backed clock. Every record carries `mono_ns` and a
  `boot_id`; wall time is best-effort. Raspberry Pi OS ships `fake-hwclock`, which saves
  the time at a clean shutdown and restores it at boot, so a clean shutdown also keeps the
  clock close. Verify on the image.

### 3.4 The boot-time budget

A stock Pi 4 takes roughly 25 to 40 seconds to boot (E), and Camden's "a few seconds" is
not achievable without a trimmed image. The Forester's cooling warm-up is the most
informative part of a drive, so a lost first half-minute is a real cost: the design
document's substitutions section must say so. Mitigations to measure (board items
`bootbudget`, `bootimg`): disable unused services, `systemd-analyze blame`, start `obdd`
as early as possible, and let capture begin before the phone Wi-Fi is up.

### 3.5 Failure-mode rows to add or change in `docs/DESIGN.md` §3

New rows for the redesign (each needs Detection, Response, and a *real* log line, filled
in when the code exists):

| Component | Failure | Detection | Response |
| --- | --- | --- | --- |
| Power | Source disappears (normal key-off) | source-present GPIO | grace, clean shutdown (§3.2) |
| Power | Source dips at crank | source-present glitch shorter than the grace period | ride through; log the dip |
| Power | Source returns mid-shutdown | source-present rises during `SHUTTING_DOWN` | finish shutdown, reboot |
| Power | Stored energy empty at key-off (bank not charged, weak source) | self-check or failed shutdown timing | crash-consistent recovery next boot (D-025) |
| Power | Charger yanked mid-drive | same as key-off | same as key-off, logged as unplanned |
| Power | Weak source (undervoltage) | `vcgencmd get_throttled` | red; log; the stored energy supplements |
| Thermal | Pi throttles in a hot cabin | `vcgencmd get_throttled`, temperature | red; log; report shows it |
| Storage | SD card fails or fills | write error, free-space check | degrade and log; do not crash |
| Radio | Bluetooth and Wi-Fi contend | `btcoex` measurement | measure and document |
| Boot | The previous session ended unclean | `REC_BOOT` flag, recovery log | recover, report in the session list |
| Light | Supervisor dies with the light lit | steady light | by design reads "not running" (§2.3) |

The existing rows (adapter drop, ECU stops answering a PID, `obdd` crash, ring overrun,
disk full, torn write, analysis model missing, supervisor death, status page flood, clock)
stay, with the power rows for D-011 replaced by the above.

### 3.6 Measurements the paper plan depends on (nothing may be claimed without these)

| Measurement | Tool | Feeds |
| --- | --- | --- |
| Pi 4 current draw by state (boot, idle, loaded, shutdown) | USB-C power meter | stored-energy sizing |
| Boot time and shutdown time on the real image | `systemd-analyze`, timestamps in the log | boot budget, shutdown budget |
| Forester supply dip at crank, and key-off behavior of the user's own charger | meter or scope | grace period, bank size |
| Bank discharge time into the Pi, and charge time from empty | meter, stopwatch | whether the bank meets P8 |
| `vcgencmd get_throttled` after a cold start | on the Pi | undervoltage |
| Pi and bank temperature in a sealed shell, hot cabin | thermometer or Pi sensor | vents, parts ratings |
| Bluetooth plus Wi-Fi sharing one radio | latency and drop counts | `btcoex` |
| Forester PID survey, protocol (`ATDP`), VIN (Mode 09 `0902`) | the dongle | model inputs, fault areas |

## 4. Audit of the repository as it stands on 2026-10-21

These are facts found by reading every tracked file. Items marked 🐞 are defects to fix.

### 4.1 What exists

- Build is clean: `make`, `make test` and `make asan` pass on the baseline (output was
  seen this session; paste fresh output in each PR).
- Real code: the lock-free ring (`src/ipc/ring.c`, tested), CRC32 (`src/store/crc32.c`,
  known-answer tested), structured logging with heartbeat (`src/common/log.c`), and a
  record-header sketch (`include/store/record.h`). All five daemons are stubs that log
  "not implemented".
- Tools: `board_sync.py` and its test work; `pidscan.py`, `replay.py` and the soak
  scripts are stubs; `provision_pi.sh` and `install.sh` are TODO stubs that exit 1.
- The schematic (`electricalDrawing/electricalDrawing.kicad_sch`) is empty apart from a
  rectangle and the text "Power Block". The symbol library is empty.
- Board: 109 checkboxes, 29 ticked. M2 is 2/22 and was due 2026-10-11.
- Git history: about 138 commits by Camden under two author names and 7 by Lance.

### 4.2 Defects and contradictions

| # | Finding | Fix |
| --- | --- | --- |
| 🐞1 | `docs/DESIGN.md` §1 draws one SPSC ring feeding `storaged`, `analyzed` and `obdctl`. A single-producer single-consumer ring has one consumer. | Decide: two rings (one per consumer), or `analyzed` reads from `storaged`. Put the answer in DESIGN §1 and in D.2's corollary. Do not guess; ask Camden (§11). |
| 🐞2 | The status-page server is a supervised child but is not in the Makefile `DAEMONS` list, and no document says what binary it is. | Decide the binary's name and add it (stage 4); record in DESIGN. |
| 🐞3 | `m1.zip` (1.9 MB, an M1 snapshot including transcripts) is committed at the repository root (commit `747c857`). It is a stray file and duplicates `partners/*/transcripts/`. | `git rm` in its own `process` commit, after Camden confirms. History keeps it. |
| 🐞4 | `electricalDrawing/.history` and `.kicad_prl` are tracked though `.gitignore` excludes them (board item `kicadtrack`). | `git rm --cached` in its own commit. Hardware: needs the regenerated SVG and `make hwcheck` output in the PR per §7.12. |
| 🐞5 | The transcript copy-out glob `*CS370-Term-Project*` matches nothing on Camden's Mac (folder is `-Users-crash-dev-school-cs370-car-watch`). | Already fixed for Camden's README (commit `2b1b5ba`). Lance's README is his to fix; leave it. |
| 🐞6 | Camden appears under two author names (`Camden Thomas`, `CamdenThomas`). The process grade counts commits per partner. | Add a `.mailmap` mapping both to one identity. Do not rewrite history. Ask Camden for the canonical name and email first. |
| 🐞7 | Commit balance is about 138 to 7. The handout probes a partner with under a quarter of the substantive commits. | Not Claude's to fix. Say so in the session summary. Open tasks are now unowned (P2), which helps; Camden should tell Lance. |
| 🐞8 | `docs/DECISIONS.md` D.2's rationale uses oil pressure as its example, which D-021 removed. | One-line fix inside the stage-1 doc commits. |
| 🐞9 | The schematic conventions and the M2 board still include a CAN block (`schcan`) although D-019 removed CAN from the data path. | Cut the line, strike the block (stage 1, power decision). |
| 🐞10 | `docs/milestones.md` carries `canirq`, `canpoll`, `expirq`, `expdrop` as unconditional M3 and M4 items, but `docs/PLAN.md` §3 says they exist only if `pisensor` commits mechanisms B and F. `pisensor` is now declined (P5). | Cut them (a human ticks, §7). |
| 🐞11 | `include/store/record.h` comments say "see D-00x" for the wall clock; `obd/main.c` comments name the CR-V and `OWNER:` lines. | Replace with real references in the code stages. |
| 🐞12 | `docs/DESIGN.md` is a skeleton with every `[N]` rate, every Response and Log cell, storage figures and evaluation targets blank. | Stage 2. |
| 🐞13 | `README.md` Quick start clones `CS370-Term-Project` and says "Status: M1". | Fix in the power-decision commit and again at M5. |
| 🐞14 | `Makefile` tests use `-fsanitize` only under `make asan`; `make memcheck` needs valgrind, which is not installed on Camden's Mac. | Run `memcheck` on the Pi or a Linux machine; say so in the PR. Not a defect to "fix". |
| 🐞15 | `markdownlint-cli2` and `cspell` are not installed on Camden's Mac, so `CLAUDE.md` §8.9 lint was not run this session. | Install them (`npm i -g markdownlint-cli2 cspell`, Camden's call) or run them in CI. Until then hand-wrap at 90 columns and paste "not run" honestly. |

### 4.3 Handout checks the plan must keep passing

- §3.1 two physically distinct cooperating sensors: the OBD2 sensors are physically
  distinct (coolant, intake air, fuel trim, MAP or MAF, O2) and the pipeline fuses them.
  Defend this in DESIGN; a skeptical grader will ask why Mode 01 replies are "sensors".
- §3.2 two menu mechanisms implemented by us and **measured**: D (`expcrash`) and E
  (`expkill`). Measurements are required, not optional.
- §3.3: no hosted model, no pretrained weights, works with the network unplugged. The
  status page binds only to the box's own access point.
- §3.4: the 48-hour soak, redefined by P3 and awaiting written confirmation.
- §11 demo day: **the graders will unplug a sensor mid-demo.** For this product that
  means pulling the OBD2 dongle (or its Bluetooth link) with the engine running. The
  required behavior is graceful degradation (red light, honest report, a log line proving
  the system noticed). Rehearse it (board item `funplug`).
- §12 deliverables: at least 40 meaningful commits, both partners; no "final submission"
  commit pair; raw soak logs; CLAUDE.md evolving; per-partner PROMPTLOG, REFLECTION and
  raw transcripts.

## 5. Decisions to record

All new entries: ⚠️ UNREVIEWED, `By: Camden (in session) + Claude`, `Reviewed: Camden ⬜ /
Lance ⬜`, added to the **review queue at the top of `docs/DECISIONS.md` in the same
commit** that enacts them. Superseded entries move to §G with a pointer and are never
deleted. IDs continue from D-021. Text below is a draft; keep each entry's structure
(Decision, Why, Alternatives rejected, Cost) like the existing ones.

| ID | Title | Supersedes / touches | Source |
| --- | --- | --- | --- |
| D-022 | Testbeds: the 2008 Forester (Camden's) and the 2015 CR-V (Lance's); neither is required of anyone | Supersedes D-016. Amends D-007 (CR-V answer stays; Forester added) and the "this CR-V" wording of D-021 | P1 |
| D-023 | The 48-hour soak is 48 hours wall-clock of normal use across key cycles | Closes D-006 (E.3). Pending written confirmation (U4) | P3 |
| D-024 | Power and shutdown: one USB-C input, a short-lived stored-energy element, clean shutdown as the normal path, crash consistency as the guarantee, off at rest | Supersedes D-011 (and the key-off cost paragraph of A.6); revises D-019's "Pi powered from the car's USB". The element's chemistry is an ❓ OPEN sub-decision (R1) | P7, P8, P9, P10, P13 |
| D-025 | Write-ahead transactions and A/B checkpoints | Extends 🔒 C.1 with Camden's explicit yes (P4). C.1 stays 🔒 and gains a pointer line. Settles `walspec`; narrows `recfmt` | P4 |
| D-026 | No pre-assigned task owners; ownership of a subsystem by first substantive commit | Supersedes 🔒 F.2 and `CLAUDE.md` §6 with Camden's explicit yes (P2). Touches `CLAUDE.md` §0 step 3 | P2 |
| D-027 | One tri-color status light, every live state pulsing | Supersedes D-014 | P10, P11 |
| D-028 | The phone page gains a report download; an app is a stretch | Extends D-013 (does not supersede). The clock-setting write (R5) is ❓ OPEN | P12 |
| D-029 | `pisensor` declined; B and F out of scope; D and E are the committed pair | Resolves the `pisensor` item and the "only if" conditions in D-017 / B.1 | P5 |

**Rules for writing them.** Never reopen a 🔒 decision without saying so in the commit
and PR and pointing at Camden's explicit yes (P2 and P4 are exactly that). Do not mark
anything reviewed or locked. State in each entry's `Why` that the human reasons came
from Camden's session and cite §1 of this file.

**Wording the claim (D-022).** D-021's claim becomes: *"carwatch learns the normal of the
car it is installed in across that car's Mode 01 engine sensors, flags behavior outside
it, and names the likely fault area. We report how often it catches faults we induce and
how often it false-alarms."* The measured results will come from whichever car the data
was captured on (initially the Forester), labeled with the car.

**D-024 must include these numbers and caveats**: the stored-energy ceiling (about one
minute at the Pi's load, goal tens of seconds), the estimates of §3.1 labeled as
estimates, the cold-start limit on crank bridging, the unsupported-always-live-source
limitation, and that clean shutdown is **not** a hold-up supply: if the element is empty
the box falls back to D-025 recovery.

## 6. File-by-file revision list

"Stage" refers to §8. Every doc edit is made inside the commit of the decision it belongs
to, so the repository is never half-consistent (`docs/PROCESS.md`, "For documentation").

### 6.1 Rules and index files

| File | Change | Stage |
| --- | --- | --- |
| `CLAUDE.md` §0 step 3 | Replace "State whose session this is. Do not edit outside that owner's area (§6)" with "State whose session this is (Camden's or Lance's) for transcript and attribution purposes; any open task may be taken." | 1 (D-026) |
| `CLAUDE.md` §3, §2.3 | §3: the device is a box with one USB-C port and one light; testbeds per D-022; claim reworded. §2.3 ("works with the network unplugged") stays. | 1 (D-022, D-024) |
| `CLAUDE.md` §6 | Replace the ownership table with the claim-on-first-commit rule and a pointer to `docs/DESIGN.md` §7, where the realized map is recorded. Keep "first authorship and answerability at the defense, not exclusivity". | 1 (D-026) |
| `CLAUDE.md` §7.11 | Unchanged. Do not weaken "Claude never ticks a question, decision or hardware line". | none |
| `docs/README.md` | Add `REVISION_PLAN.md` to the index (this file's own commit does that), then update the hardware and ownership rows as decisions land. | now, 1 |
| `docs/PLAN.md` | §1 critical paths: path A (baseline) unchanged; path B (hardware lead time) now also includes the power block parts. §3 cut list: add "the report download polish" and "any app" below the phone page. | 1 (D-024, D-028) |
| `docs/PROCESS.md` | Transcripts section: mention the per-machine folder name. No other change. | 1 |

### 6.2 Product and design documents

| File | Change | Stage |
| --- | --- | --- |
| `README.md` | Status line; one-paragraph product description (box, one port, one light); the light table (§2.3); the "plug it into a charger at home to view" note; hardware section rewritten around the box; quick start text still says "filled at M4". | 1 (D-024), final at M5 |
| `docs/PROBLEM.md` | Keep Lance (CR-V) as the named user, since it remains true and is graded; add one sentence that the same device is also tested on a second car. Mechanisms paragraph: key-off now means a clean shutdown on stored energy with crash consistency behind it. Do **not** rewrite the memo's six-heading structure. Camden decides whether the named user changes (§11). | 1 (D-022, D-024) |
| `docs/DESIGN.md` | See §6.4. | 2 |
| `docs/EVALUATION.md` | Add rows to the plan: shutdown time distribution, stored-energy margin, key-cycle recovery counts, boot-to-green time, report generation time; reframe the fault-injection table around key cycles and the unplugged dongle; keep the limitations section's honesty clause. | 2 |
| `docs/DECISIONS.md` | The eight entries of §5; the review queue; §G moves; D.2 rationale fix; C.1 pointer to D-025; E.3 closed by D-023. | 1 |
| `docs/milestones.md` | See §7. | 1 and after |

### 6.3 Hardware documents and the schematic

| File | Change | Stage |
| --- | --- | --- |
| `docs/hardware/BOM.md` | Rewrite. Core: OBD2 Bluetooth Classic dongle (STN chip, not BLE), USB-C cable, microSD, the power-block parts once chosen, tri-color LED and resistors, plastic shell. Remove the lighter-socket and "USB-C power meter for the car port" framing. **Do not list part numbers without datasheet links.** Mark everything "design, not ordered". | 1 (D-024) |
| `docs/hardware/wiring.md` | Rewrite around the box: the USB-C sink and power block (§3.1), the three LED GPIOs, the source-present and halt-done signals, the dongle and radio notes, and a dated measurement table for §3.6. Keep "a step without a recorded result has not been done". | 1, then as measured |
| `docs/hardware/pid-survey.md` | Add a Forester column; record `ATDP`, VIN; keep the CR-V column for Lance. | 3 |
| `docs/hardware/schematic-conventions.md` | Replace the block table: **Power** (the §3.1 power block), **Pi interface** (header, tri-color LED, source-present and halt-done GPIOs), **CAN** (stretch only, unchanged), drop the conditional **Sensors** block (P5). Keep rules 1 to 6. Owner line "Lance" becomes "open". | 1 (D-024) |
| `electricalDrawing/` | Populate `CS370_Project_Library.kicad_sym` and draw the Power and Pi-interface blocks. Hardware PR rules (`CLAUDE.md` §7.12): regenerated SVG, pasted `make hwcheck`, never commit `.kicad_prl` or `.history`, **never hand-write `.kicad_pcb` geometry**. Claude cannot see the board: it may review netlists and cross-check datasheets but the human places symbols and measures. | 5 |

### 6.4 `docs/DESIGN.md`, section by section (stage 2)

1. **§1 Architecture:** redraw the diagram with the box, the power block, the light and
   the signals; a rate on every arrow; resolve 🐞1 (ring consumers); show the status-page
   server and the report path.
2. **§2 Mechanism mapping:** D from the power-cut-every-drive requirement plus the WAL
   design; E from "a recorder that dies silently has harmed its user"; justify each from
   the user's requirements and name how it is measured (`expcrash`, `expkill`). Remove the
   B and F rows (P5) or move them to a "considered and declined" note.
3. **§3 Failure-mode table:** §3.5 above, plus every existing row's Response and Log
   cells (write the modes you fear, not the ones already handled).
4. **§4 Storage and data:** the record format, rates, retention, fsync policy and its
   argument, the crash story (redo and undo, §3.3), baseline persistence, clock.
5. **§5 Constraints and substitutions:** every row: wanted, using, what it costs. Add the
   cold-start gap (§3.4), the crank-bridge limit (§3.1), the always-live-source limit,
   Mode 01 request/response in place of raw CAN, one healthy car instead of a fleet of
   failing ones, the 48-hour soak definition and its pending written confirmation.
6. **§6 Evaluation plan:** every measurement with method and a **committed target**
   (`evalplan`). Targets are commitments, so pick numbers from measurements (§3.6), not
   from this file's estimates.
7. **§7 Ownership map:** the realized claim-on-first-commit map. Until commits exist,
   state that and list the subsystems unclaimed.
8. **§8 AI-use plan:** unchanged in spirit; update the boundary sentence for the report
   server; keep "the `make boundary` check is in `make test`".
9. **§9 Changelog:** stays empty until M5. Do not pre-write it.
10. **Three-isolated-workers check** before submission: a design-only reader, a parts-only
    buyer who sees only the BOM, and a builder who follows only the instructions.

### 6.5 Source, tools and scripts (stages 3 to 5)

| Path | Change |
| --- | --- |
| `include/store/record.h` | New record kinds, `boot_id`, `REC_VERSION` 2 (§3.3). Keep the CRC known-answer test untouched. |
| `src/store/` | `storaged`: batch writer with `BEGIN`/`COMMIT`, A/B checkpoints, recovery (redo, undo), with tests that cut a file at every byte offset (the strongest crash test that needs no hardware). |
| `src/obd/` | Reply parser first (tests for `NO DATA`, `?`, `BUFFER FULL`, multi-line replies), then the requester and link-recovery loop (D-012, D-019). |
| `src/ipc/` | Shared-memory ring; resolve 🐞1; a Unix-socket protocol for the supervisor. |
| `src/supervisor/` | Fork and exec, `waitpid`, fd hygiene, restart with backoff and a storm guard, heartbeats, the §3.2 state machine, the three-GPIO light driver. |
| `src/interface/` | `obdctl` verbs over the supervisor's socket; the status page and report (C, per B.4's recommendation, to keep an interpreter out of the memory test; confirm with Camden, §11). The only place network symbols are allowed. |
| `src/analysis/` | As scheduled in M4. Do not start before M3's spine exists. |
| `tools/obdsim.py` | ELM327/STN emulator on a pty, **labeled `synth`**, a development tool only. |
| `tools/pidscan.py` | Replace the stub: send `0100`, `0120`, `0140`, `0160`, decode bitmaps, also `ATDP` and `0902`. |
| `tools/capture.py` | Crude Mode 01 logger that stands in for `obdd` so baseline collection can start (`baseline`). Every line carries `src=live`. Python is allowed in `tools/`, and this is not a graded mechanism. Replace by `obdd` when it exists. |
| `scripts/provision_pi.sh`, `install.sh` | Pairing and `/dev/obd`, the access point, systemd units with `Restart=always`, a trimmed boot. Verify whether this Pi OS still ships the `rfcomm` tool before writing the TODO that depends on it. |
| `soak/` | Rewrite `README.md` and `run_soak.sh` around key cycles: a manifest per drive, a session log, the injected-fault record, the unclean-shutdown count. Logs stay raw. |
| `Makefile` | Add the status-page binary (🐞2) and any new test binaries. Keep `-Wall -Wextra -Werror -Wshadow -Wconversion -Wvla`. |
| `cspell.json` | Add every new board slug and jargon word in the same commit that introduces it. |

## 7. Board changes (`docs/milestones.md`)

Rules (`CLAUDE.md` §7.11, `docs/BOARD/BOARD.md`): edit lines, then run `python3
tools/board_sync.py` (it needs Python 3.11 or newer; the Makefile picks one for you).
**Never** type or change a `<!-- #n -->` tag. **Claude never ticks a question, decision or
hardware line**; where this plan says "cut", the *human* ticks the box and adds "(cut)".
Remove the `@camden` and `@lance` markers from every line (P2); run `--check` first to see
whether sync removes the assignee, and tell Camden if it does not.

### 7.1 Lines to retitle or reword (keep the slug)

| Slug | New wording |
| --- | --- |
| `firstcap` | Both signals alive: real Mode 01 replies from a real car (the Forester), captured to a file |
| `baseline` | Baseline data collection started on a real car; the model learns normal from it |
| `bench` | M2 hardware: Bluetooth OBD2 dongle bring-up on the Pi, no car |
| `keyoff` | M2 hardware: prove the box's USB-C input: sink-only, crank dip, key-off, undervoltage |
| `order` | Hardware ordered: the Bluetooth dongle, a USB-C cable, a microSD card; power-block parts after the design is signed |
| `led` | M3 supervisor: drive the tri-color light from child health, verdicts and power events |
| `encl` | M4 hardware: the box: shell, vents, USB-C port, light, mounting |
| `soak1`, `soak2` | 48-hour key-cycle soak, run #1 and run #2 (the margin), under the professor's definition |
| `obdsim` | M3 tools: fake OBD2 port for development, labeled synth (never the soak's source) |
| `schpower` | M2 hw: draw the Power block (USB-C sink, protection, limiter, stored energy, converter) |
| `schpi` | M2 hw: draw the Pi interface block (tri-color light, source-present, halt-done) |
| `storewrite` | M3 store: write path with transactions, commit and fsync per batch |
| `storerecover` | M3 store: recovery: checkpoint load, redo, undo, torn-tail truncation |
| `recfmt` | M2 decision: finalize the on-disk format, including transaction and boot records |
| `evalplan` | M2 docs: evaluation plan with committed target numbers, including shutdown and recovery |

### 7.2 Lines to cut (a human ticks and adds "(cut)")

| Slug | Reason |
| --- | --- |
| `soakq` | Answered by D-023 once the professor confirms in writing |
| `pisensor` | Decided by P5 and recorded as D-029 |
| `canirq`, `canpoll`, `expirq`, `expdrop` | Exist only if `pisensor` brought B and F back; declined |
| `schcan` | CAN block is stretch-only and not drawn (D-019) |
| `ownmap` | Moot under D-026 |

### 7.3 Lines to add (no tags, no `@owner`)

M2:

- `pidforester` M2 hardware: supported-PID survey on the 2008 Forester, with protocol
  and VIN
- `profmail` M2 decision: get the professor's 48-hour soak definition in writing
- `powerspec` M2 docs: power worksheet: Pi draw by state, shutdown and crank energy,
  bank size
- `shutfsm` M2 docs: power-event state machine and the grace period
- `bootbudget` M2 docs: boot-time budget and what the first minute of a drive loses
- `walspec` M2 decision: write-ahead transactions and A/B checkpoints, record kinds
- `ledspec` M2 docs: tri-color light patterns, priority and the boot-time state
- `reportspec` M2 docs: report contents and format, box fault versus car fault
- `enclspec` M2 docs: box mechanical spec: size, vents, heat-rated plastic, antenna
  clearance
- `cranklog` M2 hardware: record the Forester's supply dip at crank with a meter
- `capbench` M2 hardware: stored-energy bench test into a dummy load, no Pi (after the
  design is signed)

M3:

- `pwrmon` M3 supervisor: power-event handling and clean shutdown
- `wifiap` M3 interface: the box's own Wi-Fi access point, WPA2, no upstream
- `report` M3 interface: report generator (HTML and CSV), labeled by data source
- `bootimg` M3 process: trimmed Pi image and boot-time measurement

M4:

- `expshut` M4 experiment: clean-shutdown time distribution and stored-energy margin
- `keycyc` M4 experiment: key-cycle recovery: N real cycles, unclean endings, records lost
- `thermal` M4 hardware: Pi and stored-energy temperature in a sealed shell in a hot cabin

Keep `expcrash` (power-cut trials: now the fallback test) and `expkill`. Keep `btcoex`,
`pidhonda` (Lance's CR-V), `meterv` and `twonode` as they are.

### 7.4 Exit-criteria lines (bold) to check

- M2: both signals alive; baseline started; design document complete.
- M3: `obdd` to ring to `storaged` to disk on a real car; mechanism D or E demonstrable.
- M4: feature freeze; key-cycle soak begins with the fault planned.

**Dates:** M2 (2026-10-11) is past and M3 is 2026-11-01. Changing a due date is a human
call (the table is edited by hand); do not move it. Camden decides whether to talk to
the professor about M2 (§11).

## 8. Execution plan: stages, branches, commits

Branch names follow `CLAUDE.md` §7.2: `<owner>/<milestone>/<slug>`. Each stage is one pull
request opened as a **draft at its first commit**, with `.github/PULL_REQUEST_TEMPLATE.md`
fully filled, `Closes #<n>` (add the line to the board and sync first if no issue
exists), the decision IDs touched, a non-empty "What I am least sure about", and pasted
`make`, `make test` and `make asan` output. **Claude never pushes, merges or marks a PR
ready without Camden saying so.** Commit messages follow `.gitmessage`
(`M<n> <area>: <imperative summary>`, then WHAT, WHY, REVIEW, VERIFY) and end with the
attribution trailer the harness supplies.

### Stage 0: this plan (done in the session that wrote it)

- `M2 docs: add the revision plan for the box redesign` (this file, plus its index row
  in `docs/README.md` and the new board slugs in `cspell.json`).
- `M2 process: add the Ghostty transcript copy-out for Camden's Mac` (done, `2b1b5ba`).

### Stage 1: decisions and the documents they change (branch `camden/m2/box-decisions`)

Docs only. One commit per decision, in this order, each carrying its `DECISIONS.md`
entry, the review-queue row, every dependent doc edit from §6, and its board lines from
§7. Because the rule is "one resolved question per commit", expect these commits to be
wide and shallow.

1. `M2 docs: record D-022, the Forester and the CR-V are both testbeds`
2. `M2 docs: record D-023, the 48-hour soak is 48 hours of normal use`
3. `M2 process: record D-026, tasks have no pre-assigned owners` (touches `CLAUDE.md` §0
   and §6; cite Camden's explicit yes; this is the one place Claude edits the team
   rules file, and only because Camden said to)
4. `M2 docs: record D-024, one USB-C input and a short-lived stored-energy element`
   (wide: `BOM.md`, `wiring.md`, `schematic-conventions.md`, README, PROBLEM, PLAN,
   DESIGN §1 and §5 skeleton, board lines; chemistry left as ❓ OPEN until Camden picks)
5. `M2 docs: record D-025, write-ahead transactions and checkpoints` (C.1 pointer, DESIGN
   §4 draft, `walspec` and `recfmt` lines)
6. `M2 docs: record D-027, a tri-color status light that always pulses`
7. `M2 docs: record D-028, the phone page downloads a report`
8. `M2 docs: record D-029, decline pisensor and keep D and E`
9. `M2 docs: fix the stale oil-pressure example in D.2` (🐞8, tiny and separate)
10. `M2 process: record the sync after the box redesign` (run
    `python3 tools/board_sync.py`, commit the written tags, as earlier "record the sync"
    commits do)

Housekeeping commits that need Camden's yes first (each its own commit, §4.2): remove
`m1.zip` (🐞3), add `.mailmap` (🐞6). Hardware housekeeping (🐞4) waits for stage 5.

**Done when:** `docs/DECISIONS.md`'s review queue lists D-022 to D-029; no live document
still says the CR-V is the only testbed, that the car's USB-C or the 12 V socket powers
the Pi, that the soak is 48 powered hours, or that `@camden` or `@lance` owns a task; the
board checks (`python3 tools/board_sync.py --check`, `make test`) pass.

### Stage 2: the design document (branch `camden/m2/design-doc`)

One commit per DESIGN section from §6.4, in order, then `M2 docs: add the new
measurements to the evaluation plan` for `EVALUATION.md`. Every `[N]` and `[fill]` gets a
value or an explicit "measured at stage 3, see board item `x`" pointer. Do not invent
numbers: where a number needs a measurement, leave a clearly marked placeholder and the
board item that closes it. End with the three-isolated-workers check, run by three
readers who have not seen the rest.

**Done when:** no `TODO(M2)` or `[fill]` remains in `docs/DESIGN.md`, every table has no
empty cells except those that cite a pending measurement, and `make test` passes.

### Stage 3: software that needs no hardware (branch `camden/m2/obd-tools`)

Tests lead: write the failing test, then the code, in one commit each (`CLAUDE.md` §8.3).
One commit per responsibility:

1. `M2 tools: add obdsim, an ELM327 emulator labeled synth` (+ its test)
2. `M2 tools: decode the supported-PID bitmaps in pidscan` (run against `obdsim`)
3. `M2 tools: add capture.py, the crude Mode 01 logger labeled live` (against `obdsim`)
4. `M2 obd: parse Mode 01 replies and reject malformed ones` (C, with tests)
5. `M2 store: add transaction and boot record kinds to record.h` (C, with tests, bump
   `REC_VERSION`, CRC known-answer test unchanged)

Each commit builds and passes `make`, `make test`, `make asan` on its own.

### Stage 4: the spine (branches after M2 items close; mostly M3)

In rough order: the shared-memory ring (`ringshm`), `storaged` batch writer then
recovery with the cut-at-every-offset test, the supervisor's fork and restart logic and
heartbeats, the requester and link recovery in `obdd`, the power-event state machine and
light driver, `obdctl status`, the report and Wi-Fi access point. Each is its own PR.
The detailed breakdown is the board; do not duplicate it here.

### Stage 5: hardware (branch `camden/m2/power-block`, only after the design is signed)

Schematic blocks (Power, then Pi interface), `make hwcheck` green, `make hwdocs` output
committed, BOM and wiring regenerated from the netlist. Bench tests (§3.6) run with a
dummy load first and the Pi second, and are recorded in `wiring.md` with dates. Claude
does not place symbols, choose parts without datasheets, or write board geometry.

### Gates between stages

| Gate | Condition |
| --- | --- |
| Stage 1 to stage 2 | Camden has read and agreed the decision entries (still UNREVIEWED). |
| Stage 2 to any purchase beyond the first | The Forester PID survey result is recorded (stage 3 or earlier) and the power worksheet's estimates are marked estimate or measured. |
| Stage 4 | The design document's §3 and §4 exist. |
| Stage 5 | The chemistry sub-decision (R1) is made and the power worksheet exists. |

## 9. The professor email (draft for Camden to send; Claude never sends)

> Subject: Clarifying the 48-hour soak for our automotive device
>
> Professor Pallickara, our device is powered by the car and is off whenever the car is
> off. We understand the 48-hour soak to mean 48 hours of regular use: the device boots at
> each key-on, records, and shuts down on the loss of power at each key-off, and the
> logs show every one of those cycles over a 48-hour window. Could you confirm in writing
> that (1) the logs may show off-periods between drives with hourly heartbeats while
> running, (2) a clean shutdown at power loss counts as each session's orderly end, and
> (3) one injected fault during the window, such as unplugging the OBD2 adapter for ten
> minutes with the engine running, meets the fault-injection requirement? Thank you.

## 10. Money: what is not authorized

Nothing in this plan authorizes a purchase. Camden's standing instruction (P14) is to be
very sure of the paper plan first. When he is ready, the proposed ladder is:

1. A microSD card (the Pi has none) and the Bluetooth dongle with a cable. Camden already
   owns a power cable, but check it is rated for a Pi 4 (5.1 V at 3 A) before trusting it.
   For the first PID survey a cheap Bluetooth Classic ELM327 clone is enough, and the
   survey is the real go or no-go: if the Forester answers too few of the sensors the
   model needs, no amount of spending on the better dongle fixes that.
2. Power-block parts, only after the design (§3.1) is signed, chosen from datasheets a
   human has checked, bought as modules where possible to avoid soldering risk.
3. The shell last, after the real board's dimensions are known.

Camden's budget view, as of this session: the dongle and cable are fixed purchases; the
rest is design. Do not exceed that without asking.

## 11. Open questions for Camden (and Lance)

Ask these in the first session; record each answer in `docs/DECISIONS.md` or the board,
not in a document that nobody reads.

1. **Supercapacitor or small lithium cell** (R1)? Recommendation: supercapacitor.
2. **Ring consumers** (🐞1): two rings, or `analyzed` reading `storaged`?
3. **Light during boot** (R4): hold yellow on in hardware until the supervisor takes
   over?
4. **Can the phone page set the clock** (R5)? It would be the one write the page makes.
5. **Report format and server language** (R3): HTML plus CSV from a C program?
6. **Named user:** keep Lance and the CR-V in PROBLEM.md, or change it to a Forester
   owner? Recommendation: keep, and add one sentence about the second car.
7. **Which deliverable is due 2026-10-28** (U5)? Is M2 being submitted late, and does
   Camden want to ask the professor about it?
8. **Canonical commit identity** for `.mailmap` (🐞6), and permission to remove `m1.zip`
   (🐞3).
9. **Lance:** he has reviewed no decisions. Tell him about D-022 to D-029, the removal of
   owner markers, and that his PID survey, schematic and record-format items are open for
   anyone. Do not take or reassign anything on his behalf.
10. **Is a Forester power or key test possible this week?** The free tests (§3.6) do not
    need parts.

## 12. The do-not list

- Do **not** commit to `main`, merge, approve or close a pull request, force-push,
  rebase or amend anything pushed, `git add -A`, `git add .` or `git commit -a`. Stage
  explicit paths. Do not commit a file whose full current contents were not read in the
  session (`CLAUDE.md` §7.1).
- Do **not** mark any decision reviewed or 🔒 LOCKED, tick any board box, or type a
  `<!-- #n -->` tag. Do not create or edit issues with `gh` or the web UI; edit
  `docs/milestones.md` and run `python3 tools/board_sync.py`.
- Do **not** reopen a 🔒 decision without saying so in the commit message and PR and
  citing the human yes. The only two in this plan are C.1 (D-025) and F.2 (D-026).
- Do **not** weaken, skip or delete a test.
- Do **not** write any restriction that says where or on what car either partner must
  test, or that assigns work to Lance. Open tasks are open (P1, P2).
- Do **not** claim a hardware fact from a datasheet or memory. Paste the artifact. A
  measured value beats an opinion, and §3 estimates are not measurements.
- Do **not** write `.kicad_pcb` geometry, commit `.kicad_prl`, `.history/` or autosave
  files.
- Do **not** add a network client, an LLM call, or third-party weights anywhere in the
  product. The only network code is the status page in `src/interface/`.
- Do **not** spend money, send the professor email, or contact Lance.
- Do **not** put a number in `docs/DESIGN.md` that came from this file's estimates
  without labeling it an estimate.
- Do **not** pre-write the DESIGN changelog (§9 of the design document) or any
  evaluation result. They are written after the fact.
- Do **not** mix a content change with a reflow of the same file; markdown wraps at 90
  columns and spelling is American English (D-018).

## 13. Quick reference

**Commands** (`CLAUDE.md` §1): `make`, `make test`, `make asan`, `make memcheck`
(valgrind: use the Pi or Linux), `make soakcheck`, `make hwcheck`, `make hwdocs`,
`make replay FIX=...`, `make deploy PI=...`, `python3 tools/board_sync.py [--status |
--check]`.

**Facts about this machine and repo (2026-10-21):** macOS, Ghostty terminal, repository at
`~/dev/school/cs370/car-watch`, Claude Code folder
`~/.claude/projects/-Users-crash-dev-school-cs370-car-watch/`, `python` is not on PATH
(use `python3`; `tools/` needs 3.11 or newer for `tomllib`), `kicad-cli` is installed,
`markdownlint-cli2`, `cspell` and `valgrind` are not.

**Key source documents:** the rules `CLAUDE.md`; the index `docs/README.md`; the
decisions `docs/DECISIONS.md`; the board `docs/milestones.md`; the rubric
`docs/handout/CS370-TermProject.md` (sections 3, 6, 8, 11, 12 and 13 matter most); the
reasoning `docs/PLAN.md` and `docs/PROCESS.md`.

**Terms:** *D* and *E* are the handout's mechanism menu items (custom storage with a
crash-consistency story; multi-process with a supervisor). *WAL* is write-ahead logging.
*Stored energy* is the small element that bridges the supply for seconds. *Dwell* and
*hysteresis* are the analysis state machine's persistence rules (D.3). *Source present*
is the signal that the USB-C input is energized.
