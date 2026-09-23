# Wiring and electrical safety

The device is three off-the-shelf connections and one LED (D-011, D-015). Nothing on the
car is cut, spliced or pierced.

```text
  OBD2 port ──[USB OBD2 adapter]──USB──▶ Pi (/dev/obd)
  car USB-C port or 12 V socket adapter ──USB-C──▶ Pi power
  Pi GPIO17 ──330 Ω──▶ LED ──▶ GND          (warning light)
```

> **The Pi's GPIO is 3.3 V and has zero over-voltage protection.** The LED is the only
> thing wired to the header. Wire it with the Pi powered off.

## OBD2 adapter → Pi (D-015)

| Item | Value | Note |
|---|---|---|
| Connection | USB-A on the Pi | The adapter enumerates as a USB serial device (`/dev/ttyUSB*` or `/dev/ttyACM*`). |
| Stable name | `/dev/obd` | A udev rule matching the adapter's vendor/product ID, installed by `scripts/provision_pi.sh`. The daemon never opens a `ttyUSB` number. |
| Baud | 115200 (OBDLink SX) · 38400 (most ELM327 clones) | Record the adapter model and its baud here once it arrives. |
| Protocol | `ATSP0` (automatic) | The adapter negotiates the car's OBD2 protocol itself. |
| Adapter power | OBD2 pin 16 — **live with the key off** | The adapter draws from the car even when parked. Measure its key-off current (board item `keyoff`) before leaving it plugged in overnight. |

## Power — the car's USB-C (D-011)

The Pi is powered from the car's own USB-C port, or a USB-C adapter in the 12 V socket.

| Check | How | Record |
|---|---|---|
| Is the port switched (off with the key)? | Meter or a USB power tester, key off / ACC / run | here |
| Does it hold the Pi up at crank? | `vcgencmd get_throttled` after a cold start; must be `0x0` | here |
| Rating | ≥ 5.1 V / 3 A for a Pi 4 | adapter model, here |

A cheap built-in USB port is the likeliest silent failure in this design: an undervolted Pi
keeps running and corrupts the SD card later. Prove the port, do not trust its label.

## Warning light → Pi (D-014)

| Part | Pi pin | Pi signal | Note |
|---|---|---|---|
| LED anode, via 330 Ω | 11 | GPIO17 (proposed) | ~4 mA at 3.3 V — well inside the pin's limit. Written only by the supervisor. |
| LED cathode | 9 | GND | |

Every live state is a blink pattern; a **steady** light, on or off, means the device is not
running (DECISIONS §B.5).

## Bring-up order

1. Pi on the bench, adapter plugged in, no car: `ls -l /dev/obd`, then `ATZ` answers with
   the adapter's version string.
2. Car, key to run, engine off: `0100` returns the supported-PID bitmap.
3. Car, engine running: `010C` (RPM) changes when you touch the throttle.
4. USB-C power checks above, on a cold start.

Measured results go in this file, dated. A step without a recorded result has not been
done (CLAUDE.md §8.4).
