# CS370 Term Project — Team Rules

**Project:** `carwatch` — an always-on in-vehicle diagnostic recorder that learns *this*
car's normal and reports developing faults before a check-engine light would.

**Team:** Camden Thomas (`crash8750@gmail.com`) · Lance Baron
**Course:** CS 370 Operating Systems, Colorado State University (Pallickara)
**Handout:** `CS370-TermProject.pdf` v1.0 (2026-08-03) — the rubric is law; this file is
how we obey it.

> **Read this file every session.** It is team law, maintained by both partners. A
> constraint that lives only in one partner's head protects nothing.

---

## 0. How to use this file

You (Claude) are required in our **workflow** and forbidden in our **product**
(handout §3.3, §10.3). Every session, in this order:

1. Read this file.
2. Read `docs/milestones.md` — where we are and what the exit criteria are.
3. Read `docs/DECISIONS.md` — what is already settled, and what is awaiting review.
4. Read `docs/BOARD.md`, then run `python tools/board_sync.py --status` — what is Ready,
   what is holding up the critical path, and how long each open question has been open.
5. State whose session this is. Do not edit outside that owner's area without being told.
6. `git status` must be clean. Create the branch **before** the first edit.

Do not reopen a decision marked 🔒 LOCKED without saying so explicitly and getting a
human yes.

---

## 1. Commands

```
make              # build everything, -Wall -Wextra -Werror clean
make test         # unit tests + the boundary check
make asan         # build+run tests under AddressSanitizer + UBSan
make memcheck     # valgrind over the test binaries
make soakcheck    # 1-hour miniature of the 48h run
make hwcheck      # KiCad ERC; violations fail the build, like -Werror
make hwdocs       # regenerate schematic SVG, BOM and netlist from the schematic
make replay FIX=fixtures/<name>   # run a labeled capture through the real pipeline
make deploy PI=pi@<host>          # rsync + remote build
make clean

python tools/board_sync.py --status    # what is Ready, what is stuck, who owns it
python tools/board_sync.py --check     # has the board drifted? (exit 1 if yes)
python tools/board_sync.py             # reconcile docs/board.toml onto GitHub
```

**A change is DONE only when `make`, `make test`, and `make asan` all pass, and you have
shown me the output.** Not "should pass." Output, pasted.

---

## 2. Hard constraints — these are graded boundaries

1. **No hosted models in the product.** No LLM API calls, no cloud inference, no network
   calls of any kind except serving its own LAN interface. A grader must confirm this in
   sixty seconds by reading the tree. `src/` contains no HTTP client. Ever.
2. **Our model, our weights.** Classifier trained by us, on our data, training code in
   `tools/train/`, weights versioned in `models/`. No third-party pretrained weights.
   Inference is C we wrote.
3. **The device works with the network unplugged.** Full stop.
4. **Systems core is C17**, `-Wall -Wextra -Werror`, no VLAs, `goto`-cleanup for
   multi-resource functions. Every allocation checked. Every syscall error path handled
   *and logged*.
5. **Python lives only in `tools/` and `ui/`.** No graded mechanism may hide there.
6. **Every daemon is supervisable:** clean exit codes, no orphaned fds across restart,
   heartbeat within 60 s of start, survives `kill -9` of any sibling.
7. **Replayed or synthesized data is labeled as such EVERYWHERE** — every record carries
   `src=live|replay|synth`. Integrity boundary, not a style choice.
8. **Never weaken, skip, or delete a test to make the suite pass.** If a test is wrong,
   say so and fix it deliberately, in its own commit, with the reason.

---

## 3. What we are building (the one-paragraph version)

A Raspberry Pi riding in a daily-driven car. A USB OBD2 adapter in the car's OBD2 port
gives it the car's own sensors (D-015); the car's USB-C port powers it — nothing is cut or
spliced (D-011). Through standard Mode 01 requests (D-012) it reads oil temperature,
coolant temp, MAP, O2 / fuel trims, RPM and load, and builds a statistical model of what
normal looks like *for this car, at this operating point*. It then reports deviations
that a mileage sticker and a check-engine light both miss. The owner reads it from their
phone over the device's own Wi-Fi — read-only, no internet (D-013); `obdctl` stays the
exact interface. A single warning light on the box blinks while it is on duty and
changes pattern when something needs attention (D-014).

**The user:** a named owner of a specific car who wants maintenance driven by measured
condition instead of a generic interval. Testbed: **Lance's 2015 Honda CR-V EX-L**, the
only one (D-016). Its sticker and its Maintenance Minder both *estimate* from how far and
how hard it has been driven; the device *measures* the engine's condition (D-007).

