# Lance Baron — process artifacts

- `PROMPTLOG.md` — 6–10 annotated episodes (see the required coverage list inside)
- `REFLECTION.md` — one page, written at M5
- `transcripts/` — **raw `.jsonl` session files**, copied from `~/.claude/projects/`.
  Not summarized. Not exported. Not retyped.

## Copy-out, every milestone M1–M5

```sh
cp ~/.claude/projects/*CS370*/*.jsonl partners/lance/transcripts/
```

Transcripts purge after 30 days by default and this project runs 15 weeks. Losing
Week-5 transcripts in Week 15 is a *foreseeable* loss, and foreseeable losses are not
excused. `.claude/settings.json` raises `cleanupPeriodDays`, but the copy-out is still
mandatory — do it at the same moment you submit each milestone.

Your 10 process points are computed from **your** `.jsonl` files, corroborated against
**your** commits. Work in your own clone and your own sessions; sharing a login defeats
this and costs both partners.
