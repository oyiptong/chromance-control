# Agent Skills — Chromance Control Firmware

This file defines **explicit, mandatory agent skills** that must be followed during implementation.

These skills are **mechanical process constraints**, not suggestions. They exist to ensure traceability, auditability, and disciplined execution in an autonomous development workflow.

---

## Skill: Task Log Update

### Purpose

Ensure that all meaningful progress, decisions, and blockers are recorded in a single, append-only task log (`TASK_LOG.md`).

This skill enforces **engineering traceability**, not project management ceremony.

---

### Trigger Conditions (When This Skill MUST Run)

The agent **must** update `TASK_LOG.md` whenever **any** of the following occur:

1. A planned task is completed
2. Unit tests are added or modified
3. A design or architectural decision is made or revised
4. A blocker or unexpected issue is discovered
5. The interpretation of the spec or feedback changes
6. Mapping artifacts are generated or regenerated (e.g., `mapping/pixels.json`, `mapping/ledmap.json`, `include/generated/*.h`)
7. The mapping generator or wiring schema changes (e.g., `scripts/generate_ledmap.py`, `mapping/wiring*.json`)
8. Any “build hygiene” boundary is established or corrected (see Build Environment Hygiene)

If none of the above occurred, **do not update the task log**.

---

### Prohibited Triggers (When This Skill MUST NOT Run)

Do **not** update `TASK_LOG.md` for:

- Exploratory thinking or planning
- Reading or reviewing documents
- Asking clarifying questions
- Code that was started but not meaningfully progressed
- Pure refactors with no behavioral or structural impact

---

### Required Format

Each log entry must include:

- Date (YYYY-MM-DD)
- Status marker (one of the allowed markers)
- Short title
- Clear description of what changed and why
- Files touched (paths)
- Notes / decisions (if any)

Allowed status markers:
- 🟢 Done
- 🟡 In progress
- 🔴 Blocked
- 🔵 Decision
- 🧪 Test added

**Proof-of-life requirement (when applicable):**
- If a validation step was performed (e.g., mapping generation, invariants tests, header regeneration),
  include either:
  - a 1–3 line **summary output** (recommended), OR
  - a small **checksum/hash** of a key generated artifact, OR
  - the exact command run + a brief “PASS” result line.

Do not paste large logs. Keep this short and high-signal.

---

### Explicit Prohibitions

The agent must **not**:

- Rewrite, reorder, or delete existing log entries
- Add speculative, narrative, or promotional language
- Use vague statements like “worked on X” without describing the actual change

---

### Example (Correct)

- 2026-01-03 🧪 Test added — Validate ledmap invariants

What was done:
- Added invariant tests for mapping artifacts: unique LED indices, no collisions, correct bounds

Files touched:
- test/test_mapping_invariants.cpp
- scripts/generate_ledmap.py

Notes / Decisions:
- Treat any collision as fatal; require regen via generator only

Proof-of-life:
- generate_ledmap.py: PASS (LED_COUNT=560, collisions=0)

---

### Example (Incorrect — Do Not Do This)

- "Worked on diagnostics"
- "Made progress on tests"
- "Refactored some code"

---

## Skill: Spec-First Execution

### Purpose

Ensure changes are driven by the current specification and do not drift from the documented architecture.

### Rules

- Before starting any implementation work, the agent must read (at minimum):
  - `docs/architecture/wled_integration_implementation_plan.md`
  - Any referenced schemas or generator docs mentioned in the plan
- If the task touches mapping, also read:
  - `mapping/README_wiring.md` (if present)
  - `scripts/generate_ledmap.py` (or current generator location)
- If a discovered repo reality conflicts with the plan, the agent must:
  - Document the discrepancy in `TASK_LOG.md` (per Task Log skill)
  - Propose a concrete resolution (do not silently reinterpret requirements)

### Why this matters

Without spec-first execution, agents tend to re-introduce earlier decisions (e.g., runtime JSON parsing) or diverge on core contracts (index ordering), creating costly rework.

---

## Skill: Generator-Owned Mapping Artifacts

### Purpose

Keep mapping artifacts reproducible and prevent “hand-edited generated files” from drifting out of sync with the generator and wiring source-of-truth.

### Rules

- Treat `mapping/wiring.json` (and `mapping/wiring_bench.json` if used) as **source-of-truth** inputs.
- Treat the following as **generated outputs** and **never hand-edit** them:
  - `mapping/pixels.json`
  - `mapping/ledmap.json`
  - `include/generated/chromance_mapping_*.h`
