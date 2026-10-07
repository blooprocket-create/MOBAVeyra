"""Tests for the presentation materials' spec checker (ADR-063): the shipped spec builds, and a bad one is refused."""
from __future__ import annotations

import copy
import importlib.util
import json
import sys
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / "Game" / "Scripts" / "PresentationMaterials" / "spec.py"
SPEC_FILE = ROOT / "Game" / "ArtSource" / "Presentation" / "PresentationMaterials.json"
spec = importlib.util.spec_from_file_location("presentation_materials_spec", SCRIPT)
assert spec and spec.loader
checker = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = checker
spec.loader.exec_module(checker)

SHIPPED = json.loads(SPEC_FILE.read_text(encoding="utf-8"))


def material(kind):
    return copy.deepcopy(next(entry for entry in SHIPPED["materials"] if entry["kind"] == kind))


def with_material(entry):
    built = copy.deepcopy(SHIPPED)
    built["materials"] = [entry]
    return built


class PresentationMaterialsSpec(unittest.TestCase):
    def test_the_shipped_spec_is_valid(self):
        self.assertEqual(checker.validate(SHIPPED), [])

    def test_every_kind_is_shipped(self):
        # Each kind the checker knows is exercised by the spec, so none is checked only in theory.
        self.assertEqual({entry["kind"] for entry in SHIPPED["materials"]}, set(checker.RULES))

    def test_the_effects_sprites_and_ribbons_each_have_a_material(self):
        shapes = {entry["shape"] for entry in SHIPPED["materials"] if entry["kind"] == "particleEffect"}
        self.assertEqual(shapes, set(checker.EFFECT_SHAPES))

    def test_every_glow_is_independent_of_exposure_by_a_strength_of_its_own(self):
        # Every emissive kind carries its glow strength as data; the generator scales it by the inverse of the exposure. The
        # post-process passes draw the final picture, which no exposure scales.
        for entry in SHIPPED["materials"]:
            if entry["kind"] not in ("postProcessOutline", "postProcessInk"):
                self.assertGreater(entry["glowGain"], 0.0, entry["name"])

    def test_a_missing_value_is_refused_rather_than_defaulted(self):
        for kind in ("overlayFlash", "particleSmoke", "particleEffect"):
            entry = material(kind)
            del entry["glowGain"]
            self.assertEqual(checker.validate(with_material(entry)), [f"materials[0] ({entry['name']}): glowGain is required"])

    def test_values_out_of_range_are_refused(self):
        cases = [
            ("particleEffect", "shape", "mesh"),
            ("particleEffect", "clip", 1.0),
            ("particleEffect", "albedo", 0.0),
            ("particleEffect", "tailShade", 1.5),
            ("particleEffect", "coreWhiten", -0.1),
            ("particleEffect", "glowFalloff", 0),
            ("overlayFlash", "rimFloor", 2.0),
            ("overlayFlash", "glowGain", True),
            ("particleSmoke", "glowFloor", -1.0),
            ("postProcessInk", "stencil", 0),
            ("postProcessInk", "stencil", 2.5),
            ("postProcessInk", "darken", 1.5),
            ("postProcessInk", "depthGap", 0.0),
            ("postProcessInk", "inkTint", [0.0, 0.0, 0.0]),
            ("postProcessInk", "inkTint", [1.2, 0.8, 1.0, 1.0]),
            ("postProcessInk", "solidNeighbours", 0),
            ("postProcessInk", "solidNeighbours", 9),
        ]
        for kind, key, value in cases:
            entry = material(kind)
            entry[key] = value
            problems = checker.validate(with_material(entry))
            self.assertEqual(len(problems), 1, (key, value, problems))
            self.assertIn(f": {key} must be ", problems[0])

    def test_the_outline_needs_three_distinct_stencils(self):
        entry = material("postProcessOutline")
        entry["stencils"]["ally"] = entry["stencils"]["enemy"]
        self.assertEqual(len(checker.validate(with_material(entry))), 1)

    def test_the_ink_stencil_differs_from_every_hover_stencil(self):
        # A shared stencil would outline every character as hovered, or hide the hovered one's ink (ADR-068 §3).
        clash = copy.deepcopy(SHIPPED)
        ink = next(entry for entry in clash["materials"] if entry["kind"] == "postProcessInk")
        hover = next(entry for entry in clash["materials"] if entry["kind"] == "postProcessOutline")
        ink["stencil"] = hover["stencils"]["ally"]
        self.assertTrue(any("stencil must differ" in problem for problem in checker.validate(clash)))

    def test_names_are_unique_and_kinds_known(self):
        twice = copy.deepcopy(SHIPPED)
        twice["materials"].append(copy.deepcopy(twice["materials"][0]))
        self.assertTrue(any("name is used twice" in problem for problem in checker.validate(twice)))
        unknown = material("overlayFlash")
        unknown["kind"] = "glowSomehow"
        self.assertTrue(any("unknown kind" in problem for problem in checker.validate(with_material(unknown))))

    def test_a_spec_for_another_generator_or_outside_the_cooked_folder_is_refused(self):
        stale = copy.deepcopy(SHIPPED)
        stale["generatorVersion"] = checker.GENERATOR_VERSION - 1
        stale["destination"] = "/Game/Elsewhere"
        problems = checker.validate(stale)
        self.assertTrue(any(problem.startswith("generatorVersion") for problem in problems))
        self.assertTrue(any(problem.startswith("destination") for problem in problems))


if __name__ == "__main__":
    unittest.main()
