---
name: retrospective-skill-improvement
description: "Use when a session exposes a workflow mistake, repeated friction, or missing guardrail and the right fix is to improve project-specific rules or skills. Helps decide whether to update AGENT.md, strengthen an existing skill, or create a new project skill."
---

# Retrospective Skill Improvement

Use this skill after a concrete process failure or recurring friction has been identified and the goal is to turn that lesson into durable project guidance.

## Objective

Convert a specific lesson learned into the smallest project-local instruction that will prevent the same mistake later.

## Workflow

1. Identify the concrete failure.
2. State what should have prevented it.
3. Choose the smallest durable fix:
   - update `AGENT.md` for repo-wide rules
   - update an existing `.agents/skills/*/SKILL.md` when the lesson belongs to that workflow
   - create a new skill only when the lesson defines a reusable multi-step procedure
4. Write the rule in operational language:
   - when it applies
   - what action is required
   - what anti-pattern to avoid
5. Patch the relevant files.
6. Read back the edited sections and confirm the rule is specific, actionable, and scoped correctly.

## Decision Rules

- Prefer updating an existing artifact before creating a new one.
- Put the rule as close as possible to the failure domain.
- Do not add vague reminders such as "be more careful" or "think harder".
- Do not create a skill when a short `AGENT.md` rule is sufficient.
- Do not update unrelated instructions just because the file is nearby.

## Good Candidates

- event/API simplifications that accidentally drop required payload
- test files drifting across subsystem boundaries
- stale file context causing edits against outdated code
- repeated verification gaps
- workflow lessons that recur across tasks in this repository

## Output Standard

Every improvement should answer:

- What went wrong?
- Why did the current rules fail to prevent it?
- Why is this the smallest correct place to fix it?

If those answers are weak, do not write the rule yet.
