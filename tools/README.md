# tools/ — Python is allowed HERE ONLY

Per the handout: the systems core is C; the interface layer may use anything, but **no
graded mechanism may hide in it.** Nothing in this directory is part of the shipped
product's runtime path.

**Present:**

| Tool | Purpose |
|---|---|
| `board_sync.py` | Reconciles `docs/board.toml` onto GitHub issues and the project board. Not a product tool at all — it is how the plan is maintained. See `docs/BOARD.md`. |
| `replay.py` | Feed a labeled capture through the real pipeline at real or accelerated speed. A legitimate systems artifact (handout §5), not a workaround — but everything it produces is labeled `replay`. |
| `pidscan.py` | Mode 01 supported-PID bitmap scan. Feeds `pid-survey.md`; the input to **D-007**. |
| `train/` | Model training (skeleton). Weights land in `models/` with the training commit in the header. |

**Planned — not written yet.** Listed so nobody looks for them:

| Tool | Purpose | Board item |
|---|---|---|
| `cangen.py` | Bench rig: generate CAN traffic over a USB-to-CAN adapter for long-run rehearsal. Output is labeled `synth`. See **D-006**. | `twonode` |
| `plot_soak.py` | RSS / CPU / event-count series from soak heartbeats → the figure docs/EVALUATION.md §3 requires. | `plots` |

**The labeling rule is absolute.** Anything these tools produce carries
`src=replay` or `src=synth` in every record and every log line, all the way through to
the report and the demo screen.
