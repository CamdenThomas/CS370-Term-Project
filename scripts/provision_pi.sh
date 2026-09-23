#!/usr/bin/env bash
# Clean Raspberry Pi OS -> ready to build and run carwatch.
# This script is graded indirectly: if the TA can't boot it, the TA can't grade it.
set -euo pipefail

echo "== packages =="
sudo apt-get update
sudo apt-get install -y build-essential valgrind git screen

echo "== /dev/obd udev rule =="
# TODO(M2): write /etc/udev/rules.d/99-obd.rules pinning the USB OBD2 adapter to a stable
#           name, matched on the adapter's vendor/product ID (read them with `lsusb` once
#           it arrives; record them in docs/hardware/wiring.md):
#   SUBSYSTEM=="tty", ATTRS{idVendor}=="<vid>", ATTRS{idProduct}=="<pid>", SYMLINK+="obd", MODE="0660", GROUP="dialout"
#           then: sudo udevadm control --reload && sudo udevadm trigger
#           verify with: ls -l /dev/obd

echo "== serial access =="
# TODO(M2): sudo usermod -aG dialout "$USER"   (the daemon's user must open /dev/obd)

echo "not implemented — M0 scaffold" >&2
exit 1
