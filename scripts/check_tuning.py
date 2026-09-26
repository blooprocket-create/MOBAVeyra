#!/usr/bin/env python3
"""Validate Veyra's gameplay tuning files against their schemas (ADR-006 §6).

Run from any directory:
    python3 scripts/check_tuning.py

Every Game/Tuning/<Domain>.json needs Game/Tuning/Schemas/<Domain>.schema.json, and the reverse.
Schemas must use the tuning dialect (Game/Tuning/README.md), a strict subset of JSON Schema
draft-04. Documents must be UTF-8 without a byte-order mark, use LF line endings, be strict JSON
(no duplicate keys, NaN or Infinity) and validate against their schema.

The game applies the same rules when it loads tuning (VeyraCore, VeyraTuning.cpp). The corpus in
Game/Source/VeyraDeveloper/TestData keeps the two validators in agreement. Needs the pinned
packages in scripts/requirements-tuning.txt.
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

# The content ID format (ADR-006 §6). The only pattern the dialect allows, for content-ID strings
# and as the one key of a map's "patternProperties". FVeyraContentId::Pattern in VeyraCore matches.
CONTENT_ID_PATTERN = "^[a-z][a-z0-9]*(_[a-z0-9]+)*$"

# References from one domain's tuning to content another domain defines, which a schema cannot
# express. Each entry is (domain, JSON pointer to a content ID, domain, JSON pointer to the map
# whose keys are the valid IDs). The loading domain in the game checks the same references.
REFERENCES: list[tuple[str, str, str, str]] = []


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


def lint_schema(schema: Any, parts: list[str], is_root: bool) -> list[str]:
    """Check that a schema stays inside the tuning dialect."""
    where = f"schema {pointer(parts)}"
    if not isinstance(schema, dict):
        return [f"{where}: must be an object"]
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
        allowed = COMMON_KEYWORDS | OBJECT_KEYWORDS
    elif kind in ("number", "integer"):
        allowed = COMMON_KEYWORDS | NUMBER_KEYWORDS
    elif kind == "string":
        allowed = COMMON_KEYWORDS | STRING_KEYWORDS
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
                                    parts + ["patternProperties", CONTENT_ID_PATTERN], False)

    if kind == "string":
        has_enum, has_pattern = "enum" in schema, "pattern" in schema
        if has_enum == has_pattern:
            errors.append(f"{where}: a string declares either \"enum\" (an enum's values) or "
                          f"\"pattern\" (a content ID), not both or neither")
        elif has_enum:
            values = schema["enum"]
            if (not isinstance(values, list) or not values
                    or not all(isinstance(v, str) for v in values) or len(set(values)) != len(values)):
                errors.append(f"{where}: \"enum\" must be a non-empty array of distinct strings")
        elif schema["pattern"] != CONTENT_ID_PATTERN:
            errors.append(f"{where}: \"pattern\" must be the content ID format {CONTENT_ID_PATTERN!r}")
        return errors

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
            errors.extend(lint_schema(child, parts + ["properties", name], False))
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


def reference_errors(documents: dict[str, Any], labels: dict[str, str]) -> list[str]:
    """Check every entry of REFERENCES whose two domains validated."""
    errors = []
    for source, source_pointer, target, target_pointer in REFERENCES:
        if source not in documents or target not in documents:
            continue
        found, value = resolve_pointer(documents[source], source_pointer)
        target_found, keys = resolve_pointer(documents[target], target_pointer)
        if not found or not isinstance(value, str):
            errors.append(f"{labels[source]} {source_pointer}: the reference table expects a content ID here")
        elif not target_found or not isinstance(keys, dict):
            errors.append(f"{labels[target]} {target_pointer}: the reference table expects a map here")
        elif value not in keys:
            errors.append(f"{labels[source]} {source_pointer}: names {value!r}, which "
                          f"{labels[target]} {target_pointer} does not define")
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
    return errors, f"{checked} tuning domain(s) valid."


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--game-dir", type=Path, default=GAME,
                        help="Unreal project folder (default: the repository's Game/).")
    args = parser.parse_args(argv)
    try:
        import jsonschema  # noqa: F401
    except ImportError:
        print("ERROR: jsonschema is not installed; run "
              "python3 -m pip install -r scripts/requirements-tuning.txt", file=sys.stderr)
        return 1

    errors, summary = check(args.game_dir.resolve())
    if errors:
        for error in errors:
            print("ERROR:", error, file=sys.stderr)
        return 1
    print(f"OK: {summary}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
