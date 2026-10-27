#!/usr/bin/env python3
"""Mode 01 supported-PID survey. Closes D-007's question for one car (board: pidforester).

Run it parked, engine running, with the OBDLink LX paired (D-019):

    tools/pidscan.py --port /dev/cu.OBDLinkLX-SPP     # Mac (check `ls /dev/cu.*`)
    tools/pidscan.py --port /dev/obd                  # Pi

It prints the adapter, the protocol, the battery voltage, the VIN (on screen only) and
every supported PID with one live reading, then writes data/pidscan-<time>.json and a
markdown block to paste into docs/hardware/pid-survey.md. Files carry the VIN's hash,
never the VIN itself.
"""
import argparse
import datetime
import json
import os
import sys

import elm
import obdlib

# VIN position 10 encodes the model year on a 30-year cycle (I, O, Q, U, Z, 0 unused).
_YEAR_CODES = "ABCDEFGHJKLMNPRSTVWXY123456789"


def model_year(vin, now=None):
    """The most recent model year the VIN's 10th character can mean, or None."""
    if len(vin) != 17 or vin[9].upper() not in _YEAR_CODES:
        return None
    now = now or datetime.date.today().year
    year = 1980 + _YEAR_CODES.index(vin[9].upper())
    while year + 30 <= now + 1:
        year += 30
    return year


def scan(ad):
    """Survey one adapter session. The VIN is returned separately and never stored."""
    banner = ad.init()
    result = {
        "src": "synth" if ad.is_sim else "live",
        "t_wall": datetime.datetime.now(datetime.timezone.utc).isoformat(),
        "adapter": banner,
        "protocol": ad.protocol(),
        "voltage": ad.voltage(),
        "vin_hash": None,
        "model_year": None,
        "supported": [],
        "samples": {},
    }
    vin = None
    try:
        vin = ad.vin()
        result["vin_hash"] = obdlib.vin_hash(vin)
        result["model_year"] = model_year(vin)
    except obdlib.AdapterError as e:
        result["vin_error"] = str(e)
    for pid in sorted(ad.supported_pids()):
        key = f"{pid:02X}"
        result["supported"].append(key)
        try:
            data = ad.query(1, pid)
            value, unit = obdlib.decode(pid, data)
            result["samples"][key] = {"raw": data.hex(" ").upper(), "val": value,
                                      "unit": unit, "name": obdlib.pid_name(pid)}
        except obdlib.AdapterError as e:
            result["samples"][key] = {"error": str(e), "name": obdlib.pid_name(pid)}
    return result, vin


def render_markdown(r):
    lines = [
        f"### Survey {r['t_wall'][:19]}Z (`src={r['src']}`)",
        "",
        f"- Adapter: `{r['adapter']}`",
        f"- Protocol (`ATDP`): `{r['protocol']}`",
        f"- Battery (`ATRV`): `{r['voltage']}`",
        f"- VIN hash: `{r['vin_hash']}`; model year from VIN: {r['model_year']}",
        f"- Supported Mode 01 PIDs ({len(r['supported'])}): "
        + ", ".join(f"`{p}`" for p in r["supported"]),
        "",
        "| PID | Signal | Raw | Value |",
        "| --- | --- | --- | --- |",
    ]
    for key in r["supported"]:
        s = r["samples"][key]
        value = s.get("error") or (f"{s['val']:.2f} {s['unit']}" if s["val"] is not None
                                   else "(not decoded)")
        lines.append(f"| 0x{key} | {s['name']} | `{s.get('raw', '')}` | {value} |")
    return "\n".join(lines) + "\n"


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--port", required=True, help="adapter serial port")
    ap.add_argument("--out", default="data", help="directory for the survey files")
    args = ap.parse_args(argv)
    try:
        ad = elm.Adapter(args.port)
    except elm.LinkLost as e:
        print(f"pidscan: {e}", file=sys.stderr)
        return 2
    try:
        result, vin = scan(ad)
    except (elm.LinkLost, obdlib.AdapterError) as e:
        print(f"pidscan: survey failed: {e}", file=sys.stderr)
        return 2
    finally:
        ad.close()

    os.makedirs(args.out, exist_ok=True)
    stamp = result["t_wall"][:19].replace(":", "").replace("-", "")
    base = os.path.join(args.out, f"pidscan-{stamp}-{result['src']}")
    with open(base + ".json", "w") as f:
        json.dump(result, f, indent=2)
    md = render_markdown(result)
    with open(base + ".md", "w") as f:
        f.write(md)

    print(md)
    print(f"VIN (screen only, not saved): {vin}")
    print(f"wrote {base}.json and {base}.md")
    if result["src"] == "synth":
        print("NOTE: this adapter is obdsim. Everything above is synthetic.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
