"""Tests for the presentation effects' spec (ADR-063 §4): each system it builds, and each emitter in it, is named once."""
from __future__ import annotations

import json
import unittest
from collections import Counter
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SPEC_FILE = ROOT / "Game" / "ArtSource" / "Presentation" / "Effects.json"
SHIPPED = json.loads(SPEC_FILE.read_text(encoding="utf-8"))


def repeated(names):
    return sorted(name for name, count in Counter(names).items() if count > 1)


class EffectsSpec(unittest.TestCase):
    def test_each_system_is_named_once(self):
        # A name is the asset it builds: two specs of one name would rebuild the same package, the last one winning.
        self.assertEqual(repeated(system["name"] for system in SHIPPED["systems"]), [])

    def test_each_emitter_is_named_once_in_its_system(self):
        for system in SHIPPED["systems"]:
            with self.subTest(system=system["name"]):
                self.assertEqual(repeated(emitter["name"] for emitter in system.get("emitters", [])), [])

    def test_a_systems_own_user_floats_are_named_once_apart_from_colour_and_scale(self):
        shared = set(SHIPPED["userVariables"].values())
        for system in SHIPPED["systems"]:
            floats = system.get("userFloats", [])
            with self.subTest(system=system["name"]):
                names = [entry["name"] for entry in floats]
                self.assertEqual(repeated(names), [])
                self.assertFalse(shared & set(names), "the colour and scale are every system's already")
                for entry in floats:
                    self.assertIsInstance(entry["default"], (int, float))

    def test_every_user_value_an_expression_reads_is_one_the_system_has(self):
        # The generator adds the colour, the scale and a system's own floats; an expression reading another would fail to compile.
        import re
        shared = set(SHIPPED["userVariables"].values())
        for system in SHIPPED["systems"]:
            known = shared | {entry["name"] for entry in system.get("userFloats", [])}
            for entry in system.get("inputs", []):
                for name in re.findall(r"User\.(\w+)", entry.get("expression", "")):
                    with self.subTest(system=system["name"], input=entry["input"]):
                        self.assertIn(name, known)

    def test_local_space_is_a_flag_on_a_system_or_its_emitters(self):
        for system in SHIPPED["systems"]:
            with self.subTest(system=system["name"]):
                for holder in [system] + system.get("emitters", []):
                    self.assertIsInstance(holder.get("localSpace", False), bool)


if __name__ == "__main__":
    unittest.main()
