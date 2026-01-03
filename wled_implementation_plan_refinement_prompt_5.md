You are a senior firmware + embedded architecture lead. Your task is to apply a **final clarity + drift-prevention pass** to the implementation plan. You MUST edit the existing file in place.

## Inputs (MUST READ)
- `docs/architecture/wled_integration_implementation_plan.md`

## Deliverable
- Edit in place:
  - `docs/architecture/wled_integration_implementation_plan.md`

Plan-only task. Do NOT implement code. Do NOT change overall architecture or milestone sequencing. Only clarify ambiguous wording and tighten acceptance criteria.

---

## Scope constraints (MUST KEEP)
- Do NOT add deep planning for networking responsiveness, packet-loss tests, liveview/peek streaming, or sound/biofeedback implementation.
- Keep modular seams for future extension (`Signals`, `IModulationProvider`) but do not expand them into a full subsystem.
- Keep Option A/B/C structure and the WLED multi-bus index “kill gate”.

---

## Refinements to Apply (edit in place)

### 1) Make “header generation is required for firmware” unmistakable
**What to change:**
- In Section 5.0b, ensure the “Required behavior (firmware mapping source of truth)” is clearly labeled as REQUIRED and not mixed with optional items.
- Add a Milestone 2 acceptance criterion explicitly requiring the C++ headers:
  - `include/generated/chromance_mapping_full.h`
  - `include/generated/chromance_mapping_bench.h`
  - and that firmware uses them (no runtime JSON parsing).

**Why this matters:**
- Prevents an implementation that “works on host with JSON” but later fails on ESP32 due to RAM/fragmentation/complexity.

### 2) Replace “clamp” terminology with “reset” for FrameScheduler catch-up
**What to change:**
- In Section 5.3, replace wording like “clamp to avoid death spirals” with precise language:
  - normal path: **accumulate** `next_frame_ms_ += period_ms`
  - behind path: **reset** `next_frame_ms_ = now_ms` when too far behind
- Keep the exact condition as written, but describe it as reset.

**Why this matters:**
- “Clamp” can be interpreted as bounding `next` to a maximum delta; you intend a full reset to `now` after large delays. Misinterpretation changes timing behavior.

### 3) Add a single-line “Mapping Tables Contract” reminder near LedOutput
**What to change:**
- You already define the Mapping Tables Contract in PixelsMap (5.7). Add a short reminder in LedOutput (5.9) stating:
  - `rgb[i]`, `pixel_x/y[i]`, and `global_to_strip/local[i]` must align for the same i in `[0..LED_COUNT-1]`.

**Why this matters:**
- Prevents silent bugs where coordinate tables are generated in one ordering but strip-local tables in another (XY scan lights “wrong” pixels even though tables look plausible).

### 4) `radius_norm()`—reduce implementer ambiguity
**What to change:**
- In PixelsMap (5.7), keep `radius_norm()` but make it explicitly one of:
  - **Future convenience** (not required for Milestone 3/4), OR
  - Provide a 1–2 line definition (center choice + normalization basis).
- Do not leave it “half-defined”.

**Why this matters:**
- Prevents different engineers implementing different radius definitions and producing inconsistent visuals later.

### 5) Bench/full header selection—make the rule visible in one place
**What to change:**
- You already chose `CHROMANCE_BENCH_MODE` and preprocessor selection. Add a short “single source of truth” note in one place (5.11 PlatformIO section is best) stating:
  - which PlatformIO env sets `CHROMANCE_BENCH_MODE=0`
  - which sets `CHROMANCE_BENCH_MODE=1`
  - or if only one env exists, how developers flip it safely.

**Why this matters:**
- Prevents local builds accidentally including the wrong header and producing “it works on my machine” mapping mismatches.

### 6) Granular doc hygiene checklist (do not do a vague “quick pass”)
**What to change:**
- Replace any vague “ensure consistency throughout” with this explicit checklist and confirm each item is consistent after edits:

Checklist:
- Section 5.0b: header generation is REQUIRED; optional flags remain optional.
- Milestone 2 acceptance criteria: explicitly mention generated headers and no runtime JSON.
- Section 5.3: wording uses “reset” (not “clamp”) and includes both branches (accumulate vs reset).
- Section 5.7 and 5.9: mapping alignment contract is stated (no index-order ambiguity).
- Section 5.11: bench/full selection mechanism and build flags are explicit and consistent with 5.7.
- Document ends cleanly with no duplicate fragments.

**Why this matters:**
- Ensures the plan remains globally coherent, not just locally correct in edited sections.

---

## Constraints
- Do not change the architecture or milestones; only clarify wording/acceptance criteria.
- Do not introduce new subsystems (network/audio).
- Keep edits minimal but precise.

## Acceptance criteria
- Header generation is explicitly required for firmware, and Milestone 2 requires it.
- FrameScheduler uses “accumulate + reset” terminology (no clamp ambiguity).
- Mapping alignment contract is referenced in both PixelsMap and LedOutput sections.
- `radius_norm()` is either defined or clearly deferred.
- Bench/full selection flags are explicitly tied to PlatformIO env(s).
- Doc hygiene checklist is applied and the document reads clean end-to-end.

