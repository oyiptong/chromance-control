
u are refining an existing engineering document titled:

**“Breathing Pattern (Mode 7) — Improvement Report + Implementation Plan (v2)”**

The document is already architecturally correct and implementation-ready.  
Your task is to produce a **v2.2 refinement** that updates the INHALE path-generation rules to incorporate **center-segment uniqueness with round-robin fallback**, while keeping the rest of the design intact.

This is a refinement pass, not a redesign.

---

## Core Principles (must remain unchanged)

- Mode 7 is **topology-driven** and **event-driven**.
- INHALE is the only phase that generates paths.
- Paths are:
  - per-dot segment-simple (no segment reused within a dot),
  - downhill-preferred in `dist_to_center` (plateau-safe),
  - allowed to merge except where explicitly constrained.
- Pauses are beat-count driven.
- EXHALE is wavefront-based and does not use path routing.
- Manual stepping (`n / N / ESC`) semantics remain unchanged.

Do not introduce new features outside the scope below.

---

## Required Refinement: Center-Segment (“Center Lane”) Policy

### 1. Replace boundary-local disjointness with center-segment preference

Update the INHALE path distinctness logic as follows:

- Remove the requirement for **boundary-local global disjointness** (K steps or outer-zone rules).
- Introduce a **center-segment uniqueness preference** instead.

Define:

- `center` = chosen graph-center vertex.
- `center_segments` = all segments incident to `center`.

New policy:

- **Primary goal:** each center-incident segment is used by at most one dot during a single INHALE, *when possible*.
- **If impossible:** reuse is allowed, but which center segment is reused must **rotate across inhales** (round-robin), so the same segment is not consistently overloaded.

This policy applies **only to the final approach into the center** during INHALE.

---

### 2. Add explicit center-lane assignment step

Refine the INHALE algorithm to include:

- At inhale start, assign each dot a **preferred center lane** (i.e., a target neighbor `u` of the center, corresponding to segment `u → center`).
- Assignment rules:
  - Prefer unique center segments up to `deg(center)`.
  - If number of dots exceeds available center segments, assign remaining dots by cycling through center segments.
  - Maintain a persistent **round-robin offset** across inhales to rotate overload evenly over time.

Clarify that:
- The round-robin offset advances once per inhale cycle (or per auto reset).
- Assignment must be deterministic given the same seed and offset.

---

### 3. Route-to-target path generation

Update path generation description:

- Instead of “generate a path to center,” generate:
  - a path from boundary start vertex to the assigned **center-adjacent vertex** `u`,
  - then append the final segment `u → center`.

Constraints remain:
- per-dot no segment reuse,
- downhill-preferred in `dist_to_center`,
- plateau-safe.

---

### 4. Define fallback behavior if routing to a lane fails

Add a small, explicit fallback strategy:

- If a dot cannot reach its assigned center-adjacent vertex `u` under constraints:
  - try alternate center segments in round-robin order,
  - if all center segments fail, relax **only** the center-segment uniqueness constraint for that dot.
- Per-dot segment reuse must never be relaxed.

This ensures “use each center segment once **if possible**” without deadlock.

---

### 5. Clarify scope and phase separation

Add a short clarification note:

- Center-segment uniqueness and rotation apply **only to INHALE path generation**.
- EXHALE and PAUSE phases do not use or depend on path distinctness.

---

### 6. Update tests and determinism notes

Update the test plan to include:

- verification that:
  - center segments are used uniquely when `deg(center) ≥ num_dots`,
  - reuse rotates across inhales when `deg(center) < num_dots`,
- deterministic behavior given:
  - the same RNG seed,
  - the same center,
  - the same round-robin offset.

Do not add heavy graph-theoretic solvers or global optimization.

---

## Output Requirements

- Produce a **single refined plan (v2.2)**, not a diff.
- Preserve structure and numbering where

