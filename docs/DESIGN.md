# Design document — carwatch

**User and problem.** Lance drives a 2015 CR-V daily, and nothing reads his engine's
sensors except a check-engine light that speaks only once something has failed. carwatch
is a box that boots at every key-on, records every Mode 01 sensor through a Bluetooth
OBD2 dongle, learns *this* car's normal, and flags behavior that leaves it before the
light would. It is also tested on Camden's 2008 Forester (D-022), which made the first
live capture on 2026-10-28 (`captures/forester/20261028T220659.957/`). **Claim (D-021):**
it learns the normal of the car it is installed in, flags behavior outside it and names
the likely fault area, with measured catch and false-alarm rates on faults we induce.

**Scope, cut 2026-10-28 with the TA's agreement.** The product is a Raspberry Pi 4 on the
owner's USB-C charger, an OBDLink LX dongle read over Bluetooth, and a phone page over
the box's own Wi-Fi. There is no sensor of our own, no battery, no supercapacitor and no
cooling: the engine's own sensors are the cooperating sensors, and every key-off is a
power cut. §7 says what that costs.

## 1. Architecture

```text
OBD2 port -- OBDLink LX (Bluetooth Classic SPP, D-019); the Forester speaks CAN 11-bit/500 k
   ~~ RFCOMM ~~> BlueZ -> /dev/rfcomm0 -> /dev/obd
  obdd       Mode 01, one request outstanding; measured 20.7 replies/s,
    |        request cycle p50 42 ms, p99 91 ms; rpm and load 2.6 Hz, each other PID 0.4 Hz
    | ring: SPSC in shared memory, 20.7 slots/s; refuse and count when full, never overwrite
    v
  storaged [D]  BEGIN..records..COMMIT, one fsync per second or 64 samples;
    ^           1.2 KB/s, about 4 MB per running hour -> 16 MB segments, ext4 on the SD card
    |           events from every daemon: one SEQPACKET Unix socket, a few per drive (D-042)
    | committed records only, read back from the log, about 1 s behind (D-031)
    v
  analyzed      bins -> baselines -> residuals -> trends -> model -> FSM; a few verdicts per drive
    v
  supervisor [E] fork/exec, heartbeats, restart with backoff --GPIO--> tri-color light (D-027)
    ^  Unix socket /run/carwatch/ctl.sock, on demand (D-043)
  obdctl (CLI)   statusd: GET-only HTTP over a poll() loop; page, JSON, CSV -> phone (D-013, D-045)
power: the owner's USB-C charger -> Pi 4. It dies at key-off, so every key-off is a power cut.
```

Every rate above is from the first Forester capture (54 s, 1113 samples, one ECU
answering, no `NO DATA`, no errors), made by the Python logger (D-033); `obdd` must match
it. One reply is one sample, one record and one analysis step. Queries go to the
supervisor, which asks the process holding the answer (D-043); `analyzed` never reads a
ring (D-031). `statusd` reads a *source* behind one function; today that source is
synthetic and every row it serves says `src=synth` (D-045), and the real one is a client
of the control socket.

## 2. Mechanism mapping

D and E are the committed pair (D-017). B and F stay declined: 21 samples a second never
stresses a ring, and there is no sensor of our own to drive by interrupt.

| Item | Component | Justification from the user's requirements | Measured by |
| --- | --- | --- | --- |
| D | `storaged`; checkpoints in `analyzed` | Power vanishes at every key-off and whenever the cable is pulled, yet the record and weeks of learned baselines must survive, and no baseline may learn a sample a cut later erased (D-031): append-only CRC'd records, 1 s commits, A/B checkpoints (D-025, D-035) | 50 cable pulls mid-write; records lost per cut; recovery time |
| E | `supervisor`, `src/ipc/` | A recorder that dies silently harms an owner who thinks it is on duty. Analysis, the least trusted code, must not take capture or storage down; the page server, the only network code, must not either | `kill -9` each child 20×: detection, recovery, fd leaks |

