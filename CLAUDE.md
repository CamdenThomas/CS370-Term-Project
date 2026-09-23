# CS370 Term Project — Team Rules

**Project:** `carwatch`, an always-on in-vehicle diagnostic recorder that learns *this*
car's normal and reports developing faults before a check-engine light would.
**Team:** Camden Thomas (`crash8750@gmail.com`) · Lance Baron · CS 370, Colorado State
(Pallickara). **Rubric:** `docs/handout/CS370-TermProject.md`; the rubric is law, and this
file is how we obey it.

> **Read this file every session.** It holds the rules only, short enough to load every
> time. Everything else has exactly one home: **`docs/README.md`** maps each topic to its
> source doc. Rationale for the rules below is in `docs/PROCESS.md`.

## 0. Every session

You (Claude) are required in our **workflow** and forbidden in our **product** (handout
§3.3, §10.3).

**Start:**

1. Read `docs/milestones.md` (where we are), then the review queue at the top of
   `docs/DECISIONS.md` (what is settled, what awaits review).
2. `python tools/board_sync.py --status`. Pick work from the open boxes in
   `docs/milestones.md`, nowhere else.
3. State whose session this is. Do not edit outside that owner's area (§6) without being
   told.
4. `git status` clean, `git pull` on `main`, create the branch **before** the first edit.

**End, every time:**

1. Nothing uncommitted, no stray files.
2. PR opened or updated as a draft, template filled, `make` / `make test` / `make asan`
   output pasted.
3. Every decision made without a human present is in `docs/DECISIONS.md` as ⚠️ UNREVIEWED
   and in its review queue, **in the same commit that implements it**.
4. `python tools/board_sync.py`, the only thing that moves cards.
5. Copy this session's `.jsonl` out (`partners/<name>/transcripts/README.md`).
6. Print a summary: branch, one line per commit, PR link, decisions added, and what
   specifically needs human review.

Never reopen a 🔒 LOCKED decision without saying so and getting a human yes. **Claude
never marks a decision reviewed or LOCKED**; only Camden or Lance does, in their own commit.

## 1. Commands

```sh
make              # build everything, -Wall -Wextra -Werror clean
make test         # unit tests + boundary check + milestones.md check
make asan         # tests under AddressSanitizer + UBSan
make memcheck     # valgrind over the test binaries
make soakcheck    # 1-hour miniature of the 48h run
make hwcheck      # KiCad ERC; violations fail the build
make hwdocs       # regenerate schematic SVG, BOM and netlist
make replay FIX=fixtures/<name>   # a labeled capture through the real pipeline
make deploy PI=pi@<host>          # rsync + remote build
python tools/board_sync.py --status | --check | (no flag = reconcile)
```

**A change is DONE only when `make`, `make test` and `make asan` all pass and the output
is pasted.** Not "should pass." Output.

## 2. Hard constraints (graded boundaries)

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
7. **Replayed or synthesized data is labeled as such EVERYWHERE**: every record carries
   `src=live|replay|synth`. Integrity boundary, not a style choice.
8. **Never weaken, skip, or delete a test to make the suite pass.** If a test is wrong,
   say so and fix it deliberately, in its own commit, with the reason.

## 3. What we are building

A Raspberry Pi in Lance's **2015 Honda CR-V EX-L**, the only testbed (D-016). A Bluetooth
Classic OBD2 adapter in the OBD2 port is read over RFCOMM as `/dev/obd` (D-019); the car's
USB powers the Pi, nothing cut (D-011). Mode 01 requests (D-012) read every engine sensor
the car publishes; the device learns *this car's* normal per operating point, flags what
leaves it, and names the likely fault area. A rough first draft, not a finished product.
The owner reads it on a phone over the device's own Wi-Fi (D-013) or with `obdctl`; one
warning light blinks while on duty (D-014).

**The claim we defend (D-021):** *"learns this CR-V's normal across its engine sensors,
flags behavior outside it and names the likely fault area, with measured catch and
false-alarm rates on faults we induce."* Not "predicts failure" (handout §5).

## 4. Mechanisms

Committed and measured: **D** (append-only, crash-consistent storage) and **E**
(multi-process with a supervisor). **B** and **F** only if `pisensor` adds a Pi-side
sensor; **A**, **C** and raw CAN are stretch. Why: DECISIONS §B.1 (D-017).

## 5. The intelligence

All of it ours: operating-point binning → per-bin baselines → Mahalanobis residuals →
Theil–Sen trends → our classifier → state machine with hysteresis. Every diagnostic needs
two cooperating sensors (the threshold test). Why: DECISIONS §D.

## 6. Ownership

| Owner | Subsystems |
| --- | --- |
| **Camden** | `src/obd/`, `src/ipc/` |
| **Lance** | `src/store/`, `src/analysis/`, `tools/train/`, `electricalDrawing/` |
| **Shared** | `src/supervisor/`, `src/interface/`, `src/common/`, `Makefile`, docs |

First authorship and answerability at the defense, not exclusivity.

## 7. Git and pull-request law

Why the history is graded, and worked examples: `docs/PROCESS.md`.

### 7.1 The five absolutes

Claude may **never**, short of an explicit human override in the current session:

1. **Commit to `main`.** Every change reaches `main` through a pull request.
2. **Merge, approve, or close a pull request.** Only a human merges. Ever.
3. **`push --force`, force-with-lease, rebase a pushed branch, amend a pushed commit, or
   `reset --hard` anything that has been pushed.** History that reviewers have read is
   immutable.