**The claim we defend:** *"detects the three faults we can induce on this vehicle, with
these measured error rates."* Not "predicts failure." (handout §5)

---

## 4. Mechanism commitments

We need two. We commit to **D** (append-only crash-consistent storage — power is cut at
every key-off) and **E** (multi-process with a supervisor), and measure both. **B**
(interrupt-driven input vs polling) and **F** (high-rate no-drop ring) come back only if a
Pi-side sensor is added (board item `pisensor`): Mode 01 through a USB adapter is too slow,
and has no interrupt of ours, to justify either. **A**, **C** and raw CAN are stretch only.
Full rationale: `docs/DECISIONS.md` §B.1 (D-017).

---

## 5. The intelligence

All of it ours. Operating-point binning → per-bin baselines → Mahalanobis residuals →
Theil–Sen trends → our classifier → state machine with hysteresis. Rationale and the
standing "threshold test" rule: `docs/DECISIONS.md` §D.

---

## 6. Ownership map

| Owner | Subsystems |
|---|---|
| **Camden** | `src/can/`, `src/ipc/` |
| **Lance** | `src/store/`, `src/analysis/`, `tools/train/`, **`electricalDrawing/`** |
| **Shared** | `src/supervisor/`, `src/interface/`, `src/common/`, `Makefile`, docs |

Ownership means first authorship and answerability at the defense, not exclusivity.

---

# 7. Git and pull-request law

**This section is not advisory.** Our process grade is computed from transcripts
corroborated against commits; the git history is itself a graded deliverable; and neither
partner will break down a thousand-line diff to review it. A commit small enough to review
in ninety seconds is a commit that actually gets reviewed.

## 7.1 The five absolutes

Claude may **never**, under any instruction short of an explicit human override in the
current session:

1. **Commit to `main`.** Every change reaches `main` through a pull request.
2. **Merge, approve, or close a pull request.** Only a human merges. Ever.
3. **`push --force`, force-with-lease, rebase a pushed branch, amend a pushed commit, or
   `reset --hard` anything that has been pushed.** History that reviewers have read is
   immutable.
4. **`git add -A`, `git add .`, or `git commit -a`.** Stage explicit paths, always. An
   unintended file in a diff destroys a reviewer's trust in the whole PR.
5. **Commit a file whose full current contents were not read this session.** No blind
   edits to code you have not looked at.

Violating any of these is a stop-work event: say so, and do not attempt a workaround.

## 7.2 Branches

- `main` is protected, always green, and never committed to directly.
- **One branch per unit of work.** Naming: `<owner>/<milestone>/<slug>`
  — `camden/m3/obd-reader`, `lance/m3/store-recovery`, `camden/m2/design-doc`.
- **A branch has exactly one purpose.** If you discover unrelated work mid-branch, you do
  not fold it in. Note it, finish the branch, open a separate one.
- Branch lifetime: one session's work, three days maximum. A long-lived branch is a
  review you will never do.
- Branch from current `main`, freshly pulled. Never branch from another open branch
  without saying so in the PR.

## 7.3 Commit granularity — one commit, one responsibility

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
  together when they are genuinely one change (see §7.7).

**The check:** `git show <commit>` should answer exactly one question a human asked. If it
answers two, split. If it answers half of one, you committed too early.

## 7.4 Commit message format

```
M<n> <area>: <imperative summary, ≤ 72 chars>

WHAT:   One sentence. What changed, mechanically.
WHY:    1–3 sentences. The reason, in terms of a requirement, a decision ID,
        or a measurement. Never "to improve things."
REVIEW: What the reviewer should check hardest, and what I am least sure about.
VERIFY: The exact commands run and their result.
```

`<area>` is one of: `can`, `ipc`, `store`, `analysis`, `supervisor`,
`interface`, `common`, `build`, `tests`, `tools`, `soak`, `docs`.

Real example:

```
M3 ipc: refuse push on full ring instead of overwriting

WHAT:   ring_push now returns false and increments an overrun counter when
        head - tail == RING_CAPACITY, rather than wrapping over slot[tail].
WHY:    DECISIONS.md B.3 — silent overwrite is the exact failure mechanism F
        exists to prevent; a gap in the record is a gap in the diagnosis.
REVIEW: The memory ordering on the tail load. I used acquire so the producer
        cannot see a stale tail and wrongly declare the ring full; I am least
        sure whether relaxed would be sufficient here.
VERIFY: make && make test && make asan — all green, test_ring 3/3.
```

