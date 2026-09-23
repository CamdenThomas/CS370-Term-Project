# Bill of materials

> **Order the day you read this.** Shipping time is the most common silent
> schedule-killer on this project (handout §16).

## Core

| Qty | Part | ~Cost | Notes |
| --- | --- | --- | --- |
| 1 | Raspberry Pi 4 (2 GB+) | $45 | Pi 5/3B+ also fine. Zero 2 W is fine to *deploy* but painfully slow to compile on. |
| 2 | 32 GB A2 microSD | $20 | A2 endurance matters for a 48 h write soak. Two, because the soak eats cards. |
| 1 | USB OBD2 adapter, STN chip (e.g. OBDLink SX) | $30 | The data path (D-015). A $12 ELM327 USB clone works for the PID survey but is unreliable for a 48 h run. |
| 1 | 12 V socket → USB-C adapter, ≥ 5.1 V / 3 A | $12 | Powers the Pi (D-011). Skip only if the car's built-in USB-C port is **measured** to hold 3 A through a cold crank. |
| 1 | USB-C cable, short, 3 A-rated | $6 | |
| 1 | 5 mm LED + 330 Ω resistor + 2 jumper wires | $2 | The warning light (D-014). Any color; pick it for the enclosure. |
| 1 | USB-C power meter (inline) | $10 | Settles "is this port switched, and does it sag at crank" in seconds. |
| 1 | Food container | $0 | The enclosure. The handout is explicit about this. |

**Core subtotal ≈ $125.**

## Conditional — only if `pisensor` or D-006 resolves to "add physical sensors"

| Qty | Part | ~Cost | Notes |
| --- | --- | --- | --- |
| 1 | MPU-6050 IMU | $4 | I2C 400 kHz, hardware FIFO, `INT` pin. The only way mechanisms B and F come back (D-017), and vibration data the ECU cannot see. |
| 1 | DS18B20 waterproof probe | $4 | 1-Wire, kernel driver exists. |
| 1 | 4.7 kΩ resistor | — | 1-Wire pull-up. |
| 1 | Battery tender | $30 | Only for a parked-car soak, which also needs a socket that stays live in accessory (D-011). |

## Stretch — raw CAN (D-015)

Not ordered unless the stretch goal is taken up after mechanisms D and E are measured.

| Qty | Part | ~Cost | Notes |
| --- | --- | --- | --- |
| 1 | MCP2515 + TJA1050 CAN module | $8 | Check the crystal (8 vs 16 MHz) and meter MISO for 5 V before it touches the Pi. |
| 1 | OBD2 pass-through Y-splitter | $10 | Pins 6, 14, 5. |
| 1 | Logic analyzer (8ch clone) | $12 | Settles SPI timing arguments. |
