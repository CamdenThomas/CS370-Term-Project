# Bill of materials

> **Nothing on this page is ordered until the paper design is signed** (D-024). Then
> order the dongle first: shipping time is the most common silent schedule-killer on this
> project (handout §16).

The product is one box with one USB-C port, one light and one temperature probe, plus a
Bluetooth OBD2 dongle in the car's OBD2 port (D-019, D-024, D-037). The owner supplies the
USB-C charger.

## Core: what we buy

| Qty | Part | ~Cost | Notes |
| --- | --- | --- | --- |
| 1 | Raspberry Pi 4 (2 GB+) | owned | Both partners already have one. |
| 2 | 32 GB A2 microSD | $20 | A2 endurance matters for a key-cycle soak. Two, because the soak eats cards. |
| 1 | OBDLink LX, Bluetooth Classic OBD2 dongle (STN chip) | ~$60, verify | The data path (D-019). Bluetooth Classic (SPP), **not BLE**. A cheap ELM327 Bluetooth clone is enough for the first PID survey but is unreliable for a 48 h run. |
| 1 | USB-C cable, short, 3 A-rated | $6 | From the owner's charger to the box. |
| — | USB-C car charger | owner's | Not supplied: the box takes whatever the owner has (D-024). It must go dead with the key off (`keyoff`). |
| 1 | USB-C power meter (inline) | $10 | Settles "does this charger go dead at key-off, what does it deliver, and does it sag at crank" in seconds. |

**Fallback, not ordered:** a USB OBD2 adapter with an STN chip (e.g. OBDLink SX, ~$30,
D-015) if Bluetooth proves unreliable in the car.

## The box: designed, not ordered (D-024)

Listed by function. A part number is added only with its datasheet linked and checked by
a human (`CLAUDE.md` §8.4). Sizes marked *estimate* come from the power worksheet
(`powerspec`), not from measurement.

| Qty | Function | Notes |
| --- | --- | --- |
| 1 | USB-C receptacle, wired as a sink: 5.1 kΩ on CC1 and CC2 | The only power input. |
| 1 | Input protection: transient suppressor, reverse-polarity protection, fuse | |
| 1 | Input current limit and inrush limiter | |
| 1 | CC-level detector: comparators or a USB-C controller IC | Reports the source's advertisement (default, 1.5 A or 3 A) on GPIOs; the Pi has no analog inputs. |
| 2 | Supercapacitor cells, 2.7 V, about 20 F each (about 10 F in series) | *Estimate.* Needs cell balancing and a fuse; the temperature rating must cover a parked car's cabin. |
| 1 | Power-path controller | The source feeds the Pi first; the bank fills only the gap. |
| 1 | 5 V / 3 A converter that can step up | The bank's voltage falls as it discharges. |
| 1 | Source-present divider and halt-done switch | GPIOs chosen in the schematic (`schpi`). |
| 1 | Tri-color LED (red, yellow, green) + its resistors + the boot-time pull | The light (D-027). Two or three GPIOs, depending on the part; resistor values from its datasheet (`ledspec`). |
| 1 | DS18B20 temperature probe, waterproof, on a lead of about 1 m, + a 4.7 kΩ resistor | The box's own sensor (D-037), about $3 to $8. Bought after it is drawn (`probeorder`); powered from 3.3 V. |
| 1 | Real-time clock module on I2C, with its own backup cell or capacitor | Keeps wall time while the box is off (D-030). The backup's temperature rating must cover the cabin (`rtcspec`). |
| 1 | Heatsink for the Pi | A Pi 4 throttles near 80 °C (`thermal`). |
| 1 | Shell: heat-resistant plastic (ABS, ASA or similar), vent slots, no metal near the antenna | `enclspec`. PLA softens near 60 °C, below a parked cabin's temperature. A food container is an acceptable prototype (handout). |

## Stretch — raw CAN (D-019)

Not ordered unless the stretch goal is taken up after mechanisms D and E are measured.

| Qty | Part | ~Cost | Notes |
| --- | --- | --- | --- |
| 1 | MCP2515 + TJA1050 CAN module | $8 | Check the crystal (8 vs 16 MHz) and meter MISO for 5 V before it touches the Pi. |
| 1 | OBD2 pass-through Y-splitter | $10 | Pins 6, 14, 5. |
| 1 | Logic analyzer (8ch clone) | $12 | Settles SPI timing arguments. |