**Banned messages**, non-negotiable: `wip`, `fix`, `fixes`, `updates`, `misc`, `cleanup`,
`changes`, `more work`, `address feedback`, or any message that does not say what changed.

## 7.5 Pull requests

- **One PR = one reviewable claim.** Title matches the lead commit: `M<n> <area>: <what>`.
- **A PR is sized by its claim, not its diff.** If the title needs "and", split it. If a
  reviewer would have to approve part and object to part, split it. A PR that implements
  one coherent thing is the right size even when that thing was large — but say so in the
  body, and lay out the commit-by-commit list so the reviewer has a path through it.
- **Past ~10 commits or ~600 changed lines, re-apply the test rather than obey a number.**
  Most PRs that big contain two claims; the ones that genuinely do not are fine.
- **Open the PR as a draft at the first commit**, not at the end — so the partner can watch
  it grow and object early instead of at the finish line.
- A PR touching the **other partner's** owned area is titled `[cross]` and needs that
  partner's review specifically, not either partner's.
- The description uses `.github/PULL_REQUEST_TEMPLATE.md`, **every section filled**. A PR
  with an unfilled section is not ready for review, and Claude should not mark it ready.
- The description must contain **verbatim pasted output** of `make`, `make test`, and
  `make asan`. Not a claim that they passed. The output.
- The description must name the decision IDs the PR touches, and must contain a
  **"What I am least sure about"** paragraph that is not empty. If everything is certain,
  you have not thought hard enough about the diff.
- If a PR needs a diagram, an annotated log excerpt, or a timing capture to be
  understandable — put it in the PR body. Making the reviewer reconstruct context is how
  reviews stop happening.

## 7.6 The merge gate

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

## 7.7 Session start and session end

**Start of session:**
1. Read `CLAUDE.md`, `docs/milestones.md`, `docs/DECISIONS.md`.
2. `python tools/board_sync.py --status` — pick work from **Ready**, nowhere else.
3. State whose session this is.
4. `git status` clean, `git pull` on `main`, create the branch.

**End of session — mandatory, every time:**
1. Nothing uncommitted. No stray files.
2. PR opened or updated, template filled, build/test/asan output pasted.
3. **Every decision made without a human present is appended to `docs/DECISIONS.md` as
   ⚠️ UNREVIEWED and listed in the Review Queue — in the same commit that implements it,
   never a later one.**
4. Print a session summary: branch name, each commit as one line, PR link, decisions added,
   and what specifically needs human review.

**Claude never marks a decision reviewed or LOCKED.** Only Camden or Lance does, by editing
the `Reviewed` field, in a commit authored by that human.

## 7.8 When to stop instead of commit

Stop and emit a structured blocker — the decision, the options, the cost of each, your
recommendation — rather than proceeding, when:

- the change carries two responsibilities and cannot be cleanly separated (§7.3)
- it requires a decision not already in `docs/DECISIONS.md`
- it would touch the other partner's owned area by more than a line
- it would weaken, skip, or delete a test
- `make`, `make test`, or `make asan` is not green
- you would have to guess at hardware behavior without evidence (see §8)

## 7.9 Volume and distribution

≥ 40 meaningful commits across the semester, **both partners well represented**. A history
where one partner authored under a quarter of the substantive commits is probed hard at the
individual defense. A pair of "final submission" commits is an automatic process-grade of
zero **for both partners**.

Commit *as you go*, inside the session. A session that produces one commit at the end has
already lost the granularity this section exists to protect.

## 7.10 Repository settings (humans, once)

On GitHub, set on `main`: require a pull request before merging · require 1 approval ·
dismiss stale approvals on new commits · require conversation resolution · block force
pushes · block deletions. Enable **squash-merge only if** the squash message is rewritten
to be meaningful — otherwise prefer merge commits, since our granular history *is* the
deliverable and squashing destroys it.

## 7.11 Issues and the project board — where work and questions live

**GitHub Issues replace the blockers list.** Anything that needs a human — a question, a
decision, a measurement only one of us can take, a part that must be ordered — is an
**issue**, not a paragraph buried in a doc or a session summary nobody re-reads.

`docs/DECISIONS.md` keeps only what has been **decided** (plus the review queue).
Everything still open lives on the board, where it has an owner, a milestone, and a
visible position in the queue.

### The board is generated, not hand-maintained

**`docs/board.toml` is the source of truth. GitHub is derived from it.**
`tools/board_sync.py` reconciles the two. Full contract in `docs/BOARD.md`; the part that
binds this section:

> **Never create, retitle, relabel, re-milestone or re-assign an issue with `gh` or in
> the web UI.** Edit the `[[item]]` and re-run sync. An issue created by hand is invisible
> to the manifest, drifts immediately, and is reported forever as unmanaged.

