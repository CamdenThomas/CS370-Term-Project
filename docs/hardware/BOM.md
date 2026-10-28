# Bill of materials

> **The OBDLink LX is bought** (Camden's PROMPTLOG, E-03) and both partners own a Pi 4.
> Everything else here is small and ordered only when its board line says so.

The product is a Raspberry Pi 4 in a small box with one USB-C port and one light, plus a
Bluetooth OBD2 dongle in the car's OBD2 port (D-019). The owner supplies the USB-C
charger. No power block, battery, supercapacitor, cooling or sensor of our own (scope cut
2026-10-28, `CLAUDE.md` §3).

## Core

| Qty | Part | ~Cost | Notes |
| --- | --- | --- | --- |
| 1 | Raspberry Pi 4 (2 GB+) | owned | Both partners already have one. |
| 2 | 32 GB A2 microSD | $20 | A2 endurance matters for a key-cycle soak. Two, because the soak eats cards. |
| 1 | OBDLink LX, Bluetooth Classic OBD2 dongle (STN chip) | bought | The data path (D-019). Bluetooth Classic (SPP), **not BLE**. A cheap ELM327 Bluetooth clone is enough for a PID survey but is unreliable for a 48 h run. |
| 1 | USB-C cable, short, 3 A-rated | $6 | From the owner's charger to the Pi. |
| — | USB-C car charger | owner's | Not supplied: the box takes whatever the owner has. It must go dead with the key off (`keyoff`). |
| 1 | USB-C power meter (inline) | $10 | Settles "does this charger go dead at key-off, what does it deliver, and does it sag at crank" in seconds. |

**Fallback, not ordered:** a USB OBD2 adapter with an STN chip (e.g. OBDLink SX, ~$30,
D-015) if Bluetooth proves unreliable in the car.

## Small parts on the Pi's header

A part number is added only with its datasheet linked and checked by a human
(`CLAUDE.md` §8.4).

| Qty | Function | Notes |
| --- | --- | --- |
| 1 | Tri-color LED (red, yellow, green) + its resistors + the boot-time pull | The light (D-027). Two or three GPIOs, depending on the part; resistor values from its datasheet (`ledspec`). |
| 1 | Clock module on I2C, with its own backup cell or capacitor | Keeps wall time while the box is off. The backup's temperature rating must cover the cabin (`rtcspec`). |
| 1 | Shell: heat-resistant plastic (ABS, ASA or similar), vent slots, no metal near the antenna | `enclspec`. PLA softens near 60 °C, below a parked cabin's temperature. A food container is an acceptable prototype (handout). |

## Stretch — raw CAN (D-019)

Not ordered unless the stretch goal is taken up after mechanisms D and E are measured.

| Qty | Part | ~Cost | Notes |
| --- | --- | --- | --- |
| 1 | MCP2515 + TJA1050 CAN module | $8 | Check the crystal (8 vs 16 MHz) and meter MISO for 5 V before it touches the Pi. |
| 1 | OBD2 pass-through Y-splitter | $10 | Pins 6, 14, 5. |
| 1 | Logic analyzer (8ch clone) | $12 | Settles SPI timing arguments. |
