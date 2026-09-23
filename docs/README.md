# docs/ — the index

The reference chain is **`CLAUDE.md` (rules) → this file (where) → the source doc
(what)**. Every fact has one source. If two documents disagree, the one named here as the
**source** wins and the other is stale: fix the stale one, never the source, to match.

## Find it fast

| Topic | Source | Also mentioned in (must agree) |
| --- | --- | --- |
| Rules for every session, git and PRs | [`../CLAUDE.md`](../CLAUDE.md) | — |
| Why those rules, worked examples, repo settings | [`PROCESS.md`](PROCESS.md) | — |
| What is decided, and what awaits a signature | [`DECISIONS.md`](DECISIONS.md) | everything that cites a `D-###` |
| Open work, questions, owners, blockers | [`BOARD/board.toml`](BOARD/board.toml) → GitHub | — |
| How the board works: labels, items, CI | [`BOARD/BOARD.md`](BOARD/BOARD.md) | — |
| Milestone dates | `[[milestone]]` blocks in `BOARD/board.toml` | [`milestones.md`](milestones.md) table (generated) |
| Milestone exit criteria and the M5 deliverables | [`milestones.md`](milestones.md) | — |
| What to cut first, critical paths | [`PLAN.md`](PLAN.md) | — |
| Ownership map | [`../CLAUDE.md`](../CLAUDE.md) §6 | `DESIGN.md` §7, `DECISIONS.md` §F.2 |
| Hardware data path (Bluetooth OBD2 adapter) | `DECISIONS.md` §A.1 (D-019) | README, DESIGN, `hardware/` |
| Commit message format | [`../.gitmessage`](../.gitmessage) | `CLAUDE.md` §7.4 |
| PR body | [`../.github/PULL_REQUEST_TEMPLATE.md`](../.github/PULL_REQUEST_TEMPLATE.md) | `CLAUDE.md` §7.5 |
| Markdown and spelling rules | [`../.markdownlint.jsonc`](../.markdownlint.jsonc), [`../cspell.json`](../cspell.json) | `DECISIONS.md` §F.4 (D-018) |
| Transcript copy-out commands | `../partners/<name>/transcripts/README.md` | `CLAUDE.md` §0, §9 |

## Graded deliverables

| Document | Answers | Owner | Due |
| --- | --- | --- | --- |
| [`PROBLEM.md`](PROBLEM.md) | Who is this for, what goes wrong without it, and why must it be a device? | Camden | M1 |
| [`DESIGN.md`](DESIGN.md) | How is it built, which mechanisms, how does it fail, what will we measure? | shared | M2, as-built at M5 |
| [`EVALUATION.md`](EVALUATION.md) | What did we measure, and what are the limits? | shared | M5 |
| [`../README.md`](../README.md) | What carwatch is, and how a TA gets it running on a clean Pi | shared | M5 |

## How the project is run

| Document | Answers | Kind |
| --- | --- | --- |
| [`DECISIONS.md`](DECISIONS.md) | What is settled, why, and what still needs a human signature? | **source** for decisions |
| [`BOARD/board.toml`](BOARD/board.toml) | What work, questions and decisions are open, owned by whom, blocked by what? | **source** for all open work; GitHub is derived |
| [`BOARD/BOARD.md`](BOARD/BOARD.md) | How does `board.toml` become the GitHub board? | how-to |
| [`BOARD/board.lock.json`](BOARD/board.lock.json) | Which issue number is each board slug? | **generated**, never hand-edit |
| [`milestones.md`](milestones.md) | What must be true to reach each milestone? | exit criteria; its date table is **generated** |
| [`PLAN.md`](PLAN.md) | Why is the plan shaped this way, and what gets cut first? | reasoning |
| [`PROCESS.md`](PROCESS.md) | Why are the git, PR and review rules shaped this way? | reasoning |
| [`handout/CS370-TermProject.md`](handout/CS370-TermProject.md) | What does the rubric require? | reference: the rubric is law |

## Hardware

| Document | Answers | Owner |
| --- | --- | --- |
| [`hardware/BOM.md`](hardware/BOM.md) | What do we buy? | Camden |
| [`hardware/wiring.md`](hardware/wiring.md) | How is it connected, and what has been measured? | Camden |
| [`hardware/pid-survey.md`](hardware/pid-survey.md) | Which signals does the CR-V actually publish? | Lance's car, either partner |
| [`hardware/schematic-conventions.md`](hardware/schematic-conventions.md) | How is the KiCad schematic in `electricalDrawing/` drawn? | Lance |

## Outside `docs/`

| Path | What |
| --- | --- |
| [`../CLAUDE.md`](../CLAUDE.md) | Team rules, read by both partners and by Claude every session |
| [`../tools/README.md`](../tools/README.md) | The Python tools, present and planned; `tools/train/README.md` for training |
| [`../soak/README.md`](../soak/README.md) | The 48-hour soak runner and fault injection |
| [`../partners/`](../partners/) | Each partner's PROMPTLOG, REFLECTION and raw `.jsonl` transcripts |
