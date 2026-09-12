# carwatch — full project plan

**This file is the source of truth the GitHub board is generated from.**
`tools/bootstrap_board.sh` turns every task below into an issue with its owner, milestone
and blocked-by dependencies. Read this for *why the plan is shaped this way*; use the board
for *what to do next*.

- Decisions already made: `docs/DECISIONS.md`
- Milestone exit criteria: `docs/milestones.md`
- The rubric itself: `docs/handout/CS370-TermProject.md`

---

## 1. The two critical paths

Everything else has slack. These two do not, and they are the only reasons this project
fails in Week 14.

### Critical path A — calendar, not effort: the oil interval

Diagnostic #1 claims oil pressure degrades measurably across an oil-change interval. **That
claim needs a real interval to develop, and an interval is calendar time you cannot buy
back by working harder.**

```
baseline collection starts ──── 5-7 weeks of driving ────▶ oil change ──── 2 weeks ────▶ trend is real
        M2 (mid-Oct)                                       (~mid-Nov)                     M4 (late Nov)
```

**Consequence:** logging must be running on both cars by the end of M2, even if the code
is ugly, even if it is just `candump` to a file. A crude capture that starts on time beats
an elegant one that starts three weeks late. **If baseline collection has not started by the
M2 deadline, diagnostic #1 is dead** and we fall back to diagnostics #2 and #3 — decide that
consciously, record it in `DECISIONS.md`, and narrow the claim in writing.

### Critical path B — hardware lead time

```
order (M0) ──▶ arrives ──▶ bench bring-up ──▶ two-node bench bus ──▶ first live car capture ──▶ pipeline (M3)
```

Nothing downstream of "arrives" can start early. The handout names shipping time as the most
common silent schedule-killer, twice. **Order on the day you read this**, and order the $12
ELM327 in the same cart — it unblocks the PID survey weeks before the MCP2515 matters.

### The one cheap experiment that de-risks everything

**The PID survey (T05, T06).** Ten minutes per car, $12, no custom hardware. It answers the
project's named risk: *does either testbed actually publish analog oil pressure?* If the
answer is no, we find out in Week 4 with the whole design still soft, instead of Week 11
with the analysis engine half-written around a value that does not exist.

---

## 2. Phase structure

| Phase | Weeks | Theme | The thing that must be true at the end |
|---|---|---|---|
| **M0** | 4 | Commit and order | Parts ordered, questions asked, board live |
| **M1** | 5 | Know the problem | A named user, a named risk, and PID survey results |
| **M2** | 6–7 | Argue on paper | Design settled, **logging running on both cars** |
| **M3** | 8–10 | Build the spine | Frames → ring → disk, supervised, on a real car |
| **M4** | 11–14 | Make it smart, then prove it | Analysis done, experiments measured, soak passed |
| **M5** | 15 | Account for it honestly | Report written, limits named, history clean |
| **M6** | 15–16 | Defend it | Both partners fluent in both halves |

---

## 3. Work breakdown

Legend — **Owner:** C = Camden, L = Lance, C+L = both.
**Kind:** `task` · `question` (human answer needed) · `decision` · `hardware` (physical world).

### M0 — Commit and order

| ID | Task | Owner | Kind | Blocked by |
|---|---|---|---|---|
| T01 | Register the team as Camden Thomas + Lance Baron | C | task | — |
| T02 | Order core BOM — Pi, MCP2515, OBD2 pigtail, buck converter, SD cards, **and the $12 ELM327** | C | hardware | — |
| T03 | Confirm Lance's Honda year / model / engine / transmission | L | question | — |
| T04 | Verify M0–M6 dates against the syllabus; correct `docs/milestones.md` | C | question | — |
| T05 | Email Pallickara re: 48-hour soak on a synthesized CAN source (D-006) | C | decision | — |
| T06 | Configure GitHub: branch protection, labels, milestones, project board | C | task | — |
| T07 | Raise `cleanupPeriodDays` on both machines; verify transcript paths | C+L | task | — |
| T08 | Lance reads and signs `docs/DECISIONS.md` §A.1, B.1, C.1, D.1 | L | task | — |

### M1 — Know the problem

| ID | Task | Owner | Kind | Blocked by |
|---|---|---|---|---|
| T10 | PID survey on the Outback — record in `docs/hardware/pid-survey.md` | C | hardware | T02 |
| T11 | PID survey on the Honda | L | hardware | T02, T03 |
| T12 | **Decide: is analog oil pressure available?** If not, pick the fallback | C+L | decision | T10, T11 |
| T13 | Name the real user and vehicle in `PROBLEM.md` — a person, not a persona | C | question | — |
| T14 | Draft `PROBLEM.md`, all six headings | C | task | T13, T12 |
| T15 | Lance reviews `PROBLEM.md` against the phone-app test | L | task | T14 |
| T16 | M1 transcript copy-out, both partners | C+L | task | — |

