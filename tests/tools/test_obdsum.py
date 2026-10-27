"""obdsum: per-session and per-PID summary of a drive log."""
import os
import unittest

import obdlog
import obdsum

FIXTURE = os.path.join(os.path.dirname(__file__), "..", "..", "fixtures", "synth",
                       "drive-cycle-synth.jsonl")


class Summary(unittest.TestCase):
    def setUp(self):
        self.records, _ = obdlog.read_log(FIXTURE)
        self.s = obdsum.summarize(self.records)

    def test_session_fields(self):
        self.assertEqual(self.s["src"], "synth")
        self.assertGreater(self.s["seconds"], 15)
        self.assertGreater(self.s["rate"], 5)

    def test_per_pid_counts_add_up(self):
        n = sum(1 for r in self.records if r["type"] == "sample")
        self.assertEqual(sum(p["n"] for p in self.s["pids"].values()), n)
        self.assertEqual(self.s["samples"], n)

    def test_per_pid_stats(self):
        rpm = self.s["pids"]["0C"]
        self.assertLessEqual(rpm["min"], rpm["mean"])
        self.assertLessEqual(rpm["mean"], rpm["max"])
        self.assertGreater(rpm["hz"], 0)
        self.assertGreater(rpm["max_gap"], 0)

    def test_events_are_counted_per_pid(self):
        recs = self.records[:3] + [
            {"type": "event", "src": "synth", "seq": 99, "t_mono": 0, "t_wall": "",
             "event": "no_data", "pid": "0C"}]
        s = obdsum.summarize(recs)
        self.assertEqual(s["events"]["no_data"], 1)
        self.assertEqual(s["pids"]["0C"]["no_data"], 1)

    def test_render_warns_about_synthetic_data(self):
        text = obdsum.render(self.s, FIXTURE)
        self.assertIn("SYNTH", text)
        self.assertIn("Engine speed", text)

    def test_mixed_src_is_refused(self):
        recs = list(self.records)
        recs[5] = dict(recs[5], src="live")
        with self.assertRaises(ValueError):
            obdsum.summarize(recs)


if __name__ == "__main__":
    unittest.main()
