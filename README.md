# carwatch

An always-on in-vehicle diagnostic recorder. It reads a car's own sensors through the
OBD2 port, learns what *that specific car* looks like at each operating point, and reports
developing faults that a mileage sticker and a check-engine light both miss.

CS 370 Operating Systems term project — Camden Thomas & Lance Baron, Colorado State
University.

> **Status: M1.** Structure and contracts are in place; subsystems are stubs. Every
> document is indexed in [`docs/README.md`](docs/README.md); progress is in
> [`docs/milestones.md`](docs/milestones.md).

## What it does

| Diagnostic | Sensors that cooperate | Why a threshold can't do it |
| --- | --- | --- |
| Oil pressure degradation across oil life | oil pressure + RPM + coolant temp | Oil pressure is a function of RPM and oil temperature. The signal is the *residual* after normalizing for both, trended over weeks. |
| Cooling system anomaly | coolant temp + ambient/intake temp + engine load | A short-cycling thermostat has a period and amplitude. A single temperature reading has neither. |
| Mixture drift (vacuum leak / failing O2 / dirty MAF) | short+long term fuel trim + MAP + RPM | Trim values are only meaningful relative to load; the diagnosis is which way trims move *as load changes*. |

## Hardware

See `docs/hardware/BOM.md` for the parts list and `docs/hardware/wiring.md` for the
connections and the safety notes. Summary: a Raspberry Pi 4 and a USB OBD2 adapter in
the car's OBD2 port, appearing on the Pi as `/dev/obd` (decision D-015). Power comes
from the car's USB-C port or a 12 V-socket USB-C adapter — no wiring into the car,
nothing cut (decision D-011). One LED on GPIO17 is the warning light: slow blink =
recording, double-blink = a finding, fast blink = degraded, **steady on or off = not
running** (decision D-014).

## Quick start (clean Raspberry Pi OS)

```sh
git clone <repo> && cd CS370-Term-Project
./scripts/provision_pi.sh      # packages, /dev/obd udev rule, service units
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

**From a phone:** join the device's own Wi-Fi network (no internet — it has none) and open
its status page. The page is read-only and shows exactly `status` and `verdicts`; anything
deeper is `obdctl` (decision D-013).

## Repository layout

```text
src/                systems core, C17, -Wall -Wextra -Werror clean
  obd/              capture daemon: Mode 01 requests over the OBD2 adapter tty
  ipc/              SPSC ring + Unix-socket protocol between processes   [mechanism E]
  store/            append-only crash-consistent log                     [mechanism D]
  analysis/         binning, baselines, residuals, trends, model, state machine
  supervisor/       spawn, restart with backoff, heartbeats, warning light [mechanism E]
  interface/        obdctl CLI + read-only phone status page (D-013)
  common/           logging and shared helpers
include/            headers shared between subsystems (common, ipc, store)
tests/              unit tests (make test)
tools/              Python only here: board sync, PID scan, replay, training
scripts/            provision a clean Pi; install the service
soak/               48-hour soak runner and fault injection
docs/               everything written — start at docs/README.md
electricalDrawing/  KiCad schematic (Lance)
partners/           each partner's PROMPTLOG, REFLECTION, raw transcripts
```

Created when their first file lands, not before: `fixtures/` (labeled captures),
`models/` (our weights), `build/` and `soak/logs/` (both git-ignored).

## The boundary

This product contains no LLM client, no cloud inference, and no third-party pretrained
model. The classifier weights in `models/` were trained by us from data we collected;
the training code is in `tools/train/`. The only network activity in the shipped tree
is serving the read-only status page on the device's own Wi-Fi access point, which has
no upstream connection. See `CLAUDE.md` §2.
