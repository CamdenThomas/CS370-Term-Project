#!/usr/bin/env python3
"""The fake OBD2 port: an ELM327 emulator on a pty, driven by a scripted drive.

Everything it says is synthetic. Its reset banner contains "SIM", and obdlog labels any
session against it `src=synth` (CLAUDE.md 2.7). It is for development and tests, never a
data source for the soak (D-023).

    tools/obdsim.py --link data/obd-sim       # then point a tool at data/obd-sim
"""
import argparse
import math
import os
import select
import sys
import threading
import time
import tty

BANNER = "ELM327 v1.5 (carwatch SIM)"
PROTOCOL = "AUTO, ISO 15765-4 (CAN 11/500)"
SIM_VIN = "SIMCARWATCH000001"   # 17 characters, obviously not a real car

# The phases of one 140 s drive cycle, which repeats.
CYCLE = 140.0


def _speed(t):
    """km/h: idle 0-20 s, accelerate to 80 by 40 s, cruise to 80 s, brake to 0 by 100 s."""
    x = t % CYCLE
    if x < 20:
        return 0.0
    if x < 40:
        return 80 * (1 - math.cos(math.pi * (x - 20) / 20)) / 2
    if x < 80:
        return 80 + 3 * math.sin(2 * math.pi * (x - 40) / 20)
    if x < 100:
        return 80 * (1 + math.cos(math.pi * (x - 80) / 20)) / 2
    return 0.0


# km/h at which each upshift happens, and engine rpm per km/h in each gear.
_SHIFTS = (18, 35, 55, 75)
_RPM_PER_KMH = (100, 60, 42, 32, 26)


def _clamp(v, lo, hi):
    return max(lo, min(hi, v))


def vehicle_state(t):
    """Every modelled PID's value at t seconds after the engine started."""
    v = _speed(t)
    accel = _speed(t + 0.5) - _speed(t - 0.5)       # km/h per s
    gear = sum(v >= s for s in _SHIFTS)
    rpm = _clamp(v * _RPM_PER_KMH[gear], 780 + 15 * math.sin(t * 3.1), 5800)
    load = _clamp(20 + v * 0.15 + accel * 6, 8, 95)
    return {
        0x04: load,
        0x05: 25 + 65 * (1 - math.exp(-t / 240)),
        0x06: 3 * math.sin(2 * math.pi * t / 7) + 1.5 * math.sin(2 * math.pi * t / 2.3),
        0x07: 2.3 + 0.5 * math.sin(t / 300),
        0x0B: _clamp(28 + load * 0.72, 20, 101),
        0x0C: rpm,
        0x0D: v,
        0x0E: _clamp(8 + rpm / 400 - load / 20, -10, 50),
        0x0F: 28 + 10 * (1 - math.exp(-t / 400)),
        0x10: rpm * load / 6000,
        0x11: _clamp(load * 0.8 - 2, 0, 100),
        0x14: 0.45 + 0.4 * math.sin(2 * math.pi * t),
        0x15: 0.62 + 0.03 * math.sin(t / 5),
        0x1F: t,
        0x33: 99.0,
        0x42: 14.1 + 0.05 * math.sin(t / 3),
        0x46: 16.0,
    }


def _b(v):
    return bytes([int(_clamp(round(v), 0, 255))])


def _u16(v):
    return int(_clamp(round(v), 0, 65535)).to_bytes(2, "big")


# The inverse of obdlib's decode, per modelled PID.
ENCODE = {
    0x04: lambda v: _b(v * 255 / 100),
    0x05: lambda v: _b(v + 40),
    0x06: lambda v: _b(v * 128 / 100 + 128),
    0x07: lambda v: _b(v * 128 / 100 + 128),
    0x0B: _b,
    0x0C: lambda v: _u16(v * 4),
    0x0D: _b,
    0x0E: lambda v: _b((v + 64) * 2),
    0x0F: lambda v: _b(v + 40),
    0x10: lambda v: _u16(v * 100),
    0x11: lambda v: _b(v * 255 / 100),
    0x14: lambda v: _b(v * 200) + b"\x80",
    0x15: lambda v: _b(v * 200) + b"\x80",
    0x1F: _u16,
    0x33: _b,
    0x42: lambda v: _u16(v * 1000),
    0x46: lambda v: _b(v + 40),
}
MODELLED = sorted(ENCODE)


def _bitmap(base):
    """The support bitmap for the block after `base`, chaining to the next block."""
    bits = 0
    for pid in MODELLED:
        if base < pid < base + 0x20:
            bits |= 1 << (31 - (pid - base - 1))
    if any(pid > base + 0x20 for pid in MODELLED):
        bits |= 1   # base + 0x20: the next block exists
    return bits.to_bytes(4, "big")


def _hex(data):
    return " ".join(f"{x:02X}" for x in data) + " "


