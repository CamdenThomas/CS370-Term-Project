# Decision log — carwatch

**Purpose:** every design decision on this project, in one place, organized by category so
you can find what you need without knowing an ID. This is an **audit log**, not a
reference doc — its job is to let Camden and Lance verify, after any working session,
that no decision was made on their behalf that they disagree with.

**This file holds what is DECIDED. Open work and open questions live on the GitHub
board** as issues with owners, milestones and blocked-by dependencies (`CLAUDE.md` §7.11).
An ❓ OPEN entry below exists only as a pointer to its issue, so the reasoning sits beside
the decisions it will eventually join.

## How to use this file

**After every Claude Code session, read the Review Queue below.** It lists every
decision that no human has signed off on yet. That is the whole point of this file.

**After your partner's session, skim the Review Queue too.** Their agent's choices bind
your subsystems as surely as yours do.

## Status legend

| Mark | Meaning |
|---|---|
| 🔒 **LOCKED** | Decided and human-approved. Not reopened without saying so explicitly and getting a yes. |
| ⚠️ **UNREVIEWED** | Made by Claude during a session without a human in the loop. **Binding on nothing until a human signs it.** |
| ❓ **OPEN** | Not decided. Blocking something. Owner named. |
| 🗑 **SUPERSEDED** | Replaced. Kept with a pointer, never deleted — the reasoning is evidence. |

**Claude may never change a decision's status to LOCKED.** Only Camden or Lance does
that, by editing the `Reviewed` field, in a commit authored by that human.

---

# ⚠️ REVIEW QUEUE — read this first

Decisions currently awaiting a human signature:

| ID | Decision | Made by | Session date | Reviewer needed |
|---|---|---|---|---|
| D-007 | Honda testbed is a 2015 Honda CR-V EX-L (§A.3) | Camden + Claude | 2026-09-21 | Lance — it is your car; confirm and close issue `honda` |
| D-011 | Plugs into the OBD2 port for data; powered from the car's USB-C / 12 V socket (§A.6) | Camden + Claude | 2026-09-21 | Lance — owns the Power and CAN schematic blocks |
| D-012 | Rate-limited Mode 01 requests are the primary data path; broadcast is a bonus (§A.7) | Camden + Claude | 2026-09-21 | Lance — sets the sample rate every analysis stage sees |
| D-013 | Phone views status over the device's own Wi-Fi; read-only, obdctl stays primary (§B.4) | Camden + Claude | 2026-09-21 | Lance — `src/interface/` is shared |
| D-014 | One warning light, driven by the supervisor; every live state blinks (§B.5) | Camden + Claude | 2026-09-21 | Lance — Pi interface schematic block |

> All decisions below were made with Camden in the conversation and are marked LOCKED
> accordingly. **Lance has not reviewed any of them yet** — Lance, read at minimum
> §A.1, §B.1, §C.1 and §D.1, since those bind your subsystems.

---

# A. Hardware and interfaces

## A.1 — CAN reaches the Pi via MCP2515 on SPI, not Bluetooth 🔒
`D-001` · Decided M0 · **By:** Camden + Claude · **Reviewed:** Camden ✅ / Lance ⬜

**Decision.** The Pi reads raw CAN frames through an MCP2515 + TJA1050 module on SPI0,
with the controller's `INT` line on a GPIO for edge-triggered receive. Not a Bluetooth
ELM327 dongle.

**Why.** A Bluetooth ELM327 hands us parsed ASCII over a userspace socket at a few
samples per second. With no interrupt line, no SPI transaction and no bus-level timing,
mechanisms **A, B, C and F all become unavailable** — and the handout requires at least
two mechanisms implemented by us, below the application layer. The MCP2515 path keeps
the kernel boundary inside our repository, where it is graded. It is also better for the
user: raw bus access sees frames the ELM327 protocol never exposes, at the rate the ECU
publishes them rather than the rate a request/response dongle can poll.

**Cost.** More wiring, a 3.3 V/5 V level question to get right, one-time decode work per
vehicle. Accepted.

