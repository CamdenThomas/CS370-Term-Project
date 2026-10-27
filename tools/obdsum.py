#!/usr/bin/env python3
"""Summarize drive logs from obdlog: session length, rates, gaps and events per PID.

    tools/obdsum.py data/drive-*.jsonl

This is the first evidence for `firstcap`: how many replies each PID gave, how often,
the longest gap, and what went wrong.
"""
import argparse
import statistics
import sys

import obdlib
import obdlog

# Per-PID events that are failed requests; pid_absent and pid_present are only markers.
FAILURES = ("no_data", "timeout", "adapter_error")


def summarize(records):
    srcs = {r["src"] for r in records}
    if len(srcs) != 1:
        raise ValueError(f"one session must have one src, found {sorted(srcs)}")
    header = next((r for r in records if r["type"] == "header"), {})
    footer = next((r for r in records if r["type"] == "footer"), None)
    samples = [r for r in records if r["type"] == "sample"]
    t = [r["t_mono"] for r in records if r["type"] in ("sample", "event")]
    seconds = (max(t) - min(t)) if len(t) > 1 else 0.0

    pids, events = {}, {}
    for r in samples:
        p = pids.setdefault(r["pid"], {"vals": [], "times": [], "unit": r.get("unit", ""),
                                       "no_data": 0, "errors": 0})
        p["times"].append(r["t_mono"])
        if r.get("val") is not None:
            p["vals"].append(r["val"])
    for r in records:
        if r["type"] != "event":
            continue
        events[r["event"]] = events.get(r["event"], 0) + 1
        if "pid" in r and r["event"] in FAILURES:
            p = pids.setdefault(r["pid"], {"vals": [], "times": [], "unit": "",
                                           "no_data": 0, "errors": 0})
            p["no_data" if r["event"] == "no_data" else "errors"] += 1

    out = {}
    for key, p in sorted(pids.items()):
        times, vals = p["times"], p["vals"]
        gaps = [b - a for a, b in zip(times, times[1:])]
        out[key] = {
            "name": obdlib.pid_name(int(key, 16)), "unit": p["unit"], "n": len(times),
            "hz": len(times) / seconds if seconds else 0.0,
            "min": min(vals) if vals else None, "max": max(vals) if vals else None,
            "mean": statistics.fmean(vals) if vals else None,
            "max_gap": max(gaps) if gaps else 0.0,
            "no_data": p["no_data"], "errors": p["errors"],
        }
    return {
        "src": srcs.pop(), "adapter": header.get("adapter"),
        "protocol": header.get("protocol"), "vin_hash": header.get("vin_hash"),
        "seconds": seconds, "samples": len(samples),
        "rate": len(samples) / seconds if seconds else 0.0,
        "gps": sum(1 for r in records if r["type"] == "gps"),
        "ended": footer["reason"] if footer else "no footer (session did not end cleanly)",
        "events": events, "pids": out,
    }


def _f(v, width=8):
    return f"{v:{width}.2f}" if v is not None else " " * (width - 1) + "-"


def render(s, path, torn=False):
    lines = [f"== {path}"]
    if s["src"] != "live":
        lines.append(f"!! {s['src'].upper()} DATA: not from a real car")
    lines += [
        f"src={s['src']}  adapter={s['adapter']}  protocol={s['protocol']}  "
        f"vin_hash={s['vin_hash']}",
        f"{s['seconds']:.1f} s, {s['samples']} samples, {s['rate']:.1f} samples/s, "
        f"{s['gps']} gps fixes, ended: {s['ended']}" + (", torn last line" if torn else ""),
        "",
        f"{'PID':4} {'signal':32} {'n':>6} {'Hz':>6} {'min':>8} {'mean':>8} {'max':>8} "
        f"{'unit':6} {'gap s':>6} {'nodata':>6} {'errors':>6}",
    ]
    for key, p in s["pids"].items():
        lines.append(f"{key:4} {p['name'][:32]:32} {p['n']:6d} {p['hz']:6.2f} "
                     f"{_f(p['min'])} {_f(p['mean'])} {_f(p['max'])} {p['unit']:6} "
                     f"{p['max_gap']:6.2f} {p['no_data']:6d} {p['errors']:6d}")
    lines.append("")
    lines.append("events: " + (", ".join(f"{k}={v}" for k, v in sorted(s["events"].items()))
                               or "none"))
    return "\n".join(lines) + "\n"


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("logs", nargs="+")
    args = ap.parse_args(argv)
    for path in args.logs:
        records, torn = obdlog.read_log(path)
        print(render(summarize(records), path, torn))
    return 0


if __name__ == "__main__":
    sys.exit(main())
