# Breathing Pattern (Mode 7) — Improvement Report + Implementation Plan (v2.5)

Goal: upgrade Mode 7 (“Breathing”) from a geometry-heuristic animation into a topology-driven, event-driven breath loop:
- `num_dots` inhale dots travel inward toward the center via topology edges (segments), preferring monotone progress in `dist_to_center`, never traversing a segment twice (per-dot), with smooth motion (tail, brightness-only).
- Path “distinctness” uses a **center-lane policy**: prefer unique final center-incident segments per inhale, with round-robin rotation when reuse is unavoidable.
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

v2.5 refinement: add an INHALE-only “lane step” control (manual only):
- `s`: advance `center_lane_rr_offset` by +1 (wrap modulo `lane_count`) and reinitialize INHALE.
- `S`: reverse `center_lane_rr_offset` by -1 (wrap modulo `lane_count`) and reinitialize INHALE.

Constraints:
- `s/S` are active only when manual mode is enabled and phase is `INHALE`.
- In any other phase, `s/S` are ignored (no-op).

These controls keep `n/N` reserved for phase stepping while allowing rapid iteration over center lanes within INHALE.

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
- `num_dots` distinct start vertices are chosen per inhale using the “farthest-from-center pool” method (§5.1).
- Paths must be cycle-free and must not traverse any segment twice (per-dot).
- Paths should be “downhill-preferred” in `dist_to_center`:
  - prefer strictly decreasing `dist_to_center` moves,
  - allow equal-distance moves only when plateau-safe (see §5.2).
- Dots may converge near the center, but the final “center lane” segment is subject to the center-lane policy (prefer unique use per inhale when possible; otherwise reuse rotates across inhales).

---

## 4) Data Model Needed in Firmware

### 4.1 Required topology in C++ (vertex graph)
To follow “connecting segments”, firmware needs the segment endpoints in vertex space:
- Each segment `segId` corresponds to an undirected edge between vertices `(vx,vy)` endpoints `(A,B)`.

Today this segment list exists in Python (`scripts/generate_ledmap.py::SEGMENTS`) and in docs (`docs/architecture/wled_integration_implementation_plan.md` / `mapping/README_wiring.md`), but **not as a reusable C++ structure**.

#### Recommended approach (generator-driven; shared infrastructure for future patterns)
Extend the mapping header generator (`scripts/generate_ledmap.py --out-header`) to emit a **stable vertex list** and **per-segment endpoints** in terms of `vertex_id`.

This is not “Mode 7 specific”: once topology is emitted into `include/generated/chromance_mapping_*.h`, any future topology-aware pattern (vertex routing, hex fills, wavefronts, vertex diagnostics, etc.) can consume the same canonical data without copying topology into C++ by hand.

##### 4.1.1 Define vertex IDs (stable)
Emit:
- `constexpr uint8_t VERTEX_COUNT = ...;`
- `constexpr int8_t vertex_vx[VERTEX_COUNT];`
- `constexpr int8_t vertex_vy[VERTEX_COUNT];`

Generator stability rule:
- Build the unique vertex set from the canonical `SEGMENTS` list.
- Sort lexicographically by `(vx, vy)` and emit in that order.
- Define `vertex_id` as the index into `(vertex_vx, vertex_vy)`.

##### 4.1.2 Emit segment endpoints as vertex IDs
Emit, indexed by `segId` (1..40; index 0 unused):
- `constexpr uint8_t SEGMENT_COUNT = 40;`
- `constexpr uint8_t seg_vertex_a[SEGMENT_COUNT + 1];`
- `constexpr uint8_t seg_vertex_b[SEGMENT_COUNT + 1];`

Optional debug-friendly redundancy (not required once you trust vertex IDs):
- `seg_va_x/seg_va_y/seg_vb_x/seg_vb_y` arrays (or a packed struct).

##### 4.1.3 Segment presence (bench vs full)
The emitted topology describes the full canonical sculpture graph, but each firmware build (bench/full) should treat only “present segments” as active edges:
- Determine `present[segId]` by scanning `MappingTables::global_to_seg()` and marking any referenced segment IDs as present.

