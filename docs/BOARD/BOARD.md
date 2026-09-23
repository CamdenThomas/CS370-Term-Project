# The board: how work is tracked

Every task, question and decision in this project lives in **`docs/BOARD/board.toml`**.
GitHub issues and the `carwatch` project board are *derived* from that file by
`tools/board_sync.py`. Nothing is typed directly into the GitHub web UI except
answers, discussion, and closing an issue.

```text
docs/BOARD/board.toml   ──▶  tools/board_sync.py  ──▶  GitHub issues + project board
(source of truth)      (reconciler)              (derived)
docs/BOARD/board.lock.json   slug → issue number, committed so the mapping survives
```

## Running it

No bash, no Makefile — this runs anywhere Python 3.11+ and `gh` exist, including
PowerShell:

```powershell
python tools/board_sync.py --status     # what is Ready, what is stuck, who owns it
python tools/board_sync.py --check      # report drift; exits 1 if anything would change
python tools/board_sync.py              # reconcile
python tools/board_sync.py --only oilq  # reconcile one item
```

One-time setup per machine: `gh auth login`, then `gh auth refresh -s project`.

`--status` is the session-start and Monday-morning command. It prints the column counts,
the Ready list grouped by owner, **every critical-path item that is not Ready and the
specific open issues holding it**, and each open question or decision with its age in days.
It writes nothing and costs two API reads.

## What the reconciler guarantees

- **Idempotent.** Running it twice does nothing the second time. `--check` exits 1 when
  anything would change, which is what CI gates on.
- **Surgical.** Issues absent from `board.toml` are never edited, closed, or
  removed from the board. They are listed at the end as unmanaged.
- **Additive.** Adding a task or a question is one `[[item]]` block plus a re-run.
  Nothing unrelated is disturbed and nothing has to be closed to make room.
- **Non-destructive about closure.** GitHub wins on closure. If a human closes an
  issue in the web UI, sync moves the card to Done and reports that `board.toml`
  is behind — it never reopens.
- **Guarded.** Items labeled `question`, `decision` or `hardware` are never closed by the
  tool without the explicit `--close-questions` flag, per CLAUDE.md §7.11 — those close
  when a human answers.
- **Loud about orphans.** Deleting an `[[item]]` leaves a live issue behind. Sync retires
  the slug in the lock file and reports the issue as still open rather than forgetting it.
  To cut scope, mark the item `status = "done"` and say why in its body; don't delete it.

## What is generated

Three things derive from `board.toml` and must never be hand-edited:

| Derived thing | Where | Marker |
| --- | --- | --- |
| Issue bodies' blocked-by list and decision link | GitHub | `<!-- board-sync:deps -->` |
| The milestone table | `docs/milestones.md` | `<!-- board-sync:milestones -->` |
| slug → issue number | `docs/BOARD/board.lock.json` | (whole file) |

**Milestone dates are written in exactly one place**: the `[[milestone]]` blocks here.
Sync patches the GitHub due dates *and* regenerates the table in `docs/milestones.md`
between its markers, including a progress column counting closed issues per milestone.
When D-008 resolves, that is one edit in `board.toml` and one sync — the three cannot
disagree. Every word of exit criteria below that table is hand-written and is left alone.

## Who owns what text

`board.toml` owns the issue title and body. The tool rewrites them on GitHub, so
**edit the prose here, not in the web UI** — a body edited on GitHub is
overwritten on the next sync. The single exception is the region between the
`<!-- board-sync:deps -->` markers, which the tool computes from `blocked_by`:
it renders the `Blocked by: #12, #15` line and adds or removes the `blocked`
label as blockers open and close.

## The loop

1. A block sits in the **Backlog** column, labeled `question` or `decision`.
   Claude may never close those on its own.
2. Camden and Lance answer it — in the issue thread, or in a Claude Code session.
3. Claude factors the answer through the repo: `docs/DECISIONS.md` gets the
   decision, `docs/PLAN.md` and the affected docs get the consequence, and the
   one or two `[[item]]` entries the answer actually changed get updated —
   new tasks added, dead ones marked `status = "done"`, dependencies rewired.
4. `python tools/board_sync.py` pushes exactly that delta.
5. One commit contains the answer, its consequences, and the board move.

## Labels, and who closes what