**Consequences.** `src/can/` owns SPI transactions and the IRQ path (Camden). The
interrupt-vs-polling comparison is a first-class deliverable, not an afterthought. A
Bluetooth ELM327 may appear in `tools/` as a cross-check; it may never be the product's
path.

## A.2 — Testbeds: Subaru Outback and Lance's Honda 🔒
`D-005` · Decided M0 · **By:** Camden · **Reviewed:** Camden ✅ / Lance ⬜

**Decision.** Two daily-driven vehicles — Camden's Subaru Outback and Lance's Honda.

**Why.** The product's thesis is that a generic maintenance interval is a generalization
and your car is not generic. One vehicle cannot demonstrate that; two with different duty
cycles can, and the comparison is the demo's strongest five seconds. Two also gives each
partner a vehicle they can instrument on their own schedule.

**Cost.** Roughly doubles decode and baseline-collection work, and the baseline window is
the one thing that cannot be compressed by working harder in Week 14. Mitigation: start
on standard Mode 01 PIDs only; manufacturer-specific frames are a stretch.

**Risk.** If the schedule slips, the **first** thing cut is the second vehicle — not a
mechanism, and not the evaluation. Record it here if it happens.

## A.3 — Honda year/model/engine ⚠️ UNREVIEWED
`D-007` · **Owner:** Lance · **Tracked as:** GitHub issue `honda` (label `question`)
· **Answered:** 2026-09-21, by Camden in session · **Reviewed:** Camden ✅ / Lance ⬜
· **Blocks:** the PID survey, and therefore diagnostic #1

**Answer.** **2015 Honda CR-V EX-L.** By model year that is the 2015 refresh: 2.4 L
direct-injected four-cylinder (K24W) with a CVT. FWD or AWD is not yet recorded. The
engine and transmission are inferred from the model year, not read off the car — Lance
confirms against the VIN or the door-jamb sticker, then closes the issue.

**What the answer changes.**
- **No Honda Sensing on the EX-L** (it was Touring-only in 2015), so there is no
  lane-keep camera connector to tap. The OBD2 port is the tap point (**D-011**).
- **Oil pressure is expected to be a switch, not a sender.** This is inference from
  Honda practice, not a service-manual reading; `pidhonda` confirms it. If true, diagnostic
  #1 cannot run on the Honda from any tap point and rests on the Outback alone (`oilq`).
- **The Honda has Maintenance Minder** — an oil-life percentage estimated from how the
  engine has been run. For this car the "generic 5,000-mile sticker" framing is false, and
  `docs/PROBLEM.md` must answer the skeptic's version instead: *the car already estimates
  this.* The answer is that the Minder is an open-loop estimate from a usage model; it
  never measures the engine's condition. Ours measures.

We still need, from the car itself: which Mode 01 PIDs the ECU actually supports, whether
oil pressure is published as an analog value or only as an idiot-light bit, and the bus
bitrate.

**Why it is urgent.** A large fraction of consumer vehicles publish only a binary
low-oil-pressure switch. If neither testbed publishes analog oil pressure, **diagnostic
#1 is not implementable from the bus** and we must substitute a physical sender (real
automotive work on a daily driver) or replace the diagnostic. This is the project's named
risk in `docs/PROBLEM.md`.

**Action.** Run a supported-PID scan on both vehicles with a $12 ELM327 — a $0, ten-minute
experiment that does not need the MCP2515 to have arrived. Record in
`docs/hardware/pid-survey.md`.

## A.4 — Capture the schematic in KiCad; do not fabricate a PCB 🔒
`D-009` · Decided M0 · **By:** Camden + Claude · **Reviewed:** Camden ✅ / Lance ⬜

**Decision.** We draw the full schematic in KiCad (`electricalDrawing/`, KiCad 10, owned by
Lance) and treat it as the source of truth for wiring and the BOM. We do **not** fabricate a
custom PCB unless we take The Reach (handout §14). The empty `.kicad_pcb` stays in the repo
on purpose.

