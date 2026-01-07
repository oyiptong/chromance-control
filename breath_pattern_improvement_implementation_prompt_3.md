You are refining an existing engineering document titled:

**“Breathing Pattern (Mode 7) — Improvement Report + Implementation Plan (v2.2)”**

The document is already architecturally correct and implementation-ready.
Your task is to produce a **v2.4 refinement** that adds final safety guardrails, clarifies configuration, and removes remaining ambiguity — **without changing the core architecture**.

This is a refinement pass, not a redesign.

---

## Core Principles (must remain unchanged)

- Mode 7 is **topology-driven** and **event-driven**.
- INHALE is the only phase that generates paths.
- Paths are:
  - per-dot segment-simple (no segment reused within a dot),
  - downhill-preferred in `dist_to_center` (plateau-safe),
  - allowed to merge except where explicitly constrained.
- Center-lane policy governs the **final approach into the center**.
- Pauses are beat-count driven.
- EXHALE is wavefront-based and does not use path routing.
- Manual stepping (`n / N / ESC`) semantics remain unchanged.
- BFS distance computation remains **runtime on the MCU**.

Do not introduce new features beyond the refinements below.

---

## Required Refinements to Apply

### 1. Add a PAUSE fail-safe timeout

Strengthen the PAUSE phase so it cannot hang indefinitely if beat input is missing or fails.

Add an explicit guardrail:

- Define `max_pause_duration_ms` (or equivalent).
- If no beat events are detected for this duration:
  - inject a “virtual beat”, or
  - force phase completion and advance.

Requirements:
- This fallback must apply to both `PAUSE_1` and `PAUSE_2`.
- Beat-count logic remains the *primary* completion mechanism.
- The timeout is a safety net only, not the default behavior.

---

### 2. Generalize INHALE start selection (arbitrary count + deterministic default)

Refine INHALE start selection to support an **arbitrary number of dots**.

Define:

- `num_dots` = configurable number of inhale dots.
- If `num_dots` is not explicitly provided, default to `num_dots = 6`.

#### Start vertex selection logic

Replace fixed “boundary-start dots” language with the following:

1. Build a candidate pool of vertices **furthest from the center**:
   - Sort all vertices by descending `dist_to_center`.
2. Tie-break vertices with equal `dist_to_center` by:
   - ascending vertex degree (prefer lower degree).
3. Select the top `N_farthest` vertices (where `N_farthest ≥ num_dots`).
4. Randomly (but deterministically given seed) choose `num_dots` **distinct** start vertices from this pool:
   - no repeats within a single inhale,
   - selection order must be deterministic given the RNG seed.

This selection logic applies **only to INHALE**.

---

### 3. Make final-segment semantics explicit

Add an explicit statement:

> **A dot’s final segment is exactly one of `center_segments` (the assigned center lane).**

Clarify that:
- Every INHALE path terminates at the center vertex.
- The final edge traversed is the center lane selected during lane assignment.
- Center-lane constraints apply **only** to this final segment.

---

### 4. Lock down center-lane rotation semantics

Remove ambiguity by stating explicitly:

- Maintain `center_lane_rr_offset` as persistent effect state.
- Advance `center_lane_rr_offset` **whenever INHALE is (re)initialized**:
  - on automatic cycle transition into INHALE, or
  - on `ESC` reset to auto mode.
- Do **not** advance the offset on manual phase stepping (`n / N`).

This rule must be reflected consistently in:
- lane assignment description,
- determinism guarantees,
- tests.

---

### 5. Tighten fallback behavior for unreachable lanes

Refine fallback logic so it is bounded and deterministic:

- When routing a dot to its assigned center lane:
  - attempt each center lane **at most once** per dot per inhale.
  - try lanes in round-robin order starting from the assigned lane.
- If all lanes fail:
  - relax **only** the center-lane uniqueness constraint for that dot.
  - never relax per-dot segment simplicity.

Explicitly state this fallback is expected in rare cases and is not an error.

---

### 6. Remove remaining ambiguity in EXHALE distance definition

Lock down the rule:

- Define `seg_dist = min(dist[va], dist[vb])` as the **canonical** segment distance.
- Remove all alternative suggestions.

Briefly justify:
- produces monotone, center-anchored wavefront layering.

---

### 7. Normalize tail brightness language

Refine rendering language to:

- Replace “exponential brightness falloff” with **“perceptual brightness LUT”**.
- Clarify:
  - brightness values are hand-tuned or LUT-based,
  - RGB remains constant per dot,
  - only brightness varies along the tail.

This is a wording refinement only.

---

### 8. Preserve and reinforce determinism guarantees

Ensure the document explicitly states that:

- INHALE behavior (start selection, lane assignment, routing, fallback) is deterministic given:
  - the same RNG seed,
  - the same topology,
  - the same center selection,
  - the same `center_lane_rr_offset`.

Retry and fallback logic must not introduce nondeterminism in tests.

---

## Output Requirements

- Produce a **single refined plan (v2.4)**, not a diff.
- Preserve overall structure and numbering where possible.
- Replace hard-coded “6 boundary starts” language with the generalized mechanism.
- Remove all remaining “or” / ambiguous phrasing.
- The document should read as **final, unambiguous, and ready to implement**.

---

End of refinement prompt.