This makes the same topology tables safe in:
- `CHROMANCE_BENCH_MODE` subsets (missing segments/vertices),
- full builds,
- and any future partial deployments.

##### 4.1.4 Canonical “A→B coordinate” for LEDs on a segment (directional patterns)
Directional vertex/segment effects need a canonical coordinate along each segment that is independent of wiring direction.

Use existing mapping tables:
- `global_to_seg_k[i]` (0..13) is the LED coordinate along the **wiring** direction,
- `global_to_dir[i]` indicates whether wiring is `a_to_b` (0) or `b_to_a` (1) relative to the generator’s canonical `(A,B)`.

Define:
```text
ab_k(i) = (global_to_dir[i] == 0) ? global_to_seg_k[i] : (13 - global_to_seg_k[i])
```
Properties:
- `ab_k == 0` is nearest vertex A.
- `ab_k == 13` is nearest vertex B.

This single rule enables:
- Mode 7 path expansion in topology direction,
- “light toward a vertex” diagnostics (planned below),
- and future wavefront / gradient effects anchored at topology endpoints.

##### 4.1.5 Core accessors (portable)
Extend `src/core/mapping/mapping_tables.h` to expose the generated topology arrays:
- `vertex_count()`, `vertex_vx()`, `vertex_vy()`
- `seg_vertex_a()`, `seg_vertex_b()`

Runtime BFS on the MCU is acceptable here due to the small graph size; precomputing distances in Python is an optimization only.

### 4.2 Boundary vertices and “outer segments”
Compute from the vertex graph:
- Vertex degree map: `deg[v]`
- Boundary vertex set (canonical for v2.4): `deg[v] == 2`
- Outer segment set: segments incident to at least one boundary vertex.

Notes:
- Boundary detection is used only for EXHALE’s “outer segments” accounting. INHALE start selection uses §5.1.
- Future improvement (preferred): have the generator emit explicit boundary flags per vertex/segment so firmware doesn’t need to infer.

### 4.3 Center definition
Center is a configurable `vertex_id` (generator-emitted, stable; see §4.1.1), with deterministic resolution and fallback:
- If `CENTER_VERTEX_ID` is configured and valid for the active subgraph, use it.
- Otherwise compute “graph center” on the active subgraph:
  - compute BFS distances from each vertex to all others,
  - choose the vertex minimizing the maximum distance to all vertices (minimax eccentricity),
  - deterministic tie-break: smallest `vertex_id`.

Arrival semantics:
- Dots target the chosen `center` vertex.
- INHALE ends only when all dots reach `center` (event-driven completion).
- The final segment into `center` is managed by the center-lane policy (§5.3).

Alternative: geometric center of `(vx,vy)`, but graph-center is better for monotone `dist_to_center`.

### 4.4 Distance to center
BFS from `center` over the vertex graph:
- `dist[v] = shortest number of edges to center`

This is the monotonic constraint reference.

---

## 5) Inhale: Topology-Routed Monotone Paths (Segment-simple)

### 5.1 Path generation overview
At inhale start:
1) Determine `num_dots`:
   - `num_dots` is a configurable parameter.
   - If not explicitly provided, default `num_dots = 6`.
2) Select `num_dots` distinct start vertices using the farthest-from-center pool method:
   1. Compute `dist_to_center[v]` by BFS from `center` on the active subgraph.
   2. Create a list of vertices sorted by:
      - descending `dist_to_center` (furthest first),
      - then ascending `deg[v]` (prefer lower degree),
      - then ascending `vertex_id` (stable tie-break).
   3. Define `N_farthest` (configurable; must satisfy `N_farthest >= num_dots`). Default:
      - `N_farthest = min(VERTEX_COUNT, max(num_dots * 2, num_dots))`
   4. Take `pool = first N_farthest vertices` from the sorted list (excluding any vertex not in the active subgraph).
   5. Choose `num_dots` distinct starts from `pool` using deterministic RNG sampling without replacement.
