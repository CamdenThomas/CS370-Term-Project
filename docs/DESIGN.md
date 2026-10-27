# Design document — carwatch

**User and problem.** Lance drives a 2015 CR-V daily, and nothing reads his engine's
sensors except a check-engine light that speaks only once something has failed. carwatch
is a box that boots at every key-on, records every Mode 01 sensor through a Bluetooth
OBD2 dongle, learns *this* car's normal, and flags behavior that leaves it before the
light would. It is also tested on Camden's 2008 Forester (D-022). **Claim (D-021):** it
learns the normal of the car it is installed in, flags behavior outside it and names the
likely fault area, with measured catch and false-alarm rates on faults we induce.

## 1. Architecture

```text
OBD2 port -- OBDLink LX (STN chip, Bluetooth Classic SPP, D-019)
               ~~ RFCOMM ~~> kernel: BlueZ -> rfcomm tty -> /dev/obd
  obdd          Mode 01, one request outstanding, round-robin, ~10-20 req/s
    | SPSC ring in shared memory, ~10-20 samples/s, refuses when full (B.3)
    v
  storaged [D]  BEGIN..COMMIT batches, 1 fsync/s, ~1 KB/s -> ext4 on SD (D-025)
    | committed records only, read back from the log, ~1 s behind (D-031)
    v
  analyzed      bins -> baselines -> residuals -> trends -> model -> FSM
    | verdicts over a Unix socket, a few per drive at most
    v
  supervisor [E] spawns, heartbeats, restarts with backoff --GPIO--> light,
    ^            1-4 Hz pulse = alive, steady = not running (D-027)
    | UDS, on demand
  obdctl (CLI), status page + report --> phone on the box's own Wi-Fi (D-013, D-028)
power: owner's USB-C charger -> power block + supercapacitor -> Pi 4 (D-024)
```

Rates are estimates until the first drive measures them (`firstcap`): D-012 budgets about
10 requests a second; the logging tool reaches 22 on `obdsim`. One reply is one sample,
one record and one analysis step. `obdctl` and the page ask the supervisor, which asks
the process holding the answer; `analyzed` never reads the ring (D-031).

## 2. Mechanism mapping

D and E are the committed pair (D-017); B and F were declined: no Pi-side sensor, and
Mode 01 rates never stress a ring (D-029).

| Item | Component | Justification from the user's requirements | Measured by |
| --- | --- | --- | --- |
| D | `storaged`; checkpoints in `analyzed` | Power can vanish at any instant (empty bank at a cold start, pulled cable, crank reset, D-024), yet the record and weeks of learned baselines must survive, and no baseline may learn from a sample a cut later erased (D-031). Hence append-only CRC'd records, 1 s commits (D-035), A/B checkpoints (D-025) | 50 cuts of the Pi's 5 V rail mid-write; records lost per cut |
| E | `supervisor`, `src/ipc/` | A recorder that dies silently harms an owner who thinks it is on duty; the least trusted code (analysis) must not take capture or storage down (B.2). One dongle carries every sensor, so we isolate capture, storage and analysis, not sensors (D-029) | `kill -9` each child 20×: detection, recovery, fd leaks |

**Two physically distinct sensors (handout §3.1).** The dongle is transport. Coolant and
intake temperature, MAP or MAF, fuel trims and O2 sensors are distinct engine sensors, and
every diagnostic fuses at least two (§3, D.4).

## 3. Analysis: how normal is learned and a fault area named

All our code (handout §3.3; D.1, D.2). Per committed sample, `analyzed` (1) bins by
rpm × load × coolant temperature, so a cold idle is never compared with a warm cruise;
(2) keeps per-bin running mean and covariance (Welford), in A/B checkpoints; (3) scores the
Mahalanobis distance from the bin's baseline, which fires when a *partnership* breaks;
(4) fits Theil–Sen slopes of residuals across drives; (5) names the area with our
classifier (trained in `tools/train/`, inference in C; without weights, 1–4 still flag);
(6) surfaces verdicts through a hysteresis-and-dwell state machine (D.3).

| Fault area (D-021) | Cooperating sensors | Signature | Induced by (E.2) |
| --- | --- | --- | --- |
| Cooling | coolant, intake/ambient temp, load, speed | warm-up slope or warm settling temperature off its bin's normal for the same ambient and load | cardboard over the radiator |
| Air/fuel | fuel trims, MAP or MAF, rpm, O2 | long-term trim rising at idle with idle MAP, shrinking with load | a small vacuum line opened |
| Sensor/electrical | the failed PID and its usual partners; module voltage | one PID absent or flat while its partners move | a sensor unplugged |
| Unknown | all | residual high, no area matches | — |

No row can be called by one threshold on one value: each compares sensors that normally
move together, inside one bin (D.4).

> 🟨 **TEAM DECIDES (Wednesday):** the fault-area rows, signatures and bin axes above are
> Claude's proposal from D-021 and D.2. The AI-use plan (§9) says analysis design is
> ours, so confirm, change or replace each row, and then delete this note.

## 4. Failure-mode table

Log lines are `<process> <level> <event> key=value` with `src`; the table names the event.

