"""obdlib: ELM327 reply parsing, Mode 01 decoding, support bitmaps, VIN handling."""
import unittest

import obdlib
from obdlib import AdapterError, NoData


class ParseResponse(unittest.TestCase):
    def test_single_line_reply_returns_data_after_the_pid(self):
        data = obdlib.parse_response("41 0C 1A F8 \r\r>", 0x01, 0x0C)
        self.assertEqual(data, bytes([0x1A, 0xF8]))

    def test_spaces_off_reply(self):
        self.assertEqual(obdlib.parse_response("410C1AF8\r\r>", 0x01, 0x0C),
                         bytes([0x1A, 0xF8]))

    def test_searching_banner_is_skipped(self):
        raw = "SEARCHING...\r41 0D 3C \r\r>"
        self.assertEqual(obdlib.parse_response(raw, 0x01, 0x0D), bytes([0x3C]))

    def test_first_matching_ecu_wins(self):
        raw = "41 05 5A \r41 05 5B \r\r>"
        self.assertEqual(obdlib.parse_response(raw, 0x01, 0x05), bytes([0x5A]))

    def test_other_pid_reply_is_not_accepted(self):
        with self.assertRaises(AdapterError):
            obdlib.parse_response("41 0D 3C \r\r>", 0x01, 0x0C)

    def test_bus_init_ok_is_skipped_and_its_error_raises(self):
        raw = "BUS INIT: ...OK\r41 0C 1A F8 \r\r>"
        self.assertEqual(obdlib.parse_response(raw, 0x01, 0x0C), bytes([0x1A, 0xF8]))
        with self.assertRaises(AdapterError) as cm:
            obdlib.parse_response("BUS INIT: ...ERROR\r\r>", 0x01, 0x0C)
        self.assertEqual(str(cm.exception), "BUS INIT: ...ERROR")

    def test_no_data_is_its_own_error(self):
        with self.assertRaises(NoData):
            obdlib.parse_response("NO DATA\r\r>", 0x01, 0x5C)

    def test_adapter_errors_raise(self):
        for text in ("?", "BUFFER FULL", "UNABLE TO CONNECT", "CAN ERROR", "STOPPED",
                     "BUS BUSY", "<DATA ERROR"):
            with self.subTest(text=text), self.assertRaises(AdapterError) as cm:
                obdlib.parse_response(text + "\r\r>", 0x01, 0x0C)
            self.assertNotIsInstance(cm.exception, NoData)

    def test_no_bus_is_its_own_error(self):
        for text in ("UNABLE TO CONNECT", "CAN ERROR"):
            with self.subTest(text=text), self.assertRaises(obdlib.BusAsleep):
                obdlib.parse_response(text + "\r\r>", 0x01, 0x0C)
        with self.assertRaises(obdlib.BusAsleep):
            obdlib.check_bus("SEARCHING...\rUNABLE TO CONNECT\r\r>")
        obdlib.check_bus("SEARCHING...\r41 00 BE 1F A8 13 \r\r>")

    def test_nul_bytes_from_the_adapter_are_ignored(self):
        self.assertEqual(obdlib.parse_response("\x0041 0C 1A F8 \x00\r\r>", 0x01, 0x0C),
                         bytes([0x1A, 0xF8]))

    def test_empty_reply_raises(self):
        with self.assertRaises(AdapterError):
            obdlib.parse_response("\r>", 0x01, 0x0C)


