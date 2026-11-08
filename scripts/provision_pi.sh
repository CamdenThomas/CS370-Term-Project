#!/usr/bin/env bash
# Clean Raspberry Pi OS -> a box that logs the car at every key-on.
# This script is graded indirectly: if the TA can't boot it, the TA can't grade it.
#
#   scripts/provision_pi.sh               # packages, dialout, venv, data directory
#   scripts/provision_pi.sh <OBD-MAC>     # also bind the paired dongle to /dev/obd at
#                                         # boot and start the logger at every key-on
#
# Run it on the Pi, as the user who will own the box, from the repository root
# (make deploy puts the tree in ~/carwatch). It uses sudo. Running it again changes
# nothing that is already right.
#
# Pairing is by hand, once, with the dongle powered (12 V on OBD pin 16: a parked car,
# or a bench supply on an OBD2 breakout), because it needs a button press or a PIN:
#   bluetoothctl     then: power on / scan on / pair <MAC> / trust <MAC> / quit
#
# Until obdd replaces it, the key-on logger is tools/obdlog.py (D-033): its records are
# labeled live, and it is not the product (DESIGN.md §7).
set -euo pipefail

MAC=${1:-}
REPO=$(cd "$(dirname "$0")/.." && pwd)
USER_NAME=$(id -un)
DATA=/var/lib/carwatch/drives
UNITS=/etc/systemd/system

die() { echo "provision: $*" >&2; exit 1; }

[ "$(uname -s)" = Linux ] || die "run this on the Pi, not on $(uname -s)"
[ "$(id -u)" -ne 0 ] || die "run as the box's user, not root; it calls sudo itself"
[ -f "$REPO/tools/obdlog.py" ] || die "no tools/obdlog.py under $REPO"
if [ -n "$MAC" ]; then
    [[ $MAC =~ ^([0-9A-Fa-f]{2}:){5}[0-9A-Fa-f]{2}$ ]] ||
        die "'$MAC' is not a Bluetooth address (AA:BB:CC:DD:EE:FF)"
fi

echo "== packages =="
sudo apt-get update
sudo apt-get install -y build-essential valgrind git bluez python3-venv

echo "== serial access =="
# The logger and obdd open /dev/obd, which the udev rule below gives to dialout.
if id -nG "$USER_NAME" | tr ' ' '\n' | grep -qx dialout; then
    echo "$USER_NAME is already in dialout"
else
    sudo usermod -aG dialout "$USER_NAME"
    echo "$USER_NAME added to dialout: log out and back in before opening /dev/obd by hand"
fi

echo "== tools venv =="
# Logging needs only pyserial (tools/requirements.txt); make test adds the rest.
[ -x "$REPO/.venv/bin/python" ] || python3 -m venv "$REPO/.venv"
"$REPO/.venv/bin/pip" install -q -r "$REPO/tools/requirements.txt"

echo "== data directory =="
sudo install -d -m 0755 -o "$USER_NAME" -g "$USER_NAME" "$DATA"

if [ -z "$MAC" ]; then
    cat <<EOF

Done, except Bluetooth. With the dongle powered, pair it once:
  bluetoothctl     then: power on / scan on / pair <MAC> / trust <MAC> / quit
then run again with its address:
  scripts/provision_pi.sh <MAC>
EOF
    exit 0
fi

echo "== OBD2 adapter: bind $MAC to /dev/obd at boot (D-019) =="
# Evidence, not a guess (CLAUDE.md 8.4): newer bluez builds may leave rfcomm out.
RFCOMM=$(command -v rfcomm || true)
[ -n "$RFCOMM" ] || die "this Pi's bluez has no rfcomm tool: paste 'dpkg -L bluez | grep -i rfcomm' and 'bluetoothctl --version' into the board item 'bench' before going on"
bluetoothctl info "$MAC" 2>/dev/null | grep -q "Paired: yes" ||
    die "$MAC is not paired: pair and trust it with bluetoothctl first"

# rfcomm bind makes /dev/rfcomm0 at once; the Bluetooth link itself opens only when a
# program opens the device, so the unit succeeds with the dongle asleep or away.
sudo tee "$UNITS/carwatch-rfcomm.service" >/dev/null <<EOF
[Unit]
Description=carwatch: bind the OBD2 dongle $MAC to /dev/rfcomm0
After=bluetooth.service
Requires=bluetooth.service

[Service]
Type=oneshot
RemainAfterExit=yes
ExecStart=$RFCOMM bind 0 $MAC 1
ExecStop=$RFCOMM release 0

[Install]
WantedBy=multi-user.target
EOF

# Consumers open /dev/obd, never an rfcomm number (docs/hardware/wiring.md).
sudo tee /etc/udev/rules.d/99-obd.rules >/dev/null <<'EOF'
KERNEL=="rfcomm0", SYMLINK+="obd", MODE="0660", GROUP="dialout"
EOF
sudo udevadm control --reload

echo "== key-on logger (pilogger) =="
sed -e "s|@USER@|$USER_NAME|g" -e "s|@REPO@|$REPO|g" -e "s|@DATA@|$DATA|g" \
    "$REPO/systemd/carwatch-obdlog.service" | sudo tee "$UNITS/carwatch-obdlog.service" >/dev/null

sudo systemctl daemon-reload
sudo systemctl enable --now carwatch-rfcomm.service
sudo udevadm trigger --subsystem-match=tty
sudo systemctl enable carwatch-obdlog.service
sudo systemctl restart carwatch-obdlog.service

echo
ls -l /dev/rfcomm0 /dev/obd || die "/dev/obd did not appear: paste 'udevadm info /dev/rfcomm0'"
cat <<EOF

The logger starts at every boot and writes $DATA/drive-*.jsonl.
  status:   systemctl status carwatch-obdlog
  its log:  journalctl -u carwatch-obdlog -f
With the dongle off it keeps retrying the connection; that is expected.
EOF
