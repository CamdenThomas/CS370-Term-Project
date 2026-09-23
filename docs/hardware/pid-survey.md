# Supported-PID survey

**Sets the model's input list (D-007, D-021). Run this before M2.**

Method: with the OBD2 adapter (D-019) paired and bound as `/dev/obd`, open it in a
terminal (`screen /dev/obd`; the baud is ignored over Bluetooth) and send `ATZ`, `ATE0`,
`ATSP0`, then `0100`, `0120`, `0140`, `0160`. Each reply is a 32-bit
bitmap of which PIDs in the next block the ECU supports. Ten minutes; paste the raw
replies below the table, dated.

## What we specifically need to know

Vehicle: Lance's **2015 Honda CR-V EX-L** (D-007, D-016 — engine and transmission still
to be confirmed from the VIN).

| PID | Signal | CR-V | Notes |
| --- | --- | --- | --- |
| 0x0C | RPM | | |
| 0x04 | Calculated load | | |
| 0x05 | Coolant temp | | |
| 0x0F | Intake air temp | | |
| 0x0B | MAP | | |
| 0x06/0x07 | Short/long term fuel trim B1 | | |
| 0x5C | Engine oil temp | | |
| — | Oil pressure | | Usually not a standard PID; the CR-V likely has only a switch. Nothing depends on it (D-021). |
| 0x2F | Fuel level | | |
| 0x31 | Distance since codes cleared | | odometer proxy |

## After the survey

Every supported PID in the table above becomes an input to the model. Record which ones the
CR-V answers, with the raw replies, and update the sensor list in `docs/DESIGN.md` §4.
Manufacturer (Mode 22) PIDs are a stretch: try them only if a fault area has too few
sensors to call.
