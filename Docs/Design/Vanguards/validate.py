#!/usr/bin/env python3
"""Validate Veyra Vanguard structural data against the design bibles.

Run from anywhere:   python3 Docs/Design/Vanguards/validate.py

Checks that the per-Vanguard YAML files agree with each other, with the roster
table in the Character Bible, and with the crowd-control vocabulary defined in
the Combat Bible -- and that no gameplay tuning has leaked into this layer.

Exit code 0 = clean, 1 = errors. Warnings do not fail the run.
"""
from __future__ import annotations

import re
import sys
import pathlib

try:
    import yaml
except ImportError:
    sys.exit("PyYAML is required:  pip install pyyaml")

HERE = pathlib.Path(__file__).resolve().parent
DESIGN = HERE.parent
ROOT = DESIGN.parent.parent
BIBLE = DESIGN / "Veyra_Initial_Roster_Character_Bible_v0.6.md"
COMBAT = DESIGN / "Veyra_Combat_Bible_v0.4.md"
SHEETS = ROOT / "ConceptArt" / "Characters"

EXPECTED_COUNT = 25

# --- controlled vocabularies -------------------------------------------------

NATURE = {"human", "echo", "elemental", "fluxborn", "ancient_machine", "possessed_object",
          "wakebound", "native_fauna", "living_landscape", "bramblekin", "drakari"}

ROLE_TAGS = {"tank", "bruiser", "skirmisher", "fighter", "duelist", "assassin", "marksman",
             "ranged_carry", "hypercarry", "utility_carry", "kiter", "artillery", "control_mage",
             "dot_mage", "poke_mage", "proc_mage", "burst_caster", "battlemage", "sorcerer",
             "warlock", "enchanter", "protector", "utility", "utility_caster", "controller",
             "warden", "catcher",
             "engineer", "siege", "deployable_specialist", "disruptor", "stealth"}

# The roster is deliberately NOT organised around mandatory role slots
# (Character Bible, "Roster principles"). Positional labels must not appear
# as role tags; jungle preference belongs in terrain_affinity.
POSITIONAL = {"jungler", "jungle", "top", "toplane", "mid", "midlane", "bot", "bottom",
              "botlane", "support", "adc", "solo_lane", "duo_lane", "roamer"}

DAMAGE = {"physical", "magic", "true", "utility"}
RANGE_CLASS = {"melee", "ranged"}
RESOURCE = {"standard", "focus", "charge", "none"}
MOBILITY = {"dash", "blink", "leap", "ride"}
STEALTH = {"camouflage", "invisibility", "unseen"}
TERRAIN = {"jungle", "river", "base"}
LAYOUT = {"standard", "stance", "stance_modal"}

# Categories established by ADR-003 (Proposed).
ENTITY_CATEGORY = {"combat_unit", "placed_marker", "world_volume", "ride_state"}

SHEET_STATUS = {"current", "contradicts_canon", "incomplete", "withdrawn", "missing"}
VISION_STATUS = {"canon", "unresolved", "needs_classification"}

# --- tuning guard ------------------------------------------------------------
# ARCHITECTURE.md 1.3: no gameplay tuning outside validated designer-editable
# data. This layer is structural, so numeric scalars and tuning-shaped keys are
# rejected outright. Prose inside `guards` and `note` is exempt: it explains
# rules rather than configuring them.

TUNING_KEY = re.compile(
    r"cooldown|duration|ratio|radius|scaling|seconds|percent|amount|"
    r"\bdamage\b|\bspeed\b|\bcost\b|\bcap\b|\bthreshold\b|\bstacks?\b", re.I)
PROSE_KEYS = {"guards", "note", "notes"}


def walk(node, path, errors):
    if isinstance(node, dict):
        for k, v in node.items():
            if TUNING_KEY.search(str(k)):
                errors.append(f"{path}.{k}: tuning-shaped key -- this layer is structural only "
                              f"(ARCHITECTURE.md 1.3)")
            walk(v, f"{path}.{k}", errors)
    elif isinstance(node, list):
        for i, v in enumerate(node):
            walk(v, f"{path}[{i}]", errors)
    elif isinstance(node, (int, float)) and not isinstance(node, bool):
        if not path.endswith("roster_number"):
            errors.append(f"{path}: numeric value {node!r} -- tuning belongs in engine data, not here")


def require(cond, msg, errors):
    if not cond:
        errors.append(msg)