3) Assign each dot a preferred “center lane” (a chosen center-incident segment) using the center-lane policy (§5.3).
4) For each start, generate a vertex path to the chosen center-adjacent vertex `u` subject to constraints:
   - downhill-preferred in `dist_to_center` (strictly decreasing preferred; plateau-safe equals allowed)
   - no segment traversed twice (per-dot; always enforced)
5) Append the final segment `u → center` to the path (center lane), then convert the full path into an LED index path by expanding each segment step to its 14 LEDs.

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
4) Apply center-lane constraints/preferences in §5.3 (applies only to the final approach into center).

### 5.3 Distinctness policy (v2.5): center-lane preference + round-robin fallback
This section replaces boundary-local disjointness. The goal is to avoid consistently overloading the same “final approach” segment into the center while still allowing paths to merge elsewhere (no global edge-disjoint solver).

Definitions:
- `center`: the chosen center vertex (graph center or fixed vertex).
- `center_segments`: the set of **present** segments incident to `center`.
  - In vertex terms: neighbors `u` of `center` where an active edge `(u, center)` exists.
  - In segment terms: `segId(u, center)` for each neighbor `u`.
- “center lane”: choosing a particular neighbor `u` (equivalently the segment `(u, center)`) as a dot’s final approach into `center`.

Final-segment semantics (explicit):
- Every INHALE path terminates at the `center` vertex.
- A dot’s final segment is exactly **one** of `center_segments` (the assigned center lane), and center-lane constraints apply only to this final segment.

Policy (applies only to INHALE, only to the final approach into center):
- **Primary goal:** within a single inhale, each center-incident segment is used by at most one dot **when possible**.
- **When impossible** (e.g., `num_dots > deg(center)`): reuse is allowed, but must **rotate across inhales** so the same center segment is not consistently overloaded.

#### 5.3.1 Explicit center-lane assignment step
At inhale start:
1) Enumerate available lanes:
   - Build the neighbor list `center_neighbors[]` for the active subgraph.
   - Define `lane_count = center_degree = len(center_neighbors)`.
2) Maintain a persistent `center_lane_rr_offset` in the effect state:
   - Offset advances by 1 modulo `lane_count` whenever INHALE is (re)initialized in auto mode:
     - on automatic cycle transition into INHALE, or
     - on `ESC` reset to auto mode.
   - Offset does not advance on manual phase stepping (`n` / `N`).
   - Offset advances in manual INHALE lane stepping:
     - `s`: `offset = (offset + 1) % lane_count`
     - `S`: `offset = (offset + lane_count - 1) % lane_count`
   - Offset is included in determinism: same seed + same offset => same lane assignments.
3) Assign each dot a preferred lane index:
   - If `lane_count >= num_dots`: assign unique lanes by taking the first `num_dots` lanes starting at `center_lane_rr_offset` (wrapping around).
   - If `lane_count < num_dots`: assign lanes by cycling through lanes starting at `center_lane_rr_offset`, i.e. dot `j` gets lane `(offset + j) % lane_count`.

This achieves:
- Unique center segments when possible.
- Balanced reuse when not possible, with rotation across inhales.

#### 5.3.2 Route-to-lane path generation (then append lane)
Instead of “route to center” directly:
- For each dot, route from its start vertex `s` to its assigned center-adjacent vertex `u` (the neighbor associated with its lane).
- Then append the final segment `u → center`.

Constraints remain unchanged:
- per-dot segment-simple (no segment reused within the dot),
- downhill-preferred in `dist_to_center` (plateau-safe),
- no backtracking.

Note: to avoid dead-end routing, it’s acceptable to treat `u` as the “goal” and stop once reached, then append the final edge to `center`.

#### 5.3.3 Fallback behavior if a lane is unreachable
If a dot cannot reach its assigned `u` under constraints:
1) Attempt each center lane at most once for this dot during this inhale:
   - try the assigned lane first,
   - then try remaining lanes in round-robin order starting from the assigned lane.
2) If all lanes fail (all lanes attempted once and none reachable):
   - relax **only** the center-lane uniqueness constraint for that dot (i.e., allow selecting a lane already used by another dot in this inhale if it becomes reachable),
   - but never relax per-dot segment reuse (segment-simple must remain invariant).

