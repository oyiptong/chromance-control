You are refining an existing engineering document titled:

**“Breathing Pattern (Mode 7) — Improvement Report + Implementation Plan (v2.5)”**

The document is already architecture-correct and nearly implementation-ready.
Your task is to produce a **v2.6 refinement** that resolves the remaining correctness and robustness issues so the plan can be implemented without further design iteration.

This is a refinement and hardening pass, **not a redesign**.

---

## Core Principles (must remain unchanged)

- Mode 7 is **topology-driven** and **event-driven**.
- INHALE is the **only phase** that generates paths.
- Paths are:
  - per-dot segment-simple (no segment reused within a dot),
  - downhill-preferred in `dist_to_center` (plateau-safe),
  - allowed to merge except where explicitly constrained.
- Center-lane policy governs **only the final approach into the center**.
- Pauses are beat-count driven with a fail-safe.
- EXHALE is wavefront-based.
- Runtime BFS remains on the MCU (no generator BFS required).
- Manual controls:
  - `n/N` = phase stepping
  - `s/S` = INHALE-only lane iteration
  - `ESC` = return to auto mode

Do not change these fundamentals.

---

## Required Refinements (Must Be Applied)

### 1. Replace EXHALE completion with vertex-based logic (topology-correct)

Remove dependency on “outer segments” or boundary degree heuristics for **EXHALE completion**.

Define:

- Compute `dist_to_center[v]` for all vertices in the **active subgraph**.
- Let `d_max = max(dist_to_center[v])`.
- Define:

outermost_vertices = { v | dist_to_center[v] == d_max }


Wavefront receipt tracking:
- Maintain `received_vertex[v]` (0..target_waves) for `v ∈ outermost_vertices`.
- Maintain `last_wave_id_seen_vertex[v]` to prevent double counting.
- When a wave crosses layer `L`, for each vertex `v` where `dist_to_center[v] == L`:
- if `last_wave_id_seen_vertex[v] != wave_id`:
  - increment `received_vertex[v]`
  - update `last_wave_id_seen_vertex[v]`

**EXHALE completion condition:**

min(received_vertex[v] for v in outermost_vertices) >= target_waves


Rendering may still be segment-based; this change applies **only to completion logic**.

---

### 2. Fix plateau safety with a goal exception (path correctness)

Refine §5.2 plateau safety:

Current rule blocks valid paths when the assigned center-adjacent goal vertex lies on a plateau.

Update plateau-safe definition:

- A move from `v` to `u` with `dist[u] == dist[v]` is allowed if:
  - `u` is the assigned center-adjacent goal vertex **OR**
  - `u` has an unused neighbor `w` with `dist[w] < dist[u]`.

This preserves safety while allowing valid goal reachability.

---

### 3. Add atomic beat processing to PAUSE fail-safe (no double increment)

Prevent double increments when a real beat and timeout occur in the same frame.

Refine PAUSE beat handling:

- At most **one beat event may be processed per frame**.
- Real beat events take precedence over virtual (timeout) beats.
- Beat handling must be centralized so that:
  - `beats_completed` increments once,
  - `last_beat_ms` updates once,
  - the pulse envelope triggers once.

This applies identically to `PAUSE_1` and `PAUSE_2`.

---

### 4. Clarify `s/S` determinism and RNG behavior

Make explicit that:

- Pressing `s` or `S` in manual INHALE:
  1. Updates `center_lane_rr_offset` (±1 modulo `lane_count`)
  2. Reinitializes INHALE
  3. Re-runs RNG-based start selection
  4. Reassigns lanes
  5. Regenerates paths

- RNG consumption is deterministic:
  - Same seed + same keypress sequence ⇒ same visual result.

Do **not** preserve prior start vertices when pressing `s/S`.

---

### 5. Add guardrail for `lane_count == 0`

Explicitly define behavior when the chosen `center` has **no present incident segments**
(e.g., in bench or partial builds):

- If `lane_count == 0`:
  - Skip center-lane assignment.
  - Attempt to route dots directly to `center` ignoring lane constraints.
  - If routing fails, INHALE may safely no-op or advance (document which).

This guardrail must prevent deadlock.

---

### 6. Add memory considerations (MCU safety)

Add a short section acknowledging memory impact:

- All topology tables and caches must be:
  - fixed-size,
  - statically allocated,
  - bounded by known maxima (`VERTEX_COUNT`, `SEGMENT_COUNT`, `num_dots`).
- Avoid dynamic allocation in INHALE path generation.
- Call out the largest allocations:
  - `seg_leds_forward/reverse`
  - per-dot LED index paths

This does not require exact byte counts, just explicit intent.

---

### 7. Expand test coverage (only additions, no removals)

Add tests to existing list:

- EXHALE vertex-based completion:
  - outermost vertices receive exactly `target_waves`
- Plateau goal exception:
  - path can reach center-adjacent goal on a plateau
- Beat fail-safe:
  - real beat + timeout in same frame increments once
- `s/S` determinism:
  - same seed + same `s/S` sequence ⇒ same paths
- `lane_count == 1`:
  - `s/S` wraps but effectively no-ops lane assignment
- `s/S` ignored outside INHALE

---

## Output Requirements

- Produce a **single refined plan (v2.6)**, not a diff.
- Preserve structure and numbering where possible.
- Remove any remaining ambiguity (“or”, “may”, “could”) where behavior must be deterministic.
- Ensure EXHALE completion, INHALE routing, PAUSE progression, and manual controls are all **closed systems** (no infinite waits).

The final document should read as **fully implementation-ready** with no unresolved correctness questions.

---

End of refinement prompt.