**Two distinct sensors that cooperate (handout §3.1).** The coolant thermistor, the
intake-air thermistor, the ambient-air sensor, the manifold pressure sensor, the mass
air-flow sensor and the oxygen sensors are separate transducers; the ECU only carries their
readings, and the dongle relays them. They want different rates (rpm and load every 0.4 s,
the rest every 2.6 s) and are reconciled in one pipeline: every sample is binned by the
rpm, load and coolant temperature current when it arrived, and every diagnostic in §3
fuses at least two of them. The TA agreed on 2026-10-28 that this satisfies §3.1 without
a sensor of our own. "Both sensors alive" (M2) is the committed live capture; "through the
pipeline" (M3) is the same sensors as committed records labeled `live`.

## 3. Analysis: how normal is learned and a fault area named

All our code (handout §3.3). Per committed sample, `analyzed` (1) bins by rpm × load ×
coolant temperature, so a cold idle is never compared with a warm cruise; (2) keeps per-bin
running mean and covariance (Welford), in A/B checkpoints; (3) scores the Mahalanobis
distance from the bin's baseline, which fires when a *partnership* breaks; (4) fits
Theil–Sen slopes of residuals across drives; (5) names the area with our classifier
(trained in `tools/train/`, inference in C; without weights, 1–4 still flag); (6) surfaces
verdicts through a hysteresis-and-dwell state machine.

The Forester publishes every sensor below (`hardware/pid-survey.md`); the CR-V is not yet
surveyed.

| Fault area (D-021) | Cooperating sensors (PIDs) | Signature | Induced by |
| --- | --- | --- | --- |
| Cooling | coolant 05, intake air 0F, ambient 46, load 04, speed 0D | warm-up slope or settled temperature off its bin's normal for the same ambient and load | cardboard over the radiator |
| Air/fuel | fuel trims 06 and 07, MAP 0B, MAF 10, rpm 0C, O2 15 and lambda 24 | long-term trim rising at idle with idle MAP, shrinking with load | a small vacuum line opened |
| Sensor/electrical | the failed PID and its usual partners; module voltage 42; at a cold start, coolant, intake air and ambient | one PID absent or flat while its partners move; after a long soak, one temperature off the other two | a sensor unplugged |
| Unknown | all | residual high, no area matches | — |

No row can be called by one threshold on one value: each compares sensors that normally
move together, inside one bin. The first capture already shows the partnerships: at a warm
idle, coolant sat at 96–98 °C with intake air at 40 °C and ambient at 19 °C, and long-term
trim at −5 % against a 22 kPa manifold.

## 4. Failure-mode table

Log lines are `<process> <level> <event> key=value` with `src`; the table names the event.

| Component | Failure | Detection | Response | Log event |
| --- | --- | --- | --- | --- |
| adapter | Bluetooth drops | `read` 0/`EIO`, `POLLHUP` | reopen `/dev/obd` with backoff | `link_lost`, `reconnected` |
| adapter | garbage or timeouts | parse failure; timeout | 3 in a row: `ATZ`, then reopen | `reply_bad`, `adapter_reset` |
| adapter | engine off, key in ACC | `0100` gets `NO DATA` | poll `0100` every few s, record nothing | `ecu_silent` |
| ECU | a sensor unplugged or failed | 5 `NO DATA` in a row; or pinned or flat while its partners move; MIL on (PID 01) | mark absent or suspect, retry each minute. ECUs often substitute a value and light the CEL: not caught before the light | `pid_absent`, `pid_flat`, `mil_on`, `pid_present` |
| any child | crash, `kill -9` | `waitpid` | restart with backoff; others run on | `child_exit`, `child_restart` |
| ring | consumer stalls | sequence gap; late heartbeat | refuse and count; restart `storaged`; red | `ring_full`, `heartbeat_late` |
| storaged | power cut mid-write (every key-off) | bad CRC or no `COMMIT` at boot | truncate the tail and the open batch (D-025) | `recovered` |
| storaged | disk full; `fsync` or write `EIO` | `ENOSPC`, return values | drop the oldest segment if allowed; hold the durable offset; degrade, red | `enospc`, `fsync_failed` |
| analyzed | torn checkpoint; bad model file | slot CRC; header CRC | other slot + redo; residuals only | `checkpoint_bad`, `model_unavailable` |
| `statusd` | a bad or oversized request; out of memory | the parser's limits; RSS | 4xx, 431 or 503; capture unaffected | `request_refused`, `report_failed` |
| supervisor | dies | systemd `Restart=always` | light freezes steady = "not running" | journal |
| clock | wall time untrusted at boot (no clock module, or its backup lost) | time before the build date | run on `mono_ns`; records carry `WALL_UNTRUSTED` | `rtc_invalid` |
| power | cable pulled, crank dip, or charger live at key-off | unclean end at the next boot; `vcgencmd` undervoltage; `0100` silent with power on | crash recovery (D); log; a silent ECU with power on is recorded, not a fault | `recovered`, `undervoltage`, `ecu_silent` |
| OS | power cut corrupts the root filesystem | boot fails | the root filesystem is read-only; only the log partition is written (board `rofs`) | journal |
| radio | Bluetooth and Wi-Fi share one chip | reply p99 in the heartbeat | measure (`btcoex`) | heartbeat |

