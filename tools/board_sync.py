#!/usr/bin/env python3
"""
board_sync.py — reconcile docs/board.toml onto GitHub issues and the project board.

This is a RECONCILER, not a generator. It computes the difference between what
docs/board.toml says should exist and what GitHub actually has, then makes the
smallest set of changes that closes the gap. Consequences worth knowing:

  * Running it twice in a row does nothing the second time.
  * Issues that are not in board.toml are never touched. Not edited, not closed,
    not removed from the board. They are listed at the end as "unmanaged".
  * Adding one task = add one [[item]] block, re-run. Nothing else moves.
  * Board column, when `status` is omitted: Backlog if any blocker is still
    open, Ready if nothing blocks it, Done if the issue is closed.
  * GitHub is authoritative for CLOSURE. If a human closes an issue in the web
    UI, sync does not reopen it — it moves the card to Done and tells you the
    manifest is behind.
  * board.toml is authoritative for TEXT. Issue bodies are rewritten from it,
    except the region between the <!-- board-sync:deps --> markers, which the
    tool owns and recomputes (blocked-by list + decision link).
  * board.toml is authoritative for MILESTONE DATES. The table in
    docs/milestones.md is regenerated between its <!-- board-sync:milestones -->
    markers, so the two cannot disagree.
  * --check exits 1 when anything would change, so CI can gate on it.

Usage (Windows PowerShell, macOS, Linux — no bash required):

    python tools/board_sync.py --check           report drift, exit 1 if any
    python tools/board_sync.py --status          what is Ready, what is stuck, who owns it
    python tools/board_sync.py                   reconcile
    python tools/board_sync.py --only oilq,memo  reconcile just these slugs
    python tools/board_sync.py --close-questions allow closing question/decision/hardware
    python tools/board_sync.py --no-board        skip the project board entirely

Requires: python >= 3.11 (tomllib), gh >= 2.40 authenticated with the `repo`
and `project` scopes.  Check with: gh auth status
"""

from __future__ import annotations

import argparse
import json
import os
import re
import subprocess
import sys
import tempfile
import time
import tomllib
from datetime import datetime, timezone
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
MANIFEST = REPO_ROOT / "docs" / "board.toml"
LOCKFILE = REPO_ROOT / "docs" / "board.lock.json"
MILESTONE_DOC = REPO_ROOT / "docs" / "milestones.md"
DECISION_DOC = REPO_ROOT / "docs" / "DECISIONS.md"

DEP_OPEN = "<!-- board-sync:deps -->"
DEP_CLOSE = "<!-- /board-sync:deps -->"
DEP_RE = re.compile(re.escape(DEP_OPEN) + r".*?" + re.escape(DEP_CLOSE), re.DOTALL)

MS_OPEN = "<!-- board-sync:milestones -->"
MS_CLOSE = "<!-- /board-sync:milestones -->"
MS_RE = re.compile(re.escape(MS_OPEN) + r".*?" + re.escape(MS_CLOSE), re.DOTALL)

DECISION_RE = re.compile(r"\bD-\d{3}\b")

STATUSES = ["backlog", "blocked", "ready", "in-progress", "in-review", "done"]
# CLAUDE.md 7.11: these three close when a human answers, never by tooling.
NEVER_AUTOCLOSE = {"question", "decision", "hardware"}

# How a board.toml status maps onto whatever Status options the project actually
# has, best first. This is what lets sync drive a board built from one of
# GitHub's templates without reshaping — and reshaping someone's field silently
# discards every card's value, so it is opt-in (`[board] reshape_status = true`).
COLUMN_PREFERENCE = {
    "backlog":     ["Backlog", "Todo", "To do", "No Status"],
    "blocked":     ["Blocked", "Backlog", "Todo", "To do"],
    "ready":       ["Ready", "Backlog", "Todo", "To do"],
    "in-progress": ["In progress", "In Progress", "Doing"],
    "in-review":   ["In review", "In Review", "In progress", "In Progress"],
    "done":        ["Done", "Closed"],
}


def option_for(status: str, opts: dict[str, str]) -> tuple[str, str] | tuple[None, None]:
    """(option name, option id) for a board.toml status, or (None, None)."""
    for name in COLUMN_PREFERENCE.get(status, []):
        oid = opts.get(name.lower())
        if oid:
            return name, oid
    return None, None


# --------------------------------------------------------------------- output

def _safe(s: str) -> str:
    enc = getattr(sys.stdout, "encoding", None) or "utf-8"
    try:
        s.encode(enc)
        return s
    except UnicodeEncodeError:
        return s.encode(enc, "replace").decode(enc)


