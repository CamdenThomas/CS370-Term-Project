"""obdview: one self-contained HTML page that scrubs through a drive."""
import json
import os
import re
import tempfile
import unittest

import obdview

FIXTURE = os.path.join(os.path.dirname(__file__), "..", "..", "fixtures", "synth",
                       "drive-cycle-synth.jsonl")


class Viewer(unittest.TestCase):
    def setUp(self):
        self.dir = tempfile.TemporaryDirectory()
        self.out = os.path.join(self.dir.name, "view.html")
        obdview.build(FIXTURE, self.out)
        with open(self.out, encoding="utf-8") as f:
            self.html = f.read()

    def tearDown(self):
        self.dir.cleanup()

    def test_page_is_self_contained(self):
        self.assertNotRegex(self.html, r"<script[^>]+src=")
        self.assertNotRegex(self.html, r"<link[^>]+href=")
        self.assertNotRegex(self.html, r"https?://")

    def test_embedded_data_parses_and_has_every_pid(self):
        m = re.search(r'<script id="drive" type="application/json">(.*?)</script>',
                      self.html, re.S)
        data = json.loads(m.group(1))
        self.assertEqual(data["src"], "synth")
        self.assertIn("0C", data["pids"])
        t, v = data["pids"]["0C"]["t"], data["pids"]["0C"]["v"]
        self.assertEqual(len(t), len(v))

    def test_synthetic_banner(self):
        self.assertIn("SYNTH DATA", self.html)


def embedded(html):
    m = re.search(r'<script id="drive" type="application/json">(.*?)</script>', html, re.S)
    return json.loads(m.group(1))


class TimeZero(unittest.TestCase):
    def test_an_event_lines_up_with_the_sample_it_happened_at(self):
        # PID 01 does not decode, and on a real car it is the first PID asked.
        records = [{"type": "header", "seq": 0, "t_mono": 9.0},
                   {"type": "sample", "seq": 1, "t_mono": 10.0, "pid": "01", "val": None},
                   {"type": "sample", "seq": 2, "t_mono": 11.0, "pid": "0C", "val": 800.0,
                    "unit": "rpm"},
                   {"type": "event", "seq": 3, "t_mono": 11.0, "event": "link_lost"}]
        with tempfile.TemporaryDirectory() as d:
            log = os.path.join(d, "drive-x-synth.jsonl")
            with open(log, "w", encoding="utf-8") as f:
                for r in records:
                    f.write(json.dumps(dict({"src": "synth"}, **r)) + "\n")
            out = os.path.join(d, "view.html")
            obdview.build(log, out)
            with open(out, encoding="utf-8") as f:
                data = embedded(f.read())
        self.assertEqual(data["pids"]["0C"]["t"], [1.0])
        self.assertEqual(data["events"], [{"t": 1.0, "event": "link_lost"}])


if __name__ == "__main__":
    unittest.main()
