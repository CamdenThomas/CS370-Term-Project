# Design document — carwatch

> **STATUS: M2 SKELETON.** 3–5 pages when complete. The handout calls these "the most
> leveraged hours of the whole project" — every hour of arguing here saves five of
> rewriting later.
>
> **Before submitting, run the three-isolated-workers check:** a design-only reviewer,
> a parts-only buyer who never sees the design, and a builder who only follows
> instructions must each succeed from their own section alone.

## 1. Architecture
>
> A diagram in the spirit of the handout's Figure 1: processes, threads, kernel
> components, data flows, **and rates on every arrow**. The rates are not decoration —
> they are what makes the mechanism justifications checkable.

```text
  OBD2 port
        │                          Pi power: the box's one USB-C input from the owner's
  ┌──────────────┐                 charger + a seconds-scale supercapacitor; off at rest,
  │              │                 clean shutdown at key-off (D-024)
  │ Bluetooth    │  RFCOMM tty         ┌────────────────┐
  │ OBD2 adapter │◀ ~ ~ ~ ~ ~ ~ ~ ~ ~ ▶│      obdd      │  Mode 01 requests,
  │ (D-019)      │  /dev/obd           │  tty reader    │  [N] req/s, round-robin (D-012)
  └──────────────┘                     └───────┬────────┘
                                               │ SPSC ring, [N] samples/s
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
                                                             │ verdicts+report│  no upstream
                                                             └────────────────┘  (D-013, D-028)
                    ▲                          ▲
                    └──────────┬───────────────┘
                               │ spawn / heartbeat / restart-with-backoff
                        ┌──────────────┐  GPIOs
                        │  supervisor  │──────────▶ tri-color light (D-027)
                        └──────────────┘  [mech E]  pulse = alive; steady = not running
```

**TODO(M2):** fill every `[N]`. Replace with a real figure in `docs/figures/`.

## 2. Mechanism mapping
>
> For each chosen menu item: which component implements it, and a justification **from
> the user's requirements**. The defense will test the justification, not the choice.
> Model the form on the handout's example: "the vibration analysis is meaningless above
> 2 ms of sampling jitter, hence SCHED_FIFO."

The committed rationale is `docs/DECISIONS.md` §B.1 (D-017); expand each row here with
the measured numbers that justify it. B and F were considered and declined: no Pi-side
sensor is added, so nothing in the design needs them (D-029).

| Menu item | Component | Justification from requirements | How measured |
| --- | --- | --- | --- |
| D | `src/store/` | | recovery after mid-write power cut, N trials |
| E | `src/supervisor/`, `src/ipc/` | | `kill -9` any child; detection/degradation/recovery in the log |

## 3. Failure-mode table
>
> For each component: how it can fail, how the failure is **detected**, what the system
> **does**, and what the **log will show**. The soak test grades this table's honesty —
> so write the modes you are afraid of, not the ones you have already handled.

| Component | Failure | Detection | Response | Log line |
| --- | --- | --- | --- | --- |
| OBD2 adapter | Bluetooth link drops mid-drive (out of range, adapter asleep, unplugged) | hangup on the tty: `read` returns 0 or `EIO`, `poll` reports `POLLHUP` | degrade, do not restart-storm; reopen `/dev/obd` with backoff until the link returns | |
| OBD2 adapter | hangs or returns garbage (`?`, `NO DATA`, `BUFFER FULL`) | per-request timeout; reply parse failure | `ATZ` reset, then back off | |
| ECU | stops answering a PID (sensor unplugged) | request timeout, per PID (D-012) | mark PID absent, keep the rest | |
| obdd | crash / `kill -9` | supervisor `waitpid` | restart with backoff | |
| ring | consumer stalls, producer would overwrite | sequence gap accounting | | |
| storaged | disk full | | | |
| storaged | power cut mid-write | CRC mismatch on recovery scan | truncate torn tail | |
| analyzed | model file missing or corrupt | | fall back to residuals only | |
| supervisor | itself dies | systemd `Restart=always` | the light freezes steady, which by design reads "not running" (D-027) | |
| status page | crash, or phone floods it with requests (D-013) | supervisor `waitpid` | restart with backoff; capture and storage unaffected | |
| status page | report generation fails or runs out of memory (D-028) | error return; RSS in the heartbeat | the page says the report failed; capture and storage unaffected | |
| clock | no RTC, time jumps at boot | | | |
| power | source disappears: the normal key-off (D-024) | source-present GPIO | grace period, then commit, checkpoint, sync, halt; the power stage cuts the Pi | |
| power | source dips at crank | source-present drops for less than the grace period | ride through on the supercapacitor; log the dip | |
| power | source returns during shutdown | source-present rises in `SHUTTING_DOWN` | finish the clean shutdown; the power stage power-cycles the Pi, which boots again | |
| power | bank empty at key-off or at a cold-start crank | failed shutdown timing; next boot finds an unclean end | crash-consistent recovery at the next boot (mechanism D) | |
| power | charger pulled mid-drive | source-present GPIO | as key-off, logged as unplanned | |
| power | weak source; Pi undervolts but keeps running | `vcgencmd get_throttled` polled by supervisor | degrade; log; the bank supplements | |
| power | charger stays live with the key off (unsupported install) | source present while the engine reports nothing for a long time | log it; the box stays on; the report says so | |
| thermal | Pi throttles in a hot cabin | `vcgencmd get_throttled`, SoC temperature | degrade; log; the report shows it | |
| storage | SD card fails | write error | degrade and log; do not crash | |
| radio | Bluetooth and Wi-Fi contend on the Pi 4's one chip | `btcoex` measurement | measure and document | |
| boot | the previous session ended unclean | boot record flag, recovery log | recover; list it in the report's sessions | |

