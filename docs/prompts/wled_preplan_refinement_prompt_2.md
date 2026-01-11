You are a senior firmware + embedded architecture reviewer. Your task is to refine and correct the existing pre-planning document in this repo based on the latest feedback. You MUST edit the existing file in place.

## Deliverable
Update:
`docs/architecture/wled_integration_preplan.md`

No major code changes. Doc-only is preferred. You may add a tiny helper script ONLY if it directly supports generating/validating mapping artifacts and you document it.

---

## Goals of this refinement pass
1) Fix minor inaccuracies and tighten wording.
2) Make mapping invariants explicit (so generators/validators are unambiguous).
3) Improve structure/numbering consistency.
4) Add a practical web-based simulator plan (WASM shared-core) without overcommitting.
5) Keep everything grounded in this repo with file path references where applicable.

---

## Required edits (apply all)

### A) Correct the “vertex count” phrasing
In the Chromance mapping section, remove/replace any claim like “25-ish corner points.”
- The vertex lattice is **7 columns × 9 rows** => **63 possible vertex positions** (vx=0..6, vy=0..8).
- If you want to mention “unique vertices used by segments,” phrase it as:
  - “The segment list uses a subset of the 63 lattice points; the exact used-vertex count can be derived from the segment list.”
Do NOT invent a used-vertex count unless you compute it.

### B) Clarify strip vs segment distribution
Remove any implication that segments are evenly distributed across strips.
- The current repo indicates per-strip segment counts (e.g. **11/12/6/11**) in `src/core/layout.h` (confirm and cite).
- Update the mapping pipeline text to reflect:
  - `wiring.json` is authoritative for mapping the 40 segments across the 4 strips, including direction per segment.
  - The generator must respect the real per-strip segment counts and ordering.

### C) Tighten the DotStar driver assumption
Where the doc discusses Adafruit_DotStar constructor usage, clarify:
- When constructed with explicit data/clock pins (common in Adafruit_DotStar), the library uses software/bit-banged SPI; hardware SPI is typically used via the constructor that omits pins (or uses `SPI` object).
- Frame this as a hypothesis to be confirmed by inspecting the driver code used in this repo.
Add a short “confirmations” bullet under the spike plan:
- “Verify hardware SPI vs software SPI for each of the 4 strips by inspecting the platform driver and/or measuring CPU time spent in show()/flush.”

### D) Fix numbering/structure consistency
Renumber the “Chromance 2D Mapping” subsections so they match the document’s section lettering/numbering style.
- Avoid “5.1–5.4” nested under a different section letter.
- Ensure “Mapping Validation Patterns” sits immediately after the mapping generation spec.

### E) Add explicit mapping invariants (MUST)
In the mapping generation section, add a subsection:
`### Mapping Invariants & Validation`
Include these invariants:
1) Exactly **560 LEDs**: indices **0..559**.
2) Each LED index appears **exactly once** in the produced mapping (no duplicates, no gaps).
3) Each occupied cell in ledmap maps to a **valid** LED index.
4) Collisions are not allowed: no two LEDs share the same (x,y) cell.
5) All mapped (x,y) are within computed bounds; bounds are minimal after normalization.
6) `map.length == width*height`.

Add recommended validation steps:
- Unit test that enforces invariants.
- Optional CLI/script that outputs collision report + bounds + coverage stats.
- A “diff-friendly” stable ordering for generated JSON.

### F) Add “Web Simulator (Optional, High Leverage)” section
Add a new section that plans a browser simulator without compiling the whole firmware to WASM.

Include:
- **Scope boundary**: only compile a portable “effects core” to WASM; do not compile WiFi/OTA/drivers.
- **Inputs/outputs**:
  - Inputs: timeMs, effectId, params, mapping
  - Output: RGB buffer length 560
- **Mapping reuse**: prefer `pixels.json` (ledIndex -> x,y) as the simulator’s source-of-truth; `ledmap.json` remains the WLED artifact.
- **Two runtime modes**:
  1) Local WASM sim (effects run in-browser)
  2) Live device preview (browser renders frames streamed from device; model after WLED “peek/liveview” concept)
- **Minimal ABI** (keep it simple):
  - `init(ledCount)`
  - `setMapping(coordsPacked)`
  - `setParams(effectId, paramsBlob)`
  - `render(timeMs) -> writes into RGB buffer`
- **Rendering**: draw 560 points at (x,y) using Canvas/SVG; optionally draw segment lines.
- **FPS knob**: simulator throttles to `target_fps` or runs uncapped.
- **Placement in roadmap**: add a milestone after mapping artifacts are stable:
  - “Build simulator viewer + mapping validation patterns (XY scan + coord gradient).”

### G) Keep repo-grounded references
Where you describe “current architecture” or “current driver behavior,” keep citing file paths you can confirm in this repo.
If something is uncertain, label it “assumption to verify in spike.”

---

## Output quality bar
- Keep changes cohesive; don’t just append—integrate cleanly.
- Use short, readable sections; avoid overly generic language.
- If you add any helper script, include:
  - path (e.g. `tools/mapping/gen_ledmap.py`)
  - what it reads/writes
  - how to run it
  - how it validates invariants

## Acceptance criteria
- The updated doc contains all edits A–G.
- Mapping section is unambiguous enough to implement without follow-up questions.
- Simulator plan is realistic and clearly scoped.

