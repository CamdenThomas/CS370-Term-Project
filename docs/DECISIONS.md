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
| D-011 | Plugs into the OBD2 port for data; powered from the car's USB-C / 12 V socket (§A.6) | Camden + Claude | 2026-09-21 | Lance — owns the Power schematic block |
| D-012 | Rate-limited, round-robin Mode 01 requests are the data path (§A.7) | Camden + Claude | 2026-09-21 | Lance — sets the sample rate every analysis stage sees |
| D-013 | Phone views status over the device's own Wi-Fi; read-only, obdctl stays primary (§B.4) | Camden + Claude | 2026-09-21 | Lance — `src/interface/` is shared |
| D-014 | One warning light, driven by the supervisor; every live state blinks (§B.5) | Camden + Claude | 2026-09-21 | Lance — Pi interface schematic block |
| D-015 | The Pi reads OBD2 through a USB adapter; raw CAN is a stretch goal (§A.1, supersedes D-001) | Camden + Claude | 2026-09-21 | Lance — reopens a LOCKED decision; changes every data rate you design against |
| D-016 | The 2015 CR-V is the only testbed; the Outback is dropped (§A.2, supersedes D-005) | Camden + Claude | 2026-09-21 | Lance — it is your car, and now every live capture runs on it |
| D-017 | Commit to mechanisms D and E; B and F only if a Pi-side sensor is added (§B.1, supersedes D-002) | Camden + Claude | 2026-09-21 | Lance — D is yours, and it is now half of what we are graded on |
| D-008 | Milestone dates verified against the syllabus (§F.3) | Camden | 2026-09-21 | Camden — your answer; mark it LOCKED in your own commit |

> Decisions marked 🔒 were made with Camden in the conversation. **Lance has not reviewed
> any of them yet** — Lance, read at minimum §B.1, §C.1 and §D.1, since those bind your
> subsystems.

---

# A. Hardware and interfaces

## A.1 — The Pi reads OBD2 through a USB adapter; raw CAN is a stretch goal ⚠️ UNREVIEWED
`D-015` · Decided M1, 2026-09-21 · **By:** Camden + Claude · **Reviewed:** Camden ✅ / Lance ⬜
· **Supersedes:** D-001 (§G.1)

**Decision.** A USB OBD2 adapter with an ELM327-compatible chip plugs into the OBD2 port
and appears on the Pi as a serial device, pinned to `/dev/obd` by a udev rule. Our C daemon
sends Mode 01 requests over it and parses the replies. Prefer an STN-chip adapter (e.g.
OBDLink SX) over a clone ELM327. No MCP2515, no SPI, no CAN wiring.

**Raw CAN is a stretch goal** — an MCP2515 on SPI, taken up only after mechanisms D and E
are implemented *and measured*.

**Why.** Camden's call (PROMPTLOG E-02): time spent making raw CAN access work was time
not spent on the problem. The adapter speaks every OBD2 protocol for us, which is exactly
what a plug-in product (D-011) needs, and it removes a class of hardware risk — 5 V on
MISO, crystal frequency, bus termination — that could kill a Pi or disturb a daily
driver's bus.

**Cost — the reason D-001 existed, still true.** An adapter hands us parsed ASCII at
roughly 10–20 PID replies per second, with no interrupt line and no bus timing. That
takes most of the mechanism menu away from the OBD data path; **D-017** records what
survives. Clone ELM327s are also unreliable (truncated buffers, fake firmware), which is
why the STN chip is preferred.

**Consequences.** The capture daemon becomes a serial reader. Wiring, BOM, provisioning
and the design doc follow. The PID survey needs no extra hardware — the same adapter does
it.

## A.2 — One testbed: Lance's 2015 Honda CR-V EX-L ⚠️ UNREVIEWED
`D-016` · Decided M1, 2026-09-21 · **By:** Camden + Claude · **Reviewed:** Camden ✅ / Lance ⬜
· **Supersedes:** D-005 (§G.2)

**Decision.** The CR-V is the only testbed. The Subaru Outback is dropped.

**Why.** Camden's call, 2026-09-21. D-005 itself named the second vehicle as the first
thing to cut when the schedule slipped, and the CAN detour (PROMPTLOG E-02) spent that
slack. One car halves the survey, baseline and ground-truth work, and the baseline window
is the one thing that cannot be compressed by working harder in Week 14.

**Cost.**
- The two-car contrast — *"the same sticker says 5,000 miles to both cars, and the cars
  disagree"* — was the thesis's strongest demonstration, and it is gone. The thesis is now
  argued within one car: its measured condition against its own sticker and its own
  Maintenance Minder estimate (D-007).
