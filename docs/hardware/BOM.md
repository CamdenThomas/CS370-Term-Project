# Bill of materials

> **Order the day you read this.** Shipping time is the most common silent
> schedule-killer on this project (handout §16).

## Core — order now (M0)

| Qty | Part | ~Cost | Notes |
|---|---|---|---|
| 1 | Raspberry Pi 4 (2 GB+) | $45 | Pi 5/3B+ also fine. Zero 2 W is fine to *deploy* but painfully slow to compile on. |
| 1 | 32 GB A2 microSD | $10 | A2 endurance matters for a 48 h write soak. Buy two; the soak eats cards. |
| 1 | MCP2515 + TJA1050 CAN module | $8 | **Check the crystal — 8 MHz vs 16 MHz changes the bitrate config.** Many boards are 5 V-only; see wiring notes. |
| 1 | OBD2 male pigtail / breakout | $10 | Need pin 6 (CAN_H), pin 14 (CAN_L), pin 16 (+12 V), pin 4/5 (GND). |
| 1 | 12 V → 5 V 3 A buck converter | $8 | Automotive-rated. The Pi browns out on a cheap one at crank. |
| 1 | Jumper wires + breadboard | $8 | |
| 1 | Multimeter | — | Assume you have one. Do not skip the voltage check. |

**Core subtotal ≈ $90.**

## Conditional — only if D-006 resolves to "add physical sensors"

| Qty | Part | ~Cost | Notes |
|---|---|---|---|
| 1 | MPU-6050 IMU | $4 | I2C 400 kHz, hardware FIFO. Buys mechanism F a high-rate source and gives vibration data the ECU cannot see. |
| 1 | DS18B20 waterproof probe | $4 | 1-Wire, kernel driver exists. |
| 1 | 4.7 kΩ resistor | — | 1-Wire pull-up. |

## Nice to have

| Qty | Part | ~Cost | Notes |
|---|---|---|---|
| 1 | Cheap ELM327 BT dongle | $12 | **Not the product's path (D-001)** — a `tools/`-side cross-check and the fastest way to run the D-007 PID survey this week. |
| 1 | USB-to-CAN adapter | $25 | Drives the bench replay rig for `make replay` and soak rehearsal. |
| 1 | Logic analyzer (8ch clone) | $12 | The instrument that settles SPI timing arguments Claude cannot settle. |
| 1 | Battery tender | $30 | Only if D-006 resolves to the parked-car soak. |
| 1 | Food container | $0 | The enclosure. The handout is explicit about this. |
