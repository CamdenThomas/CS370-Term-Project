# Milestones

> **⚠ VERIFY THESE DATES.** They assume a semester start of Mon 2026-08-24, which puts
> us at the end of Week 3 on 2026-09-11. Check the syllabus and correct this table —
> decision **D-008**.

| M | Deliverable | Week | Assumed due | Team hrs | Status |
|---|---|---|---|---|---|
| M0 | Team formed; hardware ordered; repo + CLAUDE.md initialized | 4 | Sun 2026-09-20 | 2–3 | 🟡 in progress |
| M1 | Problem memo; each partner's `.jsonl` transcripts copied out | 5 | Sun 2026-09-27 | 3–4 | ⬜ |
| M2 | Design document; both sensors electrically alive; transcripts | 7 | Sun 2026-10-11 | 8–10 | ⬜ |
| M3 | Checkpoint demo: both sensors through the real pipeline into real storage; ≥1 menu mechanism working; transcripts | 10 | Sun 2026-11-01 | 25–30 | ⬜ |
| M4 | Feature freeze; 48-hour soak begins; transcripts | 14 | Sun 2026-11-29 | 30–40 | ⬜ |
| M5 | Final submission: system, soak logs, evaluation report, process artifacts, complete transcripts | 15 | Sun 2026-12-06 | 10–14 | ⬜ |
| M6 | Demo day: live demo, fault injection, individual defenses | 15–16 | TBD | — | ⬜ |

Note: M4 lands on Thanksgiving week. Pull the soak start earlier if at all possible —
the handout explicitly recommends leaving margin to run the soak **twice**.

---

## M0 — exit criteria

- [x] Repo initialized, `CLAUDE.md` written and committed
- [x] Directory structure and build system in place; `make` is green on stubs
- [ ] **Team registered** as Camden Thomas + Lance Baron
- [ ] **Hardware ordered** — see `docs/hardware/BOM.md`. Shipping time is the most
      common silent schedule-killer; order the day this is read.
- [ ] Lance's Honda year/model recorded (**D-007**)
- [ ] Email to Pallickara sent re: soak-on-synthetic-CAN (**D-006**,
      `docs/professor-email-draft.md`)
- [ ] Milestone dates verified against syllabus (**D-008**)
- [ ] `cleanupPeriodDays` raised on **both** partners' machines

## M1 — exit criteria

- [ ] `PROBLEM.md` complete, all six headings, passes the phone-app test
- [ ] A **named** user and a **named** vehicle — not a persona
- [ ] Risk named plainly
- [ ] Both partners' transcripts copied to `partners/<name>/transcripts/`
- [ ] Stretch: MCP2515 module physically talking to the Pi over SPI (`ip link` shows
      the interface), even if no car is involved yet

## M2 — exit criteria

- [ ] `DESIGN.md`: architecture diagram with rates on every arrow; mechanism mapping
      justified *from user requirements*; failure-mode table; storage/retention/fsync
      policy; constraints-and-substitutions section; evaluation plan with committed
      target numbers; ownership map; AI-use plan
- [ ] Both sensors electrically alive — real frames from a real car, captured to a file
- [ ] **Three-isolated-workers check passes** on `DESIGN.md`: a design-only reviewer, a
      parts-only buyer who never sees the design, and a builder who only follows
      instructions must each succeed from their own section alone
- [ ] Baseline data collection started on both vehicles — this is the long pole and it
      cannot be compressed later

## M3 — exit criteria

- [ ] `candaemon` → ring → `storaged` → disk, end to end, on a real car
- [ ] Mechanism **B** or **D** demonstrably working and instrumented
- [ ] Overnight soak-style runs already happening at small scale. Every leak found in
      Week 9 is a crisis avoided in Week 14.
- [ ] Replay harness runs a recorded capture through the real pipeline

## M4 — exit criteria

- [ ] Feature freeze. No new features after this line; bugs only.
- [ ] All four committed mechanisms (B, D, E, F) implemented and measured
- [ ] Induced-fault captures recorded and labeled for all three diagnostics
- [ ] `make asan` and `make memcheck` clean — ASan findings cap that component at 50%
- [ ] 48-hour soak begins, with the injected fault planned and scripted

## M5 — exit criteria

- [ ] `EVALUATION.md`: latency distributions idle + loaded, per-process CPU/RSS over
      the full soak (plotted from heartbeats), one domain metric, fault-injection
      results with log excerpts, the B-mechanism interrupt-vs-polling experiment, and
      an honest limitations section
- [ ] Raw unedited soak logs committed
- [ ] `DESIGN.md` updated to as-built with a changelog of what M2 got wrong
- [ ] Both `PROMPTLOG.md` files at 6–10 annotated episodes; both `REFLECTION.md` written
- [ ] ≥40 meaningful commits, both partners represented
- [ ] README verified by someone who has never built it

## M6 — demo day

Rehearse all three movements:
1. **Demonstration** — soaked build doing its job; they will unplug a sensor. Graded on
   graceful degradation, honest reporting through our own interface, and the log line
   proving the system noticed.
2. **Individual defense** — your subsystems line by line, one `PROMPTLOG.md` judgment
   call, one question from across the ownership boundary.
3. **Live modification** — each partner alone at the keyboard makes one small unseen
   change to their own subsystem. Practice this on each other.
