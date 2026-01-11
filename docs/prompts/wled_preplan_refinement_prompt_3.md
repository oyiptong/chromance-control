```md
You are a senior firmware + embedded architecture reviewer. Your task is to refine the existing pre-planning document to align with the current mapping generator script and to add a few missing, high-leverage clarifications for execution and validation.

## Inputs (MUST READ)
1) `docs/architecture/wled_integration_preplan.md`
2) `scripts/generate_ledmap.py` (or wherever the current generator lives; locate it in the repo)

Treat the pre-plan as the source of truth, but update it to accurately reflect what the generator script actually does and expects.

## Deliverable
Update (edit in place):
`docs/architecture/wled_integration_preplan.md`

Doc-only changes preferred. Do NOT implement new firmware features in this task.
You MAY propose small script enhancements in the doc (CLI flags, metadata outputs), but do not write code.

---

## Required refinements (apply all)

### A) Make “unique vertices used” explicitly derived
If the doc mentions a used-vertex count (e.g., 25), change the wording to:
- “The 40-segment endpoint set uses **25 unique vertices (derived by counting unique endpoints in the segment list).**”
Do not present it as a fixed truth independent of the segment list.

### B) Align wiring.json assumptions with the generator
Ensure the doc clearly states:
- `mapping/wiring.json` is a REQUIRED input for map generation (generator expects it).
- `wiring.json` is authoritative for:
  - strip ordering across 4 strips
  - segment order within each strip
  - segment direction (A->B vs B->A) for LED index assignment
- Remove any wording suggesting `wiring.json` is optional or “not yet needed,” unless you also specify a fallback path.

### C) Add one mapping invariant: exact segment coverage
In the “Mapping Invariants & Validation” subsection, add:
- All 40 segment IDs must appear **exactly once** in the wiring order (no missing, no duplicates).
- The set of segment IDs must match `{1..40}`.
Explain that failing this invalidates ledmap and pixels artifacts.

### D) Document script outputs + recommended metadata improvements
Add a short subsection:
`### Mapping Generator (generate_ledmap.py) — Inputs/Outputs`
Include:
- Inputs: segments list (from doc), wiring.json
- Outputs: ledmap.json, pixels.json
- Normalization: bounds computed and coords shifted to start at (0,0)
- Collision policy: collisions are fatal errors

Then add a “Recommended Enhancements” list (doc only) proposing:
- Add richer pixels metadata: `strip`, `seg`, `k` (0..13), `dir`
- Add CLI flags: `--x-scale`, `--y-scale`, `--flip-x`, `--flip-y`, `--t-offset`
- Add `--validate-only` mode for an existing ledmap.json
- Add optional preview image output (scatter PNG) for quick geometry sanity checks

### E) Tighten “representativeness” verification steps
Add a section:
`## Verifying ledmap.json is Valid & Representative`
Make it a clear 3-layer checklist:
1) Programmatic validity (schema, bounds, uniqueness, range, 560 lit cells)
2) Geometric representativeness (browser/plot silhouette matches Chromance)
3) Physical validation (Coord_Color_Test + XY_Scan_Test + optional Index_Walk_Test)

Include explicit acceptance criteria:
- Every LED index 0..559 appears exactly once
- Exactly 560 non-(-1) cells
- Gradients are oriented correctly
- No teleporting during XY scan
- Index-walk matches expected wiring continuity

### F) Add a WLED-specific note about multi-bus index space
Add a short callout under WLED feasibility spike:
- We must confirm WLED’s handling of **4 output busses** yields a single contiguous LED index space compatible with `ledmap.json` indices (0..559) OR define how indices are assigned per-bus and ensure ledmap aligns.

(Do not over-speculate; just make it an explicit spike item.)

### G) Clean up numbering and section placement
Ensure:
- The Chromance mapping section flows into validation patterns and verification steps.
- Subsection numbering is consistent with the rest of the doc (no mixed 5.x under lettered sections).

---

## Quality bar
- Keep edits integrated (not just appended).
- Use concise bullets and unambiguous definitions.
- Cite actual repo file paths for the generator and mapping artifacts.

## Acceptance criteria
- `docs/architecture/wled_integration_preplan.md` is updated with all items A–G.
- The doc clearly matches the generator script’s expectations and outputs.
- A reader can follow the doc to generate, validate, and physically confirm a correct ledmap.
```

