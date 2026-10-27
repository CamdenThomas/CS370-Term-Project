"""board_sync: reconcile docs/milestones.md with GitHub, against a fake `gh`."""
import contextlib
import io
import json
import os
import sys
import tempfile
import unittest
from unittest import mock

import board_sync as B

DOC = """\
<!-- board-sync:milestones -->
| M | Deliverable | Week | Due | Team hrs | Progress |
| --- | --- | --- | --- | --- | --- |
| M1 | Memo | 5 | 2026-09-27 | 3 | — |
<!-- /board-sync:milestones -->

## M1 — Memo

- [ ] First new line
- [ ] Second new line
"""

MILESTONE = {"title": "M1", "number": 1, "due_on": "2026-09-27T23:59:59Z",
             "description": "Memo"}


class FakeGh:
    """Answers `gh` calls from a table; anything unlisted fails like a gh error."""

    def __init__(self, issues=(), fail=()):
        self.issues = list(issues)
        self.fail = set(fail)      # call kinds that exit non-zero
        self.created = 0
        self.calls = []

    def __call__(self, args):
        self.calls.append(args)
        kind = " ".join(args[:2])
        if kind in self.fail:
            return 1, "", f"{kind}: HTTP 502"
        if kind == "repo view":
            return 0, "me/carwatch", ""
        if kind.startswith("api repos/me/carwatch/milestones?"):
            return 0, json.dumps([MILESTONE]), ""
        if kind == "issue list":
            return 0, json.dumps(self.issues), ""
        if kind == "issue create":
            self.created += 1
            if self.created > 1:
                return 1, "", "issue create: HTTP 502"
            return 0, "https://github.com/me/carwatch/issues/41", ""
        if kind in ("issue edit", "issue close"):
            return 0, "", ""
        return 1, "", f"unexpected gh call: {args}"


class Sync(unittest.TestCase):
    def setUp(self):
        self.dir = tempfile.TemporaryDirectory()
        self.doc = os.path.join(self.dir.name, "milestones.md")
        with open(self.doc, "w", encoding="utf-8") as f:
            f.write(DOC)

    def tearDown(self):
        self.dir.cleanup()

    def run_sync(self, fake, *flags):
        out = io.StringIO()
        with mock.patch.object(B, "LIST_DOC", B.Path(self.doc)), \
                mock.patch.object(B.Gh, "run", fake), \
                mock.patch.object(sys, "argv", ["board_sync.py", "--no-board", *flags]), \
                contextlib.redirect_stdout(out):
            try:
                code = B.main()
            except B.Fail as e:
                code = f"Fail: {e}"
        return code, out.getvalue()

    def read_doc(self):
        with open(self.doc, encoding="utf-8") as f:
            return f.read()

    def test_a_created_issue_keeps_its_tag_when_a_later_create_fails(self):
        code, _ = self.run_sync(FakeGh())
        self.assertIn("Fail", str(code))
        self.assertIn("- [ ] First new line <!-- #41 -->", self.read_doc(),
                      "the next run would create #41 a second time")
        self.assertEqual(os.listdir(self.dir.name), ["milestones.md"], "no temp file left")

    def test_check_writes_nothing(self):
        code, _ = self.run_sync(FakeGh(), "--check")
        self.assertEqual(code, 1)
        self.assertEqual(self.read_doc(), DOC)


if __name__ == "__main__":
    unittest.main()