class Decode(unittest.TestCase):
    def test_rpm(self):
        self.assertEqual(obdlib.decode(0x0C, bytes([0x1A, 0xF8])), (1726.0, "rpm"))

    def test_speed(self):
        self.assertEqual(obdlib.decode(0x0D, bytes([0x3C])), (60.0, "km/h"))

    def test_coolant_has_a_forty_degree_offset(self):
        self.assertEqual(obdlib.decode(0x05, bytes([0x5A])), (50.0, "degC"))

    def test_fuel_trim_is_centered_on_128(self):
        self.assertEqual(obdlib.decode(0x06, bytes([128])), (0.0, "%"))
        val, unit = obdlib.decode(0x06, bytes([0]))
        self.assertAlmostEqual(val, -100.0)
        self.assertEqual(unit, "%")

    def test_load_scales_255_to_100_percent(self):
        val, _ = obdlib.decode(0x04, bytes([255]))
        self.assertAlmostEqual(val, 100.0)

    def test_maf(self):
        self.assertAlmostEqual(obdlib.decode(0x10, bytes([0x01, 0x90]))[0], 4.0)

    def test_narrowband_o2_reports_the_voltage(self):
        val, unit = obdlib.decode(0x14, bytes([0x5A, 0xFF]))
        self.assertAlmostEqual(val, 0.45)
        self.assertEqual(unit, "V")

    def test_unknown_pid_decodes_to_none(self):
        self.assertEqual(obdlib.decode(0xFE, bytes([1, 2])), (None, ""))

    def test_short_data_decodes_to_none_not_an_exception(self):
        self.assertEqual(obdlib.decode(0x0C, bytes([0x1A])), (None, ""))

    def test_every_listed_pid_has_a_name(self):
        for pid in obdlib.PIDS:
            self.assertTrue(obdlib.pid_name(pid))


class SupportBitmap(unittest.TestCase):
    def test_msb_is_the_first_pid_of_the_block(self):
        # BE 1F A8 13: the classic worked example from SAE J1979.
        supported = obdlib.support_pids(0x00, bytes([0xBE, 0x1F, 0xA8, 0x13]))
        self.assertIn(0x01, supported)
        self.assertNotIn(0x02, supported)
        self.assertIn(0x0C, supported)
        self.assertIn(0x20, supported)   # the "next block exists" bit
        self.assertNotIn(0x21, supported)

    def test_offset_by_block_base(self):
        supported = obdlib.support_pids(0x20, bytes([0x80, 0, 0, 0]))
        self.assertEqual(supported, {0x21})

    def test_bitmaps_from_every_answering_ecu_are_combined(self):
        # Engine and transmission both answer 0100; with headers off either may be first.
        raw = "41 00 80 00 00 00 \r41 00 BE 1F A8 13 \r\r>"
        supported = obdlib.support_from_reply(raw, 0x00)
        self.assertIn(0x01, supported)
        self.assertIn(0x0C, supported)
        self.assertIn(0x20, supported)

    def test_wrong_length_is_an_error(self):
        with self.assertRaises(AdapterError):
            obdlib.support_pids(0x00, bytes([1, 2, 3]))


class Vin(unittest.TestCase):
    MULTI = ("014\r0: 49 02 01 31 48 47\r1: 43 4D 38 32 36 33 33\r"
             "2: 41 30 30 34 33 35 32\r\r>")

    def test_multiframe_vin(self):
        self.assertEqual(obdlib.decode_vin(self.MULTI), "1HGCM82633A004352")

    def test_single_frame_per_line_vin(self):
        raw = ("49 02 01 00 00 00 31\r49 02 02 48 47 43 4D\r49 02 03 38 32 36 33\r"
               "49 02 04 33 41 30 30\r49 02 05 34 33 35 32\r\r>")
        self.assertEqual(obdlib.decode_vin(raw), "1HGCM82633A004352")

    def test_second_ecu_frames_do_not_overwrite_the_first(self):
        other = ("014\r0: 49 02 01 32 32 32\r1: 32 32 32 32 32 32 32\r"
                 "2: 32 32 32 32 32 32 32\r")
        self.assertEqual(obdlib.decode_vin(self.MULTI.replace("\r>", "") + other + "\r>"),
                         "1HGCM82633A004352")

    def test_no_data_means_no_vin(self):
        with self.assertRaises(NoData):
            obdlib.decode_vin("NO DATA\r\r>")

    def test_hash_is_stable_short_and_hides_the_vin(self):
        h = obdlib.vin_hash("1HGCM82633A004352")
        self.assertEqual(h, obdlib.vin_hash("1HGCM82633A004352"))
        self.assertEqual(len(h), 12)
        self.assertNotIn("1HGCM", h)
        self.assertNotEqual(h, obdlib.vin_hash("1HGCM82633A004353"))


class Requests(unittest.TestCase):
    def test_request_format(self):
        self.assertEqual(obdlib.request(0x01, 0x0C), "010C")
        self.assertEqual(obdlib.request(0x09, 0x02), "0902")


if __name__ == "__main__":
    unittest.main()
