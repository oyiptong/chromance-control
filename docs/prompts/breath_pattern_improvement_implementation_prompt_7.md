You are refining an existing engineering document titled:

**“Breathing Pattern (Mode 7) — Improvement Report + Implementation Plan (v2.6)”** :contentReference[oaicite:0]{index=0}

Your task is to produce a **v2.7 refinement** that:
1) exposes key speed/timing parameters for future tuning via a web UI,  
2) adds a configuration table,  
3) adds generator test coverage,  
4) incorporates a concise memory budget + hard caps,  
while keeping the core architecture unchanged and moving us toward implementation quickly.

This is a refinement pass (spec hardening + tunability), not a redesign.

---

## Core Principles (must remain unchanged)

- Mode 7 is topology-driven and event-driven.
- Runtime BFS remains on the MCU.
- Center selection fallback (minimax eccentricity / “graph center”) remains as-is; explicitly note that O(V³) is acceptable because V≈25.
- INHALE is the only phase that generates paths.
- EXHALE completion remains vertex-based using `outermost_vertices`.
- Manual controls remain:
  - `n/N` for phase stepping,
  - `s/S` for INHALE-only lane iteration,
  - `ESC` back to auto.
- No dynamic allocation in firmware paths.

Do not change completion logic, routing rules, lane policy, or determinism requirements except as needed to expose parameters.

---

## Required Refinements to Apply

### 1) Introduce configurable parameters for tuning (web UI-ready)

Expose the following as named configuration parameters (with units, default, range, and purpose):

**INHALE**
- `dot_speed_leds_per_ms` (float) — dot head advance rate along LED path.
- `tail_length_leds` (uint8) — tail length in LEDs.
- `tail_brightness_lut[]` (fixed small LUT) — brightness-only falloff (can remain compiled-in for now, but document it).
- Optional: `dot_count` already exists as `num_dots` (ensure it is included in the config table).

**EXHALE**
- `wave_speed_layers_per_ms` (float) — how fast `cur_layer` advances across distance layers.
- `target_waves` (uint8) — number of wavefronts required for completion (already present; include in config table).
- Optional: `exhale_band_width` (float 0..1 or in layers) if you want tunable “band emphasis” later; keep minimal and clearly labeled “visual only”.

**PAUSE**
- `max_pause_duration_ms` (uint32) — beat fail-safe timeout.
- `beats_target_min` / `beats_target_max` (uint8) — default [3..7], now explicitly configurable.

Requirements:
- These parameters must be described as **intended to be adjustable via a web UI**.
- Add a brief note: “later we will store these persistently” (do not design persistence now; just note direction).
- Clarify how parameters are read at runtime (e.g., from a `Mode7Config` struct or similar), without prescribing implementation details beyond “must be settable”.

---

### 2) Add a configuration table

Add a clear Markdown table under §11 (or a new §11.1) listing at least:

| Parameter | Default | Range | Units | Purpose |
|---|---:|---:|---|---|

Include:
- `CENTER_VERTEX_ID`
- `num_dots`
- `N_farthest`
- `dot_speed_leds_per_ms`
- `tail_length_leds`
- `target_waves`
- `wave_speed_layers_per_ms`
- `beats_target_min/max`
- `max_pause_duration_ms`

Ensure defaults are consistent with the narrative text.

---

### 3) Memory budget + hard cap enforcement (spec-level)

Add a short “Memory Budget + Caps” subsection (can be in §14):

- Give approximate RAM/flash footprint for:
  - topology arrays (vertex coords, segment endpoints)
  - optional `seg_leds_forward/reverse` caches
  - per-dot path storage (vertex paths, LED paths)
  - EXHALE vertex tracking arrays

- Define and enforce hard caps as firmware constants:
  - `MAX_VERTEX_PATH_LENGTH`
  - `MAX_LED_PATH_LENGTH`
  - `MAX_DOTS` (upper bound for `num_dots`)
  - `MAX_VERTICES` / `MAX_SEGMENTS` already implied by generated tables

Specify behavior when caps are exceeded (deterministic and safe):
- Abort/skip the dot, log warning, continue.
- INHALE completes when all non-failed dots are done (or define “failed dots are treated as done”).

Do not introduce dynamic allocation.

---

### 4) Generator test coverage (Python)

Add a “Generator tests (Python)” subsection to §10:

Include tests such as:
- Vertex stability: same SEGMENTS input ⇒ same vertex ordering and vertex IDs
- Endpoint validity: `seg_vertex_a/b` are valid vertex IDs
- Completeness: every SEGMENTS endpoint appears in vertex list
- Coordinate consistency: emitted `(vx,vy)` match SEGMENTS endpoints
- Optional bench/partial coverage: generated topology supports “present segment” filtering without invalid indices

State where these tests live (e.g., `test/scripts/test_generate_ledmap.py`) and how they run.

---

### 5) Explicitly state center selection complexity is acceptable

Add a short note in §4.3:

- “Minimax eccentricity is O(V³) but V≈25; this is not performance-sensitive on ESP32 and is run infrequently (only on center fallback / init).”

This prevents future bikeshedding.

---

### 6) Update implementation checklist to mention UI tunables (without designing UI)

In §10 “Files to Change” / checklist, add a note:

- “Expose Mode 7 tunable parameters in a config surface that can later be wired to web UI controls; persistence will be added later.”

Do not propose actual UI code.

---

## Output Requirements

- Produce a **single refined plan (v2.7)**, not a diff.
- Preserve structure and numbering where possible.
- Ensure parameter names/units are consistent across:
  - narrative sections (§6, §7, §8),
  - configuration table (§11),
  - tests (§10),
  - memory/caps (§14).
- Keep additions concise and implementation-oriented.

---

End of refinement prompt.

