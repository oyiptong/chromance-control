You are a senior firmware/software engineer responsible for maintainable, evolvable embedded systems.

You are expected to:
- read and reason about existing code before proposing changes,
- clearly separate current behavior from proposed design,
- document tradeoffs and justify decisions,
- design APIs that support future control surfaces (web UI) without premature implementation,
- think in terms of persistence, migration, and long-term ownership,
- optimize for embedded constraints (RAM/Flash/CPU), and call out performance/memory tradeoffs explicitly.

Hard constraint (frame budget):
- Assume the main render loop has a tight frame budget.
- Do NOT introduce per-frame dynamic allocation, string parsing, or heavy introspection.
- Prefer compile-time/static tables and fixed-size buffers.
- If any heap usage is proposed, it must be outside the render loop and justified.

If you cannot find code for a claimed behavior, you MUST say so explicitly and show:
- the searches you performed (e.g. ripgrep commands),
- the closest related code you found,
- whether your conclusion is confirmed or inferred.

---

## Task
Create a **design + implementation document** for refactoring the Chromance effects library so it is **web-UI ready** (configurable effects with persisted settings), **without implementing the web UI**.

Write the document to:

$ROOT/chromance_effects_refactor_design_document.md

This document must serve **two purposes**:
1. A clear explanation of the *current implementation* and why it is structured the way it is.
2. An *implementation-grade refactor design* that could be executed directly in future PRs.

---

## High-level requirements

### Functional goals
The refactored system must support:
- An extensible **effects catalog** (larger than numeric 1–7 selection).
- For the active effect:
  - Typed, discoverable configuration parameters.
  - Persistence across reboot (firmware-side).
- Ability to:
  - Restart the current effect (reinitialize runtime state).
  - Enter effect-defined **sub-stages** (modes / steps).
- A design compatible with a future web UI (enumeration + schema + control hooks) without implementing UI.

### Existing constraints & context
- Current effect selection via serial keys `1–7`.
- Existing shortcuts:
  - `n/N`, `s/S`, `ESC`, `+/-` (brightness).
- Serial output currently used heavily for debugging; direction is:
  - serial as a **control surface**, not just logs.
- Debug logging may eventually be toggleable globally (e.g. `v` for verbose), but:
  - do NOT implement it,
  - include it only as context for **global vs effect-specific commands**.

### Embedded/performance constraints (MCU)
This is running on an MCU. The design doc MUST explicitly address:
- RAM usage (static vs dynamic allocations, worst-case bounds).
- Flash footprint tradeoffs (tables/strings/metadata).
- CPU overhead per frame/tick (dispatch, parameter access, persistence write frequency).
- Persistence wear and write amplification concerns (NVS/EEPROM) and mitigation (debounce, batching).
- Avoiding heap fragmentation and allocation churn in the main loop.
- How to keep the “UI-ready” metadata lightweight (compile-time vs runtime reflection, string storage strategies, PROGMEM/flash strings if applicable).
- Provide at least one “minimal footprint mode” option and one “more flexible mode” option.

When you propose design choices, include:
- expected overhead,
- why it is acceptable,
- and alternatives for tighter constraints.

If exact MCU specs are not available in the repo, state assumptions and provide ranges.

---

## Deliverable: REQUIRED DOCUMENT STRUCTURE

### 1) Context & non-goals
- Summarize what exists today (effects, shortcuts, logging, persistence or lack thereof).
- Explicitly list **non-goals** (web UI, debug toggle implementation, protocol finalization, etc).

---

### 2) Current implementation (code-grounded)
Describe the current system exactly as it exists today.

You MUST:
- Identify where:
  - effect selection lives,
  - effects are instantiated / updated,
  - serial/keyboard input is read,
  - shortcuts are handled,
  - brightness control is implemented,
  - effect state is (or is not) persisted.
- Include **extensive code snippets** (not just file paths).
- When describing behavior, cite the exact code responsible.

If behavior is unclear:
- mark it explicitly as **inferred**,
- explain why,
- show the search you performed.

Include:
- a control-flow diagram (Mermaid acceptable).

---

### 3) Problems & motivation for refactor
Enumerate pain points, including:
- numeric effect selection limits,
- hard-coded shortcuts,
- unclear global vs effect-specific commands,
- lack of persisted configuration,
- serial output being debugging-oriented rather than control-oriented,
- inability to enumerate effect parameters programmatically,
- any current performance concerns (logging overhead, dynamic allocations, large buffers, etc).

Tie each problem back to code structure.

---

### 4) Target architecture overview (UI-ready, UI-agnostic)
Propose a new architecture that includes:

- **Effect Registry / Catalog**
  - Stable effect IDs
  - Enumerability for UI and serial control

- **Effect Interface**
  - Lifecycle methods (init/start/update/render/stop/reset)
  - Optional staged/sub-state support

- **Configuration Model**
  - Typed parameters (int, float, bool, enum, color, etc)
  - Min/max/step/default metadata
  - Stable parameter identifiers
  - Clear separation between:
    - runtime state
    - persisted configuration

- **Persistence Layer**
  - storage mechanism
  - versioning & migration strategy

- **Command Routing**
  - global commands vs effect-scoped commands
  - shortcut mapping layer
  - shared control API for serial now, web UI later

Include a Mermaid diagram of the proposed architecture.

---

### 5) Detailed APIs (implementation-grade)
This section must be detailed enough to implement directly.

Include:
- C++ interfaces / structs (with examples):
  - `IEffect`
  - `EffectContext`
  - `EffectRegistry`
  - `EffectId`
  - `EffectConfigSchema`
  - `ParamDescriptor`
  - `ParamValue`
- Sub-stage design:
  - recommended approach (e.g. stage IDs + enter/list APIs),
  - at least one alternative design.
- Effect restart semantics:
  - what resets,
  - what persists.
- Brightness integration:
  - global vs per-effect,
  - conflict resolution.

List proposed file paths under `$ROOT`.

---

### 6) Persistence design (implementation-grade)
- Choose a persistence mechanism consistent with the project (Preferences/NVS/EEPROM/file/etc).
- Define:
  - key namespaces,
  - schema versioning,
  - per-effect storage layout,
  - migration strategy.
- Include pseudocode / example code for load/save.
- Define when persistence occurs (on change, debounce, explicit save, etc).
- Define failure and recovery behavior.
- Address endurance: how you minimize writes and avoid wear.

---

### 7) Shortcut & serial streamlining plan
- Explain how shortcuts work today (with code references).
- Propose a mapping layer:
  - stable global shortcuts,
  - extensible effect-specific commands.
- Outline a minimal serial control direction (examples only):
  - `effect set <id>`
  - `param set <name> <value>`
  - `effect restart`
  - `stage enter <id>`
- Do NOT implement the protocol.

---

### 8) Embedded performance & memory tradeoffs (REQUIRED)
Add a dedicated section that includes:
- A table of the main design components (registry, schema/metadata, routing, persistence) and their:
  - RAM cost
  - Flash cost
  - CPU cost per tick/event
- Identify potential hotspots and how to mitigate them.
- Compare at least two approaches for parameter/schema storage:
  1) compile-time descriptors (static tables, fixed arrays, IDs),
  2) runtime/dynamic descriptors (more flexible, more overhead),
  and recommend one.
- Discuss string storage strategies (IDs vs human names; storing names in flash; optional stripping in release builds).
- Explicitly s

