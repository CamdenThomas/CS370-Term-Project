# docs/ — the index

Every document in this project, what question it answers, and who keeps it true. If a
fact appears in two documents, the one marked **source** wins and the other is stale.

## Graded deliverables

| Document | Answers | Owner | Due |
|---|---|---|---|
| [`PROBLEM.md`](PROBLEM.md) | Who is this for, what goes wrong without it, and why must it be a device? | Camden | M1 |
| [`DESIGN.md`](DESIGN.md) | How is it built, which mechanisms, how does it fail, what will we measure? | shared | M2, as-built at M5 |
| [`EVALUATION.md`](EVALUATION.md) | What did we measure, and what are the limits? | shared | M5 |

## How the project is run

| Document | Answers | Kind |
|---|---|---|
| [`DECISIONS.md`](DECISIONS.md) | What is settled, why, and what still needs a human signature? | **source** for decisions |
| [`board.toml`](board.toml) | What work, questions and decisions are open, owned by whom, blocked by what? | **source** for all open work; GitHub is derived |
| [`BOARD.md`](BOARD.md) | How does `board.toml` become the GitHub board? | how-to |
| [`milestones.md`](milestones.md) | What must be true to reach each milestone? | exit criteria; its date table is **generated** |
| [`PLAN.md`](PLAN.md) | Why is the plan shaped this way, and what gets cut first? | reasoning |
| [`board.lock.json`](board.lock.json) | Which issue number is each board slug? | **generated** — never hand-edit |
| [`handout/CS370-TermProject.md`](handout/CS370-TermProject.md) | What does the rubric require? | reference — the rubric is law |

## Hardware

| Document | Answers | Owner |
|---|---|---|
| [`hardware/BOM.md`](hardware/BOM.md) | What do we buy? | Camden |
| [`hardware/wiring.md`](hardware/wiring.md) | How is it connected, and what has been measured? | Camden |
| [`hardware/pid-survey.md`](hardware/pid-survey.md) | Which signals does the CR-V actually publish? | Lance's car, either partner |
| [`hardware/schematic-conventions.md`](hardware/schematic-conventions.md) | How is the KiCad schematic in `electricalDrawing/` drawn? | Lance |

## Outside `docs/`

| Path | What |
|---|---|
| [`../CLAUDE.md`](../CLAUDE.md) | Team rules, read by both partners and by Claude every session |
| [`../README.md`](../README.md) | What carwatch is, and how a TA gets it running on a clean Pi |
| [`../partners/`](../partners/) | Each partner's PROMPTLOG, REFLECTION and raw `.jsonl` transcripts |