**Why.** A custom board earns zero points — the rubric grades mechanisms, measurements and
the soak — and fab turnaround lands on top of critical path B, already the likeliest cause
of an M2 slip. A breadboard plus an off-the-shelf MCP2515 module clears every guardrail, and
the handout is explicit that a food container is an acceptable enclosure.

**Why the schematic is still worth the hours.** Three graded artifacts depend on it:
`docs/DESIGN.md` needs a defensible wiring story; `docs/hardware/wiring.md` needs pin assignments
that cannot drift from the build; and the three-isolated-workers test needs a parts-only
buyer who can order from the BOM without seeing the design. A hand-maintained pin table will
disagree with reality by Week 9. A generated one cannot.

**Consequences.** `make hwcheck` (ERC) and `make hwdocs` (SVG, BOM, netlist) enter the
build. `docs/hardware/wiring.md` and `BOM.md` become **derived** artifacts. `CLAUDE.md`
§7.12 requires a rendered schematic in any PR touching hardware.

## A.5 — ERC is a build failure, and the schematic SVG is committed 🔒
`D-010` · Decided M0 · **By:** Camden + Claude · **Reviewed:** Camden ✅ / Lance ⬜

**Decision.** `kicad-cli sch erc --exit-code-violations` runs as `make hwcheck`; violations
fail the build. `make hwdocs` renders the schematic to `docs/figures/electricalDrawing.svg`,
and that SVG is **committed**.

**Why the ERC gate.** It makes electrical correctness the same kind of object as
`-Wall -Wextra -Werror`: a machine-checked precondition rather than a thing someone
remembers to look at. Unconnected pins and power-output conflicts are exactly the class of
error that survives a human read and kills a board.

**Why commit a generated file.** Normally we would not. But a reviewer cannot read a diff of
S-expression coordinates, and §7.6 says a PR nobody understands does not merge — so without
a rendered picture, hardware would fall outside the review process entirely. The SVG is the
diff a human can actually read. It is regenerated, never hand-edited.

**Open sub-item (Lance, M2):** `sym-lib-table` referenced `CS370_Project_Library.kicad_sym`,
which did not exist — KiCad errors on project open. An empty library is committed to resolve
it; populate it as parts are drawn.

## A.6 — Plug-in form: OBD2 port for data, car USB-C for power ⚠️ UNREVIEWED
`D-011` · Decided M1, 2026-09-21 · **By:** Camden + Claude · **Reviewed:** Camden ✅ / Lance ⬜

**Decision.** The device is a box anyone could install without tools:
- **Data:** a Y-splitter (pass-through) cable at the OBD2 port — CAN_H pin 6, CAN_L pin 14,
  signal ground pin 5 — into the MCP2515 module. The port stays usable for a scan tool
  or an emissions inspection.
- **Power:** the car's own USB-C port, or a USB-C adapter in the 12 V socket, into the Pi.
  **OBD2 pin 16 is not used.** The 12 V→5 V buck converter leaves the BOM.
- **Nothing is cut, spliced, pierced or clamped** on either car.

**Why.** The product thesis is that this is a thing an ordinary owner plugs in, so the
prototype is built the way the product would be installed. We are building the model to
prove the idea is worth something, not the finished product — but a prototype that needs
harness surgery proves a different, less interesting idea. Switched USB power also means
no battery drain while parked, and it makes mechanism D's defining constraint literally
true: power is cut, unannounced, at every key-off.

**Alternatives rejected** (discussed 2026-09-21):
- *Splice CAN_H/CAN_L behind the OBD2 port.* The wires behind the port are the same
  conductors as pins 6 and 14 — same traffic, plus a cut harness on a daily driver. If a
  gateway were filtering the port, a splice there would not get past it either.
- *In-line harness at the forward camera.* Not available: the 2015 CR-V EX-L has no Honda
  Sensing camera (D-007).
- *Contactless (inductive) CAN clamp.* Receive-only, a black box between the bus and our
  code, and it needs a wiring diagram to find the pair.
- *OBD2 pin 16 + our own buck converter.* Live with the key off (battery drain), and more
  hardware to build and defend.