| Label | Meaning | Who closes it |
| --- | --- | --- |
| `question` | Needs human judgment or knowledge Claude does not have | **Human only** |
| `decision` | A design choice that must be made before work proceeds | **Human only** |
| `task` | Work to be done; may be Claude's or a human's | PR merge, or owner |
| `bug` | Something is wrong | PR merge |
| `hardware` | Physical world: order it, wire it, measure it, drive it | **Human only** |
| `blocked` | Waiting on a dependency (applied and removed by sync) | n/a |

`board_sync.py` refuses to close a `question`, `decision` or `hardware` issue without the
explicit `--close-questions` flag, which is a human typing it. That is the enforcement, not
an honor system.

## Writing an item

When Claude would otherwise stop and emit a blocker (CLAUDE.md §7.8), it adds an
`[[item]]` instead, runs sync, and carries on with whatever else is unblocked:

- Title: `M<n> <area>: <what>` — same convention as commits.
- Body must state: what is blocked, what you already tried or know, the options with their
  costs, your recommendation, and **exactly what answer would unblock it**. A question a
  human can answer in one line beats a question that needs a meeting.
- Set `owner` (unassigned issues are nobody's) and `milestone` (`M0`–`M6`, so the board
  sorts by deadline).
- Give it a `slug` that will still make sense in Week 14. Slugs are permanent identity —
  **never rename or reuse one**; the slug→issue mapping in `docs/BOARD/board.lock.json` is
  what survives between sessions.
- If the item exists because of a decision, set `decision = "D-###"`.
- Every PR body contains `Closes #<n>`, so every PR traces back to an item. No item yet?
  Add it first — it is the record of *why* the work existed. Handy chain:
  `gh issue develop <n> --name <owner>/<mN>/<slug> --checkout`.

## Editing `board.toml`

```toml
[[item]]
slug = "oilpressure-sender"          # permanent identity. Never reuse, never renumber.
milestone = "M2"
labels = ["hardware", "critical-path"]
owner = "camden"                     # key into [people]; omit for unassigned
blocked_by = ["oilq"]                # slugs, never issue numbers
decision = "D-009"                   # optional; must exist in docs/DECISIONS.md
status = "ready"                     # optional; omit and sync infers backlog/ready
title = "M2 hardware: fit an analog oil-pressure sender"
body = '''
Prose. Markdown. Owned by this file.
'''
```

`status` values: `backlog`, `blocked`, `ready`, `in-progress`, `in-review`, `done`.
Omit it and the tool picks the column: **Backlog** while any blocker is still
open, **Ready** the moment nothing blocks it, **Done** once the issue is closed.
So answering one question can move several cards into Ready on the next sync —
that is the board telling you what became workable.

Set `status` by hand to override. `blocked` is reserved for waiting on something
that is *not* another item: a shipment, a reply from the professor, Lance.

`decision` renders a link line into the managed region of the issue. Any `D-###` named
in a body or a `decision` field that is *not* in `docs/DECISIONS.md` is reported at the
end of every run, so a typo'd ID cannot sit in an issue pointing at nothing.

## What stops a bad edit

The tool refuses to run on a manifest with a duplicate slug, an unknown `owner`, an
undeclared milestone or label, a malformed `decision`, an empty body, a `blocked_by`
pointing at a slug that does not exist, or a dependency cycle — so a malformed edit fails
before it can touch GitHub.

`tools/test_board.py` checks the rest offline and runs inside `make test`, because
`board.toml` is a graded artifact and deserves the same gate as the C: title convention,
slug format, exactly one kind label, no hand-set `blocked` label, no duplicate
`blocked_by`, milestone field completeness, and that **at least one open item is actually
startable** — a dependency edit that walls off the whole board is otherwise invisible.

## CI

`.github/workflows/board.yml`:

- **manifest** — `python tools/test_board.py`, offline, on every push and PR.
- **drift** — `board_sync.py --check --no-board`, fails if GitHub no longer matches
  `board.toml`. This is the one that catches a forgotten sync. `--no-board` keeps it inside
  `GITHUB_TOKEN`'s reach; user-level Projects would need a PAT, and the board follows from
  the issues anyway.
- **pr-closes-issue** — every PR body must contain `Closes #<n>` per CLAUDE.md §7.11.
  Currently `continue-on-error: true` as a soft launch; **remove that line on 2026-09-26**
  once the habit is set.

## Where this came from

`tools/bootstrap_board.sh` was a one-shot generator: it created everything or nothing,
refused to run a second time, never touched the project board, and swallowed its own
errors. It was deleted; git history has it, and the plan text it carried is now the
`[[item]]` blocks in `board.toml`. The full diagnosis is PROMPTLOG episode E-01.
