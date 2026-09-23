#!/usr/bin/env python3
"""
test_board.py — validate docs/milestones.md, the board (D-020), without the network.

Two parts: parser tests on small fixture strings, so a change to board_sync.py
cannot silently misread a line; then the real file, which must parse, carry
unique issue tags, and give every milestone a valid due date. `make test` runs
this; GitHub drift is `board_sync.py --check`, which CI runs separately.

    python tools/test_board.py

Exits 0 with a PASS line, or 1 with one line per failure.
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import board_sync as B  # noqa: E402

FIXTURE = """\
<!-- board-sync:milestones -->
| M | Deliverable | Week | Due | Team hrs | Progress |
| --- | --- | --- | --- | --- | --- |
| M1 | Memo | 5 | 2026-09-27 | 3 | — |
| M2 | Design | 6 | 2026-10-11 | 8 | — |
<!-- /board-sync:milestones -->

## M1 — Memo

- [x] **Exit criterion one** `crit` @camden <!-- #7 -->
- [ ] M1 hw: a task whose words run long enough to wrap onto a
      second line `long-one` @lance
      <!-- #8 -->
- [ ] A brand-new line with no tag yet

## M2 — Design

- [ ] Decide: something `dec`
"""


def parser_tests(fails: list[str]) -> None:
    def check(cond: bool, msg: str) -> None:
        if not cond:
            fails.append(f"parser: {msg}")

    b = B.parse(FIXTURE)
    check(list(b.rows) == ["M1", "M2"], f"table rows {list(b.rows)}")
    check(len(b.items) == 4, f"expected 4 items, got {len(b.items)}")
    if len(b.items) != 4:
        return
    one, two, new, dec = b.items
    check(one.checked and one.number == 7 and one.owner == "camden"
          and one.slug == "crit", f"first item fields: {one}")
    check(one.title == "M1: Exit criterion one", f"bold stripped, prefix added: {one.title!r}")
    check(two.number == 8 and two.owner == "lance" and two.slug == "long-one",
          f"continuation lines joined: {two}")
    check(two.title.startswith("M1 hw: a task") and two.title.endswith("second line"),
          f"existing M-prefix kept: {two.title!r}")
    check(new.number is None and not new.checked, "untagged line is a new issue")
    check(dec.milestone == "M2" and dec.title == "M2: Decide: something",
          f"section tracking: {dec}")

    B.set_tag(b, new, 42)
    B.set_checked(b, new)
    again = B.parse(B.render(b))
    check([i.number for i in again.items] == [7, 8, 42, None], "tag written back and re-read")
    check(again.items[2].checked, "tick written back and re-read")
    check("| M1 | Memo | 5 | 2026-09-27 | 3 | 🟡 2/3 |" in B.render(b),
          "progress recounted from the boxes")

    for bad, why in [
        ("- [ ] orphan line", "checkbox outside a section"),
        ("## M1\n- [ ] a <!-- #1 -->\n- [ ] b <!-- #1 -->", "duplicate tag"),
        ("## M1\n- [ ] a @nobody", "unknown owner"),
    ]:
        try:
            B.parse(bad)
            fails.append(f"parser: accepted {why}")
        except B.Fail:
            pass


def main() -> int:
    fails: list[str] = []
    parser_tests(fails)

    try:
        b = B.parse(B.LIST_DOC.read_text(encoding="utf-8"))
    except B.Fail as e:
        print(f"milestones.md                FAIL\n  {e}")
        return 1
    if list(b.rows) != [f"M{n}" for n in range(7)]:
        fails.append(f"milestones.md: table must list M0..M6 in order, got {list(b.rows)}")
    for ms, cells in b.rows.items():
        if not re.fullmatch(r"\d{4}-\d{2}-\d{2}", cells[3]):
            fails.append(f"milestones.md: {ms} Due must be YYYY-MM-DD, got {cells[3]!r}")
        if not any(it.milestone == ms for it in b.items):
            fails.append(f"milestones.md: {ms} has no checkboxes")
    slugs = [it.slug for it in b.items if it.slug]
    dup = {s for s in slugs if slugs.count(s) > 1}
    if dup:
        fails.append(f"milestones.md: slug used twice: {', '.join(sorted(dup))}")

    if fails:
        print(f"milestones.md                FAIL ({len(fails)})")
        for f in fails:
            print(f"  {f}")
        return 1
    tagged = sum(1 for it in b.items if it.number)
    print(f"milestones.md                PASS ({len(b.items)} boxes, {tagged} linked to "
          f"issues, {sum(it.checked for it in b.items)} ticked)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
