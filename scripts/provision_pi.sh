#!/usr/bin/env bash
# Clean Raspberry Pi OS -> ready to build and run carwatch.
# This script is graded indirectly: if the TA can't boot it, the TA can't grade it.
set -euo pipefail

echo "== packages =="
sudo apt-get update
sudo apt-get install -y build-essential valgrind git screen bluez   # bluez: bluetoothctl, rfcomm

echo "== OBD2 adapter: pair, bind, /dev/obd (D-019) =="
# TODO(M2): pair and trust the OBDLink LX once (Bluetooth Classic SPP, not BLE):
#             bluetoothctl -- scan on / pair <MAC> / trust <MAC>
#           record the MAC in docs/hardware/wiring.md.
# TODO(M2): bind it at every boot with a systemd unit running
#             /usr/bin/rfcomm bind 0 <MAC> 1        -> /dev/rfcomm0
#           (confirm Raspberry Pi OS's bluez still ships the rfcomm tool).
# TODO(M2): write /etc/udev/rules.d/99-obd.rules so consumers see a stable name:
#   KERNEL=="rfcomm0", SYMLINK+="obd", MODE="0660", GROUP="dialout"
#           then: sudo udevadm control --reload && sudo udevadm trigger
#           verify with: ls -l /dev/obd
# Fallback (USB adapter, D-015): replace the rule with a vendor/product match from lsusb:
#   SUBSYSTEM=="tty", ATTRS{idVendor}=="<vid>", ATTRS{idProduct}=="<pid>", SYMLINK+="obd", MODE="0660", GROUP="dialout"

echo "== serial access =="
# TODO(M2): sudo usermod -aG dialout "$USER"   (the daemon's user must open /dev/obd)

echo "not implemented — M0 scaffold" >&2
exit 1
