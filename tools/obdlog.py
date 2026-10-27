#!/usr/bin/env python3
"""The drive logger: poll every supported Mode 01 PID and append each reply to JSONL.

    tools/obdlog.py --port /dev/cu.OBDLinkLX-SPP        # Mac, a real drive
    tools/obdlog.py --sim                               # rehearsal against obdsim

One file per session, data/drive-<time>-<src>.jsonl, one JSON object per line:

    header  adapter, protocol, battery, VIN hash, PID list, tool and schema version
    sample  src, seq, t_mono, t_wall, pid, raw hex, decoded val, unit
    event   no_data, timeout, adapter_error, reinit, pid_absent, link_lost,
            reconnected, gps_unavailable, gps_error, ...
    gps     a Mac location fix, only with --gps
    footer  why the session ended, and its counts

Every line carries `src`: `live` for the car, `synth` when the adapter is obdsim
(CLAUDE.md 2.7). Writes are append-only and fsync'd every --fsync-lines lines or
--fsync-sec seconds, so a crash or a closed lid loses at most that much. A Bluetooth
drop is logged and retried with backoff; the logger only stops on Ctrl-C, SIGTERM or
--max-seconds. This is a tool for collecting data (D-033), not the product's obdd.
"""
import argparse
import datetime
import json
import os
import signal
import socket
import sys
import time

import elm
import obdlib

TOOL = "obdlog 1"
SCHEMA = 1
NO_DATA_LIMIT = 5        # consecutive NO DATA before a PID is set aside as absent
ABSENT_RETRY_SEC = 60.0  # how often an absent PID is tried again
ERROR_LIMIT = 3          # consecutive timeouts or adapter errors before a reset


def _wall():
    return datetime.datetime.now(datetime.timezone.utc).isoformat(timespec="milliseconds")


class LogWriter:
    """Append-only JSONL with periodic fsync. Every line gets src, seq and timestamps."""

    def __init__(self, path, src, fsync_lines, fsync_sec):
        self.path, self.src = path, src
        self.fsync_lines, self.fsync_sec = fsync_lines, fsync_sec
        self.f = open(path, "a", encoding="utf-8")
        self.seq = 0
        self.unsynced = 0
        self.last_sync = time.monotonic()
        dfd = os.open(os.path.dirname(os.path.abspath(path)), os.O_RDONLY)
        try:
            os.fsync(dfd)        # the new file's directory entry survives a crash too
        finally:
            os.close(dfd)

    def write(self, rtype, **fields):
        rec = {"type": rtype, "src": self.src, "seq": self.seq,
               "t_mono": round(time.monotonic(), 4), "t_wall": _wall()}
        rec.update(fields)
        self.f.write(json.dumps(rec, separators=(",", ":")) + "\n")
        self.seq += 1
        self.unsynced += 1
        if (self.unsynced >= self.fsync_lines
                or time.monotonic() - self.last_sync >= self.fsync_sec):
            self.sync()

    def sync(self):
        self.f.flush()
        os.fsync(self.f.fileno())
        self.unsynced = 0
        self.last_sync = time.monotonic()

    def close(self):
        self.sync()
        self.f.close()


def read_log(path):
    """(records, torn): every complete line, and whether a torn last line was dropped."""
    records, torn = [], False
    with open(path, encoding="utf-8") as f:
        lines = f.read().split("\n")
    for i, line in enumerate(lines):
        if not line:
            continue
        try:
            records.append(json.loads(line))
        except json.JSONDecodeError:
            if i == len(lines) - 1:
                torn = True       # the last line was mid-write when the session ended
            else:
                raise
    return records, torn


class Stop:
    def __init__(self):
        self.reason = None

    def __call__(self, signum, _frame):
        self.reason = "sigint" if signum == signal.SIGINT else "sigterm"


