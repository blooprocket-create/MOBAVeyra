#!/usr/bin/env python3
"""Validate Veyra's gameplay tuning files against their schemas (ADR-006 §6).

Run from any directory:
    python3 scripts/check_tuning.py

Every Game/Tuning/<Domain>.json needs Game/Tuning/Schemas/<Domain>.schema.json, and the reverse.
Schemas must use the tuning dialect (Game/Tuning/README.md), a strict subset of JSON Schema
draft-04. Documents must be UTF-8 without a byte-order mark, use LF line endings, be strict JSON
(no duplicate keys, NaN or Infinity) and validate against their schema.

The game applies the same rules when it loads tuning (VeyraCore, VeyraTuning.cpp). The corpus in
Game/Source/VeyraDeveloper/TestData keeps the two validators in agreement. The contract schemas
listed in CONTRACTS use the same dialect for documents that are not tuning, and each is checked
with its example. Needs the pinned packages in scripts/requirements-tuning.txt.
"""
from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path
from typing import Any

ROOT = Path(__file__).resolve().parents[1]
GAME = ROOT / "Game"
SCHEMA_VERSION_KEY = "schemaVersion"
DOMAIN_FILE = re.compile(r"^[A-Z][A-Za-z0-9]*\.json$")
DOMAIN_SCHEMA = re.compile(r"^[A-Z][A-Za-z0-9]*\.schema\.json$")

COMMON_KEYWORDS = {"$schema", "title", "description", "type"}
OBJECT_KEYWORDS = {"properties", "required", "additionalProperties"}
MAP_KEYWORDS = {"patternProperties", "additionalProperties"}
NUMBER_KEYWORDS = {"minimum", "maximum", "exclusiveMinimum", "exclusiveMaximum", "enum"}
STRING_KEYWORDS = {"enum", "pattern"}
ARRAY_KEYWORDS = {"items", "minItems", "maxItems"}

# Shared records are declared once under the root's "definitions" and used through "$ref", which
# may stand only beside a description. The game's validator applies the same rules.
ROOT_KEYWORDS = OBJECT_KEYWORDS | {"definitions"}
REF_KEYWORDS = {"$ref", "description"}
DEFINITIONS_PREFIX = "#/definitions/"

# The content ID format (ADR-006 §6): the pattern of content-ID strings, and the one key of a map's
# "patternProperties". FVeyraContentId::Pattern in VeyraCore matches.
CONTENT_ID_PATTERN = "^[a-z][a-z0-9]*(_[a-z0-9]+)*$"

