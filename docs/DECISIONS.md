# Decision log — carwatch

**Purpose:** every design decision on this project, in one place, organized by category so
you can find what you need without knowing an ID. This is an **audit log**, not a
reference doc — its job is to let Camden and Lance verify, after any working session,
that no decision was made on their behalf that they disagree with.

**This file holds what is DECIDED. Open work and open questions live on the GitHub
board** as issues with milestones, and no pre-assigned owners (D-026, `CLAUDE.md` §7.11).
An ❓ OPEN entry below exists only as a pointer to its issue, so the reasoning sits beside
the decisions it will eventually join.

## How to use this file

**After every Claude Code session, read the Review Queue below.** It lists every
decision that no human has signed off on yet. That is the whole point of this file.

**After your partner's session, skim the Review Queue too.** Their agent's choices bind
your subsystems as surely as yours do.

## Status legend

| Mark | Meaning |
| --- | --- |
| 🔒 **LOCKED** | Decided and human-approved. Not reopened without saying so explicitly and getting a yes. |
| ⚠️ **UNREVIEWED** | Made by Claude during a session without a human in the loop. **Binding on nothing until a human signs it.** |
| ❓ **OPEN** | Not decided. Blocking something. Tracked by a board item. |
| 🗑 **SUPERSEDED** | Replaced. Kept with a pointer, never deleted — the reasoning is evidence. |

**Claude may never change a decision's status to LOCKED.** Only Camden or Lance does
that, by editing the `Reviewed` field, in a commit authored by that human.

---

## ⚠️ REVIEW QUEUE — read this first

Decisions currently awaiting a human signature:

