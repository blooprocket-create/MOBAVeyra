"""Regression tests for the tuning check, including the corpus shared with the game's loader."""
from __future__ import annotations

import contextlib
import copy
import importlib.util
import io
import json
import sys
import tempfile
import unittest
from pathlib import Path


REPO = Path(__file__).resolve().parents[1]
SCRIPT = REPO / "scripts" / "check_tuning.py"
TEST_DATA = REPO / "Game" / "Source" / "VeyraDeveloper" / "TestData"
spec = importlib.util.spec_from_file_location("check_tuning", SCRIPT)
assert spec and spec.loader
tuning = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = tuning
spec.loader.exec_module(tuning)

VALID_SCHEMA = {
    "type": "object",
    "additionalProperties": False,
    "required": ["schemaVersion", "resistance"],
    "properties": {
        "schemaVersion": {"type": "integer", "minimum": 1, "enum": [1]},
        "resistance": {
            "type": "object",
            "additionalProperties": False,
            "required": ["constant"],
            "properties": {"constant": {"type": "number", "minimum": 0, "exclusiveMinimum": True}},
        },
    },
}
VALID_DOCUMENT = '{"schemaVersion": 1, "resistance": {"constant": 100}}\n'


class ConformanceCorpusTests(unittest.TestCase):
    """The same cases run in the game as Veyra.Core.Tuning.ConformanceCorpus."""

    def check_corpus(self, name: str) -> None:
        corpus = json.loads((TEST_DATA / f"{name}.json").read_text(encoding="utf-8"))
        schema, errors = tuning.load_schema(TEST_DATA / f"{name}.schema.json", "corpus schema")
        self.assertEqual(errors, [])
        self.assertGreater(len(corpus["cases"]), 0)
        for case in corpus["cases"]:
            with self.subTest(corpus=name, case=case["name"]):
                problems = tuning.validate_document(case["document"], schema, "document")
                self.assertEqual(not problems, case["valid"], problems)

    def test_corpus_verdicts(self) -> None:
        self.check_corpus("TuningConformance")

    def test_content_corpus_verdicts(self) -> None:
        self.check_corpus("TuningConformanceContent")

    def test_collections_corpus_verdicts(self) -> None:
        self.check_corpus("TuningConformanceCollections")

    def test_refs_corpus_verdicts(self) -> None:
        self.check_corpus("TuningConformanceRefs")


