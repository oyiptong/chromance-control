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

1) **The project owner (me)**  
   - I will read this to verify architectural soundness, risk containment,
     and alignment with ESP32 / OTA / render-loop constraints.

2) **A coding agent**  
   - This document must be detailed enough to serve as a **direct
     implementation guide**, with no guesswork, missing steps, or
     ambiguous ownership.

If an engineer follows this document verbatim, the implementation should
match the PRE-PLAN exactly.

────────────────────────────────────────
WHAT NOT TO INCLUDE
────────────────────────────────────────

Do NOT include:
- Marketing or user-facing documentation
- Explanations of basic technologies (Astro, Tailwind, ESP32, etc.)
- Alternative designs or optional approaches
- New features not present in the PRE-PLAN
- TODOs, TBDs, placeholders, or speculative language
- Pixel-perfect UI specs, CSS micromanagement, or animation tuning

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
DOCUMENT FORMATTING & READABILITY
────────────────────────────────────────

Clear visual structure is mandatory.

**Heading hierarchy (strict):**
- H1: Document title only
- H2: Major sections
- H3: Subsections
- H4: Detailed items (e.g. specific endpoints)
- Never skip heading levels

**Presentation rules:**
- Tables MUST be used for: endpoints, file lists, constants, mappings
- Code blocks MUST be used for: all code, configs, examples (with language tags)
- Diagrams MUST be used for: architecture, flows, state machines
- Lists MUST be used for: sequences, checklists
- Paragraphs SHOULD be short (3–6 sentences)

**Visual markers:**
- ⚠️ for critical warnings
- 🔴 / 🟡 / 🟢 for HIGH / MEDIUM / LOW risk
- **Bold** for first-use key terms
- `inline code` for paths, symbols, constants
- > Blockquotes for constraints or design rationale

**Code blocks:**
- MUST include language tag (```cpp, ```js, ```json, etc.)
- MUST include file path comment when showing full file contents
- SHOULD include context note for snippets (“Add to WebUiServer::begin()”)

────────────────────────────────────────
DIAGRAM REQUIREMENT (MANDATORY)
────────────────────────────────────────

Diagrams are REQUIRED and MUST be created using **MermaidJS only**.

Escaped example:

\`\`\`mermaid
flowchart TD
  A --> B
\`\`\`

Required diagram types:
- `flowchart` — system & build flow
- `sequenceDiagram` — API interactions
- `stateDiagram-v2` — chunked static asset sender

Do NOT embed images.
Do NOT reference external diagram tools.

────────────────────────────────────────
REQUIRED DOCUMENT STRUCTURE
────────────────────────────────────────

## 1. Executive Summary
- Concise, high-level overview
- Key architectural principles
- Why the design is safe for ESP32 + OTA + LED render loop
- Highest-risk areas and mitigations

## 2. System Architecture Overview
- Firmware / Web UI / Build pipeline overview
- MermaidJS architecture & data-flow diagrams
- Ownership boundaries (core vs platform vs UI)
- Scheduling model (render loop vs HTTP handling)

## 3. Detailed Technical Design

### 3.1 HTTP Server Architecture
- `WebServer` lifecycle
- Render-loop scheduling gate (policy-level)
- Static asset serving:
  - MUST include MermaidJS `stateDiagram-v2`
  - MUST describe chunked sender as a state machine
- Dynamic routing:
  - MUST describe a **Unified Dispatcher** strategy
    (e.g. via `onNotFound`, not per-slug handlers)

### 3.2 API Design
- Endpoint-by-endpoint specification
- Request/response schemas (JSON-schema-like or TS-style)
- Example requests and responses
- Error response shapes with explicit codes
- Rate limiting behavior
- JSON emission strategy (stream vs bounded)

### 3.3 Persistence & NVS Strategy
- Key layout
- Blob versioning & migration
- Wipe-all behavior
- Safety guarantees

### 3.4 Frontend Architecture

#### 3.4.1 Visual Intent & Principles
- Visual goal: calm, professional hardware control UI
- Priorities: clarity, hierarchy, restraint
- No decorative icons, no animation-driven feedback

#### 3.4.2 UI Design System
MUST specify:
- daisyUI theme selection + rationale
- Typography usage (sans vs monospace)
- Color semantics (primary / error / disabled)
- Layout patterns (cards, tables, responsive behavior)
- Accessibility requirements (keyboard nav, semantics)

#### 3.4.3 Component Usage & Interaction Patterns
- Buttons, forms, feedback states
- Loading/error handling
- Destructive action semantics

#### 3.4.4 Parameter Type → UI Mapping
Provide a table mapping:
- param type → HTML element → daisyUI component → notes

### 3.5 Build & Asset Pipeline
- Astro build flow
- Gzip process
- Embedded asset generation
- Asset budget enforcement
- OTA partition safety checks
- Explicit partition scheme selection + justification

## 4. Comprehensive Implementation Plan

- Step-by-step implementation breakdown
- Explicit file paths to create or modify
- Function names, class names, symbols
- Code snippets included where appropriate

Rules:
- Complete contents for critical new files
- Clear insertion points for modifications
- Pseudocode allowed ONLY for chunked sender state machine

### 4.1 New Files to Create
### 4.2 Existing Files to Modify
### 4.3 Web UI Project Setup (`webui/`)
### 4.4 PlatformIO Integration

## 5. Testing & Verification Plan
- Specific firmware tests
- Specific frontend interaction tests
- Build-time checks
- Asset-budget failure scenarios
- Render-loop safety verification methodology

## 6. Risk Register & Mitigations
- Known risks
- Design mitigations
- What to watch during implementation

## 7. Acceptance Criteria
- Concrete checklist of completion conditions
- Measurements and invariants that must hold
- Rejection conditions if constraints are violated

────────────────────────────────────────
FINAL INSTRUCTION
────────────────────────────────────────

Generate the complete **Design Document + Comprehensive Implementation Plan**
and write it to:

  ./docs/plans/webui_design_doc.md

The document must be technically precise, visually clear,
and directly actionable by a coding agent.

