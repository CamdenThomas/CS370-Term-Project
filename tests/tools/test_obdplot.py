"""obdplot: PNG figures from a drive log."""
import json
import os
import tempfile
import unittest

import obdlog
import obdplot

FIXTURE = os.path.join(os.path.dirname(__file__), "..", "..", "fixtures", "synth",
                       "drive-cycle-synth.jsonl")


def write_log(d, records):
    path = os.path.join(d, "drive-x-synth.jsonl")
    with open(path, "w", encoding="utf-8") as f:
        for r in records:
            f.write(json.dumps(dict({"src": "synth"}, **r)) + "\n")
    return path


class Plots(unittest.TestCase):
    def test_writes_every_figure(self):
        with tempfile.TemporaryDirectory() as d:
            written = obdplot.plot_session(FIXTURE, d)
            names = {os.path.basename(p) for p in written}
            for want in ("overview.png", "speed-vs-rpm.png", "intervals.png", "pid-0C.png"):
                self.assertIn(want, names)
            for p in written:
                self.assertGreater(os.path.getsize(p), 1000, p)

    def test_a_drive_with_no_decoded_sample_still_plots(self):
        with tempfile.TemporaryDirectory() as d:
            path = write_log(d, [
                {"type": "header", "seq": 0, "t_mono": 1.0},
                {"type": "event", "seq": 1, "t_mono": 1.1, "event": "no_data", "pid": "0C"},
                {"type": "footer", "seq": 2, "t_mono": 1.2, "reason": "sigint"}])
            written = obdplot.plot_session(path, os.path.join(d, "out"))
            self.assertEqual([os.path.basename(p) for p in written],
                             ["overview.png", "intervals.png"])

    def test_series_and_events_share_one_time_zero(self):
        # PID 01 does not decode, and on a real car it is the first PID asked.
        records = [{"type": "sample", "t_mono": 10.0, "pid": "01", "val": None},
                   {"type": "sample", "t_mono": 11.0, "pid": "0C", "val": 800.0},
                   {"type": "event", "t_mono": 12.0, "event": "link_lost"}]
        self.assertEqual(obdplot.start_time(records), 10.0)
        self.assertEqual(obdplot.series(records)["0C"], ([1.0], [800.0]))

    def test_title_marks_synthetic_data(self):
        records, _ = obdlog.read_log(FIXTURE)
        self.assertIn("SYNTH", obdplot.title_for(records, "x.jsonl"))

    def test_series_are_relative_seconds(self):
        records, _ = obdlog.read_log(FIXTURE)
        series = obdplot.series(records)
        t, v = series["0C"]
        self.assertEqual(min(min(times) for times, _ in series.values()), 0.0)
        self.assertEqual(len(t), len(v))
        self.assertEqual(t, sorted(t))


if __name__ == "__main__":
    unittest.main()
