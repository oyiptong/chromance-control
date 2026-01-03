# AGENTS.md — Chromance / WLED Integration

This file is the entrypoint for coding agents working in this repo.

## Read-first (mandatory)
Before doing any work, read these in order:

1) `docs/architecture/wled_integration_implementation_plan.md`
2) `AGENT_SKILLS.md` (canonical governance + process rules)
3) If the task touches mapping:
   - `mapping/README_wiring.md` (if present)
   - `scripts/generate_ledmap.py` (or current generator location)

If anything conflicts with the implementation plan, do not silently reinterpret.
Log the discrepancy in `TASK_LOG.md` and propose a resolution.

## Codex Skills
Agents should load and apply the skills found in:
- `.codex/skills/*/SKILL.md`

These are extracted from `AGENT_SKILLS.md` and exist to improve discovery and reuse.