| ID | Decision | Made by | Session date | Reviewer needed |
| --- | --- | --- | --- | --- |
| D-007 | Honda testbed is a 2015 Honda CR-V EX-L (§A.3) | Camden + Claude | 2026-09-21 | Lance — it is your car; confirm and close issue `honda` |
| D-012 | Rate-limited, round-robin Mode 01 requests are the data path (§A.7) | Camden + Claude | 2026-09-21 | Lance — sets the sample rate every analysis stage sees |
| D-013 | Phone views status over the device's own Wi-Fi; read-only, obdctl stays primary (§B.4) | Camden + Claude | 2026-09-21 | Lance — `src/interface/` is shared |
| D-017 | Commit to mechanisms D and E; B and F only if a Pi-side sensor is added (§B.1, supersedes D-002) | Camden + Claude | 2026-09-21 | Lance — D is half of what we are graded on |
| D-008 | Milestone dates verified against the syllabus (§F.3) | Camden | 2026-09-21 | Camden — your answer; mark it LOCKED in your own commit |
| D-019 | The Pi reads OBD2 over a Bluetooth Classic (SPP) adapter, OBDLink LX; USB is the fallback (§A.1, supersedes D-015) | Camden + Claude | 2026-09-23 | Both — Camden signs his call; Lance checks the radio-sharing and key-off risks |
| D-018 | Markdown linted at a 90-column wrap, American spelling checked by cSpell (§F.4) | Claude, Camden's session | 2026-09-23 | Either — it sets the style both of you write docs in |
| D-021 | One-car monitor that learns normal and names the fault area; supersedes 🔒 D-003 (§E.1) | Camden (reopened D-003) + Claude | 2026-09-23 | Lance — the analysis and evaluation now target fault areas, not three diagnostics |
| D-020 | `docs/milestones.md` is the board; board.toml retired (§F.5) | Camden + Claude | 2026-09-23 | Lance — changes how you track and close your work |
| D-022 | Testbeds: Camden's 2008 Forester and Lance's 2015 CR-V; neither required of anyone (§A.2, supersedes D-016) | Camden (in session) + Claude | 2026-10-21 | Both — Lance, it names your car as one of two, and rewords the claim |
| D-023 | The 48-hour soak is 48 hours wall-clock of normal use across key cycles (§E.3, answers D-006); confirmed by the professor (Camden, 2026-10-26) | Camden (in session) + Claude | 2026-10-21 | Both — it decides what the 30-point final-system soak is |
| D-024 | One box, one USB-C input, a seconds-scale supercapacitor; clean shutdown at key-off, crash consistency as the guarantee (§A.6, supersedes D-011) | Camden (in session) + Claude | 2026-10-21 | Both — it is the whole hardware design, and nothing is bought until it is signed |
| D-025 | Write-ahead transactions (`BEGIN`, records, `COMMIT`, one `fsync`) and A/B baseline checkpoints; extends 🔒 C.1 (§C.3) | Camden (extended C.1) + Claude | 2026-10-21 | Lance — it is the storage design behind mechanism D |
| D-027 | One tri-color status light; every live state pulses; steady yellow while booting (§B.5, supersedes D-014) | Camden (in session) + Claude | 2026-10-21 | Both — it is the one output a driver sees |
| D-028 | The phone page also shows trends and downloads a report (HTML + CSV, served by C); an app is a stretch (§B.6, extends D-013) | Camden (in session) + Claude | 2026-10-21 | Either — `src/interface/` has no owner yet |
| D-029 | `pisensor` declined: no Pi-side sensor, so B and F are out of scope and D and E are the committed pair (§B.7); revised by D-037 | Camden (in session) + Claude | 2026-10-21 | Both — D and E are now the whole mechanism grade, with nothing in reserve |
| D-037 | A DS18B20 probe on the Pi, read by its own process into a second ring; reopens D-029 (§B.9) | Camden (in session) + Claude | 2026-10-26 | Both — it adds a part, a daemon and a ring, and it is the answer to handout §3.1 |
| D-030 | A real-time clock module on I2C keeps wall time while the box is off; the phone page stays read-only (§A.8) | Camden (in session) + Claude | 2026-10-21 | Both — it adds a part, and the soak's timeline depends on it |
| D-031 | One SPSC ring, `obdd` to `storaged`; `analyzed` reads only committed records from the log (§B.8) | Camden (in session) + Claude | 2026-10-21 | Both — it fixes the M2 diagram and sets how analysis gets its input |
| D-033 | Drive logs are JSONL from a Python tool on the Mac, one file per session, VIN stored only as a hash; not the product's storage format (§C.4) | Camden (in session) + Claude | 2026-10-26 | Either — it is the format of every real-car capture until `obdd` exists |
| D-034 | Draft on-disk record format: a 40-byte header, typed body and CRC32, six record kinds, little-endian, 16 MB segments (§C.5, finishes D-025) | Claude, Camden's session | 2026-10-26 | Both — it is the format `storaged` writes and `analyzed` reads; nothing is coded yet |
| D-035 | Draft fsync discipline: commit and fsync once a second or every 64 samples, whichever comes first (§C.6, closes C.1's open sub-decision if signed) | Claude, Camden's session | 2026-10-26 | Both — it sets what a power cut can lose and how far analysis lags |
| D-042 | Draft event path: every daemon sends its events to `storaged` over one `SOCK_SEQPACKET` Unix socket, written in the next batch (§B.10) | Claude, Camden's session | 2026-10-27 | Both — it is how verdicts and faults become durable |
| D-043 | Draft control socket: a line protocol on `/run/carwatch/ctl.sock`, the five `obdctl` verbs with their arguments, and `statusd` as the page process (§B.11) | Claude, Camden's session | 2026-10-27 | Both — it is the whole query interface, and the live-modification station will extend it |
| D-041 | Draft retention: every segment kept until the disk is 80 % full, then oldest whole segments first, never one a checkpoint needs (§C.7) | Claude, Camden's session | 2026-10-27 | Both — it is how long the device remembers |
| D-036 | Draft evaluation targets: latency, kill-and-recover, drops, memory, detection and crash recovery, each with a number (§E.4) | Claude, Camden's session | 2026-10-26 | Both — numbers committed at M2 are what M5 is judged against |
| D-032 | Python tools are tested with stdlib `unittest` in a venv that `make test` builds on first use (§F.6) | Camden (in session) + Claude | 2026-10-26 | Either — it adds a step to `make test` and installs two pinned packages |
| D-026 | No pre-assigned owners; a subsystem belongs to its first substantive commit; supersedes 🔒 F.2 (§F.2) | Camden (reopened F.2) + Claude | 2026-10-21 | Lance — it removes every assignment the M0 map gave you, and changes CLAUDE.md §0 and §6 |

> Decisions marked 🔒 were made with Camden in the conversation. **Lance has not reviewed
> any of them yet** — Lance, read at minimum §B.1, §C.1 and §D.1, since those bind storage
> and analysis.

---

## A. Hardware and interfaces

### A.1 — The Pi reads OBD2 over a Bluetooth Classic adapter; USB is the fallback ⚠️ UNREVIEWED

`D-019` · Decided M1, 2026-09-23 · **By:** Camden (in session) + Claude
· **Reviewed:** Camden ⬜ / Lance ⬜
· **Supersedes:** D-015 (§G.4)

*Pi power revised 2026-10-21 by D-024 (§A.6), before any review: the Pi is in a box fed
by the owner's USB-C charger, not wired to the car's USB. The adapter's key-off draw is
now board item `dongledrain`.*

**Decision.** An **OBDLink LX** (STN chip, **Bluetooth Classic, Serial Port Profile**; not
BLE) plugs into the OBD2 port. The Pi, powered from the car's USB (D-011), talks to it with
its built-in Bluetooth radio over RFCOMM. Provisioning pairs and trusts the adapter once,
binds it with `rfcomm bind` to `/dev/rfcomm0`, and a udev rule links that to **`/dev/obd`**,
so every consumer still opens `/dev/obd` and sends Mode 01 requests (D-012). No MCP2515, no
SPI, no CAN wiring, and no data cable in the cabin.

**Raw CAN is still a stretch goal**: an MCP2515 on SPI, taken up only after mechanisms D
and E are implemented *and measured* (unchanged from D-015).

**Why.** Camden's call, 2026-09-23: the goal is the simplest data path that works. The box
becomes one power cable; the adapter stays in the port. The protocol, the parsing and every
downstream design are the same as D-015, because an STN adapter speaks the same ELM327
command set over Bluetooth as over USB.

**Why a tty and not our own Bluetooth socket.** Opening an RFCOMM socket from `obdd` would
put `socket()` in `src/obd/`, which `make boundary` forbids outside `src/interface/`
(CLAUDE.md §2.1). Letting the kernel's RFCOMM layer present a tty keeps the boundary check
meaningful and keeps `obdd` a plain serial reader.

**What D-001 (§G.1) said against this, and why it no longer applies.** G.1 rejected a
Bluetooth ELM327 because it gives up interrupts and bus-level timing, taking mechanisms A,
B, C and F with it. D-015 already accepted exactly that loss for the USB adapter, and D-017
records what survives (D and E). Bluetooth costs no further mechanism.

**Costs and risks.**

- **Provisioning.** Pairing, trust and a persistent bind must survive reboots and every
  key-off (`scripts/provision_pi.sh`, board item `bench`).
- **A new failure mode.** The link can drop while the engine runs (adapter out of range,
  adapter asleep, radio interference). `obdd` sees a hangup on the tty and must reconnect
  with backoff, not crash-loop (`docs/DESIGN.md` §3, `supbackoff`).
- **One radio, two jobs.** The Pi 4's single Wi-Fi/Bluetooth chip also runs the phone
  access point (D-013). Coexistence is expected to work but is unmeasured: board item
  `btcoex`. Putting the access point on 5 GHz is the first mitigation to try.
- **Key-off draw.** The adapter still draws from OBD2 pin 16, live with the key off, and a
  Bluetooth adapter keeps a radio up. Its sleep behavior and key-off current are measured
  before it stays plugged in overnight (`dongledrain`).

**Fallback.** The USB adapter of D-015 (§G.4) needs only a different udev rule to appear as
`/dev/obd`. If Bluetooth proves unreliable in the car, switching back is a provisioning
change, not a code change.

### A.2 — Testbeds: Camden's 2008 Forester and Lance's 2015 CR-V ⚠️ UNREVIEWED

`D-022` · Decided M2, 2026-10-21 · **By:** Camden (in session) + Claude
· **Reviewed:** Camden ⬜ / Lance ⬜
· **Supersedes:** D-016 (§G.6) · **Amends:** the claim wording of D-021 (§E.1)

**Decision.** Two cars are available as testbeds: Camden's **2008 Subaru Forester**
(non-turbo) and Lance's **2015 Honda CR-V EX-L** (D-007). Neither car is required of
anyone, and either partner tests wherever suits them. Every capture records which car it
came from.

**Why.** Camden's call, 2026-10-21 (`docs/REVISION_PLAN.md` §1.1, P1): a car on Camden's
own schedule lets live capture start without waiting on anyone, and no document should
restrict where either partner tests. The device learns whichever car it is installed in,
so a second car costs no code.

**The claim, reworded.** D-021's *"this CR-V"* becomes *"the car it is installed in"*
(§E.1). Results are reported per car and labeled with the car.

**Cost.**

- Each car needs its own PID survey (`pidforester`, `pidhonda`). The two sensor lists may
  differ, and the Forester's OBD protocol is not verified: a 2008 US car is expected to
  speak CAN (ISO 15765-4), but MAP, for one, may be absent.
- Baselines are per car. Two cars split the calendar time of normal driving unless one of
  them carries most of it.

**Alternative rejected.** Keeping D-016 (the CR-V only): every live capture would wait on
one car's schedule.

### A.3 — Honda year/model/engine ⚠️ UNREVIEWED

`D-007` · **Owner:** Lance · **Tracked as:** GitHub issue `honda` (label `question`)
· **Answered:** 2026-09-21, by Camden in session · **Reviewed:** Camden ✅ / Lance ⬜
· **Blocks:** the PID survey, which sets the sensor list

**Answer.** **2015 Honda CR-V EX-L.** By model year that is the 2015 refresh: 2.4 L
direct-injected four-cylinder (K24W) with a CVT. FWD or AWD is not yet recorded. The
engine and transmission are inferred from the model year, not read off the car — Lance
confirms against the VIN or the door-jamb sticker, then closes the issue.

**What the answer changes.**

- **No Honda Sensing on the EX-L** (it was Touring-only in 2015), so there is no
  lane-keep camera connector to tap. The OBD2 port is the tap point (**D-024**).
- **Oil pressure is expected to be a switch, not a sender** (inference from Honda
  practice). Under D-021 that only removes one sensor from the list; nothing depends on it.
- **The Honda has Maintenance Minder**, an oil-life estimate from how the engine has been
  run. We do not compete with it: carwatch watches every engine sensor for behavior that
  leaves this car's normal, which the Minder never measures.

We still need, from the car itself, which Mode 01 PIDs the ECU actually supports: that
list is the model's input.

**Action.** Run a supported-PID scan on the CR-V with the OBD2 adapter (D-019) —
a ten-minute experiment. Record in `docs/hardware/pid-survey.md`.

### A.4 — Capture the schematic in KiCad; do not fabricate a PCB 🔒

`D-009` · Decided M0 · **By:** Camden + Claude · **Reviewed:** Camden ✅ / Lance ⬜

**Decision.** We draw the full schematic in KiCad (`electricalDrawing/`, KiCad 10, owned
by Lance) and treat it as the source of truth for wiring and the BOM. We do **not**
fabricate a custom PCB unless we take The Reach (handout §14). The empty `.kicad_pcb`
stays in the repo on purpose.

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

### A.5 — ERC is a build failure, and the schematic SVG is committed 🔒

`D-010` · Decided M0 · **By:** Camden + Claude · **Reviewed:** Camden ✅ / Lance ⬜

**Decision.** `kicad-cli sch erc --exit-code-violations` runs as `make hwcheck`; violations
fail the build. `make hwdocs` renders the schematic to `docs/figures/electricalDrawing.svg`,
and that SVG is **committed**.

**Why the ERC gate.** It makes electrical correctness the same kind of object as
`-Wall -Wextra -Werror`: a machine-checked precondition rather than a thing someone
remembers to look at. Unconnected pins and power-output conflicts are exactly the class of
error that survives a human read and kills a board.

**Why commit a generated file.** Normally we would not. But a reviewer cannot read a diff
of S-expression coordinates, and §7.6 says a PR nobody understands does not merge — so
without a rendered picture, hardware would fall outside the review process entirely. The
SVG is the diff a human can actually read. It is regenerated, never hand-edited.

**Open sub-item (Lance, M2):** `sym-lib-table` referenced `CS370_Project_Library.kicad_sym`,
which did not exist — KiCad errors on project open. An empty library is committed to resolve
it; populate it as parts are drawn.

### A.6 — One box, one USB-C input, and a seconds-scale supercapacitor ⚠️ UNREVIEWED

`D-024` · Decided M2, 2026-10-21 · **By:** Camden (in session) + Claude
· **Reviewed:** Camden ⬜ / Lance ⬜
· **Supersedes:** D-011 (§G.9) · **Revises:** D-019's "the Pi, powered from the car's USB"

**Decision.** The product is one small box, plus the Bluetooth OBD2 dongle (D-019) in the
car's OBD2 port.

- **One port.** A single USB-C receptacle is the box's only power input. The owner plugs
  it into whatever USB-C charger and cable they already have in the car. The box is a
  plain sink (5.1 kΩ on both CC pins), so no source offers it more than 5 V.
- **One light, no buttons, no other ports** (light: D-027). Nothing on the car is cut,
  spliced, pierced or clamped.
- **Off at rest.** The box runs only while its source is live. At key-off the source
  disappears; the box sees it on a source-present GPIO, waits out a short grace period,
  commits and checkpoints its storage, and halts. The power stage then cuts the Pi's
  supply, and the box stays dark until the next start, when it boots by itself.
- **A small supercapacitor bank** covers brief shortfalls, rides through the dip at
  engine start where it can, and powers the clean shutdown. A seconds-scale bank cannot
  make up for a source that is too weak for a whole drive; see the limits below. It is
  short-lived by design: the goal is seconds to tens of seconds of runtime, about one
  minute is the ceiling, and it is never an always-on supply.
- **Clean shutdown is the normal path; crash consistency is the guarantee.** If the bank
  is empty, the cable is pulled or the Pi resets at crank, storage recovers at the next
  boot (mechanism D).

**The power block's requirements.** Parts come only from datasheets a human has checked
(`CLAUDE.md` §8.4), and nothing is ordered until the design is signed.

1. Sink only, 5 V only; transient and reverse-polarity protection; a fuse.
2. An input current limit below what the source advertises, and an inrush limiter,
   because an empty bank looks like a short circuit. A plain sink does not know the
   source's rating, so the box reads the source's CC advertisement (USB default, 1.5 A or
   3 A). The Pi has no analog inputs, so that takes a detector part (comparators or a
   USB-C controller IC) that reports the level on GPIOs.
3. Power path: the source feeds the Pi first; the bank covers only the shortfall or the
   gap.
4. A source-present signal on a GPIO, divided down to 3.3 V.
5. A halt-done signal: once the Pi has halted, the power stage removes the Pi's supply
   **even if the source has come back**, holds it off briefly, then re-enables. Otherwise
   a source that returns mid-shutdown leaves a halted Pi and a dark box in a running car.
6. Auto power-on when the source returns, with no button.
7. A 5 V, 3 A converter to the Pi that can step up, because the bank's voltage falls as it
   discharges.

**Estimates, not measurements.** Each is replaced by a measured value before it appears
in `docs/DESIGN.md` as anything but an estimate (`powerspec`, `capbench`).

| Quantity | Estimate |
| --- | --- |
| Pi 4 draw during boot and shutdown | 3 to 5 W |
| Grace period before shutdown starts (longer than a crank dip) | about 3 s × 4 W ÷ 0.85 ≈ 15 J |
| Clean-shutdown energy | 10–15 s × 4 W ÷ 0.85 converter efficiency ≈ 50–70 J |
| Crank-bridge energy | 1–3 s × 5 W ≈ 5–15 J |
| Two 2.7 V cells in series, 10 F total, charged to about 5 V | ½·C·V² ≈ 125 J stored; about 105 J usable above a 2 V converter floor, about 20 s at 4 W |
| Charge time from empty at a 1 A budget | 10 F × 5 V ÷ 1 A ≈ 50 s; slower on a 1.5 A source that is also running the Pi |

The margin is thin: grace plus shutdown come to about 65–85 J against about 105 J usable,
before the radios' draw is counted. `powerspec` replaces every row with a measurement and
states the margin.

**Limits, stated plainly.**

- **Cold start.** The bank is empty at the first key-on of a session, so it cannot bridge
  that crank dip, and the Pi may reset while cranking. That is survivable only because
  storage is crash-consistent. Crank bridging is best-effort; the key-off shutdown is the
  reliable path.
- **Always-live sources are not supported.** On a charger that stays live with the key
  off, the box stays on, and the Pi plus the dongle can drain the car's battery over a few
  days. This is an installation requirement, not a mode we engineer around, but whether
  the box should halt itself after a long stretch with the engine silent is open
  (`shutfsm`).
- **Weak sources.** A charger that advertises only default USB current (500 or 900 mA)
  cannot run a Pi 4 with both radios, and the bank cannot make up the difference for a
  whole drive. What the box does then (refuse to start and show red, or run degraded) is
  open (`powerspec`).
- **Boot time.** A stock Pi 4 boots in roughly half a minute (estimate, `bootbudget`), so
  the start of each drive's warm-up goes unrecorded.
- **Demo day.** The box is off unless powered, and the handout's live demonstration runs
  on live sensors (§5, §11), so the demo needs a running testbed car on site, or an
  arrangement with the professor (`democar`).
- **The dongle still draws from OBD2 pin 16 with the key off** (D-019). Its key-off
  current is measured before it stays plugged in overnight (`dongledrain`).

**Why one box.** Camden's design, 2026-10-21 (`docs/REVISION_PLAN.md` §1.1, P7–P10,
P13): "no need to reinvent the charger." The owner already has a USB-C charger, so the
team designs the box and buys only the dongle and a cable. One port and one light is a
product an ordinary owner can install.

**Why a supercapacitor.** Camden's choice, 2026-10-21 (R1). A lithium cell must not
charge above about 45 °C or below 0 °C, and a car cabin leaves that window at both ends;
many cheap Pi UPS boards have no temperature cutoff. A supercapacitor has no
thermal-runaway failure, tolerates a wider temperature range (confirm against the part's
datasheet), and fits a seconds-scale job. Its costs are low voltage per cell (two in
series, with balancing), very low internal resistance (so a fuse), and fast
self-discharge, which does not matter for a box that is off at rest.

**Alternatives rejected.** The car's USB wired straight to the Pi (D-011): no warning
before power loss, so every key-off cut a write. A small lithium cell: the charge
temperature window above. A wake button: dropped (P10).

