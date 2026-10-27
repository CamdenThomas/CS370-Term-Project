"""obdlog: the drive logger, run against obdsim."""
import contextlib
import io
import json
import os
import resource
import tempfile
import threading
import time
import unittest
from unittest import mock

import elm
import macgps
import obdlog
import obdsim


def run_logger(argv):
    with contextlib.redirect_stderr(io.StringIO()):
        return obdlog.main(argv)


def only_log(d):
    logs = [f for f in os.listdir(d) if f.startswith("drive-") and f.endswith(".jsonl")]
    assert len(logs) == 1, logs
    return os.path.join(d, logs[0])


class AgainstSim(unittest.TestCase):
    def setUp(self):
        self.dir = tempfile.TemporaryDirectory()
        self.d = self.dir.name
        self.link = os.path.join(self.d, "obd")

    def tearDown(self):
        self.dir.cleanup()

    def test_session_has_header_samples_and_footer_all_labeled_synth(self):
        with obdsim.PtyServer(link=self.link, latency=0.0):
            rc = run_logger(["--port", self.link, "--out", self.d, "--max-seconds", "1.5"])
        self.assertEqual(rc, 0)
        records, torn = obdlog.read_log(only_log(self.d))
        self.assertFalse(torn)
        self.assertEqual(records[0]["type"], "header")
        self.assertEqual(records[-1]["type"], "footer")
        self.assertEqual(records[-1]["reason"], "max_seconds")
        self.assertTrue(all(r["src"] == "synth" for r in records))
        samples = [r for r in records if r["type"] == "sample"]
        self.assertGreater(len(samples), 20)
        self.assertEqual({s["pid"] for s in samples},
                         {f"{p:02X}" for p in obdsim.MODELED})
        seqs = [r["seq"] for r in records]
        self.assertEqual(seqs, sorted(seqs))
        self.assertEqual(len(set(seqs)), len(seqs))
        for s in samples[:5]:
            for key in ("t_mono", "t_wall", "raw", "val", "unit"):
                self.assertIn(key, s)
        self.assertNotIn(obdsim.SIM_VIN, open(only_log(self.d)).read())

    def test_pid_list_can_be_narrowed(self):
        with obdsim.PtyServer(link=self.link, latency=0.0):
            run_logger(["--port", self.link, "--out", self.d, "--max-seconds", "0.8",
                        "--pids", "0C,0D"])
        records, _ = obdlog.read_log(only_log(self.d))
        self.assertEqual({r["pid"] for r in records if r["type"] == "sample"}, {"0C", "0D"})

    def test_unsupported_requested_pid_is_logged_not_fatal(self):
        with obdsim.PtyServer(link=self.link, latency=0.0):
            run_logger(["--port", self.link, "--out", self.d, "--max-seconds", "0.8",
                        "--pids", "0C,5C"])
        records, _ = obdlog.read_log(only_log(self.d))
        events = [r for r in records if r["type"] == "event"]
        self.assertTrue(any(e["event"] == "no_data" and e.get("pid") == "5C" for e in events))
        self.assertTrue(any(r["type"] == "sample" and r["pid"] == "0C" for r in records))

    def test_idles_instead_of_spinning_when_every_pid_is_absent(self):
        with obdsim.PtyServer(link=self.link, latency=0.0):
            before = resource.getrusage(resource.RUSAGE_SELF)
            t0 = time.monotonic()
            run_logger(["--port", self.link, "--out", self.d, "--max-seconds", "3",
                        "--pids", "5C"])
            wall = time.monotonic() - t0
            after = resource.getrusage(resource.RUSAGE_SELF)
        cpu = (after.ru_utime - before.ru_utime) + (after.ru_stime - before.ru_stime)
        self.assertLess(cpu, wall * 0.5, f"{cpu:.2f} s CPU in {wall:.2f} s wall")
        records, _ = obdlog.read_log(only_log(self.d))
        self.assertTrue(any(r.get("event") == "pid_absent" for r in records))

    def test_reconnects_after_the_link_drops(self):
        srv = obdsim.PtyServer(link=self.link, latency=0.0).__enter__()
        result = {}
        t = threading.Thread(target=lambda: result.update(rc=run_logger(
            ["--port", self.link, "--out", self.d, "--max-seconds", "4",
             "--backoff-min", "0.2"])))
        t.start()
        time.sleep(1.0)
        srv.__exit__(None, None, None)          # Bluetooth drops
        time.sleep(1.0)
        srv = obdsim.PtyServer(link=self.link, latency=0.0).__enter__()   # and returns
        t.join(timeout=10)
        srv.__exit__(None, None, None)
        self.assertEqual(result.get("rc"), 0)
        records, _ = obdlog.read_log(only_log(self.d))
        kinds = [r.get("event") for r in records if r["type"] == "event"]
        self.assertIn("link_lost", kinds)
        self.assertIn("reconnected", kinds)
        back = next(i for i, r in enumerate(records) if r.get("event") == "reconnected")
        self.assertTrue(any(r["type"] == "sample" for r in records[back:]))

    def test_reopens_the_port_when_the_adapter_stops_answering(self):
        real_init = elm.Adapter.init
        calls = {"n": 0}

        def init_once(ad):
            calls["n"] += 1
            if calls["n"] > 1:
                raise elm.Timeout("ATZ: no prompt")
            return real_init(ad)

        with obdsim.PtyServer(link=self.link, latency=0.0), \
                mock.patch.object(elm.Adapter, "init", init_once), \
                mock.patch.object(elm.Adapter, "query",
                                  side_effect=elm.Timeout("010C: no prompt")):
            run_logger(["--port", self.link, "--out", self.d, "--max-seconds", "1.5",
                        "--backoff-min", "0.2"])
        records, _ = obdlog.read_log(only_log(self.d))
        kinds = [r.get("event") for r in records if r["type"] == "event"]
        self.assertIn("reinit_failed", kinds)
        self.assertIn("reopen", kinds)

    def test_gps_unavailable_is_an_event_and_logging_continues(self):
        with obdsim.PtyServer(link=self.link, latency=0.0), \
                mock.patch.object(macgps, "start",
                                  side_effect=macgps.GpsUnavailable("no pyobjc")):
            run_logger(["--port", self.link, "--out", self.d, "--max-seconds", "0.8",
                        "--gps"])
        records, _ = obdlog.read_log(only_log(self.d))
        self.assertTrue(any(r.get("event") == "gps_unavailable" for r in records))
        self.assertTrue(any(r["type"] == "sample" for r in records))

    def test_gps_is_polled_at_most_twice_a_second(self):
        class FakeGps:
            polls = 0

            def poll(self):
                FakeGps.polls += 1
                return [{"lat": 40.57, "lon": -105.08}], None

            def stop(self):
                pass

        with obdsim.PtyServer(link=self.link, latency=0.0), \
                mock.patch.object(macgps, "start", return_value=FakeGps()):
            run_logger(["--port", self.link, "--out", self.d, "--max-seconds", "1.2",
                        "--gps"])
        self.assertLessEqual(FakeGps.polls, 4)
        records, _ = obdlog.read_log(only_log(self.d))
        self.assertTrue(any(r["type"] == "gps" for r in records))

    def test_missing_port_at_start_fails_cleanly(self):
        rc = run_logger(["--port", "/nonexistent/obd", "--out", self.d, "--max-seconds",
                         "0.5", "--connect-tries", "1"])
        self.assertEqual(rc, 2)


