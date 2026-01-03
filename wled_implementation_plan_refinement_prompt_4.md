```md
You are a senior firmware + embedded architecture lead. Your task is to apply a small “final clarity” refinement pass to the implementation plan so it is unambiguous for engineers/agents to execute without interpretation drift.

## Inputs (MUST READ)
- `docs/architecture/wled_integration_implementation_plan.md` :contentReference[oaicite:0]{index=0}

## Deliverable
- Edit in place:
  - `docs/architecture/wled_integration_implementation_plan.md`

Do NOT implement code. Do NOT change the overall architecture. Only tighten specification clarity.

---

## Refinements to Apply (edit in place)

### 1) Make firmware header generation “Required,” not “Planned enhancement”
In Section **5.0b** (generator), currently “Standardize on firmware header generation” appears under “Planned enhancements.”
- Move header generation into a **Required behavior** subsection (or split 5.0b into “Required behavior” vs “Optional enhancements”).
- Keep tuning flags/preview PNG/etc. in “Optional enhancements.”

**Outcome:** Implementers cannot misread header emission as optional for firmware.

### 2) Specify the exact bench/full header selection mechanism
Where the plan references both:
- `include/generated/chromance_mapping_full.h`
- `include/generated/chromance_mapping_bench.h`

Add a single explicit rule, choose one:
- **Preprocessor selection**:
  - `#if CHROMANCE_BENCH_MODE` include bench header else full header
  - Document the define source (PlatformIO build flags)
OR
- **PlatformIO environment selection**:
  - separate envs (e.g., `env:runtime_full` vs `env:runtime_bench`) each including the appropriate header

Make the choice consistent across:
- `PixelsMap` section (5.7)
- `LedOutput` section (5.9)
- `main_runtime` / PlatformIO section (5.11)
- Bench Mode section (7.5)

### 3) Explicitly define the shared-array alignment contract
Add a short “Mapping Tables Contract” note in the mapping/runtime sections stating:

- All generated mapping tables share the same `LED_COUNT`
- For each `i in [0..LED_COUNT-1]`:
  - `pixel_x[i], pixel_y[i]` correspond to the same LED index as the RGB buffer’s `rgb[i]`
  - `global_to_strip[i], global_to_local[i]` map that same `i` to a physical output pixel

This prevents subtle mismatches between coordinate tables and strip-local tables.

### 4) `radius_norm()` — mark as future convenience or define it
In `PixelsMap` (5.7), either:
- Mark `radius_norm()` explicitly as **future convenience** (not required for Milestone 3/4 validation patterns), OR
- Define it succinctly:
  - `radius_norm(i) = dist(coord(i), center) / max_dist_over_all_leds`
  - State whether `center` is geometric mean, bounding-box center, or chosen point

Pick one and make it explicit so implementers don’t invent different definitions.

### 5) Make the FrameScheduler clamp formula explicit
In the FrameScheduler section (5.3), you already specify:
- accumulating `next += period`
- clamp if >5 frames late

Add the exact condition as a single line (in addition to prose), e.g.:
- `if ((now_ms - next_frame_ms_) > 5 * period_ms) next_frame_ms_ = now_ms;`

This prevents differing interpretations of “late.”

### 6) Minor doc hygiene: ensure “Required vs Optional” is consistent throughout
Do a quick pass to ensure:
- “Required” items are not buried under “Optional/planned”
- Milestone deliverables match the required behaviors
- The document ends cleanly without duplicated fragments

---

## Constraints
- Keep scope constraints intact: no deep networking responsiveness planning; no audio/biofeedback implementation.
- Do not change the milestone structure or Option A/B/C strategy; only refine clarity.

## Acceptance criteria
- Header generation is unambiguously required for firmware.
- Bench vs full header selection is explicitly specified and consistent everywhere.
- Mapping table alignment contract is clearly documented.
- `radius_norm()` is either defined or clearly marked as future convenience.
- Scheduler clamp condition is explicit.
```

