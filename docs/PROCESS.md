# Process: why the rules are shaped this way

`CLAUDE.md` holds the rules, short enough to load every session. This file holds the
reasoning behind them, the worked examples, and the one-time setup. Section numbers in
parentheses name the `CLAUDE.md` rule each part explains.

**Read this when** a rule looks arbitrary, when you are about to argue for an exception,
or before the defense.

## Why the git history is graded (§7)

**This section is not advisory.** Our process grade is computed from transcripts
corroborated against commits; the git history is itself a graded deliverable; and neither
partner will break down a thousand-line diff to review it. A commit small enough to review
in ninety seconds is a commit that actually gets reviewed.

## Commit granularity (§7.3)

**There is no line-count limit.** A commit is sized by *what it is responsible for*, not by
how big the diff happens to be. A 200-line rewrite of a single function is **one** commit;
splitting it would force the reviewer to hold half a function in their head across two
reviews, which is worse than the problem it solves. Twenty lines spread across five
unrelated concerns is **five** commits, even though it is small.

**The test that decides it, every time:**

> Can the reviewer evaluate this as a **single yes-or-no decision**?

If approving requires accepting one part while objecting to another, it is more than one
commit. If rejecting any part means rejecting the whole thing anyway, it is one commit —
leave it alone.

**The one-sentence test, restated.** State the commit's responsibility in one sentence
without using "and". If you can, it is one commit no matter how many lines it took. If you
cannot, split along the "and".

### For code

The unit is **one behavior, one function, or one design principle applied.**

- Tests ship **in the same commit** as the behavior they test. Same responsibility.
- A refactor is its own responsibility — never folded into a feature.
- Formatting, renames, and file moves are **separate commits, labeled as such**, so the
  reviewer skims them at speed instead of hunting for logic buried in noise.
- Two subsystems means two commits, always, regardless of size.
- **Every commit builds and passes tests on its own.** This is the one hard rule in this
  section — it is what makes the history bisectable and each commit reviewable in isolation.

**Size as a smell, not a limit.** Past roughly 400 changed lines, stop and re-apply the
one-sentence test — large diffs *often* contain a smuggled second responsibility. If it
survives the test, commit it as one unit and **say in the `WHY` why it is indivisible.**
Then the reviewer knows the size was considered rather than accidental.

### For documentation, Markdown, and data files

The unit is **one resolved question or one decision** — not one file, not one section.

- Closing one open question that touches a line in six tables across three files is **one
  commit.** Splitting it is actively harmful: the reviewer can no longer see that the
  change was applied *consistently*, which is the only thing worth checking.
- A doc commit's diff is often wide and shallow. That is expected and correct.
- Never mix a **content** change with a **reflow/reformat** of the same file — the content
  change disappears into whitespace noise. Reformat separately, or not at all.
- Never mix filling in a TODO with restructuring the section around it.
- Never mix a decision's *record* with its *implementation* across files in a way that
  hides one inside the other — but **do** keep a decision and the code that enacts it
  together when they are genuinely one change.

**The check:** `git show <commit>` should answer exactly one question a human asked. If it
answers two, split. If it answers half of one, you committed too early.

### For schematics (§7.12)

Commit granularity for `electricalDrawing/` follows the documentation rule, not the code
rule: the unit is **one functional block or one resolved electrical question**. Wiring the
whole Power block is one commit even if it moves fifty symbols. Renumbering references
across the sheet is a separate, labeled commit — never folded into a wiring change, for
exactly the reason formatting is never folded into logic.

## A commit message, worked (§7.4)

The format itself is `.gitmessage` (enable it with
`git config commit.template .gitmessage`). A real example:

```text
M3 ipc: refuse push on full ring instead of overwriting

WHAT:   ring_push now returns false and increments an overrun counter when
        head - tail == RING_CAPACITY, rather than wrapping over slot[tail].
WHY:    DECISIONS.md B.3 — a silent overwrite is a gap in the record, and a
        gap in the record is a gap in the diagnosis.
REVIEW: The memory ordering on the tail load. I used acquire so the producer
        cannot see a stale tail and wrongly declare the ring full; I am least
        sure whether relaxed would be sufficient here.
VERIFY: make && make test && make asan — all green, test_ring 3/3.
```

## Pull requests (§7.5)

- **A PR is sized by its claim, not its diff.** If the title needs "and", split it. If a
  reviewer would have to approve part and object to part, split it. A PR that implements
  one coherent thing is the right size even when that thing was large — but say so in the
  body, and lay out the commit-by-commit list so the reviewer has a path through it.
- **Past ~10 commits or ~600 changed lines, re-apply the test rather than obey a number.**
  Most PRs that big contain two claims; the ones that genuinely do not are fine.
