# Milestones

**This list is the board.** Every checkbox below is one GitHub issue (D-020). Tick a box
when the thing is true, run `python tools/board_sync.py`, and GitHub follows: the issue
closes, the table's progress updates, and the project board moves the card to Done. Add a
line to add an issue; sync writes its hidden `<!-- #n -->` tag. How it works:
`docs/BOARD/BOARD.md`.

Bold lines are each milestone's exit criteria; the plain lines under them are the tasks
that get there. A trailing `` `slug` `` is the short name other docs cite. Open lines have
no owner: any of them is for whoever is free (D-026). Ticked lines keep their `@camden` /
`@lance` as the record of who did the work.

<!-- board-sync:milestones -->

| M | Deliverable | Week | Due | Team hrs | Progress |
| --- | --- | --- | --- | --- | --- |
| M0 | Team formed; hardware ordered; repo + CLAUDE.md initialized | 4 | 2026-09-20 | 2–3 | 🟡 8/9 |
| M1 | Problem memo; each partner's `.jsonl` transcripts copied out | 5 | 2026-09-27 | 3–4 | 🟡 18/23 |
| M2 | Design document; both sensors electrically alive; **baseline collection starts** | 6–7 | 2026-10-11 | 8–10 | 🟡 2/22 |
| M3 | Checkpoint demo: sensors through pipeline into storage; ≥1 menu mechanism working | 8–10 | 2026-11-01 | 25–30 | ⬜ 0/17 |
| M4 | Feature freeze; 48-hour soak begins | 11–14 | 2026-11-29 | 30–40 | 🟡 1/26 |
| M5 | Final submission: system, soak logs, eval report, process artifacts, transcripts | 15 | 2026-12-06 | 10–14 | ⬜ 0/9 |
| M6 | Demo day: live demo, fault injection, individual defenses (date assumed — see `dates`) | 15–16 | 2026-12-18 | — | ⬜ 0/3 |

<sub>Edit Deliverable, Week, Due and hours by hand; sync fills Progress and sets the GitHub
milestone due dates. Dates verified against the syllabus 2026-09-21 (D-008).</sub>
<!-- /board-sync:milestones -->

Note: M4 lands on Thanksgiving week. Pull the soak start earlier if at all possible —
the handout explicitly recommends leaving margin to run the soak **twice**.

## M0 — Team formed; hardware ordered; repo + CLAUDE.md initialized

- [x] **Repo initialized, `CLAUDE.md` written and committed** <!-- #116 -->
- [x] **Directory structure and build system in place; `make` is green on stubs**
      <!-- #117 -->
- [x] **Board live on GitHub, driven from this list** <!-- #118 -->
- [x] **Team registered as Camden Thomas + Lance Baron** `team` @camden <!-- #3 -->
- [x] **Lance's Honda year/model recorded (D-007)** `honda` @lance <!-- #5 -->
- [x] **Milestone dates verified against the syllabus (D-008)** `dates` @camden
      <!-- #6 -->
- [x] **`cleanupPeriodDays` raised on both partners' machines** `cleanup` @camden
      <!-- #9 -->
- [x] **Branch protection set on `main` (web UI; the API cannot do it)** `ghsetup` @camden
      <!-- #8 -->
- [ ] M0 review: Lance reads and signs the decision log `lancesign` <!-- #10 -->

## M1 — Problem memo; each partner's `.jsonl` transcripts copied out

- [x] **`docs/PROBLEM.md` complete, all six headings, passes the phone-app test** `memo`
      @camden <!-- #15 -->
- [x] **A named user and a named vehicle, not a persona** `nameuser` @camden <!-- #14 -->
- [x] **Risk named plainly** <!-- #119 -->
- [x] **Both partners' transcripts copied to `partners/<name>/transcripts/`** `tx1`
      <!-- #17 -->
- [ ] **Hardware ordered (`docs/hardware/BOM.md`): the dongle, a USB-C cable, microSD;
      power-block parts only after the design is signed** `order` <!-- #4 -->
- [x] M1 hardware: supported-PID survey on the Outback (cut) `pidsub` @camden <!-- #11 -->
- [ ] M1 hardware: supported-PID survey on the Honda `pidhonda` <!-- #12 -->
- [x] M1 decision: is analog oil pressure available? If not, what replaces diagnostic #1?
      (cut, D-021) `oilq` <!-- #13 -->
- [ ] M1 review: Lance reviews docs/PROBLEM.md against the phone-app test `memorev`
      <!-- #16 -->