This prevents deadlock while preserving the intent: “unique center segments if possible, otherwise rotate overload”.

#### 5.3.4 Phase separation (clarify scope)
Center-lane uniqueness and rotation apply **only to INHALE path generation**.
- PAUSE phases do not use or depend on path routing.
- EXHALE is wavefront-based and does not use routing/path distinctness.

INHALE-only lane iteration (`s/S`) is also an INHALE-only concern: it exists solely to re-run the INHALE path generation with a different `center_lane_rr_offset` without stepping phases.

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
  - perceptual brightness LUT (hand-tuned), e.g. `[255, 170, 110, 70, 45, ...]`
  - RGB is constant per dot; only brightness varies along the tail.

This eliminates stutter without changing the physical path.

---

## 7) Exhale: “7 wavefronts received by all outer segments”

### 7.1 Discrete wavefront model
Represent wavefronts as repeated outward sweeps over “distance layers” from center.

Precompute per segment:
- Canonical distance: `seg_dist[segId] = min(dist[va], dist[vb])`

Justification:
- Using `min()` produces monotone, center-anchored wavefront layering and makes “received wavefronts” tracking stable even when segments span two distance layers.

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
- Record `last_beat_ms = now_ms` (real or virtual; used for fail-safe)

Beat events:
- “Now” implementation: synthesize beats at some nominal bpm using `now_ms`, but structure code around a `beat_event` boolean.
- “Later” implementation: use `Signals` (e.g., `signals.audio_beat` or add a dedicated beat signal) and increment on rising edges.

PAUSE fail-safe timeout (guardrail):
- Define `max_pause_duration_ms` (configuration).
- During a pause, if `(now_ms - last_beat_ms) >= max_pause_duration_ms`, inject a **virtual beat** event:
  - increment `beats_completed`,
  - update `last_beat_ms = now_ms`,
  - drive the same pulse envelope as a real beat.
- This keeps beat-count completion primary while guaranteeing pauses cannot hang indefinitely if beat input is missing.
- Apply identically to `PAUSE_1` and `PAUSE_2`.

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
- `center_lane_rr_offset_` (persistent across inhales; advances only when INHALE is initialized in auto mode to rotate center-lane reuse)

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
- `center_lane_rr_offset_` advances only when INHALE is (re)initialized in auto mode (auto cycle transition into INHALE, or `ESC` reset). It does not advance on manual phase stepping.

INHALE-only lane stepping (v2.5):
- If manual mode is enabled and phase is `INHALE`:
  - `s` increments `center_lane_rr_offset_` (wrap modulo `lane_count`) and reinitializes INHALE.
  - `S` decrements `center_lane_rr_offset_` (wrap modulo `lane_count`) and reinitializes INHALE.
- In any other phase (or if not manual), `s/S` are ignored.

Reinitialization scope for `s/S` (INHALE reinit):
- Reuse `dist_to_center` if center + active subgraph are unchanged (allowed and recommended).
- Re-run start selection from the farthest pool (deterministic RNG).
- Re-run center-lane assignment using the updated offset.
- Regenerate per-dot vertex paths and LED index paths (refresh caches as needed).

Determinism note:
- `s/S` are a debugging/iteration tool; INHALE results must be deterministic given the same RNG seed and the same keypress sequence.

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
  - Emit stable vertex list + per-segment endpoint vertex IDs into `--out-header` output (shared infrastructure).
- `include/generated/chromance_mapping_*.h` (generated; do not hand-edit)
- `src/core/mapping/mapping_tables.h`
  - Expose generated topology arrays (`vertex_*`, `seg_vertex_*`) to core if present.

### Runtime wiring (controls already exist)
- `src/main_runtime.cpp`
  - Add key handling for `s/S` alongside `n/N/ESC`.
  - Wire `s/S` to “reinitialize INHALE with offset update” (manual mode + INHALE only).
  - Possibly add optional debug printing (dot start vertices, beat targets, exhale counts) behind a flag.

