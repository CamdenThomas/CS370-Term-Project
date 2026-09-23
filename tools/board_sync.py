#!/usr/bin/env python3
"""
board_sync.py — make GitHub match the checklist in docs/milestones.md (D-020).

docs/milestones.md is the board. Every checkbox line under a `## M<n>` heading is
one GitHub issue:

    - [ ] M2 hardware: first live capture from a real car `firstcap` @camden <!-- #20 -->

  [ ] / [x]        open / closed. Tick the box and sync: the issue closes.
  text             the issue title ("M<n>: " is prefixed if the text has none)
  `slug`           optional trailing name other docs cite; not part of the title
  @camden @lance   optional assignee
  <!-- #20 -->     the issue number. Written by sync the first time; never type it.

What sync does, and never does:
  * a line with no tag becomes a new issue, and the tag is written back
  * [x] on an open issue closes it
  * an issue a human closed on GitHub ticks its box here; sync never reopens
  * retitles, re-milestones and assigns to match the line
  * the table's Due column sets GitHub milestone due dates; Progress is recounted
  * issue bodies and labels are left alone: GitHub owns them
  * an open issue that no line mentions is reported, never closed

Usage (Python 3.11+, `gh` authenticated):

    python tools/board_sync.py              reconcile, then write tags/ticks back
    python tools/board_sync.py --check      report drift; exit 1 if anything would change
    python tools/board_sync.py --status     open boxes per milestone, by owner
    python tools/board_sync.py --no-board   skip the project board (CI: no PAT)
"""

from __future__ import annotations

import argparse
import json
import re
import subprocess
import sys
from dataclasses import dataclass, field
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
LIST_DOC = REPO_ROOT / "docs" / "milestones.md"

PEOPLE = {"camden": "CamdenThomas", "lance": "Lance-Baron"}
PROJECT_NUMBER = 1          # the `carwatch` project linked to the repo
WRAP = 90                   # .markdownlint.jsonc line length (D-018)

TABLE_OPEN = "<!-- board-sync:milestones -->"
TABLE_CLOSE = "<!-- /board-sync:milestones -->"

SECTION_RE = re.compile(r"^## (M[0-6])\b")
ITEM_RE = re.compile(r"^- \[([ xX])\] (.*)$")
TAG_RE = re.compile(r"\s*<!--\s*#(\d+)\s*-->")
OWNER_RE = re.compile(r"(?<!\S)@(\w+)")
SLUG_RE = re.compile(r"\s+`([a-z][a-z0-9-]{1,30})`$")
ROW_RE = re.compile(r"^\|\s*(M[0-6])\s*\|")


class Fail(Exception):
    pass


def say(msg: str = "") -> None:
    enc = getattr(sys.stdout, "encoding", None) or "utf-8"
    print(msg.encode(enc, "replace").decode(enc), flush=True)


# ------------------------------------------------------------------ parsing

@dataclass
class Item:
    milestone: str
    checked: bool
    text: str                  # the item's words, tag/owner/slug removed
    slug: str | None
    owner: str | None          # key into PEOPLE
    number: int | None         # the tag
    first: int                 # line index of "- [ ]"
    last: int                  # line index of the item's final continuation line

    @property
    def title(self) -> str:
        t = self.text.replace("**", "").strip()
        return t if re.match(r"M[0-6]\b", t) else f"{self.milestone}: {t}"


@dataclass
class Board:
    lines: list[str]
    items: list[Item] = field(default_factory=list)
    rows: dict[str, list[str]] = field(default_factory=dict)   # M -> table cells


