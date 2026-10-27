#!/usr/bin/env python3
"""One self-contained HTML page per drive log: scrub, zoom and compare every PID.

    tools/obdview.py data/drive-*.jsonl       # writes data/view-<log name>.html

Open the file in any browser; it needs no network and no server. Each PID is its own
small chart on one shared time axis. Hover to read every PID at that moment, drag
across a chart to zoom, double-click to reset, tick PIDs on and off. Dashed red lines
mark link drops, reconnects and adapter resets.
"""
import argparse
import json
import os
import sys

import obdlib
import obdlog
import obdplot

PAGE = r"""<!doctype html>
<html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Drive viewer</title>
<style>
:root{--bg:#fcfcfb;--ink:#0b0b0b;--muted:#52514e;--grid:#e4e3df;--series:#2a78d6;
--event:#d03b3b;--warn-bg:#fdecea}
@media (prefers-color-scheme:dark){:root{--bg:#1a1a19;--ink:#f0efec;--muted:#c3c2b7;
--grid:#383835;--series:#3987e5;--event:#e66767;--warn-bg:#3a1f1f}}
body{margin:0;background:var(--bg);color:var(--ink);font:14px/1.4 system-ui,sans-serif}
header{padding:12px 16px;border-bottom:1px solid var(--grid)}
h1{font-size:16px;margin:0 0 4px}
.meta{color:var(--muted);font-size:12px}
.warn{background:var(--warn-bg);color:var(--ink);padding:6px 16px;font-weight:600}
main{display:flex;gap:16px;padding:12px 16px}
nav{flex:0 0 220px;font-size:12px;max-height:80vh;overflow:auto}
nav label{display:block;padding:2px 0;cursor:pointer}
#charts{flex:1;min-width:0}
.chart{position:relative;margin-bottom:6px}
.chart .name{font-size:11px;color:var(--muted)}
.chart .val{position:absolute;right:0;top:0;font-size:11px;font-variant-numeric:tabular-nums}
canvas{display:block;width:100%;height:70px;cursor:crosshair}
#t{font-variant-numeric:tabular-nums;color:var(--muted);font-size:12px}
@media (max-width:640px){main{flex-direction:column}nav{flex:none;max-height:none}}
</style></head><body>
<header><h1 id="title"></h1><div class="meta" id="meta"></div>
<div class="meta">Hover to read, drag to zoom, double-click to reset. <span id="t"></span></div>
</header>
<div class="warn"__WARNATTR__>__WARN__</div>
<main><nav id="nav"></nav><div id="charts"></div></main>
<script id="drive" type="application/json">__DATA__</script>
<script>
const D = JSON.parse(document.getElementById('drive').textContent);
const css = n => getComputedStyle(document.documentElement).getPropertyValue(n).trim();
document.getElementById('title').textContent = D.file;
document.getElementById('meta').textContent =
  `src=${D.src} · ${D.adapter || '?'} · ${D.protocol || '?'} · VIN hash ${D.vin_hash || '-'}` +
  ` · ${D.seconds.toFixed(0)} s · ${D.samples} samples`;
let view = [0, D.seconds], hoverT = null, drag = null;
const charts = [];
const nav = document.getElementById('nav'), box = document.getElementById('charts');
for (const pid of Object.keys(D.pids).sort()) {
  const p = D.pids[pid];
  const lab = document.createElement('label');
  const cb = document.createElement('input'); cb.type = 'checkbox'; cb.checked = true;
  lab.append(cb, ` ${pid} ${p.name}`); nav.append(lab);
  const div = document.createElement('div'); div.className = 'chart';
  div.innerHTML = `<div class="name">${pid} · ${p.name} (${p.unit})</div><div class="val"></div>`;
  const cv = document.createElement('canvas'); div.append(cv); box.append(div);
  const c = {pid, p, div, cv, val: div.querySelector('.val')};
  cb.onchange = () => { div.hidden = !cb.checked; draw(); };
  cv.addEventListener('mousemove', e => {
    hoverT = xToT(c, e.offsetX);
    if (drag) drag.b = hoverT;
    draw();
  });
  cv.addEventListener('mouseleave', () => { hoverT = null; draw(); });
  cv.addEventListener('mousedown', e => { drag = {a: xToT(c, e.offsetX), b: null}; });
  cv.addEventListener('dblclick', () => { view = [0, D.seconds]; draw(); });
  charts.push(c);
}
window.addEventListener('mouseup', () => {
  if (drag && drag.b !== null && Math.abs(drag.b - drag.a) > (view[1] - view[0]) / 200)
    view = [Math.min(drag.a, drag.b), Math.max(drag.a, drag.b)];
  drag = null; draw();
});
const PAD = 44;
function xToT(c, x) {
  const w = c.cv.clientWidth - PAD;
  return view[0] + Math.max(0, Math.min(1, (x - PAD) / w)) * (view[1] - view[0]);
}
function nearest(t, ts) {
  let lo = 0, hi = ts.length - 1;
  while (hi - lo > 1) { const m = (lo + hi) >> 1; if (ts[m] < t) lo = m; else hi = m; }
  return Math.abs(ts[lo] - t) <= Math.abs(ts[hi] - t) ? lo : hi;
}
function draw() {
  const ink = css('--series'), grid = css('--grid'), muted = css('--muted'), ev = css('--event');
  document.getElementById('t').textContent =
    `Showing ${view[0].toFixed(1)} to ${view[1].toFixed(1)} s` +
    (hoverT === null ? '' : `, cursor at ${hoverT.toFixed(2)} s`);
  for (const c of charts) {
    if (c.div.hidden) continue;
    const dpr = window.devicePixelRatio || 1, W = c.cv.clientWidth, H = c.cv.clientHeight;
    c.cv.width = W * dpr; c.cv.height = H * dpr;
    const g = c.cv.getContext('2d'); g.scale(dpr, dpr); g.clearRect(0, 0, W, H);
    const {t, v} = c.p;
    let i0 = nearest(view[0], t), i1 = nearest(view[1], t);
    let lo = Infinity, hi = -Infinity;
    for (let i = i0; i <= i1; i++) { lo = Math.min(lo, v[i]); hi = Math.max(hi, v[i]); }
    if (!isFinite(lo)) continue;
    if (hi - lo < 1e-9) { lo -= 1; hi += 1; }
    const X = s => PAD + (s - view[0]) / (view[1] - view[0]) * (W - PAD);
    const Y = y => 4 + (1 - (y - lo) / (hi - lo)) * (H - 8);
    g.strokeStyle = grid; g.lineWidth = 1; g.beginPath();
    g.moveTo(PAD, Y(lo)); g.lineTo(W, Y(lo)); g.moveTo(PAD, Y(hi)); g.lineTo(W, Y(hi)); g.stroke();
    g.fillStyle = muted; g.font = '10px system-ui'; g.textAlign = 'right';
    g.fillText(+hi.toPrecision(4), PAD - 4, Y(hi) + 4); g.fillText(+lo.toPrecision(4), PAD - 4, Y(lo));
    g.strokeStyle = ev; g.setLineDash([4, 3]);
    for (const e of D.events) {
      if (e.t < view[0] || e.t > view[1]) continue;
      g.beginPath(); g.moveTo(X(e.t), 0); g.lineTo(X(e.t), H); g.stroke();
    }
    g.setLineDash([]); g.strokeStyle = ink; g.lineWidth = 1.5; g.beginPath();
    for (let i = i0; i <= i1; i++) { const x = X(t[i]), y = Y(v[i]); i === i0 ? g.moveTo(x, y) : g.lineTo(x, y); }
    g.stroke();
    if (drag && drag.b !== null) {
      g.fillStyle = grid; g.globalAlpha = 0.5;
      g.fillRect(X(Math.min(drag.a, drag.b)), 0, Math.abs(X(drag.b) - X(drag.a)), H);
      g.globalAlpha = 1;
    }
    if (hoverT !== null) {
      const i = nearest(hoverT, t);
      g.strokeStyle = muted; g.lineWidth = 1; g.beginPath();
      g.moveTo(X(hoverT), 0); g.lineTo(X(hoverT), H); g.stroke();
      g.fillStyle = ink; g.beginPath(); g.arc(X(t[i]), Y(v[i]), 4, 0, 7); g.fill();
      c.val.textContent = `${+v[i].toPrecision(5)} ${c.p.unit}`;
    } else c.val.textContent = '';
  }
}
window.addEventListener('resize', draw);
draw();
</script></body></html>
"""


