# Breathing Pattern (Mode 7) — Improvement Report + Implementation Plan (v2)

Goal: upgrade Mode 7 (“Breathing”) from a geometry-heuristic animation into a topology-driven, event-driven breath loop:
- 6 boundary-start dots (distinct boundary vertices) travel inward toward the center via topology edges (segments), preferring monotone progress in `dist_to_center`, never traversing a segment twice (per-dot), with smooth motion (tail, brightness-only).
- Path “distinctness” is **local-to-boundary by default**: enforce global segment disjointness only for the first **K** steps from the boundary (K configurable, default 2; allowed 1–3). Paths may merge organically near the center.
- Pauses are beat-count driven (3–7 beats), with crossfade progress tied to beat count (future: beat signal).
- Exhale ends when **all outer edges** have received **7 wavefronts** (tracked per outer segment).
- Manual stepping remains consistent: `n` next stage, `N` previous stage, `ESC` back to auto.

This document is a plan only: no firmware changes are made here.

---

## 1) Current State (What exists today)

### 1.1 Current Mode 7 structure (time-driven)
Mode 7 is implemented as a time-based cycle in `src/core/effects/pattern_breathing_mode.h`, where `now_ms - cycle_start_ms_` determines the active phase, and durations are hard-coded.

Relevant excerpt (current pattern is time-based):
```cpp
const uint32_t t = frame.now_ms - cycle_start_ms_;
const uint32_t t_mod = t % kCycleMs;

if (t_mod < kInhaleMs) { ... }
else if (tt < kPauseMs) { ... }
else if (tt < kExhaleMs) { ... }
else { ... }
```

The visuals are currently “geometric”: LEDs are assigned to one of 6 spokes using dot products against fixed direction vectors, and distance from center is approximated by a continuous projection (`pos_[]`).

Relevant excerpt (current “spoke” assignment is geometric, not topology-driven):
```cpp
// Define 6 spoke directions (unit vectors around the circle).
const float dirs_x[kSpokes] = {1.0f, 0.5f, -0.5f, -1.0f, -0.5f, 0.5f};
const float dirs_y[kSpokes] = {0.0f, 0.8660254f, 0.8660254f, 0.0f, -0.8660254f, -0.8660254f};

for (uint16_t i = 0; i < led_count; ++i) {
  const float dx = static_cast<float>(x - cx);
  const float dy = static_cast<float>(y - cy);
  float best = -1e9f;
  uint8_t best_s = 0;
  for (uint8_t s = 0; s < kSpokes; ++s) {
    const float dot = dx * dirs_x[s] + dy * dirs_y[s];
    if (dot > best) { best = dot; best_s = s; }
  }
  spoke_[i] = best_s;
  pos_[i] = best;   // “distance along spoke” proxy
}
```

### 1.2 Current inhale/exhale primitives (heuristic, not topology)
Mode 7 currently renders:
- a “dots” effect (single bright pixel chosen by nearest `pos_` to target),
- a “wave/plasma” effect (sinusoid over `u` and time),
- pause heartbeat using an envelope derived from `now_ms`.

### 1.3 Runtime controls (already consistent for mode 7)
`src/main_runtime.cpp` already supports:
- `n` next stage (manual)
- `N` previous stage (manual)
- `ESC` reset to auto

These controls are now consistent across modes 2/6/7, so Mode 7 can reuse the same semantics.

---

## 2) Problems to Solve (Why the current effect isn’t meeting requirements)

### 2.1 Inhale stutter
The inhale “dot” appears stuttery because it snaps to discrete LEDs (nearest LED to a moving target) and renders no subpixel/tail smoothing. Even at 60fps, a single pixel on black will “jump” visually.

