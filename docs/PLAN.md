# carwatch — full project plan

**This file is the *why*. `docs/milestones.md` is the *what*.**

The work breakdown that used to live here — 80 numbered tasks with owners and blocked-by
lists — is now the checklist in `docs/milestones.md`, which `tools/board_sync.py` mirrors
onto GitHub issues and the `carwatch` project board (D-020). Keeping a second copy here
would guarantee the two disagreed by Week 9, and nothing would have told us which one was
wrong.

So: read **this** for why the plan is shaped this way and what to sacrifice when it slips.
Read **the board** for what to do next. See `docs/BOARD/BOARD.md` for how the board works.

- Milestone exit criteria: `docs/milestones.md`
- The rubric itself: `docs/handout/CS370-TermProject.md`

Items are named below by their **slug**, the short name in backticks at the end of their
line in `docs/milestones.md`.

---

## 1. The two critical paths

Everything else has slack. These two do not, and they are the only reasons this project
fails in Week 14.

### Critical path A — calendar, not effort: learning the car's normal

The model knows nothing until it has seen a car's normal driving: cold starts, short
trips and long pulls, across weeks. **That is calendar time you cannot buy back by working
harder** (D-021).

```text
baseline collection starts ──── weeks of normal driving ────▶ induced faults ────▶ measured
        M2 (mid-Oct)                                               M4                  M5
```

**Consequence:** logging must be running on a real car by the end of M2, even if the code
is ugly, even if it is just raw adapter replies logged to a file. A crude capture that starts
on time beats an elegant one that starts three weeks late. Every week it slips is a week
less of normal to learn from, and a weaker catch rate at M5.

**Status, 2026-10-28:** the first live capture exists (`captures/forester/`, 54 s at a
warm idle, 43 PIDs at 20.7 samples/s), two and a half weeks after M2's original date.
Baseline collection proper, a log of every drive, has not started: `pilogger` carries it
until `obdd` exists.

Board chain: `order` → `firstcap` (the Mac and `tools/obdlog.py`, D-033) → **`baseline`**,
carried on by `pilogger` (the same tool on the Pi at every key-on) until `obdd` exists.
Nothing physical gates it: the Pi or the laptop runs from a plain charger.

### Critical path B — hardware lead time

```text
order (M1) ──▶ adapter arrives ──▶ first live car capture ──▶ bench bring-up on the Pi ──▶ pipeline (M3)
```

Nothing downstream of "arrives" can start early. The handout names shipping time as the most
common silent schedule-killer, twice. **Order the dongle the day the paper plan is
signed.** The Bluetooth OBD2 adapter (D-019) is the whole data path — the same part runs
the PID survey, the bench bring-up and every capture after it. It has been bought, and
nothing else is on this path: the probe and the power block were cut on 2026-10-28, and
the Pi runs straight from the owner's charger.

Board chain: `order` is done; `bench` (the Pi reads the dongle, no car) is what remains
before M3's pipeline runs on a real car.

### The one cheap experiment that de-risks everything

**The PID survey (`pidforester`, `pidhonda`).** Ten minutes per car, no custom hardware —
just the adapter. It answers *which engine sensors does this car actually publish?*, which
is the model's input list. The Forester was surveyed on 2026-10-28 (43 PIDs, in
`hardware/pid-survey.md`); the CR-V has not been. Better to learn it now than in Week 11
with the analysis half-written around a sensor that does not exist.

---

## 2. Phase structure

| Phase | Weeks | Theme | The thing that must be true at the end |
| --- | --- | --- | --- |
| **M0** | 4 | Commit and order | Parts ordered, questions asked, board live |
| **M1** | 5 | Know the problem | A named user, a named risk, and PID survey results |
| **M2** | 6–7 | Argue on paper | Design settled, **logging running on a real car** |
| **M3** | 8–10 | Build the spine | Samples → ring → disk, supervised, on a real car |
| **M4** | 11–14 | Make it smart, then prove it | Analysis done, experiments measured, soak passed |
| **M5** | 15 | Account for it honestly | Report written, limits named, history clean |
| **M6** | 15–16 | Defend it | Both partners fluent in both halves |

---

## 3. What to cut, and in what order

The device shipped in Week 15 will be smaller than the one planned in Week 4. Decide the
order now, while it is cheap, so the decision is not made at 2 a.m. in Week 13:

1. **Fault-area naming** — the rule that attributes a residual to a sensor group.
   Residuals and trends still *flag* strange behavior without it, they just stop naming
   the area. Narrow the claim to flag-only in writing, and say so at the defense. (The
   trained classifier that once sat here was cut on 2026-10-28.)
2. **The report download** — `report`. The page keeps showing status and verdicts without
   it, and `obdctl` still produces every number in it (D-028).
3. **The phone status page** — `phoneview`. `obdctl` alone still satisfies "honest
   reporting through your own interface"; the page is the product's face, not a graded
   mechanism (D-013). Cut it before any `obdctl` verb.
4. **The remaining `obdctl` verbs** — `obdrest`, down to `status` + `verdicts`.

A native phone app is already out of scope: a stretch goal, not on the board (D-028).

**Never cut, in any circumstance:** the soak (`soak1`), the committed mechanism
experiments (`expcrash`, `expkill`), the limitations section (`evalreport`), or the
transcript copy-outs (`tx1`–`tx4`). Those are graded directly, and three of them cannot be
reconstructed after the fact.

**How to cut, mechanically.** Do not delete the line — a deleted line leaves an open
issue behind that sync no longer manages, and the decision to cut disappears with it.
Instead: tick the box, add "(cut)" to its text, say why in that commit, and
sync. The board then shows a cut that was made deliberately rather than a task that
quietly stopped being mentioned.

---

## 4. Standing weekly rhythm

Every session starts and ends by `CLAUDE.md` §0: read the board, `--status`, sync
and copy transcripts out at the end. On top of that:

- **Monday:** `python tools/board_sync.py --status`, then work the open boxes in
  milestone order. Since D-020 there are no dependencies on the board, so the order within
  a milestone in `docs/milestones.md` is the plan; look for open lines nobody has said they
  are on.
- **Friday:** cross-review at least one of the other's merged PRs, and log it in
  `PROMPTLOG.md` while it is fresh.
