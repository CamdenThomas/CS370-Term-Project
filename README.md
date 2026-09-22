# carwatch

An always-on in-vehicle diagnostic recorder. It reads a car's own sensors off the CAN
bus, learns what *that specific car* looks like at each operating point, and reports
developing faults that a mileage sticker and a check-engine light both miss.

CS 370 Operating Systems term project — Camden Thomas & Lance Baron, Colorado State
University.

> **Status: M0 scaffolding.** Structure and contracts are in place; subsystems are
> stubs. See `docs/milestones.md`.

## What it does

| Diagnostic | Sensors that cooperate | Why a threshold can't do it |
|---|---|---|
| Oil pressure degradation across oil life | oil pressure + RPM + coolant temp | Oil pressure is a function of RPM and oil temperature. The signal is the *residual* after normalizing for both, trended over weeks. |
| Cooling system anomaly | coolant temp + ambient/intake temp + engine load | A short-cycling thermostat has a period and amplitude. A single temperature reading has neither. |
| Mixture drift (vacuum leak / failing O2 / dirty MAF) | short+long term fuel trim + MAP + RPM | Trim values are only meaningful relative to load; the diagnosis is which way trims move *as load changes*. |

## Hardware

See `docs/hardware/BOM.md` for the parts list and `docs/hardware/wiring.md` for the
pinout and the 3.3 V safety notes. Summary: Raspberry Pi 4, MCP2515 + TJA1050 CAN
module on SPI0 with `INT` on a GPIO, OBD2 pass-through Y-splitter (CAN_H pin 6, CAN_L
pin 14, signal ground pin 5). Power comes from the car's USB-C port or a 12 V-socket
USB-C adapter — no wiring into the car, nothing cut (decision D-011). One LED on
GPIO17 is the warning light: slow blink = recording, double-blink = a finding, fast
blink = degraded, **steady on or off = not running** (decision D-014).

## Quick start (clean Raspberry Pi OS)

```sh
git clone <repo> && cd CS370-Term-Project
./scripts/provision_pi.sh      # packages, SPI overlay, user, service units
make                           # -Wall -Wextra -Werror clean
make test
sudo ./scripts/install.sh      # installs supervisor + daemons
obdctl status
```

Full TA-facing walkthrough is filled in at M4. **If the TA can't boot it, the TA can't
grade it** — this file is graded.

## Query interface

```
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

```
src/         systems core, C17, -Wall -Wextra -Werror clean
  common/    logging, time, config, error paths
  can/       MCP2515 SPI + interrupt RX path        [mechanism B]
  capture/   optional physical sensors (MPU-6050, DS18B20)
  ipc/       SPSC no-drop ring, UDS protocol        [mechanisms E, F]
  store/     append-only crash-consistent log       [mechanism D]
  analysis/  features, baselines, model, state machine
  supervisor/ watchdog, restart policy              [mechanism E]
  interface/ obdctl query CLI + read-only phone status page (D-013)
drivers/     out-of-tree kernel module (stretch, mechanism A)
tools/       PYTHON ALLOWED HERE ONLY — replay harness, training, plots,
             board_sync.py (reconciles docs/board.toml onto the GitHub board)
tests/       unit tests and fixtures
fixtures/    labeled captures, including induced faults
soak/        soak runner, heartbeat logs, fault injection
docs/        PROBLEM.md, DESIGN.md, EVALUATION.md (the three graded documents)
             board.toml (every task/question/decision) + BOARD.md (how it works)
             DECISIONS.md (audit log), PLAN.md (why the plan is shaped this way),
             milestones.md, handout/ (the rubric as markdown), hardware/
partners/    per-partner PROMPTLOG, REFLECTION, raw .jsonl transcripts
```

## The boundary

This product contains no LLM client, no cloud inference, and no third-party pretrained
model. The classifier weights in `models/` were trained by us from data we collected;
the training code is in `tools/train/`. The only network activity in the shipped tree
is serving the read-only status page on the device's own Wi-Fi access point, which has
no upstream connection. See `CLAUDE.md` §2.
