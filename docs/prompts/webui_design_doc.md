You are a senior embedded systems architect and lead implementation engineer.

Your task is to produce a **Design Document + Comprehensive Implementation Plan**
for the Chromance firmware-hosted Web UI, using the finalized PRE-PLAN at:

  ./docs/plans/webui_preplan.md

You MUST treat the PRE-PLAN as **authoritative and binding**.
You are NOT allowed to reinterpret, relax, or redesign any architectural decisions.

Your output MUST be written to:

  ./docs/plans/webui_design_doc.md

────────────────────────────────────────
DOCUMENT PURPOSE
────────────────────────────────────────

This document has TWO audiences:

1) The project owner (me)
   - I will read this to verify architectural soundness, risk containment,
     and alignment with ESP32 / OTA / render-loop constraints.

2) A coding agent
   - This document must be detailed enough to serve as a **direct
     implementation guide**, with no guesswork, missing steps, or
     ambiguous ownership.

If an engineer follows this document verbatim, the implementation should
match the pre-plan exactly.

────────────────────────────────────────
WHAT NOT TO INCLUDE
────────────────────────────────────────

Do NOT include:
- Marketing copy or user-facing documentation
- Explanations of basic technologies (Astro, Tailwind, ESP32, etc.)
- Alternative designs or optional approaches
- New features not present in the PRE-PLAN
- TODOs, TBDs, placeholders, or speculative language
- Verbose commentary that does not directly enable implementation

────────────────────────────────────────
NON-NEGOTIABLE CONSTRAINTS
────────────────────────────────────────

You MUST preserve ALL constraints from the PRE-PLAN, including but not limited to:

- Target: ESP32 Feather HUZZAH32, 4MB flash, OTA enabled
- Partitioning:
  - You MUST explicitly select and document a partition scheme
    (e.g. `min_spiffs.csv` or custom) that prioritizes APP/OTA space
    over SPIFFS, since UI assets are embedded in flash
- UI stack (fixed):
  - Astro (static prerendered)
  - Tailwind CSS (purged)
  - daisyUI (SINGLE theme only)
  - Preact islands (leaf widgets only)
- Asset budgets:
  - Soft: 200 KB gzipped total
  - Hard: 300 KB gzipped total (build MUST fail)
- Persistence model:
  - Per-effect blob: key = `eXXXX`, size = 64 bytes
  - Global keys: `aeid`, `bright_pct`, `mode`
  - Per-effect selector only (no dump-all)
- Synchronous `WebServer` with:
  - Render-loop scheduling gate (~7ms headroom)
  - Chunked static asset sending (>1024 bytes)
  - Strict JSON emission rules (stream vs bounded; no unbounded String concat)
- No SPA router, no global state store, no heavy frontend dependencies
- OTA safety, CPU minimization, bounded RAM usage

You MUST NOT:
- Introduce new features
- Change architectural decisions
- Suggest alternate stacks
- Defer critical decisions to “implementation time”

────────────────────────────────────────
DIAGRAM REQUIREMENT (MANDATORY)
────────────────────────────────────────

Diagrams are REQUIRED and MUST be created using **MermaidJS only**.

Use fenced blocks like this (escaped example):

