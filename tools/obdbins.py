#!/usr/bin/env python3
"""Operating-point binning, a first look at the baseline idea (CLAUDE.md 5, DECISIONS D).

    tools/obdbins.py data/drive-*.jsonl --pids 05,06,07,0B,10

Each reading of a PID is tagged with the engine's operating point at that moment: the
latest rpm (0C) and load (04) within --max-age seconds. Readings are grouped into
rpm x load cells, and each cell's count, mean and spread is printed. A cell whose spread
is small is a usable baseline; a thin cell (fewer than --min-n readings, marked *) is not
yet. This is a prototype in tools/ to look at data, not the product's analysis, which
is C (CLAUDE.md 2.4).
"""
import argparse
import statistics
import sys

import obdlib
import obdlog

RPM, LOAD = "0C", "04"
CELL = 28   # characters per table cell


def snapshots(records, max_age):
    """[{'pid', 'val', 'rpm', 'load', 't'}] for every reading with a fresh operating point."""
    last = {}
    out = []
    for r in sorted((r for r in records if r["type"] == "sample" and r.get("val") is not None),
                    key=lambda r: r["t_mono"]):
        last[r["pid"]] = (r["t_mono"], r["val"])
        if r["pid"] in (RPM, LOAD):
            continue
        rpm, load = last.get(RPM), last.get(LOAD)
        if rpm is None or load is None:
            continue
        if r["t_mono"] - rpm[0] > max_age or r["t_mono"] - load[0] > max_age:
            continue
        out.append({"pid": r["pid"], "val": r["val"], "rpm": rpm[1], "load": load[1],
                    "t": r["t_mono"]})
    return out


def bin_pid(snaps, pid, rpm_step, load_step):
    """{(rpm_lo, load_lo): {'n', 'mean', 'std', 'min', 'max'}} for one PID."""
    groups = {}
    for s in snaps:
        if s["pid"] != pid:
            continue
        key = (int(s["rpm"] // rpm_step * rpm_step), int(s["load"] // load_step * load_step))
        groups.setdefault(key, []).append(s["val"])
    return {k: {"n": len(v), "mean": statistics.fmean(v),
                "std": statistics.pstdev(v), "min": min(v), "max": max(v)}
            for k, v in groups.items()}


def render(records, path, pids, rpm_step, load_step, min_n, max_age=1.5):
    src = records[0]["src"] if records else "?"
    snaps = snapshots(records, max_age)
    lines = [f"== {path}"]
    if src != "live":
        lines.append(f"!! {src.upper()} DATA: not from a real car")
    lines.append(f"cells: rpm step {rpm_step}, load step {load_step}%; "
                 f"* = fewer than {min_n} readings")
    for pid in pids:
        cells = bin_pid(snaps, pid, rpm_step, load_step)
        name = obdlib.pid_name(int(pid, 16))
        if not cells:
            lines += ["", f"{pid} {name}: no readings with a fresh operating point"]
            continue
        loads = sorted({k[1] for k in cells})
        rpms = sorted({k[0] for k in cells})
        corner = "rpm \\ load %"
        lines += ["", f"{pid} {name}: mean +/- std, n per cell",
                  f"{corner:>13}  " + " ".join(f"{lo}-{lo + load_step}".center(CELL)
                                               for lo in loads)]
        for r in rpms:
            row = [f"{r:>5}-{r + rpm_step:<6} "]
            for lo in loads:
                c = cells.get((r, lo))
                if c is None:
                    row.append(" " * CELL)
                else:
                    mark = "*" if c["n"] < min_n else " "
                    row.append(f"{c['mean']:8.2f} +/-{c['std']:6.2f} n={c['n']:<4d}{mark}"
                               .ljust(CELL))
            lines.append(" ".join(row))
    return "\n".join(lines) + "\n"


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("logs", nargs="+")
    ap.add_argument("--pids", default=None, help="hex PIDs to bin; default: all but 0C, 04")
    ap.add_argument("--rpm-step", type=int, default=500)
    ap.add_argument("--load-step", type=int, default=20)
    ap.add_argument("--min-n", type=int, default=30)
    ap.add_argument("--max-age", type=float, default=1.5)
    args = ap.parse_args(argv)
    for path in args.logs:
        records, _ = obdlog.read_log(path)
        pids = (args.pids.split(",") if args.pids else
                sorted({r["pid"] for r in records if r["type"] == "sample"} - {RPM, LOAD}))
        print(render(records, path, [p.upper() for p in pids], args.rpm_step,
                     args.load_step, args.min_n, args.max_age))
    return 0


if __name__ == "__main__":
    sys.exit(main())
