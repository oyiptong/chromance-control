You are refining an existing engineering document titled:

**“Breathing Pattern (Mode 7) — Improvement Report + Implementation Plan (v2.4)”** :contentReference[oaicite:0]{index=0}

Your task is to produce a **v2.5 refinement** that adds a dedicated **INHALE-only lane iteration control** so we can cycle center lanes while staying in INHALE, without using `n/N` (which are reserved for phase stepping).

This is a refinement pass, not a redesign.

---

## Core Principles (must remain unchanged)

- Mode 7 is topology-driven and event-driven.
- INHALE is the only phase that generates paths.
- `n` / `N` remain **phase stepping controls** (INHALE ↔ PAUSE_1 ↔ EXHALE ↔ PAUSE_2) in manual mode.
- `ESC` returns to auto mode and restarts the cycle.
- Center-lane policy governs only the **final approach segment into center**.
- Runtime BFS remains on the MCU.
- Determinism requirements remain: same seed + same topology + same center + same rr_offset => same assignments.

Do not change any other behaviors unless explicitly requested below.

---

## Required Refinements to Apply

### 1) Add new INHALE-only “lane step” control

Introduce a new manual control key:
- `s`: **advance** the center-lane round-robin offset by +1 (wrap modulo `lane_count`) and **reinitialize INHALE** (new start selection + lane assignment + routing + LED path cache refresh as needed).
- Optional but recommended: `S` (capital): **reverse** the offset by -1 (wrap modulo `lane_count`) and reinitialize INHALE.

Constraints:
- `s/S` must only be active when:
  - manual mode is enabled **and**
  - current phase is `INHALE`.
- In any other phase, `s/S` should be ignored (or logged as no-op).

This control exists so we can iterate lanes without stepping through PAUSE/EXHALE.

---

### 2) Update center-lane rotation semantics to reflect `s/S`

Refine §5.3.1 and §9.* to state explicitly:

- `center_lane_rr_offset_` is a **persistent member variable** of the Mode 7 effect instance (not a local/static).
- The offset advances in these cases:
  - Auto: when INHALE is initialized via automatic cycle transition into INHALE, or via `ESC` reset to auto.
  - Manual: when `s` is pressed in `INHALE` (advance), or `S` is pressed in `INHALE` (reverse).
- The offset **does not** advance on manual phase stepping (`n/N`), since those are reserved for phase navigation.

Also add explicit wrap-around behavior:
- `offset = (offset + 1) % lane_count` for `s`
- `offset = (offset + lane_count - 1) % lane_count` for `S`

---

### 3) Define reinitialization scope for `s/S`

When `s/S` triggers an INHALE reinit, specify that it must:
- recompute `dist_to_center` if needed (center or active subgraph unchanged, so this may be reused; clarify “reuse allowed”)
- re-run start selection from the farthest pool (deterministic RNG)
- re-run center-lane assignment using the updated offset
- regenerate per-dot vertex paths and LED index paths

Clarify that:
- `s/S` is intended as a debugging/iteration tool and should be deterministic given the same seed + keypress sequence.

---

### 4) Update runtime control documentation + file checklist

Update the “Runtime controls” section and “Files to Change” to include:
- adding key handling for `s/S` to the relevant runtime input layer (same place `n/N/ESC` are handled)
- wiring that key to “reinitialize INHALE with offset update” behavior in Mode 7

Do not change existing `n/N/ESC` behavior.

---

### 5) Update tests to cover `s/S`

Add deterministic native tests:
- In manual mode, phase=INHALE:
  - `s` increments offset modulo lane_count and triggers new lane assignments
  - `S` decrements offset modulo lane_count and triggers new lane assignments
- In other phases:
  - `s/S` do not affect offset or phase
- Verify wrap-around (“circles back to the beginning”) behavior.

---

## Output Requirements

- Produce a **single refined plan (v2.5)**, not a diff.
- Preserve overall structure and numbering where possible.
- Ensure all references to round-robin advancement are consistent across:
  - §5.3.1 (lane assignment),
  - §9.2 (manual stepping),
  - §11 (defaults/guardrails),
  - tests.

---

End of refinement prompt.