def parse(text: str) -> Board:
    """Split docs/milestones.md into table rows and checkbox items."""
    b = Board(lines=text.split("\n"))
    section = None
    in_table = False
    i = 0
    while i < len(b.lines):
        line = b.lines[i]
        if line.strip() == TABLE_OPEN:
            in_table = True
        elif line.strip() == TABLE_CLOSE:
            in_table = False
        elif in_table and ROW_RE.match(line):
            cells = [c.strip() for c in line.strip().strip("|").split("|")]
            if len(cells) != 6:
                raise Fail(f"milestones.md:{i+1}: table row needs 6 cells: {line}")
            b.rows[cells[0]] = cells
        if line.startswith("## "):
            m = SECTION_RE.match(line)
            section = m.group(1) if m else None
        m = ITEM_RE.match(line)
        if m:
            if section is None:
                raise Fail(f"milestones.md:{i+1}: checkbox outside an `## M<n>` section")
            j = i
            while j + 1 < len(b.lines) and b.lines[j + 1].startswith("  ") \
                    and b.lines[j + 1].strip():
                j += 1
            raw = " ".join([m.group(2)] + [x.strip() for x in b.lines[i + 1:j + 1]])
            tags = TAG_RE.findall(raw)
            if len(tags) > 1:
                raise Fail(f"milestones.md:{i+1}: more than one issue tag on one item")
            raw = TAG_RE.sub("", raw).strip()
            owners = OWNER_RE.findall(raw)
            for o in owners:
                if o not in PEOPLE:
                    raise Fail(f"milestones.md:{i+1}: unknown owner @{o} "
                               f"(known: {', '.join('@' + p for p in PEOPLE)})")
            raw = OWNER_RE.sub("", raw).strip()
            sm = SLUG_RE.search(raw)
            slug = sm.group(1) if sm else None
            if sm:
                raw = raw[:sm.start()].rstrip()
            b.items.append(Item(section, m.group(1) != " ", re.sub(r"\s+", " ", raw),
                                slug, owners[0] if owners else None,
                                int(tags[0]) if tags else None, i, j))
            i = j
        i += 1
    seen: dict[int, int] = {}
    for it in b.items:
        if it.number is not None:
            if it.number in seen:
                raise Fail(f"milestones.md:{it.first+1}: issue #{it.number} is also on "
                           f"line {seen[it.number]+1}")
            seen[it.number] = it.first
    return b


def render(b: Board) -> str:
    """Rewrite the table's Progress column from the checkboxes."""
    out = []
    for line in b.lines:
        m = ROW_RE.match(line)
        if m and m.group(1) in b.rows:
            ms = m.group(1)
            mine = [it for it in b.items if it.milestone == ms]
            done = sum(it.checked for it in mine)
            icon = "✅" if done == len(mine) else "🟡" if done else "⬜"
            cells = b.rows[ms][:5] + [f"{icon} {done}/{len(mine)}" if mine else "—"]
            line = "| " + " | ".join(cells) + " |"
        out.append(line)
    return "\n".join(out)


def set_tag(b: Board, it: Item, number: int) -> None:
    """Append `<!-- #n -->` to the item's last line, wrapping to stay within WRAP."""
    tag = f"<!-- #{number} -->"
    last = b.lines[it.last]
    if len(last) + 1 + len(tag) <= WRAP:
        b.lines[it.last] = f"{last} {tag}"
    else:
        b.lines.insert(it.last + 1, "      " + tag)
        for other in b.items:
            if other.first > it.last:
                other.first += 1
                other.last += 1
        it.last += 1
    it.number = number


def set_checked(b: Board, it: Item) -> None:
    b.lines[it.first] = b.lines[it.first].replace("- [ ]", "- [x]", 1)
    it.checked = True


# ---------------------------------------------------------------------- gh

class Gh:
    def __init__(self, dry: bool):
        self.dry, self.reads, self.writes, self.pending = dry, 0, 0, 0

    def run(self, args: list[str]) -> tuple[int, str, str]:
        p = subprocess.run(["gh", *args], capture_output=True, text=True,
                           encoding="utf-8", errors="replace")
        return p.returncode, p.stdout.strip(), p.stderr.strip()

    def read_json(self, args: list[str], default=None):
        self.reads += 1
        code, out, err = self.run(args)
        if code != 0:
            if default is not None:
                return default
            raise Fail(f"gh {' '.join(args)}\n  -> {err}")
        return json.loads(out) if out else default

    def write(self, args: list[str], label: str) -> str:
        if self.dry:
            self.pending += 1
            say(f"    would: {label}")
            return ""
        self.writes += 1
        code, out, err = self.run(args)
        if code != 0:
            raise Fail(f"{label}\n  gh {' '.join(args)}\n  -> {err}")
        say(f"  {label}")
        return out