- If the CR-V publishes only an oil-pressure switch, diagnostic #1 has no second car to
  fall back on (`oilq`).
- Every live capture, induced fault and soak on real data runs on Lance's car and Lance's
  schedule; Camden owns the capture code but not the car.

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
  #1 cannot run from any tap point and must be replaced or dropped (`oilq`).
- **The Honda has Maintenance Minder** — an oil-life percentage estimated from how the
  engine has been run. For this car the "generic 5,000-mile sticker" framing is false, and
  `docs/PROBLEM.md` must answer the skeptic's version instead: *the car already estimates
  this.* The answer is that the Minder is an open-loop estimate from a usage model; it
  never measures the engine's condition. Ours measures.

We still need, from the car itself: which Mode 01 PIDs the ECU actually supports, and
whether oil pressure is published as an analog value or only as an idiot-light bit.

**Why it is urgent.** A large fraction of consumer vehicles publish only a binary
low-oil-pressure switch. If the CR-V does not publish analog oil pressure, **diagnostic
#1 is not implementable from the port** and we must substitute a physical sender (real
automotive work on a daily driver) or replace the diagnostic. This is the project's named
risk in `docs/PROBLEM.md`.

**Action.** Run a supported-PID scan on the CR-V with the USB OBD2 adapter (D-015) —
a ten-minute experiment. Record in `docs/hardware/pid-survey.md`.

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

*Data half revised 2026-09-21 by D-015, before any review: the MCP2515 and Y-splitter
became a USB OBD2 adapter.*

**Decision.** The device is a box anyone could install without tools:
- **Data:** the USB OBD2 adapter (D-015) plugs into the OBD2 port; one USB cable runs to
  the Pi.
- **Power:** the car's own USB-C port, or a USB-C adapter in the 12 V socket, into the Pi.
  No buck converter.
- **Nothing is cut, spliced, pierced or clamped** on the car.

**Why.** The product thesis is that this is a thing an ordinary owner plugs in, so the
prototype is built the way the product would be installed. We are building the model to
prove the idea is worth something, not the finished product — but a prototype that needs
harness surgery proves a different, less interesting idea. Switched USB power also means
no battery drain while parked, and it makes mechanism D's defining constraint literally
true: power is cut, unannounced, at every key-off.

**Alternatives rejected** (discussed 2026-09-21): splicing the bus behind the OBD2 port
(the same conductors as the port pins, plus a cut harness on a daily driver); an in-line
harness at the forward camera (the 2015 CR-V EX-L has none, D-007); a contactless bus
clamp (receive-only, a black box); powering the Pi from OBD2 pin 16 through our own buck
converter (live with the key off, and more hardware to build and defend).

**Rule this imposes:** **the Pi must not brown out at crank.** The USB-C supply is rated
≥ 5.1 V / 3 A (Pi 4), and is proven by `vcgencmd get_throttled` after a cold start, not
by its label (`docs/hardware/wiring.md`).

**Cost.**
- No recording while the key is off — nothing happens then worth recording, but the device
  also cannot run overnight in a parked car.
- **The OBD2 adapter itself draws from pin 16, which is live with the key off.** Its
  key-off current must be measured before it is left plugged in overnight.
- D-006 option (c), the parked-car soak, now requires a socket that stays live in
  accessory plus a battery tender.
- The product is "one box, two cables", not "one dongle".

## A.7 — Standard Mode 01 requests are the primary data path ⚠️ UNREVIEWED
`D-012` · Decided M1, 2026-09-21 · **By:** Camden + Claude · **Reviewed:** Camden ✅ / Lance ⬜

*Revised 2026-09-21 by D-015, before any review: the CAN-level detail (arbitration IDs,
broadcast frames, a listen-only flag, the port-traffic check) went with the MCP2515.*

**Decision.** The capture daemon gets its signals by sending **standard OBD2 Mode 01
requests** through the USB adapter (D-015) on a fixed, rate-limited, round-robin schedule,
one request outstanding at a time. Every timeout and every unanswered PID is logged.

**Why.** D-011 makes this a plug-in device, and the thing that makes a plug-in device work
on a car it has never seen is the part of the protocol every car must speak: Mode 01 is
mandatory on US cars from model year 1996. *"Learns this car's normal"* is precisely what
lets one device serve different cars without per-model decode work: it never needs to
know what normal *is* ahead of time.

**Sample-rate consequence.** Signals are round-robined, so each PID is sampled at
(request rate ÷ PID count) — on the order of 1 Hz each for a ~10 requests/s budget over
~8 PIDs. That is ample for what we diagnose (oil trend over weeks, warm-up over minutes,
fuel trim against load) and must be stated in `docs/DESIGN.md` §4. The request budget itself
is a design-doc number to justify, not a constant to pick.

