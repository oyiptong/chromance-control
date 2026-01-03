---
name: Mapping Invariants Gate
description: Guarantee that any mapping-related change is structurally correct before it reaches firmware/WLED, preventing silent mis-maps.
version: 1
source: AGENT_SKILLS.md
---

# Mapping Invariants Gate

_Extracted from `AGENT_SKILLS.md`. Canonical governance remains in the root document._

## Skill: Mapping Invariants Gate

### Purpose

Guarantee that any mapping-related change is structurally correct before it reaches firmware/WLED, preventing silent mis-maps.

### Required invariants

**Wiring invariants (full wiring):**
- 4 strips present
- Segment IDs `{1..40}` appear **exactly once** total
- `dir` is only `a_to_b` or `b_to_a`
- `isBenchSubset` is `false`

**Wiring invariants (bench wiring):**
- `isBenchSubset` is `true`
- Segment IDs are a strict subset of `{1..40}` with **no duplicates**
- Strip count may be 1 (typical) or more (optional), but must be explicit

**Generated mapping invariants:**
- LED indices cover `0..LED_COUNT-1` exactly once
- No coordinate collisions (no two LEDs share the same raster cell)
- For ledmap: `map.length == width * height` and only `-1` or valid indices appear
- Generated header tables share the same `LED_COUNT` and align by index:
  - `rgb[i]` ↔ `pixel_x/y[i]` ↔ `global_to_strip/local[i]`

### Validation actions (must run on mapping-related changes)

- Run generator validation (or equivalent) and ensure it completes without errors (no collisions, correct counts).
- Run native/unit tests that assert wiring and mapping invariants (if present).
- If tests are not yet implemented, add a TODO entry in `TASK_LOG.md` and treat mapping outputs as provisional until invariants are enforced by tests.

### Why this matters

Mapping bugs often look like effect bugs. Enforcing invariants early prevents long debug cycles and protects WLED 2D mapping correctness.

---
