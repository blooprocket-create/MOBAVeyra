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

    def test_switches_are_set_by_a_bool_never_an_expression(self):
        # A module's switch takes only a true/false value; an expression of "true" on one crashed the generator's editor.
        for system in SHIPPED["systems"]:
            for entry in system.get("inputs", []):
                with self.subTest(system=system["name"], input=entry["input"]):
                    if "bool" in entry:
                        self.assertIsInstance(entry["bool"], bool)
                        self.assertFalse({"value", "expression", "enum"} & set(entry))
                    self.assertNotIn(entry.get("expression", "").strip().lower(), ("true", "false"))

    def test_no_system_asks_for_local_space(self):
        # Emitters stay in world space (ADR-072 §8): switched to the system's own frame they drew nothing. A beam is a sprite
        # offset in the system's frame instead, which turns with it.
        for system in SHIPPED["systems"]:
            with self.subTest(system=system["name"]):
                for holder in [system] + system.get("emitters", []):
                    self.assertNotIn("localSpace", holder)

    def test_no_ribbon_draws_a_skill_effect(self):
        # A skill's beam is a sprite aligned along it (ADR-072 §8): beam ribbons cast a shadow but drew nothing on camera.
        for system in SHIPPED["systems"]:
            if not system["purpose"].startswith("ADR-072"):
                continue
            for emitter in system.get("emitters", []):
                with self.subTest(system=system["name"], emitter=emitter["name"]):
                    self.assertNotIn("ribbonMaterial", emitter)
                    self.assertNotIn("Beam.", emitter["template"])

    def test_a_skill_effects_offsets_along_its_facing_turn_with_it(self):
        # A shape offset is in the simulation's frame unless it says otherwise; one along a skill's facing (a beam's length)
        # left in it ran the beam the wrong way (ADR-072 §8). Only a vertical or zero offset may stay there.
        for system in SHIPPED["systems"]:
            if not system["purpose"].startswith("ADR-072"):
                continue
            inputs = system.get("inputs", [])
            for entry in inputs:
                if entry["module"] != "ShapeLocation" or entry["input"] != "Offset":
                    continue
                horizontal = [part.strip() for part in entry.get("expression", "").split("(", 1)[-1].split(",")[:2]]
                if all(part in ("0", "0.0") for part in horizontal):
                    continue
                with self.subTest(system=system["name"], emitter=entry.get("emitter")):
                    self.assertTrue(any(other["module"] == "ShapeLocation" and other["input"] == "Offset Coordinate Space"
                                        and other.get("emitter") == entry.get("emitter") and other.get("enum") == "Local"
                                        for other in inputs))

if __name__ == "__main__":
    unittest.main()
