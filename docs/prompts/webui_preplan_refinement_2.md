You are a senior systems architect and technical editor.

Your task is to APPLY A FINAL REFINEMENT PASS to the existing PRE-PLAN document at:

./docs/plans/webui_preplan.md

This is NOT a rewrite. This is a precision pass to eliminate remaining ambiguity,
tighten guardrails, and make the document fully design-doc ready.
Update the document as needed.

DO NOT introduce new features.
DO NOT change the chosen tech stack.
DO NOT relax any existing constraints.
DO NOT write implementation code.

────────────────────────────────────────
NON-NEGOTIABLE CONTEXT (REPEAT)
────────────────────────────────────────
Target:
- ESP32 Feather HUZZAH32, 4MB flash, OTA enabled

UI stack (fixed):
- Astro (static prerendered)
- Tailwind CSS (purged)
- daisyUI (SINGLE theme only)
- Preact islands (leaf widgets only)

Asset budgets (fixed):
- Soft: 200 KB gzipped total
- Hard: 300 KB gzipped total (build MUST fail)

Persistence model (fixed):
- Per-effect blob, key = "e" + 4-hex-digit EffectId (e.g. e0007)
- Blob size = 64 bytes (kMaxEffectConfigSize)
- Active effect key = aeid
- Legacy keys = bright_pct, mode
- Persistence UI is per-effect selector only (no dump-all)

Priority:
- Protect LED render loop
- Minimize server CPU usage
- Predictable RAM usage
- OTA safety

────────────────────────────────────────
REFINEMENT GOALS
────────────────────────────────────────

Your goal is to resolve the FINAL remaining ambiguities and add just enough
specificity so that implementation decisions are forced and testable.

You must address ALL items below.

────────────────────────────────────────
REQUIRED REFINEMENTS
────────────────────────────────────────

1) Render-Loop Scheduling Specificity
   - The document currently states that web server work must not interfere with rendering.
   - Add 2–3 sentences that DEFINE the gating mechanism at a policy level.
   - Example styles (choose one and document it clearly):
     - time-based deadline check (render interval minus safety margin)
     - iteration-based throttling (e.g. web handling only every N loops)
   - Do NOT write code; define the rule.

2) Static Asset Sending Guardrail
   - Explicitly state that standard “send entire buffer” helpers MUST NOT be used
     for assets larger than a small threshold.
   - Mandate chunked / yield-capable sending for static assets to prevent long
     blocking TCP writes.
   - This must be stated as a MUST / MUST NOT rule.

3) JSON Emission Rules (Tightening)
   - Clarify which endpoints MAY use bounded buffering and which MUST stream/chunk.
   - Explicitly forbid unbounded String concatenation for JSON responses.
   - Clarify behavior when MAX_JSON_RESPONSE_BYTES would be exceeded
     (reject vs paginate vs stream).

4) Alias Collision Resolution (Deterministic)
   - Replace vague “fail safe” wording with a single, deterministic policy.
   - Specify:
     - what happens at startup
     - whether boot continues
     - what is logged
     - which slugs remain valid

5) Rate Limiting (Concrete)
   - Choose ONE strategy: global (not per-client).
   - Add explicit numeric limits for:
     - brightness updates
     - parameter updates
   - Specify HTTP error behavior on exceed (e.g. 429).

6) Destructive Action Confirmation (Clarify Contract)
   - Explicitly state that BOTH a per-boot confirmation token AND a confirmation phrase are required.
   - Add a short, concrete example of the expected request body shape (still no code).

7) Float Parameter Scale Field
   - Clarify where the `scale` lives:
     - explicit field in ParamDescriptor, OR
     - derived from existing metadata.
   - Add one sentence in the Data Model and one in the Code Map to reflect this.

8) OTA Partition / Binary Size Awareness
   - Add a short subsection stating that firmware binary size + embedded UI assets
     MUST fit within the OTA app partition with safety margin.
   - You may state this as a requirement even if the exact partition size is measured later.

9) Forbidden Frontend Dependencies (Guardrail)
   - Add a short appendix or section listing categories of forbidden npm dependencies:
     - client routers
     - global state managers
     - charting libraries
     - syntax highlighting
     - date libraries
     - color picker libraries
   - State that CI/build SHOULD fail if these appear in the lockfile.

────────────────────────────────────────
OUTPUT REQUIREMENTS
────────────────────────────────────────
- Output the FULL updated PRE-PLAN markdown (not a diff, not comments).
- Preserve all existing structure unless refinement requires adding a subsection.
- Use MUST / MUST NOT / SHOULD language consistently.
- Do NOT add implementation code.
- Do NOT add speculative features.
- Ensure no “OPEN QUESTION” language remains for architecture-affecting decisions.

Before finalizing:
- Scan for any remaining ambiguous phrasing.
- Ensure every constraint is enforceable or testable.
- Ensure CPU, RAM, and OTA safety rules are explicit.