- If an output needs to change, the agent must change either:
  - the generator (`scripts/generate_ledmap.py`), or
  - the wiring input (`mapping/wiring*.json`),
  then regenerate outputs.
- Any PR that changes mapping outputs must also include:
  - The generator invocation used
  - A validation result (see Mapping Invariants Gate)
  - A short proof-of-life snippet in `TASK_LOG.md` (see Task Log skill)

### Why this matters

Generated artifacts are easy to “patch” manually, but doing so breaks reproducibility and makes future regeneration overwrite manual fixes.

---

## Skill: Wiring Templates Must Carry Confidence Markers

### Purpose

Prevent plausible-but-wrong wiring from being mistaken as authoritative, and make manual correction straightforward.

### Rules

- When generating or updating `mapping/wiring.json` or `mapping/wiring_bench.json`:
  - Include `mappingVersion` and `isBenchSubset`.
  - Use `_comment` fields to guide manual editing.
  - For each segment entry, include a confidence marker in `_comment`, e.g.:
    - `CONFIDENCE: DERIVED from <file>:<symbol/line>`
    - `CONFIDENCE: PLACEHOLDER - requires physical validation`
- If generating a placeholder order, explicitly state that in the top-level `_comment` and in `mapping/README_wiring.md`.
- Ensure `mapping/README_wiring.md` includes a **Topology Key** (segment ID → vertex endpoints) so humans can identify physical tubes.

### Why this matters

Without explicit confidence markers and topology lookup, it’s easy to treat a template as “real wiring,” leading to wasted debugging time and incorrect ledmaps.

---

## Skill: Mapping Invariants Gate

### Purpose

Guarantee that any mapping-related change is structurally correct before it reaches firmware/WLED, preventing silent mis-maps.

### Required invariants

**Wiring invariants (full wiring):**
- 4 strips present
- Segment IDs `{1..40}` appear **exactly once** total
- `dir` is only `a_to_b` or `b_to_a`
- `isBenchSubset` is `false`

**Wiring invariants (bench wiring):**
- `isBenchSubset` is `true`
- Segment IDs are a strict subset of `{1..40}` with **no duplicates**
- Strip count may be 1 (typical) or more (optional), but must be explicit

**Generated mapping invariants:**
- LED indices cover `0..LED_COUNT-1` exactly once
- No coordinate collisions (no two LEDs share the same raster cell)
- For ledmap: `map.length == width * height` and only `-1` or valid indices appear
- Generated header tables share the same `LED_COUNT` and align by index:
  - `rgb[i]` ↔ `pixel_x/y[i]` ↔ `global_to_strip/local[i]`

### Validation actions (must run on mapping-related changes)

- Run generator validation (or equivalent) and ensure it completes without errors (no collisions, correct counts).
- Run native/unit tests that assert wiring and mapping invariants (if present).
- If tests are not yet implemented, add a TODO entry in `TASK_LOG.md` and treat mapping outputs as provisional until invariants are enforced by tests.

### Why this matters

Mapping bugs often look like effect bugs. Enforcing invariants early prevents long debug cycles and protects WLED 2D mapping correctness.

---

## Skill: Build Environment Hygiene

### Purpose

Prevent platform-specific dependencies from leaking into the portable core, ensuring unit tests remain runnable on the host and avoiding “Arduino.h not found” failures.

### Rules

**Core isolation:**
- Files in `src/core/**` must **never** include Arduino/ESP/FastLED headers such as:
  - `<Arduino.h>`
  - ESP-IDF headers
  - `<FastLED.h>`
  - any `WiFi*` / `AsyncWebServer*` headers
- `src/core/**` must compile cleanly in the native test environment.

**Platform isolation:**
- Files in `src/platform/**` may include Arduino/ESP-IDF specifics and are firmware-only.

**Environment selection (no implicit fallbacks):**
- Do not rely on default PlatformIO environments.
- Always specify an environment explicitly:
  - Use `pio test -e <native-env>` for core logic and mapping verification.
  - Use `pio run -e <firmware-env>` for firmware compilation/flash.
- Use the environment names defined in `platformio.ini` (e.g., `native`, `diagnostic`, `runtime`).

**Boundary enforcement:**
- If a change causes `env:native` tests to fail due to platform includes, revert the include leakage by:
  - moving platform calls behind an interface in `src/platform/**`, OR
  - introducing a small `platform_*` abstraction boundary.

### Why this matters

The implementation plan depends on portable core code. Build hygiene ensures fast host tests remain reliable and prevents slow, fragile debug cycles on device.

---

## Enforcement

These skills are **mandatory**.

Failure to comply indicates incomplete or unreliable implementation and must be corrected before work proceeds.