class SchemaLintTests(unittest.TestCase):
    def lint(self, schema: dict) -> list[str]:
        return tuning.lint_schema(schema, [], True)

    def assert_lint(self, schema: dict, fragment: str) -> None:
        errors = self.lint(schema)
        self.assertTrue(any(fragment in e for e in errors), f"expected {fragment!r} in {errors}")

    def test_valid_schema_passes(self) -> None:
        self.assertEqual(self.lint(VALID_SCHEMA), [])

    def test_rejections(self) -> None:
        def mutated(change) -> dict:
            schema = copy.deepcopy(VALID_SCHEMA)
            change(schema)
            return schema

        constant = lambda s: s["properties"]["resistance"]["properties"]["constant"]
        cases = {
            "keyword 'pattern' is not supported": lambda s: constant(s).update(pattern="x"),
            "keyword 'properties' is not supported": lambda s: constant(s).update(properties={}),
            "every number must declare \"minimum\"": lambda s: constant(s).pop("minimum"),
            "must be true or false": lambda s: constant(s).update(exclusiveMinimum=0),
            "\"exclusiveMaximum\" needs \"maximum\"": lambda s: constant(s).update(exclusiveMaximum=True),
            "no field is optional": lambda s: s["required"].remove("resistance"),
            "which is not a property": lambda s: s["required"].append("ghost"),
            "\"additionalProperties\": false": lambda s: s.update(additionalProperties=True),
            "type 'boolean' is not supported": lambda s: constant(s).update(type="boolean"),
            "the root must declare \"schemaVersion\"": lambda s: (
                s["properties"].pop("schemaVersion"), s["required"].remove("schemaVersion")),
            "one-value \"enum\"": lambda s: s["properties"]["schemaVersion"].update(enum=[1, 2]),
            "\"enum\" must be a non-empty array of integers":
                lambda s: s["properties"]["schemaVersion"].update(enum=[1.5]),
        }
        for fragment, change in cases.items():
            with self.subTest(fragment=fragment):
                self.assert_lint(mutated(change), fragment)

    def test_root_must_be_an_object(self) -> None:
        self.assert_lint({"type": "number", "minimum": 0}, "the root must be an object schema")

    def with_field(self, field: dict) -> dict:
        schema = copy.deepcopy(VALID_SCHEMA)
        schema["properties"]["field"] = field
        schema["required"].append("field")
        return schema

    def test_string_and_map_forms_pass(self) -> None:
        pattern = tuning.CONTENT_ID_PATTERN
        for field in (
            {"type": "string", "enum": ["Physical", "Magic"]},
            {"type": "string", "pattern": pattern},
            {"type": "string", "pattern": "^[A-Za-z0-9 ]{1,16}$"},
            {"type": "object", "additionalProperties": False,
             "patternProperties": {pattern: {"type": "number", "minimum": 0}}},
            {"type": "array", "items": {"type": "integer", "minimum": 0}, "minItems": 0},
            {"type": "array", "items": {"type": "string", "pattern": pattern}, "minItems": 1, "maxItems": 3},
            {"type": "array", "minItems": 1, "items": {
                "type": "object", "additionalProperties": False, "required": ["x"],
                "properties": {"x": {"type": "number", "minimum": 0}}}},
        ):
            with self.subTest(field=field):
                self.assertEqual(self.lint(self.with_field(field)), [])

    def test_string_and_map_rejections(self) -> None:
        pattern = tuning.CONTENT_ID_PATTERN
        number = {"type": "number", "minimum": 0}
        cases = {
            "either \"enum\"": {"type": "string"},
            "not both or neither": {"type": "string", "enum": ["A"], "pattern": pattern},
            "non-empty array of distinct strings": {"type": "string", "enum": ["A", "A"]},
            "\"pattern\" must be anchored": {"type": "string", "pattern": "[a-z]+"},
            "not a valid regular expression": {"type": "string", "pattern": "^([a-z]$"},
            "keyword 'minimum' is not supported": {"type": "string", "enum": ["A"], "minimum": 0},
            "an array must declare \"items\"": {"type": "array", "minItems": 0},
            "every array must declare \"minItems\"": {"type": "array", "items": number},
            "as a non-negative integer": {"type": "array", "items": number, "minItems": -1},
            "no smaller than \"minItems\"": {"type": "array", "items": number, "minItems": 2, "maxItems": 1},
            "keyword 'uniqueItems' is not supported": {"type": "array", "items": number, "minItems": 0, "uniqueItems": True},
            "every number must declare \"minimum\"": {"type": "array", "items": {"type": "number"}, "minItems": 0},
            "one entry, the content ID format": {"type": "object", "additionalProperties": False,
                                                 "patternProperties": {"^.*$": number}},
            "a map must declare \"additionalProperties\": false": {
                "type": "object", "patternProperties": {pattern: number}},
            "keyword 'properties' is not supported": {
                "type": "object", "additionalProperties": False, "properties": {},
                "patternProperties": {pattern: number}},
            "must declare \"minimum\"": {
                "type": "object", "additionalProperties": False,
                "patternProperties": {pattern: {"type": "number"}}},
        }
        for fragment, field in cases.items():
            with self.subTest(fragment=fragment):
                self.assert_lint(self.with_field(field), fragment)


class ReferenceLintTests(unittest.TestCase):
    """The same rules as Veyra.Core.TuningRefs in the game."""

    RECORD = {"type": "object", "additionalProperties": False, "required": ["ratio"],
              "properties": {"ratio": {"$ref": "#/definitions/fraction"}}}
    FRACTION = {"type": "number", "minimum": 0, "maximum": 1}

    def schema(self, nested: dict, definitions: dict) -> dict:
        schema = copy.deepcopy(VALID_SCHEMA)
        schema["definitions"] = definitions
        schema["properties"]["nested"] = nested
        schema["required"].append("nested")
        return schema

    def usual(self, **extra: dict) -> dict:
        return {"record": copy.deepcopy(self.RECORD), "fraction": dict(self.FRACTION), **extra}

    def assert_lint(self, schema: dict, fragment: str) -> None:
        errors = tuning.lint_schema(schema, [], True)
        self.assertTrue(any(fragment in e for e in errors), f"expected {fragment!r} in {errors}")

    def test_references_pass(self) -> None:
        for nested in ({"$ref": "#/definitions/record"},
                       {"description": "The shared record.", "$ref": "#/definitions/record"}):
            with self.subTest(nested=nested):
                self.assertEqual(tuning.lint_schema(self.schema(nested, self.usual()), [], True), [])

    def test_rejections(self) -> None:
        cases = {
            "\"definitions\" does not declare": (
                {"$ref": "#/definitions/ghost"}, self.usual()),
            "must name a definition": (
                {"$ref": "#/properties/resistance"}, self.usual()),
            "cannot stand beside \"$ref\"": (
                {"$ref": "#/definitions/record", "type": "object"}, self.usual()),
            "cycle of definitions": (
                {"$ref": "#/definitions/first"},
                self.usual(first={"$ref": "#/definitions/second"}, second={"$ref": "#/definitions/first"})),
            "is never used": (
                {"$ref": "#/definitions/record"}, self.usual(spare={"type": "number", "minimum": 0})),
            "keyword 'definitions' is not supported": (
                {**copy.deepcopy(self.RECORD), "definitions": {}}, {"fraction": dict(self.FRACTION)}),
        }
        for fragment, (nested, definitions) in cases.items():
            with self.subTest(fragment=fragment):
                self.assert_lint(self.schema(nested, definitions), fragment)