# References from one domain's tuning to content another domain defines, which a schema cannot
# express. Each entry is (domain, JSON pointer to content IDs, domain, JSON pointers to the maps whose
# keys are the valid IDs). A "*" segment in the first pointer stands for every key of an object or
# every item of an array; a final "#" segment stands for every key of a map, so a map's keys can
# name content too. An ID is valid when any of the maps defines it (ADR-008 §7). The game checks
# the same references in the loading domain, or, when that domain's layer cannot see the other, in
# a test of the committed tuning.
ABILITY_ARCHETYPE_MAPS = ("/targetedDamage", "/area", "/selfBuff", "/skillshot", "/dash", "/empoweredAttack", "/volley", "/tether", "/attach", "/ride")
PASSIVE_MAPS = ("/deepFoundation", "/hitChain", "/gatheringLight", "/breach", "/movingTarget", "/cadence", "/markProc", "/haunt", "/campReward", "/momentum", "/wildDominion", "/kitStatuses", "/attackStride", "/slipstream", "/reclaim")
REFERENCES: list[tuple[str, str, str, str | tuple[str, ...]]] = [
    ("Match", "/developerMatch/vanguards/*", "Vanguards", ("/vanguards",)),
    ("Vanguards", "/vanguards/*/abilities/q/*", "Abilities", ABILITY_ARCHETYPE_MAPS),
    ("Vanguards", "/vanguards/*/abilities/w/*", "Abilities", ABILITY_ARCHETYPE_MAPS),
    ("Vanguards", "/vanguards/*/abilities/e/*", "Abilities", ABILITY_ARCHETYPE_MAPS),
    ("Vanguards", "/vanguards/*/abilities/r/*", "Abilities", ABILITY_ARCHETYPE_MAPS),
    ("Vanguards", "/vanguards/*/passive/*", "Vanguards", PASSIVE_MAPS),
    ("Vanguards", "/hitChain/*/status", "Abilities", ("/statuses",)),
    ("Vanguards", "/breach/*/impact/statuses/*", "Abilities", ("/statuses",)),
    ("Vanguards", "/movingTarget/*/trackedStatus", "Abilities", ("/statuses",)),
    ("Vanguards", "/cadence/*/status", "Abilities", ("/statuses",)),
    ("Vanguards", "/cadence/*/extraStackOn", "Abilities", ("/statuses",)),
    ("Vanguards", "/cadence/*/steadyStatus", "Abilities", ("/statuses",)),
    ("Vanguards", "/cadence/*/fullStatus", "Abilities", ("/statuses",)),
    ("Vanguards", "/markProc/*/mark", "Abilities", ("/statuses",)),
    ("Vanguards", "/markProc/*/emergence/*/status", "Abilities", ("/statuses",)),
    ("Vanguards", "/markProc/*/procBolts/*/ability", "Abilities", ABILITY_ARCHETYPE_MAPS),
    ("Vanguards", "/haunt/*/hauntStatus", "Abilities", ("/statuses",)),
    ("Vanguards", "/haunt/*/statuses/*", "Abilities", ("/statuses",)),
    ("Vanguards", "/campReward/*/statuses/*", "Abilities", ("/statuses",)),
    ("Vanguards", "/wildDominion/*/statuses/*", "Abilities", ("/statuses",)),
    ("Vanguards", "/attackStride/*/hitStatuses/*", "Abilities", ("/statuses",)),
    ("Vanguards", "/slipstream/*/statuses/*", "Abilities", ("/statuses",)),
    ("Vanguards", "/reclaim/*/mark", "Abilities", ("/statuses",)),
    ("Vanguards", "/kitStatuses/*/statuses/*", "Abilities", ("/statuses",)),
    ("Vanguards", "/momentum/*/meter", "Abilities", ("/statuses",)),
    ("Vanguards", "/momentum/*/redlined/*/ability", "Abilities", ABILITY_ARCHETYPE_MAPS),
    ("Vanguards", "/momentum/*/holdFullStatuses/*", "Abilities", ("/statuses",)),
    ("Vanguards", "/momentum/*/roadhouse/reachStatus", "Abilities", ("/statuses",)),
    ("Abilities", "/area/*/casterStatuses/*", "Abilities", ("/statuses",)),
    # A variant is an ability of any archetype, and what an area spends is a status (ADR-018 §1, §6).
    ("Abilities", "/selfBuff/*/variants/*/ability", "Abilities", ABILITY_ARCHETYPE_MAPS),
    ("Abilities", "/area/*/consumesCasterStatuses/*", "Abilities", ("/statuses",)),
    # A volley's shot is a skillshot, and its bonus names the status an ally's displacement must find (ADR-018 §6).
    ("Abilities", "/volley/*/shot", "Abilities", ("/skillshot",)),
    ("Abilities", "/volley/*/bonus/*/status", "Abilities", ("/statuses",)),
    # A tether holds statuses on its target, and an attach on its host, while they last (ADR-018 §2).
    ("Abilities", "/tether/*/targetStatuses/*", "Abilities", ("/statuses",)),
    ("Abilities", "/attach/*/hostStatuses/*", "Abilities", ("/statuses",)),
    # A ride holds statuses on its rider, mounted actions of any archetype, and a skillshot vehicle (Combat Bible §56).
    ("Abilities", "/ride/*/riderStatuses/*", "Abilities", ("/statuses",)),
    ("Abilities", "/ride/*/mounted/*/ability", "Abilities", ABILITY_ARCHETYPE_MAPS),
    ("Abilities", "/ride/*/vehicle/*", "Abilities", ("/skillshot",)),
    # What a buff's end and its aura put on enemies are statuses (ADR-018 §6).
    ("Abilities", "/selfBuff/*/endPayload/*/status", "Abilities", ("/statuses",)),
    ("Abilities", "/selfBuff/*/shields/*/absorbedReward/*/statuses/*", "Abilities", ("/statuses",)),
    ("Abilities", "/selfBuff/*/aura/*/enemyStatuses/*", "Abilities", ("/statuses",)),
    # Each Flux Spell is an ordinary ability of one archetype (ADR-015 §3).
    ("Abilities", "/fluxSpells/roster/*", "Abilities", ABILITY_ARCHETYPE_MAPS),
    # Every Fluxborn Economy pays for is one World defines, and every one World defines is paid for.
    ("Economy", "/gold/fluxborn/#", "World", ("/fluxborn/units",)),
    ("Economy", "/experience/fluxborn/#", "World", ("/fluxborn/units",)),
    ("World", "/fluxborn/units/#", "Economy", ("/gold/fluxborn",)),
    # Wildlife likewise: every species is paid for and named where its camps and traits are.
    ("Economy", "/gold/wildlife/#", "World", ("/wildlife/species",)),
    ("Economy", "/experience/wildlife/#", "World", ("/wildlife/species",)),
    ("World", "/wildlife/species/#", "Economy", ("/gold/wildlife",)),
    ("World", "/wildlife/camps/*/species", "World", ("/wildlife/species",)),
    ("World", "/wildlife/species/*/traits/*", "Abilities", ("/statuses",)),
    # Recipes name items, Attunements their maps, Actives the abilities; consumables and quests are items.
    ("Items", "/items/*/components/*", "Items", ("/items",)),
    ("Items", "/items/*/attunement/*", "Items", ("/weightOfWar", "/overcharge", "/spoolUp", "/overcycle", "/perfectCut",
                                                 "/reprisalGuard", "/drag", "/convergence", "/fracture", "/endlessCleave", "/temperedByConflict",
                                                 "/residualCurrent", "/dragTheTempo", "/quietingChime",
                                                 "/markedForDoom", "/safeHarbor", "/highTide")),
    ("Items", "/items/*/active/*", "Abilities", ABILITY_ARCHETYPE_MAPS),
    ("Items", "/consumables/#", "Items", ("/items",)),
    ("Items", "/quests/#", "Items", ("/items",)),
    ("Items", "/quests/*/evolvesInto", "Items", ("/items",)),
    # Bots play Vanguards, build from the catalog and know what each ability of the kit is for.
    ("Bots", "/vanguards/#", "Vanguards", ("/vanguards",)),
    ("Bots", "/vanguards/*/build/*", "Items", ("/items",)),
    ("Bots", "/vanguards/*/abilities/#", "Abilities", ABILITY_ARCHETYPE_MAPS),
    # Bot seats take Flux Spells, and know what each is for (ADR-015 §8).
    ("Bots", "/seats/*/fluxSpells/*", "Abilities", ABILITY_ARCHETYPE_MAPS),
    ("Bots", "/fluxSpells/#", "Abilities", ABILITY_ARCHETYPE_MAPS),
]