- [x] M1 docs: factor the plug-in product design through the repo `productform` @camden
      <!-- #86 -->
- [x] M1 docs: overhaul the repo for the Bluetooth OBD2, one-car design `overhaul`
      @camden
      <!-- #87 -->
- [x] M1 process: the transcript copy-out glob also matches the HW1 project folder
      `txglob` @camden <!-- #99 -->
- [x] M1 tools: board_sync still looks for docs/board.toml after the move to docs/BOARD/
      `boardpath` @camden <!-- #96 -->
- [x] M1 process: recover Camden's M0 transcripts from the Windows machine before they
      purge `txwin` @camden <!-- #97 -->
- [x] M1 tools: the 8e8c74a merge left board.toml and board.lock.json unparseable
      `boardmerge` @camden <!-- #102 -->
- [x] M1 docs: grammar, spelling and format pass over docs/PROBLEM.md `memopolish` @camden
      <!-- #103 -->
- [x] M1 docs: markdownlint and cSpell configs, and a clean pass over every doc `mdlint`
      @camden <!-- #104 -->
- [x] M1 docs: read OBD2 over a Bluetooth Classic (SPP) adapter instead of USB `btpivot`
      @camden <!-- #105 -->
- [x] M1 docs: condense CLAUDE.md into a rules-only index over the source docs
      `claudeindex` @camden <!-- #106 -->
- [x] M1 tools: make docs/milestones.md the board; retire board.toml (D-020) `boardlist`
      @camden <!-- #120 -->
- [x] M1 docs: reframe the repo as a one-car monitor that names the fault area (D-021)
      `reframe` @camden <!-- #127 -->
- [ ] M1 decision: add electricalDrawing/ and the Makefile to LOCKED ownership map F.2
      `ownmap` <!-- #112 -->
- [ ] M1 hw: stop tracking electricalDrawing/.kicad_prl and .history `kicadtrack`
      <!-- #113 -->

## M2 — Design document; both sensors electrically alive; **baseline collection starts**

- [ ] **`docs/DESIGN.md` complete and the three-isolated-workers check passes**
      `designdoc` <!-- #28 -->
- [ ] **Both sensors alive: real Mode 01 replies from a real car, captured to a file**
      `firstcap` <!-- #20 -->
- [ ] **Baseline data collection started on a real car; the model learns normal from it**
      `baseline` <!-- #21 -->
- [ ] M2 decision: 48-hour soak on a fake OBD2 port — ask Pallickara `soakq`
      <!-- #7 -->
- [ ] M2 decision: get the professor's 48-hour soak definition (D-023) in writing
      `profmail`
- [ ] M2 docs: record the box redesign as D-022 to D-031 and bring every doc in line
      `boxdocs`
- [ ] M2 hardware: Bluetooth OBD2 adapter bring-up on the Pi, no car `bench`
      <!-- #18 -->
- [ ] M2 hardware: supported-PID survey on the 2008 Forester, with protocol and VIN
      `pidforester`
- [x] M2 hardware: two-node bench CAN bus — prove frames move before a car is attached
      (cut) `twonode` @camden <!-- #19 -->
- [ ] M2 decision: finalize the on-disk format, including transaction and boot records
      `recfmt` <!-- #22 -->
- [ ] M2 decision: fsync discipline — batch size and interval `fsync` <!-- #23 -->
- [ ] M2 docs: architecture diagram with a rate on every arrow `archdiag`
      <!-- #24 -->
- [ ] M2 docs: failure-mode table `failtable` <!-- #25 -->
- [ ] M2 docs: evaluation plan with committed target numbers `evalplan`
      <!-- #26 -->
- [ ] M2 docs: constraints and substitutions section `substit` <!-- #27 -->
- [ ] M2 hw: populate CS370_Project_Library.kicad_sym `symlib` <!-- #29 -->
- [ ] M2 hw: draw the Power block (USB-C sink, protection, limiter, supercapacitor,
      converter) `schpower` <!-- #30 -->
- [ ] M2 hw: draw the CAN block `schcan` <!-- #31 -->
- [ ] M2 hw: draw the Pi interface block (light, source-present, halt-done) `schpi`
      <!-- #32 -->
- [x] M2 hw: measure MCP2515 module VCC and MISO with the Pi DISCONNECTED (cut) `meterv`
      @camden <!-- #33 -->
- [ ] M2 hw: make hwcheck green; regenerate wiring.md and BOM.md from the schematic
      `hwgate` <!-- #34 -->
