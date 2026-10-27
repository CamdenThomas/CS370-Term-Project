# Evaluation report — carwatch

> **STATUS: M5 SKELETON.** 3–4 pages. "Adjectives are not measurements."
> Claude can generate plausible code; it cannot fake your understanding of your own
> measurements — which is why the defense draws so heavily on this document. Every
> number in here must be one you can reproduce while someone watches.

## 1. Method and setup

Hardware, kernel version, build flags, what was running, how load was generated,
how many trials. Enough for a TA to re-measure.

**Every figure states whether its data is live, replay, or synth.** No exceptions.

## 2. Latency / timing
>
> Distributions, not just means. Under both idle and loaded CPU.

- OBD reply arrival (tty read) → durable in the log: p50 / p95 / p99 / max, idle and loaded
- Histogram, not a table of means. [figure]

## 3. Resource footprint
>
> CPU and RSS per process, at steady state and **over the full soak**. The hourly
> heartbeats give you this series for free — plot it.

Every heartbeat carries each process's RSS and its `utime` + `stime` from
`/proc/<pid>/stat`, so CPU is a series too, idle and under a CPU hog.

[figure: RSS vs. time, all processes, every session of the 48 h key-cycle window (D-023)]

A flat line here is the whole point. A device that leaks for 48 hours and happens not
to die has not passed the soak; it has outrun it.

## 4. Domain metric
>
> The one the product lives or dies by.

Detection against the ground truth we constructed (D-021): for each induced fault, was it
flagged, how fast, and was the fault area named right, as a confusion matrix over fault
areas, with the false-positive rate called out
separately because a monitor that cries wolf twice gets unplugged.

## 5. Fault-injection results
>
> For each failure mode in the design document's table: what we injected, what the
> system did, and **the log excerpt proving it**.

One row per failure-table row (`docs/DESIGN.md` §4). A row that cannot be injected says
why, here.

| Failure (DESIGN §4) | How we inject it | Expected event | Observed | Light (D-027) | Log excerpt |
| --- | --- | --- | --- | --- | --- |
| Bluetooth drops | pull the dongle for 10 min, engine running | `link_lost`, `reconnected` | | | |
| Garbage or timeouts | `obdsim` serving bad replies (`synth`) | `reply_bad`, `adapter_reset` | | | |
| Engine off, key in ACC | key to ACC | `ecu_silent` | | | |
| A sensor unplugged | unplug one engine sensor, engine running (lights the CEL) | `pid_absent` or `pid_flat`, `mil_on`, `pid_present` | | | |
| Probe unplugged | pull the DS18B20 for 10 min | `sensor_absent`, `sensor_present` | | | |
| A child crashes | `kill -9` each child, 20 times | `child_exit`, `child_restart` | | | |
| Ring consumer stalls | `SIGSTOP` `storaged` for 5 min | `ring_full`, `heartbeat_late` | | | |
| Power cut mid-write | cut the Pi's 5 V rail after the converter | `recovered` | | | |
| Disk full; `fsync` error | log directory on a small loop-mounted filesystem, filled | `enospc`, `fsync_failed` | | | |
| Torn checkpoint; bad model | truncate one checkpoint slot; corrupt the model header | `checkpoint_bad`, `model_unavailable` | | | |
| Report fails | make the report directory read-only | `report_failed` | | | |
| Supervisor dies | `kill -9` the supervisor | systemd restart, in the journal | | | |
| Clock invalid | remove the clock's backup, then boot | `rtc_invalid` | | | |
| Key-off, cable pulled, crank, source back | key cycles; pull the USB-C; return it during shutdown | `source_lost`, `source_dip`, `shutdown` | | | |
| Empty bank at a cold crank | cold start with the bank discharged | `recovered` | | | |
| Undervoltage, heat | a weak charger; a closed car in the sun | `undervoltage`, `thermal` | | | |
| Bluetooth and Wi-Fi share a radio | phone on the access point during a drive | reply p99 in the heartbeat | | | |

## 6. Mechanism comparisons
>
> Required by our menu choices. Present as experiments: method, data, conclusion.

### 6.1 Crash consistency (mechanism D)

N power-cut trials, each cutting the Pi's 5 V rail after the converter so the supercapacitor
cannot cover it: recovery outcome each time, bytes lost per trial.

### 6.2 Supervision (mechanism E)

`kill -9` each child N times: time to detection, time to recovery, what degraded in the
meantime, and proof that no file descriptor leaked across the restart.

### 6.3 Write batching and `fsync` discipline (mechanism D, `expbatch`)

The same `synth` stream through `storaged` on the Pi's SD card with one `fsync` per record,
per second (D-035) and per ten seconds: `fsync` latency p50 / p99, bytes written (from
`/sys/block/mmcblk0/stat`), and records lost per power cut. The conclusion says whether
D-035's one second was the right choice.

Mechanisms B and F were declined (D-029, D-037), so they have no comparison here.

## 7. Limitations

> Plainly. Closes the loop on the constraints and substitutions declared in
> docs/DESIGN.md §7.

"An honest limitations section is worth more at the defense than a suspiciously perfect
results section, and we notice which one we are reading."

Name at minimum: the fault areas we could not induce, what results on one testbed car say
(and do not say) about the other (D-022), how many weeks of normal the baseline actually
covers, and every place a number came from replay rather than live driving.