def say(msg: str = "") -> None:
    print(_safe(msg), flush=True)


class Fail(Exception):
    pass


# ------------------------------------------------------------------ gh driver

class Gh:
    def __init__(self, dry: bool):
        self.dry = dry
        self.reads = 0
        self.writes = 0
        self.pending = 0   # would-be writes in --check mode; drives the exit code

    def _run(self, args: list[str], check: bool) -> tuple[int, str, str]:
        p = subprocess.run(
            ["gh", *args],
            capture_output=True,
            text=True,
            encoding="utf-8",
            errors="replace",
        )
        if check and p.returncode != 0:
            raise Fail(f"gh {' '.join(args)}\n  -> {p.stderr.strip()}")
        return p.returncode, p.stdout.strip(), p.stderr.strip()

    def read(self, args: list[str], check: bool = True) -> str:
        self.reads += 1
        return self._run(args, check)[1]

    def read_json(self, args: list[str], default=None):
        code, out, err = self._run(args, check=False)
        self.reads += 1
        if code != 0:
            if default is not None:
                return default
            raise Fail(f"gh {' '.join(args)}\n  -> {err}")
        return json.loads(out) if out else default

    def write(self, args: list[str], label: str, check: bool = True) -> str:
        if self.dry:
            self.pending += 1
            say(f"    would: {label}")
            return ""
        self.writes += 1
        for attempt in range(4):
            code, out, err = self._run(args, check=False)
            if code == 0:
                return out
            low = err.lower()
            if "secondary rate limit" in low or "rate limit" in low or "abuse" in low:
                wait = 20 * (attempt + 1)
                say(f"    rate limited, waiting {wait}s ...")
                time.sleep(wait)
                continue
            if check:
                raise Fail(f"gh {' '.join(args)}\n  -> {err}")
            return ""
        raise Fail(f"gh {' '.join(args)} kept hitting the rate limit")


def body_file(text: str):
    """gh --body-file avoids every Windows command-line quoting problem."""
    fh = tempfile.NamedTemporaryFile("w", suffix=".md", delete=False, encoding="utf-8")
    fh.write(text)
    fh.close()
    return fh.name


# ------------------------------------------------------------------- manifest

def load_manifest() -> dict:
    if not MANIFEST.exists():
        raise Fail(f"manifest not found: {MANIFEST}")
    with MANIFEST.open("rb") as fh:
        m = tomllib.load(fh)

    people = m.get("people", {})
    declared_ms = {ms["name"] for ms in m.get("milestone", [])}
    declared_labels = {l["name"] for l in m.get("label", [])}
    items: dict[str, dict] = {}
    order: list[str] = []
    for raw in m.get("item", []):
        slug = raw.get("slug")
        if not slug:
            raise Fail(f"an [[item]] has no slug: {raw.get('title', '?')}")
        if slug in items:
            raise Fail(f"duplicate slug: {slug}")
        owner_key = raw.get("owner")
        if owner_key and owner_key not in people:
            raise Fail(f"{slug}: owner '{owner_key}' is not in [people]")
        status = raw.get("status")
        if status and status not in STATUSES:
            raise Fail(f"{slug}: status '{status}' not one of {STATUSES}")
        ms = raw.get("milestone")
        if ms and ms not in declared_ms:
            raise Fail(f"{slug}: milestone '{ms}' has no [[milestone]] block")
        for lab in raw.get("labels", []):
            if lab not in declared_labels:
                raise Fail(f"{slug}: label '{lab}' has no [[label]] block")
        decision = raw.get("decision")
        if decision and not DECISION_RE.fullmatch(decision):
            raise Fail(f"{slug}: decision '{decision}' is not of the form D-007")
        if not raw.get("body", "").strip():
            raise Fail(f"{slug}: body is empty")
        items[slug] = {
            "slug": slug,
            "title": raw["title"].strip(),
            "body": raw.get("body", "").strip(),
            "milestone": raw.get("milestone"),
            "labels": list(raw.get("labels", [])),
            "assignee": people.get(owner_key) if owner_key else None,
            "blocked_by": list(raw.get("blocked_by", [])),
            "status": status,
            "decision": decision,
        }
        order.append(slug)

    for slug, it in items.items():
        for b in it["blocked_by"]:
            if b not in items:
                raise Fail(f"{slug}: blocked_by references unknown slug '{b}'")
            if b == slug:
                raise Fail(f"{slug}: blocked_by itself")

    # cycle check — a dependency cycle would deadlock the board silently
    state: dict[str, int] = {}

    def walk(s: str, trail: list[str]):
        if state.get(s) == 2:
            return
        if state.get(s) == 1:
            raise Fail("dependency cycle: " + " -> ".join(trail + [s]))
        state[s] = 1
        for b in items[s]["blocked_by"]:
            walk(b, trail + [s])
        state[s] = 2

    for slug in items:
        walk(slug, [])

    m["_items"] = items
    m["_order"] = order
    m["_decisions"] = check_decisions(items)
    return m