## 5. Storage and data

- **Format (D-034, `include/store/record.h`):** a 40-byte header (`magic`, `kind`, `seq`,
  `mono_ns`, `wall_ns`, `src`, flags, `boot_id`), a typed body, a CRC32 over both. Kinds:
  sample, begin, commit, boot, shutdown, event. A sample is 56 bytes and keeps the reply's
  raw bytes beside the decoded value. 16 MB segments, never edited. A `src` of 0 is
  invalid and refused at the ring (D-040).
- **Rates, measured:** 20.7 samples/s is 1.2 KB/s, about 4 MB per running hour; a 32 GB
  card holds years of two-hour days.
- **Retention (D-041):** every segment is kept until the log filesystem is 80 % full, then
  whole segments go oldest first, never one the newest checkpoint still needs. Text logs
  go to the journal, size-capped.
- **fsync (D-035, `include/store/wal.h`):** commit at 1 s or 64 samples. A cut loses at
  most a second of signals that move over minutes; longer batches would lose warm-up at
  each cold-start reset; per-record `fsync` is 20× the flash wear. Measured in `expbatch`.
- **Crash story (coded, `src/store/wal.c`, tested across `kill -9`):** at boot `storaged`
  scans the newest segment, verifies CRCs, keeps everything up to the last `COMMIT`,
  truncates the rest (undo is truncation: nothing is overwritten), and writes a boot batch
  saying whether the last session ended clean. Since power dies at every key-off, almost
  every session ends unclean, and the soak measures recovery on every key cycle. `analyzed`
  loads the newest valid checkpoint and redoes later batches through the live code path
  (D-025).

## 6. The box: hardware and power

A Pi 4 with a 32 GB A2 microSD card, in a small shell with one USB-C port and one light;
the OBDLink LX in the car's OBD2 port; the owner's own USB-C charger and a short cable.
Nothing on the car is cut. Parts: `hardware/BOM.md`; connections: `hardware/wiring.md`.

Power is the charger's: live when the car runs, dead at key-off. The box therefore boots
unattended at every start and loses power at every stop. There is no stored energy and no
clean shutdown; mechanism D is designed for exactly that, and the root filesystem is
mounted read-only so a cut cannot corrupt the operating system (`rofs`). A clock module on
I2C keeps wall time while the box is off (`rtcspec`); without one, records carry
`WALL_UNTRUSTED` and time is monotonic only.

| State | Entered when | Supervisor does | Light (D-027) |
| --- | --- | --- | --- |
| `BOOTING` | power-on, about 30 s (`bootbudget`) | start children, await heartbeats | steady yellow |
| `RUNNING` | all healthy | record | green pulse |
| `DEGRADED` | child down, dongle silent with the engine on, storage fault | record what it can; log absent against crashed | red blink |
| `OFF` | power gone | nothing; the next boot recovers | dark |

## 7. Constraints and substitutions

| Wanted | Using | Cost |
| --- | --- | --- |
| Every engine sensor | the 43 Mode 01 PIDs the Forester publishes (D-022); the CR-V not yet surveyed | oil pressure absent; lists differ by car |
| A sensor of our own beside the car's | none, since 2026-10-28 | the cold-soak cross-check uses the car's ambient sensor (46), which the car's own heat biases |
| A clean shutdown at key-off | none: power dies with the charger | the last ≤ 1 s of every drive is lost; the OS must tolerate cuts (`rofs`); every key-off is a recovery test |
| Failing engines | induced, reversible faults on two healthy cars (D-021) | a few fault areas; no real wear-out |
| Raw CAN at ECU rate | Bluetooth Mode 01 request/response (D-019): 20.7 replies/s, each slow PID every 2.6 s | no bus timing; a link that drops; most of the mechanism menu leaves the data path (D-017) |
| 48 continuous hours | 48 h of normal use across key cycles (D-023) | sessions with gaps; the professor confirmed this reading |
| Recording from key-on | a ~30 s Pi 4 boot (`bootbudget`) | the first half-minute of warm-up is lost |
| The product on the first drives | a Python logger, labeled `live` (D-033, D-046) | proves nothing about `obdd`; enters the product only as `replay` |

