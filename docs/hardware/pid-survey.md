# Supported-PID survey

**Blocks D-007 and the whole of diagnostic #1. Run this before M2.**

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
| **—** | **Oil pressure** | | **Usually NOT a standard PID.** Try manufacturer (Mode 22) PIDs through the same adapter, or accept that only a binary low-pressure switch exists. |
| 0x2F | Fuel level | | |
| 0x31 | Distance since codes cleared | | odometer proxy for oil-age trending |

## If oil pressure is unavailable

Options, in order of preference:

1. Query manufacturer-specific (Mode 22) PIDs through the same adapter for an analog
   pressure value. Support varies by make and is undocumented; try it, record what answers.
2. Add a physical pressure sender with a T-fitting at the sender port. Real automotive
   work on a daily driver — do not commit to this after Week 10.
3. Replace diagnostic #1 with an oil-*temperature*-based thermal-load metric and say so
   plainly in docs/DESIGN.md §5 as a declared substitution.

Record the outcome here and in `docs/DECISIONS.md` §E.1 (D-003), through board item `oilq`.
