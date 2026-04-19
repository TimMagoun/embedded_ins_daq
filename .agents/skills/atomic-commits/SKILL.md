---
name: atomic-commits
description: Use when preparing git commits and you need each commit to contain the minimum diff for one atomic change, including partial staging from a file that contains multiple logical changes.
---

# Atomic Commits

Use this skill when you need to turn a dirty worktree into a sequence of small commits where each commit captures exactly one logical change.

## Rules

- One commit per atomic change.
- Stage only the hunks or lines required for that change.
- If a single file contains multiple unrelated changes, split them across multiple commits instead of bundling them together.
- Re-check the remaining diff after each commit before deciding on the next one.
- Do not leave unrelated edits in a commit just because they are already in the same file.

## Workflow

1. Identify the smallest self-contained change you want to record.
2. Inspect the working tree and isolate the exact lines or hunks that belong to that change.
3. Stage only that subset of the file, even if the file also contains other edits.
4. Commit with a message that describes only that one change.
5. Review the remaining unstaged and uncommitted diff.
6. Repeat the process until each distinct change has its own commit.

## Partial Staging Guidance

- Use partial staging whenever a file mixes multiple logical changes.
- If a hunk is too large, split it so the commit stays atomic.
- If necessary, make the file easy to stage by reordering or separating edits, but still keep each commit focused on one outcome.
- Prefer several small commits over one large commit that mixes concerns.

## Anti-Patterns

- Committing every change in a modified file just because the file is already open.
- Using a single commit for unrelated bug fixes, refactors, or cleanup.
- Skipping a diff review after the first commit and assuming the rest is fine.
