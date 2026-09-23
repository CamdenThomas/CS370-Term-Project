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
> Distributions, not just means. Under both idle and loaded CPU.

- OBD reply arrival (tty read) → durable in the log: p50 / p95 / p99 / max, idle and loaded
- Histogram, not a table of means. [figure]

## 3. Resource footprint
> CPU and RSS per process, at steady state and **over the full soak**. The hourly
> heartbeats give you this series for free — plot it.

[figure: RSS vs. time, all processes, full 48 h]

A flat line here is the whole point. A device that leaks for 48 hours and happens not
to die has not passed the soak; it has outrun it.

## 4. Domain metric
> The one the product lives or dies by.

Detection accuracy against the ground truth we constructed: per-diagnostic confusion
matrix over the induced-fault fixtures, with the false-positive rate called out
separately because a monitor that cries wolf twice gets unplugged.

## 5. Fault-injection results
> For each failure mode in the design document's table: what we injected, what the
> system did, and **the log excerpt proving it**.

| Injected | Expected | Observed | Warning light (D-014) | Log excerpt |
|---|---|---|---|---|
| Sensor unplugged 10 min | | | | |
| `kill -9 obdd` | | | | |
| `kill -9 supervisor` | | | | |
| Power cut mid-write | | | | |
| Disk full | | | | |

## 6. Mechanism comparisons
> Required by our menu choices. Present as experiments: method, data, conclusion.

### 6.1 Crash consistency (mechanism D)
N power-cut trials, recovery outcome each time, bytes lost per trial.

### 6.2 Supervision (mechanism E)
`kill -9` each child N times: time to detection, time to recovery, what degraded in the
meantime, and proof that no file descriptor leaked across the restart.

### 6.3 Interrupt-driven vs. polling (mechanism B — only with `pisensor`)
Method · data · conclusion. Include the CPU cost of the polling design at the rate
needed to match the interrupt design's drop rate — that comparison is the point.

### 6.4 No-drop ring under contention (mechanism F — only with `pisensor`)
Sustained rate with sequence accounting proving zero drops, under `stress-ng`.

## 7. Limitations
> Plainly. Closes the loop on the constraints and substitutions declared in docs/DESIGN.md §5.

"An honest limitations section is worth more at the defense than a suspiciously perfect
results section, and we notice which one we are reading."

Name at minimum: the fault classes we could not induce, what one vehicle cannot tell
us about a second, how long a baseline our window actually covers versus what the oil
trend deserves, and every place a number came from replay rather than live driving.