Relevant excerpt (current inhale/exhale dot is a single nearest LED):
```cpp
const float target = outward ? (phase * max_pos_[dot_index]) : ((1.0f - phase) * max_pos_[dot_index]);
uint16_t best_i = farthest_led_[dot_index];
float best_d = 1e9f;
for (uint16_t i = 0; i < n; ++i) {
  if (spoke_[i] != dot_index) continue;
  const float d = fabsf(pos_[i] - target);
  if (d < best_d) { best_d = d; best_i = i; }
}
out[best_i] = c;
```

### 2.2 “From the edge” is not topology-correct
Current code picks a “spoke” by geometric projection; it does not follow **connected segments** from a chosen boundary vertex to the center. Therefore it cannot guarantee:
- start points are truly on topology boundary vertices,
- travel follows actual segment connectivity,
- monotone progress toward center by topology distance,
- “no segment traversed twice”.

### 2.3 Exhale completion condition is not representable as-is
Requirement: “count all outer edges having received 7 wavefronts”.
Current “wave” effect is a continuous field; it doesn’t have a notion of:
- outer segments,
- discrete wavefront ids,
- “this segment received wavefront k”.

### 2.4 Pauses are time-based and can feel “jarring” when isolated
Requirement: pauses should be cyclical and later driven by a beat signal, with beat-count-based completion and crossfade tied to beat count.
Current code derives pulse phase and stop condition from time, which makes manual/isolated execution feel discontinuous.

---

## 3) Target Behavior (Spec)

Mode 7 is an event-driven state machine with stages in this order:
1) `INHALE`: 6 distinct dots traverse inward to center; stage ends when all reach center.
2) `PAUSE_1`: heartbeat pulse for `beats_target` in [3..7], crossfade inhale→exhale across those beats.
3) `EXHALE`: outward wavefronts; stage ends when every outer segment has received 7 wavefronts.
4) `PAUSE_2`: heartbeat pulse for `beats_target` in [3..7], crossfade exhale→inhale across those beats.

Manual control:
- `n`: advance to next stage and stay there (manual mode)
- `N`: previous stage and stay there (manual mode)
- `ESC`: return to auto mode (restart cycle)

Visual constraints:
- Dot tail uses **constant RGB** per dot; **brightness-only** falloff along tail.
- Dots originate at 6 distinct topology boundary vertices (default: `degree == 2`) chosen randomly at inhale start.
- Paths must be cycle-free and must not traverse any segment twice (per-dot).
- Paths should be “downhill-preferred” in `dist_to_center`:
  - prefer strictly decreasing `dist_to_center` moves,
  - allow equal-distance moves only when plateau-safe (see §5.2).
- Dots may converge near the center and may share final segments unless explicitly configured otherwise.

---

## 4) Data Model Needed in Firmware

### 4.1 Required topology in C++ (vertex graph)
To follow “connecting segments”, firmware needs the segment endpoints in vertex space:
- Each segment `segId` corresponds to an undirected edge between vertices `(vx,vy)` endpoints `(A,B)`.

Today this segment list exists in Python (`scripts/generate_ledmap.py::SEGMENTS`) and in docs (`docs/architecture/wled_integration_implementation_plan.md` / `mapping/README_wiring.md`), but **not as a reusable C++ structure**.

#### Recommended approach (generator-driven)
Extend the mapping header generator (`scripts/generate_ledmap.py --out-header`) to emit, at minimum:
- `constexpr uint8_t SEGMENT_COUNT = 40;`
- `constexpr int8_t seg_va_x[SEGMENT_COUNT+1]`, `seg_va_y[...]`
- `constexpr int8_t seg_vb_x[...]`, `seg_vb_y[...]`

Rationale:
- Avoid duplicating the SEGMENTS list in C++ by hand.
- Ensure breathing mode always matches the generator’s canonical topology.

Optional / nice-to-have generator output (not required for v2):
- per-vertex adjacency lists
- precomputed BFS distances (e.g., `dist_to_center` for the chosen center)

Runtime BFS on the MCU is acceptable here due to the small graph size; precomputing distances in Python is an optimization only.