def connect(path, tries, backoff_min, stop, log=None):
    """Open and init the adapter, retrying with exponential backoff. None if stopped."""
    delay, attempt = backoff_min, 0
    while stop.reason is None and (tries <= 0 or attempt < tries):
        attempt += 1
        ad = None
        try:
            ad = elm.Adapter(path)
            ad.init()
            return ad, attempt
        except (elm.LinkLost, obdlib.AdapterError) as e:
            if ad:
                ad.close()
            if log:
                log("connect_failed", attempt=attempt, detail=str(e))
            print(f"obdlog: connect attempt {attempt} failed: {e}", file=sys.stderr)
            if tries > 0 and attempt >= tries:
                break
            end = time.monotonic() + delay
            while stop.reason is None and time.monotonic() < end:
                time.sleep(0.05)
            delay = min(delay * 2, 30.0)
    return None, attempt


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--port", help="adapter serial port (/dev/cu.* on a Mac, /dev/obd)")
    ap.add_argument("--sim", action="store_true", help="start obdsim and log from it")
    ap.add_argument("--out", default="data", help="directory for session files")
    ap.add_argument("--pids", help="comma-separated hex PIDs; default: all supported")
    ap.add_argument("--gps", action="store_true", help="also log Mac location (optional)")
    ap.add_argument("--fsync-lines", type=int, default=50)
    ap.add_argument("--fsync-sec", type=float, default=1.0)
    ap.add_argument("--status-sec", type=float, default=5.0)
    ap.add_argument("--max-seconds", type=float, default=0, help="stop after this long")
    ap.add_argument("--connect-tries", type=int, default=0,
                    help="attempts at the first connection; 0 = keep trying")
    ap.add_argument("--backoff-min", type=float, default=1.0, help=argparse.SUPPRESS)
    args = ap.parse_args(argv)
    if bool(args.port) == bool(args.sim):
        ap.error("give exactly one of --port or --sim")
    os.makedirs(args.out, exist_ok=True)

    stop = Stop()
    old = {s: signal.getsignal(s) for s in (signal.SIGINT, signal.SIGTERM)}
    if threading_is_main():
        for s in old:
            signal.signal(s, stop)
    sim = None
    try:
        if args.sim:
            import obdsim  # noqa: PLC0415 - only needed for rehearsals
            sim = obdsim.PtyServer(link=os.path.join(args.out, "obd-sim")).__enter__()
            args.port = sim.link
        return _run(args, stop)
    finally:
        if sim:
            sim.__exit__(None, None, None)
        if threading_is_main():
            for s, h in old.items():
                signal.signal(s, h)


def threading_is_main():
    import threading  # noqa: PLC0415
    return threading.current_thread() is threading.main_thread()