### Tests (native)
- `test/test_effect_patterns.cpp`
  - Add deterministic tests for:
    - inhale start selection:
      - chosen starts are distinct,
      - chosen starts are drawn from the farthest pool,
      - ordering is deterministic given a fixed RNG seed
    - inhale path monotonicity (`dist` non-increasing)
    - no segment reuse
    - center-lane uniqueness when `deg(center) >= num_dots`
    - center-lane reuse rotates across inhales when `deg(center) < num_dots` (round-robin offset)
    - fallback behavior when a dot cannot reach its assigned lane (tries alternates; never violates per-dot segment-simple)
    - phase progression triggered by completion
    - pause beat-count completion and crossfade progress
    - pause fail-safe: with no external beat events, virtual beats trigger and the pause still completes within bounded time
    - exhale termination when wavefront counts reach 7 on all outer edges
    - INHALE-only lane stepping:
      - in manual mode + phase `INHALE`: `s` increments and `S` decrements `center_lane_rr_offset` with wrap-around
      - in other phases: `s/S` do not affect offset or phase
      - wrap-around: increment from `lane_count-1` -> `0`, decrement from `0` -> `lane_count-1`

Determinism requirements:
- Use a deterministic RNG seed in tests.
- Ensure INHALE behavior (start selection, lane assignment, routing, fallback) is deterministic given:
  - the same RNG seed,
  - the same topology,
  - the same center selection,
  - the same `center_lane_rr_offset`.

---

## 11) Configuration (Final Defaults + Guardrails)

The following defaults remove ambiguity and make the plan implementable without further decisions:

- `num_dots`: default `6`.
- `N_farthest`: default `min(VERTEX_COUNT, max(num_dots * 2, num_dots))`, must satisfy `N_farthest >= num_dots`.
- `beats_target`: random integer in `[3..7]` chosen at pause start.
- `max_pause_duration_ms`: required guardrail; default `6000` (two 30bpm heartbeat periods) unless overridden.
- Boundary vertices for EXHALE outer-edge accounting: `deg[v] == 2` on the active subgraph.
- Center selection:
  - primary: `CENTER_VERTEX_ID` if configured and valid for the active subgraph,
  - fallback: computed graph center by minimax eccentricity with tie-break `min vertex_id`.
- Center-lane rotation:
  - `center_lane_rr_offset` advances only when INHALE is initialized in auto mode (auto transition into INHALE, or `ESC` reset).
  - `center_lane_rr_offset` does not advance on manual phase stepping (`n` / `N`).

---

## 12) Suggested incremental rollout

1) Introduce generated topology tables (vertex list + segment endpoint vertex IDs) into mapping headers (shared infrastructure).
2) Add a small portable “topology view” utility in core (build active graph from present segments; degrees/boundary; BFS distances).
3) Implement configurable center selection policy for Mode 7 using vertex IDs (fixed and future-random).
4) Implement inhale path generation and smooth dot rendering (completion-driven).
5) Implement pause phases with beat-count completion and cyclical envelope.
6) Implement exhale wavefront accounting and completion condition; tune visuals.
7) Add native tests for topology invariants + deterministic behavior.

---

## 13) Diagnostic plan (Mode 1 / Index Walk): “Vertex incident segments” scan

Goal: add a new diagnostic scan mode under the existing Mode 1 (`Index_Walk_Test`) that validates **vertex connectivity** and **segment endpoint orientation**:
- For each vertex, light all segments incident to that vertex.
- Within each lit segment, LEDs “fill” progressively **toward** the vertex (from the far endpoint into the vertex).
- Allow deterministic manual stepping across vertices and directions for mapping correction.

This diagnostic is intentionally topology-driven and is a second consumer (after Mode 7) of the generator-based topology tables in §4.1. It is also a reusable primitive for future patterns (“flood from a vertex”, “wavefronts”, “vertex pulses”, etc.).

### 13.1 Current Mode 1 implementation (context to modify)
Mode 1 is implemented in `src/core/effects/pattern_index_walk.h`. It currently supports:
- `INDEX`: existing global LED index walk
- `LTR/UTD` and `RTL/DTU`: per-segment scan derived from `MappingTables::pixel_x/pixel_y` and `global_to_seg`

