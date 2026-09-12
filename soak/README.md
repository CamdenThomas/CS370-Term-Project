# soak/ — the 48-hour unattended run

The handout requires the logs to show, at minimum:

1. A timestamped startup.
2. A heartbeat **at least hourly** recording process liveness, resident memory (RSS),
   and cumulative event counts.
3. **At least one injected fault** during the run — physically unplug a sensor for ten
   minutes, or `kill -9` a sensor process — with detection, degradation, and recovery
   visible in the log.
4. An orderly state at the end.

Raw, unedited logs are the graded deliverable. **Do not tidy them.** Real systems are
never that tidy, and a cleaned soak log is the artifact this course is best equipped to
notice.

## Before the graded run

Start soak-style overnight runs at small scale from **M3**, not M4. Every leak found in
Week 9 is a crisis avoided in Week 14. `make soakcheck` runs a one-hour miniature.

Leave margin to run the graded soak **twice**. You will want to.

## Open: what the soak runs against

See **D-006**. Until it closes, build as if live sensors are required.
