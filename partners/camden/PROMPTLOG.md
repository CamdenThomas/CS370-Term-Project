# PROMPTLOG — Camden Thomas

**6–10 annotated episodes by M5.** Required coverage (handout §12):

- [ ] One **revised plan** — a plan you rejected or changed, and why
- [ ] One **rejected diff** — code the agent produced that you did not merge, and what
      was wrong with it
- [ ] One **tool-output debugging loop with real hardware evidence** — dmesg, a timing
      capture, /proc/interrupts before and after
- [ ] One **review of your partner's work** — what you questioned, what the answer was,
      what changed

> Write these **as they happen**. A log reconstructed in Week 15 reads like a log
> reconstructed in Week 15, and one of these episodes is defended out loud on demo day.
>
> "A team whose two halves never disagreed about anything all semester has not been
> reviewing."

---

## Episode template

### E-00 — (short title)

**Milestone:** M?  **Date:**
**Category:** plan / rejected diff / hardware loop / partner review

**Context.** What I was trying to do and what I already knew.

**What I asked.** The actual prompt, or its substance. Include the evidence I pasted.

**What came back.** Summarized, with the part that mattered quoted.

**My judgment.** What I accepted, what I rejected, and *why* — this paragraph is the
one that is graded and the one I will be asked to defend.

**Outcome.** Commit(s), and what I would do differently.

---

## Episodes

### E-01 — The board that never populated

**Milestone:** M0  **Date:** 2026-09-11  **Category:** revised plan

> **DRAFT — evidence only.** Everything below the line in *What came back* is the record
> of what actually happened, assembled from the session transcript so the details are
> exact rather than remembered. **"My judgment" is deliberately empty: that paragraph is
> the graded one and the one I will be asked to defend out loud, so it has to be mine.**
> Rewrite the rest in my own voice and delete this note before committing.

**Context.** `tools/bootstrap_board.sh` was supposed to create the whole GitHub board —
7 labels, 7 milestones, ~80 issues and ~90 blocked-by dependencies — in one run. I ran it
dry, then for real, twice. Nothing appeared in Issues, nothing appeared on the project
board, and the script never seemed to finish. I had no error message to work from.

**What I asked.** To diagnose why the script produced nothing, and then to compare the
generate-once-from-a-script approach against a one-time setup that I adjust incrementally
as questions get answered — because what I actually need is for answers from Lance and me
to propagate into the board and the docs, not for the whole plan to be regenerated.

**What came back.** Five separate faults, found by running things rather than reading the
script:

1. **The script was never executed.** My shell history showed `./tools/bootstrap_board.sh`
   run from PowerShell. `bash` is not on PATH there, so PowerShell handed the `.sh` file to
   the Windows file association instead of running it — which is both why nothing happened
   and why it appeared to hang. The same script run under Git Bash produced correct
   dry-run output immediately.
2. **It never touched the project board at all.** Its own closing lines read
   `Next: 1. Add all issues to the project board and set up the columns.` Even a perfect
   run would have left the kanban empty.
3. **The token had no `project` scope.** `gh project list` returned
   `error: your authentication token is missing required scopes [read:project]`.
4. **`gh issue dependency` does not exist** in gh 2.100.0, despite the script's header
   claiming the feature "landed 2026-06". All ~90 dependency links would have silently
   degraded to plain comments, at two failed API round-trips each.
5. **Every failure was swallowed** — `2>/dev/null` on issue creation, `|| echo "(exists,
   skipping)"` on milestones, and a `set -e` bug on the final line that makes a *successful*
   real run exit 1. A half-broken run and a working run looked identical.

The deeper finding was architectural, and it is the one that mattered: the script was a
**generator**, not a **reconciler**. It refused to run a second time (`Open issues already
exist. Re-running would create duplicates.`), so it could not be used to add a single task
later, and the plan text lived in 33KB of bash that could not safely be re-executed. Fixing
the five bugs would have left that intact.

**What replaced it.** `docs/board.toml` as the source of truth, `tools/board_sync.py` as an
idempotent reconciler, `docs/board.lock.json` holding the slug→issue mapping. Verified by
running it three times: the first created 80 issues, the second reported
`created 0, updated 40`, the third reported `created 0, updated 0, reads 7, writes 0`.
Items absent from the manifest are never touched.

**My judgment.**

