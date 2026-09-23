# The board: tick a box, run sync

**`docs/milestones.md` is the board** (D-020). Every checkbox under a `## M<n>` heading
is one GitHub issue. There is no other place work is written down.

```sh
python tools/board_sync.py              # make GitHub match the list
python tools/board_sync.py --status     # open boxes per milestone, by owner
python tools/board_sync.py --check      # what would change; exit 1 if anything would
```

One-time setup per machine: `gh auth login`, then `gh auth refresh -s project` so sync can
move cards on the `carwatch` project board.

## A line

```markdown
- [ ] M2 hardware: first live capture from a real car `firstcap` @camden <!-- #20 -->
```

| Part | Meaning |
| --- | --- |
| `[ ]` / `[x]` | open / done. Tick it, sync, and the issue closes and its card moves to Done. |
| the text | the issue title. `M<n>:` is added on GitHub if the text does not start with it. |
| `` `firstcap` `` | optional short name other docs cite (PLAN.md, DECISIONS.md). |
| `@camden`, `@lance` | optional assignee. |
| `<!-- #20 -->` | the issue number, hidden when rendered. **Sync writes it; never type one.** |

**Bold** lines are the milestone's exit criteria; the plain lines under them are the tasks
that get there. Long lines wrap onto continuation lines indented six spaces.

## What sync does

- **New line, no tag:** creates the issue in that milestone and writes the tag back.
- **Ticked box, open issue:** closes it.
- **Issue closed on GitHub, box not ticked:** ticks the box here. Sync never reopens.
- **Wording, milestone or `@owner` changed:** retitles, re-milestones or assigns.
- **Table's Due column:** sets the GitHub milestone due dates; Progress is recounted.
- **Never touches** issue bodies or labels (GitHub owns them), and never closes an issue
  that has disappeared from the list: it reports it instead.

To cut a task, tick it and add "(cut)" to its text, so the record says it was dropped on
purpose. Only a human ticks a question, decision or hardware line (CLAUDE.md §7.11).

## Checks

- `make test` runs `tools/test_board.py`: parser tests, then the real list must parse,
  every tag must be unique, and every milestone needs a due date and at least one box.
- CI (`.github/workflows/board.yml`) runs the same test, plus `board_sync.py --check
  --no-board` to catch a forgotten sync, plus a soft check that every PR body says
  `Closes #<n>`.

## History

Until 2026-09-23 the board was 99 `[[item]]` blocks in `docs/BOARD/board.toml`, with
bodies, dependencies and a lock file, reconciled by a 940-line tool. D-020 replaced it
with this list; every one of those issues is a line in `docs/milestones.md`, and the old
files are in git history.