class CheckTests(unittest.TestCase):
    def setUp(self) -> None:
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.game = Path(self.tmp.name) / "Game"
        (self.game / "Tuning" / "Schemas").mkdir(parents=True)
        self.write("Tuning/Schemas/Combat.schema.json", json.dumps(VALID_SCHEMA))
        self.write("Tuning/Combat.json", VALID_DOCUMENT)
        # This tree has no contract schemas; ContractTests covers them.
        original = tuning.CONTRACTS
        tuning.CONTRACTS = []
        self.addCleanup(setattr, tuning, "CONTRACTS", original)

    def write(self, relative: str, text: str) -> None:
        (self.game / relative).write_bytes(text.encode("utf-8"))

    def errors(self) -> list[str]:
        return tuning.check(self.game)[0]

    def assert_error(self, fragment: str) -> None:
        errors = self.errors()
        self.assertTrue(any(fragment in e for e in errors), f"expected {fragment!r} in {errors}")

    def test_valid_tree_passes(self) -> None:
        self.assertEqual(self.errors(), [])

    def test_document_without_schema(self) -> None:
        self.write("Tuning/Economy.json", VALID_DOCUMENT)
        self.assert_error("Game/Tuning/Economy.json: has no schema")

    def test_schema_without_document(self) -> None:
        self.write("Tuning/Schemas/Economy.schema.json", json.dumps(VALID_SCHEMA))
        self.assert_error("Economy.schema.json: has no data file")

    def test_file_names_are_pascal_case(self) -> None:
        self.write("Tuning/combat_extra.json", VALID_DOCUMENT)
        self.assert_error("a domain file is named <Domain>.json in PascalCase")

    def test_crlf_document_rejected(self) -> None:
        self.write("Tuning/Combat.json", VALID_DOCUMENT.replace("\n", "\r\n"))
        self.assert_error("contains a carriage return")

    def test_byte_order_mark_rejected(self) -> None:
        self.write("Tuning/Combat.json", "﻿" + VALID_DOCUMENT)
        self.assert_error("starts with a byte-order mark")

    def test_invalid_value_reported_with_pointer(self) -> None:
        self.write("Tuning/Combat.json", '{"schemaVersion": 1, "resistance": {"constant": 0}}\n')
        self.assert_error("Game/Tuning/Combat.json /resistance/constant:")

    def test_invalid_schema_stops_that_domain(self) -> None:
        schema = copy.deepcopy(VALID_SCHEMA)
        schema["additionalProperties"] = True
        self.write("Tuning/Schemas/Combat.schema.json", json.dumps(schema))
        self.assert_error("\"additionalProperties\": false")

    def test_main_exit_codes(self) -> None:
        output = io.StringIO()
        with contextlib.redirect_stdout(output), contextlib.redirect_stderr(output):
            self.assertEqual(tuning.main(["--game-dir", str(self.game)]), 0)
            self.write("Tuning/Combat.json", "{}\n")
            self.assertEqual(tuning.main(["--game-dir", str(self.game)]), 1)
        self.assertIn("ERROR:", output.getvalue())