# Documents that are not tuning but use its dialect, each as (schema, example), relative to Game/.
# The example is what the other side of the contract writes: the backend's contract test writes the
# match assignment (ADR-007 §5), and the match server validates it against the schema.
CONTRACTS: list[tuple[str, str]] = [
    ("Source/VeyraServices/Schemas/MatchAssignment.schema.json",
     "Source/VeyraDeveloper/TestData/MatchAssignment.example.json"),
    # The player settings registry (ADR-024 §2): presentation data the game loads, outside the tuning hash.
    ("Settings/Settings.schema.json", "Settings/Settings.json"),
]


class DuplicateKeyError(ValueError):
    pass


def _reject_duplicates(pairs: list[tuple[str, Any]]) -> dict[str, Any]:
    result: dict[str, Any] = {}
    for key, value in pairs:
        if key in result:
            raise DuplicateKeyError(f"duplicate key {key!r}")
        result[key] = value
    return result


def _reject_constant(name: str) -> Any:
    raise ValueError(f"{name} is not valid JSON")


def is_number(value: Any) -> bool:
    return isinstance(value, (int, float)) and not isinstance(value, bool)


def is_integer(value: Any) -> bool:
    return isinstance(value, int) and not isinstance(value, bool)


def text_errors(text: str, label: str) -> list[str]:
    """The byte-level rules, applied to decoded text."""
    errors = []
    if text.startswith("﻿"):
        errors.append(f"{label}: starts with a byte-order mark; save it as UTF-8 without one")
    if "\r" in text:
        errors.append(f"{label}: contains a carriage return; tuning files use LF line endings")
    return errors


