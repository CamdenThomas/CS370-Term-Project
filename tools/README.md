# tools/ — Python is allowed HERE ONLY

Per the handout: the systems core is C; the interface layer may use anything, but **no
graded mechanism may hide in it.** Nothing in this directory is part of the shipped
product's runtime path.

**Present:**

| Tool | Purpose |
| --- | --- |
| `board_sync.py` | Reconciles `docs/BOARD/board.toml` onto GitHub issues and the project board. Not a product tool at all — it is how the plan is maintained. See `docs/BOARD/BOARD.md`. |
| `replay.py` | Feed a labeled capture through the real pipeline at real or accelerated speed. A legitimate systems artifact (handout §5), not a workaround — but everything it produces is labeled `replay`. |
| `pidscan.py` | Mode 01 supported-PID bitmap scan. Feeds `pid-survey.md`; the input to **D-007**. |
| `train/` | Model training (skeleton). Weights land in `models/` with the training commit in the header. |

**Planned — not written yet.** Listed so nobody looks for them:

| Tool | Purpose | Board item |
| --- | --- | --- |
| `obdsim.py` | The fake OBD2 port: an ELM327 emulator on a pty that answers Mode 01 requests from recorded captures, for soak rehearsal and testing. Output is labeled `synth`. See **D-006**. | `obdsim` |
| `plot_soak.py` | RSS / CPU / event-count series from soak heartbeats → the figure docs/EVALUATION.md §3 requires. | `plots` |

**The labeling rule is absolute.** Anything these tools produce carries
`src=replay` or `src=synth` in every record and every log line, all the way through to
the report and the demo screen.
