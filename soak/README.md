# soak/ — the 48-hour key-cycle run

**What the soak is (D-023):** a 48-hour wall-clock window of the car's normal use. The box
boots at every key-on, records, and shuts down at every key-off, on the live car
(`src=live`). The fake OBD2 port (`obdsim`) is a development tool and is never the soak's
data source. The professor confirmed this reading (reported by Camden, 2026-10-26;
board item `profmail`).

The handout requires the logs to show, at minimum:

1. A timestamped startup.
2. A heartbeat **at least hourly** recording process liveness, resident memory (RSS),
   and cumulative event counts.
3. **At least one injected fault** during the run — physically unplug a sensor for ten
   minutes, or `kill -9` a sensor process — with detection, degradation, and recovery
   visible in the log.
4. An orderly state at the end.

Under D-023 these apply per session: every session logs its start, a heartbeat at start,
at least hourly while running and at its end, and says whether it ended in a clean
shutdown or was recovered at the next boot. The off periods between drives are deliberate
gaps between a logged shutdown and a logged boot.

Raw, unedited logs are the graded deliverable. **Do not tidy them.** Real systems are
never that tidy, and a cleaned soak log is the artifact this course is best equipped to
notice.

## Before the graded run

Start soak-style runs at small scale from **M3**, not M4: daily drives, plus overnight
bench runs on a home charger against `obdsim` (labeled `synth`) to find leaks. Every leak
found in Week 9 is a crisis avoided in Week 14. `make soakcheck` runs a one-hour bench
miniature.

Leave margin to run the graded soak **twice**. You will want to.
