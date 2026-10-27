"""elm: one serial session with an ELM327-compatible adapter, run against obdsim."""
import os
import tempfile
import time
import unittest

import elm
import obdsim


class AgainstSim(unittest.TestCase):
    def setUp(self):
        self.dir = tempfile.TemporaryDirectory()
        self.link = os.path.join(self.dir.name, "obd")
        self.srv = obdsim.PtyServer(link=self.link, latency=0.0).__enter__()
        self.ad = elm.Adapter(self.link)

    def tearDown(self):
        self.ad.close()
        self.srv.__exit__(None, None, None)
        self.dir.cleanup()

    def test_init_returns_the_banner_and_flags_a_simulator(self):
        banner = self.ad.init()
        self.assertIn("ELM327", banner)
        self.assertTrue(self.ad.is_sim)

    def test_query_returns_data_bytes(self):
        self.ad.init()
        self.assertEqual(len(self.ad.query(1, 0x0C)), 2)

    def test_supported_pids_walks_the_chain(self):
        self.ad.init()
        self.assertEqual(self.ad.supported_pids(), set(obdsim.MODELED))

    def test_protocol_voltage_and_vin(self):
        self.ad.init()
        self.assertIn("15765", self.ad.protocol())
        self.assertRegex(self.ad.voltage(), r"V$")
        self.assertEqual(self.ad.vin(), obdsim.SIM_VIN)

    def test_a_reply_returns_as_soon_as_its_prompt_arrives(self):
        self.ad.init()
        t0 = time.monotonic()
        for _ in range(40):
            self.ad.query(1, 0x0C)
        self.assertLess(time.monotonic() - t0, 1.0, "40 queries to a zero-latency sim")

    def test_link_lost_when_the_port_goes_away(self):
        self.ad.init()
        self.srv.__exit__(None, None, None)
        with self.assertRaises((elm.LinkLost, elm.Timeout)):
            for _ in range(3):
                self.ad.query(1, 0x0C)
        self.srv = obdsim.PtyServer(link=self.link, latency=0.0).__enter__()


class Opening(unittest.TestCase):
    def test_missing_port_is_link_lost(self):
        with self.assertRaises(elm.LinkLost):
            elm.Adapter("/nonexistent/obd")


if __name__ == "__main__":
    unittest.main()
