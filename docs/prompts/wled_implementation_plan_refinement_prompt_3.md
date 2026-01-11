You are a senior firmware lead. I have received final peer review on the implementation plan. Your task is to apply the following **polish refinements** to make the spec unambiguous and implementation-ready.

## Inputs (MUST READ)
- `docs/architecture/wled_integration_implementation_plan.md`

## Deliverable
- Edit in place:
  - `docs/architecture/wled_integration_implementation_plan.md`

Do NOT implement code. Do NOT change the overall architecture. Only refine the implementation details in the specified places.

---

## Refinements to Apply (Edit in place)

### 1) Standardize on C++ Header Generation for Firmware (CRITICAL)
Update the sections that describe the mapping generator and runtime mapping usage to make this the default:

- The generator (e.g., `scripts/generate_ledmap.py`) MUST emit **C++ headers** into a clearly defined include location such as:
  - `src/generated/` or `include/generated/` (choose one and use it consistently)

Add explicit header outputs (names can vary, but must be stable and documented):
- `generated/chromance_mapping_full.h` (full 4-strip sculpture)
- `generated/chromance_mapping_bench.h` (bench-mode subset)

These headers must contain `static const` tables (stored in flash) for at minimum:
- `pixel_x[]`, `pixel_y[]` (or packed `pixel_xy[]`) for `PixelsMap`
- `global_to_strip[]`, `global_to_local[]` for `LedOutput` (global index -> (stripId, stripOffset))
- `LED_COUNT` (active LED count for that build: 560 full, 140 bench or derived)

Also document:
- Firmware uses these headers **zero-copy** (no JSON parsing at runtime).
- Native tests/simulator MAY load JSON OR reuse the same generated headers; document which is preferred.

### 2) Define FrameScheduler Drift / Catch-up Policy (Select accumulating time)
In the `FrameScheduler` section, explicitly select the policy:

- Use accumulating time to maintain average tempo:
  - `next_frame_ms_ += period_ms_` (not `next = now + period`)

Add a safety clamp to prevent death spirals:
- If `now` is far behind `next_frame_ms_` (e.g., more than 5 missed frames),
  - reset `next_frame_ms_ = now` and continue.

Document this behavior in plain language so the implementer does not reinterpret it.

### 3) Clarify multi-strip bus-length math in the WLED spike
In Milestone 1 (WLED spike), make the bus lengths self-explanatory by adding the derivation inline:
- `11 segments * 14 LEDs = 154`
- `12 * 14 = 168`
- `6 * 14 = 84`
- `11 * 14 = 154`
So the numbers `154/168/84/154` are obviously tied to the sculpture wiring distribution.

### 4) Signals & Null Provider: make the sentinel dependency explicit
In the section describing `Signals` and `NullModulationProvider`, add a short clarifying note:
- `NullModulationProvider` relies on the `Signals` sentinel values (e.g., `kNotProvided` / `-1`) to indicate “not present.”

---

## Constraints
- Keep the rest of the plan intact.
- Do not introduce new features (no networking responsiveness work, no audio/biofeedback implementation).
- Do not change Option A/B/C strategy; only refine the implementation details above.

## Acceptance criteria
- The plan clearly states firmware uses generated headers (not JSON) for mapping tables.
- Scheduler policy is explicit (`next += period` with clamp).
- Bus length numbers are explained by segment math.
- Sentinel dependency for signals is documented.

