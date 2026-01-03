You are a senior firmware + embedded architecture reviewer. Your task is to UPDATE the existing pre-planning document in this repo to incorporate new, concrete mapping details for Chromance, plus the architectural refinements discussed.

## Deliverable
Update (edit in place) the existing planning document:
`docs/architecture/wled_integration_preplan.md`

If that file does not exist, create it at that path.

Do NOT implement the full system yet. Prefer doc-only changes. You may add a tiny helper script *only if* it materially supports generating `ledmap.json` from the wiring + segment list (and mention it in the doc).

---

## New confirmed facts to incorporate
### Physical layout + topology
- The physical geometry comes from `chromance_layout.svg` (already in repo/workspace).
- Treat “steps/rows/columns” as interchangeable.
- Vertex lattice is **7 columns × 9 rows** of vertices:
  - `vx = 0..6`, `vy = 0..8`
- There are **40 segments**, **14 LEDs per segment** = **560 LEDs total**.
- There are **4 strips total** (wiring splits the 40 segments across 4 independent strips).
- The coding agent already has the strip/LED wiring map (strip -> ordered segments + direction + LED order).

### WLED mapping requirement
- WLED 2D mapping is a rectangular grid with a sparse map file:
  - `ledmap.json` containing `{ "width", "height", "map" }`
  - `map` is length `width*height` row-major; each entry is LED index or `-1` for empty.
- We will generate a ledmap for Chromance from SVG-derived geometry + wiring order.

---

## Insert these new sections into the doc (and update existing sections as needed)

### 1) Add “Open questions”
Add a short section near the top listing the remaining unknowns that affect A/B/C decisions, including:
- Feasibility of **4 independent APA102/DotStar outputs** under WLED (and expected limitations/risks).
- Whether we will keep a custom control plane vs adopt WLED UI.
- How we will store and version mapping/presets (NVS vs filesystem, etc.).

### 2) Add “Measurable spike success criteria”
Add explicit pass/fail metrics for the first spike, e.g.:
- Free heap headroom after WiFi/UI enabled (define a target, e.g. ≥ 80KB — pick reasonable).
- Stable animation at target FPS without visible stutter.
- No random flicker/data corruption across all 4 strips at chosen SPI frequency.
- ledmap correctness validated by mapping test patterns (below).

### 3) Clarify “FPS configurability” decision
Update any FPS discussion to reflect:
- In OUR firmware, runtime-configurable `target_fps` is easy and recommended.
- In stock WLED, the main loop FPS is typically a compile-time constant (note this as a tradeoff).
- Add a proposed mechanism: `target_fps=0` => uncapped; otherwise frame scheduler.

### 4) Expand “APA102 multi-output feasibility” subsection
Add a dedicated subsection under the spike plan:
- Identify whether current code uses hardware SPI or software/bit-banged output per strip.
- State what we’ll measure: `flush_ms`, total `frame_ms`, stability at multiple SPI clocks.
- Outline 3 test configurations: conservative, medium, aggressive SPI clock.
- Define success for “4 strips working” in WLED if we attempt Option B.

### 5) Add “Chromance 2D Mapping (WLED ledmap.json)” section
This is the key new section. Include:

#### 5.1 Vertex grid definition
- 7×9 vertices; index ranges; concept of “vertex coords” vs “pixel coords”.

#### 5.2 Segment topology list (endpoints in vertex coords)
Use this exact segment list (40 edges). Treat endpoints as undirected for geometry;
wiring config supplies direction later:

1. (0,1)-(0,3)
2. (0,1)-(1,0)
3. (0,1)-(1,2)
4. (0,3)-(1,4)
5. (1,0)-(2,1)
6. (1,2)-(1,4)
7. (1,2)-(2,1)
8. (1,4)-(1,6)
9. (1,4)-(2,3)
10. (1,4)-(2,5)
11. (1,6)-(2,7)
12. (2,1)-(2,3)
13. (2,1)-(3,0)
14. (2,1)-(3,2)
15. (2,3)-(3,4)
16. (2,5)-(2,7)
17. (2,5)-(3,4)
18. (2,7)-(3,6)
19. (2,7)-(3,8)
20. (3,6)-(3,4)
21. (3,6)-(4,7)
22. (3,8)-(4,7)
23. (3,0)-(4,1)
24. (3,2)-(3,4)
25. (3,2)-(4,1)
26. (3,4)-(4,3)
27. (3,4)-(4,5)
28. (4,1)-(4,3)
29. (4,1)-(5,0)
30. (4,1)-(5,2)
31. (4,3)-(5,4)
32. (4,5)-(4,7)
33. (4,5)-(5,4)
34. (4,7)-(5,6)
35. (5,6)-(5,4)
36. (5,0)-(6,1)
37. (5,2)-(5,4)
38. (5,2)-(6,1)
39. (5,4)-(6,3)
40. (6,1)-(6,3)

