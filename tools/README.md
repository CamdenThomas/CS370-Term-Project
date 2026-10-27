# tools/ — Python is allowed here (and in its tests, `tests/tools/`) only

Per the handout: the systems core is C; the interface layer may use anything, but **no
graded mechanism may hide in it.** Nothing in this directory is part of the shipped
product's runtime path.

**Present:**

| Tool | Purpose |
| --- | --- |
| `board_sync.py` | Mirrors the checklist in `docs/milestones.md` onto GitHub issues and the project board. Not a product tool at all — it is how the plan is maintained. See `docs/BOARD/BOARD.md`. |
| `pidscan.py` | Mode 01 supported-PID survey for one car: protocol, battery, VIN hash, one reading per PID. Feeds `pid-survey.md`, which sets each testbed's inputs (**D-022**). |
| `obdlog.py` | The drive logger: every supported PID, round-robin, to `data/drive-*.jsonl` (**D-033**). Run book: `docs/hardware/first-drive.md`. |
| `obdsim.py` | The fake OBD2 port: an ELM327 emulator on a pty with a scripted drive, for development and tests, never the soak's data source (**D-023**). Sessions against it are labeled `synth`. |
| `obdsum.py` | Per-session and per-PID summary of a drive log: rates, gaps, events. |
| `obdplot.py` | PNG figures of a drive log: every PID on one time axis, speed against rpm, reply intervals. |
| `obdview.py` | One offline HTML page per drive log to scrub, zoom and read every PID at once. |
| `obdbins.py` | Operating-point binning prototype: per rpm x load cell, each PID's mean and spread. A look at the data, not the product's analysis. |
| `obdlib.py`, `elm.py`, `macgps.py` | Shared code: reply parsing and PID decoding; one adapter session; optional Mac location. |
| `train/` | Model training (skeleton). Weights land in `models/` with the training commit in the header. |

**Planned — not written yet.** Listed so nobody looks for them:

| Tool | Purpose | Board item |
| --- | --- | --- |
| `replay.py` | Feed an `obdlog` capture through the real pipeline at its recorded timing, labeled `replay` (a `synth` capture stays `synth`). A legitimate systems artifact (handout §5). Today it is an M0 stub that exits 1. | `replay`, `replayjsonl` |
| `plot_soak.py` | RSS / CPU / event-count series from soak heartbeats → the figure docs/EVALUATION.md §3 requires. | `plots` |

**Setup:** `make toolcheck` builds `.venv/` from `requirements-analysis.txt` (pyserial,
matplotlib, numpy) and runs the tool tests in `tests/tools/` (D-032). Run a tool as
`PYTHONPATH=tools .venv/bin/python tools/<tool>.py`. Logging alone needs only
`requirements.txt`; `--gps` needs `requirements-gps.txt`.

**The labeling rule is absolute.** Every record and log line these tools produce carries
its `src`: `live` only for `obdlog.py` and `pidscan.py` talking to a real car, otherwise
`replay` or `synth`, all the way through to
the report and the demo screen.