### M2 — Argue on paper, and start the clock

| ID | Task | Owner | Kind | Blocked by |
|---|---|---|---|---|
| T20 | Bench bring-up: MCP2515 on the Pi, `ip link show can0`, no car involved | C | hardware | T02 |
| T21 | Two-node bench bus: prove frames move before a car is attached | C | hardware | T20 |
| T22 | First live capture from a car — `candump` to a file | C | hardware | T21 |
| T23 | **Start baseline collection on both vehicles** — crude is fine, on time is not optional | C+L | task | T22 |
| T24 | Decide the record format: fields, framing, CRC placement | L | decision | — |
| T25 | Decide fsync discipline — batch size, interval, and the argument from user needs | L | decision | T24 |
| T26 | Architecture diagram with a rate on every arrow | C | task | — |
| T27 | Failure-mode table — write the modes you are afraid of, not the handled ones | C+L | task | T26 |
| T28 | Evaluation plan with committed target numbers | L | task | T26 |
| T29 | Constraints-and-substitutions section — what we wanted vs. what we bought | C | task | T12 |
| T30 | Assemble `DESIGN.md`; run the three-isolated-workers check | C+L | task | T26, T27, T28, T29, T25 |
| T32 | KiCad: populate `CS370_Project_Library.kicad_sym`; resolve the sym-lib-table error | L | task | — |
| T33 | Schematic: draw the Power block (OBD2 pin 16, fuse, buck, bulk + bypass) | L | task | — |
| T34 | Schematic: draw the CAN block (transceiver, MCP2515, crystal, termination, INT) | L | task | T20 |
| T35 | Schematic: draw the Pi interface block (40-pin header, SPI0, IRQ GPIO, grounds) | L | task | T20 |
| T36 | **Measure and record** MCP2515 module VCC and MISO levels with the Pi disconnected | C | hardware | T02 |
| T37 | `make hwcheck` green; regenerate `wiring.md` pin tables and `BOM.md` from the schematic | L | task | T33, T34, T35 |
| T31 | M2 transcript copy-out | C+L | task | — |

### M3 — Build the spine

| ID | Task | Owner | Kind | Blocked by |
|---|---|---|---|---|
| T40 | `candaemon`: open SocketCAN, epoll on the IRQ path | C | task | T22, T30 |
| T41 | `candaemon`: `--poll` variant behind a flag, for the mechanism-B comparison | C | task | T40 |
| T42 | Move the ring into shared memory; prove cross-process | C | task | T30 |
| T43 | `storaged`: record write path with the chosen fsync policy | L | task | T24, T25 |
| T44 | `storaged`: recovery scan, CRC verify, torn-tail truncation | L | task | T43 |
| T45 | `supervisor`: fork/exec, `waitpid`, fd hygiene across restart | C+L | task | T30 |
| T46 | `supervisor`: backoff + restart-storm guard; distinguish crashed from unplugged | C+L | task | T45 |
| T47 | Wire hourly heartbeats (liveness, RSS, event counts) through every daemon | C+L | task | T45 |
| T48 | `obdctl`: UDS transport + `status` verb | C+L | task | T45 |
| T49 | Replay harness: feed a capture through the real pipeline, preserving timing | L | task | T43 |
| T50 | **Begin overnight small-scale soak runs** — every leak found now is a Week-14 crisis avoided | C+L | task | T47 |
| T51 | Checkpoint demo rehearsal (M3 is graded live) | C+L | task | T40, T43, T47 |
| T52 | M3 transcript copy-out | C+L | task | — |

### M4 — Make it smart, then prove it