def build(path, out):
    records, _ = obdlog.read_log(path)
    data = obdplot.series(records)
    header = next((r for r in records if r["type"] == "header"), {})
    t0 = min((r["t_mono"] for r in records if r["type"] == "sample"), default=0.0)
    unit = {r["pid"]: r.get("unit", "") for r in records if r["type"] == "sample"}
    seconds = max((max(t) for t, _ in data.values()), default=0.0)
    drive = {
        "file": os.path.basename(path), "src": records[0]["src"] if records else "?",
        "adapter": header.get("adapter"), "protocol": header.get("protocol"),
        "vin_hash": header.get("vin_hash"), "seconds": seconds,
        "samples": sum(len(t) for t, _ in data.values()),
        "pids": {pid: {"name": obdlib.pid_name(int(pid, 16)), "unit": unit[pid],
                       "t": [round(x, 3) for x in t], "v": [round(x, 4) for x in v]}
                 for pid, (t, v) in data.items()},
        "events": [{"t": round(r["t_mono"] - t0, 3), "event": r["event"]}
                   for r in records if r["type"] == "event"
                   and r["event"] in ("link_lost", "reconnected", "reinit")],
    }
    blob = json.dumps(drive, separators=(",", ":")).replace("</", "<\\/")
    live = drive["src"] == "live"
    page = (PAGE.replace("__WARNATTR__", " hidden" if live else "")
            .replace("__WARN__", "" if live else
                     f"{drive['src'].upper()} DATA: not from a real car")
            .replace("__DATA__", blob))
    with open(out, "w", encoding="utf-8") as f:
        f.write(page)
    return out


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("logs", nargs="+")
    ap.add_argument("--out-dir", default="data")
    args = ap.parse_args(argv)
    os.makedirs(args.out_dir, exist_ok=True)
    for path in args.logs:
        name = os.path.splitext(os.path.basename(path))[0]
        print(build(path, os.path.join(args.out_dir, f"view-{name}.html")))
    return 0


if __name__ == "__main__":
    sys.exit(main())
