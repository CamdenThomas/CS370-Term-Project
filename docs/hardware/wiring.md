# Wiring and electrical safety

> **The Pi's GPIO is 3.3 V and has zero over-voltage protection.** Feed a pin 5 V and
> it can die silently — sometimes taking the SoC with it. Swap VCC and GND and the
> board usually does not warn you first; it just stops working.
>
> Rules, non-negotiable: **breadboard anything 5 V-adjacent first. Check every voltage
> with a meter before the Pi is ever plugged in. Wire with the board powered off.**

## MCP2515 module → Pi

| MCP2515 | Pi pin | Pi signal | Note |
|---|---|---|---|
| VCC | [TBD] | 3.3 V or 5 V | **Read your module.** Many boards run the TJA1050 transceiver at 5 V and level-shift the MCP2515 side; many cheap clones do not and will drive 5 V onto MISO. Verify before connecting. |
| GND | 6 | GND | Common ground with the vehicle. |
| CS | 24 | GPIO8 / SPI0_CE0 | |
| SO (MISO) | 21 | GPIO9 / SPI0_MISO | **Meter this line for 5 V before connecting.** |
| SI (MOSI) | 19 | GPIO10 / SPI0_MOSI | |
| SCK | 23 | GPIO11 / SPI0_SCLK | |
| INT | [TBD] | GPIO25 (proposed) | Edge-triggered. This line *is* mechanism B. |

## Warning light → Pi (D-014)

| Part | Pi pin | Pi signal | Note |
|---|---|---|---|
| LED anode, via 330 Ω | 11 | GPIO17 (proposed) | ~4 mA at 3.3 V — well inside the pin's limit. Written only by the supervisor. |
| LED cathode | 9 | GND | |

Every live state is a blink pattern; a **steady** light, on or off, means the device is not
running (DECISIONS §B.5).

**Crystal:** note whether your module has an 8 MHz or 16 MHz crystal — it goes straight
into the bitrate configuration and a wrong value produces a silently dead bus.

```
dtoverlay=mcp2515-can0,oscillator=<8000000|16000000>,interrupt=25
dtoverlay=spi0-hw-cs
```

## OBD2 connector — data only (D-011)

Connected through a **pass-through Y-splitter**, so the port stays free for a scan tool.
Nothing on the car is cut, spliced or pierced.

| OBD2 pin | Signal | Used? |
|---|---|---|
| 6 | CAN_H | yes → module CAN_H |
| 14 | CAN_L | yes → module CAN_L |
| 5 | Signal ground | yes → module GND (CAN reference) |
| 4 | Chassis ground | no |
| 16 | +12 V battery (always live) | **no** — power comes from USB-C, below |

**Before the module ever touches a car:**
1. **Remove the module's 120 Ω termination jumper** (often `J1`). The car's bus is already
   terminated at both ends; a third terminator drops it to ~40 Ω.
2. **Keep the stub short:** Y-cable plus module leads under ~0.3 m at 500 kbit/s.

## Power — the car's USB-C (D-011)

The Pi is powered from the car's own USB-C port, or a USB-C adapter in the 12 V socket.
There is no buck converter and no connection to OBD2 pin 16.

| Check | How | Record |
|---|---|---|
| Is the port switched (off with the key)? | Meter or a USB power tester, key off / ACC / run | here, per car |
| Does it hold the Pi up at crank? | `vcgencmd get_throttled` after a cold start; must be `0x0` | here, per car |
| Rating | ≥ 5.1 V / 3 A for a Pi 4 | adapter model, here |
| Ground offset | Meter Pi GND to OBD2 pin 5, engine running | the reading, with the date |

A cheap built-in USB port is the likeliest silent failure in this design: an undervolted Pi
keeps running and corrupts the SD card later. Prove the port, do not trust its label.

## Bring-up order

1. Meter the module's VCC and MISO with the Pi disconnected.
2. Pi + module on the bench, no car. `ip link show can0`.
3. Two-node bench bus (module + USB-to-CAN adapter) — prove frames move before a car is
   ever involved.
4. Car, engine off, key to accessory. `candump can0`.
5. Car, running.

Do not skip step 3. Debugging a silent bus with a car attached is debugging two
unknowns at once.