def parse_strict(text: str, label: str) -> tuple[Any, list[str]]:
    try:
        return json.loads(text, object_pairs_hook=_reject_duplicates,
                          parse_constant=_reject_constant), []
    except (ValueError, DuplicateKeyError) as error:
        return None, [f"{label}: {error}"]


def pointer(parts: list[str]) -> str:
    escaped = [str(part).replace("~", "~0").replace("/", "~1") for part in parts]
    return "/" + "/".join(escaped) if escaped else "(root)"


def lint_schema(schema: Any, parts: list[str], is_root: bool,
                definitions: dict[str, Any] | None = None, used: set[str] | None = None) -> list[str]:
    """Check that a schema stays inside the tuning dialect. At the root, also check its
    "definitions", each of which some "$ref" must use."""
    if is_root:
        return lint_root(schema)
    where = f"schema {pointer(parts)}"
    if not isinstance(schema, dict):
        return [f"{where}: must be an object"]
    if "$ref" in schema:
        return ref_errors(schema, where, definitions or {}, used if used is not None else set())
    return lint_node(schema, parts, False, definitions or {}, used if used is not None else set())


def lint_root(schema: Any) -> list[str]:
    where = f"schema {pointer([])}"
    if not isinstance(schema, dict):
        return [f"{where}: must be an object"]
    definitions = schema.get("definitions", {})
    errors = []
    if not isinstance(definitions, dict):
        errors.append(f"schema {pointer(['definitions'])}: must be an object of schemas")
        definitions = {}
    used: set[str] = set()
    errors.extend(lint_node(schema, [], True, definitions, used))
    for name, definition in definitions.items():
        errors.extend(lint_schema(definition, ["definitions", name], False, definitions, used))
    if not errors:
        errors.extend(f"schema {pointer(['definitions', name])}: is never used by a \"$ref\""
                      for name in definitions if name not in used)
    return errors


def ref_errors(schema: dict[str, Any], where: str, definitions: dict[str, Any], used: set[str]) -> list[str]:
    """A "$ref" names a definition, possibly through a chain of references, and stands alone."""
    errors = [f"{where}: keyword {keyword!r} cannot stand beside \"$ref\"" for keyword in sorted(set(schema) - REF_KEYWORDS)]
    followed: list[str] = []
    node: Any = schema
    while isinstance(node, dict) and "$ref" in node:
        target = node["$ref"]
        name = target[len(DEFINITIONS_PREFIX):] if isinstance(target, str) and target.startswith(DEFINITIONS_PREFIX) else ""
        if not name or "/" in name:
            return errors + [f"{where}: \"$ref\" must name a definition as \"{DEFINITIONS_PREFIX}<name>\""]
        if name not in definitions:
            return errors + [f"{where}: \"$ref\" names {name!r}, which \"definitions\" does not declare"]
        if name in followed:
            return errors + [f"{where}: \"$ref\" leads round a cycle of definitions"]
        followed.append(name)
        used.add(name)
        node = definitions[name]
    return errors