Then add C++ accessors (portable) in core, e.g.:
- `core/mapping/mapping_tables.h` gains accessors for these arrays when present.

Fallback (if we don’t want to touch generator yet):
- Add a small `src/core/topology/segments.h` with the SEGMENTS list copied from the plan.
- Risk: drift vs generator; mitigated with a host test that asserts equivalence (but that also requires parsing the generator list).

### 4.2 Boundary vertices and “outer segments”
Compute from the vertex graph:
- Vertex degree map: `deg[v]`
- Boundary vertex set (default heuristic): `deg[v] == 2`
- Outer segment set: segments incident to at least one boundary vertex.

Notes:
- `deg==2` is a pragmatic heuristic for v2 and may be imperfect on some topologies.
- Future improvement (preferred): have the generator emit explicit boundary flags per vertex/segment so firmware doesn’t need to infer.

### 4.3 Center definition
Recommended: “graph center” vertex.
- Compute BFS distances from each vertex to all others (graph is tiny: ~25 vertices).
- Choose the vertex minimizing the maximum distance to all vertices.
- Deterministic tie-break: among ties, pick the vertex with the smallest `(vx, vy)` lexicographically (or a fixed stable vertex index if you index vertices deterministically).

Arrival semantics:
- Default: dots target the chosen `center` vertex.
- Optional mode (if desired): allow “center-adjacent arrival”, where a dot can terminate at any vertex with `dist_to_center == 1` (useful if the true visual “center” is a small cluster).
- Do not require unique final segments unless explicitly enabled; shared final segments/vertices are allowed.

Alternative: geometric center of `(vx,vy)`, but graph-center is better for monotone `dist_to_center`.

### 4.4 Distance to center
BFS from `center` over the vertex graph:
- `dist[v] = shortest number of edges to center`

This is the monotonic constraint reference.

---

## 5) Inhale: 6 Unique Monotone Paths (No segment reused)

### 5.1 Path generation overview
At inhale start:
1) Choose 6 distinct boundary start vertices randomly.
2) For each start, generate a vertex path to center subject to constraints:
   - downhill-preferred in `dist_to_center` (strictly decreasing preferred; plateau-safe equals allowed)
   - no segment traversed twice (per-dot; always enforced)
   - global disjointness enforced only near the boundary for the first K steps (default K=2; configurable 1–3)
3) Convert each vertex path into an LED index path by expanding each segment step to its 14 LEDs.

### 5.2 Constraints and selection rules
For a dot at vertex `v`:
- Valid neighbor `u` must satisfy:
  - `dist[u] <= dist[v]`  (no backtracking)
  - segment `segId(v,u)` not already used in this dot’s path
  - `u` not previously visited (required; prevents cycles on plateaus)

Plateau safety (recommended):
- If choosing `dist[u] == dist[v]`, only allow if `u` has some neighbor `w` where `dist[w] < dist[u]` and the segment `segId(u,w)` is not used (so you don’t get trapped).

Canonical selection rule (“downhill-preferred walk”):
1) Enumerate candidate neighbors that pass the validity rules above.
2) Prefer candidates with `dist[u] < dist[v]`.
3) Only consider `dist[u] == dist[v]` moves if plateau-safe (as defined above).
4) Apply additional constraints/preferences in §5.3 (boundary-local disjointness).

### 5.3 Uniqueness across the 6 paths
Default requirement (v2): **reject full global edge-disjointness**. Do not require all six inhale paths to be globally segment-unique end-to-end.

Required uniqueness (always enforced):
- **Per-dot**: a dot may not traverse the same segment twice.
- **Boundary-local global**: enforce global segment uniqueness only for the first **K** steps from the boundary (K configurable, default 2; allowed 1–3).

Why boundary-local disjointness only:
- True global edge-disjointness can be impossible in graphs with bottlenecks/cuts (and can cause frequent “reroll until it works” behavior).
- Visually, merging near the center is acceptable and often looks more organic.