# -------------------------------------------------------------------- sync

def sync_milestones(gh: Gh, repo: str, b: Board) -> None:
    have = {m["title"]: m for m in
            gh.read_json(["api", f"repos/{repo}/milestones?state=all&per_page=100"], [])}
    for ms, cells in b.rows.items():
        due, desc = cells[3], cells[1]
        if not re.fullmatch(r"\d{4}-\d{2}-\d{2}", due):
            raise Fail(f"milestones.md: {ms} Due must be YYYY-MM-DD, got {due!r}")
        cur = have.get(ms)
        if cur is None:
            gh.write(["api", f"repos/{repo}/milestones", "-f", f"title={ms}",
                      "-f", f"due_on={due}T23:59:59Z", "-f", f"description={desc}"],
                     f"create milestone {ms}")
        elif (cur.get("due_on") or "")[:10] != due or (cur.get("description") or "") != desc:
            gh.write(["api", "-X", "PATCH", f"repos/{repo}/milestones/{cur['number']}",
                      "-f", f"due_on={due}T23:59:59Z", "-f", f"description={desc}"],
                     f"update milestone {ms} (due {due})")


def sync_issues(gh: Gh, b: Board) -> list[str]:
    """Make each line's issue match it. Returns lines worth reporting."""
    issues = {r["number"]: r for r in gh.read_json(
        ["issue", "list", "--state", "all", "--limit", "1000",
         "--json", "number,title,state,milestone,assignees"], [])}
    report = []
    for it in list(b.items):
        if it.number is None:
            label = "decision" if re.match(r"(Decide:|M[0-6] (decision|question))",
                                           it.text) else "task"
            cmd = ["issue", "create", "--title", it.title, "--milestone", it.milestone,
                   "--label", label, "--body",
                   f"Tracked in `docs/milestones.md`, {it.milestone}. Tick the box there "
                   "and run `python tools/board_sync.py`."]
            if it.owner:
                cmd += ["--assignee", PEOPLE[it.owner]]
            url = gh.write(cmd, f"create '{it.title}'")
            if gh.dry:
                if it.checked:
                    gh.write([], "close it again (the box is ticked)")
                continue
            set_tag(b, it, int(url.rsplit("/", 1)[-1]))
            issues[it.number] = {"number": it.number, "title": it.title, "state": "OPEN",
                                 "milestone": {"title": it.milestone},
                                 "assignees": [{"login": PEOPLE[it.owner]}]
                                 if it.owner else []}
        issue = issues.get(it.number)
        if issue is None:
            raise Fail(f"milestones.md:{it.first+1}: #{it.number} does not exist on GitHub")
        edit = []
        if issue["title"] != it.title:
            edit += ["--title", it.title]
        if (issue.get("milestone") or {}).get("title") != it.milestone:
            edit += ["--milestone", it.milestone]
        if it.owner and PEOPLE[it.owner] not in {a["login"] for a in issue["assignees"]}:
            edit += ["--add-assignee", PEOPLE[it.owner]]
        if edit:
            gh.write(["issue", "edit", str(it.number), *edit],
                     f"edit #{it.number}: {' '.join(e for e in edit if e.startswith('--'))}")
        closed = issue["state"].upper() == "CLOSED"
        if it.checked and not closed:
            gh.write(["issue", "close", str(it.number), "--reason", "completed"],
                     f"close #{it.number} (ticked)")
        elif closed and not it.checked:
            set_checked(b, it)
            report.append(f"#{it.number} was closed on GitHub; ticked its box")
    listed = {it.number for it in b.items}
    for n, r in sorted(issues.items()):
        if n not in listed and r["state"].upper() == "OPEN":
            report.append(f"#{n} is open on GitHub but on no line: {r['title']}")
    return report