| Component | Failure | Detection | Response | Log event |
| --- | --- | --- | --- | --- |
| adapter | Bluetooth drops | `read` 0/`EIO`, `POLLHUP` | reopen `/dev/obd` with backoff | `link_lost`, `reconnected` |
| adapter | garbage or timeouts | parse failure; timeout | 3 in a row: `ATZ`, then reopen | `reply_bad`, `adapter_reset` |
| adapter | engine off, key in ACC | `0100` gets `NO DATA` | poll `0100` every few s, record nothing | `ecu_silent` |
| ECU | a PID stops (sensor unplugged) | 5 `NO DATA` in a row | mark absent, retry each minute; report it | `pid_absent`, `pid_present` |
| any child | crash, `kill -9` | `waitpid` | restart with backoff; others run on | `child_exit`, `child_restart` |
| ring | consumer stalls | sequence gap; late heartbeat | refuse and count (B.3); restart `storaged`; red | `ring_full`, `heartbeat_late` |
| storaged | power cut mid-write | bad CRC / no `COMMIT` at boot | truncate tail and open batch (D-025) | `recovered` |
| storaged | disk full; `fsync` or write `EIO` | `ENOSPC`, return values | drop oldest segment if allowed; hold durable offset; degrade, red | `enospc`, `fsync_failed` |
| analyzed | torn checkpoint; bad model file | slot CRC; header CRC | other slot + redo; residuals only | `checkpoint_bad`, `model_unavailable` |
| status page | report fails, out of memory | error; RSS | page says so; capture unaffected | `report_failed` |
| supervisor | dies | systemd `Restart=always` | light freezes steady = "not running" | journal |
| clock | RTC lost backup (D-030) | invalid time at boot | run on `mono_ns`; wall time untrusted | `rtc_invalid` |
| power | key-off, cable pulled, crank dip, source back mid-shutdown | source-present GPIO timing | §6 state machine | `source_lost`, `source_dip`, `shutdown` |
| power | empty bank at cold crank | unclean end at next boot | crash recovery (D) | `recovered` |
| power, heat | undervoltage, throttling, charger live at key-off | `vcgencmd`, SoC temp | degrade, log, report; halt policy open | `undervoltage`, `thermal` |
| radio | BT and Wi-Fi share one chip | reply p99 in heartbeat | measure (`btcoex`) | heartbeat |

## 5. Storage and data

- **Format (D-034):** 40-byte header (`magic`, `kind`, `seq`, `mono_ns`, `wall_ns`, `src`,
  flags, `boot_id`), typed body, CRC32. Kinds: sample, begin, commit, boot, shutdown,
  event; a 56-byte sample keeps raw reply bytes beside the value. 16 MB segments, deleted
  whole, never edited; how long raw data is kept is open (`retention`).
- **Rates:** 10–25 samples/s is 0.56–1.4 KB/s, 2–5 MB per running hour.
- **fsync (D-035):** commit at 1 s or 64 samples, and at clean shutdown. An unclean cut
  loses at most a second (one reading per PID) of signals that move over minutes; longer
  batches lose warm-up at each cold-start reset; per-record `fsync` is 20× the flash wear.
- **Crash story:** at boot `storaged` scans from its last committed offset, verifies CRCs,
  truncates the torn tail and any uncommitted batch (undo is truncation: nothing is
  overwritten), and logs whether the last session ended clean. `analyzed` loads the newest
  valid checkpoint and redoes later batches through the live code path (D-025).

## 6. Constraints and substitutions

| Wanted | Using | Cost |
| --- | --- | --- |
| Every engine sensor | The Mode 01 PIDs the ECU publishes (D-022) | Oil pressure likely absent; lists differ by car |
| Failing engines | Induced, reversible faults on two healthy cars (D-021) | A few fault areas; no real wear-out |
| Raw CAN at ECU rate | Bluetooth Mode 01 request/response (D-019) | ~10–20 samples/s, no bus timing, a link that drops; most of the menu leaves the data path (D-017) |
| 48 continuous hours | 48 h of normal use across key cycles (D-023) | Sessions with gaps; awaiting written confirmation (`profmail`) |
| Full hold-up | Owner's charger + seconds-scale supercapacitor (D-024) | A cold-start crank may reset the Pi |
| Recording from key-on | A ~30 s Pi 4 boot (`bootbudget`) | The first half-minute of warm-up is lost |
| The product on the first drives | A Python tool, labeled `live` (D-033) | Proves nothing about `obdd`; enters the product only as `replay` |

## 7. Evaluation plan

Targets are drafts until D-036 is signed.

| Measurement | Method | Target |
| --- | --- | --- |
| Reply → durable, p50 / p99 | timestamps at tty read and after `fsync`; idle and with a CPU hog | ≤ 1.2 s / ≤ 2.5 s |
| Sustained request rate | answers per second from the log, per drive | ≥ 10/s; every model input every ≤ 2 s |
| `kill -9` each child | 20 trials per child, from the log | detect ≤ 100 ms; heartbeat ≤ 5 s; no fd leak |
| Ring drops under contention | sequence accounting | 0 |
| RSS over the soak | heartbeats at start, hourly, end, per session | slope < 100 KB per running hour |
| Detection per fault area | 5 induced events per area | ≥ 4 of 5; ≤ 1 false verdict per 10 h |
| Recovery after a cut mid-write | cut the Pi's 5 V rail after the converter | 50 / 50 recover; ≤ 1 batch lost |
| Boot to first durable sample | log time against the source-present edge | ≤ 45 s |

## 8. Ownership map

A subsystem belongs to whoever lands its first substantive commit (D-026).

| Subsystem | Owner | Evidence |
| --- | --- | --- |
| `src/ipc/` | Camden | the SPSC ring and its tests (M0) |
| `src/obd/`, `src/store/`, `src/analysis/`, `src/supervisor/`, `src/interface/`, `src/common/`, `tools/train/`, `electricalDrawing/` | unclaimed | |

## 9. AI-use plan

Claude Code is used for planning, explaining kernel and serial mechanics, test fixtures,
tooling, adversarial review of diffs and documentation; never at runtime, and not for the
analysis pipeline's design decisions. `src/` holds no network code except the read-only
status page in `src/interface/`, which serves the box's own access point with no upstream
(D-013, D-028); `make boundary`, part of `make test`, fails the build otherwise.

## 10. Changelog (M5)

What this M2 version got wrong, written at M5.