def subset(values, allowed, label, where, errors):
    for v in values or []:
        if v not in allowed:
            errors.append(f"{where}: unknown {label} {v!r} (allowed: {', '.join(sorted(allowed))})")


def combat_bible_cc() -> set[str]:
    """Derive the allowed CC vocabulary from the Combat Bible so it cannot drift."""
    if not COMBAT.exists():
        return set()
    text = COMBAT.read_text(encoding="utf-8")
    m = re.search(r"### Core CC types\n(.*?)\n### ", text, re.S)
    if not m:
        return set()
    return {t.lower() for t in re.findall(r"^- \*\*(\w+)\*\*", m.group(1), re.M)}


def bible_roster() -> dict[int, str]:
    """Parse the roster overview table from the Character Bible."""
    if not BIBLE.exists():
        return {}
    out = {}
    for line in BIBLE.read_text(encoding="utf-8").splitlines():
        m = re.match(r"\|\s*(\d+)\s*\|\s*\*\*([^*|,]+)", line)
        if m:
            out[int(m.group(1))] = m.group(2).strip()
    return out


def main() -> int:
    errors: list[str] = []
    warnings: list[str] = []

    files = sorted(HERE.glob("*.yaml"))
    require(len(files) == EXPECTED_COUNT,
            f"expected {EXPECTED_COUNT} Vanguard files, found {len(files)}", errors)

    cc_vocab = combat_bible_cc()
    if cc_vocab:
        print(f"  CC vocabulary derived from Combat Bible: {len(cc_vocab)} types")
    else:
        warnings.append("could not parse the Combat Bible CC vocabulary; cc values unchecked")

    roster = bible_roster()
    if not roster:
        warnings.append("could not parse the Character Bible roster table; names unchecked")

    seen_ids, seen_numbers, records = {}, {}, []

    for f in files:
        where = f.name
        try:
            d = yaml.safe_load(f.read_text(encoding="utf-8"))
        except yaml.YAMLError as e:
            errors.append(f"{where}: YAML parse failure: {e}")
            continue
        if not isinstance(d, dict):
            errors.append(f"{where}: top level is not a mapping")
            continue
        records.append(d)

        for key in ("id", "roster_number", "name", "nature", "role_tags", "damage_profile",
                    "basic_attack", "resource", "abilities", "cc", "guards", "sheet"):
            require(key in d, f"{where}: missing required field {key!r}", errors)
        if "id" not in d or "roster_number" not in d:
            continue

        vid, num = d["id"], d["roster_number"]
        require(vid not in seen_ids, f"{where}: duplicate id {vid!r} (also {seen_ids.get(vid)})", errors)
        require(num not in seen_numbers,
                f"{where}: duplicate roster_number {num} (also {seen_numbers.get(num)})", errors)
        seen_ids[vid], seen_numbers[num] = where, where
        require(f.name == f"{num:02d}-{vid}.yaml",
                f"{where}: filename should be {num:02d}-{vid}.yaml", errors)

        # tuning guard, skipping explanatory prose
        scrubbed = {k: v for k, v in d.items() if k not in PROSE_KEYS}
        walk(scrubbed, where, errors)

        # vocabularies
        require(d.get("nature") in NATURE, f"{where}: unknown nature {d.get('nature')!r}", errors)
        subset(d.get("role_tags"), ROLE_TAGS, "role_tag", where, errors)
        for tag in d.get("role_tags") or []:
            if tag in POSITIONAL:
                errors.append(f"{where}: role_tag {tag!r} is positional. The roster enforces no role "
                              f"slots -- use terrain_affinity instead.")
        subset(d.get("damage_profile"), DAMAGE, "damage type", where, errors)
        subset(d.get("mobility"), MOBILITY, "mobility type", where, errors)
        subset(d.get("stealth"), STEALTH, "stealth type", where, errors)
        subset(d.get("terrain_affinity"), TERRAIN, "terrain", where, errors)
        require(d.get("resource") in RESOURCE, f"{where}: unknown resource {d.get('resource')!r}", errors)
        require(d.get("ability_layout", "standard") in LAYOUT,
                f"{where}: unknown ability_layout {d.get('ability_layout')!r}", errors)

        ba = d.get("basic_attack") or {}
        require(ba.get("damage_type") in DAMAGE, f"{where}: bad basic_attack.damage_type", errors)
        require(ba.get("range_class") in RANGE_CLASS, f"{where}: bad basic_attack.range_class", errors)

        if cc_vocab:
            subset(d.get("cc"), cc_vocab, "CC type (Combat Bible)", where, errors)

        for ent in d.get("owned_entities") or []:
            require(ent.get("category") in ENTITY_CATEGORY,
                    f"{where}: entity {ent.get('id')!r} has unknown category "
                    f"{ent.get('category')!r} (ADR-003)", errors)
            require(isinstance(ent.get("destructible"), bool),
                    f"{where}: entity {ent.get('id')!r} must declare destructible: true/false", errors)

        for vt in d.get("vision_touchpoints") or []:
            require(vt.get("status") in VISION_STATUS,
                    f"{where}: vision_touchpoint status {vt.get('status')!r} unknown", errors)
            if vt.get("status") != "canon":
                warnings.append(f"{where}: vision touchpoint on {vt.get('ability')!r} is "
                                f"{vt.get('status')} -- needs a Vision Bible ruling")

        sheet = d.get("sheet") or {}
        require(sheet.get("status") in SHEET_STATUS,
                f"{where}: sheet.status {sheet.get('status')!r} unknown", errors)
        if sheet.get("status") == "withdrawn":
            require(sheet.get("file") is None,
                    f"{where}: withdrawn sheets must have file: null", errors)
        elif sheet.get("file"):
            require((SHEETS / sheet["file"]).exists(),
                    f"{where}: sheet file {sheet['file']!r} not found in ConceptArt/Characters/", errors)

        if roster and num in roster:
            expected = roster[num]
            require(expected.lower().startswith(d["name"].split()[0].lower()),
                    f"{where}: name {d['name']!r} does not match Character Bible roster "
                    f"entry {expected!r} at #{num}", errors)

        require(bool(d.get("guards")), f"{where}: no guards recorded", errors)

    if seen_numbers:
        missing = set(range(1, EXPECTED_COUNT + 1)) - set(seen_numbers)
        require(not missing, f"roster numbers missing: {sorted(missing)}", errors)

    # --- report ---------------------------------------------------------------
    print()
    for w in warnings:
        print(f"  WARN   {w}")
    for e in errors:
        print(f"  ERROR  {e}")

    if records:
        print("\n--- roster summary ---")
        ents: dict[str, list[str]] = {}
        for d in records:
            for ent in d.get("owned_entities") or []:
                ents.setdefault(ent.get("category", "?"), []).append(f"{ent.get('id')} ({d['id']})")
        print("owned field entities (ADR-003 inventory):")
        for cat in sorted(ents):
            print(f"  {cat:<15} {len(ents[cat]):>2}  {', '.join(sorted(ents[cat]))}")

        no_cc = sorted(d["id"] for d in records if not d.get("cc"))
        hard = {"stun", "root", "knockup", "knockback", "pull", "fear", "taunt", "charm",
                "sleep", "suppression", "polymorph"}
        no_hard = sorted(d["id"] for d in records
                         if not (set(d.get("cc") or []) & hard))
        healers = sorted(d["id"] for d in records
                         if "ally_heal" in (d.get("grants") or []))
        shielders = sorted(d["id"] for d in records
                           if "ally_shield" in (d.get("grants") or []))
        print(f"\nno crowd control at all ({len(no_cc)}): {', '.join(no_cc)}")
        print(f"no HARD crowd control ({len(no_hard)}): {', '.join(no_hard)}")
        print(f"\nally healing ({len(healers)}): {', '.join(healers) or 'NONE'}")
        print(f"ally shielding ({len(shielders)}): {', '.join(shielders) or 'NONE'}")
        if len(healers) <= 1:
            print("  NOTE: ally healing is concentrated in a single Vanguard. Any composition "
                  "wanting sustain must pick them.")

        stacky = sorted(d["id"] for d in records if d.get("marks"))
        print(f"\napplies named marks or meters ({len(stacky)}/{len(records)}): {', '.join(stacky)}")

        sheets_bad = sorted(d["id"] for d in records
                            if (d.get("sheet") or {}).get("status") != "current")
        print(f"\nsheets not usable as-is ({len(sheets_bad)}): {', '.join(sheets_bad)}")

    print()
    if errors:
        print(f"FAILED: {len(errors)} error(s), {len(warnings)} warning(s)")
        return 1
    print(f"OK: {len(records)} Vanguards valid, {len(warnings)} warning(s)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
