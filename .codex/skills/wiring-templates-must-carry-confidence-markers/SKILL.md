---
name: Wiring Templates Must Carry Confidence Markers
description: Prevent plausible-but-wrong wiring from being mistaken as authoritative, and make manual correction straightforward.
version: 1
source: AGENT_SKILLS.md
---

# Wiring Templates Must Carry Confidence Markers

_Extracted from `AGENT_SKILLS.md`. Canonical governance remains in the root document._

## Skill: Wiring Templates Must Carry Confidence Markers

### Purpose

Prevent plausible-but-wrong wiring from being mistaken as authoritative, and make manual correction straightforward.

### Rules

- When generating or updating `mapping/wiring.json` or `mapping/wiring_bench.json`:
  - Include `mappingVersion` and `isBenchSubset`.
  - Use `_comment` fields to guide manual editing.
  - For each segment entry, include a confidence marker in `_comment`, e.g.:
    - `CONFIDENCE: DERIVED from <file>:<symbol/line>`
    - `CONFIDENCE: PLACEHOLDER - requires physical validation`
- If generating a placeholder order, explicitly state that in the top-level `_comment` and in `mapping/README_wiring.md`.
- Ensure `mapping/README_wiring.md` includes a **Topology Key** (segment ID → vertex endpoints) so humans can identify physical tubes.

### Why this matters

Without explicit confidence markers and topology lookup, it’s easy to treat a template as “real wiring,” leading to wasted debugging time and incorrect ledmaps.

---
