# Note on the PR template

GitHub picks up `.github/PULL_REQUEST_TEMPLATE.md` automatically — every new pull request
opens pre-filled with it. Do not delete sections to make a PR faster to open; the sections
are the review.

## Branch protection to set on `main` (once, by a human)

Settings → Branches → Add rule for `main`:

- [x] Require a pull request before merging
- [x] Require approvals: **1**
- [x] Dismiss stale pull request approvals when new commits are pushed
- [x] Require conversation resolution before merging
- [x] Do not allow bypassing the above settings
- [x] Block force pushes
- [x] Block deletions

Prefer **merge commits** over squash. Our granular history is a graded deliverable, and
squashing a ten-commit PR into one destroys exactly the thing we built these rules to
protect. If you do squash, rewrite the squash message so it is meaningful.
