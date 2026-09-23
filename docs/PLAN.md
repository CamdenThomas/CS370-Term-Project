# carwatch — full project plan

**This file is the *why*. `docs/BOARD/board.toml` is the *what*.**

The work breakdown that used to live here — 80 numbered tasks with owners and blocked-by
lists — moved to `docs/BOARD/board.toml`, which `tools/board_sync.py` reconciles onto GitHub
issues and the `carwatch` project board. Keeping a second copy here would guarantee the two
disagreed by Week 9, and nothing would have told us which one was wrong.

So: read **this** for why the plan is shaped this way and what to sacrifice when it slips.
Read **the board** for what to do next. See `docs/BOARD/BOARD.md` for how the board works.

- Decisions already made: `docs/DECISIONS.md`
- Milestone exit criteria: `docs/milestones.md`
- The rubric itself: `docs/handout/CS370-TermProject.md`

Items are named below by their board **slug**, which is permanent. Issue numbers are not
written into any document — they live only in `docs/BOARD/board.lock.json`, which sync maintains.

---

## 1. The two critical paths

Everything else has slack. These two do not, and they are the only reasons this project
fails in Week 14.

### Critical path A — calendar, not effort: the oil interval

Diagnostic #1 claims oil pressure degrades measurably across an oil-change interval. **That
claim needs a real interval to develop, and an interval is calendar time you cannot buy
back by working harder.**

```text
baseline collection starts ──── 5-7 weeks of driving ────▶ oil change ──── 2 weeks ────▶ trend is real
        M2 (mid-Oct)                                       (~mid-Nov)                     M4 (late Nov)
```

**Consequence:** logging must be running on the CR-V by the end of M2, even if the code is
ugly, even if it is just raw adapter replies logged to a file. A crude capture that starts
on time beats an elegant one that starts three weeks late. **If baseline collection has
not started by the M2 deadline, diagnostic #1 is dead** and we fall back to diagnostics #2
and #3 — decide that consciously, record it in `DECISIONS.md`, and narrow the claim in
writing.

Board chain: `order` → `bench` → `firstcap` → **`baseline`** → `foil`, with `keyoff`
(the USB-C supply proven) also gating `baseline`.

### Critical path B — hardware lead time

```text
order (M0) ──▶ adapter arrives ──▶ bench bring-up ──▶ first live car capture ──▶ pipeline (M3)
```

Nothing downstream of "arrives" can start early. The handout names shipping time as the most
common silent schedule-killer, twice. **Order on the day you read this.** The Bluetooth
OBD2 adapter (D-019) is the whole data path — the same part runs the PID survey, the bench
bring-up and every capture after it.

Board chain: `order` blocks the PID survey, `bench` and `keyoff`, and everything physical
after them.

### The one cheap experiment that de-risks everything

**The PID survey (`pidhonda`).** Ten minutes, no custom hardware — just the adapter. It
answers the project's named risk: *does the CR-V actually publish analog oil pressure?*
If the answer is no, we find out in Week 5 with the whole design still soft, instead of
Week 11 with the analysis engine half-written around a value that does not exist. It
feeds `oilq`, which is where the answer gets recorded.

---

## 2. Phase structure

| Phase | Weeks | Theme | The thing that must be true at the end |
| --- | --- | --- | --- |
| **M0** | 4 | Commit and order | Parts ordered, questions asked, board live |
| **M1** | 5 | Know the problem | A named user, a named risk, and PID survey results |
| **M2** | 6–7 | Argue on paper | Design settled, **logging running on the CR-V** |
| **M3** | 8–10 | Build the spine | Samples → ring → disk, supervised, on a real car |
| **M4** | 11–14 | Make it smart, then prove it | Analysis done, experiments measured, soak passed |
| **M5** | 15 | Account for it honestly | Report written, limits named, history clean |
| **M6** | 15–16 | Defend it | Both partners fluent in both halves |

---

## 3. What to cut, and in what order

The device shipped in Week 15 will be smaller than the one planned in Week 4. Decide the
order now, while it is cheap, so the decision is not made at 2 a.m. in Week 13:

1. **Diagnostic #1 (oil)** if critical path A slips — `foil`, and the oil claim in
   `docs/PROBLEM.md`. Fall back to #2 and #3, narrow the claim in writing, and say so in the
   limitations section.
2. **The trained classifier** — `train`, `infer`. The features are the intelligence;
   residuals and trends still produce verdicts without it. Say so at the defense.
3. **The phone status page** — `phoneview`. `obdctl` alone still satisfies "honest
   reporting through your own interface"; the page is the product's face, not a graded
   mechanism (D-013). Cut it before any `obdctl` verb.
4. **The remaining `obdctl` verbs** — `obdrest`, down to `status` + `verdicts`.

**Never cut, in any circumstance:** the soak (`soak1`), the committed mechanism
experiments (`expcrash`, `expkill` — plus `expirq` and `expdrop` if `pisensor` commits B
and F), the limitations section (`evalreport`), or the transcript copy-outs (`tx1`–`tx4`).
Those are graded directly, and three of them cannot be reconstructed after the fact.

**How to cut, mechanically.** Do not delete the `[[item]]` — a deleted item leaves an open
issue behind that sync no longer manages, and the decision to cut disappears with it.
Instead: record the cut in `DECISIONS.md`, add a line to the item's body saying it was cut
and why, set `status = "done"`, and close the issue by hand. The board then shows a cut that
was made deliberately rather than a task that quietly stopped being mentioned.

---

## 4. Standing weekly rhythm

- **Monday:** `python tools/board_sync.py --check`, then work the **Ready** column. Cards
  move out of Backlog on their own as blockers close — what you are looking for is anything
  Ready that nobody has picked up, and anything still Backlog that should not be.
- **Both partners, every session:** read the `DECISIONS.md` review queue and sign or contest.
- **Friday:** cross-review at least one of the other's merged PRs, and log it in
  `PROMPTLOG.md` while it is fresh.
- **Every milestone:** transcripts out, no exceptions, before anything else is submitted.
- **End of every session:** `python tools/board_sync.py`. A run with nothing to do writes
  nothing, so there is no excuse for skipping it.
