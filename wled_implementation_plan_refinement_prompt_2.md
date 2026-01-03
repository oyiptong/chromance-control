```md
You are a senior firmware + embedded architecture lead. Your task is to apply a final refinement pass to the existing implementation plan so it is internally consistent, unambiguous for execution, and free of leftover artifacts.

## Inputs (MUST READ)
1) `docs/architecture/wled_integration_preplan.md` (requirements + constraints)
2) `docs/architecture/wled_integration_implementation_plan.md` (the plan to refine)

You may inspect the repo to confirm environment names and file paths if needed, but your primary task is to refine the plan document.

## Deliverable
Update (edit in place):
`docs/architecture/wled_integration_implementation_plan.md`

Plan-only task. Do NOT implement code.

---

## Scope constraints (MUST KEEP)
- Do NOT add deep planning for networking responsiveness, packet-loss tests, liveview/peek streaming, or sound/biofeedback implementation.
- Keep modular seams for future extension (`Signals`, `ModulationProvider`), but do not expand them into a full subsystem.
- Keep Option A/B/C decision framing and the WLED “kill gate”.

---

## Required refinements (apply all)

### A) Remove internal inconsistencies around FPS ownership
Currently the plan risks duplicating FPS control (e.g., `target_fps` appearing both in effect params and in the scheduler/config).
- Make `target_fps` owned by **one place only** (recommended: scheduler/render config).
- Update any types/APIs listed in the plan accordingly.
- Add a short note: effects must not depend on FPS caps; they render based on time input.

### B) Clarify `Signals` defaults to represent “not provided”
Update the planned `Signals` / `ModulationInputs` struct defaults so effects can distinguish:
- “no signal provided” vs “signal provided at 0”
Do this by either:
- sentinel values (e.g., `-1.0f`), OR
- `has_*` booleans
Update effect signature examples in the plan to reflect this.

### C) Decide and document ONE XY scan model for the custom engine
The plan should not be ambiguous about how `XY_Scan_Test` is implemented for the custom engine.
- Choose **sorted-pixels scan** as the canonical approach:
  - precompute a stable ordering of LED indices by `(y, x, ledIndex)` from `pixels.json`
  - iterate that list for the scan
- Reserve “grid scan” only for WLED/ledmap use (optional), but do not require it for the custom engine.
Update milestone and file-by-file entries accordingly.

### D) Make Bench Mode and variable LED count fully coherent
Ensure the plan consistently treats LED count as **derived from mapping/wiring**, not hard-coded to 560.
- Replace any wording like “enforce out_len == 560” with:
  - “enforce out_len == map.led_count()”
- If an upper bound constant is desired (e.g., 560), frame it as `kMaxLeds` and keep runtime `active_led_count`.
- Ensure the bench wiring subset and full wiring path are both compatible with the same core code.

### E) Make the WLED multi-bus kill gate outcomes explicit and decisive
In the WLED spike (Milestone 1 / Section 8), ensure:
- The multi-bus index model test is the first acceptance criterion.
- The plan states explicit pass/fail outcomes:
  - PASS: contiguous indices 0..N-1 across 4 buses with ledmap interpretation compatible → Option B viable.
  - FAIL: per-bus/overlapping indices → Option B not viable without remap/fork; document the chosen fallback (Option C or remap layer) as the next step.
- Include how ledmap indices would be interpreted under each model (briefly, without deep WLED ops planning).

### F) Clean up any stray paste artifacts / duplicated fragments
Remove any trailing duplicated fragments (e.g., repeated “Build environments / Dependencies” blocks) or incomplete sections so the document ends cleanly and consistently.

### G) Optional but recommended: add mapping version propagation (lightweight)
If not already present, add a brief plan to include:
- `mappingVersion` in `wiring.json`
- propagate it into generated `pixels.json` and `ledmap.json`
This is for future compatibility checks; no migration logic required now.

---

## Quality bar
- Apply changes by integrating into the existing plan (not by appending a separate addendum).
- Keep it concise and execution-oriented.
- Maintain correct, repo-grounded environment names where referenced.

## Acceptance criteria
- FPS ownership is unambiguous.
- Signals defaults correctly represent “not provided.”
- XY scan implementation is unambiguous for the custom engine (sorted pixels).
- Bench mode and variable LED count assumptions are consistent across the doc.
- WLED kill gate has explicit pass/fail outcomes and next actions.
- Document is free of stray paste artifacts and reads clean end-to-end.
```

