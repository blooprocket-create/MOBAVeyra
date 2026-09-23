#!/usr/bin/env python3
"""Check Veyra's agent routing index and build line-accurate section maps.

Run from any directory:
    python3 scripts/check_doc_context.py --check
    python3 scripts/check_doc_context.py --write

Only Docs/Design/Veyra_*_Bible_v*.md in the active Design directory are
current bible candidates. Never index historical Archives as current canon.
"""
from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DESIGN = ROOT / "Docs" / "Design"
ROUTES = ROOT / "Docs" / "CONTEXT_MAP.md"
SECTIONS = ROOT / "Docs" / "Index" / "sections"

# Separate generated maps keep the entrypoint small, while allowing an agent to
# find a section and fetch only a bounded line range from a very large bible.
MAPPED_BIBLES = (
    "Veyra_Initial_Roster_Character_Bible_v0.6.md",
    "Veyra_Combat_Bible_v0.5.md",
    "Veyra_Battleground_Bible_v0.9.md",
    "Veyra_Pre_Game_Client_UX_Bible_v0.1.md",
    "Veyra_Settings_Accessibility_Bible_v0.1.md",
    "Veyra_Client_Platform_Bible_v0.1.md",
)


def section_map(filename: str) -> str:
    source = DESIGN / filename
    lines = source.read_text(encoding="utf-8").splitlines()
    result = [
        f"# Section locator: {filename}",
        "",
        "> GENERATED from the active design bible. Do not edit by hand.",
        "> This page is a **locator**, not canon. Read the linked source section",
        "> and its relevant cross-domain owner before changing a rule.",
        "",
        f"Source: [\u0060{filename}\u0060](../../Design/{filename})",
        "",
        "Links open GitHub's line-numbered source view; in a checkout,",
        "use the listed line as the start of a bounded read. Line numbers",
        "are regenerated when the source changes.",
        "",
    ]
    for number, line in enumerate(lines, 1):
        match = re.match(r"^(#{1,3})\s+(.+?)\s*$", line)
        if not match:
            continue
        level = len(match.group(1))
        title = match.group(2).strip()
        # Link text is plain text; the original heading remains in the source.
        title = title.replace("[", r"\[").replace("]", r"\]")
        indent = "  " * (level - 1)
        result.append(
            f"{indent}- [L{number}: {title}](../../Design/{filename}?plain=1#L{number})"
        )
    result.extend(("", "Read only the source section you need; this map does not",
                   "replace the bible, approved ADRs, or the architecture rules.", ""))
    return "\n".join(result)


def check_routes() -> list[str]:
    errors = []
    if not ROUTES.is_file():
        return [f"Missing routing index: {ROUTES.relative_to(ROOT)}"]
    text = ROUTES.read_text(encoding="utf-8")
    listed = set(re.findall(
        r"\]\(Design/(Veyra_[^)/\s]+_Bible_v[^)/\s]+\.md)\)", text
    ))
    actual = {p.name for p in DESIGN.glob("Veyra_*_Bible_v*.md")}
    for filename in sorted(actual - listed):
        errors.append(f"Unindexed active bible: Docs/Design/{filename}")
    for filename in sorted(listed - actual):
        errors.append(f"Index points to missing/superseded bible: {filename}")
    for filename in MAPPED_BIBLES:
        if filename not in actual:
            errors.append(f"Mapped bible missing from current canon: {filename}")
    # Two existing accepted ADRs currently share a numeric ID. Until that is
    # resolved explicitly, route by exact filename rather than by number.
    for filename in ("ADR-003-owned-field-entities.md",
                     "ADR-003-unified-unreal-client-states.md"):
        if f"ADR/{filename}" not in text:
            errors.append(f"Missing explicit ambiguous ADR route: {filename}")
    return errors


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--check", action="store_true")
    mode.add_argument("--write", action="store_true")
    args = parser.parse_args()

    errors = check_routes()
    expected_names = {Path(name).stem + ".md" for name in MAPPED_BIBLES}
    if args.check and SECTIONS.is_dir():
        extra = {p.name for p in SECTIONS.glob("Veyra_*_Bible_v*.md")} - expected_names
        for filename in sorted(extra):
            errors.append(f"Stale generated section map: {filename}")

    for filename in MAPPED_BIBLES:
        source = DESIGN / filename
        if not source.is_file():
            continue
        destination = SECTIONS / (source.stem + ".md")
        generated = section_map(filename)
        if args.write:
            SECTIONS.mkdir(parents=True, exist_ok=True)
            destination.write_text(generated, encoding="utf-8")
        elif not destination.is_file() or destination.read_text(encoding="utf-8") != generated:
            errors.append(f"Section map out of date: {destination.relative_to(ROOT)}")

    if errors:
        for error in errors:
            print("ERROR:", error, file=sys.stderr)
        if args.check:
            print("Run python3 scripts/check_doc_context.py --write after updating routes.")
        return 1
    print(f"OK: {len(list(DESIGN.glob('Veyra_*_Bible_v*.md')))} active bibles routed; "
          f"{len(MAPPED_BIBLES)} section maps {'checked' if args.check else 'generated'}.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