class ReferenceTests(unittest.TestCase):
    """Cross-domain references from the table in check_tuning.py."""

    def setUp(self) -> None:
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.game = Path(self.tmp.name) / "Game"
        (self.game / "Tuning" / "Schemas").mkdir(parents=True)
        pattern = tuning.CONTENT_ID_PATTERN
        catalogue = {"type": "object", "additionalProperties": False, "required": ["schemaVersion", "things"],
                     "properties": {"schemaVersion": {"type": "integer", "minimum": 1, "enum": [1]},
                                    "things": {"type": "object", "additionalProperties": False,
                                               "patternProperties": {pattern: {"type": "number", "minimum": 0}}}}}
        user = {"type": "object", "additionalProperties": False, "required": ["schemaVersion", "chosen"],
                "properties": {"schemaVersion": {"type": "integer", "minimum": 1, "enum": [1]},
                               "chosen": {"type": "string", "pattern": pattern}}}
        self.write("Tuning/Schemas/Catalogue.schema.json", json.dumps(catalogue))
        self.write("Tuning/Schemas/User.schema.json", json.dumps(user))
        self.write("Tuning/Catalogue.json", '{"schemaVersion": 1, "things": {"test_bolt": 1}}\n')
        original = tuning.REFERENCES
        tuning.REFERENCES = [("User", "/chosen", "Catalogue", "/things")]
        self.addCleanup(setattr, tuning, "REFERENCES", original)

    def write(self, relative: str, text: str) -> None:
        (self.game / relative).write_bytes(text.encode("utf-8"))

    def test_defined_reference_passes(self) -> None:
        self.write("Tuning/User.json", '{"schemaVersion": 1, "chosen": "test_bolt"}\n')
        self.assertEqual(tuning.check(self.game)[0], [])

    def test_undefined_reference_fails(self) -> None:
        self.write("Tuning/User.json", '{"schemaVersion": 1, "chosen": "other_bolt"}\n')
        errors = tuning.check(self.game)[0]
        self.assertTrue(any("names 'other_bolt'" in e for e in errors), errors)


class ContractTests(unittest.TestCase):
    """Contract schemas from the table in check_tuning.py, each checked with its example."""

    def setUp(self) -> None:
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.game = Path(self.tmp.name) / "Game"
        (self.game / "Schemas").mkdir(parents=True)
        self.write("Schemas/Line.schema.json", json.dumps(VALID_SCHEMA))
        original = tuning.CONTRACTS
        tuning.CONTRACTS = [("Schemas/Line.schema.json", "Line.example.json")]
        self.addCleanup(setattr, tuning, "CONTRACTS", original)

    def write(self, relative: str, text: str) -> None:
        (self.game / relative).write_bytes(text.encode("utf-8"))

    def errors(self) -> list[str]:
        return tuning.check_contracts(self.game)[0]

    def test_valid_example_passes(self) -> None:
        self.write("Line.example.json", VALID_DOCUMENT)
        self.assertEqual(self.errors(), [])

    def test_invalid_example_fails(self) -> None:
        self.write("Line.example.json", '{"schemaVersion": 2}\n')
        self.assertTrue(any("Game/Line.example.json" in e for e in self.errors()), self.errors())

    def test_missing_example_fails(self) -> None:
        self.assertTrue(any("Line.example.json: is missing" in e for e in self.errors()), self.errors())

    def test_missing_schema_fails(self) -> None:
        (self.game / "Schemas" / "Line.schema.json").unlink()
        self.assertTrue(any("Line.schema.json: is missing" in e for e in self.errors()), self.errors())


class ProvenanceTests(unittest.TestCase):
    """The provisional-values marker (ADR-008 §7)."""

    def test_counts_provisional_records_at_any_depth(self) -> None:
        document = {
            "schemaVersion": 1,
            "experience": {"provenance": "Provisional", "toNextLevel": [1]},
            "things": {"a": {"provenance": "Reviewed"}, "b": {"provenance": "Provisional"}},
            "list": [{"provenance": "Provisional"}, {"provenance": "Canon"}],
        }
        self.assertEqual(tuning.count_provisional(document), 3)

    def test_a_document_without_markers_counts_none(self) -> None:
        self.assertEqual(tuning.count_provisional({"schemaVersion": 1, "value": 2}), 0)


class RealRepositoryTests(unittest.TestCase):
    def test_committed_tuning_passes(self) -> None:
        errors, summary = tuning.check(tuning.GAME)
        self.assertEqual(errors, [], "\n".join(errors))
        self.assertIn("tuning domain", summary)

    def test_committed_contracts_pass(self) -> None:
        errors, summary = tuning.check_contracts(tuning.GAME)
        self.assertEqual(errors, [], "\n".join(errors))
        self.assertIn("contract schema", summary)


if __name__ == "__main__":
    unittest.main()