**Rules this imposes** (also in `docs/hardware/wiring.md`):
1. **Remove the module's 120 Ω termination jumper.** The car's bus is already terminated;
   ours would drop it to ~40 Ω.
2. **Keep the stub short** — the Y-cable plus module leads under ~0.3 m at 500 kbit/s.
3. **The Pi must not brown out at crank.** Adapter rated ≥ 5.1 V / 3 A (Pi 4). Proven by
   `vcgencmd get_throttled` after a cold start, not by the adapter's label.

**Cost.**
- No recording while the key is off — nothing happens then worth recording, but the device
  also cannot run overnight in a parked car.
- D-006 option (c), the parked-car soak, now requires a socket that stays live in
  accessory plus a battery tender.
- USB ground and OBD2 signal ground both reach chassis by different paths. Expected to be
  harmless; **measure before trusting it** (CLAUDE.md §8.4).
- The CAN side still needs a physical cable to the port; the product is "one box, two
  cables", not "one dongle".

## A.7 — Standard Mode 01 requests are the primary data path ⚠️ UNREVIEWED
`D-012` · Decided M1, 2026-09-21 · **By:** Camden + Claude · **Reviewed:** Camden ✅ / Lance ⬜

**Decision.** `candaemon` gets its signals by sending **standard OBD2 Mode 01 requests** at
a fixed, rate-limited schedule (functional request ID `0x7DF`, responses `0x7E8`–`0x7EF`,
one request outstanding at a time). Manufacturer broadcast frames are recorded when the
port carries them and used where decoded, but **nothing depends on them.** A
`--listen-only` build flag puts the MCP2515 in listen-only mode and sends nothing.

**Why.** D-011 makes this a plug-in device, and the thing that makes a plug-in device work
on a car it has never seen is the part of the protocol every car must speak. Mode 01 over
CAN is mandatory on US cars from model year 2008; manufacturer frames differ per make, per
model, often per year. This also matches D-005's existing mitigation — "start on standard
Mode 01 PIDs only". *"Learns this car's normal"* is precisely what lets one device serve
different cars without per-model decode work: it never needs to know what normal *is*
ahead of time.

**Sample-rate consequence.** Signals are round-robined, so each PID is sampled at
(request rate ÷ PID count) — on the order of 1 Hz each for a ~10 requests/s budget over
~8 PIDs. That is ample for what we diagnose (oil trend over weeks, warm-up over minutes,
fuel trim against load) and must be stated in `docs/DESIGN.md` §4. The request budget itself
is a design-doc number to justify, not a constant to pick.

**Consequence for mechanism B — to check, not assumed.** D-002's case for interrupts is a
busy 500 kbit/s bus overflowing the MCP2515's two buffers in milliseconds. That holds only
if the port carries **broadcast** traffic. If a car's port is silent except for our own
responses, B's *commitment* stands but its *justification* on that car does not. The PID
survey now includes a 60 s `ATMA` capture at the port for exactly this
(`docs/hardware/pid-survey.md`). If both ports are quiet, B.1's justification is reopened
with a human — not quietly rewritten.

**Cost.** The device now transmits on a daily driver's bus. Kept safe by the rate limit,
one outstanding request, and only standard requests; the listen-only flag is the fallback
if either owner objects.

**Scope note.** "Works on any car" is the product's direction, not our claim. The claim is
still E.1: three faults, two cars, measured error rates.

---

# B. Systems architecture and mechanisms

## B.1 — Mechanism commitments: B, D, E, F 🔒
`D-002` · Decided M0 · **By:** Camden + Claude · **Reviewed:** Camden ✅ / Lance ⬜

The handout requires two. We commit four and will measure all of them.

**B — interrupt-driven input with a polling comparison.** `src/can/`. At 500 kbit/s a busy
bus delivers a frame roughly every 230 µs; the MCP2515's two receive buffers overflow in
milliseconds. A poll loop either burns a core or drops frames. Measured as event-latency
distribution and CPU, both ways, idle and loaded.