def lint_node(schema: dict[str, Any], parts: list[str], is_root: bool,
              definitions: dict[str, Any], used: set[str]) -> list[str]:
    where = f"schema {pointer(parts)}"
    kind = schema.get("type")
    if not isinstance(kind, str):
        return [f"{where}: must declare \"type\" as a string"]

    if is_root and kind != "object":
        return [f"{where}: the root must be an object schema"]
    is_map = kind == "object" and "patternProperties" in schema
    if is_root and is_map:
        return [f"{where}: the root must be an object schema with \"properties\", not a map"]
    errors = []
    if is_map:
        allowed = COMMON_KEYWORDS | MAP_KEYWORDS
    elif kind == "object":
        allowed = COMMON_KEYWORDS | (ROOT_KEYWORDS if is_root else OBJECT_KEYWORDS)
    elif kind in ("number", "integer"):
        allowed = COMMON_KEYWORDS | NUMBER_KEYWORDS
    elif kind == "string":
        allowed = COMMON_KEYWORDS | STRING_KEYWORDS
    elif kind == "array":
        allowed = COMMON_KEYWORDS | ARRAY_KEYWORDS
    else:
        return [f"{where}: type {kind!r} is not supported"]
    for keyword in sorted(set(schema) - allowed):
        errors.append(f"{where}: keyword {keyword!r} is not supported here")

    if is_map:
        patterns = schema.get("patternProperties")
        if schema.get("additionalProperties") is not False:
            errors.append(f"{where}: a map must declare \"additionalProperties\": false")
        if not isinstance(patterns, dict) or list(patterns) != [CONTENT_ID_PATTERN]:
            return errors + [f"{where}: a map must declare \"patternProperties\" with one entry, "
                             f"the content ID format {CONTENT_ID_PATTERN!r}"]
        return errors + lint_schema(patterns[CONTENT_ID_PATTERN],
                                    parts + ["patternProperties", CONTENT_ID_PATTERN], False, definitions, used)

    if kind == "string":
        has_enum, has_pattern = "enum" in schema, "pattern" in schema
        if has_enum == has_pattern:
            errors.append(f"{where}: a string declares either \"enum\" (an enum's values) or "
                          f"\"pattern\" (a content ID or a text format), not both or neither")
        elif has_enum:
            values = schema["enum"]
            if (not isinstance(values, list) or not values
                    or not all(isinstance(v, str) for v in values) or len(set(values)) != len(values)):
                errors.append(f"{where}: \"enum\" must be a non-empty array of distinct strings")
        else:
            errors.extend(pattern_errors(schema["pattern"], where))
        return errors

    if kind == "array":
        items = schema.get("items")
        if not isinstance(items, dict):
            return errors + [f"{where}: an array must declare \"items\" as one schema object"]
        minimum = schema.get("minItems")
        if not is_integer(minimum) or minimum < 0:
            errors.append(f"{where}: every array must declare \"minItems\" as a non-negative integer")
        if "maxItems" in schema:
            maximum = schema["maxItems"]
            if not is_integer(maximum) or (is_integer(minimum) and maximum < minimum):
                errors.append(f"{where}: \"maxItems\" must be an integer no smaller than \"minItems\"")
        return errors + lint_schema(items, parts + ["items"], False, definitions, used)

    if kind == "object":
        properties = schema.get("properties")
        if not isinstance(properties, dict):
            return errors + [f"{where}: an object schema must declare \"properties\" as an object"]
        if schema.get("additionalProperties") is not False:
            errors.append(f"{where}: an object schema must declare \"additionalProperties\": false")
        required = schema.get("required")
        if not isinstance(required, list) or not all(isinstance(r, str) for r in required):
            errors.append(f"{where}: \"required\" must be an array listing every property")
        else:
            for name in properties:
                if name not in required:
                    errors.append(f"{where}: \"required\" must list {name!r}; no field is optional")
            for name in required:
                if name not in properties:
                    errors.append(f"{where}: \"required\" lists {name!r}, which is not a property")
        if is_root:
            version = properties.get(SCHEMA_VERSION_KEY)
            if version is None:
                errors.append(f"{where}: the root must declare \"schemaVersion\"")
            elif not (isinstance(version, dict) and version.get("type") == "integer"
                      and isinstance(version.get("enum"), list) and len(version["enum"]) == 1
                      and is_integer(version["enum"][0])):
                errors.append(f"{where}: \"schemaVersion\" must be an integer with a one-value \"enum\"")
        for name, child in properties.items():
            errors.extend(lint_schema(child, parts + ["properties", name], False, definitions, used))
        return errors

    integer = kind == "integer"
    if not is_number(schema.get("minimum")):
        errors.append(f"{where}: every number must declare \"minimum\"")
    if "maximum" in schema and not is_number(schema["maximum"]):
        errors.append(f"{where}: \"maximum\" must be a number")
    for flag in ("exclusiveMinimum", "exclusiveMaximum"):
        if flag in schema and not isinstance(schema[flag], bool):
            errors.append(f"{where}: {flag!r} must be true or false (draft-04)")
    if schema.get("exclusiveMaximum") is True and "maximum" not in schema:
        errors.append(f"{where}: \"exclusiveMaximum\" needs \"maximum\"")
    if "enum" in schema:
        values = schema["enum"]
        check = is_integer if integer else is_number
        if not isinstance(values, list) or not values or not all(check(v) for v in values):
            errors.append(f"{where}: \"enum\" must be a non-empty array of "
                          f"{'integers' if integer else 'numbers'}")
    return errors


