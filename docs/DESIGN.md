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
  │ (D-019)      │  /dev/obd           │  tty reader    │  ~20 req/s, round-robin (D-012)
  └──────────────┘                     └───────┬────────┘
                                               │ SPSC ring in shm, ~20 samples/s,
                                               │ sequence-accounted; one consumer (D-031)
                                               ▼
                                       ┌────────────────┐
                                       │    storaged    │  append-only log; BEGIN/COMMIT
                                       │    [mech D]    │  batches, 1 fsync/s, ~1 KB/s (D-025, D-035)
                                       └───────┬────────┘
                                               │ committed records only, read from
                                               │ the log, ~20 records/s, ~1 s behind (D-031)
                                               ▼
                                       ┌────────────────┐
                                       │    analyzed    │  bins → baselines → residuals
                                       │                │  → trends → model → FSM
                                       └───────┬────────┘
                                               │ verdicts, UDS, a few per drive at most
                                               ▼
   ┌────────────────┐       UDS        ┌────────────────┐  GPIOs
   │ obdctl         │◀────────────────▶│   supervisor   │──────────▶ tri-color light (D-027), 1-4 Hz
   │ query CLI,     │   on demand      │   [mech E]     │            pulse = alive;
   │ 5 verbs        │       ┌─────────▶│                │            steady = not running
   └────────────────┘       │   UDS    └────────────────┘
   ┌────────────────┐       │          spawns, heartbeats and restarts, with backoff,
   │ status page    │◀──────┘          every process in this figure
   │ verdicts+report│──▶ phone: the box's own Wi-Fi AP, no upstream (D-013, D-028)
   └────────────────┘