- [ ] M2 process: transcript copy-out `tx2` <!-- #35 -->
- [ ] M2 hardware: measure Bluetooth and Wi-Fi sharing the Pi 4's one radio `btcoex`
      <!-- #110 -->
- [ ] M2 decision: add a sensor on the Pi itself — MPU-6050, DS18B20, both, or neither?
      `pisensor` <!-- #93 -->
- [ ] M2 hardware: prove the box's USB-C input: charger dead at key-off, crank dip,
      undervoltage `keyoff` <!-- #61 -->
- [ ] M2 hardware: measure the OBD2 dongle's key-off current from pin 16 `dongledrain`
- [ ] M2 docs: power worksheet: Pi draw by state, shutdown and crank energy, bank size
      `powerspec`
- [ ] M2 docs: power-event state machine and the grace period `shutfsm`
- [ ] M2 docs: tri-color light patterns, priority, the boot state and the LED part
      `ledspec`
- [ ] M2 docs: report contents and format, box fault versus car fault `reportspec`
- [ ] M2 docs: real-time clock: part, backup cell, cabin temperature rating `rtcspec`
- [ ] M2 docs: boot-time budget and what the first minute of a drive loses `bootbudget`
- [ ] M2 docs: box mechanical spec: size, vents, heat-rated plastic, antenna clearance
      `enclspec`
- [ ] M2 hardware: record the supply dip at crank with a meter `cranklog`
- [ ] M2 hardware: supercapacitor bench test into a dummy load, no Pi, after the design
      is signed `capbench`

## M3 — Checkpoint demo: sensors through pipeline into storage; ≥1 menu mechanism working

- [ ] **`obdd` → ring → `storaged` → disk, end to end, on a real car** `m3demo`
      <!-- #47 -->
- [ ] **Mechanism D or E demonstrably working and instrumented** <!-- #121 -->
- [ ] **Overnight soak-style runs already happening at small scale** `nightly`
      <!-- #46 -->
- [ ] **Replay harness runs a recorded capture through the real pipeline** `replay`
      <!-- #45 -->
- [ ] M3 capture: interrupt-driven read path for the Pi-side sensor `canirq`
      <!-- #36 -->
- [ ] M3 capture: --poll variant behind a flag for the mechanism-B comparison `canpoll`
      <!-- #37 -->
- [ ] M3 obd: rate-limited Mode 01 requester on the adapter tty `canreq`
      <!-- #88 -->
- [ ] M3 analysis: read committed records from the log as storaged appends them `logread`
- [ ] M3 ipc: move the ring into shared memory, prove cross-process `ringshm`
      <!-- #38 -->
- [ ] M3 store: write path: BEGIN, records, COMMIT, one fsync per batch `storewrite`
      <!-- #39 -->
- [ ] M3 store: recovery: CRC scan, torn-tail and uncommitted-batch truncation
      `storerecover` <!-- #40 -->
- [ ] M3 supervisor: fork/exec, waitpid, fd hygiene across restart `supfork` <!-- #41 -->
- [ ] M3 supervisor: backoff and restart-storm guard `supbackoff` <!-- #42 -->
- [ ] M3 supervisor: drive the tri-color light from child health, verdicts and power
      events `led` <!-- #89 -->
- [ ] M3 common: wire hourly heartbeats through every daemon `heartbeat` <!-- #43 -->
- [ ] M3 interface: obdctl UDS transport + status verb `obdstatus` <!-- #44 -->
- [ ] M3 tools: fake OBD2 port — an ELM327 emulator on a pty, labeled synth `obdsim`
      <!-- #90 -->
- [ ] M3 supervisor: power-event handling and clean shutdown `pwrmon`
- [ ] M3 supervisor: set the clock from the RTC at boot; flag invalid time `rtcboot`
- [ ] M3 interface: the box's own Wi-Fi access point, WPA2, no upstream `wifiap`
- [ ] M3 interface: report generator (HTML and CSV), labeled by data source `report`
- [ ] M3 process: trimmed Pi image and boot-time measurement `bootimg`
- [ ] M3 process: transcript copy-out `tx3` <!-- #48 -->

## M4 — Feature freeze; 48-hour soak begins

- [ ] **Feature freeze: no new features after this line; bugs only** `freeze` <!-- #67 -->
- [ ] **Mechanisms D and E implemented and measured** <!-- #122 -->
- [ ] **Induced-fault captures recorded and labeled for each fault area (D-021)**
      <!-- #123 -->
- [ ] **`make asan` and `make memcheck` clean everywhere** `sanitize` <!-- #70 -->
- [ ] **48-hour key-cycle soak begins (D-023), with the injected fault planned and
      scripted** `soak1` <!-- #68 -->
