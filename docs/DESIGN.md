# Design document — carwatch

> **STATUS: M2 SKELETON.** 3–5 pages when complete. The handout calls these "the most
> leveraged hours of the whole project" — every hour of arguing here saves five of
> rewriting later.
>
> **Before submitting, run the three-isolated-workers check:** a design-only reviewer,
> a parts-only buyer who never sees the design, and a builder who only follows
> instructions must each succeed from their own section alone.

## 1. Architecture
> A diagram in the spirit of the handout's Figure 1: processes, threads, kernel
> components, data flows, **and rates on every arrow**. The rates are not decoration —
> they are what makes the mechanism justifications checkable.

```
  OBD2 port
        │                          Pi power: car USB-C / 12 V socket, ≥ 5.1 V / 3 A,
  ┌──────────────┐                 switched — cut, unannounced, at every key-off (D-011)
  │ USB OBD2     │  USB tty /dev/obd   ┌────────────────┐
  │ adapter      │◀───────────────────▶│      obdd      │  Mode 01 requests,
  │ (D-015)      │  [N] baud           │  tty reader    │  [N] req/s, round-robin (D-012)
  └──────────────┘                     └───────┬────────┘
                                               │ SPSC ring, [N] samples/s  (mech F only with pisensor)
                                               │ shm, sequence-accounted
                    ┌──────────────────────────┼──────────────────────┐
                    ▼                          ▼                      ▼
            ┌───────────────┐         ┌────────────────┐     ┌────────────────┐
            │   storaged    │         │   analyzed     │     │   obdctl       │
            │ append-only   │◀────────│ features →     │────▶│ query CLI      │
            │ CRC, fsync    │  read   │ baseline →     │ UDS │ 5 verbs        │
            │   [mech D]    │         │ model → FSM    │     └────────────────┘
            └───────────────┘         └───────┬────────┘     ┌────────────────┐
                                              └─────────────▶│ status page    │──▶ phone
                                                        UDS  │ read-only,     │  own Wi-Fi AP,
                                                             │ status+verdicts│  no upstream
                                                             └────────────────┘  (D-013)
                    ▲                          ▲
                    └──────────┬───────────────┘
                               │ spawn / heartbeat / restart-with-backoff
                        ┌──────────────┐  GPIO17
                        │  supervisor  │──────────▶ warning light (D-014)
                        └──────────────┘  [mech E]  blink = alive; steady = not running
```

**TODO(M2):** fill every `[N]`. Replace with a real figure in `docs/figures/`.

## 2. Mechanism mapping
> For each chosen menu item: which component implements it, and a justification **from
> the user's requirements**. The defense will test the justification, not the choice.
> Model the form on the handout's example: "the vibration analysis is meaningless above
> 2 ms of sampling jitter, hence SCHED_FIFO."

The committed rationale is `docs/DECISIONS.md` §B.1 (D-017); expand each row here with
the measured numbers that justify it.

| Menu item | Component | Justification from requirements | How measured |
|---|---|---|---|
| D | `src/store/` | | recovery after mid-write power cut, N trials |
| E | `src/supervisor/`, `src/ipc/` | | `kill -9` any child; detection/degradation/recovery in the log |
| B *(only with `pisensor`)* | Pi-side sensor capture path | | event-latency distribution + CPU, IRQ vs poll, idle and loaded |
| F *(only with `pisensor`)* | `src/ipc/ring.c` | | sustained rate, zero drops, sequence-accounted, under contention |

## 3. Failure-mode table
> For each component: how it can fail, how the failure is **detected**, what the system
> **does**, and what the **log will show**. The soak test grades this table's honesty —
> so write the modes you are afraid of, not the ones you have already handled.

