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

    def test_corpus_verdicts(self) -> None:
        corpus = json.loads((TEST_DATA / "TuningConformance.json").read_text(encoding="utf-8"))
        schema, errors = tuning.load_schema(TEST_DATA / "TuningConformance.schema.json", "corpus schema")
        self.assertEqual(errors, [])
        self.assertGreater(len(corpus["cases"]), 0)
        for case in corpus["cases"]:
            with self.subTest(case=case["name"]):
                problems = tuning.validate_document(case["document"], schema, "document")
                self.assertEqual(not problems, case["valid"], problems)


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
            "type 'string' is not supported": lambda s: constant(s).update(type="string"),
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


class CheckTests(unittest.TestCase):
    def setUp(self) -> None:
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.game = Path(self.tmp.name) / "Game"
        (self.game / "Tuning" / "Schemas").mkdir(parents=True)
        self.write("Tuning/Schemas/Combat.schema.json", json.dumps(VALID_SCHEMA))
        self.write("Tuning/Combat.json", VALID_DOCUMENT)

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


class RealRepositoryTests(unittest.TestCase):
    def test_committed_tuning_passes(self) -> None:
        errors, summary = tuning.check(tuning.GAME)
        self.assertEqual(errors, [], "\n".join(errors))
        self.assertIn("tuning domain", summary)


if __name__ == "__main__":
    unittest.main()
