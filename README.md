# carwatch

A small in-car box that watches one car's engine. It reads the car's own sensors through
a Bluetooth OBD2 dongle, learns what *that specific car* looks like at each operating
point, and flags behavior that leaves its normal, naming the likely fault area, before a
check-engine light would. A rough first draft of the idea: one box, one dongle, one car at
a time.

CS 370 Operating Systems term project — Camden Thomas & Lance Baron, Colorado State
University.

> **Status: M2.** The design is being written; subsystems are stubs. Every
> document is indexed in [`docs/README.md`](docs/README.md); progress is in
> [`docs/milestones.md`](docs/milestones.md).

## What it does

It learns how all the engine sensors normally move *together* at each operating point, and
flags a broken partnership that no single threshold could see (decision D-021). When it
flags something, it names the likely fault area:

| Fault area | Sensors that cooperate | Why a threshold can't do it |
| --- | --- | --- |
| Cooling | coolant temp + intake temp + engine load | A lazy or short-cycling thermostat shows up in *how* temperature moves with load, not in any single reading. |
| Air/fuel mixture (vacuum leak, MAF, O2) | short+long term fuel trim + MAP + RPM | Trim is only meaningful relative to load; the fault is which way trims move *as load changes*. |
| Sensor or electrical | any sensor against its usual partners | A dead or unplugged sensor reads a plausible value that no longer agrees with the others. |
| Unusual, area unknown | all of them | Behavior outside the learned normal that matches no known area is still worth a flag. |

We measure how often it catches faults we induce (a vacuum leak, a cooling restriction,
an unplugged sensor) and how often it false-alarms, on our two testbed cars: a 2008 Subaru
Forester and a 2015 Honda CR-V (decision D-022). Every result is labeled with the car it
came from.

## Hardware

See `docs/hardware/BOM.md` for the parts list and `docs/hardware/wiring.md` for the
connections and the safety notes. Summary: one small box with one USB-C port and one
light, plus a Bluetooth Classic OBD2 dongle (OBDLink LX) in the car's OBD2 port. Inside
the box are a Raspberry Pi 4, its microSD card, a real-time clock (decision D-030) and a
power block with a small supercapacitor. Plug the dongle into the OBD2 port and the box
into any USB-C charger in the car; nothing is cut or wired into the car (decision D-024).
A small temperature probe on a lead from the box reads cabin air: the box's own sensor,
which cross-checks the engine's temperature sensors after a long soak (decision D-037).
The box runs only while the car does: at key-off it shuts itself down cleanly on its
stored energy and stays dark until the next start. The Pi pairs with the dongle and reads
it over RFCOMM as `/dev/obd` (decision D-019).

**The light** (decision D-027). Every live state pulses, so a light that is stuck can only
mean the box is not running:

| Light | Meaning |
| --- | --- |
| green, slow pulse | ready: recording, all systems go |
| yellow, blinking | running on stored energy: riding a supply dip, or shutting down |
| red, fast blink | an issue in the box or in the car; the phone report says which |
| steady yellow | booting |
| any other steady light, or dark while the car runs | **not running** |
| dark while the car is off | normal: the box is off |

## Quick start (clean Raspberry Pi OS)

```sh
git clone <repo> && cd CS370-Term-Project
./scripts/provision_pi.sh      # packages, adapter pairing + /dev/obd, service units
make                           # -Wall -Wextra -Werror clean
make test
sudo ./scripts/install.sh      # installs supervisor + daemons
obdctl status
```

Full TA-facing walkthrough is filled in at M4. **If the TA can't boot it, the TA can't
grade it** — this file is graded.

## Query interface

```sh
obdctl status                        # liveness, per-process RSS, event counts
obdctl verdicts [--since <when>]     # current and historical diagnoses
obdctl series <pid> --from --to      # raw or binned history
obdctl baseline <pid>                # the learned model for an operating-point bin
obdctl faults                        # detection/degradation/recovery events
```

**From a phone:** join the box's own Wi-Fi network (no internet — it has none) and open
its status page. The page shows status, current verdicts and trends, and a **Download
report** button returns an HTML report plus a CSV of the same data. It is read-only, and
anything deeper is `obdctl` (decisions D-013, D-028). The box is off whenever the car is,
so to read it at home, plug it into any USB-C charger there.

## Repository layout

```text
src/                systems core, C17, -Wall -Wextra -Werror clean
  obd/              capture daemon: Mode 01 requests over the OBD2 adapter tty
  ambient/          probe daemon: the box's DS18B20 cabin probe (D-037)
  ipc/              SPSC ring + Unix-socket protocol between processes   [mechanism E]
  store/            append-only crash-consistent log                     [mechanism D]
  analysis/         binning, baselines, residuals, trends, model, state machine
  supervisor/       spawn, restart with backoff, heartbeats, the light  [mechanism E]
  interface/        obdctl CLI + read-only phone status page and report (D-013, D-028)
  common/           logging and shared helpers
include/            headers shared between subsystems (common, ipc, store)
tests/              unit tests (make test)
tools/              Python only here: board sync, PID scan, replay, training
scripts/            provision a clean Pi; install the service
soak/               48-hour soak runner and fault injection
docs/               everything written — start at docs/README.md
electricalDrawing/  KiCad schematic
partners/           each partner's PROMPTLOG, REFLECTION, raw transcripts
```

Created when their first file lands, not before: `fixtures/` (labeled captures),
`models/` (our weights), `build/` and `soak/logs/` (both git-ignored).

## The boundary

This product contains no LLM client, no cloud inference, and no third-party pretrained
model. The classifier weights in `models/` were trained by us from data we collected;
the training code is in `tools/train/`. The only network activity in the shipped tree
is serving the read-only status page and its report on the box's own Wi-Fi access point,
which has no upstream connection. See `CLAUDE.md` §2.
