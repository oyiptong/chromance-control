#!/usr/bin/env python3
"""
sync_codex_skills.py

Extracts "## Skill: <Name>" sections from AGENT_SKILLS.md into Codex-style skills:
  .codex/skills/<slug>/SKILL.md

Also creates an index:
  .codex/skills/README.md

Optionally creates:
  .codex/AGENTS.md  (only if missing, unless --overwrite-agents)

Usage:
  python3 scripts/sync_codex_skills.py
  python3 scripts/sync_codex_skills.py --source AGENT_SKILLS.md --codex-dir .codex
  python3 scripts/sync_codex_skills.py --overwrite
  python3 scripts/sync_codex_skills.py --overwrite --overwrite-agents
"""

from __future__ import annotations

import argparse
import re
from dataclasses import dataclass
from pathlib import Path
from typing import List, Tuple

PURPOSE_RE = re.compile(
    r"^###\s+Purpose\s*\n+(.*?)(?:\n---\n|\n##\s+Skill:|\n###\s+|\Z)",
    re.MULTILINE | re.DOTALL
)
SKILL_HEADER_RE = re.compile(r"^##\s+Skill:\s*(.+?)\s*$", re.MULTILINE)


def slugify(name: str) -> str:
    s = name.strip().lower()
    s = re.sub(r"[^a-z0-9]+", "-", s)
    s = re.sub(r"-{2,}", "-", s).strip("-")
    return s or "skill"


@dataclass
class SkillSection:
    name: str
    slug: str
    body: str  # markdown including "## Skill: ..." heading


def split_skills(md: str) -> Tuple[str, List[SkillSection]]:
    """
    Returns (preamble, skills), where preamble is everything before the first Skill header.
    Each skill body includes its own heading.
    """
    matches = list(SKILL_HEADER_RE.finditer(md))
    if not matches:
        return md.strip() + "\n", []

    preamble = md[: matches[0].start()].strip() + "\n"
    skills: List[SkillSection] = []

    for i, m in enumerate(matches):
        name = m.group(1).strip()
        start = m.start()
        end = matches[i + 1].start() if i + 1 < len(matches) else len(md)
        body = md[start:end].strip() + "\n"
        skills.append(SkillSection(name=name, slug=slugify(name), body=body))

    return preamble, skills


def write_text(path: Path, content: str, overwrite: bool) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    if path.exists() and not overwrite:
        return
    path.write_text(content, encoding="utf-8")


def extract_description(skill_body: str, fallback: str) -> str:
    """
    Pull a short description from the '### Purpose' section.
    Returns a single-line string suitable for YAML.
    """
    m = PURPOSE_RE.search(skill_body)
    if not m:
        return fallback
    # First non-empty line(s), collapse whitespace.
    text = m.group(1).strip()
    if not text:
        return fallback
    # Take first paragraph, trim and collapse.
    first_para = re.split(r"\n\s*\n", text)[0].strip()
    one_line = re.sub(r"\s+", " ", first_para)
    # Keep it reasonably short for YAML display.
    return one_line[:200]


def make_skill_md(skill: SkillSection, source_path: str) -> str:
    fallback_desc = f"Extracted skill: {skill.name}"
    desc = extract_description(skill.body, fallback_desc)

    return (
        f"---\n"
        f"name: {skill.name}\n"
        f"description: {desc}\n"
        f"version: 1\n"
        f"source: {source_path}\n"
        f"---\n\n"
        f"# {skill.name}\n\n"
        f"_Extracted from `{source_path}`. Canonical governance remains in the root document._\n\n"
        f"{skill.body}"
    )


def make_skills_index(skills: List[SkillSection]) -> str:
    lines = [
        "# Codex Skills Index\n",
        "These skills are extracted from the repo’s canonical `AGENT_SKILLS.md`.\n",
        "Do not edit generated skill files directly unless you also update `AGENT_SKILLS.md`.\n",
        "Re-run `scripts/sync_codex_skills.py` after changes.\n",
        "## Skills\n",
    ]
    for s in skills:
        lines.append(f"- `{s.slug}` — {s.name}\n")
    return "".join(lines)


def make_agents_md_if_missing() -> str:
    # Minimal, practical entrypoint.
    return """# AGENTS.md — Chromance / WLED Integration

This file is the entrypoint for coding agents working in this repo.

## Read-first (mandatory)
Before doing any work, read these in order:

1) `docs/architecture/wled_integration_implementation_plan.md`
2) `AGENT_SKILLS.md` (canonical governance + process rules)
3) If the task touches mapping:
   - `mapping/README_wiring.md` (if present)
   - `scripts/generate_ledmap.py` (or current generator location)

If anything conflicts with the implementation plan, do not silently reinterpret.
Log the discrepancy in `TASK_LOG.md` and propose a resolution.

## Codex Skills
Agents should load and apply the skills found in:
- `.codex/skills/*/SKILL.md`

These are extracted from `AGENT_SKILLS.md` and exist to improve discovery and reuse.
"""


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--source", default="AGENT_SKILLS.md", help="Path to AGENT_SKILLS.md")
    ap.add_argument("--codex-dir", default=".codex", help="Codex directory root (default: .codex)")
    ap.add_argument("--overwrite", action="store_true", help="Overwrite existing SKILL.md and index")
    ap.add_argument(
        "--overwrite-agents",
        action="store_true",
        help="Overwrite .codex/AGENTS.md if it exists (default: only create if missing)",
    )
    args = ap.parse_args()

    source = Path(args.source)
    if not source.exists():
        raise SystemExit(f"Source file not found: {source}")

    codex_dir = Path(args.codex_dir)
    skills_dir = codex_dir / "skills"

    md = source.read_text(encoding="utf-8")
    _, skills = split_skills(md)

    if not skills:
        print("No '## Skill:' sections found. Nothing to extract.")
        return 0

    # Write skill modules
    for s in skills:
        out_dir = skills_dir / s.slug
        out_file = out_dir / "SKILL.md"
        content = make_skill_md(s, source_path=str(source))
        write_text(out_file, content, overwrite=args.overwrite)

    # Write index
    write_text(skills_dir / "README.md", make_skills_index(skills), overwrite=args.overwrite)

    # Create AGENTS.md if missing (or overwrite if requested)
    agents_md = codex_dir / "AGENTS.md"
    if (not agents_md.exists()) or args.overwrite_agents:
        write_text(agents_md, make_agents_md_if_missing(), overwrite=True)

    print(f"Extracted {len(skills)} skills into {skills_dir}/")
    print(f"- Index: {skills_dir / 'README.md'}")
    print(f"- AGENTS: {agents_md}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

