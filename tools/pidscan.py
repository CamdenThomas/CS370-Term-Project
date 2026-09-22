#!/usr/bin/env python3
"""Mode 01 supported-PID bitmap scan. Closes D-007.

Run this on the CR-V before M2. Ten minutes through the USB OBD2 adapter
(/dev/obd, D-015). The answer decides whether diagnostic #1 (oil pressure) is
implementable at all, and that is the project's named risk.

Record results in docs/hardware/pid-survey.md.
"""
import sys

# PIDs 0x00/0x20/0x40/0x60 each return a 32-bit bitmap of support for the next block.
SUPPORT_PIDS = [0x00, 0x20, 0x40, 0x60]


def main() -> int:
    print("not implemented — M0 scaffold", file=sys.stderr)
    print("Manual fallback (works today, no code):", file=sys.stderr)
    print("  screen /dev/obd <baud>: ATZ, ATE0, ATSP0, then 0100 / 0120 / 0140 / 0160.",
          file=sys.stderr)
    print("  Decode the bitmaps and fill in docs/hardware/pid-survey.md.", file=sys.stderr)
    return 1


if __name__ == "__main__":
    sys.exit(main())