def _run(args, stop):
    t_start = time.monotonic()
    deadline = t_start + args.max_seconds if args.max_seconds else None

    def expired():
        return deadline is not None and time.monotonic() >= deadline

    ad, _ = connect(args.port, args.connect_tries or 0, args.backoff_min, stop)
    if ad is None:
        print(f"obdlog: could not connect to {args.port}", file=sys.stderr)
        return 2

    src = "synth" if (ad.is_sim or args.sim) else "live"
    try:
        supported = ad.supported_pids()
        vin = None
        try:
            vin = ad.vin()
        except obdlib.AdapterError:
            pass
        header = {"tool": TOOL, "schema": SCHEMA, "host": socket.gethostname(),
                  "port": args.port, "adapter": ad.banner, "protocol": ad.protocol(),
                  "voltage": ad.voltage(),
                  "vin_hash": obdlib.vin_hash(vin) if vin else None,
                  "supported": [f"{p:02X}" for p in sorted(supported)]}
    except (elm.LinkLost, obdlib.AdapterError) as e:
        ad.close()
        print(f"obdlog: session setup failed: {e}", file=sys.stderr)
        return 2
    pids = ([int(p, 16) for p in args.pids.split(",")] if args.pids
            else sorted(supported))
    header["pids"] = [f"{p:02X}" for p in pids]

    stamp = _wall()[:19].replace(":", "").replace("-", "")
    path = os.path.join(args.out, f"drive-{stamp}-{src}.jsonl")
    out = LogWriter(path, src, args.fsync_lines, args.fsync_sec)
    out.write("header", **header)

    counts = {"samples": 0, "events": 0, "gps": 0}
    last_event = ["none"]

    def event(name, **fields):
        counts["events"] += 1
        last_event[0] = name + (f" {fields['pid']}" if "pid" in fields else "")
        out.write("event", event=name, **fields)

    gps = None
    if args.gps:
        import macgps  # noqa: PLC0415 - optional
        try:
            gps = macgps.start()
        except macgps.GpsUnavailable as e:
            event("gps_unavailable", detail=str(e))

    no_data = {p: 0 for p in pids}
    absent = {}                       # pid -> monotonic time it was set aside
    errors = 0
    next_status = time.monotonic() + args.status_sec
    print(f"obdlog: logging {len(pids)} PIDs to {path} (src={src}); Ctrl-C to stop",
          file=sys.stderr)

    def status():
        nonlocal next_status
        if time.monotonic() < next_status:
            return
        el = time.monotonic() - t_start
        print(f"[{int(el) // 60:02d}:{int(el) % 60:02d}] "
              f"{counts['samples']} samples, "
              f"{counts['samples'] / el:.1f}/s, {counts['events']} events "
              f"(last: {last_event[0]}), gps {counts['gps']}",
              file=sys.stderr)
        next_status += args.status_sec

    try:
        while stop.reason is None and not expired():
            now = time.monotonic()
            if all(p in absent and now - absent[p] < ABSENT_RETRY_SEC for p in pids):
                # Every PID is set aside (engine off, ECU quiet): wait for the next retry
                # instead of spinning through an empty pass.
                time.sleep(0.2)
                status()
                continue
            for pid in pids:
                if stop.reason is not None or expired():
                    break
                key = f"{pid:02X}"
                now = time.monotonic()
                if pid in absent:
                    if now - absent[pid] < ABSENT_RETRY_SEC:
                        continue
                    absent[pid] = now
                try:
                    data = ad.query(1, pid)
                except obdlib.NoData:
                    no_data[pid] += 1
                    event("no_data", pid=key)
                    if no_data[pid] == NO_DATA_LIMIT and pid not in absent:
                        absent[pid] = now
                        event("pid_absent", pid=key, after=NO_DATA_LIMIT)
                    continue
                except elm.LinkLost as e:
                    event("link_lost", detail=str(e))
                    ad.close()
                    ad, attempts = connect(args.port, 0, args.backoff_min, stop,
                                           log=lambda n, **f: event(n, **f))
                    if ad is None:
                        break
                    event("reconnected", attempts=attempts)
                    errors = 0
                    break
                except obdlib.AdapterError as e:
                    errors += 1
                    event("timeout" if isinstance(e, elm.Timeout) else "adapter_error",
                          pid=key, detail=str(e))
                    if errors >= ERROR_LIMIT:
                        errors = 0
                        try:
                            event("reinit", banner=ad.init())
                        except (elm.LinkLost, obdlib.AdapterError) as e2:
                            event("reinit_failed", detail=str(e2))
                    continue
                errors = 0
                no_data[pid] = 0
                if pid in absent:
                    del absent[pid]
                    event("pid_present", pid=key)
                value, unit = obdlib.decode(pid, data)
                out.write("sample", pid=key, raw=data.hex(" ").upper(), val=value,
                          unit=unit)
                counts["samples"] += 1

                if gps is not None:
                    fixes, err = gps.poll()
                    for fix in fixes:
                        out.write("gps", **fix)
                        counts["gps"] += 1
                    if err:
                        event("gps_error", detail=err)

                status()
            if ad is None:
                break
    finally:
        reason = stop.reason or ("max_seconds" if expired() else "link_gave_up")
        out.write("footer", reason=reason, seconds=round(time.monotonic() - t_start, 3),
                  **counts)
        out.close()
        if ad is not None:
            ad.close()
        if gps is not None:
            gps.stop()
    print(f"obdlog: {counts['samples']} samples, {counts['events']} events -> {path}",
          file=sys.stderr)
    return 0


if __name__ == "__main__":
    sys.exit(main())
