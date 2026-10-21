# Claude Code transcripts — Camden Thomas

Camden's raw `.jsonl` session files, copied out of Claude Code's own store. They are a
graded deliverable (handout §12, CLAUDE.md §9): **raw — not summarized, not exported, not
retyped, never edited.** Claude Code deletes its copies after `cleanupPeriodDays`, so this
folder is the only copy that is guaranteed to survive to Week 15.

## At the end of every session

Exit Claude Code first (`/exit`), so the session file is complete. Then, in a normal
terminal at the repository root:

### macOS: Camden's laptop (Ghostty, zsh)

On this Mac the repository is `~/dev/school/cs370/car-watch`, so Claude Code stores its
sessions in `~/.claude/projects/-Users-crash-dev-school-cs370-car-watch/`. The folder name
contains `car-watch`, not `CS370-Term-Project`, so use this pattern. Exit Claude Code
(`/exit`), open Ghostty, and paste this one line. It copies every session, stages them,
and shows what is staged:

```sh
cd ~/dev/school/cs370/car-watch && ls -d ~/.claude/projects/*car-watch*/ && cp ~/.claude/projects/*car-watch*/*.jsonl partners/camden/transcripts/ && git add partners/camden/transcripts/*.jsonl && git status --short partners/camden/transcripts/
```

The same thing, one step at a time, if you want to check the folder first:

```sh
cd ~/dev/school/cs370/car-watch
ls -d ~/.claude/projects/*car-watch*/
cp ~/.claude/projects/*car-watch*/*.jsonl partners/camden/transcripts/
git add partners/camden/transcripts/*.jsonl
git status --short partners/camden/transcripts/
```

The `ls -d` line must print exactly one folder, the one ending in `cs370-car-watch`. If
zsh answers `no matches found`, or prints a folder from another project, stop and list
`~/.claude/projects/` to find the right name before copying.

### Git Bash / Linux

```sh
cp ~/.claude/projects/*CS370-Term-Project*/*.jsonl partners/camden/transcripts/
git add partners/camden/transcripts/*.jsonl
```

### PowerShell

```powershell
Copy-Item "$env:USERPROFILE\.claude\projects\*CS370-Term-Project*\*.jsonl" partners\camden\transcripts\
git add partners/camden/transcripts/*.jsonl
```

Then commit on the session's branch, so the transcripts ride that session's pull request:

```sh
git commit -m "M<n> process: copy out Camden's Claude Code transcripts"
```

Copying again is safe: a session file only ever grows, so a newer copy replaces an older,
shorter one of the same session.

**Match `*CS370-Term-Project*`, not `*CS370*`.** The wider pattern also matches the HW1
rbtree-lab project folder, and would copy another assignment's sessions into this one's
graded deliverable. Check what you are about to copy before you copy it:

```sh
ls -d ~/.claude/projects/*CS370-Term-Project*/
```

**If nothing is copied**, the project folder has a different name on this machine — list
`~/.claude/projects/` and use the folder whose name contains this repository's path.