> *(Mine to write.)* The questions worth answering here:
>
> - I accepted the rewrite rather than the five bug fixes. Was that the right call, or was
>   it scope creep dressed up as architecture? What would I say to someone who argued the
>   script was two hours from working?
> - The diagnosis came from running `gh` commands and reading my own shell history, not
>   from reading the script. Four of the five faults were invisible in the source. What
>   does that say about how I should direct the agent when something "doesn't work"?
> - The agent's original `bootstrap_board.sh` asserted a `gh issue dependency` feature that
>   does not exist, with a confident version number and date. I did not check it. What is
>   my rule for that class of claim from here on?

**Outcome.** Commits: *(fill in)*. What I would do differently: *(fill in)*.

### E-02 — Avoid Overcomplicating CAN

**Milestone:** M1  **Date:** 2026-09-21  **Category:** plan change

**Context.**

**What I asked.** The actual prompt, or its substance. Include the evidence I pasted.

**What came back.** Summarized, with the part that mattered quoted.

**My judgment.** I had been digging around into ways to make accessing CAN simple for far
too long before even beginning the dreaded goal of untangling the car's CAN messages.

**Outcome.** Commit(s), and what I would do differently.

### E-03 — Which adapter, and does it work on a Mac?

**Milestone:** M2  **Date:** 2026-10-22 to 2026-10-26  **Category:** plan

> **DRAFT — evidence only,** written by the agent from the session, like E-01. Rewrite it
> in my own voice, write "My judgment" myself, and delete this note.

**Context.** Before buying the OBDLink LX (D-019), I wanted to log a drive from my Mac to
study the data before designing the pipeline around it.

**What I asked.** Whether the LX would connect to a Mac, since OBDLink lists only Android
and Windows, and whether a cheaper adapter was guaranteed to work on both the Mac and the
Pi.

**What came back.** The agent first said "yes" too confidently, then corrected itself:
the vendor list is a support statement, macOS normally exposes a Bluetooth SPP device as
`/dev/cu.*`, but no adapter is *guaranteed* on macOS, and it found no report of the LX
itself on a Mac. It recommended buying from a returnable seller and testing at the car.

**My judgment.** *(Mine to write.)* Was "likely, not guaranteed" enough to buy on? What is
my rule for an agent's first confident answer about hardware compatibility?

**Outcome.** Bought the LX. Tools and a run book for the first drive on branch
`camden/m2/car-ready-tools` (PR #155). The Mac pairing result goes in
`docs/hardware/first-drive.md`.

### E-04 — The adapter that answered once

**Milestone:** M2  **Date:** 2026-10-28  **Category:** hardware loop

> **DRAFT — evidence only,** written by the agent from the session and the commits, like
> E-01. Rewrite it in my own voice, write "My judgment" myself, and delete this note.

**Context.** The first drive of the Forester, with `scripts/capture.sh` recording the
video clip for the M2 zip. The script ran `pidscan` (survey) and then `obdlog` (the drive
log) against the same Bluetooth serial port, as the run book said, and both had worked
back to back against `obdsim`.

**What I asked.** Why the survey printed 43 PIDs and then the logger sat at zero samples
on the same port, seconds later, engine running.

**What came back.** The evidence was the logger's own status line (0 samples, no error)
against the survey that had just succeeded, and `ls /dev/cu.*` still listing the port.
The agent's reading: on macOS, closing a Bluetooth SPP serial port drops the RFCOMM
link, and reopening the device node does not re-establish it, so the second process gets
a port that accepts writes and never answers. It is recorded in the script itself:

> On macOS, closing a Bluetooth serial port drops the link, and reopening the port does
> not bring it back: obdlog then gets no bytes at all (2026-10-28, OBDLink LX).

The fix (`b1a6819`): open a real adapter once per run. The logger owns the port, and
step 1 shows the newest live survey from disk instead of re-running `pidscan`. The next
run logged 1042 samples in 50.9 s (ended by Ctrl-C), and the one after it, 1113 samples
in 53.8 s at 20.7 samples/s, is the capture committed as `02ba151`.

**My judgment.** *(Mine to write.)* The simulator never showed this because a pty does
not have a link to drop. What is my rule for "it works on the simulator" from here on?

**Outcome.** Commits `b1a6819`, `02ba151`; `captures/forester/20261028T220659.957/`.
Open: whether the Pi's RFCOMM behaves the same way (`bench`).
