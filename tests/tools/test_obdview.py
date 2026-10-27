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


if __name__ == "__main__":
    unittest.main()
