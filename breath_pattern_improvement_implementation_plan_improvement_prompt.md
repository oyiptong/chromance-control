You are reviewing and revising an existing engineering plan titled:

**“Breathing Pattern (Mode 7) — Improvement Report + Implementation Plan”**

Your task is to **produce a refined v2 version of the plan**, not commentary.  
Preserve the plan’s professional tone, structure, and depth.

---

## Core Intent (must remain intact)

The goal is to upgrade Mode 7 (“Breathing”) from a geometry-heuristic, time-driven animation into a **topology-driven, event-driven breath loop** with the following properties:

- Inhale: multiple dots originate from boundary topology and travel inward along valid graph paths.
- Paths respect topology (segments + vertices), not geometric projection.
- Motion is smooth (fixed-point position, brightness-only tails).
- Pauses are **beat-count driven**, not time-based.
- Exhale completion is **discrete and measurable**, not continuous heuristics.
- Manual stepping (`n`, `N`, `ESC`) remains consistent and deterministic.

---

## Required Improvements to Incorporate

### 1. Clarify the “6 distinct paths” constraint (critical)

Revise the plan to explicitly **reject full global edge-disjointness** as a default requirement.

Incorporate these principles:

- **Per-dot segment reuse is always forbidden** (no dot may traverse the same segment twice).
- **Global segment uniqueness is enforced only near the boundary**, not globally.
  - Enforce global disjointness for the first **K steps from the boundary** (K configurable, default 2; allowed 1–3).
  - Allow paths to merge in later stages (near the center).
- Explicitly document *why*:
  - Graph bottlenecks and cut constraints may make full global disjointness impossible.
  - Visual merging near the center is acceptable and organic.

Update Section 5.3 (“Uniqueness across 6 paths”) accordingly.

---

### 2. Center and arrival semantics

Refine the center logic to:

- Use **graph center** (minimax BFS distance) as the canonical reference.
- Explicitly handle tie-breaking (deterministic; specify a stable rule such as lexicographic `(vx,vy)` or stable vertex index).
- Clarify that dots may:
  - either arrive at the center vertex **with shared final segments**, or
  - arrive at **center-adjacent vertices** if desired.

Do **not** require unique final segments unless explicitly enabled.

---

### 3. Boundary definition (make explicit)

Revise boundary selection to avoid relying solely on inference:

- Default heuristic: `degree == 2` vertices.
- Acknowledge limitation and note future improvement:
  - Prefer generator-emitted boundary flags when available.
- Keep current heuristic acceptable for v1.

---

### 4. Generator pipeline refinement (Section 4.1)

Keep the strong recommendation to emit **segment endpoint arrays** from the Python generator.

Clarify the tradeoff for additional data:

- **Must emit**:
  - segment endpoints `(va, vb)`
- **Optional / nice-to-have**:
  - adjacency lists
  - precomputed BFS distances

Explicitly state:
- Runtime BFS on the MCU is acceptable due to small graph size.
- Precomputing BFS in Python is an optimization, not a requirement.

---

### 5. Inhale path generation algorithm (tighten wording)

Refine the algorithm to emphasize a **“downhill-preferred walk”**:

- Prefer neighbors with strictly lower `dist_to_center`.
- Allow equal-distance moves **only** when plateau-safe:
  - the next vertex must have at least one strictly-downhill exit.
- Explicitly forbid cycles.

Make this the canonical description.

---

### 6. Rendering refinement (tails)

In the rendering section:

- Keep fixed-point `p16` motion.
- Replace “linear tail brightness” examples with **perceptual / exponential falloff**.
- Clarify:
  - RGB is constant per dot.
  - Brightness falloff may use LUT or hand-tuned exponential values.
  - Full gamma correction is optional.

---

### 7. Exhale wavefront robustness

Strengthen the exhale section to explicitly handle frame-skip edge cases:

- Ensure wavefront accounting cannot skip a segment if radius advances quickly.
- Mention one of:
  - iterating through all crossed distance layers per frame, or
  - layer-based wave advancement.

Reaffirm reuse of `dist_to_center` / `seg_dist` for both inhale and exhale.

---

### 8. Tests and determinism

Explicitly require:

- Deterministic RNG seeding for tests.
- Tests covering:
  - monotonicity of paths
  - per-dot segment non-reuse
  - boundary-local disjointness (first K steps)
  - boundary uniqueness
  - phase completion conditions
  - exhale wavefront counts

---

## Output Requirements

- Produce a **single revised plan** (v2), not a diff.
- Keep section numbering and overall structure similar where possible.
- Make all constraints and relaxations **explicit**, not implied.
- Do not add speculative features beyond the scope above.
- Write as if this document will be used directly for firmware implementation.
- Prefer defaults over open questions; leave open questions only where truly necessary.

---

End of prompt.
