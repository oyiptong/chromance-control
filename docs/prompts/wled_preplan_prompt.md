You are a senior firmware + embedded architecture reviewer. Your task is to produce a *pre-planning architecture document* for integrating (or replacing with) WLED in this repository.

## Goal
Write a planning document that:
1) Explains the *current* firmware/app architecture as it exists in this repo (render loop, pattern system, mapping, config, IO, networking, etc.).
2) Proposes integration choices for WLED:
   - **Option A:** WLED as a sister module (keep current core, embed/interop with WLED components)
   - **Option B:** Replace everything with WLED (fork / upstream-friendly)
   - **Option C:** Hybrid / external control (e.g., keep our firmware, use WLED protocol outputs or drive WLED via network)
3) Enables future implementation of a **library of patterns** and a **2D physical mapping** system.
4) Accounts for **sound-reactive and biofeedback inputs** (e.g., heart rate) that modulate effects.

## Context / Requirements
Patterns to support (initial set):
- **Breath comets pattern:** “breath in” = chase-like comets moving toward center; “breath out” = comets radiating outward in waves from center.
- Additional effects (leave extensible; define how new effects plug in).

Inputs / modulation:
- Sound-reactive features (mic / FFT / envelope follower, etc. depending on repo + hardware).
- Biofeedback: heartbeat rate (BPM) that can modulate speed/intensity/brightness/palette. Assume HR input could arrive via BLE / serial / analog / network—design should be adaptable.

Mapping:
- Must include a **physical mapping of an irregular LED layout into a 2D array** (or 2D coordinate map), supporting:
  - serpentine rows, gaps/holes, non-rectangular shapes
  - mapping from (x,y) -> LED index and LED index -> (x,y)
  - “center” concept (for breath effects), plus optional named regions/segments
  - future: multiple panels / segments / strips

## Instructions
### 1) First: Inspect the repository
- Read the top-level README and any docs related to firmware, LEDs, patterns, mapping, networking, or sensors.
- Locate the core rendering loop and where patterns/effects are registered and updated.
- Identify how configuration is stored/loaded (compile-time constants, JSON, NVS, web UI, etc.).
- Identify how LED output is handled (FastLED/NeoPixelBus/etc), frame timing, and any existing mapping abstraction.
- Identify any existing sound/biofeedback code paths.

### 2) Deliverable: a single Markdown document
Create: `docs/architecture/wled_integration_preplan.md`

The document must include these sections:

#### A. Executive summary (1–2 pages)
- What exists today, what we want, and the key architectural decision(s).
- A concise recommendation (even if “recommend doing a spike first”), plus why.

#### B. Current architecture overview (as-is)
Explain with diagrams (use Mermaid where helpful):
- Main loop / frame pipeline: timing, update frequency, where effects compute pixels.
- Pattern/effect model: how patterns are defined, selected, parameterized, transitioned.
- Data model: how pixels are represented (CRGB arrays, buffers), brightness scaling, gamma, palettes.
- Physical mapping: what exists today, what’s missing.
- Inputs: sound, biofeedback, UI/network.
- Build/deploy: target MCU (ESP32 assumed unless repo indicates otherwise), toolchain, constraints.

Be specific: reference the actual files/modules you found.

#### C. Constraints & design goals
- CPU/memory budget (ESP32 class), frame rate targets, LED count assumptions.
- Determinism (no stutters), safety (watchdog), debuggability.
- Extensibility: adding new patterns, new sensors, new mappings.
- Maintainability: avoid deep forks if possible; keep boundaries clean.

#### D. Integration options (A/B/C) with tradeoffs
For each option:
- What code we keep vs replace
- How patterns are authored and registered
- How mapping is handled
- How sensors (sound/heartbeat) feed into effects
- Web/UI implications (WLED UI vs custom)
- Update strategy (staying compatible with upstream WLED)
- Risks (merge pain, flash size, RAM, timing conflicts)
- “What a first milestone looks like”

Include a comparison table and clear decision criteria.

#### E. Proposed target architecture (to-be)
Regardless of option, propose a clean architecture that includes:
- **Effect engine** (pure-ish effect function: inputs + time + params -> pixel buffer)
- **Modulation layer** (sound envelope/FFT features, BPM signal, “breath phase” signal) producing normalized control signals
- **Mapping layer** (2D coordinate system + center + regions)
- **Output layer** (LED driver, dithering/gamma/brightness, power limits)
- **Control/UI layer** (effect selection, parameter editing, presets, networking)

Define:
- Key interfaces/types (pseudo-code is fine)
- Data flow per frame
- Threading/RTOS assumptions (if any)
- Configuration/preset format proposal

#### F. Mapping model specification
Design a mapping representation that supports irregular 2D grids.
Include:
- Example data structure for mapping table (x,y -> index, index -> x,y)
- How to encode holes (no LED) and multi-strip layouts
- How to compute “center” and radial distance fields used by breath patterns
- A plan for generating/validating the mapping (tooling suggestions welcome)

#### G. Effect authoring guide (for future implementation)
- How a new effect should be added (file location, registration, parameters)
- Suggested parameter schema (speed, density, palette, direction, center bias)
- Testing strategy: unit tests for mapping + deterministic effect snapshots

#### H. Implementation roadmap
- 3–5 milestones with clear deliverables
- “Spike” tasks needed to decide A/B/C (e.g., build WLED in our toolchain, measure RAM, confirm driver compatibility)
- Risks + mitigations

### 3) Output quality bar
- Do *not* implement code changes beyond adding the Markdown doc (unless you must add tiny helper notes/scripts to understand architecture—avoid).
- Be explicit and concrete: cite file paths, modules, and explain the “why.”
- Prefer simple, modular boundaries that keep us flexible.

## Acceptance criteria
- `docs/architecture/wled_integration_preplan.md` exists and is comprehensive.
- It accurately summarizes current architecture with file references.
- It offers clear WLED integration choices with tradeoffs and a recommendation.
- It includes a robust plan for 2D physical mapping and sensor-driven modulation.