## 4. Storage and data
>
> What is stored, at what rate, in what format, with what retention, and what happens to
> it when the power dies mid-write.

- **Record format:** `[magic][seq][mono_ns][wall_ns][src: live|replay|synth][pid][value][crc32]`
- **Rates:** [fill — Mode 01 request budget, and the per-PID rate it implies (D-012)]
- **Retention and rollup:** [fill — raw window, then binned aggregates?]
- **fsync discipline:** [fill — batch size, interval, and the argument for it]
- **Crash story:** [fill — recovery scan, torn-tail truncation, what is lost and why
  losing it is acceptable]
- **Baseline persistence:** learned per-bin statistics must survive a power cut. This is
  the requirement that made mechanism D non-optional.

## 5. Constraints and substitutions
>
> What the ideal build would use, what we are actually using, and what the gap costs.
> "A design document with nothing to report here has usually not met its hardware yet."

| Wanted | Using | What the substitution costs |
| --- | --- | --- |
| Every engine sensor, direct | Whatever Mode 01 PIDs the ECU publishes (D-007, D-022) | The model sees only what the car reports, and the two testbeds may report different lists; oil pressure, for one, is probably absent |
| A year of failing engines | Induced faults on healthy testbed cars, one car at a time (D-021, D-022) | A handful of fault areas, all reversible; no real wear-out failure is ever seen |
| Raw CAN at the ECU's own publish rate | Mode 01 replies from a Bluetooth OBD2 adapter (D-019) | ~10–20 samples/s total, request/response only; no interrupt line, no bus timing — most of the mechanism menu leaves the data path (D-017) |
| 48 continuous hours on live sensors | 48 hours wall-clock of normal use across key cycles (D-023), pending the professor's written confirmation | The log is a series of sessions with deliberate gaps while parked; the memory plot is per session; the soak needs a car driven normally for two days |
| A supply with full hold-up through crank and shutdown | The owner's USB-C charger plus a seconds-scale supercapacitor (D-024) | Shutdown is clean only once the bank has charged. At a cold start the bank is empty, so the first crank may reset the Pi. A charger that stays live with the key off keeps the box on |
| A device that is recording the instant the key turns | A stock Pi 4 boot, roughly half a minute (estimate, `bootbudget`) | The start of each drive's warm-up, among its most informative minutes, goes unrecorded |

## 6. Evaluation plan
>
> The measurements we will take, each with **method and committed target**. Numbers
> committed now are twice as credible when hit later, and instructive either way.

| Measurement | Method | Target |
| --- | --- | --- |
| OBD reply → stored, p50/p99 | timestamp at tty read and at fsync | |
| `kill -9` each child: detection + recovery time | N trials per child, from the log | |
| Drop rate under contention | sequence accounting | zero |
| RSS per process over the 48 h window | heartbeats at session start, hourly and at session end (D-023), plotted per session | flat |
| Detection: TPR / FPR per diagnostic | induced-fault fixtures | |
| Recovery after mid-write power cut | N pull-the-plug trials | 100% |

## 7. Ownership map

Nothing is pre-assigned (D-026). A subsystem belongs to whoever lands its first
substantive commit, and this table records the map as it is realized. Ownership means
first authorship and answerability at the defense, not exclusivity.

| Subsystem | Owner | Claimed by |
| --- | --- | --- |
| `src/ipc/` | Camden | the SPSC ring and its tests (M0) |
| `src/obd/` | unclaimed | |
| `src/store/` | unclaimed | |
| `src/analysis/` | unclaimed | |
| `src/supervisor/` | unclaimed | |
| `src/interface/` | unclaimed | |
| `src/common/` | unclaimed | |
| `tools/train/` | unclaimed | |
| `electricalDrawing/` | unclaimed | |

Helpers written as scaffolding (the CRC32 in `src/store/`, the logger in `src/common/`)
are not a claim on their subsystem.

## 8. AI-use plan
>
> What we use Claude Code for, what we don't, and how the §3.3 boundary stays visible
> in the repository.

Used for: planning, explaining kernel and serial-device mechanics, drafting test fixtures,
adversarial review of diffs, documentation. Not used for: the analysis pipeline's
design decisions, and never at runtime.

The boundary is legible by inspection: `src/` contains no HTTP client and no network code
except the read-only status page and report server in `src/interface/`, which serves the
box's own Wi-Fi access point and has no upstream connection (D-013, D-028). `grep -r` for
any network symbol outside that directory returns nothing, and that check is in
`make test`.

## 9. Changelog (added at M5)
>
> What M2's version got wrong. This section is graded and an empty one is not credible.
