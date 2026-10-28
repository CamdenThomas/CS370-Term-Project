# Supported-PID survey

**Sets the model's input list (D-021, D-022). Run it on each car's first drive.**

Method: `tools/pidscan.py`, as `docs/hardware/first-drive.md` describes (Mac or Pi). It
sends `ATZ`, `ATE0`, `ATSP0`, then the supported-PID bitmaps `0100`, `0120`, `0140`, ...
(each a 32-bit map of which PIDs in the next block the ECU answers), takes one reading of
every supported PID, and writes a markdown block that `scripts/capture.sh` commits beside
the drive under `captures/<car>/<time>/pidscan.md` (D-046). By hand, the same commands
work in `screen /dev/obd`.

## The cars

| | 2008 Subaru Forester (Camden) | 2015 Honda CR-V EX-L (Lance) |
| --- | --- | --- |
| Surveyed | 2026-10-28, parked at a warm idle, from a Mac | not yet (`pidhonda`) |
| Protocol (`ATDP`) | `ISO 15765-4 (CAN 11/500)` | |
| Battery (`ATRV`) | 14.0 V, engine running | |
| VIN | hash `16d15766b381`; model year 2008 from the VIN | |
| Supported Mode 01 PIDs | 43 | |
| Raw replies | `captures/forester/20261028T220659.957/pidscan.md` | |

The VIN is stored only as its hash (D-033); whether the raw VIN is also recorded is open
(`vinsurvey`).

## The sensors the design uses

A check mark is a live reading from the survey; the value is what the car reported at the
time, for scale, not a baseline.

| PID | Signal | Forester | CR-V | Notes |
| --- | --- | --- | --- | --- |
| 0x01 | Monitor status: MIL and DTC count | ✓ `00 07 E5 00`, MIL off | | the failure table's `mil_on` |
| 0x0C | RPM | ✓ 649 rpm | | bin axis; polled at 2.6 Hz |
| 0x04 | Calculated load | ✓ 27 % | | bin axis; polled at 2.6 Hz |
| 0x05 | Coolant temp | ✓ 71 °C | | bin axis; cooling; cold-soak check |
| 0x0F | Intake air temp | ✓ 28 °C | | cold-soak check |
| 0x46 | Ambient air temp | ✓ 13 °C | | cooling; cold-soak check |
| 0x0D | Vehicle speed | ✓ 0 km/h | | cooling |
| 0x0B | MAP | ✓ 26 kPa | | air/fuel |
| 0x10 | MAF | ✓ 2.5 g/s | | air/fuel; both MAP and MAF are published |
| 0x11 | Throttle position | ✓ 15 % | | |
| 0x06 | Short-term fuel trim B1 | ✓ −1.6 % | | air/fuel |
| 0x07 | Long-term fuel trim B1 | ✓ 0.8 % | | air/fuel |
| 0x14–0x1B | O2 sensor voltages | 0x15 only, 0.84 V | | air/fuel; the upstream sensor is wideband, 0x24 |
| 0x24 | O2 sensor 1 lambda (wideband) | ✓ 1.00 | | air/fuel |
| 0x42 | Control module voltage | ✓ 14.0 V | | sensor/electrical |
| 0x5C | Engine oil temp | absent | | |
| — | Oil pressure | absent | | Not a standard PID. Nothing depends on it (D-021). |
| 0x2F | Fuel level | ✓ 34 % | | |
| 0x31 | Distance since codes cleared | ✓ 8309 km | | odometer proxy |
| 0x33 | Barometric pressure | ✓ 84 kPa | | matches Fort Collins' altitude; an altitude input for the bins |

The Forester also publishes 0x03, 0x0E, 0x13, 0x1C, 0x1F, 0x21, 0x2C, 0x2D, 0x2E, 0x30,
0x32, 0x34, 0x3C, 0x41, 0x43, 0x44, 0x45, 0x47, 0x49, 0x4A, 0x4C, 0x4D, 0x4E, 0x51 and
0x5A; the logger records all of them, decoded where `tools/obdlib.py` knows the formula.

## After the survey

Every supported PID becomes an input to the model for that car. Manufacturer (Mode 22)
PIDs are a stretch: try them only if a fault area has too few sensors to call.
