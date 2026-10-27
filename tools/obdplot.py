#!/usr/bin/env python3
"""PNG figures from obdlog drive logs, for looking at a drive before analyzing it.

    tools/obdplot.py data/drive-*.jsonl            # figures in data/plots/<log name>/

Per log: overview.png (every PID as its own small chart on one shared time axis),
pid-<PID>.png (one PID, full width), speed-vs-rpm.png (gears show as rays) and
intervals.png (time between replies, which shows adapter stalls). One series per
chart, so no legend and no second axis. Non-live data says so in every title.
"""
import argparse
import os
import sys

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt  # noqa: E402 - the backend must be chosen first

import obdlib  # noqa: E402
import obdlog  # noqa: E402

SERIES = "#2a78d6"      # one hue for every single-series chart
EVENT = "#d03b3b"       # reserved for events (link lost, timeouts), always labeled
INK, MUTED, GRID = "#0b0b0b", "#52514e", "#e4e3df"


def series(records):
    """{pid: (seconds since the first sample, values)} for decoded samples."""
    samples = [r for r in records if r["type"] == "sample" and r.get("val") is not None]
    if not samples:
        return {}
    t0 = min(r["t_mono"] for r in samples)
    out = {}
    for r in samples:
        t, v = out.setdefault(r["pid"], ([], []))
        t.append(r["t_mono"] - t0)
        v.append(r["val"])
    return out


def title_for(records, path):
    src = records[0]["src"] if records else "?"
    tag = "" if src == "live" else f"  [{src.upper()} DATA, not a real car]"
    return f"{os.path.basename(path)}{tag}"


def _style(ax):
    ax.grid(True, color=GRID, linewidth=0.6)
    ax.set_axisbelow(True)
    for side in ("top", "right"):
        ax.spines[side].set_visible(False)
    for side in ("left", "bottom"):
        ax.spines[side].set_color(MUTED)
    ax.tick_params(colors=MUTED, labelsize=8)


def _events(ax, records, t0):
    for r in records:
        if r["type"] == "event" and r["event"] in ("link_lost", "reconnected", "reinit"):
            ax.axvline(r["t_mono"] - t0, color=EVENT, linewidth=1, linestyle="--")


def plot_session(path, out_dir):
    records, _ = obdlog.read_log(path)
    data = series(records)
    os.makedirs(out_dir, exist_ok=True)
    title = title_for(records, path)
    t0 = min((r["t_mono"] for r in records if r["type"] == "sample"), default=0.0)
    unit = {r["pid"]: r.get("unit", "") for r in records if r["type"] == "sample"}
    written = []

    def save(fig, name):
        p = os.path.join(out_dir, name)
        fig.savefig(p, dpi=110, bbox_inches="tight")
        plt.close(fig)
        written.append(p)

    pids = sorted(data)
    fig, axes = plt.subplots(len(pids), 1, sharex=True, squeeze=False,
                             figsize=(11, 1.1 * len(pids) + 0.6), layout="constrained")
    for ax, pid in zip(axes[:, 0], pids):
        t, v = data[pid]
        ax.plot(t, v, color=SERIES, linewidth=1.2)
        _events(ax, records, t0)
        ax.set_ylabel(f"{pid}\n{unit[pid]}", rotation=0, ha="right", va="center",
                      fontsize=8, color=INK)
        ax.set_title(obdlib.pid_name(int(pid, 16)), loc="left", fontsize=8, color=MUTED,
                     pad=2)
        _style(ax)
    axes[-1, 0].set_xlabel("seconds since first sample", color=INK)
    fig.suptitle(title, x=0.01, ha="left", color=INK)
    save(fig, "overview.png")

    for pid in pids:
        t, v = data[pid]
        fig, ax = plt.subplots(figsize=(11, 3))
        ax.plot(t, v, color=SERIES, linewidth=1.2)
        _events(ax, records, t0)
        ax.set_title(f"{obdlib.pid_name(int(pid, 16))} (0x{pid}, {unit[pid]})  {title}",
                     loc="left", fontsize=9, color=INK)
        ax.set_xlabel("seconds since first sample", color=INK)
        _style(ax)
        save(fig, f"pid-{pid}.png")

    if "0C" in data and "0D" in data:
        # Pair each speed reading with the nearest rpm reading in time.
        rt, rv = data["0C"]
        pairs, j = [], 0
        for t, v in zip(*data["0D"]):
            while j + 1 < len(rt) and abs(rt[j + 1] - t) <= abs(rt[j] - t):
                j += 1
            pairs.append((v, rv[j]))
        fig, ax = plt.subplots(figsize=(6, 5))
        ax.scatter([p[0] for p in pairs], [p[1] for p in pairs], s=10, color=SERIES,
                   alpha=0.6, linewidths=0)
        ax.set_xlabel("vehicle speed, km/h", color=INK)
        ax.set_ylabel("engine speed, rpm", color=INK)
        ax.set_title(f"speed against rpm  {title}", loc="left", fontsize=9, color=INK)
        _style(ax)
        save(fig, "speed-vs-rpm.png")

    times = sorted(r["t_mono"] for r in records if r["type"] == "sample")
    gaps_ms = [(b - a) * 1000 for a, b in zip(times, times[1:])]
    fig, ax = plt.subplots(figsize=(7, 3.5))
    ax.hist(gaps_ms, bins=60, color=SERIES, edgecolor="white", linewidth=0.5)
    ax.set_xlabel("ms between consecutive replies (any PID)", color=INK)
    ax.set_ylabel("count", color=INK)
    ax.set_title(f"reply intervals  {title}", loc="left", fontsize=9, color=INK)
    _style(ax)
    save(fig, "intervals.png")
    return written


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("logs", nargs="+")
    ap.add_argument("--out", default=None, help="default: data/plots/<log name>/")
    args = ap.parse_args(argv)
    for path in args.logs:
        name = os.path.splitext(os.path.basename(path))[0]
        out = args.out or os.path.join("data", "plots", name)
        written = plot_session(path, out)
        print(f"{path}: {len(written)} figures in {out}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