**D — custom append-only storage, crash-consistent.** `src/store/`. *The defining
constraint: power is cut mid-write every time the key turns off.* We never get a clean
shutdown. Measured by pulling power mid-write, repeatedly, and proving recovery.

**E — multi-process with a supervisor.** `src/supervisor/`, `src/ipc/`. A recorder that
dies silently has actively harmed its user, who believes it is on duty. Any child may be
`kill -9`'d; the system degrades, logs, recovers.

**F — no-drop SPSC ring, sequence-accounted.** `src/ipc/ring.c`. Key-on produces a burst;
a gap in the record is a gap in the diagnosis. Measured as sustained rate with zero drops
under contention.

**Stretch, M4+ only:** **A** (character driver for the MCP2515) and **C** (`SCHED_FIFO` on
the capture path). Do not start either until B, D, E and F are implemented *and measured*.
A half-finished kernel module is worth zero points and costs two weeks.

## B.2 — Four processes, not one 🔒
`D-002` (corollary) · **By:** Camden + Claude · **Reviewed:** Camden ✅ / Lance ⬜

`candaemon`, `storaged`, `analyzed` and `supervisor` are separate processes with separate
address spaces, communicating over a shared-memory ring and Unix domain sockets.

**Why.** Fault isolation is the product requirement, not an architectural preference: the
analysis engine is the most complex and least trustworthy component, and it must not be
able to take the capture path down with it. One process would be simpler and would fail
the thing the device exists to do.

## B.3 — The ring refuses rather than overwrites 🔒
Decided M0 · **By:** Claude, from D-002 · **Reviewed:** Camden ✅ / Lance ⬜

When the ring is full the producer returns failure and increments an overrun counter. It
does **not** overwrite the oldest unread slot.

**Why.** Overwriting loses data silently, and silent loss is the exact failure mechanism F
exists to make impossible. A refusal is visible, countable, and reportable; an overwrite
looks like a quiet minute. Enforced by `tests/test_ring.c`.

**Reviewer note.** This is a real trade: under sustained overload we drop *new* data
rather than *old*. If that is ever the wrong choice for a diagnostic, reopen this.

## B.4 — The phone is a read-only window over the device's own Wi-Fi ⚠️ UNREVIEWED
`D-013` · Decided M1, 2026-09-21 · **By:** Camden + Claude · **Reviewed:** Camden ✅ / Lance ⬜

**Decision.** The Pi runs its own Wi-Fi access point — WPA2, a per-device passphrase, **no
upstream connection, no internet**. The owner's phone joins it and opens a status page
served from `src/interface/` by a supervised child process. The page shows exactly two
things — **status** and **current verdicts** — rendered from the same supervisor UDS verbs
`obdctl` uses. It holds no state, runs no analysis, and writes nothing. `obdctl` remains
the primary, exact interface with all five verbs. No app, no account, no cloud, no
Bluetooth pairing.

**Why.** The user is a car owner, not someone with a terminal in the passenger seat, so a
product that can only be read over SSH has no user. The handout permits it directly
("Serving a dashboard on the LAN is fine", §3.3; CLAUDE.md §2.1), and the device keeps
working with no phone present at all. It also sharpens the phone-app test: the Pi is the
part that is awake at every key-on; the phone is only a window onto what it already
recorded.

**Why so thin.** The handout warns: *"Resist the pretty web app. A crisp CLI with five
query verbs and exact semantics is more defensible, and far more extensible at the
live-modification station."* So the page is a view, not a second interface: two screens,
no framework, and no number on it that `obdctl` cannot also produce. Every graded behavior
remains demonstrable through `obdctl` alone.

**Cost.**
- A network listener in the product. It binds only to the AP interface, is read-only, and
  lives only in `src/interface/`, where `make boundary` already allows network symbols.
- One more process in the 48-hour RSS plot, and Wi-Fi radio power on the car's USB port.
- **Open sub-decision (M2, both):** the server's language. C in `src/interface/` keeps an
  interpreter out of the memory-stability test; Python in `ui/` is permitted by CLAUDE.md
  §2.5 but adds ~30 MB RSS to the soak. Recommendation: C, because the page is tiny.

