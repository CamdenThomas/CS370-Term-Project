"""One serial session with an ELM327-compatible adapter (the OBDLink LX, D-019).

The adapter is a serial port: /dev/cu.* on the Mac, /dev/obd on the Pi. Over Bluetooth
SPP the baud rate is ignored, so any value works.
"""
import time

import serial

import obdlib

try:
    import termios
except ImportError:  # Windows has no termios; pyserial raises SerialException there.
    termios = None

# What a vanished port raises. On Linux a pty or RFCOMM tty whose other end has gone fails
# tcflush() with termios.error (EIO), which is not an OSError.
PORT_ERRORS = (serial.SerialException, OSError) + ((termios.error,) if termios else ())


class LinkLost(Exception):
    """The port vanished or errored: Bluetooth dropped, adapter unplugged or asleep."""


class Timeout(obdlib.AdapterError):
    """No prompt from the adapter within the timeout."""


# Seconds to wait for a timed-out request's late prompt before interrupting it.
RESYNC_WAIT = 0.5

# Settings sent after reset: echo off, no linefeeds, spaces on, headers off, auto protocol.
INIT = ("ATE0", "ATL0", "ATS1", "ATH0", "ATSP0")


class Adapter:
    def __init__(self, path, timeout=2.0, baud=115200):
        self.path, self.timeout = path, timeout
        self.banner = ""
        self.stale = False     # a request timed out; its reply may still be on its way
        try:
            # A write blocked on a stalled Bluetooth link must time out, or Ctrl-C could
            # never end the session; pyserial reports it as a SerialException (LinkLost).
            self.port = serial.Serial(path, baud, timeout=0.05, write_timeout=timeout)
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

    def _read_to_prompt(self, deadline):
        """Bytes up to and including the `>` prompt, or None at the deadline."""
        buf = b""
        while not buf.rstrip().endswith(b">"):
            if time.monotonic() > deadline:
                return None
            # Read what is waiting, or block for one byte, so the reply returns as soon as
            # its prompt arrives instead of at the end of a read timeout.
            buf += self.port.read(self.port.in_waiting or 1)
        return buf

    def _resync(self, cmd):
        """Wait out the reply to a request that timed out, so it is not read as the
        answer to the next one. First wait for its prompt; if none comes, send a bare CR,
        which makes a busy adapter stop (`STOPPED`, then `>`). A CR to an idle adapter
        would repeat the last command, which is why it is not sent first."""
        if self._read_to_prompt(time.monotonic() + min(RESYNC_WAIT, self.timeout)) is None:
            self.port.write(b"\r")
            if self._read_to_prompt(time.monotonic() + self.timeout) is None:
                raise Timeout(f"{cmd}: adapter still busy with a timed-out request")
        self.stale = False

    def command(self, cmd, timeout=None):
        """Send one command and return the raw reply text up to the `>` prompt."""
        try:
            if self.stale:
                self._resync(cmd)
            deadline = time.monotonic() + (timeout or self.timeout)
            self.port.reset_input_buffer()
            self.port.write(cmd.encode("ascii") + b"\r")
            buf = self._read_to_prompt(deadline)
            if buf is None:
                self.stale = True
                raise Timeout(f"{cmd}: no prompt after {timeout or self.timeout}s")
        except PORT_ERRORS as e:
            raise LinkLost(f"{cmd}: {e}") from e
        return buf.decode("ascii", errors="replace")

    def init(self):
        """Reset and configure the adapter. Returns the reset banner."""
        raw = self.command("ATZ", timeout=5.0)
        self.banner = " ".join(line for line in reply_lines(raw) if line != "ATZ")
        for cmd in INIT:
            self.command(cmd)
        # The first OBD request makes the adapter search for the protocol; allow for it.
        # With the ignition off that search fails (BusAsleep), and the caller waits.
        obdlib.check_bus(self.command("0100", timeout=15.0))
        return self.banner

    def query(self, mode, pid):
        """The data bytes of one request. Raises NoData, BusAsleep, AdapterError, Timeout,
        LinkLost."""
        return obdlib.parse_response(self.command(obdlib.request(mode, pid)), mode, pid)

    def supported_pids(self):
        """Every Mode 01 PID the ECU lists, walking 0x00, 0x20, ... while chained."""
        found, base = set(), 0x00
        while base <= 0xE0:
            # Every ECU that answers contributes; the engine may not answer first.
            raw = self.command(obdlib.request(1, base))
            try:
                block = obdlib.support_from_reply(raw, base)
            except obdlib.NoData:
                if base == 0x00:
                    raise           # nothing answers at all: the caller must know
                break               # a block the previous one chained to, unanswered
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
