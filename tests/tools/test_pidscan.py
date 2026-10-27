"""pidscan: the supported-PID survey, run against obdsim."""
import contextlib
import io
import json
import os
import tempfile
import unittest

import obdlib
import obdsim
import pidscan


class ModelYear(unittest.TestCase):
    def test_digits_are_2001_to_2009(self):
        self.assertEqual(pidscan.model_year("JF2SG63688H700000", now=2026), 2008)

    def test_letters_take_the_latest_year_not_in_the_future(self):
        self.assertEqual(pidscan.model_year("5J6RM4H70FL000000", now=2026), 2015)
        self.assertEqual(pidscan.model_year("1HGCM82633A004352", now=2026), 2003)

    def test_unknown_code(self):
        self.assertIsNone(pidscan.model_year("SHORT", now=2026))


class ScanSim(unittest.TestCase):
    def test_scan_writes_json_and_markdown_and_labels_synth(self):
        with tempfile.TemporaryDirectory() as d:
            link = os.path.join(d, "obd")
            with obdsim.PtyServer(link=link, latency=0.0):
                with contextlib.redirect_stdout(io.StringIO()):
                    rc = pidscan.main(["--port", link, "--out", d])
            self.assertEqual(rc, 0)
            files = sorted(os.listdir(d))
            js = [f for f in files if f.endswith(".json")]
            md = [f for f in files if f.endswith(".md")]
            self.assertEqual(len(js), 1)
            self.assertEqual(len(md), 1)
            with open(os.path.join(d, js[0])) as f:
                result = json.load(f)
            self.assertEqual(result["src"], "synth")
            self.assertEqual(result["vin_hash"], obdlib.vin_hash(obdsim.SIM_VIN))
            self.assertEqual(sorted(int(p, 16) for p in result["supported"]),
                             obdsim.MODELLED)
            with open(os.path.join(d, js[0])) as f:
                self.assertNotIn(obdsim.SIM_VIN, f.read(), "the VIN never reaches a file")
            with open(os.path.join(d, md[0])) as f:
                text = f.read()
            self.assertIn("synth", text)
            self.assertIn("0C", text)
            self.assertNotIn(obdsim.SIM_VIN, text)

    def test_missing_port_fails_cleanly(self):
        with tempfile.TemporaryDirectory() as d:
            with contextlib.redirect_stderr(io.StringIO()):
                rc = pidscan.main(["--port", "/nonexistent/obd", "--out", d])
            self.assertEqual(rc, 2)


if __name__ == "__main__":
    unittest.main()