**Cost.** The device sends requests to the car's ECU. Kept safe by the rate limit, one
outstanding request, and only standard read-only Mode 01 requests — the same thing every
scan tool does.

**Scope note.** "Works on any car" is the product's direction, not our claim. The claim is
still E.1: three faults, one car, measured error rates.

---

# B. Systems architecture and mechanisms

## B.1 — Mechanism commitments: D and E; B and F only with a Pi-side sensor ⚠️ UNREVIEWED
`D-017` · Decided M1, 2026-09-21 · **By:** Camden + Claude · **Reviewed:** Camden ✅ / Lance ⬜
· **Supersedes:** D-002 (§G.3)

The handout requires two, implemented by us and measured.

| Mechanism | Status | Where |
|---|---|---|
| **D** — custom append-only storage, crash-consistent | **committed** | `src/store/` |
| **E** — multi-process with a supervisor | **committed** | `src/supervisor/`, `src/ipc/` |
| **B** — interrupt-driven input with a polling comparison | only if `pisensor` adds an MPU-6050 | capture path for that sensor |
| **F** — high-rate no-drop SPSC ring | only if `pisensor` adds an MPU-6050 | `src/ipc/ring.c` |
| **A**, **C**, raw CAN | stretch, after D and E are measured | — |

**D.** *The defining constraint: power is cut mid-write every time the key turns off*
(D-011). We never get a clean shutdown. Measured by pulling power mid-write, repeatedly,
and proving recovery (`expcrash`).

**E.** A recorder that dies silently has actively harmed its user, who believes it is on
duty. Any child may be `kill -9`'d; the system degrades, logs, recovers. Measured by killing
each child repeatedly and timing detection and recovery (`expkill`).

**Why B and F left the OBD data path (D-015).** B needs an interrupt that *our* design
services. The USB adapter's interrupts belong to the kernel's USB-serial driver; our process
only blocks on a tty, and at ~10–20 replies/s a poll loop keeps up trivially — the
comparison would measure the tty layer, not a design of ours. F needs a *high-rate* stream;
~10–20 samples/s never stresses a ring, so "zero drops under contention" would be true and
meaningless.

**What brings them back.** An MPU-6050 on the Pi's I2C, its `INT` pin on a GPIO: kHz
samples from a hardware FIFO give a real interrupt-vs-polling comparison (B) and a rate
that stresses the ring (F). That is board item `pisensor`, due at M2.

**The ring stays regardless.** It is how capture hands samples to storage across the
process boundary E requires, and B.3 still binds it.

**Risk.** D and E alone meet the floor exactly. If either measurement fails, nothing is in
reserve — which is why `pisensor` is on the critical path.

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
| fast blink (~4 Hz) | **degraded** — a child is down, the adapter silent while the engine runs, storage failing, or undervoltage | highest |
| double-blink, pause | **a verdict has surfaced** (after D.3 hysteresis and dwell) — look at the phone | |
| slow blink (~1 Hz) | on duty, recording, nothing to report | lowest |
| **steady on or steady off** | **the device is not running.** Never a valid state. | — |

**Why a light at all.** The 3 a.m. test: nobody is looking at a phone at the moment that
matters. It also makes "honest reporting through our own interface" (demo movement 1)
visible from across the room when a sensor is unplugged.

**Why every live state blinks.** A GPIO keeps its last level after the process driving it
dies. If "verdict" were solid-on, a crashed supervisor would freeze the light into a false
alarm, or into a false all-clear if it froze off. With blink-only states, a stuck light of
either kind can only mean *not running*. This is mechanism E's principle — a recorder that dies
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

The claim we defend is: *"detects the three faults we can induce on this vehicle, with
these error rates."* Not "predicts failure." Not "generalizes to any post-1996 car."
*(Narrowed from "these two vehicles" by D-016, 2026-09-21.)*

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
- **Oil** — log continuously across a real oil change on the CR-V. Cannot be faked
  and cannot be rushed, which is why baseline collection starts at M2.

Ten labeled recordings we made beat ten thousand unlabeled samples we found, because we
know what ours mean — and we will be asked at the defense how we know.

## E.3 — 48-hour soak: live sensors vs. a fake OBD2 port ❓ OPEN
`D-006` · **Owner:** Camden · **Tracked as:** board item `soakq` (label `decision`)
· **Must close before M4** · Blocks nothing before M3

**The conflict.** A car cannot run for 48 hours, so Camden's memo (PROBLEM.md, risk)
proposes a **fake OBD2 port** that answers Mode 01 requests continuously — board item
`obdsim`. Handout §5 endorses exactly this kind of harness as "a legitimate systems
artifact." But the same paragraph says: *"The 48-hour soak and the live demonstration run
on live sensors; everything else may run on honest replay."* A soak on the fake port is
synthesized data by definition, and is labeled `src=synth` everywhere regardless.

