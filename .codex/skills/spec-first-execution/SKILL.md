---
name: Spec-First Execution
description: Ensure changes are driven by the current specification and do not drift from the documented architecture.
version: 1
source: AGENT_SKILLS.md
---

# Spec-First Execution

_Extracted from `AGENT_SKILLS.md`. Canonical governance remains in the root document._

## Skill: Spec-First Execution

### Purpose

Ensure changes are driven by the current specification and do not drift from the documented architecture.

### Rules

- Before starting any implementation work, the agent must read (at minimum):
  - `docs/architecture/wled_integration_implementation_plan.md`
  - Any referenced schemas or generator docs mentioned in the plan
- If the task touches mapping, also read:
  - `mapping/README_wiring.md` (if present)
  - `scripts/generate_ledmap.py` (or current generator location)
- If a discovered repo reality conflicts with the plan, the agent must:
  - Document the discrepancy in `TASK_LOG.md` (per Task Log skill)
  - Propose a concrete resolution (do not silently reinterpret requirements)

### Why this matters

Without spec-first execution, agents tend to re-introduce earlier decisions (e.g., runtime JSON parsing) or diverge on core contracts (index ordering), creating costly rework.

---
