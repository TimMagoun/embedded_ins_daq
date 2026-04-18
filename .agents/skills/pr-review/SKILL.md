---
name: pr-review
description: "Use when reviewing a pull request, triaging PR comments, or checking whether a change is ready to merge. Focus on correctness, architecture boundaries, edge cases, style, and whether tests and documentation are adequate."
---

# PR Review

Use this skill when the task is to review code or address PR feedback.

## Review Priorities

1. Check behavioral correctness first. Verify the code does what the change claims and look for silent failure modes.
2. Check architecture boundaries. Make sure production interfaces do not absorb test scaffolding, debug-only state, or stage-specific shortcuts without a clear reason.
3. Check edge cases. Look for null handling, empty values, wraparound behavior, invalid configuration, and repeated resource initialization.
4. Check style and maintainability. Follow the repository formatter and local conventions; keep declarations scoped tightly and avoid avoidable duplication.
5. Check tests and docs. New or changed logic should have unit coverage, and public headers should explain the API contract while source files call out non-trivial implementation details.

## When Addressing Review Comments

- Map each review comment to a concrete code or documentation change before editing.
- Prefer code changes that resolve the root concern instead of replying with rationale alone.
- If a reviewer asks for a reusable process improvement, update repo-local instructions or add a skill when that guidance should persist across future tasks.
- After changes, rerun the narrowest relevant validation and summarize what was verified and what remains unverified.
