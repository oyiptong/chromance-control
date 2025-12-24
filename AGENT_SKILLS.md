# Agent Skills — Chromance Control Firmware

This file defines **explicit, mandatory agent skills** that must be followed during implementation.

These skills are **mechanical process constraints**, not suggestions. They exist to ensure traceability, auditability, and disciplined execution in an autonomous development workflow.

---

## Skill: Task Log Update

### Purpose

Ensure that all meaningful progress, decisions, and blockers are recorded in a single, append-only task log (`TASK_LOG.md`).

This skill enforces **engineering traceability**, not project management ceremony.

---

### Trigger Conditions (When This Skill MUST Run)

The agent **must** update `TASK_LOG.md` whenever **any** of the following occur:

1. A planned task is completed
2. Unit tests are added or modified
3. A design or architectural decision is made or revised
4. A blocker or unexpected issue is discovered
5. The interpretation of the spec or feedback changes

If none of the above occurred, **do not update the task log**.

---

### Prohibited Triggers (When This Skill MUST NOT Run)

Do **not** update `TASK_LOG.md` for:

- Exploratory thinking or planning
- Reading or reviewing documents
- Asking clarifying questions
- Code that was started but not meaningfully progressed
- Pure refactors with no behavioral or structural impact

---

### Required Behavior

When triggered, the agent must:

1. Append a **new entry at the top** of `TASK_LOG.md` (reverse chronological order)
2. Follow the existing format exactly
3. Use **factual, concrete language only**
4. Reference **specific files, functions, or tests**
5. Choose exactly **one status marker** per entry

Allowed status markers:
- 🟢 Done
- 🟡 In progress
- 🔴 Blocked
- 🔵 Decision
- 🧪 Test added

---

### Explicit Prohibitions

The agent must **not**:

- Rewrite, reorder, or delete existing log entries
- Add speculative, narrative, or promotional language
- Invent tasks or future work
- Add time estimates or percentages
- Modify the task log without a qualifying trigger

---

### Validation Rule

Any implementation work that:

- changes code behavior
- adds or modifies tests
- introduces or resolves a blocker
- makes or revises a decision

**is invalid** unless a corresponding `TASK_LOG.md` entry exists.

---

### Example (Correct)

```
### 2025-01-12 — Diagnostic strip state machine implemented

Status: 🟢 Done

What was done:
- Implemented per-strip diagnostic state machine with 5-cycle flash behavior
- Added restart logic after final segment

Files touched:
- src/core/diagnostic_strip_sm.h
- test/test_diagnostic_pattern.cpp

Notes / Decisions:
- Defined one flash as ON+OFF cycle per spec
```

---

### Example (Incorrect — Do Not Do This)

- "Worked on diagnostics"
- "Made progress on tests"
- "Refactored some code"

---

## Enforcement

This skill is **mandatory**.

Failure to comply indicates incomplete or unreliable implementation and must be corrected before work proceeds.

