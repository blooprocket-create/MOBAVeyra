"""Tests for the art kits' material data (ADR-040, ADR-064 §4): the shipped kits build, and bad values are refused."""
from __future__ import annotations

import copy
import importlib.util
import json
import sys
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / "Game" / "Scripts" / "KitMaterials" / "spec.py"
ART = ROOT / "Game" / "ArtSource"
spec = importlib.util.spec_from_file_location("kit_materials_spec", SCRIPT)
assert spec and spec.loader
checker = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = checker
spec.loader.exec_module(checker)


def kit(*path):
    return json.loads(ART.joinpath(*path).read_text(encoding="utf-8"))


STRUCTURES = kit("Structures", "StructureKit.json")
FLUXBORN = kit("Fluxborn", "FluxbornKit.json")
ENVIRONMENT = kit("Environment", "CrucibleKit.json")
VANGUARDS = kit("Vanguards", "VanguardKit.json")


def with_flux(source, **values):
    """source with its first glowing surface's values replaced."""
    changed = copy.deepcopy(source)
    flux = next(material for material in changed["materials"] if material["emission"] > 0)
    flux.update(values)
    return changed


class ShippedKits(unittest.TestCase):
    def test_the_structure_and_fluxborn_surfaces_are_valid(self):
        self.assertEqual(checker.surface_problems(STRUCTURES), [])
        self.assertEqual(checker.surface_problems(FLUXBORN), [])

    def test_their_flux_glows(self):
        # Each kit's Flux is a surface the importer gives an exposure-independent glow.
        for source in (STRUCTURES, FLUXBORN):
            self.assertTrue(any(material["emission"] > 0 for material in source["materials"]))

    def test_the_environment_glyphs_are_valid(self):
        self.assertEqual(checker.glyph_problems(ENVIRONMENT["look"]), [])

    def test_the_body_material_is_valid(self):
        self.assertEqual(checker.body_problems(VANGUARDS), [])


class Refusals(unittest.TestCase):
    def test_a_surface_needs_an_emission_of_at_least_zero(self):
        for bad in (-0.5, True, "2"):
            self.assertTrue(checker.surface_problems(with_flux(STRUCTURES, emission=bad)), bad)
        missing = copy.deepcopy(FLUXBORN)
        del missing["materials"][0]["emission"]
        self.assertTrue(checker.surface_problems(missing))

    def test_a_surface_needs_its_colour_and_finish(self):
        self.assertTrue(checker.surface_problems(with_flux(STRUCTURES, color=[0.1, 0.2, 0.3])))
        self.assertTrue(checker.surface_problems(with_flux(STRUCTURES, roughness=1.5)))
        self.assertTrue(checker.surface_problems({"materials": []}))

    def test_a_surface_name_is_used_once(self):
        doubled = copy.deepcopy(STRUCTURES)
        doubled["materials"].append(copy.deepcopy(doubled["materials"][0]))
        self.assertTrue(checker.surface_problems(doubled))

    def test_glyphs_need_a_glow_and_a_breath_that_never_goes_below_none(self):
        for key, bad in (("glyphStrength", 0.0), ("glyphStrength", None), ("glyphPulseDepth", 1.5),
                         ("glyphPulseRate", -1.0), ("glyphColor", [0.2, 0.6])):
            look = dict(ENVIRONMENT["look"], **{key: bad})
            self.assertTrue(checker.glyph_problems(look), (key, bad))

    def test_the_body_material_needs_a_glow_and_a_toon_light(self):
        toon = VANGUARDS["bodyMaterial"]["toon"]
        for values in ({"glowGain": 0.0, "toon": toon}, {"glowGain": 2.0}, {"toon": toon}):
            self.assertTrue(checker.body_problems(dict(VANGUARDS, bodyMaterial=values)), values)
        without = {key: value for key, value in VANGUARDS.items() if key != "bodyMaterial"}
        self.assertTrue(checker.body_problems(without))

    def test_each_toon_value_out_of_range_is_refused_alone(self):
        cases = [
            ("lightCollection", "ToonLight"),
            ("toSun", [0.0, 0.0, 0.0]),
            ("sunColor", [1.0, 1.2, 0.9]),
            ("shadowTint", [0.5, 0.5]),
            ("bandThreshold", 1.0),
            ("bandSoftness", 0.0),
            ("rimExponent", -1.0),
            ("rimStart", 0.8),
            ("rimStrength", -0.1),
            ("brightness", 0.0),
        ]
        for key, bad in cases:
            toon = dict(VANGUARDS["bodyMaterial"]["toon"], **{key: bad})
            material = dict(VANGUARDS["bodyMaterial"], toon=toon)
            problems = checker.body_problems(dict(VANGUARDS, bodyMaterial=material))
            self.assertEqual(len(problems), 1, (key, bad, problems))


if __name__ == "__main__":
    unittest.main()
