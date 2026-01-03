# AGENTS.md — Chromance / WLED Integration

This file is the entrypoint for coding agents working in this repo.

## Read-first (mandatory)
Before doing any work, read these in order:

1) `docs/architecture/wled_integration_implementation_plan.md`
2) `AGENT_SKILLS.md` (canonical governance + process rules)
3) If the task touches mapping:
   - `mapping/README_wiring.md` (if present)
   - `scripts/generate_ledmap.py` (current generator)

If anything in the repo conflicts with the implementation plan, **do not silently reinterpret**.
Log the discrepancy in `TASK_LOG.md` and propose a resolution.

## Prompts (task-specific)
This repo may include prompt templates to run Codex/agents for specific deliverables.

- If your task involves invoking an agent using a prepared prompt (e.g., to generate wiring templates, implementation artifacts, etc.), read:
  - `wled_implementation_prompt.md`

Prompts are **not canonical architecture**. If a prompt conflicts with
`docs/architecture/wled_integration_implementation_plan.md`, follow the plan and
update the prompt/template accordingly.

---

## Core principles (non-negotiable)

### 1) Spec-first execution
Work must follow `docs/architecture/wled_integration_implementation_plan.md`.
No re-architecting without updating the plan.

### 2) Generator-owned mapping artifacts
Never hand-edit generated files:
- `mapping/pixels.json`
- `mapping/ledmap.json`
- `include/generated/chromance_mapping_*.h`

To change them, update:
- `mapping/wiring*.json`, or
- `scripts/generate_ledmap.py`,
then regenerate.

### 3) Wiring files are authoritative but may start as templates
If wiring order/direction cannot be derived from repo tables, generate **templates** only.
Templates must include:
- `isBenchSubset` field
- `_comment` confidence markers per segment (`DERIVED` vs `PLACEHOLDER`)

### 4) Build environment hygiene
- `src/core/**` must compile in native tests (no Arduino/ESP headers).
- `src/platform/**` is firmware-only.
Always run explicit PlatformIO environments (no defaults).

### 5) Task log traceability
Update `TASK_LOG.md` when meaningful progress/decisions/tests/generator runs occur.
Include a short “proof-of-life” output (1–3 lines) for validations.

---

## Standard workflow (follow unless the task says otherwise)

1) **Understand**
   - Read required docs (above)
   - Locate relevant repo files
2) **Plan changes**
   - Identify files to touch
   - Identify which envs compile them (`native`, `diagnostic`, etc.)
3) **Implement**
4) **Validate**
   - `pio test -e <native-env>` for core/invariants
   - `pio run -e <firmware-env>` for firmware builds (when relevant)
   - If mapping touched: run generator + invariants
5) **Log**
   - Append to `TASK_LOG.md` with status + files touched + proof-of-life

---

## Repo conventions (quick reference)

### Mapping
- Source-of-truth: `mapping/wiring.json` (+ `mapping/wiring_bench.json` if used)
- Generated:
  - `mapping/pixels.json`
  - `mapping/ledmap.json`
  - `include/generated/chromance_mapping_full.h`
  - `include/generated/chromance_mapping_bench.h`

### Bench vs Full selection
- Controlled by build flag: `CHROMANCE_BENCH_MODE`
- Bench build uses the bench header; full build uses full header

### Validation patterns
- `XY_Scan_Test`
- `Coord_Color_Test`
- (Optional later) `Index_Walk_Test`

---

## When uncertain
- Prefer explicit TODOs + questions in `TASK_LOG.md` over guessing.
- Never invent wiring order. Use confidence markers.
- Keep changes minimal and aligned with the implementation plan.

