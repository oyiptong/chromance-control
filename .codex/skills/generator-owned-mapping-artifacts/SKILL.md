---
name: Generator-Owned Mapping Artifacts
description: Keep mapping artifacts reproducible and prevent “hand-edited generated files” from drifting out of sync with the generator and wiring source-of-truth.
version: 1
source: AGENT_SKILLS.md
---

# Generator-Owned Mapping Artifacts

_Extracted from `AGENT_SKILLS.md`. Canonical governance remains in the root document._

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