## 8. Evaluation plan

| Measurement | Method | Target |
| --- | --- | --- |
| Reply → durable, p50 / p99 | timestamps at tty read and after `fsync`; idle and with a CPU hog | ≤ 1.2 s / ≤ 2.5 s |
| Sustained request rate | answers per second from the log, per drive | ≥ 15/s (20.7 measured by the logger); rpm and load every ≤ 0.5 s, every other input every ≤ 3 s |
| `kill -9` each child | 20 trials per child, from the log | detect ≤ 100 ms; heartbeat ≤ 5 s; no fd leak |
| Ring drops under contention | sequence accounting | 0 |
| CPU per process | `utime` + `stime` in each heartbeat; idle and with a CPU hog | each daemon ≤ 10 % of one core; all ≤ 25 % |
| RSS over the soak | heartbeats at start, hourly, end, per session | slope < 100 KB per running hour |
| fsync batch size (`expbatch`) | per record, 1 s, 10 s on the Pi's SD: `fsync` p99, bytes written, loss per cut | 1 s: ≤ 1 s lost, ≤ ⅕ the bytes |
| Every failure-table row (§4) | inject each (EVALUATION.md §5) | its event logged; capture never stops for another's fault |
| Detection per fault area | 5 induced events per area | ≥ 4 of 5; ≤ 1 false verdict per 10 h |
| Recovery after a cut mid-write | pull the USB-C cable, 50 times, engine running | 50 / 50 recover; ≤ 1 batch lost |
| Key-cycle recovery (`keycyc`) | N real drives; unclean endings and records lost, from the boot records | every session recovered; ≤ 1 s lost each |
| Boot to first durable sample | log time against power-on | ≤ 45 s |

## 9. Ownership map

A subsystem belongs to whoever lands its first substantive commit (D-026).

| Subsystem | Owner | Evidence |
| --- | --- | --- |
| `src/ipc/` ring and shared memory; `src/store/` record format, CRC32, write-ahead log and `storaged`; `src/common/` logging; build and `board_sync` | Camden | M0: 0e66851, 269e2c5; M3: 3860d8e, 9b145d4, aaac098, 972015a |
| `tools/` OBD2 logging and analysis tools; `scripts/capture.sh` and `captures/` | Camden | M2: PR #155, #183 |
| `src/interface/` HTTP parser, JSON, control-socket reply grammar, routes, `poll()` server, page and `statusd` | Lance | M3: 61be86f, 591718b, e316a32, 38c25ed (PR #184) |
| `src/obd/`, `src/analysis/`, `src/supervisor/`, `obdctl`, `tools/train/` | unclaimed | stubs only so far |

## 10. AI-use plan

Claude Code is used for planning, explaining kernel and serial mechanics, test fixtures,
tooling, adversarial review and drafting; never at runtime. A draft binds nothing until
one of us has read it: every choice Claude made without a human is named in the pull
request under "What I am least sure about" and in the `WHY` of the commit that made it
(CLAUDE.md §0, §7.7). The §3 fault-area rows and the §8 targets were drafted with Claude
and checked against the Forester capture; the transcripts are committed in `partners/`.
`src/` holds no network code except `statusd`, which serves the box's own access point
with no upstream (D-013, D-045); `make boundary`, in `make test`, fails the build
otherwise.

## 11. Changelog (M5)

What this M2 version got wrong, written at M5. Already known on 2026-10-28: the design of
2026-10-21 had a DS18B20 cabin probe with its own daemon, a supercapacitor power block
and a clean shutdown at key-off; all three were cut before any was built, and the code
still carries a second ring namespace reserved for the probe (board `probering`).
