# tools/ — Python is allowed HERE ONLY

Per the handout: the systems core is C; the interface layer may use anything, but **no
graded mechanism may hide in it.** Nothing in this directory is part of the shipped
product's runtime path.

| Tool | Purpose |
|---|---|
| `replay.py` | Feed a labeled capture through the real pipeline at real or accelerated speed. A legitimate systems artifact (handout §5), not a workaround — but everything it produces is labeled `replay`. |
| `cangen.py` | Bench rig: generate CAN traffic over a USB-to-CAN adapter for long-run rehearsal. Output is labeled `synth`. See **D-006**. |
| `plot_soak.py` | RSS / CPU / event-count series from soak heartbeats → the figure EVALUATION.md §3 requires. |
| `pidscan.py` | Mode 01 supported-PID bitmap scan. Closes **D-007**. |
| `train/` | Model training. Weights land in `models/` with the training commit in the header. |

**The labeling rule is absolute.** Anything these tools produce carries
`src=replay` or `src=synth` in every record and every log line, all the way through to
the report and the demo screen.