What you *may* do directly on GitHub: **comment**, **answer**, and **close**. Those are the
human parts, and sync respects all three — it never reopens what a human closed, and it
reports any issue closed on GitHub that `board.toml` still thinks is open, so the manifest
gets caught up in the next commit.

### Issue types

| Label | Meaning | Who closes it |
|---|---|---|
| `question` | Needs human judgment or knowledge Claude does not have | **Human only** |
| `decision` | A design choice that must be made before work proceeds | **Human only** |
| `task` | Work to be done; may be Claude's or a human's | PR merge, or owner |
| `bug` | Something is wrong | PR merge |
| `hardware` | Physical world: order it, wire it, measure it, drive it | **Human only** |
| `blocked` | Waiting on a dependency (applied and removed by sync) | n/a |

`board_sync.py` refuses to close a `question`, `decision` or `hardware` issue without the
explicit `--close-questions` flag, which is a human typing it. That is the enforcement, not
an honour system.

### Claude's standing instructions

- **When you would stop and emit a blocker (§7.8), add an `[[item]]` to `docs/board.toml`
  instead**, run sync, and continue with whatever else is unblocked. Link the issue number
  in the session summary.
- Title: `M<n> <area>: <what>` — same convention as commits.
- Body must state: what is blocked, what you already tried or know, the options with their
  costs, your recommendation, and **exactly what answer would unblock it**. A question a
  human can answer in one line beats a question that needs a meeting.
- Set `owner`. Unassigned issues are nobody's.
- Set `milestone` (`M0`–`M6`) so the board sorts by deadline.
- Give it a `slug` that will still make sense in Week 14. Slugs are permanent identity —
  **never rename or reuse one**; the slug→issue mapping in `docs/board.lock.json` is what
  survives between sessions.
- If the item exists because of a decision, set `decision = "D-###"`. Sync renders the link
  into the issue and warns if that ID is not in `docs/DECISIONS.md`.

### Dependencies — the gate

`blocked_by` in `board.toml` takes **slugs, never issue numbers**. Sync resolves them,
writes the `Blocked by: #12, #15` block into the body, and applies or removes the `blocked`
label as blockers open and close. It refuses to run on a dependency cycle or a dangling
slug, so a bad edit fails before it reaches GitHub.

The board column follows from that, automatically:

| Column | Means |
|---|---|
| **Backlog** | something it depends on is still open |
| **Ready** | nothing blocks it — this is the actionable list |
| **Blocked** | waiting on something that is *not* another item (a shipment, a reply) — set by hand |
| **In progress** / **In review** | set by hand while work is live |
| **Done** | the issue is closed |

**Do not start work on an issue that is not in Ready** — say so and pick up something that
is. Answering one question can move several cards into Ready on the next sync; that is the
board telling you what just became workable.

### Issue → branch → PR, as one chain

```sh
gh issue develop <n> --name camden/m3/obd-reader --checkout   # branch linked to issue
# ... commits per §7.3 ...
gh pr create --draft --title "M3 obd: Mode 01 reader on the adapter tty" --body-file <filled template>
```

The PR body must contain `Closes #<n>`. Merging then closes the issue; the next sync moves
the card to Done. **Every PR traces back to an issue**; if there is no issue, add the
`[[item]]` first — that is the record of *why* the work existed.

### Session end

Run `python tools/board_sync.py`. It is the only thing that moves cards, and it is cheap:
a run with nothing to do makes zero writes. **A board that lies is worse than no board**,
and the only way it can lie now is if you skipped this step.

CI enforces it: `.github/workflows/board.yml` runs `--check --no-board` on every push and
pull request and fails on drift, and `make test` validates `docs/board.toml` itself through
`tools/test_board.py`. So a forgotten sync is a red build, not a quiet lie.

### What this is worth at the defense

The board is not project-management theater. It is a timestamped record that the questions
were asked before the work, that a human answered them, and that the answer shaped the
code — which is precisely the process the transcripts are graded against, corroborated by
an independent system we did not write.

---

## 7.12 Hardware changes (KiCad)

The schematic lives in `electricalDrawing/` (KiCad 10, owned by Lance). KiCad files are
S-expression **text**, so hardware goes through the same branches, commits and pull requests
as the C — with two additions, both of which exist because a reviewer cannot read a diff of
coordinate tuples.

**Every PR touching `electricalDrawing/` must:**

1. **Include the regenerated schematic SVG** (`make hwdocs`, commit
   `docs/figures/electricalDrawing.svg`). A hardware PR without a picture is unreviewable,
   and §7.6 then blocks it — correctly.