def check_decisions(items: dict[str, dict]) -> list[str]:
    """Every D-### named in the manifest must exist in docs/DECISIONS.md.

    A warning, not an error: Claude may raise an item that gates a decision
    before the decision has been written up. But a typo'd D-ID would otherwise
    sit in an issue body forever pointing at nothing.
    """
    if not DECISION_DOC.exists():
        return []
    known = set(DECISION_RE.findall(DECISION_DOC.read_text(encoding="utf-8")))
    missing: dict[str, set[str]] = {}
    for slug, it in items.items():
        refs = set(DECISION_RE.findall(it["body"]))
        if it["decision"]:
            refs.add(it["decision"])
        for bad in sorted(refs - known):
            missing.setdefault(bad, set()).add(slug)
    return [f"{did} (referenced by {', '.join(sorted(slugs))})"
            for did, slugs in sorted(missing.items())]


def load_lock() -> dict:
    if LOCKFILE.exists():
        try:
            return json.loads(LOCKFILE.read_text(encoding="utf-8"))
        except json.JSONDecodeError:
            say(f"!! {LOCKFILE.name} is corrupt; rebuilding it from GitHub")
    return {"items": {}, "project": {}}


def save_lock(lock: dict, dry: bool) -> None:
    if dry:
        return
    LOCKFILE.write_text(
        json.dumps(lock, indent=2, sort_keys=True, ensure_ascii=False) + "\n",
        encoding="utf-8",
    )


# ---------------------------------------------------------------------- parts

def sync_labels(gh: Gh, manifest: dict) -> None:
    say("== labels ==")
    have = {l["name"]: l for l in gh.read_json(["label", "list", "--json", "name,color,description"], [])}
    for spec in manifest.get("label", []):
        name, color, desc = spec["name"], spec["color"].lstrip("#"), spec.get("description", "")
        cur = have.get(name)
        if cur and cur["color"].lower() == color.lower() and (cur.get("description") or "") == desc:
            continue
        verb = "update" if cur else "create"
        gh.write(
            ["label", "create", name, "--color", color, "--description", desc, "--force"],
            f"{verb} label {name}",
        )
        if not gh.dry:
            say(f"  {verb}d {name}")


def sync_milestones(gh: Gh, repo: str, manifest: dict) -> dict[str, int]:
    say("== milestones ==")
    existing = gh.read_json(["api", f"repos/{repo}/milestones?state=all&per_page=100"], [])
    by_title = {m["title"]: m for m in existing}
    out: dict[str, int] = {}
    for spec in manifest.get("milestone", []):
        title = spec["name"]
        due = f"{spec['due']}T23:59:59Z"
        desc = spec.get("deliverable") or spec.get("description", "")
        cur = by_title.get(title)
        if cur is None:
            gh.write(
                ["api", f"repos/{repo}/milestones", "-f", f"title={title}",
                 "-f", f"due_on={due}", "-f", f"description={desc}"],
                f"create milestone {title}",
            )
            if not gh.dry:
                say(f"  created {title}")
            out[title] = -1
            continue
        out[title] = cur["number"]
        cur_due = (cur.get("due_on") or "")[:10]
        if cur_due != spec["due"] or (cur.get("description") or "") != desc:
            gh.write(
                ["api", "-X", "PATCH", f"repos/{repo}/milestones/{cur['number']}",
                 "-f", f"due_on={due}", "-f", f"description={desc}"],
                f"update milestone {title}",
            )
            if not gh.dry:
                say(f"  updated {title} (due {cur_due} -> {spec['due']})")
    return out


def fetch_issues(gh: Gh) -> dict[int, dict]:
    rows = gh.read_json(
        ["issue", "list", "--state", "all", "--limit", "500",
         "--json", "number,title,body,labels,milestone,assignees,state,url,createdAt"],
        [],
    )
    return {r["number"]: r for r in rows}