Implementation approach:
- Maintain `used_segments_global_prefix` that is consulted only while the dot is within its first K steps from the boundary.
- Generation can be sequential: generate path0..path5, updating `used_segments_global_prefix` as you commit each path’s first K segments.
- Outside the prefix window, you may still **prefer** unused segments to reduce overlap, but it is not required.

### 5.4 LED path expansion
Once we have an ordered list of segments with direction, for each segment step:
- Append the 14 global indices for that segment in the correct direction.

This requires a reliable mapping:
- from `segId` to global indices (available via generated tables `global_to_seg` and `global_to_seg_k`)
- and direction for that segment traversal (A→B vs B→A in topology space), which may not match wiring’s `dir`.

Recommendation:
- Build a per-segment cache at startup:
  - `seg_leds_forward[segId][k] = global_index` for k=0..13 representing A→B direction in topology space.
  - `seg_leds_reverse[...]` for B→A.

This uses `global_to_seg` and `global_to_seg_k` plus the canonical A/B definition.

---

## 6) Smooth Dot Rendering (Brightness-only tail, constant color)

For each dot:
- Keep a fixed-point position along its LED path: `p16` (e.g., 16.16 fixed).
- Advance `p16` by `speed * dt_ms` each frame.
- Render:
  - head at `idx = floor(p)`
  - tail behind it of length `L` (e.g., 4–6)
  - perceptual/exponential brightness falloff (LUT or hand-tuned values), e.g. `[255, 170, 110, 70, 45, ...]`
  - **same RGB** for head+tail; only brightness changes.

This eliminates stutter without changing the physical path.

---

## 7) Exhale: “7 wavefronts received by all outer segments”

### 7.1 Discrete wavefront model
Represent wavefronts as repeated outward sweeps over “distance layers” from center.

Precompute per segment:
- `seg_dist[segId] = min(dist[va], dist[vb])` (or `avg`, choose once and keep consistent)

Maintain:
- `uint8_t received[segId]` for outer segs (0..7)
- `uint8_t last_wave_id_seen[segId]` to avoid double counting per wave sweep

Define wave propagation:
- `wave_id` increments each time a sweep restarts.
- Advance the wave by **distance layers** to avoid frame-skip errors:
  - Maintain `prev_layer` and `cur_layer` for the current wave (integer distance units).
  - Each frame, compute `cur_layer` from accumulated progress (time-based or beat-based is fine for visuals).
  - For every crossed layer `L` in `(prev_layer, cur_layer]`, process all segments whose `seg_dist == L` and mark them “received” for `wave_id`.
  - This guarantees you cannot skip marking a segment even if `dt` is large or FPS drops.
- Use `last_wave_id_seen[segId]` to prevent double counting within a wave.

Termination:
- EXHALE ends when `min(received[outer_segments]) >= 7`.

### 7.2 Rendering during exhale
You can still render a smooth “wave field” visually, but the completion logic must be tied to the discrete wavefront tracker above.

Relevant excerpt (current exhale “plasma-like” look comes from a continuous sinusoid along `u` and time):
```cpp
const float phase = (u / wavelength) + (outward ? -1.0f : 1.0f) * (tt * speed);
const float w = 0.5f + 0.5f * sinf(6.2831853f * phase);
...
const float amp = w * env;
out[i] = scale(base, static_cast<uint8_t>(amp * frame.params.brightness));
```

To reduce the “plasma” look and make it feel like wavefronts:
- emphasize a narrow band around the current `radius`
- optionally add a subtle sinusoid inside the band

---

## 8) Pause Phases: Beat-count driven, cyclical, crossfade tied to beats

At pause start:
- Choose `beats_target = random_int(3..7)`
- Set `beats_completed = 0`
- Set `crossfade_phase = 0`

