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

**Crystal:** note whether your module has an 8 MHz or 16 MHz crystal — it goes straight
into the bitrate configuration and a wrong value produces a silently dead bus.

```
dtoverlay=mcp2515-can0,oscillator=<8000000|16000000>,interrupt=25
dtoverlay=spi0-hw-cs
```

## OBD2 connector

| OBD2 pin | Signal |
|---|---|
| 6 | CAN_H |
| 14 | CAN_L |
| 16 | +12 V battery (always live) |
| 4, 5 | Chassis / signal ground |

**Pin 16 is live with the key off.** That is what makes the unattended soak possible and
what makes a dead battery possible. Fuse it, and measure the device's key-off draw
before leaving it in a car overnight — record the number in `docs/EVALUATION.md`.

## Bring-up order

1. Meter the module's VCC and MISO with the Pi disconnected.
2. Pi + module on the bench, no car. `ip link show can0`.
3. Two-node bench bus (module + USB-to-CAN adapter) — prove frames move before a car is
   ever involved.
4. Car, engine off, key to accessory. `candump can0`.
5. Car, running.

Do not skip step 3. Debugging a silent bus with a car attached is debugging two
unknowns at once.