class Simulator:
    """ELM327 command handling, without any I/O. `handle` returns the full reply text."""

    def __init__(self):
        self._reset()

    def _reset(self):
        self.echo = True
        self.searched = False

    def handle(self, cmd, t):
        cmd = cmd.strip().upper().replace(" ", "")
        lines = self._answer(cmd, t)
        text = "\r".join(lines) + "\r\r>"
        return (cmd + "\r" + text) if self.echo else text

    def _answer(self, cmd, t):
        if cmd in ("ATZ", "ATWS"):
            self._reset()
            return ["", BANNER]
        if cmd == "ATI":
            return [BANNER]
        if cmd in ("ATE0", "ATE1"):
            self.echo = cmd == "ATE1"
            return ["OK"]
        if cmd == "ATDP":
            return [PROTOCOL]
        if cmd == "ATRV":
            return [f"{vehicle_state(t)[0x42] - 0.3:.1f}V"]
        if cmd.startswith("AT") and len(cmd) > 2:
            return ["OK"]
        if len(cmd) == 4 and all(c in "0123456789ABCDEF" for c in cmd):
            banner = [] if self.searched else ["SEARCHING..."]
            self.searched = True
            return banner + self._obd(int(cmd[:2], 16), int(cmd[2:], 16), t)
        return ["?"]

    def _obd(self, mode, pid, t):
        if mode == 0x01:
            if pid % 0x20 == 0 and (pid == 0 or pid in self._supported()):
                return [_hex(bytes([0x41, pid]) + _bitmap(pid))]
            if pid in ENCODE:
                return [_hex(bytes([0x41, pid]) + ENCODE[pid](vehicle_state(t)[pid]))]
            return ["NO DATA"]
        if mode == 0x09 and pid == 0x00:
            return [_hex(bytes([0x49, 0x00, 0x40, 0, 0, 0]))]
        if mode == 0x09 and pid == 0x02:
            body = bytes([0x49, 0x02, 0x01]) + SIM_VIN.encode("ascii")
            frames = [body[:6]] + [body[i:i + 7] for i in range(6, len(body), 7)]
            return [f"{len(body):03X}"] + [f"{i}: {_hex(f)}" for i, f in enumerate(frames)]
        return ["NO DATA"]

    def _supported(self):
        found = set()
        for base in (0x00, 0x20, 0x40):
            if base and base not in found:
                break
            bits = int.from_bytes(_bitmap(base), "big")
            found |= {base + 1 + i for i in range(32) if bits & (1 << (31 - i))}
        return found


class PtyServer:
    """Serve a Simulator on a pty, optionally behind a symlink (as /dev/obd is, D-019)."""

    def __init__(self, link=None, latency=0.04, speed=1.0):
        self.link, self.latency, self.speed = link, latency, speed
        self.sim = Simulator()
        self._stop = threading.Event()

    def __enter__(self):
        self.master, self.slave = os.openpty()
        tty.setraw(self.slave)
        self.path = os.ttyname(self.slave)
        if self.link:
            if os.path.lexists(self.link):
                os.unlink(self.link)
            os.symlink(self.path, self.link)
        self._t0 = time.monotonic()
        self._thread = threading.Thread(target=self._serve, daemon=True)
        self._thread.start()
        return self

    def __exit__(self, *exc):
        self._stop.set()
        self._thread.join(timeout=2)
        os.close(self.master)
        os.close(self.slave)
        if self.link and os.path.islink(self.link):
            os.unlink(self.link)

    def _serve(self):
        buf = b""
        while not self._stop.is_set():
            ready, _, _ = select.select([self.master], [], [], 0.05)
            if not ready:
                continue
            try:
                buf += os.read(self.master, 1024)
            except OSError:
                return
            while b"\r" in buf:
                line, buf = buf.split(b"\r", 1)
                cmd = line.replace(b"\n", b"").decode("ascii", errors="replace")
                if not cmd.strip():
                    continue
                if self.latency and not cmd.strip().upper().startswith("AT"):
                    time.sleep(self.latency)
                t = (time.monotonic() - self._t0) * self.speed
                try:
                    os.write(self.master, self.sim.handle(cmd, t).encode("ascii"))
                except OSError:
                    return


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--link", default="data/obd-sim", help="symlink to create to the pty")
    ap.add_argument("--latency", type=float, default=0.04,
                    help="seconds per OBD request, roughly a Bluetooth adapter")
    ap.add_argument("--speed", type=float, default=1.0, help="drive-profile time scale")
    args = ap.parse_args()
    os.makedirs(os.path.dirname(args.link) or ".", exist_ok=True)
    with PtyServer(args.link, args.latency, args.speed) as srv:
        print(f"obdsim: synthetic ELM327 on {srv.path} (link {args.link}); "
              "every byte is src=synth. Ctrl-C to stop.", file=sys.stderr)
        try:
            while True:
                time.sleep(1)
        except KeyboardInterrupt:
            pass
    return 0


if __name__ == "__main__":
    sys.exit(main())