```

`obdctl` and the status page never read samples themselves: they ask the supervisor, which
asks the process that holds the answer (D-013). `analyzed` never reads the ring (D-031).

Every rate is an estimate until the first drive measures it (`firstcap`): about 20
requests a second is what `obdsim` gives at a 40 ms reply time (D-012), and the OBDLink
LX's real reply time is unknown. The ring, the log and `analyzed` all run at the request
rate, because each reply is one sample, one record and one analysis step. Heartbeat and
backoff timings are set in the supervisor's design (`supbackoff`). A drawn figure in
`docs/figures/` is still to do (`archfig`).

## 2. Mechanism mapping
>
> For each chosen menu item: which component implements it, and a justification **from
> the user's requirements**. The defense will test the justification, not the choice.
> Model the form on the handout's example: "the vibration analysis is meaningless above
> 2 ms of sampling jitter, hence SCHED_FIFO."

The committed rationale is `docs/DECISIONS.md` §B.1 (D-017); the measured numbers that
justify each row arrive with the evaluation (§6).

**Two physically distinct sensors (handout §3.1).** Every sensor reaches the Pi through
one dongle, but the dongle is their transport, not a sensor. Coolant temperature, intake
air temperature, manifold pressure or mass air flow, the fuel trims and the O2 sensors are
physically distinct sensors on the engine, and every diagnostic fuses at least two of them
(D.4). For example, a vacuum leak is called only when long-term fuel trim rises while
manifold pressure at idle rises with it in the same operating-point bin; neither signal
alone can call it. B and F were considered and declined: no Pi-side
sensor is added, so nothing in the design needs them (D-029).

| Menu item | Component | Justification from requirements | How measured |
| --- | --- | --- | --- |
| D | `src/store/` (`storaged`), checkpoints in `src/analysis/` | Power can vanish at any instant: the bank is empty at a cold start, a cable can be pulled, a crank can reset the Pi (D-024). The owner needs the record and the learned baselines to survive that, and a baseline must never learn from a sample a cut later erased (D-031). Hence append-only CRC'd records, one-second commits (D-035) and A/B checkpoints (D-025) | 50 cuts of the Pi's 5 V rail mid-write; every recovery logged with bytes and records lost |
| E | `src/supervisor/`, `src/ipc/` | A recorder that dies silently harms an owner who believes it is on duty. The analysis is the most complex and least trusted code, so it must not be able to take capture or storage down with it (B.2). All sensors share one dongle, so the isolation we defend is between capture, storage and analysis, not between sensors (D-029) | `kill -9` each child 20 times; detection, degradation and recovery times from the log; no fd leaked |

## 3. Failure-mode table
>
> For each component: how it can fail, how the failure is **detected**, what the system
> **does**, and what the **log will show**. The soak test grades this table's honesty —
> so write the modes you are afraid of, not the ones you have already handled.

Log lines are drafts, in the form `<process> <level> <event> key=value ...`, written
through the shared logger in `src/common/`; every one also carries `src` (C.2).

| Component | Failure | Detection | Response | Log line |
| --- | --- | --- | --- | --- |
| OBD2 adapter | Bluetooth link drops mid-drive (out of range, adapter asleep, unplugged) | hangup on the tty: `read` returns 0 or `EIO`, `poll` reports `POLLHUP` | degrade, do not restart-storm; reopen `/dev/obd` with backoff until the link returns | `obdd WARN link_lost err=EIO`, then `obdd INFO reconnected attempts=4 down_ms=8200` |
| OBD2 adapter | hangs or returns garbage (`?`, `NO DATA`, `BUFFER FULL`) | per-request timeout; reply parse failure | three in a row: `ATZ` reset, then back off | `obdd WARN reply_bad pid=0C text="BUFFER FULL"`, `obdd WARN adapter_reset after=3` |
| OBD2 adapter | answers, but finds no protocol (engine off, key in accessory) | `UNABLE TO CONNECT` or `NO DATA` for every PID, including `0100` | treat as engine off: poll `0100` once a few seconds, record nothing, light stays green | `obdd INFO ecu_silent since_ms=12000` |
| ECU | stops answering a PID (sensor unplugged) | request timeout or `NO DATA` five times in a row, per PID (D-012) | mark PID absent, keep the rest, retry it once a minute; the report lists it (D-028) | `obdd WARN pid_absent pid=05 after=5`, `obdd INFO pid_present pid=05` |
| obdd | crash / `kill -9` | supervisor `waitpid` | restart with backoff | `supervisor ERROR child_exit name=obdd signal=9`, `supervisor INFO child_restart name=obdd delay_ms=500` |
| ring | consumer stalls, producer would overwrite | sequence gap accounting; `storaged` heartbeat late | producer refuses and counts the overrun (B.3); supervisor restarts the stalled `storaged`; light red while overruns grow | `obdd ERROR ring_full overruns=37`, `supervisor WARN heartbeat_late name=storaged ms=4100` |
| storaged | disk full | `write` returns `ENOSPC`, or free space below a floor at each segment close | delete the oldest whole segment if retention allows; else stop committing, let the ring refuse, light red | `storaged ERROR enospc free_kb=0 action=drop_segment seg=log-123.cwr` |
| storaged | power cut mid-write | CRC mismatch, or a batch with no `COMMIT`, on the recovery scan | truncate the torn tail and the uncommitted batch (D-025) | `storaged WARN recovered bytes_truncated=213 records_lost=17 prev_end=recovered` |
| storaged | `fsync` fails (`EIO`) | `fsync` return value | treat the batch as not durable; do not advance the durable offset; retry once, then degrade | `storaged ERROR fsync_failed err=EIO txn=4411` |
| analyzed | checkpoint torn by a power cut | slot CRC or generation check | load the other slot; redo from its older offset (D-025) | `analyzed WARN checkpoint_bad slot=B using=A redo_txns=812` |
| analyzed | model file missing or corrupt | `open` fails, or the weights header's CRC or version does not match | fall back to residuals only | `analyzed WARN model_unavailable reason=crc mode=residuals_only` |
| analyzed | falls behind the durable offset | gap between its read offset and `storaged`'s durable offset | keep reading in order; nothing is lost because it reads the log, not the ring (D-031) | `analyzed WARN lagging behind_records=2400` |
| supervisor | itself dies | systemd `Restart=always` | the light freezes steady, which by design reads "not running" (D-027) | `systemd: supervisor.service: Main process exited, status=9/KILL` (journal) |
| status page | crash, or phone floods it with requests (D-013) | supervisor `waitpid` | restart with backoff; capture and storage unaffected | `supervisor ERROR child_exit name=statusd status=1` |
| status page | report generation fails or runs out of memory (D-028) | error return; RSS in the heartbeat | the page says the report failed; capture and storage unaffected | `statusd ERROR report_failed err=ENOMEM rss_kb=48000` |
| clock | the RTC lost its backup or is missing (D-030) | the RTC reports invalid time, or does not answer, at boot | run on `mono_ns`; mark the session's wall time untrusted; the report says so | `supervisor WARN rtc_invalid wall_untrusted=1` |
| power | source disappears: the normal key-off (D-024) | source-present GPIO | grace period, then commit, checkpoint, sync, halt; the power stage cuts the Pi | `supervisor INFO source_lost`, then `supervisor INFO shutdown reason=source_lost` |
| power | source dips at crank | source-present drops for less than the grace period | ride through on the supercapacitor; log the dip | `supervisor INFO source_dip ms=640` |
| power | source returns during shutdown | source-present rises in `SHUTTING_DOWN` | finish the clean shutdown; the power stage power-cycles the Pi, which boots again | `supervisor INFO source_back state=shutting_down action=finish` |
| power | bank empty at key-off or at a cold-start crank | failed shutdown timing; next boot finds an unclean end | crash-consistent recovery at the next boot (mechanism D) | `storaged WARN recovered prev_end=recovered` at the next boot |
| power | charger pulled mid-drive | source-present GPIO | as key-off, logged as unplanned | `supervisor WARN source_lost engine=running` |
| power | weak source; Pi undervolts but keeps running | CC advertisement at start; `vcgencmd get_throttled` polled by supervisor | degrade and log; the bank covers only brief dips; a source too weak for a drive is open (`powerspec`) | `supervisor WARN undervoltage throttled=0x50005` |
| power | charger stays live with the key off (unsupported install) | source present while the engine reports nothing for a long time | log it; the report says so; whether to halt to save the car's battery is open (`shutfsm`) | `supervisor WARN source_live_engine_silent minutes=30` |
| thermal | Pi throttles in a hot cabin | `vcgencmd get_throttled`, SoC temperature | degrade; log; the report shows it | `supervisor WARN thermal_throttle soc_c=82` |
| storage | SD card fails | write error | degrade and log; do not crash | `storaged ERROR write_failed err=EIO` |
| radio | Bluetooth and Wi-Fi contend on the Pi 4's one chip | `btcoex` measurement | measure and document | `obdd INFO reply_ms p99=180` in the heartbeat, compared with Wi-Fi on and off |
| boot | the previous session ended unclean | boot record flag, recovery log | recover; list it in the report's sessions | `storaged INFO boot boot_id=212 prev_end=recovered` |

## 4. Storage and data
>
> What is stored, at what rate, in what format, with what retention, and what happens to
> it when the power dies mid-write.

- **Record format (draft, D-034):** a 40-byte header (`magic`, `kind`, `body_len`, `seq`,
  `mono_ns`, `wall_ns`, `src: live|replay|synth`, flags, `boot_id`), a typed body and a
  CRC32 over both. Six kinds: sample, begin, commit, boot, shutdown, event. Samples are
  grouped into `BEGIN` … `COMMIT` batches (D-025). A sample record is 56 bytes and keeps
  the raw reply bytes beside the decoded value.
- **Rates (estimates until the first drive measures them):** the adapter answers one
  request at a time, about 15 to 25 a second (D-012; `obdsim` at a 40 ms reply time gives
  22). Round-robin over the supported PIDs, about 17 on the simulator, gives each PID
  about 1 Hz. That is 0.8 to 1.4 KB/s, or 3 to 5 MB per running hour.
- **Retention and rollup:** open (`retention`). The segment layout (D-034) lets retention
  delete whole segments, oldest first, without editing any file; how long raw data stays
  before only binned aggregates remain is not decided.
- **fsync discipline (draft, D-035):** one `COMMIT` and one `fsync` when a batch is one
  second old or holds 64 samples, and always at a clean shutdown. The argument: a cut
  that beats the clean shutdown (cold-start crank, pulled cable) then loses at most a
  second, about one reading per PID, while every signal we watch moves over minutes;
  longer batches would lose warm-up data at every cold-start reset; per-record `fsync`
  would multiply flash writes about twentyfold for no reader that needs it.
- **Crash story:** at boot, `storaged` scans forward from its last committed offset,
  verifies every CRC, and truncates the torn tail and any batch with no `COMMIT`
  (undo is truncation, because nothing is overwritten in place). An unclean cut loses the
  open batch, at most one second of samples; the boot record counts what was truncated,
  and the report lists it per session (D-025, D-028). Losing it is acceptable because no
  analysis stage can tell one missing second from a slow round-robin cycle.
- **Baseline persistence:** learned per-bin statistics must survive a power cut. This is
  the requirement that made mechanism D non-optional. `analyzed` checkpoints them to A/B
  slots and, at start, redoes every committed batch after the newest valid slot (D-025).

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
| A wired link with steady timing to the adapter | Bluetooth Classic SPP to the OBDLink LX (D-019) | A new failure mode (the link drops mid-drive), a radio shared with the phone's Wi-Fi (`btcoex`), and a reply time not yet measured |
| The product's own capture path recording the first drives | A Python logging tool on a laptop or the Pi (D-033), labeled `live` | The first captures shape the design but prove nothing about `obdd`'s timing or about mechanism D; they enter the product only through `replay`, labeled so |
| A live demonstration at the bench | A running testbed car at the demo, since the box is off unless powered (D-024) | The demo depends on a car being on site (`democar`) |

## 6. Evaluation plan
>
> The measurements we will take, each with **method and committed target**. Numbers
> committed now are twice as credible when hit later, and instructive either way.

Targets are drafts until a human signs D-036.

| Measurement | Method | Target |
| --- | --- | --- |
| OBD reply → stored, p50/p99 | timestamp at tty read and after `fsync` returns, idle and with a CPU hog | ≤ 1.2 s / ≤ 2.5 s |
| Sustained request rate | requests answered per second from the log, per drive | ≥ 15/s; every PID at least every 2 s |
| `kill -9` each child: detection + recovery time | 20 trials per child, from the log | detection ≤ 100 ms; first heartbeat ≤ 5 s; no fd leaked |
| Drop rate under contention | sequence accounting | zero |
| RSS per process over the 48 h window | heartbeats at session start, hourly and at session end (D-023), plotted per session | flat: slope under 100 KB per running hour |
| Detection: TPR / FPR per fault area (D-021) | induced-fault fixtures, five events per area | ≥ 4 of 5 caught per area; ≤ 1 false verdict per 10 h of normal driving |
| Recovery after mid-write power cut | N cuts of the Pi's 5 V rail after the converter, so the supercapacitor cannot cover them (pulling the charger now triggers a clean shutdown) | 50 of 50 recover; ≤ 1 batch lost per cut |
| Boot to first durable sample | log timestamps against the source-present edge | ≤ 45 s |

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
