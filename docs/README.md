# docs/ — the index

The reference chain is **`CLAUDE.md` (rules) → this file (where) → the source doc
(what)**. Every fact has one source. If two documents disagree, the one named here as the
**source** wins and the other is stale: fix the stale one, never the source, to match.

`D-###` labels throughout the repository name entries of the old decision log, removed on
2026-10-28; `git show 8c09b3d:docs/DECISIONS.md` shows it as it last stood, D-046 being its
last entry.

## Find it fast

| Topic | Source | Also mentioned in (must agree) |
| --- | --- | --- |
| Rules for every session, git and PRs | [`../CLAUDE.md`](../CLAUDE.md) | — |
| Why those rules, worked examples, repo settings | [`PROCESS.md`](PROCESS.md) | — |
| Open work, questions, owners | [`milestones.md`](milestones.md) checkboxes → GitHub | — |
| How the board works: a line, sync, CI | [`BOARD/BOARD.md`](BOARD/BOARD.md) | — |
| Milestone dates | the table in [`milestones.md`](milestones.md) | GitHub milestone due dates (synced) |
| Milestone exit criteria and the M5 deliverables | [`milestones.md`](milestones.md) (bold lines) | — |
| What to cut first, critical paths | [`PLAN.md`](PLAN.md) | — |
| Ownership rule (D-026) and the realized map | [`../CLAUDE.md`](../CLAUDE.md) §6, `DESIGN.md` §9 | — |
| Hardware data path (Bluetooth OBD2 adapter) | [`hardware/wiring.md`](hardware/wiring.md) | README, DESIGN §1 |
| The box: power and shutdown, light, clock | [`hardware/wiring.md`](hardware/wiring.md) | README, DESIGN §6 |
| Testbed cars | [`hardware/pid-survey.md`](hardware/pid-survey.md) | README, PROBLEM |
| The box's own sensor (DS18B20 probe) | [`hardware/wiring.md`](hardware/wiring.md) | README, PROBLEM, DESIGN |
| Processes, rings and sockets | [`DESIGN.md`](DESIGN.md) §1 | README query interface |
| On-disk format, fsync, retention | [`DESIGN.md`](DESIGN.md) §5 | — |
| The 48-hour soak's definition | [`../soak/README.md`](../soak/README.md) | DESIGN §7 |
| Evaluation targets | [`DESIGN.md`](DESIGN.md) §8 | `EVALUATION.md` |
| Commit message format | [`../.gitmessage`](../.gitmessage) | `CLAUDE.md` §7.4 |
| PR body | [`../.github/PULL_REQUEST_TEMPLATE.md`](../.github/PULL_REQUEST_TEMPLATE.md) | `CLAUDE.md` §7.5 |
| Markdown and spelling rules | [`../.markdownlint.jsonc`](../.markdownlint.jsonc), [`../cspell.json`](../cspell.json) | — |
| Transcript copy-out commands | `../partners/<name>/transcripts/README.md` | `CLAUDE.md` §0, §9 |

## Graded deliverables

| Document | Answers | First author | Due |
| --- | --- | --- | --- |
| [`PROBLEM.md`](PROBLEM.md) | Who is this for, what goes wrong without it, and why must it be a device? | Camden; Lance wrote its header and risk (c94da22) | M1 |
| [`DESIGN.md`](DESIGN.md) | How is it built, which mechanisms, how does it fail, what will we measure? | Camden | M2, as-built at M5 |
| [`EVALUATION.md`](EVALUATION.md) | What did we measure, and what are the limits? | Camden (skeleton) | M5 |
| [`../README.md`](../README.md) | What carwatch is, and how a TA gets it running on a clean Pi | Camden | M5 |

No document has an owner (D-026); "first author" is history, not an assignment.

## How the project is run

| Document | Answers | Kind |
| --- | --- | --- |
| [`BOARD/BOARD.md`](BOARD/BOARD.md) | How does the checklist become the GitHub board? | how-to |
| [`milestones.md`](milestones.md) | What is open, what is done, and what must be true to reach each milestone? | **source** for all work; GitHub is derived |
| [`PLAN.md`](PLAN.md) | Why is the plan shaped this way, and what gets cut first? | reasoning |
| [`PROCESS.md`](PROCESS.md) | Why are the git, PR and review rules shaped this way? | reasoning |
| [`handout/CS370-TermProject.md`](handout/CS370-TermProject.md) | What does the rubric require? | reference: the rubric is law |

## Hardware

| Document | Answers |
| --- | --- |
| [`hardware/BOM.md`](hardware/BOM.md) | What do we buy, and what is bought? |
| [`hardware/wiring.md`](hardware/wiring.md) | How is it connected, and what has been measured? |
| [`hardware/pid-survey.md`](hardware/pid-survey.md) | Which signals does each testbed car actually publish? |
| [`hardware/first-drive.md`](hardware/first-drive.md) | How do we survey a car and log its first drive (D-033)? |
| [`hardware/schematic-conventions.md`](hardware/schematic-conventions.md) | How is the KiCad schematic in `electricalDrawing/` drawn? |

## Outside `docs/`

| Path | What |
| --- | --- |
| [`../CLAUDE.md`](../CLAUDE.md) | Team rules, read by both partners and by Claude every session |
| [`../tools/README.md`](../tools/README.md) | The Python tools, present and planned; `tools/train/README.md` for training |
| [`../soak/README.md`](../soak/README.md) | The 48-hour soak runner and fault injection |
| [`../partners/`](../partners/) | Each partner's PROMPTLOG, REFLECTION and raw `.jsonl` transcripts |