def render_managed(item: dict, numbers: dict[str, int],
                   issues: dict[int, dict]) -> tuple[str, bool]:
    """Return the tool-owned region of the issue body, and whether it is blocked.

    The region carries the blocked-by list and the decision link. Everything
    outside it comes from board.toml; everything inside it is recomputed.
    """
    lines, blocking = [], False
    if item["blocked_by"]:
        parts = []
        for slug in item["blocked_by"]:
            num = numbers.get(slug)
            if num is None:
                parts.append(f"`{slug}` (not created yet)")
                blocking = True
                continue
            issue = issues.get(num)
            done = issue is not None and issue["state"].upper() == "CLOSED"
            parts.append(f"#{num}{' ✔' if done else ''}")
            blocking = blocking or not done
        head = "Blocked by" if blocking else "Was blocked by (all clear)"
        lines.append(f"**{head}:** " + ", ".join(parts))
    if item["decision"]:
        lines.append(f"**Decision:** {item['decision']} — recorded in `docs/DECISIONS.md`")
    if not lines:
        return "", False
    block = (
        f"{DEP_OPEN}\n\n---\n" + "\n\n".join(lines) +
        "\n\n<sub>Maintained by tools/board_sync.py from docs/board.toml — edits here are overwritten.</sub>\n"
        f"{DEP_CLOSE}"
    )
    return block, blocking


def desired_body(item: dict, dep_block: str) -> str:
    base = DEP_RE.sub("", item["body"]).strip()
    return (base + ("\n\n" + dep_block if dep_block else "")).strip()


def norm(s: str | None) -> str:
    return (s or "").replace("\r\n", "\n").strip()


def column_for(item: dict, issue: dict | None,
               numbers: dict[str, int], issues: dict[int, dict]) -> str:
    """The status key an item should sit in. One definition, used everywhere.

    An explicit `status` in board.toml wins. Otherwise: Done once the issue is
    closed, Backlog while anything it depends on is still open, Ready when the
    path is clear. The Blocked column is reserved for `status = "blocked"` —
    waiting on something that is not another item.
    """
    if item["status"]:
        return item["status"]
    if issue is not None and issue["state"].upper() == "CLOSED":
        return "done"
    _, blocking = render_managed(item, numbers, issues)
    return "backlog" if blocking else "ready"


def age_days(issue: dict | None) -> int | None:
    stamp = (issue or {}).get("createdAt") or ""
    try:
        created = datetime.fromisoformat(stamp.replace("Z", "+00:00"))
    except ValueError:
        return None
    return (datetime.now(timezone.utc) - created).days


def status_report(items: dict, order: list[str], numbers: dict[str, int],
                  issues: dict[int, dict]) -> None:
    """The Monday view: what is actionable, what is stuck, and who owns it."""
    cols: dict[str, list[str]] = {}
    for slug in order:
        num = numbers.get(slug)
        col = column_for(items[slug], issues.get(num) if num else None, numbers, issues)
        cols.setdefault(col, []).append(slug)

    say("== board ==")
    say("  " + " · ".join(f"{c}: {len(cols.get(c, []))}" for c in STATUSES))

    say("")
    say("== ready to pick up ==")
    ready = cols.get("ready", [])
    if not ready:
        say("  (nothing — everything open is waiting on something)")
    by_owner: dict[str, list[str]] = {}
    for slug in ready:
        by_owner.setdefault(items[slug]["assignee"] or "unassigned", []).append(slug)
    for owner in sorted(by_owner, key=lambda o: (o == "unassigned", o)):
        say(f"  {owner}:")
        for slug in by_owner[owner]:
            it = items[slug]
            flag = "  [critical-path]" if "critical-path" in it["labels"] else ""
            say(f"    #{numbers.get(slug, '?')}  {it['title']}{flag}")

    say("")
    say("== critical path, not yet ready ==")
    stuck = [s for s in order
             if "critical-path" in items[s]["labels"]
             and cols_of(s, cols) not in ("ready", "done", "in-progress", "in-review")]
    if not stuck:
        say("  (clear)")
    for slug in stuck:
        it = items[slug]
        holders = []
        for b in it["blocked_by"]:
            bnum = numbers.get(b)
            bissue = issues.get(bnum) if bnum else None
            if bissue is None or bissue["state"].upper() != "CLOSED":
                holders.append(f"{b} (#{bnum})" if bnum else b)
        say(f"  #{numbers.get(slug, '?')}  {it['title']}")
        say(f"      held by: {', '.join(holders) if holders else 'nothing — set status by hand'}")

    say("")
    say("== open questions and decisions ==")
    asked = [s for s in order
             if ({"question", "decision"} & set(items[s]["labels"]))
             and cols_of(s, cols) != "done"]
    if not asked:
        say("  (none open)")
    for slug in asked:
        it, num = items[slug], numbers.get(slug)
        days = age_days(issues.get(num) if num else None)
        age = f"{days}d" if days is not None else "?"
        who = it["assignee"] or "unassigned"
        say(f"  #{num}  [{age:>4}]  {who:<14} {it['title']}")