## B.5 — One warning light, driven by the supervisor ⚠️ UNREVIEWED
`D-014` · Decided M1, 2026-09-21 · **By:** Camden + Claude · **Reviewed:** Camden ✅ / Lance ⬜

**Decision.** One LED on a GPIO (proposed GPIO17, header pin 11, through 330 Ω), written
**only by the supervisor**. Every state in which the device is working is a *blink
pattern*:

| Light | Meaning | Priority |
|---|---|---|
| fast blink (~4 Hz) | **degraded** — a child is down, CAN silent while the engine runs, storage failing, or undervoltage | highest |
| double-blink, pause | **a verdict has surfaced** (after D.3 hysteresis and dwell) — look at the phone | |
| slow blink (~1 Hz) | on duty, recording, nothing to report | lowest |
| **steady on or steady off** | **the device is not running.** Never a valid state. | — |

**Why a light at all.** The 3 a.m. test: nobody is looking at a phone at the moment that
matters. It also makes "honest reporting through our own interface" (demo movement 1)
visible from across the room when a sensor is unplugged.

**Why every live state blinks.** A GPIO keeps its last level after the process driving it
dies. If "verdict" were solid-on, a crashed supervisor would freeze the light into a false
alarm, or into a false all-clear if it froze off. With blink-only states, a stuck light of
either kind can only mean *not running*. This is D-002's principle — a recorder that dies
silently has harmed its user — applied to the one output a driver actually sees.

**Why the supervisor, not `analyzed`.** The supervisor already knows every child's
liveness (mechanism E), so it can show "degraded" when `analyzed` itself has died; a light
owned by `analyzed` would show a stale verdict instead. Verdicts reach the supervisor over
the existing UDS. One writer, no GPIO contention.

**Cost.** One more thing the supervisor does, and one more thing it must not block on: the
blink timer must never delay `waitpid` handling. Colour, brightness and placement are open
(M4 enclosure).

---

# C. Storage and data integrity

## C.1 — Append-only, self-framing, CRC'd records 🔒
`D-002/D` · Decided M0 · **By:** Camden + Claude · **Reviewed:** Camden ✅ / Lance ⬜

Records are never rewritten in place. Each carries a magic number for resynchronization
and a CRC32 over its whole body. Recovery scans from the last known-good offset, verifies
CRCs, truncates the torn tail, and logs exactly what was lost.

**Why.** We are interrupted mid-write by design, several times a day. Rewriting in place
can destroy data that was previously correct; appending can only ever damage the last
record. The CRC is what makes a torn record *detectable* rather than merely wrong — and
wrong numbers you trust are more dangerous than missing numbers you know about.

**Open sub-decision (M2, Lance):** fsync batch size and interval. There is no correct
value, only one justifiable from user requirements. Document the argument in `docs/DESIGN.md`
§4 before writing the code.

## C.2 — Every record carries a provenance tag 🔒
Decided M0 · **By:** Camden + Claude · **Reviewed:** Camden ✅ / Lance ⬜

Every record and every log line carries `src = live | replay | synth`. Present in the
format from day one, before any replay tooling exists.

**Why.** Handout §5: replayed or synthesized data must be labeled everywhere it appears —
logs, report, demo. Making it a mandatory field rather than a convention means it is
structurally impossible to forget. Mislabeling synthesized data as live is the single
integrity failure this course is best equipped to detect, and the worst trade available
anywhere in it.

---

# D. Intelligence and analysis

## D.1 — Self-trained model; training in Python, inference in C 🔒
`D-004` · Decided M0 · **By:** Camden (confirmed with instructor) · **Reviewed:** Camden ✅ / Lance ⬜

A locally-trained, self-authored model is permitted; hosted APIs and third-party
pretrained weights are not. Therefore: training in `tools/train/`, weights versioned under
`models/` with the training commit in the header, **inference in C in `src/analysis/`**, no
network at runtime.

