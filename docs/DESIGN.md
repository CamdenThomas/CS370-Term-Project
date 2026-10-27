# Design document — carwatch

> **WEDNESDAY REVIEW — delete this box, and every 🟨 / 📏 note, before submitting.**
> 🟨 **TEAM** = decide or review together. 📏 **DRIVE** = replace with a number from the
> first Forester drive (`firstcap`, `pidforester`). Checklist:
>
> - [ ] Sign, change or reject D-019 to D-036 in `DECISIONS.md` (the doc cites them).
> - [ ] Decide the fault-area rows in §3 and fill the ownership map in §9.
> - [ ] Drive numbers into §1, §3 and §5; Lance's power numbers into §6.
> - [ ] Three-isolated-workers check: design-only reader, BOM-only buyer, wiring-only
>   builder each succeed from their own part.
> - [ ] Render to PDF: 5 pages or fewer without this box.

**User and problem.** Lance drives a 2015 CR-V daily, and nothing reads his engine's
sensors except a check-engine light that speaks only once something has failed. carwatch
is a box that boots at every key-on, records every Mode 01 sensor through a Bluetooth
OBD2 dongle, learns *this* car's normal, and flags behavior that leaves it before the
light would. It is also tested on Camden's 2008 Forester (D-022). **Claim (D-021):** it
learns the normal of the car it is installed in, flags behavior outside it and names the
likely fault area, with measured catch and false-alarm rates on faults we induce.

## 1. Architecture