\`\`\`mermaid
flowchart TD
  A --> B
\`\`\`

Required diagram types:
- `flowchart` — system and build flow
- `sequenceDiagram` — API interactions
- `stateDiagram-v2` — chunked static asset sender state machine

Do NOT embed images.
Do NOT reference external diagram tools.

────────────────────────────────────────
REQUIRED DOCUMENT STRUCTURE
────────────────────────────────────────

Your output MUST follow this structure exactly:

────────────────────
1. Executive Summary
────────────────────
- Concise, high-level description
- Key architectural principles
- Why this design is safe for ESP32 + OTA + LED render loop
- Explicit call-out of highest-risk areas and mitigations

────────────────────────────────────
2. System Architecture Overview
────────────────────────────────────
- Firmware / Web UI / Build pipeline overview
- MermaidJS architecture and data-flow diagrams
- Ownership boundaries (core vs platform vs UI)
- Scheduling model (render loop vs HTTP handling)

────────────────────────────────────
3. Detailed Technical Design
────────────────────────────────────

3.1 HTTP Server Architecture
- `WebServer` lifecycle
- Render-loop scheduling gate (policy-level description)
- Static asset serving strategy:
  - MUST include a MermaidJS **stateDiagram-v2**
  - MUST describe the chunked sender as a state machine
- Dynamic route handling:
  - MUST describe a **Unified Dispatcher** strategy
    (e.g. routing via `onNotFound` rather than per-slug handlers)

3.2 API Design
- Endpoint-by-endpoint specification
- Request/response schemas:
  - Use JSON-schema-like or TypeScript-style type notation
  - Include example request/response pairs
  - Include error response shapes with explicit error codes
- Rate limiting behavior
- JSON emission strategy (stream vs bounded buffer)

3.3 Persistence & NVS Strategy
- Key layout
- Blob versioning and migration behavior
- Wipe-all behavior
- Safety guarantees

3.4 Frontend Architecture
- Astro project structure
- Page shells
- Preact island boundaries
- Forbidden dependency enforcement
- UI behavior per page

3.5 Build & Asset Pipeline
- Astro build flow
- Gzip process
- Embedded asset generation
- Asset budget enforcement
- OTA partition safety checks
- Explicit partition scheme selection and justification

────────────────────────────────────
4. Comprehensive Implementation Plan
────────────────────────────────────

THIS SECTION MUST BE VERY DETAILED.

- Step-by-step implementation breakdown
- Explicit file paths to create or modify
- Function names, class names, and symbols
- Code snippets MUST be included where appropriate

Code snippet rules:
- Use fenced code blocks with language tags (e.g. ```cpp, ```js)
- For new files: provide complete contents
- For modified files: clearly mark insertion points
- Do NOT include pseudocode except for the chunked sender state machine

4.1 New Files to Create
- Path
- Purpose
- Key symbols
- Internal responsibilities
- For the chunked static asset sender:
  - Provide a pseudo-code outline of the state machine

4.2 Existing Files to Modify
- Path
- What to change
- Why the change is required

4.3 Web UI Project Setup (`webui/`)
- Exact directory tree
- `package.json` with locked dependency versions
- Complete `astro.config.mjs`
- Complete `tailwind.config.js` with single daisyUI theme
- Preact island layout
- npm scripts

4.4 PlatformIO Integration
- Pre-build script flow
- Asset generation logic
- Size enforcement logic
- Failure conditions
- Example successful build output

────────────────────────────────────
5. Testing & Verification Plan
────────────────────────────────────
- Firmware tests (specific cases)
- Frontend tests (specific island interactions)
- Build-time checks
- Asset budget failure scenarios
- Render-loop safety verification methodology

────────────────────────────────────
6. Risk Register & Mitigations
────────────────────────────────────
- Known risks
- How each is mitigated by design
- What to watch during implementation

────────────────────────────────────
7. Acceptance Criteria
────────────────────────────────────
- Concrete checklist of conditions that MUST be met
- Measurements and invariants that must hold
- Conditions under which the implementation is rejected

────────────────────────────────────────
SELF-CHECK BEFORE SUBMISSION
────────────────────────────────────────

Before finalizing, verify:
☐ Every MUST from the PRE-PLAN is addressed
☐ Every new file has complete contents
☐ Every modified file shows exact changes
☐ Every API endpoint has schemas and examples
☐ Chunked sender has a state diagram
☐ Partition scheme is explicitly selected
☐ No placeholders, TODOs, or speculative language remain
☐ A coding agent could implement without asking questions

────────────────────────────────────────
FINAL INSTRUCTION
────────────────────────────────────────

Generate the complete **Design Document + Comprehensive Implementation Plan**
and write it to:

  ./docs/plans/webui_design_doc.md

The document must be self-contained, technically precise, and directly actionable.

