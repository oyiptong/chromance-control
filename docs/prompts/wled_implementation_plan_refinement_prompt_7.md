You are a senior firmware + embedded architecture lead. Your task is to apply a small corrective refinement pass to the implementation plan to undo recent regressions and eliminate ambiguity. You MUST edit the existing plan document in place.

## Input (MUST READ)
- `docs/architecture/wled_integration_implementation_plan.md`

## Deliverable
- Edit in place:
  - `docs/architecture/wled_integration_implementation_plan.md`

Plan-only task. Do NOT implement code. Do NOT change the architecture or overall milestone ordering.

---

## Scope constraints (MUST KEEP)
- No deep planning for networking responsiveness, liveview/peek streaming, packet-loss testing.
- No sound/biofeedback implementation planning beyond existing seams.
- Keep Option A/B/C framing and the WLED multi-bus “kill gate”.

---

## Corrections to Apply (edit in place)

### 1) Restore Milestone 2 acceptance criteria for firmware header generation (REGRESSION FIX)
**What to change**
In **Milestone 2 → Acceptance criteria**, re-add the previously required items:

- “C++ firmware headers are generated and treated as required artifacts:
  - `include/generated/chromance_mapping_full.h`
  - `include/generated/chromance_mapping_bench.h`”
- “Firmware mapping uses generated headers (zero-copy) and performs **no runtime JSON parsing** for mapping tables.”
- “Bench selection is verified:
  - when built with `CHROMANCE_BENCH_MODE=1`, firmware includes `include/generated/chromance_mapping_bench.h` (not the full header).”

**Why this matters**
This prevents a drift where Milestone 2 “passes” with JSON-only host artifacts but firmware later fails due to runtime parsing/RAM/fragmentation.

### 2) FrameScheduler: replace “Clamp” with “Reset” (TERMINOLOGY FIX)
**What to change**
In **Section 5.3 (FrameScheduler) Algorithm**, replace any wording like:
- “Clamp to avoid death spirals…”

with:
- “Reset to avoid death spirals…”

Keep the condition exactly as specified, but name it correctly:
- accumulate normally: `next_frame_ms_ += period_ms`
- reset when far behind: `if ((now_ms - next_frame_ms_) > 5 * period_ms) next_frame_ms_ = now_ms;`

**Why this matters**
“Clamp” is ambiguous and can be interpreted as bounding rather than resetting. You want an explicit reset policy.

### 3) Re-introduce first-call initialization behavior in FrameScheduler (AMBIGUITY FIX)
**What to change**
Still in **Section 5.3**, add a one-liner at the top of the algorithm:

- “First-call initialization: if `next_frame_ms_ == 0`, set `next_frame_ms_ = now_ms` and return true (render immediately).”

(Or equivalent wording; the key is to eliminate multiple plausible implementations.)

**Why this matters**
Without first-call behavior, firmware and native implementations may diverge subtly and create timing differences.

### 4) Doc hygiene: ensure the “required vs optional” is consistent after the above edits
**What to change**
At the end of the document (or in the Doc Hygiene Checklist), ensure these are true after edits:

Checklist:
- Milestone 2 acceptance criteria includes the generated headers + “no runtime JSON parsing”.
- Section 5.3 uses “reset” terminology (not clamp) and includes first-call behavior.
- Document ends cleanly (no duplicated fragments).

**Why this matters**
This is a drift-prevention pass: the goal is to keep the plan globally consistent, not just locally edited.

---

## Constraints
- Do not change milestone names or sequencing.
- Do not add new features.
- Keep changes minimal and localized to Milestone 2 acceptance criteria, Section 5.3 wording/algorithm, and the hygiene checklist.

## Acceptance criteria
- Milestone 2 acceptance criteria explicitly requires generated headers and no runtime JSON parsing for firmware mapping.
- FrameScheduler wording uses “reset” (not clamp) and includes first-call initialization.
- Doc hygiene checklist reflects these requirements.