| Component | Failure | Detection | Response | Log line |
|---|---|---|---|---|
| OBD2 adapter | unplugged mid-drive (`/dev/obd` disappears) | read error / `ENODEV` on the tty | degrade, do not restart-storm; reopen when udev brings it back | |
| OBD2 adapter | hangs or returns garbage (`?`, `NO DATA`, `BUFFER FULL`) | per-request timeout; reply parse failure | `ATZ` reset, then back off | |
| ECU | stops answering a PID (sensor unplugged) | request timeout, per PID (D-012) | mark PID absent, keep the rest | |
| obdd | crash / `kill -9` | supervisor `waitpid` | restart with backoff | |
| ring | consumer stalls, producer would overwrite | sequence gap accounting | | |
| storaged | disk full | | | |
| storaged | power cut mid-write | CRC mismatch on recovery scan | truncate torn tail | |
| analyzed | model file missing or corrupt | | fall back to residuals only | |
| supervisor | itself dies | systemd `Restart=always` | warning light freezes steady, which by design reads "not running" (D-014) | |
| status page | crash, or phone floods it with requests (D-013) | supervisor `waitpid` | restart with backoff; capture and storage unaffected | |
| clock | no RTC, time jumps at boot | | | |
| power | USB port sags at crank; Pi undervolts but keeps running (D-011) | `vcgencmd get_throttled` polled by supervisor | | |
| power | cut at key-off, every drive (D-011) | none possible in advance — recovery scan at next boot | | |

## 4. Storage and data
> What is stored, at what rate, in what format, with what retention, and what happens to
> it when the power dies mid-write.

- **Record format:** [magic][seq][mono_ns][wall_ns][src: live|replay|synth][pid][value][crc32]
- **Rates:** [fill — Mode 01 request budget, and the per-PID rate it implies (D-012)]
- **Retention and rollup:** [fill — raw window, then binned aggregates?]
- **fsync discipline:** [fill — batch size, interval, and the argument for it]
- **Crash story:** [fill — recovery scan, torn-tail truncation, what is lost and why
  losing it is acceptable]
- **Baseline persistence:** learned per-bin statistics must survive a power cut. This is
  the requirement that made mechanism D non-optional.

## 5. Constraints and substitutions
> What the ideal build would use, what we are actually using, and what the gap costs.
> "A design document with nothing to report here has usually not met its hardware yet."

| Wanted | Using | What the substitution costs |
|---|---|---|
| Analog oil pressure sender, direct | Whatever the ECU publishes (D-007) | Possibly binary switch only — headline diagnostic at risk |
| A year of failing engines | Induced faults on one healthy car (D-016) | Only three fault classes, and none of them is a real bearing failure |
| Raw CAN at the ECU's own publish rate | Mode 01 replies through a USB OBD2 adapter (D-015) | ~10–20 samples/s total, request/response only; no interrupt line, no bus timing — most of the mechanism menu leaves the data path (D-017) |
| 48h of live driving | [pending D-006] | |
| Fused automotive supply with hold-up for a clean shutdown | The car's switched USB-C port (D-011) | No warning before power loss; brown-out at crank must be measured, not assumed |

## 6. Evaluation plan
> The measurements we will take, each with **method and committed target**. Numbers
> committed now are twice as credible when hit later, and instructive either way.

| Measurement | Method | Target |
|---|---|---|
| OBD reply → stored, p50/p99 | timestamp at tty read and at fsync | |
| `kill -9` each child: detection + recovery time | N trials per child, from the log | |
| IRQ vs polling: latency + CPU *(only with `pisensor`)* | both paths, idle and `stress-ng` loaded | |
| Drop rate under contention | sequence accounting | zero |
| RSS per process over 48h | hourly heartbeat, plotted | flat |
| Detection: TPR / FPR per diagnostic | induced-fault fixtures | |
| Recovery after mid-write power cut | N pull-the-plug trials | 100% |

## 7. Ownership map
| Owner | Subsystems |
|---|---|
| Camden | `src/obd/`, `src/ipc/` |
| Lance | `src/store/`, `src/analysis/`, `tools/train/` |
| Shared | `src/supervisor/`, `src/interface/`, `src/common/`, docs |

Ownership means first authorship and answerability at the defense, not exclusivity.

## 8. AI-use plan
> What we use Claude Code for, what we don't, and how the §3.3 boundary stays visible
> in the repository.

Used for: planning, explaining kernel and serial-device mechanics, drafting test fixtures,
adversarial review of diffs, documentation. Not used for: the analysis pipeline's
design decisions, and never at runtime.

The boundary is legible by inspection: `src/` contains no HTTP client and no network
code except the read-only status page server in `src/interface/`, which serves the
device's own Wi-Fi access point and has no upstream connection (D-013). `grep -r` for any network symbol
outside that directory returns nothing, and that check is in `make test`.

## 9. Changelog (added at M5)
> What M2's version got wrong. This section is graded and an empty one is not credible.
