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
# Where a concept sheet named by `sheet.file` is looked up. The baked-text sheets that
# once lived here were deleted on 2026-09-21 (not archived), so the directory does not
# exist today and every entry is `file: null`. The check stays so that any status
# asserting a sheet (`current`, `contradicts_canon`, `incomplete`) fails until that
# file is actually committed here. sheet.file stores the bare filename, so only this
# constant tracks the location.
SHEETS = ROOT / "ConceptArt" / "Archives" / "Characters"


def newest(stem: str) -> pathlib.Path:
    """The highest-versioned `<stem>_vX.Y.md` in Docs/Design.

    Resolved at run time rather than hardcoded: the bibles are versioned by
    filename and superseded ones move to Archives/, so pinning a version here
    means the next bump silently breaks every check that reads it.
    """
    def version(path: pathlib.Path) -> tuple[int, int]:
        m = re.search(r"_v(\d+)\.(\d+)\.md$", path.name)
        return (int(m.group(1)), int(m.group(2))) if m else (-1, -1)

    found = sorted(DESIGN.glob(f"{stem}_v*.md"), key=version)
    return found[-1] if found else DESIGN / f"{stem}_MISSING.md"


BIBLE = newest("Veyra_Initial_Roster_Character_Bible")
COMBAT = newest("Veyra_Combat_Bible")
REGISTER = newest("Sheet_Canon_Discrepancy_Register")

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
STEALTH = {"camouflage", "invisibility"}

# Validated because the roster summary derives its sustain conclusions from
# these values: a typo would silently drop a Vanguard from the totals.
GRANTS = {"shield", "ally_shield", "ally_fluxborn_shield", "temporary_health",
          "self_heal", "ally_heal", "ally_regeneration",
          "movement_speed", "ally_movement_speed", "attack_speed", "ally_attack_speed",
          "ally_cooldown_reduction", "tenacity", "slow_resistance",
          "displacement_resistance", "ally_displacement_resistance",
          "displacement_immunity", "displacement_reduction",
          "ally_damage_reduction", "ally_damage_mitigation", "armor_penetration"}
TERRAIN = {"jungle", "river", "base"}
LAYOUT = {"standard", "stance", "stance_modal"}

# Categories established by ADR-003 (Accepted).
ENTITY_CATEGORY = {"combat_unit", "placed_marker", "world_volume", "ride_state"}

# `withdrawn` and `superseded` both mean the sheet is gone, for different reasons, and
# the difference is worth keeping: `withdrawn` is the three pulled for third-party
# content, `superseded` is the twenty-two retired once authored art replaced them and
# deleted on 2026-09-21. Collapsing them would lose why a file is absent.
SHEET_STATUS = {"current", "contradicts_canon", "incomplete", "withdrawn", "superseded", "missing"}
NO_SHEET = {"withdrawn", "superseded", "missing"}
VISION_STATUS = {"canon", "unresolved", "needs_classification"}

# --- tuning guard ------------------------------------------------------------
# ARCHITECTURE.md 1.3: no gameplay tuning outside validated designer-editable
# data. This layer is structural, so numeric scalars and tuning-shaped keys are
# rejected outright. Prose inside `guards` and `note` is exempt: it explains
# rules rather than configuring them.

TUNING_KEY = re.compile(
    r"cooldown|duration|ratio|radius|scaling|seconds|percent|amount|"
    r"\bdamage\b|\bspeed\b|\bcost\b|\bcap\b|\bthreshold\b|\bstacks?\b|\brange\b",
    re.I)
# `range_class` is structural, not tuning: \brange\b does not match it because
# `_` is a word character, so no boundary follows "range".

PROSE_KEYS = {"guards", "note", "notes"}

# A scalar that is entirely a number, optionally with a unit, is tuning however
# it is typed. Catches the string form ("1200", "10 metres", "50%") that a bare
# numeric check misses. Descriptive text is unaffected -- it never matches whole.
TUNING_STRING = re.compile(r"^[-+]?\d+(?:\.\d+)?\s*[A-Za-z%°/]*$")


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
    elif isinstance(node, str) and TUNING_STRING.match(node.strip()):
        errors.append(f"{path}: {node!r} is a quantity -- tuning belongs in engine data, not here")


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


