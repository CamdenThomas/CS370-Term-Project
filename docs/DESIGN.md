# Design document — carwatch

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

Every rate is an estimate until the first drive measures it (`firstcap`): D-012 budgets
about 10 requests a second; the unthrottled logging tool reaches 22 against `obdsim`.
Each reply is one sample, one record and one analysis step, so the ring, the log and
`analyzed` all run at the request rate. `obdctl` and the status page ask the supervisor,
which asks the process that holds the answer; `analyzed` never reads the ring (D-031).

## 2. Mechanism mapping

D and E are the committed pair (D-017, D-029); B and F were declined because no Pi-side
sensor is added, and Mode 01 rates never stress a ring.

| Item | Component | Justification from the user's requirements | Measured by |
| --- | --- | --- | --- |
| D | `storaged`; checkpoints in `analyzed` | Power can vanish at any instant: the bank is empty at a cold start, a cable can be pulled, a crank can reset the Pi (D-024). The record and the learned baselines must survive, and a baseline must never learn from a sample a cut later erased (D-031). Hence append-only CRC'd records, one-second commits (D-035), A/B checkpoints (D-025) | 50 cuts of the Pi's 5 V rail mid-write; bytes and records lost per cut |
| E | `supervisor`, `src/ipc/` | A recorder that dies silently harms an owner who believes it is on duty. The analysis is the most complex, least trusted code and must not take capture or storage down (B.2). One dongle carries every sensor, so the isolation we defend is capture / storage / analysis, not sensor / sensor (D-029) | `kill -9` each child 20 times; detection, recovery, no fd leaked |

**Two physically distinct sensors (handout §3.1).** The dongle is transport, not a sensor.
Coolant and intake temperature, manifold pressure or mass air flow, the fuel trims and the
O2 sensors are distinct sensors on the engine, and every diagnostic fuses at least two
(D.4): a vacuum leak is called only when long-term fuel trim and idle manifold pressure
rise together in one operating-point bin.

## 3. Failure-mode table

Log lines take the form `<process> <level> <event> key=value`, carry `src`, and go through
the shared logger; the table names the event.

| Component | Failure | Detection | Response | Log event |
| --- | --- | --- | --- | --- |
| adapter | Bluetooth drops mid-drive | `read` 0 / `EIO`, `POLLHUP` | reopen `/dev/obd` with backoff; no restart storm | `link_lost`, `reconnected` |
| adapter | garbage (`?`, `BUFFER FULL`) or timeouts | parse failure, per-request timeout | three in a row: `ATZ`; if that fails, reopen | `reply_bad`, `adapter_reset` |
| adapter | no protocol: engine off, key in accessory | `UNABLE TO CONNECT` / `NO DATA` for `0100` | poll `0100` every few s, record nothing | `ecu_silent` |
| ECU | a PID stops answering (sensor unplugged) | `NO DATA` 5× in a row for it | mark absent, keep the rest, retry each minute; report lists it | `pid_absent`, `pid_present` |
| obdd, storaged, analyzed, page | crash or `kill -9` | supervisor `waitpid` | restart with backoff; others unaffected | `child_exit`, `child_restart` |
| ring | consumer stalls | sequence gap; late heartbeat | producer refuses and counts (B.3); restart `storaged`; light red | `ring_full`, `heartbeat_late` |
| storaged | disk full | `ENOSPC`; free space below floor | drop oldest whole segment if retention allows, else stop and go red | `enospc` |
| storaged | power cut mid-write | CRC mismatch or no `COMMIT` at boot | truncate torn tail and open batch (D-025) | `recovered` |
| storaged | `fsync` fails | return value | durable offset not advanced; retry once, then degrade | `fsync_failed` |
| analyzed | checkpoint torn | slot CRC / generation | load the other slot, redo from the log | `checkpoint_bad` |
| analyzed | model missing or corrupt | `open` fails; header CRC | residuals only, no area naming | `model_unavailable` |
| status page | report fails or runs out of memory (D-028) | error return; RSS in heartbeat | page says so; capture unaffected | `report_failed` |
| supervisor | dies | systemd `Restart=always` | light freezes steady = "not running" (D-027) | systemd journal |
| clock | RTC lost its backup (D-030) | invalid time at boot | run on `mono_ns`; wall time flagged untrusted | `rtc_invalid` |
| power | key-off; charger pulled | source-present GPIO | grace period, commit, checkpoint, halt | `source_lost`, `shutdown` |
| power | crank dip; source back mid-shutdown | GPIO edge timing | ride the bank; finish shutdown, then power-cycle | `source_dip`, `source_back` |
| power | bank empty at a cold-start crank | next boot finds an unclean end | crash recovery (mechanism D) | `recovered prev_end=recovered` |
| power, thermal | undervoltage, throttling, live charger with key off | `vcgencmd get_throttled`, SoC temp, engine silent | degrade, log, report; halt policy open (`shutfsm`) | `undervoltage`, `thermal_throttle` |
| SD card | write errors | `EIO` | degrade and log; do not crash | `write_failed` |
| radio | Bluetooth and Wi-Fi share one chip | reply-time p99 in the heartbeat | measure (`btcoex`) | heartbeat |

