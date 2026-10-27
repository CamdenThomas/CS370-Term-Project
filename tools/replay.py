#!/usr/bin/env python3
"""Replay a labeled capture through the real pipeline.

Faithfully preserving inter-sample timing is the engineering here — a replay that
collapses the gaps is not testing the system that will run in the car.

Everything emitted is labeled `replay`. Not optional (CLAUDE.md §2.7).
"""
import argparse
import sys


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--fixture", required=True, help="path under fixtures/")
    ap.add_argument("--label", default="replay", choices=["replay", "synth"])
    ap.add_argument("--speed", type=float, default=1.0,
                    help="1.0 = real time; >1 compresses a month into an afternoon")
    args = ap.parse_args()

    print(f"replay: fixture={args.fixture} label={args.label} speed={args.speed}x",
          file=sys.stderr)
    # TODO(M3): read fixture records, preserve inter-sample deltas / args.speed,
    #           write into the same ring the live path uses, src = args.label.
    print("not implemented — M0 scaffold", file=sys.stderr)
    return 1


if __name__ == "__main__":
    sys.exit(main())