def sync_board(gh: Gh, repo: str, b: Board) -> str | None:
    """Put every listed issue on the project board: open -> Ready, ticked -> Done."""
    projects = gh.read_json(["project", "list", "--owner", "@me", "--format", "json"], {})
    proj = next((p for p in projects.get("projects", [])
                 if p["number"] == PROJECT_NUMBER), None)
    if proj is None:
        return "board skipped: cannot read projects (run `gh auth refresh -s project`)"
    pnum, pid = str(PROJECT_NUMBER), proj["id"]
    fields = gh.read_json(["project", "field-list", pnum, "--owner", "@me",
                           "--format", "json"], {"fields": []})
    status_field = next((f for f in fields["fields"] if f["name"].lower() == "status"), None)
    opts = {o["name"].lower(): (o["name"], o["id"])
            for o in (status_field or {}).get("options", [])}
    cards = {(c.get("content") or {}).get("number"): c for c in gh.read_json(
        ["project", "item-list", pnum, "--owner", "@me", "--format", "json",
         "--limit", "1000"], {"items": []})["items"]}
    for it in b.items:
        if it.number is None:
            continue
        card = cards.get(it.number)
        if card is None:
            out = gh.write(["project", "item-add", pnum, "--owner", "@me", "--url",
                            f"https://github.com/{repo}/issues/{it.number}",
                            "--format", "json"], f"add #{it.number} to the board")
            if gh.dry:
                continue
            card = json.loads(out)
        want = opts.get("done") if it.checked else (opts.get("ready") or opts.get("todo"))
        if status_field and want and (card.get("status") or "") != want[0]:
            gh.write(["project", "item-edit", "--id", card["id"], "--project-id", pid,
                      "--field-id", status_field["id"],
                      "--single-select-option-id", want[1]],
                     f"#{it.number} -> {want[0]}")
    return None


def status(b: Board) -> None:
    for ms in b.rows:
        mine = [it for it in b.items if it.milestone == ms]
        open_ = [it for it in mine if not it.checked]
        say(f"{ms}  {len(mine) - len(open_)}/{len(mine)} done  (due {b.rows[ms][3]})")
        for owner in sorted({it.owner or "" for it in open_}):
            say(f"  {'@' + owner if owner else 'unassigned'}:")
            for it in open_:
                if (it.owner or "") == owner:
                    say(f"    #{it.number or '(new)'}  {it.text.replace('**', '')}")


def main() -> int:
    ap = argparse.ArgumentParser(description="Make GitHub match docs/milestones.md.")
    ap.add_argument("--check", action="store_true", help="change nothing; exit 1 on drift")
    ap.add_argument("--status", action="store_true", help="print open boxes and stop")
    ap.add_argument("--no-board", action="store_true", help="skip the project board")
    args = ap.parse_args()

    original = LIST_DOC.read_text(encoding="utf-8")
    b = parse(original)
    if args.status:
        status(b)
        return 0

    gh = Gh(dry=args.check)
    code, repo, err = gh.run(["repo", "view", "--json", "nameWithOwner",
                              "-q", ".nameWithOwner"])
    if code != 0:
        raise Fail(f"gh repo view -> {err}")
    say(f"repo: {repo}" + ("   [CHECK ONLY]" if args.check else ""))
    sync_milestones(gh, repo, b)
    report = sync_issues(gh, b)
    note = None if args.no_board else sync_board(gh, repo, b)

    new = render(b)
    if new != original:
        if args.check:
            gh.pending += 1
            say("    would: rewrite docs/milestones.md (tags, ticks or progress)")
        else:
            LIST_DOC.write_text(new, encoding="utf-8")
            say("  wrote docs/milestones.md")
    for r in report:
        say(f"  ! {r}")
    if note:
        say(f"  {note}")
    say(f"reads {gh.reads}, writes {gh.writes}")
    if args.check and gh.pending:
        say(f"(check only — {gh.pending} change(s) pending)")
        return 1
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except Fail as e:
        say(f"\nERROR: {e}")
        sys.exit(1)
    except KeyboardInterrupt:
        say("\ninterrupted — re-run to continue")
        sys.exit(130)