2. **Show `make hwcheck` output in the body.** ERC violations are build failures, the same
   way `-Werror` findings are. Pasted output, not a claim.

**Commit granularity for schematics** follows the documentation rule in §7.3, not the code
rule: the unit is **one functional block or one resolved electrical question**. Wiring the
whole CAN block is one commit even if it moves fifty symbols. Renumbering references across
the sheet is a separate, labeled commit — never folded into a wiring change, for exactly the
reason formatting is never folded into logic.

**Never commit:** `.kicad_prl` (per-user state), `.history/`, autosave or backup files, or
anything under `make hwdocs`'s outputs except the schematic SVG. The `.gitignore` enforces
this.

**What Claude may not do here.** I cannot see the board. Symbol pin numbering, footprint
choice, placement, thermal, EMI and mechanical fit are all things I reason about blind, and
hand-writing `.kicad_pcb` geometry is forbidden — the coordinate math fails silently at fab.
I am useful for netlist review, datasheet cross-checks, design rules, BOM and export
scripting, and reading ERC JSON. **A measured voltage always beats my opinion**, and §8.4
applies with full force: no hardware conclusion from a verbal description or an unmeasured
module.

---

---

## 8. Workflow (non-git)

1. **Plan first.** Multi-file or algorithmic change: plan mode, wait for approval, no code
   until the plan is accepted.
2. **Explore before writing.** Read the existing code and the datasheet before proposing.
   Say what you read.
3. **Tests lead.** New behavior gets a test that fails first.
4. **Evidence over assertion — hardware clause (handout §10.2).** Never propose a hardware
   fix from a verbal description. Paste the artifact: `dmesg` verbatim, the raw adapter
   replies, the `vcgencmd get_throttled` reading, the timing histogram.
   Without evidence, your job is to say what to capture — not to guess.
5. **Adversarial review.** Every milestone's diff gets a fresh-context agent review *and* a
   human review by the partner who did not write it.
6. **Smallest diff that passes.** Do not refactor unrelated code. Do not "improve" things
   nobody asked about.
7. **Autonomous execution with declared blockers.** Run to completion on everything you
   can; stop with structured output at decisions that need human judgment.
8. **Credit discipline.** Minimal tool calls, no re-reads of files already in context, no
   re-derivation of settled results, batch independent work, smallest edits.

### Prompt quality bar (handout §10.4)

Useless: "make the sensor work." Effective: "`/dev/obd` opens and `ATZ` answers, but
`010C` returns `NO DATA` with the engine running. Here is our init sequence and the raw
replies [paste both]. Diff them against the ELM327 datasheet's protocol-selection steps and
identify the missing one; do not rewrite the reader."

---

## 9. Milestone hygiene — do not let this slip

**At every milestone M1–M5, each partner copies their raw `.jsonl` session files out of
`~/.claude/projects/` into `partners/<name>/transcripts/`.** Raw. Not summarized, not
exported, not retyped. Transcripts purge after 30 days by default and this project runs 15
weeks — losing Week-5 transcripts in Week 15 is a *foreseeable* loss and is not excused.

Each partner maintains `partners/<name>/PROMPTLOG.md` — 6–10 annotated episodes by M5,
including one revised plan, one rejected diff, one tool-output debugging loop with real
hardware evidence, and one review of the partner's work. **Write these as they happen.**

---

## 10. Deliverables checklist (handout §12)

- [ ] Source + `Makefile` + `README.md` that takes a TA from a clean Pi to running
- [ ] `docs/PROBLEM.md` (as revised), `docs/DESIGN.md` (as it ended + changelog of what M2 got
      wrong), `docs/EVALUATION.md`
- [ ] Raw unedited 48-hour soak logs including the injected fault
- [ ] `CLAUDE.md`, checked in, visibly evolving across milestones
- [ ] Per partner: `PROMPTLOG.md`, `REFLECTION.md`, raw `.jsonl` transcripts
- [ ] ≥ 40 meaningful commits spanning the milestones, both partners represented

---

## 11. Open blockers

**On the board, not in this file.** Open `question` and `decision` issues, in the Backlog
column of the `carwatch` project. `python tools/board_sync.py --status` lists them with
how long each has been open.
`docs/DECISIONS.md` records only what has been *decided*, plus the review queue. Nothing
open is tracked in two places, so nothing open can go stale in one of them.

---

*Last updated: M0 — §0, §7.11 and §11 rewritten when the board moved to
`docs/board.toml` + `tools/board_sync.py`. Both partners maintain this file.*