**Options.**
- **(a)** A written exception from Pallickara for a soak on the fake port. The email text
  is in the body of board item `soakq`.
- **(b)** A sensor on the Pi itself (`pisensor`: MPU-6050 and/or DS18B20, ~$8) runs live
  for all 48 hours while the OBD side comes from the fake port, labeled `synth`.
- **(c)** The parked CR-V, key in accessory, on the live port with a battery tender. Fully
  live, but engine-off values barely move, the socket must stay live in accessory (D-011),
  and it ties up Lance's car for two days.

**Posture.** Build as if (a) is refused. (b) is the default fallback, which is one more
reason `pisensor` is on the critical path. The `src=` field is in the record format
regardless.

---

# F. Process, team and tooling

## F.1 — Git and pull-request law 🔒
Decided M0 · **By:** Camden · **Reviewed:** Camden ✅ / Lance ⬜

The binding ruleset lives in **`CLAUDE.md` §7**, because that is the file the agent reads
every session. Summary of the load-bearing parts:

- Claude never commits to `main`, never merges, never force-pushes, never `git add -A`.
- One commit = one responsibility, sized by what it is responsible for — not by line
  count. *(The original ≤ 150-line / ≤ 3-file and ≤ 500-line PR caps were replaced by the
  responsibility test in `CLAUDE.md` §7.3 and §7.5 during M0; CLAUDE.md is the binding
  text.)*
- One PR = one reviewable claim.
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
| **Camden** | `src/obd/`, `src/ipc/` |
| **Lance** | `src/store/`, `src/analysis/`, `tools/train/` |
| **Shared** | `src/supervisor/`, `src/interface/`, `src/common/`, build, docs |

Ownership means **first authorship and answerability at the defense**, not exclusivity.
Each partner must still be able to answer one question from across the boundary — demo day
guarantees one.

## F.3 — Milestone dates ⚠️ UNREVIEWED
`D-008` · **Owner:** Camden · **Tracked as:** board item `dates` (label `question`)
· **Answered:** 2026-09-21, by Camden · **Reviewed:** Camden ⬜ / Lance ⬜

**Answer.** Camden verified the milestone dates against the syllabus (checked off in
`docs/milestones.md`, 2026-09-21): the `[[milestone]]` dates in `docs/board.toml` stand as
written, M0 2026-09-20 through M6 2026-12-18. Recorded by Claude from that check-off;
Camden marks it LOCKED in his own commit.

**Why it mattered.** The handout gives weeks, not dates; a one-week error would have moved
the M4 soak start, and M4 already lands on Thanksgiving week.

---

# G. Superseded

When a decision is replaced, it moves here with a pointer to its replacement. Never delete
one: the reasoning is evidence, and "what M2 got wrong" is a graded section of
`docs/DESIGN.md`.

## G.1 — (was A.1) CAN reaches the Pi via MCP2515 on SPI, not Bluetooth 🗑
`D-001` · Decided M0 · **By:** Camden + Claude · **Reviewed:** Camden ✅ / Lance ⬜
· **Superseded 2026-09-21 by D-015 (§A.1)** — raw CAN became a stretch goal.

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

**What replaced it, and what that cost.** D-015 accepts the mechanism loss this entry
warned about, in exchange for hardware that works on day one; D-017 records which
mechanisms survive.

## G.2 — (was A.2) Testbeds: Subaru Outback and Lance's Honda 🗑
`D-005` · Decided M0 · **By:** Camden · **Reviewed:** Camden ✅ / Lance ⬜
· **Superseded 2026-09-21 by D-016 (§A.2)** — the second vehicle was cut, as this entry
said it would be first.

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

## G.3 — (was B.1) Mechanism commitments: B, D, E, F 🗑
`D-002` · Decided M0 · **By:** Camden + Claude · **Reviewed:** Camden ✅ / Lance ⬜
· **Superseded 2026-09-21 by D-017 (§B.1)** — with raw CAN a stretch goal (D-015), B and F
lost their data source.

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

---

## Adding a decision

Append it to the right category with the next free `D-###` for citation, and fill every
field. If Claude made it without a human present, it is ⚠️ **UNREVIEWED** and it goes in the
Review Queue at the top — **in the same commit that implements it**, never a later one.

**Closing an open question.** When a human answers a `decision` or `question` issue, the
answer moves here as a 🔒 LOCKED entry citing the issue number, and the issue closes. One
commit does both — resolving one question is one responsibility, however many lines it
touches across this file (`CLAUDE.md` §7.3, documentation rules).
