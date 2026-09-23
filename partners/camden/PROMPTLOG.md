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

## E-01 — The board that never populated

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

**Milestone:** 1?  **Date:9/21/2026**  **Category:** plan change

**Context.**

**What I asked.** The actual prompt, or its substance. Include the evidence I pasted.

**What came back.** Summarized, with the part that mattered quoted.

**My judgment.** I had been digging around into ways to make accessing CAN simple for far
too long before even beginning the dreaded goal of untangling the car's CAN messages.

**Outcome.** Commit(s), and what I would do differently.
