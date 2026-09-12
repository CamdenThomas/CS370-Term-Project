#!/usr/bin/env bash
# Clean Raspberry Pi OS -> ready to build and run carwatch.
# This script is graded indirectly: if the TA can't boot it, the TA can't grade it.
set -euo pipefail

echo "== packages =="
sudo apt-get update
sudo apt-get install -y build-essential can-utils valgrind git

echo "== SPI + MCP2515 overlay =="
# TODO(M2): append to /boot/firmware/config.txt, with the CORRECT crystal frequency:
#   dtparam=spi=on
#   dtoverlay=mcp2515-can0,oscillator=<8000000|16000000>,interrupt=25
#   dtoverlay=spi0-hw-cs
# The crystal value is on the module's can and a wrong one produces a silently dead
# bus. See docs/hardware/wiring.md.

echo "== bring up can0 =="
# TODO(M2): sudo ip link set can0 up type can bitrate 500000
#           verify with: candump can0

echo "not implemented — M0 scaffold" >&2
exit 1
