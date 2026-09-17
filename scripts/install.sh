#!/usr/bin/env bash
# Install the supervisor as a systemd unit so the device comes back after a power cut.
# The car cuts power without warning; coming back up unattended is the product.
set -euo pipefail
# TODO(M3): install build/bin/* to /usr/local/bin, write carwatch.service with
#           Restart=always, enable it, and verify a heartbeat appears within 60s.
echo "not implemented — M0 scaffold" >&2
exit 1