def pattern_errors(pattern: Any, where: str) -> list[str]:
    """A string's "pattern" is anchored at both ends and compiles. The game matches it with ICU,
    so patterns keep to syntax both engines share (README)."""
    if not isinstance(pattern, str) or len(pattern) < 2 or not pattern.startswith("^") or not pattern.endswith("$"):
        return [f"{where}: \"pattern\" must be anchored: start with ^ and end with $"]
    try:
        re.compile(pattern)
    except re.error as error:
        return [f"{where}: \"pattern\" is not a valid regular expression ({error})"]
    return []


def full_match(pattern: str, text: str) -> bool:
    """Whether the whole of text matches. Python's "$" also matches before a final newline, which
    the game's validator (and the content ID format) does not allow."""
    return re.fullmatch(pattern.removeprefix("^").removesuffix("$"), text) is not None


def strict_validator_class() -> Any:
    """Draft-04 validation, with "pattern" and "patternProperties" matching whole strings."""
    from jsonschema import Draft4Validator, validators
    from jsonschema.exceptions import ValidationError

    def pattern(validator: Any, expected: str, instance: Any, schema: dict[str, Any]) -> Any:
        if validator.is_type(instance, "string") and not full_match(expected, instance):
            yield ValidationError(f"{instance!r} does not match {expected!r}")

    def pattern_properties(validator: Any, patterns: dict[str, Any], instance: Any,
                           schema: dict[str, Any]) -> Any:
        if not validator.is_type(instance, "object"):
            return
        for expected, subschema in patterns.items():
            for key, value in instance.items():
                if full_match(expected, key):
                    yield from validator.descend(value, subschema, path=key, schema_path=expected)

    def additional_properties(validator: Any, allowed: Any, instance: Any,
                              schema: dict[str, Any]) -> Any:
        if not validator.is_type(instance, "object") or allowed is not False:
            return
        declared = schema.get("properties", {})
        patterns = schema.get("patternProperties", {})
        extras = [key for key in instance
                  if key not in declared and not any(full_match(p, key) for p in patterns)]
        if extras:
            yield ValidationError(f"Additional properties are not allowed "
                                  f"({', '.join(repr(key) for key in extras)} unexpected)")

    return validators.extend(Draft4Validator, {
        "pattern": pattern,
        "patternProperties": pattern_properties,
        "additionalProperties": additional_properties,
    })


def validate_document(text: str, schema: dict[str, Any], label: str) -> list[str]:
    """Apply every document rule; the schema must already have passed lint_schema."""
    errors = text_errors(text, label)
    document, parse_errors = parse_strict(text, label)
    if parse_errors:
        return errors + parse_errors
    validator = strict_validator_class()(schema)
    for error in sorted(validator.iter_errors(document), key=lambda e: list(e.absolute_path)):
        errors.append(f"{label} {pointer([str(p) for p in error.absolute_path])}: {error.message}")
    return errors


def resolve_pointer(document: Any, where: str) -> tuple[bool, Any]:
    """The value at a JSON pointer, as (found, value)."""
    value = document
    for raw in where.split("/")[1:]:
        key = raw.replace("~1", "/").replace("~0", "~")
        if not isinstance(value, dict) or key not in value:
            return False, None
        value = value[key]
    return True, value