def cols_of(slug: str, cols: dict[str, list[str]]) -> str:
    for col, slugs in cols.items():
        if slug in slugs:
            return col
    return "backlog"


# ------------------------------------------------------- generated doc table

def sync_milestone_doc(manifest: dict, items: dict, numbers: dict[str, int],
                       issues: dict[int, dict], dry: bool) -> bool:
    """Regenerate the milestone table in docs/milestones.md from board.toml.

    board.toml owns the dates; this table derives from them, so the two cannot
    drift. Only the region between the markers is touched — every word of exit
    criteria below it is hand-written and stays that way.
    """
    if not MILESTONE_DOC.exists():
        return False
    doc = MILESTONE_DOC.read_text(encoding="utf-8")
    if not MS_RE.search(doc):
        say("  ! docs/milestones.md has no <!-- board-sync:milestones --> markers; "
            "table left alone")
        return False

    done_by_ms: dict[str, list[int]] = {}
    for slug, it in items.items():
        num = numbers.get(slug)
        closed = num is not None and issues.get(num, {}).get("state", "").upper() == "CLOSED"
        tally = done_by_ms.setdefault(it["milestone"] or "—", [0, 0])
        tally[1] += 1
        if closed:
            tally[0] += 1

    rows = ["| M | Deliverable | Week | Due | Team hrs | Progress |",
            "|---|---|---|---|---|---|"]
    for spec in manifest.get("milestone", []):
        name = spec["name"]
        done, total = done_by_ms.get(name, [0, 0])
        if total == 0:
            prog = "—"
        elif done == total:
            prog = f"✅ {done}/{total}"
        elif done:
            prog = f"🟡 {done}/{total}"
        else:
            prog = f"⬜ 0/{total}"
        rows.append(
            f"| {name} | {spec.get('deliverable') or spec.get('description', '')} "
            f"| {spec.get('week', '—')} | {spec['due']} | {spec.get('hours', '—')} | {prog} |")

    block = (MS_OPEN + "\n\n" + "\n".join(rows) +
             "\n\n<sub>Generated by `tools/board_sync.py` from `docs/board.toml`. "
             "Edit the dates there, not here.</sub>\n" + MS_CLOSE)
    new = MS_RE.sub(lambda _: block, doc)
    if new == doc:
        return False
    if dry:
        say("    would: regenerate the milestone table in docs/milestones.md")
        return True
    MILESTONE_DOC.write_text(new, encoding="utf-8")
    say("  regenerated the milestone table in docs/milestones.md")
    return True


# ----------------------------------------------------------------------- main

