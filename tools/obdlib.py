"""ELM327 / OBD-II Mode 01 and Mode 09 helpers shared by the tools.

Nothing here talks to a port. It parses adapter text, decodes the standard Mode 01 PIDs
(SAE J1979 formulas), turns support bitmaps into PID sets and handles the VIN. The logger,
the survey and the simulator all import it, so they agree on one reading of the protocol.
"""
import hashlib
import re


class AdapterError(Exception):
    """The adapter or the bus gave an answer we cannot use."""


class NoData(AdapterError):
    """The adapter answered `NO DATA`: the ECU did not reply to this request."""


# Replies that mean the request failed, as text the adapter prints.
_ERROR_TEXT = ("?", "BUFFER FULL", "UNABLE TO CONNECT", "CAN ERROR", "STOPPED",
               "BUS BUSY", "BUS ERROR", "DATA ERROR", "<DATA ERROR", "FB ERROR",
               "LV RESET", "ACT ALERT", "BUS INIT: ERROR", "BUS INIT:...ERROR",
               "ERROR")
_SKIP_TEXT = ("SEARCHING...", "BUS INIT: OK", "BUS INIT:...OK")
_HEX_LINE = re.compile(r"^(?:[0-9A-F]{2})+$")


def request(mode, pid):
    """The command text for a Mode request, e.g. request(1, 0x0C) is '010C'."""
    return f"{mode:02X}{pid:02X}"


def _lines(raw):
    """Reply text to clean upper-case lines: no prompt, no blanks."""
    out = []
    for line in re.split(r"[\r\n]+", raw.replace(">", "")):
        line = line.strip().upper()
        if line:
            out.append(line)
    return out


def _hex_bytes(line):
    """A hex line, with or without spaces, as bytes; None if it is not hex."""
    packed = line.replace(" ", "")
    if _HEX_LINE.match(packed):
        return bytes.fromhex(packed)
    return None


def _check_text(line):
    if line == "NO DATA":
        raise NoData("NO DATA")
    if line in _ERROR_TEXT:
        raise AdapterError(line)


def parse_response(raw, mode, pid):
    """Data bytes after the `<mode+0x40> <pid>` header of the first matching line.

    Raises NoData for `NO DATA` and AdapterError for any other failure, including a reply
    that holds no line for this request.
    """
    want = (mode + 0x40, pid)
    seen = False
    for line in _lines(raw):
        seen = True
        if line in _SKIP_TEXT:
            continue
        _check_text(line)
        data = _hex_bytes(line)
        if data is not None and len(data) >= 2 and (data[0], data[1]) == want:
            return data[2:]
    raise AdapterError("no matching reply" if seen else "empty reply")


def _u16(d):
    return d[0] * 256 + d[1]


# pid: (name, unit, bytes needed, decode function over the data bytes)
PIDS = {
    0x04: ("Calculated engine load", "%", 1, lambda d: d[0] * 100 / 255),
    0x05: ("Coolant temperature", "degC", 1, lambda d: d[0] - 40),
    0x06: ("Short term fuel trim, bank 1", "%", 1, lambda d: (d[0] - 128) * 100 / 128),
    0x07: ("Long term fuel trim, bank 1", "%", 1, lambda d: (d[0] - 128) * 100 / 128),
    0x08: ("Short term fuel trim, bank 2", "%", 1, lambda d: (d[0] - 128) * 100 / 128),
    0x09: ("Long term fuel trim, bank 2", "%", 1, lambda d: (d[0] - 128) * 100 / 128),
    0x0A: ("Fuel pressure", "kPa", 1, lambda d: d[0] * 3),
    0x0B: ("Intake manifold pressure", "kPa", 1, lambda d: d[0]),
    0x0C: ("Engine speed", "rpm", 2, lambda d: _u16(d) / 4),
    0x0D: ("Vehicle speed", "km/h", 1, lambda d: d[0]),
    0x0E: ("Timing advance", "deg", 1, lambda d: d[0] / 2 - 64),
    0x0F: ("Intake air temperature", "degC", 1, lambda d: d[0] - 40),
    0x10: ("Mass air flow", "g/s", 2, lambda d: _u16(d) / 100),
    0x11: ("Throttle position", "%", 1, lambda d: d[0] * 100 / 255),
    0x1F: ("Run time since start", "s", 2, _u16),
    0x21: ("Distance with MIL on", "km", 2, _u16),
    0x2C: ("Commanded EGR", "%", 1, lambda d: d[0] * 100 / 255),
    0x2F: ("Fuel level", "%", 1, lambda d: d[0] * 100 / 255),
    0x30: ("Warm-ups since codes cleared", "count", 1, lambda d: d[0]),
    0x31: ("Distance since codes cleared", "km", 2, _u16),
    0x33: ("Barometric pressure", "kPa", 1, lambda d: d[0]),
    0x42: ("Control module voltage", "V", 2, lambda d: _u16(d) / 1000),
    0x43: ("Absolute load", "%", 2, lambda d: _u16(d) * 100 / 255),
    0x44: ("Commanded equivalence ratio", "ratio", 2, lambda d: _u16(d) / 32768),
    0x45: ("Relative throttle position", "%", 1, lambda d: d[0] * 100 / 255),
    0x46: ("Ambient air temperature", "degC", 1, lambda d: d[0] - 40),
    0x47: ("Absolute throttle position B", "%", 1, lambda d: d[0] * 100 / 255),
    0x49: ("Accelerator pedal position D", "%", 1, lambda d: d[0] * 100 / 255),
    0x4A: ("Accelerator pedal position E", "%", 1, lambda d: d[0] * 100 / 255),
    0x4C: ("Commanded throttle actuator", "%", 1, lambda d: d[0] * 100 / 255),
    0x5C: ("Engine oil temperature", "degC", 1, lambda d: d[0] - 40),
    0x5E: ("Engine fuel rate", "L/h", 2, lambda d: _u16(d) / 20),
}
# Narrow-band O2 sensors 0x14-0x1B report volts first (the short term trim is second).
for _pid in range(0x14, 0x1C):
    PIDS[_pid] = (f"O2 sensor {_pid - 0x13} voltage", "V", 2, lambda d: d[0] / 200)
