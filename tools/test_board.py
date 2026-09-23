#!/usr/bin/env python3
"""
test_board.py — validate docs/BOARD/board.toml without touching the network.

docs/BOARD/board.toml is a graded artifact: it is where every task, question and
decision in this project is written down, and a bad hand-edit to it is the
failure mode most likely to happen at 1am in Week 11. So it is checked by the
same gate as the C code — `make test` runs this.

Everything here is offline and deterministic. Network behavior (does GitHub
actually match the manifest?) is `board_sync.py --check`, which CI runs
separately.

    python tools/test_board.py

Exits 0 with a PASS line, or 1 with one line per failure.
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import board_sync as B  # noqa: E402


# `M<n> <area>: <what>`. The area is free-form here rather than the closed list
# CLAUDE.md 7.4 fixes for commit subjects — issues legitimately cover ground the
# commit areas do not ("hardware", "ground truth", "admin", "process").
TITLE_RE = re.compile(r"^M[0-6] [^:]{2,24}: \S")
SLUG_RE = re.compile(r"^[a-z][a-z0-9-]{1,30}$")


def main() -> int:
    fails: list[str] = []

    def check(cond: bool, msg: str) -> None:
        if not cond:
            fails.append(msg)

    # load_manifest already raises Fail on: a missing slug, a duplicate slug, an
    # unknown owner, an undeclared milestone or label, a malformed decision ID,
    # an empty body, a dangling blocked_by, self-blocking, and dependency cycles.
    # Anything it raises is a hard stop — there is nothing else worth checking.
    try:
        manifest = B.load_manifest()
    except B.Fail as e:
        print(f"board.toml                   FAIL\n  {e}")
        return 1

    items, order = manifest["_items"], manifest["_order"]
    check(bool(items), "manifest has no [[item]] blocks")

    for slug in order:
        it = items[slug]
        check(bool(SLUG_RE.match(slug)),
              f"{slug}: slug must be lower-case letters, digits and dashes")
        check(bool(TITLE_RE.match(it["title"])),
              f"{slug}: title must read 'M<n> <area>: <what>' (CLAUDE.md 7.4) — "
              f"got {it['title']!r}")
        check(it["milestone"] is not None, f"{slug}: no milestone")
        kinds = {"task", "question", "decision", "bug", "hardware"} & set(it["labels"])
        check(len(kinds) >= 1,
              f"{slug}: needs one kind label (task/question/decision/bug/hardware)")
        check("blocked" not in it["labels"],
              f"{slug}: do not set the `blocked` label by hand — sync owns it")
        check(len(it["blocked_by"]) == len(set(it["blocked_by"])),
              f"{slug}: duplicate entry in blocked_by")

    # A board where nothing is actionable is a board nobody can work from. This
    # is the check that would have caught a dependency edit that walled off the
    # whole M0 column.
    roots = [s for s in order if not items[s]["blocked_by"]
             and items[s]["status"] != "done"]
    check(bool(roots), "every open item is blocked by another — nothing is startable")

    # Milestones must be declared in order and carry the fields the generated
    # table in docs/milestones.md renders.
    names = [ms["name"] for ms in manifest.get("milestone", [])]
    check(names == sorted(names), f"[[milestone]] blocks out of order: {names}")
    for ms in manifest.get("milestone", []):
        for field in ("due", "week", "hours", "deliverable"):
            check(field in ms, f"milestone {ms['name']}: missing `{field}`")
        check(bool(re.fullmatch(r"\d{4}-\d{2}-\d{2}", ms.get("due", ""))),
              f"milestone {ms['name']}: due must be YYYY-MM-DD")

    # Unknown decision IDs are a warning in sync (DECISIONS.md may lag an item),
    # but a typo caught here is a typo that never reaches an issue body.
    for warn in manifest["_decisions"]:
        print(f"  warn: decision ID not in docs/DECISIONS.md — {warn}")

    if fails:
        print(f"board.toml                   FAIL ({len(fails)})")
        for f in fails:
            print(f"  {f}")
        return 1

    print(f"board.toml                   PASS ({len(items)} items, "
          f"{len(manifest.get('milestone', []))} milestones)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
