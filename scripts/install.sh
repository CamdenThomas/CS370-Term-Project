#!/usr/bin/env bash
# Install the supervisor as a systemd unit so the device comes back at every key-on.
# Power disappears at every key-off (D-024); booting unattended at the next start is the
# product.
set -euo pipefail
# TODO(M3): install build/bin/* to /usr/local/bin, write carwatch.service with
#           Restart=always, enable it, and verify a heartbeat appears within 60s.
echo "not implemented — M0 scaffold" >&2
exit 1