def expand_pointer(document: Any, pattern: str) -> list[tuple[str, Any]]:
    """
    Every (pointer, value) a pointer pattern reaches; a "*" segment matches each key or item, and a
    final "#" segment reaches each key of a map as the value.
    """
    reached: list[tuple[str, Any]] = [("", document)]
    for raw in pattern.split("/")[1:]:
        key = raw.replace("~1", "/").replace("~0", "~")
        following: list[tuple[str, Any]] = []
        for where, value in reached:
            if raw == "#" and isinstance(value, dict):
                following.extend((f"{where}/{name}", name) for name in value)
            elif raw == "*" and isinstance(value, dict):
                following.extend((f"{where}/{name}", item) for name, item in value.items())
            elif raw == "*" and isinstance(value, list):
                following.extend((f"{where}/{index}", item) for index, item in enumerate(value))
            elif isinstance(value, dict) and key in value:
                following.append((f"{where}/{raw}", value[key]))
        reached = following
    return reached


def reference_errors(documents: dict[str, Any], labels: dict[str, str]) -> list[str]:
    """Check every entry of REFERENCES whose two domains validated."""
    errors = []
    for source, source_pattern, target, target_pointers in REFERENCES:
        if source not in documents or target not in documents:
            continue
        pointers = (target_pointers,) if isinstance(target_pointers, str) else target_pointers
        maps = []
        for target_pointer in pointers:
            target_found, keys = resolve_pointer(documents[target], target_pointer)
            if not target_found or not isinstance(keys, dict):
                errors.append(f"{labels[target]} {target_pointer}: the reference table expects a map here")
            else:
                maps.append(keys)
        if len(maps) != len(pointers):
            continue
        reached = expand_pointer(documents[source], source_pattern)
        if not reached and "*" not in source_pattern and not source_pattern.endswith("/#"):
            errors.append(f"{labels[source]} {source_pattern}: the reference table expects a content ID here")
        for where, value in reached:
            if not isinstance(value, str):
                errors.append(f"{labels[source]} {where}: the reference table expects a content ID here")
            elif not any(value in keys for keys in maps):
                errors.append(f"{labels[source]} {where}: names {value!r}, which {labels[target]} "
                              f"{' or '.join(pointers)} does not define")
    return errors


def load_schema(path: Path, label: str) -> tuple[dict[str, Any] | None, list[str]]:
    from jsonschema import Draft4Validator
    from jsonschema.exceptions import SchemaError

    raw = path.read_bytes()
    try:
        text = raw.decode("utf-8")
    except UnicodeDecodeError as error:
        return None, [f"{label}: not valid UTF-8 ({error})"]
    errors = text_errors(text, label)
    schema, parse_errors = parse_strict(text, label)
    if parse_errors:
        return None, errors + parse_errors
    errors.extend(lint_schema(schema, [], True))
    try:
        Draft4Validator.check_schema(schema)
    except SchemaError as error:
        errors.append(f"{label}: not a valid draft-04 schema: {error.message}")
    return (schema if not errors else None), errors


def check(game_dir: Path) -> tuple[list[str], str]:
    tuning = game_dir / "Tuning"
    schemas_dir = tuning / "Schemas"
    base = game_dir.parent
    errors: list[str] = []

    def shown(path: Path) -> str:
        try:
            return path.relative_to(base).as_posix()
        except ValueError:
            return path.as_posix()

    documents = sorted(tuning.glob("*.json")) if tuning.is_dir() else []
    schemas = sorted(schemas_dir.glob("*.json")) if schemas_dir.is_dir() else []
    for path in documents:
        if not DOMAIN_FILE.match(path.name):
            errors.append(f"{shown(path)}: a domain file is named <Domain>.json in PascalCase")
    for path in schemas:
        if not DOMAIN_SCHEMA.match(path.name):
            errors.append(f"{shown(path)}: a schema is named <Domain>.schema.json in PascalCase")

    domains = {p.name[: -len(".json")] for p in documents}
    schema_domains = {p.name[: -len(".schema.json")] for p in schemas if p.name.endswith(".schema.json")}
    for domain in sorted(domains - schema_domains):
        errors.append(f"{shown(tuning / (domain + '.json'))}: has no schema at "
                      f"{shown(schemas_dir / (domain + '.schema.json'))}")
    for domain in sorted(schema_domains - domains):
        errors.append(f"{shown(schemas_dir / (domain + '.schema.json'))}: has no data file")

    checked = 0
    valid_documents: dict[str, Any] = {}
    labels: dict[str, str] = {}
    for domain in sorted(domains & schema_domains):
        schema_path = schemas_dir / f"{domain}.schema.json"
        document_path = tuning / f"{domain}.json"
        schema, schema_errors = load_schema(schema_path, shown(schema_path))
        errors.extend(schema_errors)
        if schema is None:
            continue
        raw = document_path.read_bytes()
        try:
            text = raw.decode("utf-8")
        except UnicodeDecodeError as error:
            errors.append(f"{shown(document_path)}: not valid UTF-8 ({error})")
            continue
        document_errors = validate_document(text, schema, shown(document_path))
        errors.extend(document_errors)
        if not document_errors:
            valid_documents[domain] = json.loads(text)
            labels[domain] = shown(document_path)
        checked += 1
    errors.extend(reference_errors(valid_documents, labels))
    provisional = sum(count_provisional(document) for document in valid_documents.values())
    return errors, f"{checked} tuning domain(s) valid; {provisional} provisional record(s) await review."