- [ ] M4 analysis: operating-point binning (RPM x load x coolant temp) `bin`
      <!-- #49 -->
- [ ] M4 analysis: per-bin running mean and variance (Welford) `welford`
      <!-- #50 -->
- [ ] M4 analysis: A/B baseline checkpoints, and redo from the log after a power cut
      `persist` <!-- #51 -->
- [ ] M4 analysis: multivariate residual (Mahalanobis distance) `mahal`
      <!-- #52 -->
- [ ] M4 analysis: robust trend estimation (Theil-Sen) `trend` <!-- #53 -->
- [ ] M4 analysis: state machine with hysteresis and dwell `fsm` <!-- #54 -->
- [ ] M4 tools: training pipeline and weights format with provenance header `train`
      <!-- #55 -->
- [ ] M4 analysis: C inference over our own weights `infer` <!-- #56 -->
- [ ] M4 interface: remaining four obdctl verbs `obdrest` <!-- #57 -->
- [ ] M4 interface: read-only phone status page, with trends, over the box's own Wi-Fi
      `phoneview` <!-- #91 -->
- [ ] M4 experiment: interrupt vs polling — latency distribution and CPU, idle and loaded
      `expirq` <!-- #58 -->
- [ ] M4 experiment: no-drop under CPU contention, proven by sequence accounting `expdrop`
      <!-- #59 -->
- [ ] M4 experiment: supervision — kill -9 each child, time detection and recovery
      `expkill` <!-- #92 -->
- [ ] M4 experiment: crash consistency — N power-cut trials `expcrash` <!-- #60 -->
- [ ] M4 ground truth: induce and label a vacuum leak on a testbed car `fvac` <!-- #62 -->
- [ ] M4 ground truth: induce and label a cooling anomaly `fcool` <!-- #63 -->
- [ ] M4 ground truth: sensor unplug — and demo-day rehearsal `funplug`
      <!-- #64 -->
- [x] M4: capture across a real oil change on the CR-V (cut, D-021) `foil` <!-- #65 -->
- [ ] M4 hardware: the box: shell, vents, USB-C port, light, mounting `encl` <!-- #66 -->
- [ ] M4 experiment: clean-shutdown time distribution and supercapacitor margin `expshut`
- [ ] M4 experiment: key-cycle recovery: N real cycles, unclean endings, records lost
      `keycyc`
- [ ] M4 hardware: Pi and supercapacitor temperature in a sealed shell in a hot cabin
      `thermal`
- [ ] M4 soak: 48-hour key-cycle run #2 (the margin) `soak2` <!-- #69 -->
- [ ] M4 process: transcript copy-out `tx4` <!-- #71 -->

## M5 — Final submission: system, soak logs, eval report, process artifacts, transcripts

- [ ] **`docs/EVALUATION.md` written, with an honest limitations section** `evalreport`
      <!-- #73 -->
- [ ] **Raw unedited soak logs committed** <!-- #124 -->
- [ ] **`docs/DESIGN.md` as-built, with a changelog of what M2 got wrong** `designfinal`
      <!-- #74 -->
- [ ] **≥ 40 meaningful commits, both partners represented** `commitaudit`
      <!-- #78 -->
- [ ] **README verified on a clean Pi by someone who has never built it** `readmecheck`
      <!-- #75 -->
- [ ] M5 tools: soak figures — RSS, CPU, event counts over 48h `plots` <!-- #72 -->
- [ ] M5 process: PROMPTLOG.md complete, both partners `promptlog` <!-- #76 -->
- [ ] M5 process: REFLECTION.md, both partners `reflection` <!-- #77 -->
- [ ] M5 submit: assemble the tarball and M5 transcript copy-out `tarball` <!-- #79 -->

## M6 — Demo day: live demo, fault injection, individual defenses (date assumed — see `dates`)

Rehearse all three movements: the **demonstration** (they will unplug a sensor; graded
on graceful degradation, honest reporting through our own interface, and the log line
proving the system noticed), the **individual defense** (your subsystems line by line,
one PROMPTLOG judgment call, one question from across the ownership boundary), and **live
modification** (each partner alone at the keyboard makes one small unseen change to their
own subsystem).

- [ ] M6 demo: full rehearsal with surprise fault injection `demorehearse` <!-- #80 -->
- [ ] M6 demo: live-modification practice `livemod` <!-- #81 -->
- [ ] M6 demo: cross-boundary quiz `crossquiz` <!-- #82 -->