4. **`git add -A`, `git add .`, or `git commit -a`.** Stage explicit paths, always.
5. **Commit a file whose full current contents were not read this session.**

Violating any of these is a stop-work event: say so, and do not attempt a workaround.

### 7.2 Branches

`<owner>/<milestone>/<slug>` (e.g. `camden/m3/obd-reader`), one purpose each, one
session's work, three days at most. Unrelated work found mid-branch gets its own branch.
Branch from freshly pulled `main`; branching from another open branch is said in the PR.

### 7.3 Commits

One commit, one responsibility: the reviewer can approve it as **one yes-or-no decision**,
and its purpose fits one sentence without "and". Tests ship in the same commit as the
behavior. Refactors, formatting, renames and moves are separate, labeled commits. Two
subsystems, two commits. Docs: one resolved question per commit, never content mixed with
reflow. **Every commit builds and passes tests on its own.** Past ~400 lines, re-apply
the test and say in `WHY` why it is indivisible. Details: `docs/PROCESS.md`.

### 7.4 Commit messages

Format: `.gitmessage` (`M<n> <area>: <imperative summary>`, then `WHAT` / `WHY` /
`REVIEW` / `VERIFY`). `<area>` is one of `obd`, `ipc`, `store`, `analysis`, `supervisor`,
`interface`, `common`, `build`, `tests`, `tools`, `soak`, `docs`, `process`. **Banned:**
`wip`, `fix`, `fixes`, `updates`, `misc`, `cleanup`, `changes`, `more work`,
`address feedback`, or anything that does not say what changed.

### 7.5 Pull requests

One PR, one reviewable claim, titled like its lead commit; if the title needs "and",
split. Open it **as a draft at the first commit**. A PR touching the other partner's area
is titled `[cross]` and needs *their* review. Body: `.github/PULL_REQUEST_TEMPLATE.md`,
every section filled, `Closes #<n>`, the decision IDs touched, a non-empty "What I am least
sure about", and **verbatim** `make` / `make test` / `make asan` output. Claude never marks
a PR ready with a section unfilled.

### 7.6 The merge gate

**If the reviewer does not understand a line, the PR does not merge.** Remedy: explain it
in a PR comment, split the commit, or rewrite it simpler; never "trust it". The question
set every reviewer must answer is in the PR template and `docs/PROCESS.md`.

### 7.7 Decisions

A decision and the code that enacts it share a commit. Session start and end are §0.

### 7.8 When to stop

Do not commit when the change carries two inseparable responsibilities, needs a decision
not in `docs/DECISIONS.md`, touches the other partner's area by more than a line, weakens
a test, has `make` / `make test` / `make asan` red, or guesses at hardware without
evidence (§8.4). **Add an open box to `docs/milestones.md` instead** (§7.11), run sync,
and continue with whatever is unblocked.

### 7.9 Volume

≥ 40 meaningful commits across the semester, both partners well represented. Commit as
you go.

### 7.10 Repository settings

Set once by humans; the list and the configured state are in `docs/PROCESS.md`.

### 7.11 The board

**`docs/milestones.md` is the board** (D-020): one checkbox, one issue. Never create,
retitle, re-milestone or re-assign an issue with `gh` or the web UI: edit the line and run
`python tools/board_sync.py`. Never type or change a `<!-- #n -->` tag; sync owns it.
Humans may comment and close on GitHub. **Claude never ticks a question, decision or
hardware line**; ticking is the human act. Details: `docs/BOARD/BOARD.md`.

### 7.12 Hardware (KiCad)

Every PR touching `electricalDrawing/` includes the regenerated schematic SVG
(`make hwdocs`) and pasted `make hwcheck` output. Never commit `.kicad_prl`, `.history/`,
autosave or backup files. Claude never hand-writes `.kicad_pcb` geometry. Schematic rules:
`docs/hardware/schematic-conventions.md`.

## 8. Workflow

1. **Plan first.** Multi-file or algorithmic change: plan mode, wait for approval.
2. **Explore before writing.** Read the code and the datasheet first; say what you read.
3. **Tests lead.** New behavior gets a test that fails first.
4. **Evidence over assertion (handout §10.2).** Never propose a hardware fix from a verbal
   description. Paste the artifact: `dmesg` verbatim, the raw adapter replies, the
   `vcgencmd get_throttled` reading, the timing histogram. Without evidence, your job is
   to say what to capture, not to guess. A measured voltage beats any opinion.
5. **Adversarial review.** Every milestone's diff gets a fresh-context agent review *and*
   a human review by the partner who did not write it.
6. **Smallest diff that passes.** Do not refactor or "improve" what nobody asked about.
7. **Autonomous execution with declared blockers.** Run to completion on everything you
   can; stop with a board item at decisions that need human judgment.
8. **Credit discipline.** Minimal tool calls, no re-reads of files already in context, no
   re-derivation of settled results, batch independent work, smallest edits.
9. **Lint-clean writing.** Every `.md` passes markdownlint (`.markdownlint.jsonc`) and
   every file passes cSpell en-US (`cspell.json`), D-018.

## 9. Transcripts and prompt log

Copy-out every session (§0), commands in `partners/<name>/transcripts/README.md`. Raw,
never edited. `partners/<name>/PROMPTLOG.md`: write episodes as they happen.

## 10. Deliverables

Handout §12; the checklist is `docs/milestones.md` M5.

## 11. Open questions

On the board, never in a doc: `python tools/board_sync.py --status`.
