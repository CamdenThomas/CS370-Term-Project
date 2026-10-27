# Supported-PID survey

**Sets the model's input list (D-021, D-022). Run it on each car's first drive.**

Method: `tools/pidscan.py`, as `docs/hardware/first-drive.md` describes (Mac or Pi). It
sends `ATZ`, `ATE0`, `ATSP0`, then the supported-PID bitmaps `0100`, `0120`, `0140`, ...
(each a 32-bit map of which PIDs in the next block the ECU answers), takes one reading of
every supported PID, and writes a markdown block for this file. Paste that block and the
raw replies below the table, dated. By hand, the same commands work in `screen /dev/obd`.

## What we specifically need to know

Vehicles (D-022): Camden's **2008 Subaru Forester** (non-turbo; OBD protocol not yet
verified, expected CAN) and Lance's **2015 Honda CR-V EX-L** (D-007; engine and
transmission still to be confirmed from the VIN). For each car, also record the protocol
the adapter negotiated (`ATDP`) and the VIN's hash, which is what `pidscan` stores (D-033);
whether this file also records the raw VIN is open (`vinsurvey`).

| PID | Signal | Forester | CR-V | Notes |
| --- | --- | --- | --- | --- |
| 0x01 | Monitor status: MIL and DTC count | | | the failure table's `mil_on` |
| 0x0C | RPM | | | bin axis |
| 0x04 | Calculated load | | | bin axis |
| 0x05 | Coolant temp | | | bin axis; cooling; cold-soak check |
| 0x0F | Intake air temp | | | cold-soak check |
| 0x46 | Ambient air temp | | | cooling; else the DS18B20 (D-037) |
| 0x0D | Vehicle speed | | | cooling |
| 0x0B | MAP | | | air/fuel |
| 0x10 | MAF | | | air/fuel, if MAP is absent |
| 0x11 | Throttle position | | | |
| 0x06 | Short-term fuel trim B1 | | | air/fuel |
| 0x07 | Long-term fuel trim B1 | | | air/fuel |
| 0x14–0x1B | O2 sensor voltages | | | air/fuel |
| 0x42 | Control module voltage | | | sensor/electrical |
| 0x5C | Engine oil temp | | | |
| — | Oil pressure | | | Usually not a standard PID; the CR-V likely has only a switch. Nothing depends on it (D-021). |
| 0x2F | Fuel level | | | |
| 0x31 | Distance since codes cleared | | | odometer proxy |

## After the survey

Every supported PID in the table above becomes an input to the model for that car. Record
which ones each car answers, with the raw replies, and update the sensor list in
`docs/DESIGN.md` §3.
Manufacturer (Mode 22) PIDs are a stretch: try them only if a fault area has too few
sensors to call.