#### 5.3 Canonical mapping artifacts (recommended)
Define this artifact pipeline (even if not implemented yet):
- `layout/chromance_layout.svg` (authoring)
- `mapping/segments.json` (40 segments with endpoints in vertex coords, plus derived pixel coords)
- `mapping/wiring.json` (strip -> ordered segments + direction; source-of-truth for index order)
- generated `mapping/ledmap.json` for WLED
- generated `mapping/pixels.json` (optional richer map) with `ledIndex -> (x,y)` for our engine

#### 5.4 Projection from vertex coords to integer raster coords
We want an “exact” ledmap grid size that is deterministic.
Propose one default embedding and explain we can adjust after visual testing:

- Default projection:
  - `X = 28 * vx`
  - `Y = 14 * vy`
  (This preserves 60° directions well on an integer grid and ties edge length to 14 LEDs.)

Then specify the rule for placing 14 LEDs on each segment without collisions at vertices:
- For each segment endpoints (X0,Y0)->(X1,Y1), for k=0..13:
  - `t = (k + 0.5) / 14`  (half-step avoids landing on shared vertices)
  - `x = round(X0 + t*(X1-X0))`
  - `y = round(Y0 + t*(Y1-Y0))`

Then specify how to compute the *minimal* ledmap bounds:
- Generate all 560 (x,y) points first.
- Compute min/max bounds and set:
  - `width = max_x - min_x + 1`
  - `height = max_y - min_y + 1`
- Normalize coords by subtracting (min_x, min_y) so all are ≥ 0.

Finally specify WLED map fill:
- Initialize `map = [-1] * (width*height)`
- For each LED index i with coord (x,y):
  - `map[x + y*width] = i`

Add a note:
- If any collisions occur (two LEDs map to same cell), treat it as a bug; resolve by adjusting scale or stepping scheme.

### 6) Add “Mapping Validation Patterns” section
User will do a visual test once we have a suitable pattern.
Add two planned patterns:

#### 6.1 XY_Scan_Test
- Sweeps x/y over the ledmap grid; lights any non-(-1) cell briefly.
- Purpose: confirm spatial continuity, axis orientation, holes.

#### 6.2 Coord_Color_Test
- Colors each LED based on coordinate:
  - R = x normalized, G = y normalized, optional B = distance-to-center.
- Purpose: catch flips/swaps/scaling instantly.

Define acceptance criteria:
- Every physical LED lights exactly once in scan (no missing points).
- Gradient directions match expected left/right and top/bottom.
- No “teleporting” behavior during scan.

### 7) Strengthen control plane + preset schema discussion (Option C)
Update the doc to avoid “accidental reimplementation of WLED”:
- Propose a minimal preset schema now (JSON) even if we later adopt WLED presets.
- Explicitly list required fields: effectId, paletteId, brightness, speed, intensity, mapping id, strip/region selection, and versioning.

### 8) Keep the doc grounded in the repo
Wherever you describe current architecture, reference actual file paths and modules you find.
If you can’t find something, say so explicitly and describe assumptions.

---

## Output quality bar
- Be concrete. Add diagrams (Mermaid) where it helps.
- Keep it concise but complete; avoid overly generic statements.
- Ensure the updated doc reads as a cohesive plan and highlights the key decision tradeoffs (A/B/C).

## Acceptance criteria
- `docs/architecture/wled_integration_preplan.md` includes all new sections above.
- Chromance ledmap generation is specified clearly enough that an engineer can implement it.
- Visual validation patterns are documented.
- The doc makes it obvious what to spike first and how to decide between A/B/C.

