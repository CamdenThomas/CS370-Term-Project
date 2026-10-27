"""obdlog: the drive logger, run against obdsim."""
import contextlib
import io
import json
import os
import resource
import signal
import tempfile
import threading
import time
import unittest
from unittest import mock

import elm
import macgps
import obdlib
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
        with open(only_log(self.d)) as f:
            self.assertNotIn(obdsim.SIM_VIN, f.read())

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

    def test_a_sleeping_bus_is_waited_out_not_reset(self):
        asleep = mock.Mock(side_effect=obdlib.BusAsleep("UNABLE TO CONNECT"))
        with obdsim.PtyServer(link=self.link, latency=0.0), \
                mock.patch.object(elm.Adapter, "query", asleep):
            run_logger(["--port", self.link, "--out", self.d, "--max-seconds", "1.5",
                        "--backoff-min", "0.2"])
        records, _ = obdlog.read_log(only_log(self.d))
        kinds = [r.get("event") for r in records if r["type"] == "event"]
        self.assertEqual(kinds.count("bus_asleep"), 1, kinds)
        self.assertNotIn("reinit", kinds)
        self.assertLessEqual(asleep.call_count, 5, "tries back off: 0, 0.2, 0.6, 1.4 s")

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

    def test_bad_pid_lists_are_refused_before_connecting(self):
        for bad in ("", "0C,", "ZZ", "0C,1FF"):
            with self.subTest(pids=bad), \
                    mock.patch.object(elm, "Adapter") as adapter, \
                    self.assertRaises(SystemExit):
                run_logger(["--port", self.link, "--out", self.d, "--pids", bad])
            adapter.assert_not_called()

    def test_an_ecu_that_lists_nothing_ends_the_session_at_once(self):
        with obdsim.PtyServer(link=self.link, latency=0.0), \
                mock.patch.object(elm.Adapter, "supported_pids", return_value=set()):
            t0 = time.monotonic()
            rc = run_logger(["--port", self.link, "--out", self.d, "--max-seconds", "2"])
        self.assertEqual(rc, 2)
        self.assertLess(time.monotonic() - t0, 1.5)

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

    def test_a_field_cannot_relabel_a_record(self):
        with tempfile.TemporaryDirectory() as d:
            p = os.path.join(d, "drive-x.jsonl")
            w = obdlog.LogWriter(p, "synth", fsync_lines=1, fsync_sec=1.0)
            for field in ("src", "type", "seq", "t_mono", "t_wall"):
                with self.subTest(field=field), self.assertRaises(ValueError):
                    w.write("gps", **{field: "live", "lat": 40.0})
            w.close()
            records, _ = obdlog.read_log(p)
            self.assertEqual(records, [], "nothing was written")


class Interrupt(unittest.TestCase):
    def test_the_first_signal_asks_the_second_forces(self):
        stop = obdlog.Stop()
        stop(signal.SIGINT, None)
        self.assertEqual(stop.reason, "sigint")
        with self.assertRaises(KeyboardInterrupt):
            stop(signal.SIGTERM, None)
        self.assertEqual(stop.reason, "sigint", "the first reason is kept for the footer")

    def test_a_forced_stop_still_writes_the_footer(self):
        with tempfile.TemporaryDirectory() as d:
            link = os.path.join(d, "obd")

            def interrupted(_ad, _mode, _pid):
                raise KeyboardInterrupt

            with obdsim.PtyServer(link=link, latency=0.0), \
                    mock.patch.object(elm.Adapter, "query", interrupted), \
                    self.assertRaises(KeyboardInterrupt):
                run_logger(["--port", link, "--out", d, "--max-seconds", "2"])
            records, torn = obdlog.read_log(only_log(d))
            self.assertFalse(torn)
            self.assertEqual(records[-1]["type"], "footer")


class Footer(unittest.TestCase):
    def test_an_unexpected_error_is_named_in_the_footer(self):
        with tempfile.TemporaryDirectory() as d:
            link = os.path.join(d, "obd")
            with obdsim.PtyServer(link=link, latency=0.0), \
                    mock.patch.object(elm.Adapter, "query",
                                      side_effect=RuntimeError("boom")), \
                    self.assertRaises(RuntimeError):
                run_logger(["--port", link, "--out", d, "--max-seconds", "2"])
            records, _ = obdlog.read_log(only_log(d))
            self.assertEqual(records[-1]["reason"], "error: RuntimeError: boom")


class Naming(unittest.TestCase):
    def test_two_sessions_in_one_millisecond_get_two_files(self):
        with tempfile.TemporaryDirectory() as d, \
                mock.patch.object(obdlog, "_wall", return_value="2026-10-27T12:00:00.123+00:00"):
            for _ in range(2):
                run_logger(["--sim", "--out", d, "--max-seconds", "0.3"])
            logs = sorted(f for f in os.listdir(d) if f.startswith("drive-"))
            self.assertEqual(logs, ["drive-20261027T120000.123-2-synth.jsonl",
                                    "drive-20261027T120000.123-synth.jsonl"])
            for name in logs:
                records, _ = obdlog.read_log(os.path.join(d, name))
                self.assertEqual([r["type"] for r in records].count("header"), 1, name)

    def test_the_writer_never_appends_to_an_existing_file(self):
        with tempfile.TemporaryDirectory() as d:
            p = os.path.join(d, "drive-x.jsonl")
            obdlog.LogWriter(p, "synth", 50, 1.0).close()
            with self.assertRaises(FileExistsError):
                obdlog.LogWriter(p, "synth", 50, 1.0)


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