def main() -> int:
    ap = argparse.ArgumentParser(description="Reconcile docs/board.toml onto GitHub.")
    ap.add_argument("--check", action="store_true",
                    help="report drift, change nothing; exits 1 if anything would change")
    ap.add_argument("--status", action="store_true",
                    help="print what is Ready, what is stuck and who owns it, then stop")
    ap.add_argument("--only", default="", help="comma-separated slugs to reconcile")
    ap.add_argument("--close-questions", action="store_true",
                    help="permit closing issues labeled question/decision/hardware")
    ap.add_argument("--no-board", action="store_true", help="skip the project board")
    args = ap.parse_args()

    gh = Gh(dry=args.check or args.status)
    manifest = load_manifest()
    items, order = manifest["_items"], manifest["_order"]
    lock = load_lock()
    only = {s.strip() for s in args.only.split(",") if s.strip()}
    if only:
        unknown = only - set(items)
        if unknown:
            raise Fail(f"--only names unknown slugs: {', '.join(sorted(unknown))}")

    repo = gh.read(["repo", "view", "--json", "nameWithOwner", "-q", ".nameWithOwner"])
    say(f"repo: {repo}" + ("   [CHECK ONLY — nothing will be changed]" if args.check else ""))
    say("")

    if not args.status:
        sync_labels(gh, manifest)
        sync_milestones(gh, repo, manifest)

    # ---- issues -----------------------------------------------------------
    if not args.status:
        say("== issues ==")
    issues = fetch_issues(gh)
    by_title = {norm(r["title"]): n for n, r in issues.items()}
    numbers: dict[str, int] = {}

    # Resolve slug -> issue number from the lock, falling back to exact title
    # match so a lost lock file re-adopts rather than duplicates.
    for slug in order:
        num = lock["items"].get(slug, {}).get("number")
        if num in issues:
            numbers[slug] = num
        else:
            match = by_title.get(norm(items[slug]["title"]))
            if match:
                numbers[slug] = match
                say(f"  adopted #{match} for '{slug}' by title")

    if args.status:
        status_report(items, order, numbers, issues)
        return 0

    managed_labels = {spec["name"] for spec in manifest.get("label", [])} | {"blocked"}
    created = updated = 0
    stale_closed: list[str] = []

    # Two passes: create everything first so dependency numbers exist, then
    # write the dependency blocks against the complete map.
    for slug in order:
        if only and slug not in only:
            continue
        item = items[slug]
        if slug in numbers:
            continue
        cmd = ["issue", "create", "--title", item["title"]]
        path = body_file(desired_body(item, ""))
        cmd += ["--body-file", path]
        for lab in item["labels"]:
            cmd += ["--label", lab]
        if item["milestone"]:
            cmd += ["--milestone", item["milestone"]]
        if item["assignee"]:
            cmd += ["--assignee", item["assignee"]]
        out = gh.write(cmd, f"create issue '{item['title']}'")
        os.unlink(path)
        if args.check:
            say(f"  + {item['title']}")
            continue
        num = int(out.rsplit("/", 1)[-1])
        numbers[slug] = num
        issues[num] = {
            "number": num, "title": item["title"], "state": "OPEN",
            "createdAt": "", "body": desired_body(item, ""),
            "labels": [{"name": l} for l in item["labels"]],
            "milestone": {"title": item["milestone"]} if item["milestone"] else None,
            "assignees": [{"login": item["assignee"]}] if item["assignee"] else [],
            "url": out,
        }
        created += 1
        say(f"  #{num}  {item['title']}")
        time.sleep(0.7)  # stay under GitHub's content-creation rate limit

    for slug in order:
        if only and slug not in only:
            continue
        num = numbers.get(slug)
        if num is None:
            continue
        item, issue = items[slug], issues[num]
        dep_block, blocking = render_managed(item, numbers, issues)
        want_body = desired_body(item, dep_block)

        want_labels = set(item["labels"])
        if blocking:
            want_labels.add("blocked")
        have_labels = {l["name"] for l in issue.get("labels", [])}
        add = sorted(want_labels - have_labels)
        drop = sorted((have_labels & managed_labels) - want_labels)

        cmd, why = ["issue", "edit", str(num)], []
        if norm(issue.get("title")) != norm(item["title"]):
            cmd += ["--title", item["title"]]
            why.append("title")
        path = None
        if norm(issue.get("body")) != norm(want_body):
            path = body_file(want_body)
            cmd += ["--body-file", path]
            why.append("body")
        want_ms = item["milestone"]
        have_ms = (issue.get("milestone") or {}).get("title")
        if want_ms and have_ms != want_ms:
            cmd += ["--milestone", want_ms]
            why.append(f"milestone {have_ms}->{want_ms}")
        if item["assignee"]:
            have_as = {a["login"] for a in issue.get("assignees", [])}
            if item["assignee"] not in have_as:
                cmd += ["--add-assignee", item["assignee"]]
                why.append(f"assign {item['assignee']}")
        for lab in add:
            cmd += ["--add-label", lab]
        for lab in drop:
            cmd += ["--remove-label", lab]
        if add or drop:
            why.append("labels " + " ".join([f"+{l}" for l in add] + [f"-{l}" for l in drop]))

        if why:
            gh.write(cmd, f"edit #{num}: {', '.join(why)}")
            updated += 1
            say(f"  ~ #{num}  {', '.join(why)}")
        if path:
            os.unlink(path)

        closed = issue["state"].upper() == "CLOSED"
        if item["status"] == "done" and not closed:
            blocked_label = NEVER_AUTOCLOSE & set(item["labels"])
            if blocked_label and not args.close_questions:
                say(f"  ! #{num} is marked done but labeled {'/'.join(sorted(blocked_label))} — "
                    f"re-run with --close-questions to close it")
            else:
                gh.write(["issue", "close", str(num)], f"close #{num}")
                say(f"  x #{num}  closed")
        elif closed and item["status"] != "done":
            stale_closed.append(f"{slug} (#{num})")

    # ---- generated milestone table ----------------------------------------
    doc_changed = sync_milestone_doc(manifest, items, numbers, issues, args.check)

    # ---- project board ----------------------------------------------------
    board_note = None
    if not args.no_board:
        board_note = sync_board(gh, manifest, items, order, numbers, issues,
                                only, args.check, repo)

    # ---- lock + report ----------------------------------------------------
    for slug, num in numbers.items():
        lock["items"].setdefault(slug, {})["number"] = num
        lock["items"][slug]["title"] = items[slug]["title"]
    # A slug removed from board.toml leaves a live issue behind. Do not drop it
    # silently — retire it in the lock and say so, so the issue gets dealt with.
    orphaned = []
    for slug in list(lock["items"]):
        if slug not in items:
            entry = lock["items"].pop(slug)
            num = entry.get("number")
            lock.setdefault("retired", {})[slug] = entry
            state = issues.get(num, {}).get("state", "?").upper()
            if state != "CLOSED":
                orphaned.append(f"{slug} (#{num}) — still open")
    save_lock(lock, args.check)

    managed_nums = set(numbers.values())
    unmanaged = [f"#{n} {r['title']}" for n, r in sorted(issues.items())
                 if n not in managed_nums and r["state"].upper() == "OPEN"]

    say("")
    say(f"created {created}, updated {updated}, reads {gh.reads}, writes {gh.writes}")
    if manifest["_decisions"]:
        say("")
        say("decision IDs referenced but not found in docs/DECISIONS.md:")
        for d in manifest["_decisions"]:
            say(f"  {d}")
    if board_note:
        say(board_note)
    if orphaned:
        say("")
        say("removed from board.toml but still open on GitHub — close or re-add:")
        for o in orphaned:
            say(f"  {o}")
    if stale_closed:
        say("")
        say("closed on GitHub but not marked done in board.toml — fold these in:")
        for s in stale_closed:
            say(f"  {s}")
    if unmanaged:
        say("")
        say("open issues not in board.toml (left untouched):")
        for u in unmanaged:
            say(f"  {u}")
    if args.check:
        pending = gh.pending + (1 if doc_changed else 0)
        say("")
        if pending:
            say(f"(check only — {pending} change(s) pending; run without --check to apply)")
            return 1
        say("(check only — in sync)")
    return 0


