"""elm: one serial session with an ELM327-compatible adapter, run against obdsim."""
import os
import tempfile
import time
import unittest
from unittest import mock

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


class SlowPort:
    """A fake serial port in front of an ELM327 that answers some requests late.

    `delays` maps a command to how many seconds its reply takes. Like the real chip, any
    byte written while a reply is pending interrupts it: the pending reply is dropped and
    `STOPPED` is printed. A bare CR to an idle adapter repeats the last command.
    """

    def __init__(self, delays):
        self.delays = delays
        self.pending = None          # (time the reply is ready, reply bytes)
        self.out = b""               # bytes the host can read now
        self.last = None

    def _reply(self, cmd):
        pid = cmd[2:]
        data = {"0C": "1A F8", "0D": "3C"}.get(pid, "00")
        return f"41 {pid} {data} \r\r>".encode("ascii")

    def _advance(self):
        if self.pending and time.monotonic() >= self.pending[0]:
            self.out += self.pending[1]
            self.pending = None

    def write(self, data):
        self._advance()
        if self.pending:
            self.pending = None
            self.out += b"STOPPED\r\r>"
            return
        cmd = data.decode("ascii").strip() or self.last
        self.last = cmd
        self.pending = (time.monotonic() + self.delays.get(cmd, 0.0), self._reply(cmd))
        self._advance()

    @property
    def in_waiting(self):
        self._advance()
        return len(self.out)

    def read(self, n):
        self._advance()
        if not self.out:
            time.sleep(0.01)
            self._advance()
        chunk, self.out = self.out[:n], self.out[n:]
        return chunk

    def reset_input_buffer(self):
        self._advance()
        self.out = b""

    def close(self):
        pass


def slow_adapter(delays):
    with mock.patch.object(elm.serial, "Serial", return_value=SlowPort(delays)):
        return elm.Adapter("/dev/fake", timeout=0.2)


class AfterATimeout(unittest.TestCase):
    def test_a_late_reply_is_not_taken_as_the_next_answer(self):
        ad = slow_adapter({"010C": 0.35})
        with self.assertRaises(elm.Timeout):
            ad.query(1, 0x0C)
        self.assertEqual(ad.query(1, 0x0D), bytes([0x3C]))

    def test_a_request_that_never_ends_is_interrupted(self):
        ad = slow_adapter({"010C": 30.0})
        with self.assertRaises(elm.Timeout):
            ad.query(1, 0x0C)
        self.assertEqual(ad.query(1, 0x0D), bytes([0x3C]))
        self.assertEqual(ad.query(1, 0x0D), bytes([0x3C]), "no repeated reply is left over")


class SupportChain(unittest.TestCase):
    def test_no_data_on_a_later_block_ends_the_chain(self):
        replies = {"0100": "41 00 80 00 00 01 \r\r>",     # 01, and block 0x20 exists
                   "0120": "NO DATA\r\r>"}
        ad = elm.Adapter.__new__(elm.Adapter)
        ad.command = lambda cmd, timeout=None: replies[cmd]
        self.assertEqual(ad.supported_pids(), {0x01})

    def test_no_data_on_the_first_block_still_raises(self):
        ad = elm.Adapter.__new__(elm.Adapter)
        ad.command = lambda cmd, timeout=None: "NO DATA\r\r>"
        with self.assertRaises(elm.obdlib.NoData):
            ad.supported_pids()


class Opening(unittest.TestCase):
    def test_missing_port_is_link_lost(self):
        with self.assertRaises(elm.LinkLost):
            elm.Adapter("/nonexistent/obd")


if __name__ == "__main__":
    unittest.main()
