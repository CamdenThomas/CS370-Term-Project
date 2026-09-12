# tools/train — model training (D-004)

The model is ours: our architecture, our training loop, our data, our weights. No
third-party pretrained weights anywhere in this project.

- Training runs **here**, offline, in Python.
- Weights are written to `models/<name>-<date>.bin` with a header recording the training
  commit and the fixture set used.
- **Inference is C, in `src/analysis/`.** No Python in the runtime path — see D-004 for
  why (boundary legibility, and 30 MB of interpreter RSS in a memory-stability test we
  are graded on).

Before training anything, note what the model is *not* doing: the features are the
intelligence. Operating-point binning, per-bin baselines, Mahalanobis residuals and
Theil–Sen trends are hand-written and would still work with the model file deleted. The
classifier only labels. Be able to say that at the defense.
