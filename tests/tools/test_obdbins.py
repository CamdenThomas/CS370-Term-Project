"""obdbins: operating-point binning prototype over a drive log."""
import os
import unittest

import obdbins
import obdlog

FIXTURE = os.path.join(os.path.dirname(__file__), "..", "..", "fixtures", "synth",
                       "drive-cycle-synth.jsonl")


class Binning(unittest.TestCase):
    def setUp(self):
        self.records, _ = obdlog.read_log(FIXTURE)

    def test_snapshots_carry_the_latest_rpm_and_load(self):
        snaps = obdbins.snapshots(self.records, max_age=1.5)
        self.assertTrue(snaps)
        for s in snaps[:20]:
            self.assertIn("rpm", s)
            self.assertIn("load", s)

    def test_stale_operating_point_is_not_used(self):
        recs = [
            {"type": "sample", "src": "synth", "t_mono": 0.0, "pid": "0C", "val": 800.0},
            {"type": "sample", "src": "synth", "t_mono": 0.1, "pid": "04", "val": 20.0},
            {"type": "sample", "src": "synth", "t_mono": 5.0, "pid": "05", "val": 80.0},
        ]
        self.assertEqual(obdbins.snapshots(recs, max_age=1.5), [])

    def test_idle_cell_is_the_best_populated(self):
        cells = obdbins.bin_pid(obdbins.snapshots(self.records, 1.5), "0B",
                                rpm_step=500, load_step=20)
        best = max(cells.items(), key=lambda kv: kv[1]["n"])[0]
        self.assertEqual(best[0], 500)           # 500-1000 rpm

    def test_stats_are_sane(self):
        cells = obdbins.bin_pid(obdbins.snapshots(self.records, 1.5), "0B", 500, 20)
        for c in cells.values():
            self.assertGreaterEqual(c["std"], 0)
            self.assertLessEqual(c["min"], c["mean"])
            self.assertLessEqual(c["mean"], c["max"])

    def test_map_rises_with_load(self):
        cells = obdbins.bin_pid(obdbins.snapshots(self.records, 1.5), "0B", 6000, 20)
        means = [cells[k]["mean"] for k in sorted(cells)]
        self.assertEqual(means, sorted(means))

    def test_render_flags_thin_bins_and_synthetic_data(self):
        text = obdbins.render(self.records, FIXTURE, ["0B"], 500, 20, min_n=10)
        self.assertIn("SYNTH", text)
        self.assertIn("*", text)


if __name__ == "__main__":
    unittest.main()