# ----------------------------------------------------------------------- board

def sync_board(gh: Gh, manifest, items, order, numbers, issues, only, dry, repo) -> str | None:
    say("")
    say("== project board ==")
    cfg = manifest.get("board", {})
    title = cfg.get("title")
    columns = cfg.get("columns", [])
    if not title:
        return None

    projects = gh.read_json(["project", "list", "--owner", "@me", "--format", "json"], None)
    if projects is None:
        say("  ! cannot read projects — run: gh auth refresh -s project")
        return "  board skipped (missing `project` scope)"

    # Identify by NUMBER when board.toml gives one. Matching on title is how you
    # end up quietly writing to one project while reading another in the browser.
    number = cfg.get("number")
    if number:
        proj = next((p for p in projects.get("projects", []) if p["number"] == number), None)
        if proj is None:
            raise Fail(f"[board] number = {number} does not exist for this account. "
                       f"Projects: " + ", ".join(f"#{p['number']} {p['title']!r}"
                                                 for p in projects.get("projects", [])))
        if proj["title"] != title:
            say(f"  ! [board] title is {title!r} but project #{number} is called "
                f"{proj['title']!r} — rename one of them")
    else:
        proj = next((p for p in projects.get("projects", []) if p["title"] == title), None)
    if proj is None:
        if dry:
            say(f"    would: create project '{title}'")
            return "  board not created (check only)"
        out = gh.write(["project", "create", "--owner", "@me", "--title", title,
                        "--format", "json"], f"create project '{title}'")
        proj = json.loads(out)
        say(f"  created project #{proj['number']}  {proj.get('url', '')}")
    pnum, pid = str(proj["number"]), proj["id"]
    say(f"  project #{pnum}  {proj['title']}  {proj.get('url', '')}")

    # The project people actually open is the one linked to the repo. If that is
    # not the one being written to, say so loudly — this is exactly the failure
    # where the board looks untouched because the cards being moved are elsewhere.
    linked = gh.read_json(
        ["api", "graphql", "-f",
         "query={repository(owner:\"%s\",name:\"%s\"){projectsV2(first:20)"
         "{nodes{number title}}}}" % tuple(repo.split("/", 1))], {})
    nodes = (((linked.get("data") or {}).get("repository") or {})
             .get("projectsV2") or {}).get("nodes") or []
    if nodes and not any(n["number"] == proj["number"] for n in nodes):
        say("  ! this project is NOT linked to the repository. The repo's Projects tab "
            "opens: " + ", ".join(f"#{n['number']} {n['title']!r}" for n in nodes))
        say("    Cards are being set here; that other board is not managed by sync.")

    fields = gh.read_json(["project", "field-list", pnum, "--owner", "@me",
                           "--format", "json", "--limit", "50"], {"fields": []})
    status = next((f for f in fields["fields"] if f["name"].lower() == "status"), None)
    if status is None:
        say("  ! no Status field on this project; cards will be added without a column")
        opts = {}
        fid = None
    else:
        fid = status["id"]
        opts = {o["name"].lower(): o["id"] for o in status.get("options", [])}
        missing = [c for c in columns if c.lower() not in opts]
        if missing and not cfg.get("reshape_status"):
            have = ", ".join(o["name"] for o in status.get("options", []))
            say(f"  Status options on this project: {have}")
            say(f"    board.toml also lists {', '.join(missing)} — mapping onto what exists "
                "rather than rewriting the field (set [board] reshape_status = true to rewrite; "
                "it discards every card's current value).")
            missing = []
        if missing and not dry:
            if reshape_status(gh, pid, fid, columns):
                fields = gh.read_json(["project", "field-list", pnum, "--owner", "@me",
                                       "--format", "json", "--limit", "50"], {"fields": []})
                status = next(f for f in fields["fields"] if f["id"] == fid)
                opts = {o["name"].lower(): o["id"] for o in status.get("options", [])}
                say("  Status field columns: " + ", ".join(columns))
            else:
                say("  ! could not reshape the Status field; mapping onto its existing "
                    "columns instead (see FALLBACK_COLUMN in board_sync.py)")

    existing = gh.read_json(["project", "item-list", pnum, "--owner", "@me",
                             "--format", "json", "--limit", "500"], {"items": []})
    on_board = {}
    for it in existing.get("items", []):
        c = it.get("content") or {}
        if c.get("number") is not None:
            on_board[c["number"]] = it

    added = moved = 0
    for slug in order:
        if only and slug not in only:
            continue
        num = numbers.get(slug)
        if num is None:
            continue
        item, issue = items[slug], issues[num]
        card = on_board.get(num)
        if card is None:
            out = gh.write(["project", "item-add", pnum, "--owner", "@me",
                            "--url", issue["url"], "--format", "json"],
                           f"add #{num} to board")
            if dry:
                continue
            card = json.loads(out)
            on_board[num] = card
            added += 1

        if not fid:
            continue
        want = column_for(item, issue, numbers, issues)
        chosen, oid = option_for(want, opts)
        if oid is None:
            say(f"  ! no Status option for '{want}' on this project (#{num})")
            continue
        if (card.get("status") or "").lower() == chosen.lower():
            continue
        gh.write(["project", "item-edit", "--id", card["id"], "--project-id", pid,
                  "--field-id", fid, "--single-select-option-id", oid],
                 f"#{num} -> {chosen}")
        moved += 1

    say(f"  added {added} card(s), moved {moved}")
    return None


