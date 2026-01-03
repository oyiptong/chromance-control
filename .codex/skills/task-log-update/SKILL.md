---
name: Task Log Update
description: Ensure that all meaningful progress, decisions, and blockers are recorded in a single, append-only task log (`TASK_LOG.md`).
version: 1
source: AGENT_SKILLS.md
---

# Task Log Update

_Extracted from `AGENT_SKILLS.md`. Canonical governance remains in the root document._

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
6. Mapping artifacts are generated or regenerated (e.g., `mapping/pixels.json`, `mapping/ledmap.json`, `include/generated/*.h`)
7. The mapping generator or wiring schema changes (e.g., `scripts/generate_ledmap.py`, `mapping/wiring*.json`)
8. Any “build hygiene” boundary is established or corrected (see Build Environment Hygiene)

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

### Required Format

Each log entry must include:

- Date (YYYY-MM-DD)
- Status marker (one of the allowed markers)
- Short title
- Clear description of what changed and why
- Files touched (paths)
- Notes / decisions (if any)

Allowed status markers:
- 🟢 Done
- 🟡 In progress
- 🔴 Blocked
- 🔵 Decision
- 🧪 Test added

**Proof-of-life requirement (when applicable):**
- If a validation step was performed (e.g., mapping generation, invariants tests, header regeneration),
  include either:
  - a 1–3 line **summary output** (recommended), OR
  - a small **checksum/hash** of a key generated artifact, OR
  - the exact command run + a brief “PASS” result line.

Do not paste large logs. Keep this short and high-signal.

---

### Explicit Prohibitions

The agent must **not**:

- Rewrite, reorder, or delete existing log entries
- Add speculative, narrative, or promotional language
- Use vague statements like “worked on X” without describing the actual change

---

### Example (Correct)

- 2026-01-03 🧪 Test added — Validate ledmap invariants

What was done:
- Added invariant tests for mapping artifacts: unique LED indices, no collisions, correct bounds

Files touched:
- test/test_mapping_invariants.cpp
- scripts/generate_ledmap.py

Notes / Decisions:
- Treat any collision as fatal; require regen via generator only

Proof-of-life:
- generate_ledmap.py: PASS (LED_COUNT=560, collisions=0)

---

### Example (Incorrect — Do Not Do This)

- "Worked on diagnostics"
- "Made progress on tests"
- "Refactored some code"

---