**Cost.** A power block to design, draw, buy and bench-test, on the hardware critical
path (`powerspec`, `schpower`, `capbench`), and more to measure before any claim
(`keyoff`, `cranklog`, `expshut`).

### A.7 — Standard Mode 01 requests are the primary data path ⚠️ UNREVIEWED

`D-012` · Decided M1, 2026-09-21 · **By:** Camden + Claude
· **Reviewed:** Camden ✅ / Lance ⬜

*Revised 2026-09-21 by D-015, before any review: the CAN-level detail (arbitration IDs,
broadcast frames, a listen-only flag, the port-traffic check) went with the MCP2515.*

**Decision.** The capture daemon gets its signals by sending **standard OBD2 Mode 01
requests** through the OBD2 adapter (D-019) on a fixed, rate-limited, round-robin schedule,
one request outstanding at a time. Every timeout and every unanswered PID is logged.

**Why.** D-011 (now D-024) makes this a plug-in device, and the thing that makes a plug-in
device work on a car it has never seen is the part of the protocol every car must speak:
Mode 01 is mandatory on US cars from model year 1996. *"Learns this car's normal"* is
precisely what lets one device serve different cars without per-model decode work: it
never needs to know what normal *is* ahead of time.

**Sample-rate consequence.** Signals are round-robined, so each PID is sampled at
(request rate ÷ PID count) — on the order of 1 Hz each for a ~10 requests/s budget over
~8 PIDs. That is ample for what we watch (warm-up over minutes, fuel trim against load,
drift over days) and must be stated in `docs/DESIGN.md` §4. The request budget itself
is a design-doc number to justify, not a constant to pick.

**Cost.** The device sends requests to the car's ECU. Kept safe by the rate limit, one
outstanding request, and only standard read-only Mode 01 requests — the same thing every
scan tool does.

**Scope note.** "Works on any car" is the product's direction, not our claim. The claim is
E.1 (D-021): one car, a learned normal, fault areas named, measured error rates.

### A.8 — A real-time clock keeps wall time while the box is off ⚠️ UNREVIEWED

`D-030` · Decided M2, 2026-10-21 · **By:** Camden (in session) + Claude
· **Reviewed:** Camden ⬜ / Lance ⬜

**Decision.** The box gets a small real-time clock module on the Pi's I2C bus, kept
running by its own backup cell or capacitor while the box is off. At boot the system
clock is set from it. If the clock reports its time as invalid (its backup was lost),
that session's wall time is marked untrusted. Every record still carries `mono_ns`
regardless. The phone page stays read-only: it does not set the clock.

**Why.** The Pi has no battery-backed clock, and the box has no network (`CLAUDE.md`
§2.3). Raspberry Pi OS's `fake-hwclock` restores the time saved at the last shutdown, so
the clock does not advance while the box is off, and every boot would appear to follow the
previous shutdown directly. The soak (D-023) must prove a 48-hour wall-clock span across
parked gaps, and the report's session list needs real times. Camden chose the clock
module, 2026-10-21, over the phone setting the time (that depends on someone opening the
page on every drive) and over no wall clock at all (`docs/REVISION_PLAN.md` §1.2, R5).

**Cost.**

- One more part, on I2C (GPIO2 and GPIO3, header pins 3 and 5), drawn in the Pi interface
  block, and bought only after the design is signed.
- Its backup cell or capacitor lives in a parked car's cabin, so its temperature rating
  must cover that, checked against the part's datasheet (`rtcspec`).
- A clock that has drifted or lost its backup must be detected rather than trusted, so the
  boot path checks it (`rtcboot`).

---

## B. Systems architecture and mechanisms

### B.1 — Mechanism commitments: D and E; B and F only with a Pi-side sensor ⚠️ UNREVIEWED

`D-017` · Decided M1, 2026-09-21 · **By:** Camden + Claude
· **Reviewed:** Camden ✅ / Lance ⬜
· **Supersedes:** D-002 (§G.3)

*B and F declined 2026-10-21 by D-029 (§B.7), before any review: no Pi-side sensor will be
added, so D and E are the committed pair and the "only if" rows below no longer apply.*

The handout requires two, implemented by us and measured.

| Mechanism | Status | Where |
| --- | --- | --- |
| **D** — custom append-only storage, crash-consistent | **committed** | `src/store/` |
| **E** — multi-process with a supervisor | **committed** | `src/supervisor/`, `src/ipc/` |
| **B** — interrupt-driven input with a polling comparison | declined (D-029) | — |
| **F** — high-rate no-drop SPSC ring | declined (D-029) | — |
| **A**, **C**, raw CAN | stretch, after D and E are measured | — |

**D.** *Power disappears every time the key turns off* (D-024). The box shuts down
cleanly on its supercapacitor when it can, but the bank is empty at a cold start, a
pulled cable beats it, and a crank dip can reset the Pi, so storage must survive a cut at
any instant. *(Revised 2026-10-21 by D-024; it read "we never get a clean shutdown".)*
Measured by cutting the Pi's own 5 V rail mid-write, after the converter so the
supercapacitor cannot cover it, repeatedly, and proving recovery (`expcrash`). Pulling
the charger no longer tests this: it triggers a clean shutdown.

**E.** A recorder that dies silently has actively harmed its user, who believes it is on
duty. Any child may be `kill -9`'d; the system degrades, logs, recovers. Measured by killing
each child repeatedly and timing detection and recovery (`expkill`).

**Why B and F left the OBD data path (D-015, D-019).** B needs an interrupt that *our*
design services. The adapter's interrupts belong to the kernel's Bluetooth and RFCOMM
stack; our process only blocks on a tty, and at ~10–20 replies/s a poll loop keeps up
trivially — the comparison would measure the tty layer, not a design of ours. F needs a
*high-rate* stream; ~10–20 samples/s never stresses a ring, so "zero drops under
contention" would be true and meaningless.

**What brings them back.** An MPU-6050 on the Pi's I2C, its `INT` pin on a GPIO: kHz
samples from a hardware FIFO give a real interrupt-vs-polling comparison (B) and a rate
that stresses the ring (F). That was board item `pisensor`; D-029 declined it.

**The ring stays regardless.** It is how capture hands samples to storage across the
process boundary E requires, and B.3 still binds it.

**Risk.** D and E alone meet the floor exactly. If either measurement fails, nothing is in
reserve. `pisensor` was that reserve, and D-029 declined it.

### B.2 — Four processes, not one 🔒

`D-002` (corollary) · **By:** Camden + Claude · **Reviewed:** Camden ✅ / Lance ⬜

`obdd`, `storaged`, `analyzed` and `supervisor` are separate processes with separate
address spaces, communicating over a shared-memory ring and Unix domain sockets.

**Why.** Fault isolation is the product requirement, not an architectural preference: the
analysis engine is the most complex and least trustworthy component, and it must not be
able to take the capture path down with it. One process would be simpler and would fail
the thing the device exists to do.

### B.3 — The ring refuses rather than overwrites 🔒

Decided M0 · **By:** Claude, from D-002 · **Reviewed:** Camden ✅ / Lance ⬜

When the ring is full the producer returns failure and increments an overrun counter. It
does **not** overwrite the oldest unread slot.

**Why.** Overwriting loses data silently, and silent loss is the exact failure mechanism F
exists to make impossible. A refusal is visible, countable, and reportable; an overwrite
looks like a quiet minute. Enforced by `tests/test_ring.c`.

**Reviewer note.** This is a real trade: under sustained overload we drop *new* data
rather than *old*. If that is ever the wrong choice for a diagnostic, reopen this.

### B.7 — No Pi-side sensor: mechanisms B and F are out of scope ⚠️ UNREVIEWED

`D-029` · Decided M2, 2026-10-21 · **By:** Camden (in session) + Claude
· **Reviewed:** Camden ⬜ / Lance ⬜
· **Resolves:** board item `pisensor`, and the "only if" conditions of D-017 (§B.1)

*Revised 2026-10-26 by D-037 (§B.9), before any review: one Pi-side sensor, a DS18B20
probe, is added for handout §3.1 and mechanism E's wording. B and F stay out of scope,
and the "no MPU-6050, no DS18B20" below now reads "no MPU-6050".*

**Decision.** No sensor is added on the Pi itself: no MPU-6050, no DS18B20. Mechanisms
**D** and **E** are the committed pair, implemented and measured. **B** and **F** are out
of scope. **A**, **C** and raw CAN stay stretch goals.

**Why.** Camden declined `pisensor` in the 2026-10-21 session (`docs/REVISION_PLAN.md`
§1.1, P5). The device's job is the car's engine sensors, and D and E follow from that job
(§B.1).

**Risks, stated plainly.**

- D and E meet the handout's floor of two exactly (§3.2). If either measurement fails,
  nothing is in reserve, so `expcrash` and `expkill` are never cut (PLAN.md §3).
- Every sensor now reaches the Pi through one dongle. The handout asks for at least two
  physically distinct sensors that cooperate (§3.1). Our answer: coolant temperature,
  intake air temperature, MAP or MAF, the fuel trims and the O2 sensors are physically
  distinct sensors on the engine, fused by our pipeline, and the dongle is their
  transport, not a sensor. `docs/DESIGN.md` §2 must defend this; a skeptical grader will
  ask.
- The SPSC ring stays, because B.3 binds it and it is the capture-to-storage handoff
  mechanism E needs. It is not claimed as mechanism F: Mode 01 rates never stress it.
- The handout words mechanism E as "sensors isolated in separate processes". Here one
  `obdd` reads every sensor, because they share one dongle. The isolation we defend is
  between capture, storage and analysis (B.2); `docs/DESIGN.md` §2 must say so.

