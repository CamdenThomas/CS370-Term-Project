# carwatch

A small in-car box that watches one car's engine. It reads the car's own sensors through
a Bluetooth OBD2 dongle, learns what *that specific car* looks like at each operating
point, and flags behavior that leaves its normal, naming the likely fault area, before a
check-engine light would. A rough first draft of the idea: one box, one dongle, one car at
a time.

CS 370 Operating Systems term project — Camden Thomas & Lance Baron, Colorado State
University.

> **Status: M2 submitted 2026-10-28.** The design is written and the first live capture
> from the Forester is in `captures/forester/`. The shared-memory ring, `storaged` and
> the phone page server `statusd` exist with tests; `obdd`, `analyzed` and the supervisor
> are still stubs (M3). Every document is indexed in [`docs/README.md`](docs/README.md);
> progress is in [`docs/milestones.md`](docs/milestones.md).

## What it does

It learns how all the engine sensors normally move *together* at each operating point, and
flags a broken partnership that no single threshold could see (decision D-021). When it
flags something, it names the likely fault area:

| Fault area | Sensors that cooperate | Why a threshold can't do it |
| --- | --- | --- |
| Cooling | coolant temp + intake temp + ambient air temp + load + speed | A lazy or short-cycling thermostat shows up in *how* temperature moves with load, not in any single reading. |
| Air/fuel mixture (vacuum leak, MAF, O2) | short+long term fuel trim + MAP or MAF + RPM + O2 | Trim is only meaningful relative to load; the fault is which way trims move *as load changes*. |
| Sensor or electrical | any sensor against its usual partners; after a long soak, coolant + intake temp + ambient | A dead or unplugged sensor reads a plausible value that no longer agrees with the others. |
| Unusual, area unknown | all of them | Behavior outside the learned normal that matches no known area is still worth a flag. |

We measure how often it catches faults we induce (a vacuum leak, a cooling restriction,
an unplugged sensor) and how often it false-alarms, on our two testbed cars: a 2008 Subaru
Forester and a 2015 Honda CR-V (decision D-022). Every result is labeled with the car it
came from.

## Hardware

See `docs/hardware/BOM.md` for the parts list and `docs/hardware/wiring.md` for the
connections. Summary: a Raspberry Pi 4 with its microSD card, a clock module and one
light, in a small box with one USB-C port, plus a Bluetooth Classic OBD2 dongle (OBDLink
LX) in the car's OBD2 port. Plug the dongle into the OBD2 port and the box into any USB-C
charger in the car; nothing is cut or wired into the car. The box runs only while the car
does: the charger dies at key-off and so does the box, and the storage layer (mechanism D)
recovers the log at the next boot. There is no battery, supercapacitor, cooling or sensor
of our own: the scope was cut to the engine's own sensors on 2026-10-28, with the TA's
agreement. The Pi pairs with the dongle and reads it over RFCOMM as `/dev/obd` (decision
D-019). The Forester publishes 43 Mode 01 sensors over CAN, read at about 21 readings a
second from a Mac (`captures/forester/`); the Pi's own rate is still to be measured.

**The light** (decision D-027). Every live state pulses, so a light that is stuck can only
mean the box is not running:

| Light | Meaning |
| --- | --- |
| green, slow pulse | ready: recording, all systems go |
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
obdctl status                                   # liveness, RSS, CPU, counts, sensors
obdctl verdicts [--since WHEN]                  # current and historical diagnoses
obdctl series PID --from WHEN --to WHEN [--binned]   # raw or per-bin history
obdctl baseline PID [--bin RPM,LOAD,COOLANT]    # the learned normal, every bin or one
obdctl faults [--since WHEN]                    # detection/degradation/recovery events
```

`WHEN` is ISO 8601 or relative, such as `-2h`. The protocol underneath is decision D-043.

**From a phone:** join the box's own Wi-Fi network (no internet — it has none) and open
its status page. The page shows status, current verdicts and trends, and a **Download
report** button returns an HTML report plus a CSV of the same data. It is read-only, and
anything deeper is `obdctl` (decisions D-013, D-028). The box is off whenever the car is,
so to read it at home, plug it into any USB-C charger there.

## Repository layout

```text
src/                systems core, C17, -Wall -Wextra -Werror clean
  obd/              capture daemon: Mode 01 requests over the OBD2 adapter tty
  ipc/              SPSC ring + Unix-socket protocol between processes   [mechanism E]
  store/            append-only crash-consistent log                     [mechanism D]
  analysis/         binning, baselines, residuals, trends, fault-area rule, state machine
  supervisor/       spawn, restart with backoff, heartbeats, the light  [mechanism E]
  interface/        obdctl CLI + read-only phone status page and report (D-013, D-028)
  common/           logging and shared helpers
include/            headers shared between subsystems (common, ipc, store)
tests/              C unit tests; tests/tools/ holds the Python tool tests (make test)
tools/              Python only here: board sync, PID survey, drive logger, simulator,
                    plots, replay
scripts/            provision a clean Pi; install the service; capture.sh records a car
soak/               48-hour soak runner and fault injection
captures/           every live capture from a real car, committed whole, by car and time
docs/               everything written — start at docs/README.md
partners/           each partner's PROMPTLOG, REFLECTION, raw transcripts
```

`fixtures/` holds labeled captures for tests (one `synth` drive so far). `build/` and
`soak/logs/` are created when their first file lands, and both are git-ignored.

## The boundary

This product contains no LLM client, no cloud inference, no third-party pretrained model
and no trained model of any kind: the intelligence is statistics we wrote in C
(`src/analysis/`), shown as trends on the phone page. The only network activity in the
shipped tree is serving the read-only status page and its report on the box's own Wi-Fi
access point, which has no upstream connection. See `CLAUDE.md` §2.