**Why inference in C.** First and most important for the grade: the runtime path sits
inside the supervised multi-process architecture, and a Python process there muddies the
boundary a grader must verify in sixty seconds. Second: it removes an interpreter, a
garbage collector and ~30 MB of RSS from a 48-hour memory-stability test we are graded on.

**What the model is not doing.** The features are the intelligence — operating-point
binning, per-bin baselines, Mahalanobis residuals, Theil–Sen trends, all hand-written.
Delete the weights file and the system still produces residuals and trends; it loses only
the final labeling step. That separation is deliberate and it is the answer at the defense.

## D.2 — Analysis pipeline shape 🔒
Decided M0 · **By:** Camden + Claude · **Reviewed:** Camden ✅ / Lance ⬜

Operating-point binning (RPM × load × coolant temp) → per-bin running mean/variance →
multivariate residual (Mahalanobis) → robust trend (Theil–Sen) → classifier → state
machine with hysteresis and dwell.

**Why binning comes first.** Comparing a cold idle to a warm cruise is a false-alarm
generator. Every comparison happens inside one bin, against genuinely comparable
conditions. This single step removes more false positives than anything else downstream.

**Why multivariate.** The signal is the *correlation between sensors*, not any single
sensor's value — oil pressure is supposed to rise with RPM. A measure of anomaly that
already accounts for normal partnerships, and alarms only when a partnership breaks, is
what makes this something a threshold provably cannot do (handout §3.3).

## D.3 — Hysteresis and dwell before any verdict surfaces 🔒
Decided M0 · **By:** Camden + Claude · **Reviewed:** Camden ✅ / Lance ⬜

No verdict reaches the user until it has persisted. Trip and un-trip levels differ
deliberately.

**Why.** A monitor that cries wolf twice gets unplugged, and an unplugged monitor is worse
than none because someone far away believes it is on duty. **False-positive rate is a
first-class metric in `docs/EVALUATION.md`, not an afterthought.**

## D.4 — The threshold test, as a standing rule 🔒
Decided M0 · **By:** Camden + Claude · **Reviewed:** Camden ✅ / Lance ⬜

Every diagnostic we ship must require at least two cooperating sensors to reach its
conclusion. If a single threshold on a single value could produce the verdict, it is not
intelligence and it does not count. **Write the reason down in `docs/DESIGN.md` when adding a
diagnostic** — this is checked at the defense.

---

# E. Scope, claims and evaluation

## E.1 — Three induced-and-measured diagnostics, and no more 🔒
`D-003` · Decided M0 · **By:** Camden + Claude · **Reviewed:** Camden ✅ / Lance ⬜

The claim we defend is: *"detects the three faults we can induce on these two vehicles,
with these error rates."* Not "predicts failure." Not "generalizes to any post-1996 car."

1. **Oil pressure degradation across oil life** — residual after normalizing for RPM and
   temperature, trended across an interval.
2. **Cooling system anomaly** — thermostat short-cycling signature (period and amplitude)
   and abnormal warm-up slope, gated on ambient temp and load.
3. **Mixture drift** — long-term fuel trim divergence as a function of load.

**Why.** Handout §5 says it directly: a narrow claim met and measured outscores a broad
claim gestured at. Twelve vague diagnoses is the named failure mode for this project's
seed.

## E.2 — Induced ground truth 🔒
`D-003` (corollary) · **By:** Camden + Claude · **Reviewed:** Camden ✅ / Lance ⬜

We cannot wait a semester for a failure, so we provoke conditions and label the recordings:

- **Vacuum leak** — briefly disconnect a small vacuum line. Reversible, safe, moves LTFT
  within a minute.
- **Cooling** — partially block radiator airflow with cardboard.
- **Sensor fault** — unplug a sensor. Also demo day's injected fault, so it gets rehearsed.
- **Oil** — log continuously across a real oil change on both vehicles. Cannot be faked
  and cannot be rushed, which is why baseline collection starts at M2.

Ten labeled recordings we made beat ten thousand unlabeled samples we found, because we
know what ours mean — and we will be asked at the defense how we know.