class Sim(unittest.TestCase):
    def test_sim_flag_runs_the_whole_path_labeled_synth(self):
        with tempfile.TemporaryDirectory() as d:
            rc = run_logger(["--sim", "--out", d, "--max-seconds", "1.0"])
            self.assertEqual(rc, 0)
            records, _ = obdlog.read_log(only_log(d))
            self.assertTrue(all(r["src"] == "synth" for r in records))


class Writer(unittest.TestCase):
    def test_tick_flushes_a_quiet_buffer_after_the_interval(self):
        with tempfile.TemporaryDirectory() as d:
            p = os.path.join(d, "drive-x.jsonl")
            w = obdlog.LogWriter(p, "synth", fsync_lines=1000, fsync_sec=0.1)
            w.write("event", event="link_lost")
            self.assertEqual(os.path.getsize(p), 0, "still buffered")
            time.sleep(0.15)
            w.tick()
            self.assertGreater(os.path.getsize(p), 0, "tick wrote it out")
            w.close()


class Reader(unittest.TestCase):
    def test_torn_last_line_is_dropped_and_reported(self):
        with tempfile.TemporaryDirectory() as d:
            p = os.path.join(d, "drive-x.jsonl")
            with open(p, "w") as f:
                f.write(json.dumps({"type": "header", "src": "live", "seq": 0}) + "\n")
                f.write(json.dumps({"type": "sample", "src": "live", "seq": 1}) + "\n")
                f.write('{"type": "sample", "src": "li')
            records, torn = obdlog.read_log(p)
            self.assertEqual(len(records), 2)
            self.assertTrue(torn)


if __name__ == "__main__":
    unittest.main()
