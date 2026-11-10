"""obdd --replay through storaged (scripts/replay.sh, make replay): a capture's samples
reach the log once each, with the label the rule gives them, the value obdlog decoded,
and the capture's timing divided by the speed (Camden, 2026-11-08: the replay is C).

Runs where make has built the daemons; skips elsewhere. Uses the real ring name, so it
must not run on a box whose obdd is live.
"""
import json
import os
import struct
import subprocess
import sys
import tempfile
import time
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from test_obdd_e2e import BIN, REC_EVENT, REC_SAMPLE, RING, ROOT, records  # noqa: E402

SYNTH = os.path.join(ROOT, "fixtures", "synth", "drive-cycle-synth.jsonl")
FORESTER = os.path.join(ROOT, "captures", "forester", "20261028T220659.957", "drive.jsonl")
SRC_REPLAY, SRC_SYNTH = 2, 3
EV_SESSION, EV_REPLAY = 0x0201, 0x020D


def capture(path):
    """(samples, first t_mono, last t_mono) of an obdlog capture."""
    with open(path) as f:
        rows = [json.loads(line) for line in f]
    samples = [r for r in rows if r.get("type") == "sample"]
    return samples, samples[0]["t_mono"], samples[-1]["t_mono"]


def f32(x):
    return struct.unpack("<f", struct.pack("<f", x))[0]


@unittest.skipUnless(sys.platform.startswith("linux"), "the ring is POSIX shm on Linux")
@unittest.skipUnless(os.path.exists(os.path.join(BIN, "obdd")) and
                     os.path.exists(os.path.join(BIN, "storaged")),
                     f"{BIN}/obdd and storaged not built: run make first")
class Replay(unittest.TestCase):
    def setUp(self):
        self.dir = tempfile.TemporaryDirectory()

    def tearDown(self):
        self.dir.cleanup()

    def replay(self, path, speed, out="log"):
        out = os.path.join(self.dir.name, out)
        env = dict(os.environ, CARWATCH_BIN=BIN)
        t = time.monotonic()
        r = subprocess.run([os.path.join(ROOT, "scripts", "replay.sh"), path, str(speed), out],
                           cwd=ROOT, env=env, capture_output=True, text=True, timeout=120)
        elapsed = time.monotonic() - t
        recs = []
        if os.path.isdir(out):
            for name in sorted(os.listdir(out)):
                if name.endswith(".cwr"):
                    recs += list(records(os.path.join(out, name)))
        return r, recs, elapsed

    def check(self, path, speed, label):
        want, t0, t1 = capture(path)
        r, recs, elapsed = self.replay(path, speed)
        self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
        samples = [(src, body) for kind, src, body in recs if kind == REC_SAMPLE]
        self.assertEqual(len(samples), len(want), "every sample of the capture, once")
        self.assertTrue(all(src == label for src, _ in samples), "the label the rule gives")

        # In order, with obdlog's value for every PID it decoded (NaN where it wrote null).
        for (_, body), row in zip(samples, want):
            _mode, pid, _n, _raw, value = struct.unpack("<BBB5sf", body)
            self.assertEqual(pid, int(row["pid"], 16))
            if row["val"] is None:
                self.assertNotEqual(value, value, f"PID {row['pid']} has no formula: NaN")
            else:
                self.assertEqual(value, f32(row["val"]), f"PID {row['pid']} {row['raw']}")

        codes = [struct.unpack_from("<H", b)[0] for k, _, b in recs if k == REC_EVENT]
        self.assertIn(EV_REPLAY, codes, "the replay says what it replayed")
        self.assertIn(EV_SESSION, codes, "and the recorded session")
        self.assertGreater(elapsed, (t1 - t0) / speed * 0.9, "the recorded spacing is kept")
        return elapsed

    def test_a_live_capture_comes_out_replay_with_the_logged_values(self):
        e = self.check(FORESTER, 50, SRC_REPLAY)
        print(f"\n  forester: 1113 samples, 53.8 s of drive at 50x in {e:.1f} s",
              file=sys.stderr)

    def test_a_synth_capture_stays_synth(self):
        self.check(SYNTH, 50, SRC_SYNTH)

    def test_an_unlabeled_capture_is_refused(self):
        bad = os.path.join(self.dir.name, "bad.jsonl")
        with open(bad, "w") as f:
            f.write('{"type":"header","seq":0}\n{"type":"sample","pid":"0C","raw":"0A 10",'
                    '"t_mono":1.0}\n')
        r, recs, _ = self.replay(bad, 1)
        self.assertNotEqual(r.returncode, 0)
        self.assertIn("refused", r.stderr)
        self.assertEqual([k for k, _, _ in recs if k == REC_SAMPLE], [])

        # obdd itself refuses it too, not only the script in front of it.
        sock = os.path.join(self.dir.name, "ev.sock")
        r = subprocess.run([os.path.join(BIN, "obdd"), "--replay", bad, "--create-ring",
                            "--events", sock], capture_output=True, text=True, timeout=30)
        if os.path.exists(RING):
            os.unlink(RING)
        self.assertEqual(r.returncode, 1, "exit 1: the capture is refused")
        self.assertIn("replay_refused", r.stdout)
        self.assertNotIn("replay_done", r.stdout)


if __name__ == "__main__":
    unittest.main()