Beat events:
- “Now” implementation: synthesize beats at some nominal bpm using `now_ms`, but structure code around a `beat_event` boolean.
- “Later” implementation: use `Signals` (e.g., `signals.audio_beat` or add a dedicated beat signal) and increment on rising edges.

Crossfade:
- `fade = beats_completed / beats_target` (0..1), or a smoothstep variant.
- Use `fade` to interpolate colors (inhale→exhale for pause1, exhale→inhale for pause2).

Visual pulse envelope:
- On each beat event, trigger a short envelope `attack/decay` (cyclical, independent).
- The pause can run indefinitely without a “jarring stop”; completion is a separate condition (beat count).

---

## 9) State Machine Design (Event-driven)

### 9.1 Proposed per-mode state
Add a `Phase` enum:
- `INHALE`
- `PAUSE_1`
- `EXHALE`
- `PAUSE_2`

Keep:
- `phase_`
- `phase_start_ms_` (for per-phase local time only, not for completion)
- `manual_enabled_` and manual phase selection (already exists; adapt to new phases)

Phase completion conditions:
- INHALE: `all_dots_done == true`
- PAUSE_*: `beats_completed == beats_target`
- EXHALE: `all_outer_segments_received_7 == true`

### 9.2 Manual stepping interactions
When manual mode is enabled:
- `n` / `N` selects a phase and stays there.
- phase progression does not happen automatically.
- animations continue looping within the selected phase.
- `ESC` disables manual and resets to auto phase = INHALE with new random starts.

---

## 10) Files to Change (Implementation checklist)

### Core effect
- `src/core/effects/pattern_breathing_mode.h`
- `src/core/effects/pattern_breathing_mode.cpp`
  - Convert from time-driven to event-driven phase machine.
  - Add topology-driven inhale path generation + smooth dot rendering.
  - Add exhale wavefront accounting + outer segment detection.
  - Refactor pauses to beat-count-driven progression and cyclical envelope.

### Topology data availability (recommended)
- `scripts/generate_ledmap.py`
  - Emit segment endpoint arrays into `--out-header` output.
- `include/generated/chromance_mapping_*.h` (generated; do not hand-edit)
- `src/core/mapping/mapping_tables.h`
  - Expose generated segment endpoint arrays to core if present.

### Runtime wiring (controls already exist)
- `src/main_runtime.cpp`
  - No new controls required; mode 7 already has `n/N/ESC`.
  - Possibly add optional debug printing (dot start vertices, beat targets, exhale counts) behind a flag.

### Tests (native)
- `test/test_effect_patterns.cpp`
  - Add deterministic tests for:
    - inhale path monotonicity (`dist` non-increasing)
    - no segment reuse
    - 6 distinct boundary starts
    - boundary-local disjointness (first K segments are globally unique)
    - phase progression triggered by completion
    - pause beat-count completion and crossfade progress
    - exhale termination when wavefront counts reach 7 on all outer edges

Determinism requirements:
- Use a deterministic RNG seed in tests.
- Ensure path generation is deterministic given the same seed and the same topology inputs (including deterministic center tie-breaks).

---

## 11) Open Questions (Need confirmation)

1) Boundary heuristic:
   - confirm `degree==2` is acceptable as the default boundary definition for now, or specify a different boundary set.

2) Center-adjacent arrival:
   - confirm whether to keep the default “arrive at center vertex”, or enable “arrive at dist==1 ring” for visual reasons.

---

## 12) Suggested incremental rollout

1) Introduce segment endpoint arrays into generated headers (topology available in firmware).
2) Implement topology-based graph + `dist_to_center` computation in breathing mode.
3) Implement inhale path generation and smooth dot rendering (completion-driven).
4) Implement pause phases with beat-count completion and cyclical envelope.
5) Implement exhale wavefront accounting and completion condition; tune visuals.
6) Add native tests for invariants and deterministic behavior.
