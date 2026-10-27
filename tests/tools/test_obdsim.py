"""obdsim: a fake ELM327 on a pty. Everything it says is synthetic."""
import os
import tempfile
import time
import unittest

import obdlib
import obdsim
import serial


def ask(sim, cmd, t=10.0):
    return sim.handle(cmd, t)


class Commands(unittest.TestCase):
    def setUp(self):
        self.sim = obdsim.Simulator()

    def test_reset_names_itself_as_a_simulator(self):
        reply = ask(self.sim, "ATZ")
        self.assertIn("ELM327", reply)
        self.assertIn("SIM", reply)
        self.assertTrue(reply.endswith(">"))

    def test_echo_is_on_until_ate0(self):
        self.assertTrue(ask(self.sim, "ATRV").startswith("ATRV\r"))
        self.assertTrue(ask(self.sim, "ATE0").endswith("OK\r\r>"))
        self.assertFalse(ask(self.sim, "ATRV").startswith("ATRV"))

    def test_settings_commands_say_ok(self):
        for cmd in ("ATE0", "ATL0", "ATH0", "ATS1", "ATSP0", "ATST64", "ATAT1"):
            with self.subTest(cmd=cmd):
                self.assertIn("OK", ask(self.sim, cmd))

    def test_protocol_and_voltage(self):
        ask(self.sim, "ATE0")
        self.assertIn("ISO 15765-4", ask(self.sim, "ATDP"))
        self.assertRegex(ask(self.sim, "ATRV"), r"\d+\.\dV")

    def test_garbage_gets_a_question_mark(self):
        ask(self.sim, "ATE0")
        self.assertEqual(ask(self.sim, "ZZZZ"), "?\r\r>")

    def test_first_obd_request_shows_the_searching_banner_once(self):
        ask(self.sim, "ATE0")
        ask(self.sim, "ATSP0")
        self.assertIn("SEARCHING...", ask(self.sim, "0100"))
        self.assertNotIn("SEARCHING...", ask(self.sim, "0100"))

    def test_reset_clears_the_banner_state(self):
        ask(self.sim, "ATE0")
        ask(self.sim, "0100")
        ask(self.sim, "ATZ")
        ask(self.sim, "ATE0")
        self.assertIn("SEARCHING...", ask(self.sim, "0100"))


class ModeOne(unittest.TestCase):
    def setUp(self):
        self.sim = obdsim.Simulator()
        ask(self.sim, "ATE0")
        ask(self.sim, "0100")   # consume the banner

    def supported(self):
        found, base = set(), 0x00
        while True:
            reply = ask(self.sim, obdlib.request(1, base))
            block = obdlib.support_pids(base, obdlib.parse_response(reply, 1, base))
            found |= block
            if base + 0x20 not in block:
                return found
            base += 0x20

    def test_support_chain_lists_every_modeled_pid(self):
        self.assertEqual({p for p in self.supported() if p % 0x20}, set(obdsim.MODELED))

    def test_values_round_trip_through_the_decoder(self):
        t = 75.0
        state = obdsim.vehicle_state(t)
        for pid in obdsim.MODELED:
            with self.subTest(pid=hex(pid)):
                reply = self.sim.handle(obdlib.request(1, pid), t)
                data = obdlib.parse_response(reply, 1, pid)
                value, _unit = obdlib.decode(pid, data)
                self.assertAlmostEqual(value, state[pid], delta=max(1.0, abs(state[pid]) * 0.02))

    def test_unmodeled_pid_has_no_data(self):
        with self.assertRaises(obdlib.NoData):
            obdlib.parse_response(ask(self.sim, "015C"), 1, 0x5C)

    def test_vin_is_multiframe_and_decodes(self):
        vin = obdlib.decode_vin(ask(self.sim, "0902"))
        self.assertEqual(vin, obdsim.SIM_VIN)
        self.assertEqual(len(vin), 17)

    def test_mode_nine_support(self):
        data = obdlib.parse_response(ask(self.sim, "0900"), 9, 0x00)
        self.assertIn(0x02, obdlib.support_pids(0x00, data))


class TwoEcus(unittest.TestCase):
    def test_listed_pids_are_answered_by_two_ecus(self):
        sim = obdsim.Simulator(two_ecu={0x0C})
        ask(sim, "ATE0")
        ask(sim, "0100")
        self.assertEqual(len(obdlib.parse_all(ask(sim, "010C"), 1, 0x0C)), 2)
        self.assertEqual(len(obdlib.parse_all(ask(sim, "010D"), 1, 0x0D)), 1)

    def test_one_ecu_by_default(self):
        sim = obdsim.Simulator()
        ask(sim, "ATE0")
        self.assertEqual(len(obdlib.parse_all(ask(sim, "010C"), 1, 0x0C)), 1)


class DriveProfile(unittest.TestCase):
    def test_phases_cover_idle_acceleration_cruise_and_stop(self):
        speeds = [obdsim.vehicle_state(t)[0x0D] for t in range(0, 140)]
        self.assertEqual(speeds[5], 0)
        self.assertGreater(max(speeds), 70)
        self.assertEqual(speeds[130], 0)

    def test_signals_are_continuous(self):
        prev = obdsim.vehicle_state(0.0)
        for i in range(1, 2800):
            cur = obdsim.vehicle_state(i * 0.05)
            self.assertLess(abs(cur[0x0D] - prev[0x0D]), 3.0, f"speed jumps at {i * 0.05}")
            self.assertLess(abs(cur[0x05] - prev[0x05]), 0.5, f"coolant jumps at {i * 0.05}")
            prev = cur

    def test_engine_warms_up(self):
        self.assertLess(obdsim.vehicle_state(0)[0x05], 40)
        self.assertGreater(obdsim.vehicle_state(1200)[0x05], 85)

    def test_values_stay_inside_their_pid_range(self):
        for t in range(0, 1500, 7):
            s = obdsim.vehicle_state(float(t))
            self.assertTrue(0 <= s[0x04] <= 100)
            self.assertTrue(0 <= s[0x11] <= 100)
            self.assertTrue(-100 <= s[0x06] <= 99)
            self.assertTrue(600 <= s[0x0C] <= 6000)


class PtyServer(unittest.TestCase):
    def test_serial_client_talks_to_the_sim_through_a_link(self):
        with tempfile.TemporaryDirectory() as d:
            link = os.path.join(d, "obd")
            with obdsim.PtyServer(link=link, latency=0.0):
                port = serial.Serial(link, 38400, timeout=2)
                try:
                    port.write(b"ATZ\r")
                    buf = b""
                    deadline = time.monotonic() + 3
                    while not buf.endswith(b">") and time.monotonic() < deadline:
                        buf += port.read(64)
                    self.assertIn(b"ELM327", buf)
                finally:
                    port.close()
            self.assertFalse(os.path.lexists(link), "link is removed when the sim stops")


if __name__ == "__main__":
    unittest.main()
