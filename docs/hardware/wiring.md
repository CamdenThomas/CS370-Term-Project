# Wiring and electrical safety

The product is a Raspberry Pi 4 in a small box with one USB-C port and one light, plus a
Bluetooth OBD2 dongle in the car's OBD2 port (D-019). Nothing on the car is cut, spliced
or pierced, and nothing of ours is wired to it: the dongle plugs in, the box plugs into
the owner's charger. There is no power block, battery, supercapacitor, cooling or sensor
of our own (scope cut 2026-10-28, `CLAUDE.md` §3).

```text
  OBD2 port ──[OBDLink LX]~~ Bluetooth (RFCOMM) ~~▶ Pi (/dev/rfcomm0 → /dev/obd)

  owner's USB-C charger ──USB-C cable──▶ Pi 4 USB-C power input
  Pi GPIOs ──resistors──▶ tri-color LED ──▶ GND                     (the light, D-027)
  Pi I2C (GPIO2 SDA, GPIO3 SCL) ──▶ clock module, powered from 3.3 V (`rtcspec`)
```

> **The Pi's GPIO is 3.3 V and has no over-voltage protection.** Power every header
> part from the 3.3 V pin, never 5 V, and wire with the Pi powered off.

## OBD2 dongle → Pi (D-019)

| Item | Value | Note |
| --- | --- | --- |
| Dongle | OBDLink LX (STN chip; reports itself as `ELM327 v1.4b`) | **Bluetooth Classic, Serial Port Profile. Not BLE:** a BLE-only adapter has no RFCOMM channel and will not bind. |
| Connection | The Pi's built-in Bluetooth | Paired and trusted once with `bluetoothctl`; record the dongle's MAC address here when the Pi is paired (`bench`). |
| Stable name | `/dev/obd` | `rfcomm bind 0 <MAC> 1` creates `/dev/rfcomm0` at every boot, and a udev rule links it to `/dev/obd` (`scripts/provision_pi.sh`). The daemon never opens an `rfcomm` number. |
| Baud | ignored | RFCOMM carries bytes without a line rate; `screen` and `obdd` may set any baud. |
| Protocol | `ATSP0` (automatic) | The dongle negotiated `ISO 15765-4 (CAN 11/500)` on the Forester (`pid-survey.md`). |
| Dongle power | OBD2 pin 16 — **live with the key off** | The dongle draws from the car even when parked. Measure its key-off current (`dongledrain`) before leaving it plugged in overnight. |
| On a Mac | `/dev/cu.OBDLinkLX` | Pairs from System Settings. Closing the port drops the link and reopening it gets no bytes (2026-10-28), so the adapter is opened once per run (`scripts/capture.sh`). |

## Power: the owner's charger

The Pi takes whatever USB-C charger the owner has. It is live when the car runs and
dead at key-off, so the box boots at every start and loses power at every stop; the
storage layer recovers the log at the next boot (`DESIGN.md` §5, §6). Two things to
prove with a meter, not assume:

| Measurement | How | Feeds | Date | Result |
| --- | --- | --- | --- | --- |
| Does the owner's charger go dead with the key off? Is it live in accessory? | USB-C meter, key off / ACC / run | `keyoff` | | |
| Does the supply dip at crank reboot the Pi? | watch the light at a cold start | `keyoff` | | |
| `vcgencmd get_throttled` after a cold start | on the Pi; must be `0x0` | `keyoff` | | |
| Boot time on the real image | `systemd-analyze`, log timestamps | `bootbudget` | | |
| Dongle key-off current from pin 16 | meter | `dongledrain` | | |

A charger that stays live with the key off keeps the box on; it then sees a silent ECU
and records nothing (`ecu_silent`). An undervolted Pi keeps running and corrupts the SD
card later: prove the supply, do not trust its label.

## The light → Pi (D-027)

One tri-color LED, written only by the supervisor. Two or three GPIOs, depending on
whether the part has its own yellow die or mixes red and green (`ledspec`). Each GPIO
drives its color through a resistor sized from the LED's datasheet, keeping the current
well inside the pin's limit. A pull holds yellow on from power-up, while the GPIO is still
an input, so a booting box shows steady yellow. Every live state is a pulse or blink; a
**steady** light, other than yellow while booting, or a dark one while the car runs, means
the device is not running.

## Clock → Pi (`rtcspec`)

| Clock pin | Pi pin | Pi signal | Note |
| --- | --- | --- | --- |
| SDA | 3 | GPIO2 (I2C1 SDA) | |
| SCL | 5 | GPIO3 (I2C1 SCL) | |
| VCC | 1 | 3.3 V | Power it from 3.3 V, so its I2C pull-ups never put 5 V on the Pi. Meter it before connecting. |
| GND | 6 | GND | |

The part and its backup cell or capacitor, and that backup's temperature rating, are
chosen in `rtcspec`. Without a clock the box runs on monotonic time and marks every
record's wall time untrusted (`DESIGN.md` §4).

## Bring-up order

1. Pi on the bench, dongle powered, no car: `bluetoothctl` pairs and trusts it, then
   `rfcomm bind 0 <MAC> 1`, `ls -l /dev/obd`, and `ATZ` answers with the dongle's
   version string. Reboot once and confirm `/dev/obd` comes back without a hand (`bench`).
2. Car, key to run, engine off: `0100` returns the supported-PID bitmap.
3. Car, engine running: `010C` (RPM) changes when you touch the throttle. Done on
   2026-10-28 from a Mac: `captures/forester/20261028T220659.957/`.
4. The charger measurements above, with the meter, at the next drive.

Measured results go in this file, dated. A step without a recorded result has not been
done (`CLAUDE.md` §8.4).
