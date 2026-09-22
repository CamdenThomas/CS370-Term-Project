<!-- Every section is mandatory. A PR with an unfilled section is not ready for review.
     Caps: <= 500 changed lines, <= 10 commits. Over either? Split it. -->

## What this changes
<!-- One paragraph, plain language. What is different after this merges? -->


## Why
<!-- Tie it to a requirement, a decision ID from docs/DECISIONS.md, or a measurement.
     "To improve things" is not a reason. -->

**Decision IDs touched:**
**Milestone:** M
**Mechanism affected (B / D / E / F / none):**


## Review this hardest
<!-- Point the reviewer at the part most likely to be wrong. Be specific:
     a function, a memory ordering, an error path, a lifetime. -->


## What I am least sure about
<!-- Must not be empty. If everything is certain, the diff has not been thought about
     hard enough. -->


## Commit-by-commit
<!-- One line per commit, in order. The reviewer should be able to read this list and
     know the shape of the change before opening a single file. -->
1.
2.


## Verification — paste real output, do not summarize

<details><summary><code>make</code></summary>

```
```
</details>

<details><summary><code>make test</code></summary>

```
```
</details>

<details><summary><code>make asan</code></summary>

```
```
</details>

<!-- Hardware changes also require real evidence: dmesg verbatim, raw adapter replies,
     vcgencmd get_throttled, timing capture. No fixes argued from a verbal
     description of the symptom. -->


## Reviewer checklist
<!-- The reviewer ticks these. Anything unticked blocks the merge. -->

- [ ] I can explain **every line** of this diff in my own words
- [ ] I can answer, for any line: what wakes this? what happens on error? what is freed
      and when? why can't this drop or reorder a sample?
- [ ] Each commit is one idea, and builds and tests on its own
- [ ] No formatting, renames, or unrelated refactoring mixed into a logic commit
- [ ] No test was weakened, skipped, or deleted
- [ ] No network call, no hosted model, no third-party pretrained weights
- [ ] Any data that is not live is labeled `replay` or `synth` everywhere it appears
- [ ] Every allocation checked; every syscall error path handled **and logged**
- [ ] New decisions are in `docs/DECISIONS.md`, and I have signed the ones I agree with
- [ ] **If I do not understand something, I have asked instead of approving**

<!-- Approving code you cannot defend alone at the keyboard on demo day is the only
     unrecoverable mistake in this workflow. It is worth 20 individual points. -->