# The provenance marker (ADR-008 §7): a record whose values the implementer drafted says so, and the
# author changes it to "Reviewed" after reviewing it.
PROVENANCE_KEY = "provenance"
PROVISIONAL = "Provisional"


def provisional_records(value: Any, parts: list[str] | None = None) -> list[str]:
    """The JSON pointer of each record in a tuning document still marked provisional, in document order."""
    parts = parts or []
    found: list[str] = []
    if isinstance(value, dict):
        if value.get(PROVENANCE_KEY) == PROVISIONAL:
            found.append(pointer(parts))
        for key, child in value.items():
            found += provisional_records(child, parts + [key])
    elif isinstance(value, list):
        for index, child in enumerate(value):
            found += provisional_records(child, parts + [str(index)])
    return found


def count_provisional(value: Any) -> int:
    """How many records in a tuning document are still marked provisional."""
    return len(provisional_records(value))


def list_provisional(game_dir: Path) -> list[str]:
    """Every provisional record in the tuning files, as "<file> <pointer>", for the author's review."""
    lines: list[str] = []
    for path in sorted((game_dir / "Tuning").glob("*.json")):
        document = json.loads(path.read_bytes().decode("utf-8"))
        lines += [f"{path.name} {where}" for where in provisional_records(document)]
    return lines


def check_contracts(game_dir: Path) -> tuple[list[str], str]:
    base = game_dir.parent
    errors: list[str] = []

    def shown(path: Path) -> str:
        try:
            return path.relative_to(base).as_posix()
        except ValueError:
            return path.as_posix()

    for schema_relative, example_relative in CONTRACTS:
        schema_path = game_dir / schema_relative
        example_path = game_dir / example_relative
        if not schema_path.is_file():
            errors.append(f"{shown(schema_path)}: is missing")
            continue
        schema, schema_errors = load_schema(schema_path, shown(schema_path))
        errors.extend(schema_errors)
        if schema is None:
            continue
        if not example_path.is_file():
            errors.append(f"{shown(example_path)}: is missing")
            continue
        try:
            text = example_path.read_bytes().decode("utf-8")
        except UnicodeDecodeError as error:
            errors.append(f"{shown(example_path)}: not valid UTF-8 ({error})")
            continue
        errors.extend(validate_document(text, schema, shown(example_path)))
    return errors, f"{len(CONTRACTS)} contract schema(s) valid."


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--game-dir", type=Path, default=GAME,
                        help="Unreal project folder (default: the repository's Game/).")
    parser.add_argument("--list-provisional", action="store_true",
                        help="After a passing check, list each provisional record (ADR-008 §7).")
    args = parser.parse_args(argv)
    try:
        import jsonschema  # noqa: F401
    except ImportError:
        print("ERROR: jsonschema is not installed; run "
              "python3 -m pip install -r scripts/requirements-tuning.txt", file=sys.stderr)
        return 1

    errors, summary = check(args.game_dir.resolve())
    contract_errors, contract_summary = check_contracts(args.game_dir.resolve())
    errors += contract_errors
    if errors:
        for error in errors:
            print("ERROR:", error, file=sys.stderr)
        return 1
    print(f"OK: {summary} {contract_summary}")
    if args.list_provisional:
        for line in list_provisional(args.game_dir.resolve()):
            print(line)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
