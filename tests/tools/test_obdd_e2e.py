"""obdd end to end at a desk: tools/obdsim.py -> obdd -> ring -> storaged -> the log.

The C daemons as processes, against the fake OBD2 port, with no car. Every sample obdd
pushes must be in the log, labeled synth; the session, supported and static_pids events
must be there too, and a second ECU must be reported once (Camden, 2026-11-08).

Runs where make has built build/bin/obdd and build/bin/storaged (make test builds them
first); skips elsewhere, saying why. It uses the real ring name, so it must not run on a
box whose obdd is live.
"""
import os
import re
import signal
import struct
import subprocess
import sys
import tempfile
import time
import unittest
import zlib

import obdsim

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
# make sets CARWATCH_BIN (build/asan/bin under make asan); run by hand, build/bin.
BIN = os.environ.get("CARWATCH_BIN") or os.path.join(ROOT, "build", "bin")
BIN = BIN if os.path.isabs(BIN) else os.path.join(ROOT, BIN)
RING = "/dev/shm/carwatch-obd"           # RING_NAME_OBD (include/ipc/ring.h)

# include/store/record.h: magic, kind, body_len, seq, mono_ns, wall_ns, src, flags,
# reserved, boot_id; then the body; then a CRC32 of both.
HDR = struct.Struct("<IHHQQQBBHI")  # cspell:disable-line
REC_MAGIC, REC_SAMPLE, REC_EVENT = 0x31525743, 1, 6
SRC_SYNTH = 3

EV_SESSION, EV_STATIC, EV_MULTI, EV_SUPPORTED = 0x0201, 0x0202, 0x020C, 0x020E
RUN_SECONDS = 8


def records(path):
    """(kind, src, body) for every record in a segment, checking each CRC."""
    with open(path, "rb") as f:
        data = f.read()
    off = 0
    while off + HDR.size <= len(data):
        magic, kind, body_len, _seq, _mono, _wall, src, _fl, _res, _boot = \
            HDR.unpack_from(data, off)
        if magic != REC_MAGIC:
            raise AssertionError(f"bad magic at {off}")
        end = off + HDR.size + body_len
        (crc,) = struct.unpack_from("<I", data, end)
        if zlib.crc32(data[off:end]) != crc:
            raise AssertionError(f"bad CRC at {off}")
        yield kind, src, data[off + HDR.size:end]
        off = end + 4


@unittest.skipUnless(sys.platform.startswith("linux"), "the ring is POSIX shm on Linux")
@unittest.skipUnless(os.path.exists(os.path.join(BIN, "obdd")) and
                     os.path.exists(os.path.join(BIN, "storaged")),
                     f"{BIN}/obdd and storaged not built: run make first")
class ObddAgainstSim(unittest.TestCase):
    def setUp(self):
        self.dir = tempfile.TemporaryDirectory()
        d = self.dir.name
        self.link = os.path.join(d, "obd")
        self.sock = os.path.join(d, "ev.sock")
        self.logdir = os.path.join(d, "log")
        self.sim = obdsim.PtyServer(link=self.link, latency=0.04,
                                    two_ecu={0x05}).__enter__()
        self.children, self.files = [], []

    def tearDown(self):
        for p in self.children:
            if p.poll() is None:
                p.kill()
                p.wait()
        for f in self.files:
            f.close()
        self.sim.__exit__(None, None, None)
        if os.path.exists(RING):
            os.unlink(RING)
        self.dir.cleanup()

    def start(self, name, *args):
        out = open(os.path.join(self.dir.name, name + ".out"), "w")  # noqa: SIM115
        self.files.append(out)
        p = subprocess.Popen([os.path.join(BIN, name), *args], stdout=out,
                             stderr=subprocess.STDOUT)
        self.children.append(p)
        return p

    def stop(self, p):
        p.send_signal(signal.SIGTERM)
        return p.wait(timeout=20)

    def output(self, name):
        with open(os.path.join(self.dir.name, name + ".out")) as f:
            return f.read()

    def test_every_pushed_sample_and_the_startup_events_reach_the_log(self):
        obdd = self.start("obdd", "--port", self.link, "--create-ring",
                          "--events", self.sock)
        for _ in range(100):                      # obdd makes the ring (bench mode)
            if os.path.exists(RING):
                break
            time.sleep(0.05)
        self.assertTrue(os.path.exists(RING), "obdd created the ring")
        # storaged starts after obdd, so obdd's first events wait in its queue.
        storaged = self.start("storaged", "--dir", self.logdir, "--src", "synth",
                              "--events", self.sock)
        time.sleep(RUN_SECONDS)

        self.assertEqual(self.stop(obdd), 0, "obdd stops cleanly on SIGTERM")
        time.sleep(1.5)                           # one batch: storaged drains the rest
        self.assertEqual(self.stop(storaged), 0, "storaged stops cleanly")

        m = re.search(r"stop samples=(\d+) ring_full=(\d+) events_sent=(\d+) "
                      r"events_dropped=(\d+)", self.output("obdd"))
        self.assertIsNotNone(m, "obdd logged its stop line")
        pushed, ring_full, _sent, dropped = map(int, m.groups())
        self.assertEqual((ring_full, dropped), (0, 0), "nothing refused, nothing dropped")

        recs = []
        for name in sorted(os.listdir(self.logdir)):
            if name.endswith(".cwr"):
                recs += list(records(os.path.join(self.logdir, name)))
        samples = [(src, body) for kind, src, body in recs if kind == REC_SAMPLE]
        events = {}
        for kind, src, body in recs:
            if kind == REC_EVENT:
                code, n = struct.unpack_from("<HH", body)
                events.setdefault(code, []).append((src, body[4:4 + n].decode("ascii")))

        rate = pushed / RUN_SECONDS
        print(f"\n  obdd pushed {pushed} samples in {RUN_SECONDS} s ({rate:.1f}/s), "
              f"log holds {len(samples)}", file=sys.stderr)
        self.assertEqual(len(samples), pushed, "every pushed sample is in the log once")
        self.assertGreater(rate, 10.0, "obdsim answers about 20 a second")
        self.assertTrue(all(src == SRC_SYNTH for src, _ in samples),
                        "obdsim's banner says SIM: every sample is synth")

        coolant = [struct.unpack_from("<BBB5sf", b) for _, b in samples if b[1] == 0x05]
        self.assertTrue(coolant, "coolant was polled")
        self.assertTrue(all(-40.0 <= c[4] <= 215.0 for c in coolant), "coolant decoded")

        for code, what in ((EV_SESSION, "session"), (EV_SUPPORTED, "supported"),
                           (EV_STATIC, "static_pids"), (EV_MULTI, "multi_ecu")):
            self.assertIn(code, events, f"a {what} event is in the log")
        self.assertTrue(all(src == SRC_SYNTH for evs in events.values() for src, _ in evs),
                        "events keep obdd's label")
        session = events[EV_SESSION][0][1]
        self.assertIn("vin_hash=bb984ebda934", session, "the same hash as obdlib.py")
        self.assertIn("carwatch SIM", session)
        self.assertEqual(events[EV_STATIC][0][1], "none", "obdsim has no static PIDs")
        self.assertIn("n=17", events[EV_SUPPORTED][0][1])
        multi = events[EV_MULTI]
        self.assertEqual(len(multi), 1, "a second ECU is reported once per connection")
        self.assertIn("pid=05 n_ecu=2", multi[0][1])


if __name__ == "__main__":
    unittest.main()
