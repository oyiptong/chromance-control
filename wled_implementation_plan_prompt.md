```md
You are a senior firmware + embedded architecture lead. Your task is to produce a **detailed implementation plan** by FIRST reading the existing pre-planning document, then mapping every planned feature into concrete repo changes.

## Source of truth (MUST READ FIRST)
1) Open and read:
   `docs/architecture/wled_integration_preplan.md`
2) Treat it as the primary source of truth for:
   - goals, constraints, options A/B/C
   - Chromance mapping approach + segment list
   - mapping generator and artifacts (`pixels.json`, `ledmap.json`, `wiring.json`)
   - validation patterns
   - FPS configurability guidance

If you find inconsistencies between the repo and the pre-plan, document them and propose a resolution, but do not silently change requirements.

## Repo grounding (MUST)
After reading the pre-plan:
- Inspect the repo to identify real file paths and existing modules (loop, LED drivers, config, mapping script, tests).
- The plan must reference **actual paths**. If a module doesn’t exist, state it and propose where it should live.

## Deliverable (single document)
Create:
`docs/architecture/wled_integration_implementation_plan.md`

This is a plan only. Do NOT implement features in this task.

---

## IMPORTANT SCOPE CONSTRAINTS (MUST FOLLOW)
For this implementation plan:
- **DO NOT implement or deeply plan for networking responsiveness**, WLED UI behavior under load, liveview/peek streaming, or packet-loss testing.
- **DO NOT implement sound or heartbeat/biofeedback inputs now.**
- HOWEVER, the architecture MUST be modular enough to add networking + sound/biofeedback later without changing effect signatures or refactoring core modules.

Concretely, you MUST include:
- A `Signals` / `ModulationInputs` struct (initially empty or defaulted) passed into effects.
- A `ModulationProvider` interface (stubbed for now) that can later supply audio/BPM/breath-phase.
- A strict separation between `core/` (portable, pure-ish logic) and `platform/` (hardware).

You SHOULD still plan basic performance instrumentation:
- `flush_ms` and total `frame_ms` measurement hooks
(but not network tests).

---

## Required coverage (must be comprehensive within the scope)
Your plan must cover:

1) **WLED integration strategy**
   - Options A/B/C exactly as framed in the pre-plan
   - Spike(s) to decide between options using measurable criteria already in the pre-plan
   - Concrete touchpoints: what code is kept/replaced per option
   - (No network responsiveness testing plan beyond basic viability; keep it minimal)

2) **Portable effects engine (core)**
   - Effect registry + selection model
   - Parameter model (speed/intensity/palette/etc.)
   - Runtime FPS cap (`target_fps`; 0=uncapped) implemented by a frame scheduler
   - Deterministic timing hooks (time passed in; no sleeping inside effects)
   - `Signals` struct + `ModulationProvider` interface (stubs) for future sound/biofeedback

3) **Chromance mapping pipeline (must be code-mapped)**
   - 7×9 vertex lattice, 40 segments × 14 LEDs, 560 LEDs, 4 strips
   - Inputs/outputs:
     - `mapping/wiring.json` (authoritative order + direction)
     - `mapping/pixels.json` (ledIndex -> x,y [+ recommended metadata])
     - WLED `mapping/ledmap.json` (width/height/map sparse matrix)
     - generator script path (e.g. `scripts/generate_ledmap.py`)
   - Exact algorithms (projection, half-step sampling, minimal bounds, normalization)
   - Mapping invariants + unit tests
   - Mapping validation patterns: `XY_Scan_Test` and `Coord_Color_Test`

4) **Output layer / multi-strip integration**
   - Abstract 4 strips cleanly (per-strip buffers or unified buffer)
   - Plan to verify HW vs SW SPI in the current driver (inspection + flush timing)
   - Basic perf instrumentation (`flush_ms`, `frame_ms`) and acceptance checks
   - Path to swap driver implementations later if needed (keep the interface stable)

5) **(Optional but planned) Web simulator**
   - Shared-core boundary: compile ONLY the effects core to WASM (not firmware/platform)
   - JS viewer renders from `pixels.json`
   - Simulator uses the same `target_fps` concept
   - Keep “live device preview” as a FUTURE extension only (mention as later work, no detail)

---

## Required document structure (use these headings in this order)

### 1) Scope & non-goals
- Explicitly list deferred items: networking responsiveness, audio, heartbeat.

### 2) Milestones and sequencing (4–7 milestones)
For each milestone: deliverables, acceptance criteria, risks/mitigations, complexity (low/med/high; no time estimates).

### 3) Current repo map (as-is)
- Relevant modules and file paths (loop, LEDs, config, mapping script, tests).

### 4) Target architecture (to-be)
- Module boundaries:
  - `core/effects/`
  - `core/mapping/`
  - `core/scheduler/`
  - `core/modulation/` (interfaces only for now)
  - `platform/led/`
  - `platform/time/`
  - `platform/storage/` (if needed)
- Mermaid diagram of frame data flow.

### 5) File-by-file implementation plan (MOST IMPORTANT)
For every new/modified file:
- path
- purpose
- public API (pseudo-signatures)
- key logic steps
- tests to add

**Additional requirement:** For key algorithms (mapping generation/validation, frame scheduler, effect registry, mapping lookup), include **pseudo-code or strict step-by-step logic** in the file entry. Avoid generic phrasing like “implements logic”; describe the algorithm.

Include at minimum:
- effects registry
- frame scheduler (target_fps)
- mapping runtime structures (load pixels / compile-time table, lookups)
- mapping validation patterns integration
- instrumentation points (`flush_ms`, `frame_ms`)
- where the generator script lives and how it is used in workflows/CI

### 6) Mapping pipeline: exact algorithms & schemas
- Restate the pre-plan’s segment list and the generator approach.
- Document `wiring.json` schema clearly.
- Document `pixels.json` schema (include recommended metadata fields if practical).
- Document WLED `ledmap.json` emission format.
- Collision policy and how failures are reported.

### 7) Validation & testing strategy
- mapping invariants tests
- deterministic effect tests (optional snapshots)
- on-device manual checklist (XY scan, gradient, index-walk optional)
- perf measurement checklist (flush/frame)

**Additional requirement:** Define **Physical Test Rig** requirements and include a **Bench Mode** plan:
- what can be tested with a single strip (or subset of LEDs)
- what requires the full sculpture
- how Bench Mode is enabled (config flag / compile-time define / wiring.json subset)

### 8) WLED integration plan (A/B/C) with decision criteria
- concrete steps per option
- which modules are reused vs replaced
- decision gates aligned to the pre-plan’s spike criteria
- keep networking considerations minimal (out of scope)

**Additional requirement:** For Option B (replace with WLED), explicitly list **which existing repo files/modules become obsolete** and would likely be deleted or archived. Label them as “likely obsolete” if uncertain.

### 9) Web simulator plan (optional milestone)
- directory layout (e.g. `tools/sim/`)
- WASM toolchain choice (Emscripten or wasm-pack) + rationale
- minimal ABI between JS and WASM
- viewer rendering approach (Canvas/SVG)
- how it loads `pixels.json`
- how it runs mapping validation patterns

### 10) Risk register
- multi-output APA102 feasibility (WLED and non-WLED)
- signal integrity / SPI clock stability
- mapping rasterization collisions
- maintainability / WLED fork risk

---

## Quality bar / rules
- Be explicit; avoid vague “TBD.”
- When unknowns exist, specify what to measure and pass/fail thresholds.
- Keep the plan aligned with the pre-plan; cite relevant sections by name (not long quotes).
- Do NOT implement features—plan only.

## Acceptance criteria
- `docs/architecture/wled_integration_implementation_plan.md` exists.
- It clearly derives from `docs/architecture/wled_integration_preplan.md`.
- It provides a concrete, file-by-file plan with APIs and tests.
- It respects the scope constraints (no deep network/audio planning) while keeping modular seams for later.
```