| ID | Task | Owner | Kind | Blocked by |
|---|---|---|---|---|
| T60 | Analysis: operating-point binning (RPM × load × coolant temp) | L | task | T43 |
| T61 | Analysis: per-bin running mean/variance (Welford) | L | task | T60 |
| T62 | Analysis: persist baselines through a power cut | L | task | T61, T44 |
| T63 | Analysis: multivariate residual (Mahalanobis) | L | task | T61 |
| T64 | Analysis: robust trend estimation (Theil–Sen) | L | task | T61 |
| T65 | Analysis: state machine with hysteresis and dwell | L | task | T63, T64 |
| T66 | Training pipeline in `tools/train/`; weights format with provenance header | L | task | T63 |
| T67 | C inference over our own weights; no interpreter in the runtime path | L | task | T66, T65 |
| T68 | `obdctl`: remaining four verbs (`verdicts`, `series`, `baseline`, `faults`) | C+L | task | T48, T65 |
| T70 | **Experiment:** interrupt vs. polling — latency distribution + CPU, idle and loaded | C | task | T41 |
| T71 | **Experiment:** no-drop under contention, proven by sequence accounting | C | task | T42 |
| T72 | **Experiment:** crash consistency — N power-cut trials, bytes lost each time | L | task | T44 |
| T73 | Measure key-off current draw before leaving the device in a car overnight | C | hardware | T22 |
| T75 | Induce + label: vacuum leak, both vehicles | C+L | hardware | T60 |
| T76 | Induce + label: cooling anomaly (blocked radiator airflow) | C+L | hardware | T60 |
| T77 | Induce + label: sensor unplug — also the demo-day rehearsal | C | hardware | T46 |
| T78 | **Capture across a real oil change on both vehicles** — critical path A | C+L | hardware | T23 |
| T79 | Enclosure and in-vehicle mounting | C | hardware | T73 |
| T80 | **FEATURE FREEZE** | C+L | task | T67, T68, T70, T71, T72 |
| T81 | 48-hour soak, run #1, with a scheduled injected fault | C+L | task | T80, T77 |
| T82 | 48-hour soak, run #2 — the margin the handout says you will want | C+L | task | T81 |
| T83 | `make asan` and `make memcheck` clean across all components | C+L | task | T80 |
| T84 | M4 transcript copy-out | C+L | task | — |

### M5 — Account for it honestly

| ID | Task | Owner | Kind | Blocked by |
|---|---|---|---|---|
| T90 | `plot_soak.py`: RSS / CPU / event-count figures from the soak heartbeats | L | task | T82 |
| T91 | `EVALUATION.md`: latency, footprint, domain metric, fault injection, comparisons, limits | C+L | task | T90, T70, T71, T72, T75, T76 |
| T92 | `DESIGN.md` updated to as-built, with a changelog of what M2 got wrong | C+L | task | T80 |
| T93 | README verified by someone who has never built it, on a clean Pi | C+L | task | T80 |
| T94 | `PROMPTLOG.md` — 6–10 episodes, required coverage complete (per partner) | C+L | task | — |
| T95 | `REFLECTION.md` — one page (per partner) | C+L | task | T91 |
| T96 | Commit audit: ≥ 40 meaningful commits, both partners well represented | C | task | — |
| T97 | M5 transcript copy-out and tarball assembly | C+L | task | T91, T92, T94, T95 |

### M6 — Defend it

| ID | Task | Owner | Kind | Blocked by |
|---|---|---|---|---|
| T98 | Demo rehearsal including surprise fault injection by the other partner | C+L | task | T82 |
| T99 | Live-modification practice — each partner, alone, on their own subsystem | C+L | task | T93 |
| T100 | Cross-boundary quiz: each partner defends the other's subsystem | C+L | task | T93 |

---

## 4. What to cut, and in what order

The device shipped in Week 15 will be smaller than the one planned in Week 4. Decide the
order now, while it is cheap, so the decision is not made at 2 a.m. in Week 13:

1. **The second vehicle** (T11, and the Honda half of T75/T76/T78). Halves the decode and
   baseline work. Costs the strongest five seconds of the demo, but not the grade.
2. **Diagnostic #1 (oil)** if critical path A slips. Fall back to #2 and #3, narrow the
   claim in writing, and say so in the limitations section.
3. **The trained classifier** (T66, T67). The features are the intelligence; residuals and
   trends still produce verdicts without it. Say so at the defense.
4. **The remaining `obdctl` verbs** (T68) down to `status` + `verdicts`.

**Never cut, in any circumstance:** the soak (T81), the mechanism comparison experiments
(T70–T72), the limitations section (T91), or the transcript copy-outs. Those are graded
directly, and three of them cannot be reconstructed after the fact.

---

## 5. Standing weekly rhythm

- **Monday:** review the board; anything `Blocked` whose blocker is closed moves to `Ready`.
- **Both partners, every session:** read the `DECISIONS.md` review queue and sign or contest.
- **Friday:** cross-review at least one of the other's merged PRs, and log it in
  `PROMPTLOG.md` while it is fresh.
- **Every milestone:** transcripts out, no exceptions, before anything else is submitted.