```text
OBD2 port -- OBDLink LX (Bluetooth SPP, D-019)   DS18B20 cabin probe (D-037)
   ~~ RFCOMM ~~> BlueZ -> rfcomm tty -> /dev/obd    1-Wire -> kernel w1-gpio -> sysfs
  obdd    Mode 01, one outstanding, ~10-20 req/s   ambientd   one read every 2 s
    | ring 1: SPSC, ~10-20 samples/s                  | ring 2: SPSC, 0.5 samples/s
    +------------------------+------------------------+  shared memory, refuse when full
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
10 requests a second; the logging tool reaches 22 on `obdsim`. One reply, or one probe
reading, is one sample, one record and one analysis step. `obdctl` and the page ask the
supervisor, which asks the process holding the answer; `analyzed` never reads the ring
(D-031).

> 📏 **DRIVE:** measured request rate, reply-time p50/p99 and PID count replace the
> estimates here and in §5.

## 2. Mechanism mapping

D and E are the committed pair (D-017); B and F stay declined: Mode 01 and a 0.5 Hz
probe never stress a ring, and the probe's 1-Wire timing belongs to the kernel, not to
us (D-029, D-037).

| Item | Component | Justification from the user's requirements | Measured by |
| --- | --- | --- | --- |
| D | `storaged`; checkpoints in `analyzed` | Power can vanish at any instant (empty bank at a cold start, pulled cable, crank reset, D-024), yet the record and weeks of learned baselines must survive, and no baseline may learn from a sample a cut later erased (D-031). Hence append-only CRC'd records, 1 s commits (D-035), A/B checkpoints (D-025) | 50 cuts of the Pi's 5 V rail mid-write; records lost per cut |
| E | `supervisor`, `src/ipc/` | A recorder that dies silently harms an owner who thinks it is on duty; the least trusted code (analysis) must not take capture or storage down (B.2). Each sensor source has its own process: a hung Bluetooth link cannot stop the probe, nor a dead probe the engine sensors (D-037) | `kill -9` each child 20×: detection, recovery, fd leaks |

**Two physically distinct sensors that cooperate (handout §3.1).** The DS18B20 probe on
the Pi and the car's coolant and intake-air sensors are different devices on different
buses, read by different processes (D-037). They cooperate in the cold-soak check (§3):
after hours off (the clock says how long, D-030) all three should agree, and the one
that disagrees is the suspect. Beyond the probe, the engine's sensors are distinct
transducers the dongle only carries, and every diagnostic fuses at least two (D.4). "Both
sensors alive" (M2) means a real Mode 01 capture and a pasted probe reading; "through the
pipeline" (M3) means both arrive as committed records labeled `live`.

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
| Cooling | coolant, intake temp, ambient (PID 0x46, else the probe), load, speed | warm-up slope or warm settling temperature off its bin's normal for the same ambient and load | cardboard over the radiator |
| Air/fuel | fuel trims, MAP or MAF, rpm, O2 | long-term trim rising at idle with idle MAP, shrinking with load | a small vacuum line opened |
| Sensor/electrical | the failed PID and its usual partners; module voltage; at a cold start, coolant, intake temp and the probe | one PID absent or flat while its partners move; after a long soak, one temperature off the other two | a sensor unplugged |
| Unknown | all | residual high, no area matches | — |

No row can be called by one threshold on one value: each compares sensors that normally
move together, inside one bin (D.4). 📏 **DRIVE:** strike any sensor the survey shows the
Forester does not publish.

> 🟨 **TEAM DECIDES (Wednesday):** the fault-area rows, signatures and bin axes above are
> Claude's proposal from D-021 and D.2. The AI-use plan (§10) says analysis design is
> ours, so confirm, change or replace each row, and then delete this note.

## 4. Failure-mode table

Log lines are `<process> <level> <event> key=value` with `src`; the table names the event.

| Component | Failure | Detection | Response | Log event |
| --- | --- | --- | --- | --- |
| adapter | Bluetooth drops | `read` 0/`EIO`, `POLLHUP` | reopen `/dev/obd` with backoff | `link_lost`, `reconnected` |
| adapter | garbage or timeouts | parse failure; timeout | 3 in a row: `ATZ`, then reopen | `reply_bad`, `adapter_reset` |
| adapter | engine off, key in ACC | `0100` gets `NO DATA` | poll `0100` every few s, record nothing | `ecu_silent` |
| ECU | a sensor unplugged or failed | 5 `NO DATA` in a row; or the value pinned or flat while its partners move, with the MIL on and the DTC count up (PID 0101) | mark absent or suspect, retry each minute; report it. Most ECUs substitute a default value and light the CEL, so this fault is not caught before the light | `pid_absent`, `pid_flat`, `mil_on`, `pid_present` |
| probe | unplugged, broken wire, bad read | device gone from sysfs; CRC `NO`; the 85.0 °C power-on value | mark absent, retry every 10 s; cold-soak check off, reported | `sensor_absent`, `sensor_present`, `reply_bad` |
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

## 6. The box: hardware and safe power-down

One small box: a Pi 4, one USB-C sink input (5.1 kΩ on CC, so never more than 5 V) from
the owner's charger, fuse and transient protection, an input current and inrush limit, a
supercapacitor bank (two 2.7 V cells, about 10 F, about 105 J usable, estimate) behind a
5 V / 3 A boost converter, an I2C real-time clock (D-030), a DS18B20 probe on a short
lead outside the shell (D-037) and one tri-color light (D-027). Two signals reach the
Pi: **source-present** (charger live) and, from the Pi, **halt-done**, after which the
power stage cuts the Pi even if the source has returned, then re-enables it (D-024).
Nothing on the car is cut or spliced. Buyer: `hardware/BOM.md`; builder: the schematic
and `hardware/wiring.md`.

| State | Entered when | Supervisor does | Light |
| --- | --- | --- | --- |
| `BOOTING` | power-on (~30 s, `bootbudget`) | start children, await heartbeats | steady yellow (hardware pull) |
| `RUNNING` | all healthy, source present | record | green pulse |
| `DEGRADED` | child down, dongle silent with engine on, storage or power fault | record what it can; log absent vs crashed | red blink |
| `GRACE` | source-present lost | keep recording; source back → `RUNNING` (a crank dip) | yellow blink |
| `SHUTTING_DOWN` | grace (~3 s) expires | stop `obdd`, final `COMMIT` + `fsync`, checkpoint, `sync`, halt; source back → finish, then reboot | yellow blink |
| `OFF` | halt-done; power stage cuts the Pi | nothing | dark |

Energy (estimates, D-024): grace plus shutdown need 65–85 J of about 105 J usable. A
crash at any point is still recovered by mechanism D.

> 🟨 **TEAM / LANCE (Wednesday):** the power block's parts, the bank size, the grace
> period and the shutdown time are estimates until `powerspec` and `capbench` measure
> them; replace each with a number or mark it "estimate". Decide what the box does when
> a charger stays live with the engine silent (`shutfsm`).

## 7. Constraints and substitutions

| Wanted | Using | Cost |
| --- | --- | --- |
| Every engine sensor | The Mode 01 PIDs the ECU publishes (D-022) | Oil pressure likely absent; lists differ by car |
| Outside air temperature | A DS18B20 in the cabin (D-037) | Reads cabin air: ambient only after a long soak, and sun-heated in summer |
| Failing engines | Induced, reversible faults on two healthy cars (D-021) | A few fault areas; no real wear-out |
| Raw CAN at ECU rate | Bluetooth Mode 01 request/response (D-019) | ~10–20 samples/s, no bus timing, a link that drops; most of the menu leaves the data path (D-017) |
| 48 continuous hours | 48 h of normal use across key cycles (D-023) | Sessions with gaps; the professor confirmed this reading (D-023) |
| Full hold-up | Owner's charger + seconds-scale supercapacitor (D-024) | A cold-start crank may reset the Pi |
| Recording from key-on | A ~30 s Pi 4 boot (`bootbudget`) | The first half-minute of warm-up is lost |
| The product on the first drives | A Python tool, labeled `live` (D-033) | Proves nothing about `obdd`; enters the product only as `replay` |

## 8. Evaluation plan

> 🟨 **TEAM:** these targets are D-036, a draft; agree each number or change it.

| Measurement | Method | Target |
| --- | --- | --- |
| Reply → durable, p50 / p99 | timestamps at tty read and after `fsync`; idle and with a CPU hog | ≤ 1.2 s / ≤ 2.5 s |
| Sustained request rate | answers per second from the log, per drive | ≥ 10/s; every model input every ≤ 2 s |
| `kill -9` each child | 20 trials per child, from the log | detect ≤ 100 ms; heartbeat ≤ 5 s; no fd leak |
| Ring drops under contention (B.3) | sequence accounting, both rings | 0 |
| CPU per process | `utime` + `stime` from `/proc/<pid>/stat` in every heartbeat; idle and with a CPU hog | each daemon ≤ 10 % of one core; all ≤ 25 % |
| RSS over the soak | heartbeats at start, hourly, end, per session | slope < 100 KB per running hour |
| fsync batch size (`expbatch`) | per record, 1 s and 10 s batches on the Pi's SD: `fsync` p50 / p99, bytes written, records lost per cut | 1 s loses ≤ 1 s per cut and writes ≤ ⅕ of per-record's bytes |
| Every failure-table row (§4) | inject it as EVALUATION.md §5 lists; read the log | its log event appears; a fault outside capture never stops capture |
| Probe unplugged (D-037) | pull it with everything running | `sensor_absent` ≤ 10 s; engine capture unaffected |
| Detection per fault area | 5 induced events per area | ≥ 4 of 5; ≤ 1 false verdict per 10 h |
| Recovery after a cut mid-write | cut the Pi's 5 V rail after the converter | 50 / 50 recover; ≤ 1 batch lost |
| Boot to first durable sample | log time against the source-present edge | ≤ 45 s |

## 9. Ownership map

A subsystem belongs to whoever lands its first substantive commit (D-026).

| Subsystem | Owner | Evidence |
| --- | --- | --- |
| `src/ipc/` | Camden | the SPSC ring and its tests (M0) |
| `tools/` OBD2 logging and analysis tools | Camden | `obdlib` to `obdbins` (M2, PR #155) |
| `src/obd/`, `src/ambient/`, `src/store/`, `src/analysis/`, `src/supervisor/`, `src/interface/`, `src/common/`, `tools/train/`, `electricalDrawing/` | unclaimed | |

> 🟨 **TEAM:** the rubric grades this map. Agree who takes each unclaimed row (D-026
> still applies: the first substantive commit is the claim) and fill it in.

## 10. AI-use plan

Claude Code is used for planning, explaining kernel and serial mechanics, test fixtures,
tooling, adversarial review of diffs and documentation; never at runtime, and not for the
analysis pipeline's design decisions. `src/` holds no network code except the read-only
status page in `src/interface/`, which serves the box's own access point with no upstream
(D-013, D-028); `make boundary`, part of `make test`, fails the build otherwise. Every
choice Claude drafts lands in `DECISIONS.md` as UNREVIEWED until one of us signs it;
D-032 to D-036 and much of this text were drafted that way, and the transcripts are
committed.

## 11. Changelog (M5)

What this M2 version got wrong, written at M5.
