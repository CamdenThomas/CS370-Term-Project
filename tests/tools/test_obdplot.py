"""obdplot: PNG figures from a drive log."""
import os
import tempfile
import unittest

import obdlog
import obdplot

FIXTURE = os.path.join(os.path.dirname(__file__), "..", "..", "fixtures", "synth",
                       "drive-cycle-synth.jsonl")


class Plots(unittest.TestCase):
    def test_writes_every_figure(self):
        with tempfile.TemporaryDirectory() as d:
            written = obdplot.plot_session(FIXTURE, d)
            names = {os.path.basename(p) for p in written}
            for want in ("overview.png", "speed-vs-rpm.png", "intervals.png", "pid-0C.png"):
                self.assertIn(want, names)
            for p in written:
                self.assertGreater(os.path.getsize(p), 1000, p)

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
