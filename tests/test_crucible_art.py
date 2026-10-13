"""The Crucible's mass canopy (ADR-073 §5): current with its masses and profile, and standing on its mass's top."""
from __future__ import annotations

import importlib.util
import json
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "Game" / "Scripts" / "Crucible"))

import crucible_art as art  # noqa: E402
import crucible_geometry as geometry  # noqa: E402

WORLD = geometry.load_world()
STYLE = art.load_style()
ART = json.loads(art.ART_JSON.read_text(encoding="utf-8"))
MASSES = {m["id"]: m for m in WORLD["layout"]["masses"]["items"]}
HAS_SHAPELY = importlib.util.find_spec("shapely") is not None


class CrucibleMassArt(unittest.TestCase):
    def test_it_was_built_from_the_current_masses_and_profile(self):
        hint = "Run Game/Scripts/Crucible/BuildCrucibleMassArt.py after the masses or massArt change."
        self.assertEqual(ART["massesInputSha256"], WORLD["layout"]["masses"]["inputSha256"], hint)
        self.assertEqual(ART["profileSha256"], art.profile_sha256(STYLE), hint)

    @unittest.skipUnless(HAS_SHAPELY, "shapely (Game/Scripts/Crucible/requirements.txt) derives the art")
    def test_it_is_exactly_what_its_inputs_derive(self):
        self.assertEqual(ART, art.derive_art(WORLD, STYLE))

    def test_it_covers_team_as_masses_each_once(self):
        self.assertEqual(sorted(m["id"] for m in ART["masses"]), sorted(i for i in MASSES if i.startswith("a")))

    @unittest.skipUnless(HAS_SHAPELY, "shapely measures containment")
    def test_the_canopy_stands_on_its_mass_top(self):
        # Inside the gameplay edge by more than the sculpted face steps back, so nothing floats past the lip (ADR-073 §5);
        # and nothing tall stands on rock, where it would reach the gameplay camera.
        from shapely.geometry import Point, Polygon

        forms = STYLE["massForms"]["kinds"]
        for mass in ART["masses"]:
            if not mass["canopy"]:
                continue
            top = Polygon([(p["x"], p["y"]) for p in MASSES[mass["id"]]["points"]]).buffer(-art.farthest_step_back(forms[mass["kind"]]))
            for piece in mass["canopy"]:
                self.assertTrue(top.contains(Point(piece["at"])), f"{mass['id']} canopy at {piece['at']}")
                if mass["kind"] != "Forest":
                    self.assertNotEqual(piece["kind"], "Tree", f"{mass['id']} has a tree on rock")

    def test_every_kind_has_a_form_that_rises(self):
        forms = STYLE["massForms"]["kinds"]
        for kind in {m["kind"] for m in MASSES.values()}:
            self.assertIn(kind, forms, kind)
            heights = [h for h, _ in forms[kind]["profile"]]
            self.assertEqual(heights, sorted(set(heights)), f"{kind}: the profile rises")
            self.assertLess(forms[kind]["heightVariation"], 1.0, kind)


if __name__ == "__main__":
    unittest.main()