## E.3 — 48-hour soak: live sensors vs. synthetic CAN ❓ OPEN
`D-006` · **Owner:** Camden · **Tracked as:** GitHub issue (label `decision`, milestone M4)
· **Must close before M4** · Blocks nothing before M3

**The conflict.** Our PC-side rig generating CAN traffic is a good idea and we are building
it regardless — handout §5 explicitly endorses a replay harness as "a legitimate systems
artifact." But the same paragraph says: *"The 48-hour soak and the live demonstration run
on live sensors; everything else may run on honest replay."* A soak on generated traffic is
synthesized data by definition.

**Options.** (a) Written exception from Pallickara — email drafted at
`docs/professor-email-draft.md`. (b) Add MPU-6050 + DS18B20 (~$8) so the soak runs on
genuinely live sensors while CAN is replayed and labeled; bonus is vibration order-tracked
against CAN RPM, which no dongle can produce. (c) Soak in the parked car on live CAN with a
battery tender.

**Posture.** Build as if (a) is refused. (b) is the default fallback and the tree is already
shaped for it — `src/capture/` exists and is empty on purpose. The `src=` field is in the
record format regardless.

---

# F. Process, team and tooling

## F.1 — Git and pull-request law 🔒
Decided M0 · **By:** Camden · **Reviewed:** Camden ✅ / Lance ⬜

The binding ruleset lives in **`CLAUDE.md` §7**, because that is the file the agent reads
every session. Summary of the load-bearing parts:

- Claude never commits to `main`, never merges, never force-pushes, never `git add -A`.
- One commit = one idea. **≤ 150 changed lines and ≤ 3 files**, default one file.
- One PR = one reviewable claim. **≤ 500 changed lines, ≤ 10 commits.**
- **If the reviewer does not understand a line, the PR does not merge.** The remedy is
  explanation or a smaller commit, never trust.
- Every commit builds and passes tests on its own.

**Why so strict.** Our process grade is computed from transcripts corroborated against
commits, the git history is a graded deliverable, and neither of us will break down a
thousand-line diff. A commit small enough to review in ninety seconds is a commit we will
actually review — and the defense asks us to answer for individual lines.

## F.2 — Ownership map 🔒
Decided M0 · **By:** Camden · **Reviewed:** Camden ✅ / Lance ⬜

| Owner | Subsystems |
|---|---|
| **Camden** | `src/can/`, `src/capture/`, `src/ipc/`, `drivers/` |
| **Lance** | `src/store/`, `src/analysis/`, `tools/train/` |
| **Shared** | `src/supervisor/`, `src/interface/`, `src/common/`, build, docs |

Ownership means **first authorship and answerability at the defense**, not exclusivity.
Each partner must still be able to answer one question from across the boundary — demo day
guarantees one.

## F.3 — Milestone dates ❓ OPEN
`D-008` · **Owner:** Camden · **Tracked as:** GitHub issue (label `question`, milestone M0)
· **Close this week**

`docs/milestones.md` assumes a semester start of Mon 2026-08-24, placing 2026-09-11 at the
end of Week 3 — meaning **M0 is due next week**. The handout gives weeks, not dates. If the
assumption is off by one week the M4 soak start moves, and M4 already lands on Thanksgiving
week. Verify against the syllabus and mark LOCKED.

---

# G. Superseded

*(none yet — when a decision is replaced, move it here with a pointer to its replacement.
Never delete one. The reasoning is evidence, and "what M2 got wrong" is a graded section of
`docs/DESIGN.md`.)*

---

## Adding a decision

Append it to the right category with the next free `D-###` for citation, and fill every
field. If Claude made it without a human present, it is ⚠️ **UNREVIEWED** and it goes in the
Review Queue at the top — **in the same commit that implements it**, never a later one.

**Closing an open question.** When a human answers a `decision` or `question` issue, the
answer moves here as a 🔒 LOCKED entry citing the issue number, and the issue closes. One
commit does both — resolving one question is one responsibility, however many lines it
touches across this file (`CLAUDE.md` §7.3, documentation rules).