# Wide-band O2 sensors 0x24-0x2B report the equivalence ratio first.
for _pid in range(0x24, 0x2C):
    PIDS[_pid] = (f"O2 sensor {_pid - 0x23} lambda", "ratio", 4,
                  lambda d: _u16(d) / 32768)
# Catalyst temperatures.
for _pid, _label in ((0x3C, "B1S1"), (0x3D, "B2S1"), (0x3E, "B1S2"), (0x3F, "B2S2")):
    PIDS[_pid] = (f"Catalyst temperature {_label}", "degC", 2, lambda d: _u16(d) / 10 - 40)


def pid_name(pid):
    entry = PIDS.get(pid)
    return entry[0] if entry else f"PID {pid:02X}"


def decode(pid, data):
    """(value, unit) for a Mode 01 reply's data bytes; (None, '') if unknown or short."""
    entry = PIDS.get(pid)
    if entry is None or len(data) < entry[2]:
        return (None, "")
    return (float(entry[3](data)), entry[1])


def support_pids(base, data):
    """The PIDs a support-bitmap reply (PID 0x00, 0x20, ...) lists as supported.

    The most significant bit is PID base+1 and the least significant is base+0x20, which
    says whether the next block exists.
    """
    if len(data) != 4:
        raise AdapterError(f"support bitmap needs 4 bytes, got {len(data)}")
    bits = int.from_bytes(data, "big")
    return {base + 1 + i for i in range(32) if bits & (1 << (31 - i))}


def decode_vin(raw):
    """The VIN from a Mode 09 PID 02 reply, multi-frame or one frame per line."""
    chunks = []
    framed = {}
    for line in _lines(raw):
        _check_text(line)
        m = re.match(r"^(\d+):\s*(.*)$", line)
        if m:
            data = _hex_bytes(m.group(2))
            if data is not None:
                framed[int(m.group(1))] = data
            continue
        data = _hex_bytes(line)
        if data is not None and len(data) >= 3 and data[:2] == b"\x49\x02":
            chunks.append((data[2], data[3:]))
    if framed:
        body = b"".join(framed[k] for k in sorted(framed))
        body = body[3:] if body[:2] == b"\x49\x02" else body
    elif chunks:
        body = b"".join(c for _, c in sorted(chunks))
    else:
        raise AdapterError("no VIN in reply")
    vin = body.replace(b"\x00", b"").decode("ascii", errors="replace").strip()
    if not vin:
        raise AdapterError("empty VIN")
    return vin


def vin_hash(vin):
    """A short stable label that groups sessions by car without writing the VIN down.

    This is pseudonymization, not anonymity: a VIN has little entropy, so anyone holding
    candidate VINs can check them against it. It keeps the VIN out of files and git.
    """
    return hashlib.sha256(vin.encode("ascii")).hexdigest()[:12]