## 4. Storage and data

- **Format (D-034):** 40-byte header (`magic`, `kind`, `seq`, `mono_ns`, `wall_ns`, `src`,
  flags, `boot_id`), typed body, CRC32 over both. Kinds: sample, begin, commit, boot,
  shutdown, event. A 56-byte sample keeps the raw reply bytes beside the decoded value.
  Segments of about 16 MB; retention deletes whole segments, never edits one. How long
  raw data stays before only binned aggregates remain is open (`retention`).
- **Rates:** 10 to 25 samples a second is 0.56 to 1.4 KB/s, 2 to 5 MB per running hour;
  each of about 17 PIDs is read roughly every 1 to 2 s.
- **fsync (D-035):** commit and `fsync` at one second or 64 samples, and at every clean
  shutdown. An unclean cut then loses at most a second, about one reading per PID, while
  every signal we watch moves over minutes. Longer batches would lose warm-up data at each
  cold-start reset; per-record `fsync` would multiply flash writes twenty times for nothing.
- **Crash story:** at boot `storaged` scans forward from its last committed offset,
  verifies CRCs, truncates the torn tail and any batch without `COMMIT` (undo is
  truncation: nothing is overwritten in place), and writes a boot record saying whether
  the last session ended clean. `analyzed` loads the newest valid A/B checkpoint and redoes
  every committed batch after it through the live code path (D-025).

## 5. Constraints and substitutions

| Wanted | Using | Cost |
| --- | --- | --- |
| Every engine sensor, direct | The Mode 01 PIDs the ECU publishes (D-022) | Only what the car reports; oil pressure probably absent; lists differ by car |
| A fleet of failing engines | Induced, reversible faults on two healthy cars (D-021) | A few fault areas; no real wear-out ever seen |
| Raw CAN at the ECU's rate | Bluetooth Mode 01 request/response (D-019) | ~10-20 samples/s, no interrupt or bus timing, a link that can drop; most of the menu leaves the data path (D-017) |
| 48 continuous hours | 48 h wall-clock of normal use across key cycles (D-023) | A series of sessions; needs written confirmation (`profmail`) |
| Full hold-up through crank and shutdown | Owner's charger + seconds-scale supercapacitor (D-024) | Cold-start crank may reset the Pi; a charger live with the key off keeps the box on |
| Recording from key-on | A Pi 4 boot of about 30 s (`bootbudget`) | The first half-minute of warm-up is lost |
| The product capturing the first drives | A Python tool, labeled `live` (D-033) | Shapes the design, proves nothing about `obdd`; enters the product only as `replay` |

## 6. Evaluation plan

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

## 7. Ownership map

A subsystem belongs to whoever lands its first substantive commit (D-026).

| Subsystem | Owner | Evidence |
| --- | --- | --- |
| `src/ipc/` | Camden | the SPSC ring and its tests (M0) |
| `src/obd/`, `src/store/`, `src/analysis/`, `src/supervisor/`, `src/interface/`, `src/common/`, `tools/train/`, `electricalDrawing/` | unclaimed | |

## 8. AI-use plan

Claude Code is used for planning, explaining kernel and serial mechanics, test fixtures,
tooling, adversarial review of diffs and documentation; never at runtime, and not for the
analysis pipeline's design decisions. `src/` holds no network code except the read-only
status page in `src/interface/`, which serves the box's own access point with no upstream
(D-013, D-028); `make boundary`, part of `make test`, fails the build otherwise.

## 9. Changelog (M5)

What this M2 version got wrong, written at M5.