- **Draft at the first commit**, not at the end — so the partner can watch it grow and
  object early instead of at the finish line.
- The "What I am least sure about" paragraph must not be empty. If everything is certain,
  you have not thought hard enough about the diff.
- If a PR needs a diagram, an annotated log excerpt, or a timing capture to be
  understandable — put it in the PR body. Making the reviewer reconstruct context is how
  reviews stop happening.

## The merge gate (§7.6)

**If the reviewer does not understand a line, the pull request does not merge.**

The remedy is never "trust it." The remedy is, in order of preference:

1. Claude explains it in a PR comment, in plain language, tied to the requirement.
2. The commit is split further, so each piece is obvious on its own.
3. The code is rewritten more simply, even at a cost in elegance or a few instructions.

A reviewer must be able to answer these about **any line** in the diff before approving —
they are the demo-day question set, asked early:

- What wakes this? What is it waiting on?
- What happens to this file descriptor / this allocation when the child dies?
- What is the error path, and what does it log?
- Why can't this drop, duplicate, or reorder a sample?
- Which requirement or decision made it this way rather than the obvious alternative?

Neither partner approves a PR they could not defend at the keyboard, alone, on demo day.
**Approving code you do not understand is the only unrecoverable mistake in this
workflow** — it is 20 individual points, and the defense will find it.

## Volume and distribution (§7.9)

≥ 40 meaningful commits across the semester, **both partners well represented**. A history
where one partner authored under a quarter of the substantive commits is probed hard at the
individual defense. A pair of "final submission" commits is an automatic process-grade of
zero **for both partners**.

Commit *as you go*, inside the session. A session that produces one commit at the end has
already lost the granularity this section exists to protect.

## Repository settings (§7.10, humans, once)

On GitHub, set on `main`: require a pull request before merging · require 1 approval ·
dismiss stale approvals on new commits · require conversation resolution · block force
pushes · block deletions. Enable **squash-merge only if** the squash message is rewritten
to be meaningful — otherwise prefer merge commits, since our granular history *is* the
deliverable and squashing destroys it.

**As configured** (board item `ghsetup`, verified 2026-09-12): the `Main` ruleset has all
six, with **merge commits only**.

## Why issues, and why the board is generated (§7.11)

**GitHub Issues replace the blockers list.** Anything that needs a human — a question, a
decision, a measurement only one of us can take, a part that must be ordered — is an
**issue**, not a paragraph buried in a doc or a session summary nobody re-reads.

`docs/DECISIONS.md` keeps only what has been **decided** (plus the review queue).
Everything still open lives on the board, where it has an owner, a milestone, and a
visible position in the queue.

The board is not project-management theater. It is a timestamped record that the questions
were asked before the work, that a human answered them, and that the answer shaped the
code — which is precisely the process the transcripts are graded against, corroborated by
an independent system we did not write. How the board works: `docs/BOARD/BOARD.md`.

## Hardware pull requests (§7.12)

KiCad files are S-expression **text**, so hardware goes through the same branches, commits
and pull requests as the C — with two additions, both of which exist because a reviewer
cannot read a diff of coordinate tuples: the regenerated schematic SVG in the PR (without
it the PR is unreviewable, and §7.6 then blocks it — correctly), and pasted `make hwcheck`
output (ERC violations are build failures, the same way `-Werror` findings are).

**What Claude may not do here.** Claude cannot see the board. Symbol pin numbering,
footprint choice, placement, thermal, EMI and mechanical fit are all things it reasons about
blind, and hand-writing `.kicad_pcb` geometry is forbidden — the coordinate math fails
silently at fab. It is useful for netlist review, datasheet cross-checks, design rules, BOM
and export scripting, and reading ERC JSON. **A measured voltage always beats its
opinion.**

## Prompt quality bar (§8, handout §10.4)

Useless: "make the sensor work." Effective: "`/dev/obd` opens and `ATZ` answers, but
`010C` returns `NO DATA` with the engine running. Here is our init sequence and the raw
replies [paste both]. Diff them against the ELM327 datasheet's protocol-selection steps and
identify the missing one; do not rewrite the reader."

## Transcripts and the prompt log (§9)

Transcripts purge after 30 days by default and this project runs 15 weeks — losing Week-5
transcripts in Week 15 is a *foreseeable* loss and is not excused. That is why the copy-out
happens at the end of **every session**, not only at milestones: the milestone items
(`tx1`–`tx4`, `tarball`) then only confirm that nothing is missing.

`PROMPTLOG.md` needs 6–10 annotated episodes by M5, including one revised plan, one
rejected diff, one tool-output debugging loop with real hardware evidence, and one review
of the partner's work. **Write these as they happen**; an episode reconstructed in Week 15
reads like one.