Relevant excerpt (existing scan-mode structure):
```cpp
enum class ScanMode : uint8_t { kIndex, kTopoLtrUtd, kTopoRtlDtu };
// build_topo_sequences(): groups indices by segId then sorts each group by x/y axis
```

### 13.2 Add a new scan mode: `VERTEX_TOWARD`
Add:
- `ScanMode::kVertexToward` (name in serial logs: `VERTEX_TOWARD` or `VERTEX`)

Behavior:
- The scan iterates **vertex IDs**, not LED indices.
- For each selected vertex:
  - all incident segments are lit (subject to “present segment” filtering),
  - each incident segment fills from far end toward the selected vertex.

### 13.3 Topology data required (provided by generator-based approach)
Uses §4.1 outputs:
- `vertex_vx/vertex_vy` and `VERTEX_COUNT`
- `seg_vertex_a/seg_vertex_b`
- present-segment detection by scanning `global_to_seg`

### 13.4 Build “incident segment lists” for vertices
At effect reset (or on first render), build adjacency for the active subgraph:
- Initialize `incident_count[VERTEX_COUNT]=0`.
- For each `segId` where `present[segId]==true`:
  - `va = seg_vertex_a[segId]`, `vb = seg_vertex_b[segId]`
  - append `segId` to both `incident[va]` and `incident[vb]`

Implementation detail:
- Use fixed-size arrays; the graph is tiny and bounded.
- Determine `MAX_DEGREE` from the canonical topology (or conservatively set to a small upper bound and assert if exceeded in native tests).

### 13.5 Per-LED rule: fill “toward the vertex”
For each LED index `i`, you have:
- `seg = global_to_seg[i]` (skip if 0 or not incident)
- `seg_k = global_to_seg_k[i]` (0..13)
- `dir = global_to_dir[i]` (0=a_to_b, 1=b_to_a relative to canonical topology endpoints)

Compute canonical coordinate along topology endpoint A:
```text
ab_k(i) = (dir == 0) ? seg_k : (13 - seg_k)   // 0 near A, 13 near B
```

Let `p` be the fill progress (0..14), where `p==0` means nothing lit and `p==14` means the whole segment lit.

Then:
- If `selected_vertex_id == seg_vertex_a[seg]` (vertex at A):
  - fill is from B → A, so light LEDs with `ab_k >= (14 - p)`
- If `selected_vertex_id == seg_vertex_b[seg]` (vertex at B):
  - fill is from A → B, so light LEDs with `ab_k <= (p - 1)`

This makes direction visually unambiguous and directly tests whether endpoint A/B semantics match reality.

### 13.6 Auto-cycle vs manual stepping (n/N/ESC)
Add a “manual hold” behavior consistent with the established convention used in modes 2/6/7:
- Auto:
  - iterate through all vertex IDs that have `incident_count>0` (active subgraph),
  - run a short fill animation per vertex (e.g., fill for ~1s then advance).
- Manual:
  - `n`: next vertex id in the active set, stay there (fill repeats)
  - `N`: previous vertex id in the active set, stay there
  - `ESC`: return to Mode 1’s existing `INDEX` auto behavior

Note: you can also make `n/N` cycle scan modes forward/backward when not in manual vertex stepping; decide once and keep consistent in serial UX.

### 13.7 Serial logging (required)
Whenever the active vertex changes (auto or manual), print:
- scan mode name (e.g., `VERTEX_TOWARD`)
- `vertex=<id> (vx,vy)`
- incident segment list: `segs=[s1,s2,...]`

Optional high-signal additions:
- For each incident segment, print whether the selected vertex is endpoint A or B (`seg=12 end=A`), to make endpoint validation explicit.

### 13.8 Native tests (recommended)
Add deterministic tests for the vertex diagnostic logic (portable core):
- adjacency building correctness (incident segments for a known vertex id)
- `ab_k` computation invariants (for any LED on a segment, `ab_k` spans 0..13 exactly once)
- fill-to-vertex rule lights monotone toward the endpoint (for both A and B endpoints)