**Alternative rejected.** An MPU-6050 to bring back B and F (D-017's option): more
hardware, for a sensor the user's problem does not need.

### B.8 — One ring, capture to storage; analysis reads the committed log ⚠️ UNREVIEWED

`D-031` · Decided M2, 2026-10-21 · **By:** Camden (in session) + Claude
· **Reviewed:** Camden ⬜ / Lance ⬜
· **Fixes:** `docs/DESIGN.md` §1, which fanned one single-consumer ring out to three
readers

*Revised 2026-10-26 by D-037 (§B.9), before any review: a second SPSC ring carries the
DS18B20 probe's samples from `ambientd` to `storaged`. Each ring keeps one producer and
one consumer, so the reasoning below holds for both.*

**Decision.** The shared-memory SPSC ring has exactly one producer, `obdd`, and one
consumer, `storaged`. `analyzed` does not read the ring: it reads `storaged`'s log, in
order, and only records whose batch has been committed to disk. "Committed" means
durable, not merely written: a reader of the file sees a `COMMIT` in the page cache before
`fsync` returns, so `storaged` publishes a **durable offset** only after `fsync` returns,
and `analyzed` never reads past it. `obdctl` and the status page never read samples
themselves; they ask the supervisor over its Unix socket, and the supervisor asks the
process that holds the answer.

**Why.** An SPSC ring has one consumer, so the old diagram could not be built as drawn.
Camden chose this over two rings, 2026-10-21:

- the ring stays truly single-producer, single-consumer, which B.3 and its tests assume;
- `analyzed` learns only from what is durable, so a baseline never absorbs a sample that
  a power cut later erased;
- rebuilding baselines after a crash reads the same log through the same code as live
  analysis, so recovery is not a second implementation to test;
- `analyzed` can crash or restart without losing input: it resumes from where it last
  read.

**Cost.** Analysis lags capture by about one commit batch (about a second, provisional:
`fsync`), which is nothing for signals that move over minutes. `analyzed` depends on the
log format, so a format change touches both processes. If `storaged` is down, analysis
stalls rather than running on unrecorded data, and the light shows red. A reader of a file
that is still growing must stop at the durable offset and treat anything after it as not
there yet.

**Open, for the stage-2 architecture (`archdiag`).** The ring carries only samples, so this
entry does not yet say how `analyzed`'s verdicts and the supervisor's fault and heartbeat
events reach durable storage, which the report and `obdctl verdicts --since` need. The
obvious answer is a Unix socket to `storaged`; it is not decided here. *Answered in draft
2026-10-27 by D-042 (§B.10).*

**Alternative rejected.** Two rings, `obdd` pushing every sample into both: lower
analysis latency, but the analysis ring overruns whenever `analyzed` restarts, and
recovery would need its own replay path.

### B.9 — A DS18B20 probe on the Pi, read by its own process ⚠️ UNREVIEWED

`D-037` · Decided M2, 2026-10-26 · **By:** Camden (in session) + Claude
· **Reviewed:** Camden ⬜ / Lance ⬜
· **Revises:** D-029 (§B.7), which declined every Pi-side sensor; D-031 (§B.8), which had
one ring; D-034 (§C.5), whose sample record gains a namespace for it
· **Answers:** board item `pisensor`

**Decision.** The box gets one sensor of its own: a **DS18B20** digital temperature probe
on a short lead outside the shell, in cabin air.

- **Wiring.** The standard 1-Wire hookup: data on GPIO4 (header pin 7, the default of
  the kernel's `w1-gpio` overlay) with a 4.7 kΩ pull-up to 3.3 V, power from 3.3 V, not
  parasite power. Checked against the part's datasheet and drawn in the Pi interface
  block (`schpi`) before it is wired.
- **Its own process.** `ambientd` (`src/ambient/`) reads
  `/sys/bus/w1/devices/28-*/w1_slave` every 2 seconds. A reading whose CRC line ends in
  `NO`, or that holds the 85.0 °C power-on value, is rejected and logged. A missing
  device is `sensor_absent`, retried every 10 s, and `sensor_present` when it returns.
- **Its own ring.** A second shared-memory SPSC ring, `ambientd` to `storaged`, with the
  same type and the same refuse-when-full rule as the first (B.3). Each ring keeps one
  producer and one consumer, so D-031's reasons for one consumer still hold; `storaged`
  drains both into the same batches.
- **Its records.** An ordinary sample record (D-034) with `mode` 0x00, which no OBD2
  request uses, as the namespace for the box's own sensors, and `pid` 0x01 for this
  probe, value in °C.

**What it cooperates in.**

- **The cold-soak check** (sensor/electrical area). After the box has been off for hours
  (the clock says how long, D-030), coolant temperature, intake-air temperature and the
  probe should agree within a few degrees at key-on. When two agree and one does not,
  the one that disagrees is the suspect. No single threshold on one value can say that.
- **Ambient for the cooling area** when the car does not publish PID 0x46, using the
  key-on reading after a long soak.

**Why.** Camden, 2026-10-26, choosing the audit's option for two rubric risks:

- Handout §3.1 asks for at least two physically distinct sensors that cooperate. Through
  the dongle alone, every sensor reaches the Pi by one device, one link and one process,
  and a grader may call that one sensor. The probe is a second device on a second bus.
- Handout §3.2 words mechanism E as "sensors isolated in separate processes". With
  `obdd` and `ambientd`, that is now literally true, on top of the isolation of capture,
  storage and analysis (B.2).
- Demo day unplugs a sensor (handout §11). The probe is one a grader can pull by hand,
  without opening the hood of a running car.

**What it does not change.** B and F stay declined: 1-Wire's bit timing is done by the
kernel, so there is no interrupt of ours to compare against polling, and 0.5 samples a
second never stresses a ring. D and E remain the committed pair (D-017).

**Cost.**

- One part, a resistor and a GPIO; bought only after it is drawn (`probeorder`) and
  brought up on the bench with the evidence pasted (`probebench`).
- A second ring, a fifth daemon, and their rows in the failure table and the soak plot.
- The probe reads cabin air: equal to outside air only after a long soak, and
  sun-heated in summer. The design doc states this as a substitution.

**Alternative rejected.** An MPU-6050 (D-017's option): it would bring back B and F, but
it is more parts and a sensor the user's problem does not need. The written defense
alone ("the dongle is only transport"): it leaves the §3.1 question to the grader.

### B.10 — Events reach storage over one Unix socket to `storaged` (draft) ⚠️ UNREVIEWED

`D-042` · Drafted M2, 2026-10-27 · **By:** Claude, in Camden's session
· **Reviewed:** Camden ⬜ / Lance ⬜
· **Answers (if signed):** D-031's open item (§B.8) and board item `verdictpath`

**Decision (draft).** Samples travel by the rings; everything else that must be durable
is an **event**, and events travel by one socket.

- **The socket.** `storaged` listens on a Unix domain socket of type `SOCK_SEQPACKET`,
  `/run/carwatch/events.sock`, created by the supervisor's runtime directory, mode 0600.
  One packet is one event: `code` u16, then up to 200 bytes of `key=value` text. A
  sequenced packet socket keeps message boundaries and order and never splits an event.
- **Who sends.** Every daemon sends its own events: `analyzed` its verdicts (and also
  tells the supervisor, which drives the light), the supervisor its child, power and
  clock events, `obdd` and `ambientd` their link and sensor events. The event names are
  the failure table's log events (`docs/DESIGN.md` §4).
- **What `storaged` does.** It writes each event as an `event` record (D-034) in the next
  batch, so an event is durable at the same commit as the samples around it and is
  ordered with them by `seq`.
- **When `storaged` is down.** A sender keeps up to 64 unsent events in memory and drops
  the oldest beyond that, counting the drops; the count is in its heartbeat. Every event
  is also a log line in the journal at the moment it happens, so the journal is the
  record of last resort and the socket never blocks a sender (`MSG_DONTWAIT`).

**Event codes.** High byte by sender, low byte by event: `0x01` supervisor, `0x02`
`obdd`, `0x03` `ambientd`, `0x04` `analyzed` (verdicts), `0x05` `storaged` itself (for
example `recovered`, `segment_dropped`). The table of codes lives in one header,
`include/common/events.h`, and nowhere else.

**Why.** D-031 left this open, and the report (D-028) and `obdctl verdicts --since` need
verdicts and faults on disk, in order with the samples. One socket keeps `storaged` the
only writer of the log (C.1). A packet socket is the simplest IPC that keeps one event
one message. A sender that blocked on a dead `storaged` would let a storage fault stop
capture or analysis, which B.2 exists to prevent.

**Cost.** One more listener in `storaged` and one more fd per sender. Events that arrive
while `storaged` is down and overflow the 64-event queue are lost from the log (but not
from the journal), and the drop count says how many. The socket is `AF_UNIX`, so the
boundary rule must allow it outside `src/interface/` (`make boundary`).

**Alternative rejected.** Each process writing its own event file: several writers, no
common order with the samples, and one more crash-recovery story per file. Events in the
rings: the slot type is sized for a reply, and the supervisor, which has no ring, could
not send.

### B.11 — The supervisor's control socket, the five `obdctl` verbs, and `statusd` (draft) ⚠️ UNREVIEWED

`D-043` · Drafted M2, 2026-10-27 · **By:** Claude, in Camden's session
· **Reviewed:** Camden ⬜ / Lance ⬜
· **Answers (if signed):** board item `ipcspec`; names the status-page process D-013 and
D-028 left unnamed

**Decision (draft).**

- **The control socket.** The supervisor listens on `/run/carwatch/ctl.sock`, a Unix
  domain `SOCK_STREAM` socket, mode 0660. A request is one line, `VERB key=value ...`,
  at most 512 bytes. A reply is zero or more lines of `key=value` fields, then `ok`, or
  `err <code> <reason>`, then the connection closes. Text, so it can be typed by hand
  and quoted in the report and at the defense.
- **Routing.** The supervisor answers `status` itself, from its children's heartbeats.
  It forwards `verdicts`, `series` and `faults` to `storaged`, which reads its own
  committed log up to the durable offset (D-031), and `baseline` to `analyzed`, the only
  holder of baselines, each over that process's own query socket in the same format. A
  child that is down gives `err unavailable <name>`; nothing else waits on it.

**The five verbs.**

| Verb | Arguments | Answers |
| --- | --- | --- |
| `status` | none | each process: up, down or restarting, pid, uptime, restarts, RSS, CPU, event and drop counts; each ring's depth and overruns; dongle link and probe present or absent; the light's state; whether wall time is trusted; the mix of `src` in this session |
| `verdicts` | `--since WHEN` (default: this boot) | each surfaced verdict: start, end or active, fault area, the sensors that cooperated, residual, `src` |
| `series` | `PID --from WHEN --to WHEN [--binned]` | the samples of one PID (`0C`, or `probe`) in the window, each with `src`; `--binned` gives per-bin means instead |
| `baseline` | `PID [--bin RPM,LOAD,COOLANT]` | the learned mean and variance of one PID in every bin, or in the one bin named |
| `faults` | `--since WHEN` (default: this boot) | detection, degradation and recovery events (D-042), each with `src` |

`WHEN` is ISO 8601 or relative (`-2h`). `obdctl` exits 0 on `ok`, 1 on `err`, 2 on a
usage error and 3 when the supervisor cannot be reached.

**`statusd`.** The phone page and the report generator (D-013, D-028) are one supervised
process, `statusd`, in `src/interface/`. It serves HTTP only on the box's access-point
address, and builds every page and report from the same control-socket verbs, so it holds
no state of its own. It is the only process with an `AF_INET` socket.

**Why.** Board item `ipcspec`: B.2, D-013 and D-031 all route queries through the
supervisor without saying how, the README's `baseline` line was ambiguous about bins,
and the status page had no process name for the failure table, the Makefile or the
soak's RSS plot. A line protocol over one socket is the smallest thing that lets the CLI,
the page and a human at a terminal ask the same questions. History is answered by
`storaged` because it owns the log; the supervisor does not hold data, so a query can
never slow the restart path.

**Cost.** Three query sockets (supervisor, `storaged`, `analyzed`) and their parsers,
each a place for a malformed request to be handled; every parser gets a test with bad
input. History queries cost `storaged` reads while it writes, which the CPU measurement
(D-036) will show.

**Alternative rejected.** `obdctl` reading the log files itself: simpler, but it breaks
D-031's rule that only the process holding the answer reads it, and the page would need a
second copy of that reader.

### B.4 — The phone is a read-only window over the device's own Wi-Fi ⚠️ UNREVIEWED

`D-013` · Decided M1, 2026-09-21 · **By:** Camden + Claude
· **Reviewed:** Camden ✅ / Lance ⬜

*Extended 2026-10-21 by D-028 (§B.6): the page also shows trends and downloads a report,
and the server-language sub-decision below is closed (C).*

**Decision.** The Pi runs its own Wi-Fi access point — WPA2, a per-device passphrase, **no
upstream connection, no internet**. The owner's phone joins it and opens a status page
served from `src/interface/` by a supervised child process. The page shows exactly two
things — **status** and **current verdicts** — rendered from the same supervisor UDS verbs
`obdctl` uses. It holds no state, runs no analysis, and writes nothing. `obdctl` remains
the primary, exact interface with all five verbs. No app, no account, no cloud, and the
phone never pairs over Bluetooth. (The Pi itself pairs with the OBD2 adapter, D-019.)

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
- One more process in the 48-hour RSS plot, and Wi-Fi radio power from the box's supply
  (D-024).
- The access point shares the Pi 4's one radio chip with the Bluetooth link to the adapter
  (D-019). Measured by `btcoex`.
- **Sub-decision, closed by D-028:** the server's language. C in `src/interface/` keeps an
  interpreter out of the memory-stability test; Python in `ui/` is permitted by CLAUDE.md
  §2.5 but adds ~30 MB RSS to the soak. Recommendation: C, because the page is tiny.
  Camden chose C, 2026-10-21.

### B.6 — The phone page also downloads a report; an app is a stretch ⚠️ UNREVIEWED

`D-028` · Decided M2, 2026-10-21 · **By:** Camden (in session) + Claude
· **Reviewed:** Camden ⬜ / Lance ⬜
· **Extends:** D-013 (§B.4)

**Decision.** The status page (D-013) shows status, current verdicts and trends, and has
one button: **Download report**. The report is one self-contained HTML file plus a CSV of
the same data, generated on the box by a C program in `src/interface/`. The page still
writes nothing to the device. A native phone app is a stretch goal, not planned for M2 to
M5.

**Report contents** (first draft; finalized in `reportspec`):

| Section | Content |
| --- | --- |
| Header | Box id, report time, whether wall-clock time is trustworthy, and the mix of data sources (`live`, `replay`, `synth`), always labeled (C.2) |
| Sessions | Each key cycle: boot id, start, end, duration, ended cleanly or recovered, records lost on recovery |
| Box health | Process liveness and restarts, ring overruns (must be zero), undervoltage and throttle events, temperature, the supercapacitor's self-check, free storage |
| Car findings | Each surfaced verdict: fault area, the sensors that cooperated to call it, residual and trend evidence, when it began and ended, confidence |
| Baseline maturity | Hours of normal driving learned per operating-point bin, so the owner knows how far to trust the verdicts |
| Absent signals | Any PID the car stopped answering, and for how long |

**Reachability.** The box is off whenever the car is (D-024), so a phone reaches it only
while the car runs, or while the box is plugged into any USB-C charger at home, where it
boots and serves the same page.

**Why.** Camden, 2026-10-21 (`docs/REVISION_PLAN.md` §1.1, P12): the owner views data and
trends on a phone and downloads a report that says what is wrong with the box or the car.
HTML opens on any phone with no app, and the CSV lets the owner or a mechanic take the
numbers elsewhere. C, as D-013 recommended: the server is small, and an interpreter would
add tens of megabytes to the 48-hour memory plot (R3).

**Rule kept from D-013.** The report is assembled from the same supervisor UDS verbs
`obdctl` uses, so every number in it can also be produced at the CLI, and every graded
behavior stays demonstrable through `obdctl` alone.

**Cost.** A report generator is new code in the product: it gets rows in the failure
table, its memory shows in the soak plot, and the boundary check still has to show the
page is the only network code.

**Alternatives rejected.** A PDF report (needs a PDF writer we would have to defend). A
native app (a second product to build). A Python server in `ui/` (the soak's memory).

### B.5 — One tri-color status light, and every live state pulses ⚠️ UNREVIEWED

`D-027` · Decided M2, 2026-10-21 · **By:** Camden (in session) + Claude
· **Reviewed:** Camden ⬜ / Lance ⬜
· **Supersedes:** D-014 (§G.10)

**Decision.** One LED, visible through the shell, shows red, yellow or green. It is
written **only by the supervisor** (D-014's rule, kept).

| Light | Meaning |
| --- | --- |
| green, slow pulse (about 1 Hz) | Ready: booted, recording, all systems go |
| yellow, medium blink (about 2 Hz) | Running on stored energy: riding a supply dip, or shutting down |
| red, fast blink (about 4 Hz) | An issue, in the box **or** in the car; the report says which |
| steady yellow | Booting: held on from power-up, before the supervisor runs |
| any other steady light, or dark while the car runs | **Not running.** Never a valid live state |
| dark while the car is off | Normal: the box is off |

Priority when more than one applies: red over yellow over green. Steady yellow for more
than about a minute means the boot never finished.

**What counts as red.** In the box: a child down or restart-limited; the dongle silent
while the engine runs; storage failing or nearly full; undervoltage or thermal
throttling; the supercapacitor's self-check failed. In the car: a verdict has surfaced
after hysteresis and dwell (D.3), or a sensor the car stopped answering.

**Why every live state pulses.** D-014's reasoning, applied to three colors: a GPIO keeps
its last level after the process driving it dies, so a steady green would freeze into a
false all-clear if the supervisor crashed. If every live state blinks, a frozen light of
any color can only mean "not running". Camden asked for whichever pattern is safer (P11),
and this is.

**The boot state.** The Pi spends roughly half a minute (estimate, `bootbudget`) booting
before any software can drive the light. Yellow is held on from power-up in hardware: a
pull keeps it lit while its GPIO is still an input, and once the supervisor drives the
pin, the pin wins. Camden's choice (R4). The firmware's `gpio=` setting in `config.txt`
might do the same with no parts; confirm on the board before relying on it.

**Open (`ledspec`).** What the light shows after the supervisor takes the GPIOs but before
every child's first heartbeat. Yellow blink is taken (stored energy). Claude's
recommendation, not decided: the supervisor keeps yellow steady until its first
all-healthy check, so the light still reads "not running yet".

**The part.** Whether yellow is its own die or red and green mixed is a part choice (two
GPIOs or three), made in `ledspec` from the LED's datasheet, which also sets the
resistors. Pins are assigned in the schematic (`schpi`).

**Why.** Camden, 2026-10-21 (`docs/REVISION_PLAN.md` §1.1, P10, P11): one light, three
colors, and red means an issue in either the box or the car.

**Cost.** Two or three GPIOs instead of one; the supervisor's blink timer still must never
delay `waitpid` handling; the colors must stay distinguishable through the shell
(`enclspec`). Red merges box and car issues, so the report must say which.

**Alternative rejected.** D-014's single-color patterns (a double-blink for a verdict): a
pattern code the owner has to learn.

---

## C. Storage and data integrity

### C.1 — Append-only, self-framing, CRC'd records 🔒

`D-002/D` · Decided M0 · **By:** Camden + Claude · **Reviewed:** Camden ✅ / Lance ⬜

*Extended 2026-10-21 by D-025 (§C.3), with Camden's explicit yes: records are written in
committed batches, and baselines are saved in A/B checkpoint slots. Every rule below
stays.*

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

### C.2 — Every record carries a provenance tag 🔒

Decided M0 · **By:** Camden + Claude · **Reviewed:** Camden ✅ / Lance ⬜

Every record and every log line carries `src = live | replay | synth`. Present in the
format from day one, before any replay tooling exists.

**Why.** Handout §5: replayed or synthesized data must be labeled everywhere it appears —
logs, report, demo. Making it a mandatory field rather than a convention means it is
structurally impossible to forget. Mislabeling synthesized data as live is the single
integrity failure this course is best equipped to detect, and the worst trade available
anywhere in it.

### C.3 — Write-ahead transactions and A/B checkpoints ⚠️ UNREVIEWED

`D-025` · Decided M2, 2026-10-21 · **By:** Camden (in session, extending 🔒 C.1) + Claude
· **Reviewed:** Camden ⬜ / Lance ⬜
· **Extends:** 🔒 C.1, whose rules all stay: append-only, self-framing, CRC'd records,
each carrying its `src` tag (C.2)

**Decision.**

- **Transactions (`storaged`).** `storaged` drains the ring into batches: a
  `BEGIN(txn_id)` record, the sample records, a `COMMIT(txn_id)` record, then one
  `fsync`. A batch is durable only once its `COMMIT` is on disk.
- **Checkpoints (`analyzed`).** `analyzed` holds the learned per-bin baselines (D-031),
  so it checkpoints them: now and then, a snapshot of the baselines plus the log offset
  and the last committed `txn_id` it covers. There are two alternating slots, A and B,
  each CRC'd and carrying a generation counter, so a checkpoint torn by a power cut never
  destroys the previous good one.
- **Recovery at boot.** `storaged` scans its log forward from the last offset it knows was
  committed, verifying every CRC. The scan stops at the first bad record and truncates the
  torn tail, and a batch with no `COMMIT` is truncated at its `BEGIN` (**undo**).
  `analyzed` then loads the newest valid checkpoint slot and re-applies every committed
  batch after its offset (**redo**), through the same code as live analysis. Both log
  exactly what was recovered and what was lost, as counts and offsets.
- **Every session starts with a boot record** that says whether the previous session
  ended in a clean shutdown or was recovered. That needs a positive marker: a clean
  shutdown ends the log with a shutdown record after its last `COMMIT`, because a crash
  just after a `COMMIT` otherwise looks exactly like a clean end.

**Why undo is only truncation.** Nothing is ever overwritten in place (C.1), so there is
no before-image to restore: undo means discarding an uncommitted tail. Say so plainly at
the defense, and do not claim an undo log the design does not need.

**Extending a locked decision.** C.1 is 🔒. Camden approved this extension explicitly in
the 2026-10-21 session (`docs/REVISION_PLAN.md` §1.1, P4), because it ties the storage
layer to the logging and checkpoint techniques taught in class. C.1 stays 🔒 and gains a
pointer.

**Provisional numbers (estimates).** One `COMMIT` and one `fsync` per batch of about one
second of samples. Records of about 48 bytes at 10 to 20 requests a second come to about
1 KB/s, or about 3.5 MB per running hour. The worst loss from a power cut that beats the
clean shutdown is then about one second of samples. The argument for the batch size is
still written in `docs/DESIGN.md` §4 before the code, as C.1 requires (`fsync`).

**Record format.** The new record kinds (transaction begin and commit, boot, clean
shutdown) and a boot id in the header are settled in `recfmt`. The CRC known-answer test
does not change.

**Cost.** Recovery now spans two processes, so the crash tests (cut the file at every
byte offset, then `expcrash` on the device) must cover both. Checkpoint frequency trades
write load against redo time at boot.

### C.4 — Drive captures are JSONL from a Python tool, VIN hashed ⚠️ UNREVIEWED

`D-033` · Decided M2, 2026-10-26 · **By:** Camden (in session) + Claude
· **Reviewed:** Camden ⬜ / Lance ⬜

**Decision.** Until `obdd` exists, real-car data is collected by `tools/obdlog.py`, run on
Camden's Mac (or the Pi) against the OBDLink LX. It writes one file per session,
`data/drive-<time>-<src>.jsonl`, one JSON object per line: a `header` (adapter,
protocol, battery voltage, VIN hash, PID list, tool and schema version), a `sample` per
reply (`src`, `seq`, `t_mono`, `t_wall`, `pid`, raw hex, decoded value, unit), an `event`
for every non-sample outcome (`no_data`, `timeout`, `link_lost`, `reconnected`, ...),
optional `gps` lines and a `footer`. It polls every supported PID round-robin as fast as
the adapter answers. Writes are append-only and fsync'd every 50 lines or 1 s. The VIN is
shown on screen and stored only as the first 12 hex digits of its SHA-256. A session
against `obdsim` is labeled `synth` on every line, because the simulator's reset banner
says `SIM`.

**Why.** Camden, 2026-10-26: the first drive must log everything the car will answer so
the data can be studied before the pipeline is designed around it. JSONL is readable by
eye and by any analysis tool; the raw hex beside the decoded value means a decoding bug
never costs data; event lines mean nothing is dropped silently (D-012). Camden chose
hashing over storing the VIN.

**A departure from D-012, stated.** D-012 budgets about 10 requests a second on a
rate-limited schedule. This tool keeps D-012's one outstanding request, the core of its
safety argument, but sends the next request as soon as the last is answered, because the
first drives exist to measure what the adapter can do. The product's `obdd` keeps a rate
limit, set from that measurement (`canreq`).

**What this is not.** It is not mechanism D and not the on-disk format of `storaged`
(`recfmt`, D-025). The product stays C (CLAUDE.md §2.4); this is a collection tool in
`tools/` (§2.5). `tools/replay.py` will convert these files into the product's records,
labeled `replay`.

**Cost.** The hash is pseudonymization, not anonymity: a VIN has little entropy, so
anyone holding candidate VINs can test them. Raw logs stay out of git (`data/` is
ignored); only short excerpts are committed as evidence.

### C.5 — The on-disk record format (draft) ⚠️ UNREVIEWED

`D-034` · Drafted M2, 2026-10-26 · **By:** Claude, in Camden's session, without his review
of the details · **Reviewed:** Camden ⬜ / Lance ⬜
· **Finishes:** D-025's open `recfmt` item; keeps every rule of 🔒 C.1 and 🔒 C.2

*Extended 2026-10-26 by D-037 (§B.9): a sample with `mode` 0x00 is from the box's own
sensors; `pid` 0x01 is the DS18B20 probe, value in °C.*

**Decision (draft).** `storaged` writes a log of self-framing records. Every multi-byte
field is little-endian, the Pi's native order. Each record is a fixed header, a body whose
layout depends on its kind, and a CRC32 (the existing `src/store/` CRC, known-answer
tested) over everything before it, magic included.

| Offset | Size | Field | Notes |
| --- | --- | --- | --- |
| 0 | 4 | `magic` | `0x31525743`, the bytes `CWR1`; the resynchronization point (C.1) |
| 4 | 2 | `kind` | 1 sample, 2 begin, 3 commit, 4 boot, 5 shutdown, 6 event |
| 6 | 2 | `body_len` | bytes of body; a reader rejects a length its kind does not allow |
| 8 | 8 | `seq` | per log, never reused, continues across boots |
| 16 | 8 | `mono_ns` | `CLOCK_MONOTONIC` at capture (samples) or at write (others) |
| 24 | 8 | `wall_ns` | Unix time from the real-time clock (D-030) |
| 32 | 1 | `src` | 0 `live`, 1 `replay`, 2 `synth` (C.2); any other value fails the record |
| 33 | 1 | `flags` | bit 0: wall time untrusted this session (D-030) |
| 34 | 2 | reserved | zero |
| 36 | 4 | `boot_id` | increments at every boot, so `mono_ns` is comparable only within one |
| 40 | n | body | per kind, below |
| 40 + n | 4 | `crc32` | over bytes 0 to 40 + n − 1 |

| Kind | Body |
| --- | --- |
| sample | `mode` u8, `pid` u8, `raw_len` u8 (0 to 5), `raw[5]`, `value` f32 (12 bytes): the raw reply bytes are kept beside the decoded value so a decoding bug never costs data. Five bytes is the most a single-frame Mode 01 reply carries (0x64, for one, uses all five); a longer, multi-frame reply is stored as an event holding its hex |
| begin | `txn_id` u64 |
| commit | `txn_id` u64, `records` u32 (samples in the batch), reserved u32 |
| boot | `prev_end` u8 (0 clean shutdown, 1 recovered, 2 first boot), reserved u8[3], `bytes_truncated` u32, `records_lost` u32 |
| shutdown | `reason` u8 (1 source lost, 2 signal, 3 operator), reserved u8[7] |
| event | `code` u16, `len` u16, then `len` bytes of text, at most 200; the codes are open until D-031's open item decides how verdicts and supervisor events reach storage |

**Segments.** The log is a directory of segment files, `log-<first seq>.cwr`, each closed
at about 16 MB (about four hours of driving at the D-025 rate). Recovery scans only the
newest segment; retention deletes whole segments, oldest first, and never edits one.

**Size and rate.** A sample record is 56 bytes. At 10 to 25 requests a second (D-012's
budget up to what the unthrottled tool reaches on `obdsim`) that is 0.56 to 1.4 KB/s, or
2 to 5 MB per running hour, plus a begin and a commit (112 bytes) per
batch. D-025's estimate of 48 bytes becomes 56.

**Why.** D-025 left the record kinds and the boot id to `recfmt`. The fields come straight
from the decisions that need them: `src` (C.2), the boot id and untrusted-time flag
(D-030), begin and commit (D-025), a boot record with a positive clean-shutdown marker
(D-025), and raw bytes beside the value, which proved its worth in the logging tool
(D-033). Fixed widths and a fixed header make the forward scan simple and the
cut-at-every-byte test exhaustive. Segments keep recovery time bounded however long the
box has run.

**Open.** The event codes, and whether verdicts travel as events, wait on D-031's open
item. The value field is f32: enough for every Mode 01 formula, but say so if a PID needs
more.

**Cost.** A format change touches `storaged`, `analyzed` and `tools/replay.py` at once
(D-031). Little-endian is fixed in the file, so the log is not portable to a big-endian
machine without a converter, which nothing here needs.

### C.6 — fsync discipline: one commit a second (draft) ⚠️ UNREVIEWED

`D-035` · Drafted M2, 2026-10-26 · **By:** Claude, in Camden's session, without his review
of the argument · **Reviewed:** Camden ⬜ / Lance ⬜
· **Answers (if signed):** 🔒 C.1's open sub-decision and board item `fsync`

**Decision (draft).** `storaged` closes a batch, writes its `COMMIT` and calls `fsync` when
the batch is **one second old or holds 64 samples**, whichever comes first, and always at
a clean shutdown. A batch is never left open across a ring wake-up that finds no data for
a second: the timer closes it.

**The argument, from the user's requirements.**

- **What a cut costs.** The clean shutdown commits everything (D-024), so loss happens only
  when a cut beats it: an empty bank at a cold start, a pulled cable, a crank reset. Then
  the uncommitted batch is lost: at most one second, about 20 samples, roughly one reading
  of each PID. The signals we watch move over minutes (warm-up, fuel trim against load,
  drift over days, D-012), so one second is invisible to every analysis stage.
- **Why not longer.** Cold-start crank resets are expected, not rare (D-024), and they
  land in the first minutes of a drive, the most informative ones. Ten-second batches
  would lose ten seconds of warm-up at every such reset.
- **Why not every record.** An SD card `fsync` costs milliseconds; twenty a second would
  spend a noticeable share of every second blocked and multiply flash writes about
  twenty times over for no user benefit, since nothing reads a sample within a second anyway.
- **Analysis lag.** `analyzed` reads only durable records (D-031), so it trails capture by
  one batch, about a second. Nothing the owner sees changes faster than that.
- **Flash wear.** About one page plus metadata per second, a few tens of MB per running
  hour including write amplification. Two hours a day for a year is well under the
  endurance of any current SD card. Estimate, not measured.
- **The 64-sample cap** bounds a batch if the rate ever rises (a faster adapter, or the
  raw CAN stretch goal), so the loss stays about a second's worth either way.

**How it is checked.** The evaluation measures `fsync` latency on the Pi (p50, p99) and
reply-to-durable latency (DESIGN.md §6); the crash tests show at most one batch lost per
cut (`expcrash`).

**Cost.** One second of data is lost on every unclean cut, by design, and the report says
how many records each recovery dropped (D-025).

### C.7 — Retention: keep every segment until the disk is 80 % full (draft) ⚠️ UNREVIEWED

`D-041` · Drafted M2, 2026-10-27 · **By:** Claude, in Camden's session
· **Reviewed:** Camden ⬜ / Lance ⬜
· **Answers (if signed):** board item `retention`; the "retention" that handout §8 asks
the storage section to state

**Decision (draft).**

- **Records.** `storaged` keeps every closed segment (D-034) while the filesystem holding
  the log is under 80 % full. Above that, it deletes whole segments, oldest first, until
  it is under 75 %, and logs each deletion (`segment_dropped`, with its first and last
  `seq`). It never deletes the open segment, nor any segment at or after the log offset
  of the newest valid checkpoint (D-025), so the redo after a power cut always has its
  input. Nothing is ever edited in place (C.1).
- **Checkpoints.** The two A/B slots are never deleted; each overwrite replaces the older
  slot only (D-025).
- **Text logs.** Daemons log to standard output, which systemd sends to the journal;
  provisioning caps the journal's size (`SystemMaxUse=`) so logs cannot fill the card.
  The graded soak's logs are copied out of the journal at the end of each run, raw.
- **If even that is not enough** (a full disk with nothing deletable), the failure table's
  `enospc` row applies: hold the durable offset, degrade, show red.

**The arithmetic (estimate).** A 32 GB card with about 24 GB free after the OS, at 80 %,
holds about 19 GB of segments. At 2 to 5 MB per running hour (D-034) that is about 4,000
to 9,600 running hours: five years or more of two-hour days. Retention exists for a
fault, not for normal use.

**Why.** Handout §8 asks what is stored, with what retention. The baseline is the
product: older data is what lets a slow drift be seen against months of normal, so it is
kept as long as space allows. Whole segments keep deletion an unlink, never an edit, so
retention cannot tear a record (C.1). The checkpoint guard keeps the crash story true.

**Cost.** Once the card is full, the oldest raw data goes, and with it the ability to
re-learn a baseline from scratch out of the log; the checkpoints still hold the learned
state. The thresholds are estimates for a card nobody has filled.

---

## D. Intelligence and analysis

### D.1 — Self-trained model; training in Python, inference in C 🔒

`D-004` · Decided M0 · **By:** Camden (confirmed with instructor)
· **Reviewed:** Camden ✅ / Lance ⬜

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

### D.2 — Analysis pipeline shape 🔒

Decided M0 · **By:** Camden + Claude · **Reviewed:** Camden ✅ / Lance ⬜

Operating-point binning (RPM × load × coolant temp) → per-bin running mean/variance →
multivariate residual (Mahalanobis) → robust trend (Theil–Sen) → classifier → state
machine with hysteresis and dwell.

**Why binning comes first.** Comparing a cold idle to a warm cruise is a false-alarm
generator. Every comparison happens inside one bin, against genuinely comparable
conditions. This single step removes more false positives than anything else downstream.

**Why multivariate.** The signal is the *correlation between sensors*, not any single
sensor's value — fuel trim is supposed to track load. A measure of anomaly that
already accounts for normal partnerships, and alarms only when a partnership breaks, is
what makes this something a threshold provably cannot do (handout §3.3).

### D.3 — Hysteresis and dwell before any verdict surfaces 🔒

Decided M0 · **By:** Camden + Claude · **Reviewed:** Camden ✅ / Lance ⬜

No verdict reaches the user until it has persisted. Trip and un-trip levels differ
deliberately.

**Why.** A monitor that cries wolf twice gets unplugged, and an unplugged monitor is worse
than none because someone far away believes it is on duty. **False-positive rate is a
first-class metric in `docs/EVALUATION.md`, not an afterthought.**

### D.4 — The threshold test, as a standing rule 🔒

Decided M0 · **By:** Camden + Claude · **Reviewed:** Camden ✅ / Lance ⬜

Every diagnostic we ship must require at least two cooperating sensors to reach its
conclusion. If a single threshold on a single value could produce the verdict, it is not
intelligence and it does not count. **Write the reason down in `docs/DESIGN.md` when
adding a diagnostic** — this is checked at the defense.

---

## E. Scope, claims and evaluation

### E.1 — A one-car monitor that learns normal and names the fault area ⚠️ UNREVIEWED

`D-021` · Decided M1, 2026-09-23 · **By:** Camden, reopening 🔒 D-003 in session, + Claude
· **Reviewed:** Camden ⬜ / Lance ⬜ · **Supersedes:** D-003 (§G.5)

*Claim reworded 2026-10-21 by D-022 (§A.2), before any review: "this CR-V" became "the
car it is installed in", since two cars are now testbeds.*

**Decision.** carwatch is a rough-draft demo of one idea: plug it into one car, let it learn
that car's normal, and have it say when something acts strange. The claim we defend:

> *carwatch learns the normal of the car it is installed in across that car's Mode 01
> engine sensors, flags behavior outside it, and names the likely fault area. We report
> how often it catches faults we induce — a vacuum leak, a cooling restriction, an
> unplugged sensor — and how often it false-alarms.*

Not "predicts failure." Not "generalizes to any car." Not oil life.

**Fault areas (first draft, refined by the M2 design):** cooling; air/fuel mixture
(vacuum leak, MAF, O2); sensor or electrical (a dead or unplugged sensor, supply voltage);
and "unusual, area unknown" when behavior leaves normal without matching an area. Every
area still needs two cooperating sensors to call (D.4).

**Why.** Camden, 2026-09-23: the build is one car, one Bluetooth OBD2 adapter and one
USB-powered Pi with storage and our model, and the goal is the first attainable step of
the idea, not a finished product. D-003 made oil-pressure degradation diagnostic #1, but
the CR-V most likely publishes no analog oil pressure (D-007), and trending oil life
needed a full oil interval of calendar time. A general monitor uses every sensor the car
does publish, and still meets handout §5: a narrow, measured claim.

**What changed with it.** Oil is one sensor among many if the CR-V publishes it (PID 0x5C
oil temperature), never a headline. Board items `oilq` and `foil` are cut. The analysis
pipeline (§D.2), mechanisms D and E, and the two-sensor rule are unchanged.

### E.2 — Induced ground truth ⚠️ UNREVIEWED

`D-021` (corollary) · **By:** Camden + Claude · **Reviewed:** Camden ⬜ / Lance ⬜
· **Supersedes:** the D-003 corollary (§G.5)

We cannot wait a semester for a failure, so we provoke one per fault area and label the
recordings:

- **Air/fuel** — briefly disconnect a small vacuum line. Reversible, safe, moves LTFT
  within a minute.
- **Cooling** — partially block radiator airflow with cardboard.
- **Sensor** — unplug a sensor. Also demo day's injected fault, so it gets rehearsed.

Ten labeled recordings we made beat ten thousand unlabeled samples we found, because we
know what ours mean — and we will be asked at the defense how we know.

### E.3 — The 48-hour soak is 48 hours of normal use across key cycles ⚠️ UNREVIEWED

`D-023` · Decided M2, 2026-10-21 · **By:** Camden (in session) + Claude
· **Reviewed:** Camden ⬜ / Lance ⬜
· **Answers:** D-006 (§G.7) · **Confirmed:** by the professor, reported by Camden on
2026-10-26 (`profmail`)

**Decision.** The graded soak (handout §3.4) is a **48-hour wall-clock window of the car's
normal use**. Across that window the box:

- boots at every key-on, records, and shuts down at every key-off;
- logs a timestamped start of every session, and a heartbeat at session start, at least
  hourly while running, and at session end, so a drive shorter than an hour still has
  one;
- takes at least one injected fault (the OBD2 dongle unplugged for ten minutes with the
  engine running, or `kill -9` of a child), with detection, degradation and recovery in
  the log;
- ends every session in an orderly state, and the log says whether each session ended in
  a clean shutdown or was recovered at the next boot.

The data source is the live car, `src=live`. The fake OBD2 port (`obdsim`) is a
development tool and never the soak's data source.

**Why.** Professor Pallickara told Camden, verbally, that the 48-hour soak means 48 hours
of regular use: the car starts and stops, the product handles every cycle, shuts down
gracefully at key-off and boots again at the next start. Camden chose the wall-clock
reading (`docs/REVISION_PLAN.md` §1.1, P3). It also dissolves D-006's conflict: the soak
runs on live sensors, as handout §5 requires.

**Confirmed.** The handout's text says "48 continuous hours"; the professor confirmed this
reading (reported by Camden, 2026-10-26). The logs are still built so that a grader
reading the handout literally can follow them: every off period is a deliberate gap
between a logged shutdown and a logged boot.

**Cost.**

- The soak needs a real car driven normally for two days, with margin to do it twice
  (handout §16).
- Proving a 48-hour wall-clock span needs wall-clock time that survives the box being
  off. The Pi has no battery-backed clock, so the box gets one (D-030).
- Heartbeats exist only while the car runs, so the memory plot is a series of sessions,
  not one line.
- Two days of normal driving may hold only a few powered hours, which is thin evidence
  against a slow leak. A continuous bench run against `obdsim`, labeled `synth`, could
  add memory evidence alongside the soak, never in place of it. Open: not decided here.

### E.4 — Committed evaluation targets (draft) ⚠️ UNREVIEWED

`D-036` · Drafted M2, 2026-10-26 · **By:** Claude, in Camden's session, without his review
of the numbers · **Reviewed:** Camden ⬜ / Lance ⬜
· **Answers (if signed):** board item `evalplan`

*Extended 2026-10-26, before any review, by Claude in Camden's session: rows for CPU per
process, the fsync batch-size experiment, every failure-table row, and the probe (D-037),
because handout §9 requires CPU and fault-injection results and mechanism D is measured
by its write batching.*

**Decision (draft).** `docs/DESIGN.md` §6 commits to these targets. Each one is chosen from
what the user needs or what an earlier decision already implies, not from what we expect
to hit.

| Measurement | Target | Where the number comes from |
| --- | --- | --- |
| Reply to durable, p50 / p99 | ≤ 1.2 s / ≤ 2.5 s, idle and loaded | one batch a second (D-035) plus `fsync` time; nothing the owner sees moves faster |
| Sustained request rate | ≥ 10 per second; every PID the model uses read at least every 2 s | D-012's budget of about 10; `obdd` polls the model's input list, not every supported PID, so the 2 s bound holds for up to 20 inputs |
| `kill -9` of each child, detection | ≤ 100 ms, 20 trials per child | `SIGCHLD` arrives at once; slower means the supervisor is blocked |
| `kill -9` of each child, recovery | ≤ 5 s to the first heartbeat, no fd leaked | a recorder silent for longer has missed a reading of every PID |
| Ring drops under contention | 0 overruns, both rings | B.3: refusals are counted, so zero is checkable. Sequence accounting, not a mechanism F claim (D-029) |
| CPU per process | each daemon ≤ 10 % of one core at steady state; all together ≤ 25 %, idle and loaded | handout §9; a recorder that crowds out its own capture under load has failed the user |
| RSS per process over the soak | slope under 100 KB per running hour, per process | flat within measurement noise over D-023's sessions |
| fsync batch size (`expbatch`) | 1 s batches lose ≤ 1 s per cut and write ≤ ⅕ of the bytes of per-record `fsync` | D-035's argument, tested: per record, 1 s and 10 s on the Pi's own SD |
| Every failure-table row | its log event appears when it is injected; a fault outside capture never stops capture | handout §9 asks for each failure mode in the design table |
| Probe unplugged (D-037) | `sensor_absent` within 10 s; engine capture unaffected | one retry period plus one read |
| Detection per fault area (D-021) | catch ≥ 4 of 5 induced events per area | five events per area is what the semester allows |
| False alarms | ≤ 1 surfaced verdict per 10 hours of normal driving | D.3: a monitor that cries wolf gets unplugged |
| Recovery after a cut mid-write | 50 of 50 cuts recover; ≤ 1 batch lost each | D-025 and D-035 promise exactly this |
| Boot to first durable sample | ≤ 45 s | what `bootbudget` must defend; the start of warm-up is lost before it |

**Why commit numbers now.** Handout §8: numbers committed at M2 are twice as credible when
hit later, and instructive when missed. Each target is written so a miss says something
specific.

**Cost.** Some targets will be missed, and the evaluation report must say so plainly.
Five induced events per fault area gives a coarse rate (4 of 5 is 80 %, with a wide
interval); the report states the counts, not just the ratio.

---

## F. Process, team and tooling

### F.1 — Git and pull-request law 🔒

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

### F.2 — No pre-assigned owners; a subsystem belongs to its first substantive commit ⚠️ UNREVIEWED

`D-026` · Decided M2, 2026-10-21 · **By:** Camden (in session, reopening 🔒 F.2) + Claude
· **Reviewed:** Camden ⬜ / Lance ⬜
· **Supersedes:** 🔒 the M0 ownership map (§G.8)

**Decision.** No task, subsystem or document has a pre-assigned owner. Any open line on
the board is for whoever is available. For the ownership map the handout requires (§8,
§10.1), a subsystem belongs to **whoever lands its first substantive commit**, and
`docs/DESIGN.md` §7 records the map as it is realized. Ownership still means first
authorship and answerability at the defense, not exclusivity, and each partner must still
answer one question from across the boundary.

**Reopening a locked decision.** The M0 map was 🔒. Camden reopened it explicitly in the
2026-10-21 session and chose claim-on-first-commit (`docs/REVISION_PLAN.md` §1.1, P2).
Lance has not reviewed it.

**Why.** Camden's call: work goes to whoever is free, and no document assigns work to
either partner, least of all while the two of them have not talked it through. The
handout still needs a map, and the commit history is where a grader can check one.

**What changed with it.**

- `CLAUDE.md` §0 step 3 and §6, and `docs/DESIGN.md` §7.
- `@owner` markers are gone from open board lines; ticked lines keep theirs as the record
  of who did the work. `tools/board_sync.py` only ever adds assignees, so removing a
  marker does not unassign the GitHub issue: a human clears those by hand.
- `OWNER:` lines are gone from the daemon scaffolds. Owner fields inside earlier decision
  entries are history, not assignments.

**Cost.** Less structure: two people can start the same task, so say "I'm on this" in a
comment on the line's issue. The commit balance no longer follows from a map (handout
§10.1: a partner under a quarter of the substantive commits is probed hard at the
defense), so both partners have to watch it.

**Alternative rejected.** Keeping the M0 map: it assigned half the open work to a partner
who had not agreed to the redesign it now serves.

### F.3 — Milestone dates ⚠️ UNREVIEWED

`D-008` · **Owner:** Camden · **Tracked as:** board item `dates` (label `question`)
· **Answered:** 2026-09-21, by Camden · **Reviewed:** Camden ⬜ / Lance ⬜

**Answer.** Camden verified the milestone dates against the syllabus (checked off in
`docs/milestones.md`, 2026-09-21): the milestone dates (now in the `docs/milestones.md`
table, D-020) stand as written, M0 2026-09-20 through M6 2026-12-18. Recorded by Claude
from that check-off; Camden marks it LOCKED in his own commit.

**Why it mattered.** The handout gives weeks, not dates; a one-week error would have moved
the M4 soak start, and M4 already lands on Thanksgiving week.

### F.4 — Markdown is linted with markdownlint, at a 90-column wrap ⚠️ UNREVIEWED

`D-018` · **By:** Claude, in Camden's session 2026-09-23
· **Reviewed:** Camden ⬜ / Lance ⬜

`.markdownlint.jsonc` at the repository root sets the rules the VS Code markdownlint
extension enforces: every default rule on, lines wrapped at **90 columns** (tables, code
and headings exempt), `<details>`, `<summary>`, `<code>`, `<sub>` and `<br>` allowed as
inline HTML, and tables in the compact `| a | b |` / `| --- | --- |` style.
`.markdownlintignore` excludes `docs/handout/`, which is a transcription of the
instructor's PDF.

Spelling is **American English**, checked by the VS Code Code Spell Checker (cSpell).
`cspell.json` holds the project dictionary: our jargon, command names and board slugs.
Add a word there when it is a real term; fix it when it is a typo.

**Why 90, not the default 80.** The docs were already wrapped at about 90: at 80 the repo
raised 1,184 line-length warnings, and at 90 it raised 24. Rewrapping everything to 80
would bury every file's history in reflow for no reader benefit.

**Why a config at all.** Without one, each editor shows over 1,500 warnings, and a real
problem (a broken heading tree, a list that does not render) is invisible in the noise.

### F.5 — The milestone checklist is the board; board.toml is retired ⚠️ UNREVIEWED

`D-020` · Decided M1, 2026-09-23 · **By:** Camden (in session) + Claude
· **Reviewed:** Camden ⬜ / Lance ⬜

**Decision.** `docs/milestones.md` is the single source of truth for all work. Every
checkbox under a `## M<n>` heading is one GitHub issue, linked by a hidden `<!-- #n -->`
tag that `tools/board_sync.py` writes. Tick a box and sync: the issue closes and the card
moves to Done. Add a line and sync: a new issue appears. Bold lines are the milestone's
exit criteria; plain lines are the tasks under them. `docs/BOARD/board.toml` and
`board.lock.json` are retired; every one of their 99 issues became a line, and nothing
was closed or deleted to get there.

**Why.** Camden's call: 99 items with bodies, dependencies and labels in a TOML file was
more board than two people can follow, and it duplicated the exit criteria kept in
`milestones.md`. A checklist is what we already read; making it the board removes the
second copy.

**What changed with it.**

- Issue **bodies** are no longer generated. GitHub owns them; the old text stays there.
- **Dependencies** (`blocked_by`) and the auto-applied `blocked` label are gone. Order
  within a milestone is the plan.
- The `--close-questions` flag is gone. Ticking a box is the human act, so **Claude
  never ticks a question, decision or hardware line** (CLAUDE.md §7.11).
- Milestone due dates are edited in the `milestones.md` table itself.

**Cost.** Less structure: no enforced dependency order, and a closed issue explains
itself only through its body and comments. If the board grows unwieldy again, the
fix is fewer lines, not more tooling.

### F.6 — Python tools are tested in `make test`, in a venv built on first use ⚠️ UNREVIEWED

`D-032` · Decided M2, 2026-10-26 · **By:** Camden (in session) + Claude
· **Reviewed:** Camden ⬜ / Lance ⬜

**Decision.** `make toolcheck`, part of `make test`, runs the tests in `tests/tools/` with
stdlib `unittest`. If `.venv/` does not exist, or a requirements file is newer than its
last install, it creates or refreshes it with the `PY` interpreter from
`tools/requirements-analysis.txt` (pyserial and matplotlib, pinned), so the plotting tool
is tested too. The GPS dependency lives in its own file and is never needed to log a
drive; logging itself needs only pyserial.

**Why.** The drive logger, the PID survey and the simulator are on the path to the M2
exit criteria `firstcap` and `pidforester`, and CLAUDE.md §2.8 forbids shipping behavior
without a test. A Python test that is not in `make test` would never run. A venv built on
demand keeps a fresh clone green without a manual setup step. The standard library
`unittest` avoids a second test dependency.

**Cost.** The first `make test` on a new machine needs the network once, to install
pyserial and matplotlib (with numpy). That is a development step; the product, which has
no Python, never does. Python stays in `tools/` and `tests/tools/` (CLAUDE.md §2.5).

---

## G. Superseded

When a decision is replaced, it moves here with a pointer to its replacement. Never delete
one: the reasoning is evidence, and "what M2 got wrong" is a graded section of
`docs/DESIGN.md`.

### G.1 — (was A.1) CAN reaches the Pi via MCP2515 on SPI, not Bluetooth 🗑

`D-001` · Decided M0 · **By:** Camden + Claude · **Reviewed:** Camden ✅ / Lance ⬜
· **Superseded 2026-09-21 by D-015 (now §G.4)** — raw CAN became a stretch goal.

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
mechanisms survive. D-019 then moved that adapter to Bluetooth, the option this entry
rejected, at no further mechanism cost.

### G.2 — (was A.2) Testbeds: Subaru Outback and Lance's Honda 🗑

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

### G.3 — (was B.1) Mechanism commitments: B, D, E, F 🗑

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

### G.4 — (was A.1) The Pi reads OBD2 through a USB adapter; raw CAN is a stretch goal 🗑

`D-015` · Decided M1, 2026-09-21 · **By:** Camden + Claude
· **Reviewed:** Camden ✅ / Lance ⬜
· **Supersedes:** D-001 (§G.1)
· **Superseded 2026-09-23 by D-019 (§A.1)**: the same protocol over Bluetooth Classic,
with this USB adapter kept as the fallback.

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

### G.5 — (was E.1) Three induced-and-measured diagnostics, and no more 🗑

`D-003` · Decided M0 · **By:** Camden + Claude · **Reviewed:** Camden ✅ / Lance ⬜
· **Superseded 2026-09-23 by D-021 (§E.1)**: reopened by Camden in session; oil life
and the fixed three diagnostics gave way to a general monitor with fault areas.

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

#### (was E.2) Induced ground truth, D-003 corollary 🗑

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

### G.6 — (was A.2) One testbed: Lance's 2015 Honda CR-V EX-L 🗑

`D-016` · Decided M1, 2026-09-21 · **By:** Camden + Claude
· **Reviewed:** Camden ✅ / Lance ⬜
· **Supersedes:** D-005 (§G.2)
· **Superseded 2026-10-21 by D-022 (§A.2)**: Camden's Forester joined as a second
testbed, and neither car is required of anyone.

**Decision.** The CR-V is the only testbed. The Subaru Outback is dropped.

**Why.** Camden's call, 2026-09-21. D-005 itself named the second vehicle as the first
thing to cut when the schedule slipped, and the CAN detour (PROMPTLOG E-02) spent that
slack. One car halves the survey, baseline and ground-truth work, and the baseline window
is the one thing that cannot be compressed by working harder in Week 14.

**Cost.**

- The two-car contrast was the old thesis's strongest demonstration, and it is gone. The
  thesis is now argued within one car: what it does against its own learned normal
  (D-021).
- Every live capture, induced fault and soak on real data runs on Lance's car and Lance's
  schedule; Camden owns the capture code but not the car.

### G.7 — (was E.3) 48-hour soak: live sensors vs. a fake OBD2 port 🗑

`D-006` · **Owner:** Camden · **Tracked as:** board item `soakq` (label `decision`)
· **Must close before M4** · Blocks nothing before M3
· **Answered 2026-10-21 by D-023 (§E.3)**: the soak is 48 hours of normal use across key
cycles, on the live car, so none of the options below is needed.

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

### G.8 — (was F.2) Ownership map 🗑

Decided M0 · **By:** Camden · **Reviewed:** Camden ✅ / Lance ⬜
· **Superseded 2026-10-21 by D-026 (§F.2)**: reopened by Camden in session; no
pre-assigned owners, and a subsystem belongs to its first substantive commit.

| Owner | Subsystems |
| --- | --- |
| **Camden** | `src/obd/`, `src/ipc/` |
| **Lance** | `src/store/`, `src/analysis/`, `tools/train/` |
| **Shared** | `src/supervisor/`, `src/interface/`, `src/common/`, build, docs |

Ownership means **first authorship and answerability at the defense**, not exclusivity.
Each partner must still be able to answer one question from across the boundary — demo day
guarantees one.

### G.9 — (was A.6) Plug-in form: OBD2 port for data, car USB-C for power 🗑

`D-011` · Decided M1, 2026-09-21 · **By:** Camden + Claude
· **Reviewed:** Camden ✅ / Lance ⬜
· **Superseded 2026-10-21 by D-024 (§A.6)**: the Pi moved into one box with one USB-C
input and a seconds-scale supercapacitor, so key-off is a clean shutdown, not a cut.

*Data half revised 2026-09-21 by D-015, before any review: the MCP2515 and Y-splitter
became a USB OBD2 adapter. Revised again 2026-09-23 by D-019: the adapter talks to the Pi
over Bluetooth Classic, so no data cable runs to the Pi.*

**Decision.** The device is a box anyone could install without tools:

- **Data:** the Bluetooth OBD2 adapter (D-019) plugs into the OBD2 port; the Pi reads it
  over RFCOMM. No data cable.
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
- The product is "one box, one power cable, plus the dongle in the port".

### G.10 — (was B.5) One warning light, driven by the supervisor 🗑

`D-014` · Decided M1, 2026-09-21 · **By:** Camden + Claude
· **Reviewed:** Camden ✅ / Lance ⬜
· **Superseded 2026-10-21 by D-027 (§B.5)**: one tri-color light; the blink-only rule is
kept.

**Decision.** One LED on a GPIO (proposed GPIO17, header pin 11, through 330 Ω), written
**only by the supervisor**. Every state in which the device is working is a *blink
pattern*:

| Light | Meaning | Priority |
| --- | --- | --- |
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
either kind can only mean *not running*. This is mechanism E's principle — a recorder that
dies silently has harmed its user — applied to the one output a driver actually sees.

**Why the supervisor, not `analyzed`.** The supervisor already knows every child's
liveness (mechanism E), so it can show "degraded" when `analyzed` itself has died; a light
owned by `analyzed` would show a stale verdict instead. Verdicts reach the supervisor over
the existing UDS. One writer, no GPIO contention.

**Cost.** One more thing the supervisor does, and one more thing it must not block on: the
blink timer must never delay `waitpid` handling. Color, brightness and placement are open
(M4 enclosure).

---

## Adding a decision

Append it to the right category with the next free `D-###` for citation, and fill every
field. If Claude made it without a human present, it is ⚠️ **UNREVIEWED** and it goes in the
Review Queue at the top — **in the same commit that implements it**, never a later one.

**Closing an open question.** When a human answers a `decision` or `question` issue, the
answer moves here as a 🔒 LOCKED entry citing the issue number, and the issue closes. One
commit does both — resolving one question is one responsibility, however many lines it
touches across this file (`CLAUDE.md` §7.3, documentation rules).
