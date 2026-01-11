You are a senior firmware tooling engineer. Your task is to generate **pre-filled, schema-valid** wiring files for Chromance that I can manually edit and validate.

## Source of truth (MUST READ FIRST)
Read:
- `docs/architecture/wled_integration_implementation_plan.md`

From it, extract:
- required `mapping/wiring.json` schema and invariants
- strip count + expected segment distribution (11/12/6/11 unless repo evidence differs)
- the Segment Topology List (segment IDs 1..40 -> vertex endpoints)
- bench mode expectations (strip0-only default is fine)

## Goal
Create two JSON files as **editable templates**:
1) `mapping/wiring.json` (full)
2) `mapping/wiring_bench.json` (bench subset)

These are templates: they must be syntactically valid and pass schema/invariant checks *structurally*, but the segment order/direction may require manual correction unless it can be derived from repo code.

---

## IMPORTANT: Attempt to derive, but DO NOT hallucinate wiring
Attempt to derive real strip->segment order and segment directions by inspecting repo sources that may encode it:
- `src/core/strip_layout.h`
- `src/core/layout.h`
- `src/platform/dotstar_leds.*`
- any tables mapping global LED index -> (strip, local) or (segment, k)

### Placeholder strategy decision tree (MUST FOLLOW)
1) **If complete wiring is derivable from repo**: Use it entirely.
2) **If partial wiring is derivable**:
   - Use derived segments in the derived order/strip placement.
   - Fill remaining unknown segments using a placeholder order.
   - Mark each segment entry with a confidence `_comment`:
     - `"CONFIDENCE: DERIVED from <file>:<line or symbol>"`
     - `"CONFIDENCE: PLACEHOLDER - requires physical validation"`
3) **If no wiring is derivable**: Use full placeholder numeric order distribution (see below) and mark all segments as placeholder.

In all cases, add top-level `_comment` stating whether the file is fully derived, partially derived, or placeholder.

---

## Direction (`dir`) inference rules (MUST FOLLOW)
- **Hard evidence (allowed to set dir confidently):**
  - explicit direction encoding in code/tables (e.g., a boolean “reversed” flag)
  - clear comments in code stating direction
- **No hard evidence (default):**
  - set `dir: "a_to_b"`
  - add `_comment: "DIR UNCHECKED - validate with Index_Walk_Test / segment endpoint test"`

**Note:** Do NOT attempt “soft evidence” heuristics (e.g., guessing based on geometry, naming conventions, or SVG arrow assumptions) because they often look plausible but are frequently wrong. We prefer explicit warnings + physical validation.

---

## Requirements for generated JSON (MUST)
### Common
- Include:
  - `"mappingVersion": "chromance-v1"` (or similar)
  - `"isBenchSubset": false|true` (REQUIRED in both files)
- Use `"_comment"` fields (string or array) liberally to guide manual edits.
- Use `dir` values exactly: `"a_to_b"` or `"b_to_a"`.

### `mapping/wiring.json` (full)
- Exactly **4 strips**.
- Segment IDs **1..40 appear exactly once total** across all strips.
- Expected per-strip segment counts: **11/12/6/11** unless repo evidence contradicts.
  - If repo evidence differs, use repo-derived counts but:
    - WARN in top-level `_comment`
    - document the discrepancy in the README
- Each strip object:
  - `name` (e.g., "strip0")
  - `segments`: array of `{ "seg": <int>, "dir": "...", "_comment": "..." }`
- Each segment entry should include a confidence marker in `_comment`:
  - DERIVED vs PLACEHOLDER

### `mapping/wiring_bench.json` (bench)
- `isBenchSubset: true`
- Default: **strip0-only** with 11 segments (154 LEDs) as the initial bench subset.
- Segment IDs must be a subset of 1..40, no duplicates.
- Include `_comment` stating this is a dev/test subset and must not be used as full wiring.

---

## Placeholder ordering (only if real wiring cannot be fully derived)
If you cannot infer wiring for some/all strips, use this placeholder distribution:
- strip0: seg 1..11
- strip1: seg 12..23
- strip2: seg 24..29
- strip3: seg 30..40

Bench default:
- wiring_bench uses strip0 placeholder seg 1..11

Mark all placeholder entries with:
- `"CONFIDENCE: PLACEHOLDER - reorder to match physical wiring"`

---

## Output files (MUST CREATE)
Create these files in the repo working tree:
- `mapping/wiring.json`
- `mapping/wiring_bench.json`

Also create a companion guide:
- `mapping/README_wiring.md`

### README requirements (MUST)
The README must include:

1) **Topology Key (CRITICAL):**  
   A table listing Segment IDs 1..40 and their vertex endpoints:
   - `Seg 1: (0,1)-(0,3)`
   - ...
   Derived from the implementation plan’s segment topology list.

2) **Validation checklist (structured):**
   - Structural validation:
     - segment coverage rules, direction enum validity
   - Generate artifacts:
     - run the generator with wiring.json
     - what “success” looks like (no collisions; LED count matches)
   - Physical validation patterns (later on device):
     - `Index_Walk_Test`: continuity/no teleports
     - `XY_Scan_Test`: spatial coherence / no teleporting
     - `Coord_Color_Test`: axis orientation
   - Correction workflow:
     - reorder segments when index-walk jumps
     - flip `dir` when a segment runs backward
     - regenerate + retest

3) **How to interpret confidence markers:**
   - DERIVED entries should be validated but are likely correct
   - PLACEHOLDER entries are expected to be edited

4) **Bench mode guidance:**
   - what is testable with bench subset
   - how to grow bench subset if desired

---

## Nice-to-haves (MENTION ONLY — do not implement unless trivial)
The following suggestions are nice but optional. Mention them in README as optional future improvements if you like, but do NOT spend time implementing them now:

- File formatting requirements (UTF-8, LF, 2-space indent): helpful for clean diffs, but not required for correctness.
- Example JSON snippet in the prompt: helpful for humans, but you will already output real files.
- “Soft evidence” direction heuristics from SVG arrows/naming: not worth it because it increases false confidence.
- Extra bench alternatives (2 strips, 1–2 segments): optional; start with strip0-only for simplicity.

If any of these are extremely easy to add without distracting from the main outputs (e.g., consistent 2-space indent), you may do so, but prioritize correctness and clarity.

---

## Constraints
- Do NOT implement firmware features.
- Do NOT change the implementation plan doc.
- Keep the JSON valid (no actual JSON comments); use `_comment` fields instead.

## Acceptance criteria
- Both JSON files parse as valid JSON.
- `wiring.json` has 4 strips, `isBenchSubset:false`, and covers seg IDs 1..40 exactly once.
- `wiring_bench.json` has `isBenchSubset:true` and contains a valid subset with no duplicates.
- `_comment` fields clearly mark DERIVED vs PLACEHOLDER segments.
- README contains the Topology Key and a concrete validation/correction workflow.