def reshape_status(gh: Gh, project_id: str, field_id: str, columns: list[str]) -> bool:
    """Rewrite the Status single-select options to match board.toml columns.

    gh cannot pass a JSON array as a GraphQL variable on the command line, so the
    whole query+variables document goes in via --input.
    """
    query = (
        "mutation($f:ID!,$o:[ProjectV2SingleSelectFieldOptionInput!]!){"
        "updateProjectV2Field(input:{fieldId:$f, singleSelectOptions:$o}){"
        "projectV2Field{... on ProjectV2SingleSelectField{id}}}}"
    )
    payload = {
        "query": query,
        "variables": {
            "f": field_id,
            "o": [{"name": c, "color": "GRAY", "description": ""} for c in columns],
        },
    }
    path = body_file(json.dumps(payload))
    try:
        code, out, err = gh._run(["api", "graphql", "--input", path], check=False)
        gh.writes += 1
        if code != 0 or '"errors"' in out:
            detail = (err or out).strip().splitlines()
            say(f"    (reshape failed: {detail[0] if detail else 'unknown'})")
            return False
        return True
    finally:
        os.unlink(path)


if __name__ == "__main__":
    try:
        sys.exit(main())
    except Fail as e:
        say(f"\nERROR: {e}")
        sys.exit(1)
    except KeyboardInterrupt:
        say("\ninterrupted — re-run to continue where it left off")
        sys.exit(130)
