#!/usr/bin/env bash
# Fault injection for the soak and for demo-day rehearsal.
#
# Demo day WILL unplug one of our sensors at a moment of their choosing. This is
# announced in the handout so that nobody can call it unfair later. Rehearse it.
set -euo pipefail

case "${1:-}" in
  kill)    echo "TODO(M3): kill -9 a named child; supervisor must detect and restart" ;;
  unplug)  echo "TODO(M4): simulate 10-minute sensor absence; must DEGRADE, not storm" ;;
  powercut) echo "TODO(M4): cut power mid-write; storaged must recover the torn tail" ;;
  diskfull) echo "TODO(M4): fill the volume; must degrade and log, not crash" ;;
  *) echo "usage: $0 {kill|unplug|powercut|diskfull}" >&2; exit 2 ;;
esac
exit 1