def bible_roster() -> dict[int, tuple[str, str | None]]:
    """Parse the roster overview table from the Character Bible into (name, title).

    Rows read `| 7 | **Vera, The Last Volley** | ...`, so the cell splits on its
    first comma. A trailing companion suffix is normalised away: the table lists
    Marek as "The Black Accord + Nix" while his title is "The Black Accord".
    """
    if not BIBLE.exists():
        return {}
    out = {}
    for line in BIBLE.read_text(encoding="utf-8").splitlines():
        m = re.match(r"\|\s*(\d+)\s*\|\s*\*\*([^*|]+)\*\*", line)
        if not m:
            continue
        name, _, title = m.group(2).partition(",")
        title = re.sub(r"\s*\+\s*\w+$", "", title.strip()) or None
        out[int(m.group(1))] = (name.strip(), title)
    return out


def register_sections() -> set[str]:
    """Section ids in the discrepancy register, so `register_refs` cannot rot."""
    if not REGISTER.exists():
        return set()
    return set(re.findall(r"^#{2,3}\s+([A-G]\d*)\.", REGISTER.read_text(encoding="utf-8"), re.M))


def main() -> int:
    errors: list[str] = []
    warnings: list[str] = []

    files = sorted(HERE.glob("*.yaml"))
    require(len(files) == EXPECTED_COUNT,
            f"expected {EXPECTED_COUNT} Vanguard files, found {len(files)}", errors)

    print(f"  reading {BIBLE.name}, {COMBAT.name}, {REGISTER.name}")

    cc_vocab = combat_bible_cc()
    if cc_vocab:
        print(f"  CC vocabulary derived from Combat Bible: {len(cc_vocab)} types")
    else:
        warnings.append("could not parse the Combat Bible CC vocabulary; cc values unchecked")

    roster = bible_roster()
    if not roster:
        warnings.append("could not parse the Character Bible roster table; names unchecked")

    sections = register_sections()
    if sections:
        print(f"  discrepancy-register sections: {len(sections)}")
    else:
        warnings.append("could not parse the discrepancy register; register_refs unchecked")

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
        subset(d.get("grants"), GRANTS, "grant", where, errors)
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
        status = sheet.get("status")
        require(status in SHEET_STATUS, f"{where}: sheet.status {status!r} unknown", errors)

        # These mean there is no sheet to look at; every other status asserts one
        # exists, so it must be named AND present on disk.
        if status in NO_SHEET:
            require(sheet.get("file") is None,
                    f"{where}: sheet.status {status!r} must have file: null", errors)
        elif status in SHEET_STATUS:
            if not sheet.get("file"):
                errors.append(f"{where}: sheet.status {status!r} claims a sheet exists but "
                              f"file is empty -- use 'missing' or name the file")
            else:
                require((SHEETS / sheet["file"]).exists(),
                        f"{where}: sheet file {sheet['file']!r} not found in "
                        f"{SHEETS.relative_to(ROOT).as_posix()}/", errors)

        refs = sheet.get("register_refs") or []
        if sections:
            for ref in refs:
                require(ref in sections,
                        f"{where}: register_ref {ref!r} is not a section of the discrepancy "
                        f"register (have: {', '.join(sorted(sections))})", errors)
        # A status that asserts a PROBLEM must point at the register section documenting
        # it. `superseded` asserts no problem -- it means authored art replaced the sheet --
        # so it is exempt. Five sheets (Torr, Qazharr, Gorraveth, Aurelisse, Eudora) never
        # had a conflict to record, and requiring a ref would mean inventing one.
        if status and status not in ("current", "superseded"):
            require(bool(refs),
                    f"{where}: sheet.status {status!r} requires at least one register_ref "
                    f"so the problem is documented", errors)

        if roster and num in roster:
            exp_name, exp_title = roster[num]
            require(d["name"] == exp_name,
                    f"{where}: name {d['name']!r} does not match Character Bible roster "
                    f"entry {exp_name!r} at #{num}", errors)
            require(d.get("title") == exp_title,
                    f"{where}: title {d.get('title')!r} does not match Character Bible roster "
                    f"entry {exp_title!r} at #{num}", errors)

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

        # Counted by status rather than as one "not usable" bucket: `superseded` and
        # `withdrawn` are the expected end state of the deleted sheets, not a problem.
        by_status: dict[str, list[str]] = {}
        for d in records:
            st = (d.get("sheet") or {}).get("status") or "?"
            by_status.setdefault(st, []).append(d["id"])
        print("\nconcept sheet status (rendered sheets are built by render_sheet.py):")
        for st in sorted(by_status):
            ids = sorted(by_status[st])
            print(f"  {st:<18} {len(ids):>2}  {', '.join(ids)}")

    print()
    if errors:
        print(f"FAILED: {len(errors)} error(s), {len(warnings)} warning(s)")
        return 1
    print(f"OK: {len(records)} Vanguards valid, {len(warnings)} warning(s)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
