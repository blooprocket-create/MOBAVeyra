"""The Crucible's mass art (ADR-073 §5): current with its masses and profile, and never reaching past a gameplay edge."""
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

    def test_the_first_step_is_the_gameplay_outline(self):
        # Its footprint at walking height is the gameplay edge, corner for corner (ADR-073 §5).
        for mass in ART["masses"]:
            first = [s for s in mass["strata"] if s["level"] == 0]
            self.assertEqual(len(first), 1, mass["id"])
            self.assertEqual(first[0]["points"], [[p["x"], p["y"]] for p in MASSES[mass["id"]]["points"]], mass["id"])

    @unittest.skipUnless(HAS_SHAPELY, "shapely measures containment")
    def test_nothing_reaches_past_its_gameplay_edge(self):
        from shapely.geometry import Point, Polygon

        for mass in ART["masses"]:
            edge = Polygon([(p["x"], p["y"]) for p in MASSES[mass["id"]]["points"]]).buffer(1.0)
            for stratum in mass["strata"]:
                self.assertTrue(Polygon(stratum["points"]).within(edge), f"{mass['id']} level {stratum['level']}")
            for piece in mass["canopy"]:
                self.assertTrue(edge.contains(Point(piece["at"])), f"{mass['id']} canopy at {piece['at']}")


if __name__ == "__main__":
    unittest.main()
