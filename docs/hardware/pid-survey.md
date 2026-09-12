# Supported-PID survey

**Blocks D-007 and the whole of diagnostic #1. Run this before M2.**

Method: query Mode 01 PIDs `0x00`, `0x20`, `0x40`, `0x60` — each returns a 32-bit
bitmap of which PIDs in the next block the ECU supports. A $12 ELM327 does this in ten
minutes per car; no need to wait on the MCP2515 hardware.

## What we specifically need to know

| PID | Signal | Outback | Honda | Notes |
|---|---|---|---|---|
| 0x0C | RPM | | | |
| 0x04 | Calculated load | | | |
| 0x05 | Coolant temp | | | |
| 0x0F | Intake air temp | | | |
| 0x0B | MAP | | | |
| 0x06/0x07 | Short/long term fuel trim B1 | | | |
| 0x5C | Engine oil temp | | | |
| **—** | **Oil pressure** | | | **Usually NOT a standard PID.** Check for a manufacturer-specific frame, or accept that only a binary low-pressure switch exists. |
| 0x2F | Fuel level | | | |
| 0x31 | Distance since codes cleared | | | odometer proxy for oil-age trending |

## If oil pressure is unavailable on both vehicles

Options, in order of preference:
1. Sniff manufacturer-specific frames for an analog pressure value (real work, real
   payoff, and exactly the CAN reverse-engineering this project was originally about).
2. Add a physical pressure sender with a T-fitting at the sender port. Real automotive
   work on a daily driver — do not commit to this after Week 10.
3. Replace diagnostic #1 with an oil-*temperature*-based thermal-load metric and say so
   plainly in docs/DESIGN.md §5 as a declared substitution.

Record the outcome here and update `docs/decisions/D-003-narrow-the-claim.md`.
