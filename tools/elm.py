"""One serial session with an ELM327-compatible adapter (the OBDLink LX, D-019).

The adapter is a serial port: /dev/cu.* on the Mac, /dev/obd on the Pi. Over Bluetooth
SPP the baud rate is ignored, so any value works.
"""
import time

import serial

import obdlib


class LinkLost(Exception):
    """The port vanished or errored: Bluetooth dropped, adapter unplugged or asleep."""


class Timeout(obdlib.AdapterError):
    """No prompt from the adapter within the timeout."""


# Settings sent after reset: echo off, no linefeeds, spaces on, headers off, auto protocol.
INIT = ("ATE0", "ATL0", "ATS1", "ATH0", "ATSP0")


class Adapter:
    def __init__(self, path, timeout=2.0, baud=115200):
        self.path, self.timeout = path, timeout
        self.banner = ""
        try:
            self.port = serial.Serial(path, baud, timeout=0.05)
        except (serial.SerialException, OSError) as e:
            raise LinkLost(f"cannot open {path}: {e}") from e

    @property
    def is_sim(self):
        """True when the adapter announced itself as obdsim. Its data is synth."""
        return "SIM" in self.banner.upper()

    def close(self):
        try:
            self.port.close()
        except (serial.SerialException, OSError):
            pass

    def command(self, cmd, timeout=None):
        """Send one command and return the raw reply text up to the `>` prompt."""
        deadline = time.monotonic() + (timeout or self.timeout)
        try:
            self.port.reset_input_buffer()
            self.port.write(cmd.encode("ascii") + b"\r")
            buf = b""
            while not buf.rstrip().endswith(b">"):
                if time.monotonic() > deadline:
                    raise Timeout(f"{cmd}: no prompt after {timeout or self.timeout}s")
                # Read what is waiting, or block for one byte, so the reply returns as
                # soon as its prompt arrives instead of at the end of a read timeout.
                buf += self.port.read(self.port.in_waiting or 1)
        except (serial.SerialException, OSError) as e:
            raise LinkLost(f"{cmd}: {e}") from e
        return buf.decode("ascii", errors="replace")

    def init(self):
        """Reset and configure the adapter. Returns the reset banner."""
        raw = self.command("ATZ", timeout=5.0)
        self.banner = " ".join(line for line in reply_lines(raw) if line != "ATZ")
        for cmd in INIT:
            self.command(cmd)
        # The first OBD request makes the adapter search for the protocol; allow for it.
        self.command("0100", timeout=15.0)
        return self.banner

    def query(self, mode, pid):
        """The data bytes of one request. Raises NoData, AdapterError, Timeout, LinkLost."""
        return obdlib.parse_response(self.command(obdlib.request(mode, pid)), mode, pid)

    def supported_pids(self):
        """Every Mode 01 PID the ECU lists, walking 0x00, 0x20, ... while chained."""
        found, base = set(), 0x00
        while base <= 0xE0:
            block = obdlib.support_pids(base, self.query(1, base))
            found |= {p for p in block if p % 0x20}
            if base + 0x20 not in block:
                break
            base += 0x20
        return found

    def protocol(self):
        return " ".join(reply_lines(self.command("ATDP")))

    def voltage(self):
        return " ".join(reply_lines(self.command("ATRV")))

    def vin(self):
        return obdlib.decode_vin(self.command("0902", timeout=5.0))


def reply_lines(raw):
    """Clean reply lines, without the prompt."""
    return [line.strip() for line in raw.replace(">", "").replace("\n", "\r").split("\r")
            if line.strip()]
