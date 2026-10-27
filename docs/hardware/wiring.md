# Wiring and electrical safety

The product is one box with one USB-C port and one light, plus a Bluetooth OBD2 dongle
in the car's OBD2 port (D-019, D-024). Nothing on the car is cut, spliced or pierced.

```text
  OBD2 port ──[Bluetooth OBD2 dongle]~~ RFCOMM ~~▶ Pi (/dev/rfcomm0 → /dev/obd)

  owner's USB-C charger ──USB-C──▶ [box: power block, supercapacitor] ──5 V──▶ Pi
                                        │  source-present (divided to 3.3 V) ──▶ GPIO
                                        ◀── halt-done ─────────────────────── GPIO
  Pi GPIOs ──resistors──▶ tri-color LED ──▶ GND    (the light, D-027)
  Pi I2C (GPIO2 SDA, GPIO3 SCL) ──▶ real-time clock, powered from 3.3 V   (D-030)
  Pi GPIO4 (1-Wire) ◀── DS18B20 probe on a short lead, 4.7 kΩ pull-up    (D-037)
```

GPIO numbers for source-present, halt-done and the light are set in the schematic
(`schpi`), not here.

> **The Pi's GPIO is 3.3 V and has zero over-voltage protection.** Source-present comes
> from the 5 V USB input, so it reaches the header only through a divider; meter it
> before it is connected. Wire everything with the Pi powered off.

## OBD2 dongle → Pi (D-019)

| Item | Value | Note |
| --- | --- | --- |
| Dongle | OBDLink LX (STN chip) | **Bluetooth Classic, Serial Port Profile. Not BLE:** a BLE-only adapter has no RFCOMM channel and will not bind. |
| Connection | The Pi's built-in Bluetooth | Paired and trusted once with `bluetoothctl`; record the dongle's MAC address here. |
| Stable name | `/dev/obd` | `rfcomm bind 0 <MAC> 1` creates `/dev/rfcomm0` at every boot, and a udev rule links it to `/dev/obd` (`scripts/provision_pi.sh`). The daemon never opens an `rfcomm` number. |
| Baud | ignored | RFCOMM carries bytes without a line rate; `screen` and `obdd` may set any baud. |
| Fallback | USB OBD2 adapter (D-015, §G.4) | Swap the udev rule to match its vendor/product ID; nothing else changes. |
| Protocol | `ATSP0` (automatic) | The dongle negotiates the car's OBD2 protocol itself. Record what it chose (`ATDP`) in `pid-survey.md`. |
| Dongle power | OBD2 pin 16 — **live with the key off** | The dongle draws from the car even when parked. Measure its key-off current (board item `dongledrain`) before leaving it plugged in overnight. |

## Power — the box's USB-C input (D-024)

The box takes the owner's own USB-C charger. Its power block (requirements in
`docs/DECISIONS.md` §A.6):

1. sinks 5 V only (5.1 kΩ on both CC pins), behind transient and reverse-polarity
   protection and a fuse;
2. limits input current below what the source advertises, and limits inrush into the
   empty supercapacitor bank;
3. feeds the Pi from the source first and from the bank only for the gap;
4. reports source-present on a GPIO;
5. cuts the Pi's supply when the Pi signals halt-done, even if the source has returned,
   then re-enables after a short off time;
6. powers on again by itself when the source returns.

**Not supported:** a charger that stays live with the key off. The box would stay on.

An undervolted Pi keeps running and corrupts the SD card later. Prove the supply; do not
trust its label.

### Measurements (dated; a row without a result has not been done)

| Measurement | How | Feeds | Date | Result |
| --- | --- | --- | --- | --- |
| Does the owner's charger go dead with the key off? Is it live in accessory? | USB-C meter, key off / ACC / run | `keyoff` | | |
| Supply dip at crank | meter or scope at the charger's output | `cranklog`, the grace period | | |
| Pi 4 draw by state: boot, idle, loaded, shutdown | USB-C meter | `powerspec` | | |
| Boot time and shutdown time on the real image | `systemd-analyze`, log timestamps | `bootbudget` | | |
| Bank discharge into a dummy load; charge time from empty | meter, stopwatch | `capbench` | | |
| `vcgencmd get_throttled` after a cold start | on the Pi; must be `0x0` | `keyoff` | | |
| Pi and bank temperature in a sealed shell, hot cabin | Pi sensor, thermometer | `thermal` | | |
| Dongle key-off current from pin 16 | meter | `dongledrain` | | |

## The light → Pi (D-027)

One tri-color LED, written only by the supervisor. Two or three GPIOs, depending on
whether the part has its own yellow die or mixes red and green (`ledspec`). Each GPIO
drives its color through a resistor sized from the LED's datasheet, keeping the current
well inside the pin's limit. A pull holds yellow on from power-up, while the GPIO is still
an input, so a booting box shows steady yellow.

Every live state is a pulse or blink; a **steady** light, other than yellow while booting,
or a dark one while the car runs, means the device is not running (DECISIONS §B.5).

## Real-time clock → Pi (D-030)

| Clock pin | Pi pin | Pi signal | Note |
| --- | --- | --- | --- |
| SDA | 3 | GPIO2 (I2C1 SDA) | |
| SCL | 5 | GPIO3 (I2C1 SCL) | |
| VCC | 1 | 3.3 V | Power it from 3.3 V, so its I2C pull-ups never put 5 V on the Pi. Meter it before connecting. |
| GND | 6 | GND | |

The part, its backup cell or capacitor, and that backup's temperature rating are chosen in
`rtcspec`.

## DS18B20 probe → Pi (D-037)

| Probe wire | Pi pin | Pi signal | Note |
| --- | --- | --- | --- |
| DQ (data) | 7 | GPIO4 | The default pin of the kernel's `w1-gpio` overlay. A 4.7 kΩ pull-up from DQ to 3.3 V. |
| VDD | 1 or 17 | 3.3 V | Powered, not parasite mode. Never 5 V: the data line would pull the GPIO above 3.3 V. |
| GND | 9 | GND | |

Pins and the resistor are the 1-Wire defaults, to be checked against the probe's datasheet
and drawn in the Pi interface block (`schpi`) before wiring. Probe wire colors vary by
seller; identify them from the seller's sheet, not by color. The probe sits outside the
shell, in cabin air, away from the Pi's heat.

## Bring-up order

1. Pi on the bench, dongle powered, no car: `bluetoothctl` pairs and trusts it, then
   `rfcomm bind 0 <MAC> 1`, `ls -l /dev/obd`, and `ATZ` answers with the dongle's
   version string. Reboot once and confirm `/dev/obd` comes back without a hand.
2. Car, key to run, engine off: `0100` returns the supported-PID bitmap.
3. Car, engine running: `010C` (RPM) changes when you touch the throttle.
4. Probe, Pi on the bench: `dtoverlay=w1-gpio` in `/boot/firmware/config.txt`, reboot,
   then `cat /sys/bus/w1/devices/28-*/w1_slave` shows `YES` on the first line and a
   `t=` value near room temperature on the second. Paste it here, dated (`probebench`).
5. The power measurements above: the free ones first (the owner's charger with a meter,
   key off and at crank), the bank only after the power design is signed.

Measured results go in this file, dated. A step without a recorded result has not been
done (CLAUDE.md §8.4).
