# Claude Code transcripts — Lance Baron

Lance's raw `.jsonl` session files, copied out of Claude Code's own store. They are a
graded deliverable (handout §12, CLAUDE.md §9): **raw — not summarized, not exported, not
retyped, never edited.** Claude Code deletes its copies after `cleanupPeriodDays`, so this
folder is the only copy that is guaranteed to survive to Week 15.

## At the end of every session

Exit Claude Code first (`/exit`), so the session file is complete. Then, in a normal
terminal at the repository root:

**Git Bash / macOS / Linux**

```sh
cp ~/.claude/projects/*CS370*/*.jsonl partners/lance/transcripts/
git add partners/lance/transcripts/*.jsonl
```

**PowerShell**

```powershell
Copy-Item "$env:USERPROFILE\.claude\projects\*CS370*\*.jsonl" partners\lance\transcripts\
git add partners/lance/transcripts/*.jsonl
```

Then commit on the session's branch, so the transcripts ride that session's pull request:

```sh
git commit -m "M<n> process: copy out Lance's Claude Code transcripts"
```

Copying again is safe: a session file only ever grows, so a newer copy replaces an older,
shorter one of the same session.

**If nothing is copied**, the project folder has a different name on this machine — list
`~/.claude/projects/` and use the folder whose name contains this repository's path.
