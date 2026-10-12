"""The Crucible's layout data (ADR-073): generated masses are current, ids line up, and Team B is Team A's half turn."""
from __future__ import annotations

import importlib.util
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "Game" / "Scripts" / "Crucible"))

import crucible_geometry as geometry  # noqa: E402

WORLD = geometry.load_world()
LAYOUT = WORLD["layout"]
HAS_SHAPELY = importlib.util.find_spec("shapely") is not None


class CrucibleLayout(unittest.TestCase):
    def test_the_masses_were_built_from_the_current_inputs(self):
        # Only BuildCrucibleMasses.py writes them; an edited input without a rebuild leaves the hash behind.
        self.assertEqual(LAYOUT["masses"]["inputSha256"], geometry.input_sha256(WORLD),
                         "Run Game/Scripts/Crucible/BuildCrucibleMasses.py after editing the layout's open spaces.")

    @unittest.skipUnless(HAS_SHAPELY, "shapely (Game/Scripts/Crucible/requirements.txt) rebuilds the masses")
    def test_the_masses_are_exactly_what_their_inputs_derive(self):
        self.assertEqual(LAYOUT["masses"]["items"], geometry.derive_masses(WORLD))

    def test_team_bs_masses_are_team_as_turned_half_a_turn(self):
        items = {m["id"]: m for m in LAYOUT["masses"]["items"]}
        team_a = [m for m in items.values() if m["id"].startswith("a")]
        self.assertTrue(team_a)
        for mass in team_a:
            twin = items.get("b" + mass["id"][1:])
            self.assertIsNotNone(twin, mass["id"])
            self.assertEqual(twin["kind"], mass["kind"])
            self.assertEqual(twin["points"], [{"x": -p["x"], "y": -p["y"]} for p in mass["points"]])

    def test_each_mass_outline_runs_counter_clockwise(self):
        for mass in LAYOUT["masses"]["items"]:
            pts = [(p["x"], p["y"]) for p in mass["points"]]
            area = sum(a[0] * b[1] - b[0] * a[1] for a, b in zip(pts, pts[1:] + pts[:1]))
            self.assertGreater(area, 0, mass["id"])

    def test_ids_are_unique_and_every_camp_stands_on_its_own_clearing(self):
        for key in ("clearings", "routes"):
            ids = [item["id"] for item in LAYOUT[key]]
            self.assertEqual(len(ids), len(set(ids)), key)
        camps = WORLD["wildlife"]["camps"]
        self.assertEqual(len({c["id"] for c in camps}), len(camps))
        grounds = {c["id"]: c for c in LAYOUT["clearings"] if c["kind"] == "Camp"}
        self.assertEqual({c["id"] for c in camps}, set(grounds), "each camp's ground is the Camp clearing of its id")
        for camp in camps:
            self.assertEqual(camp["center"], grounds[camp["id"]]["center"], camp["id"])

    def test_each_spire_has_a_side(self):
        for lane in LAYOUT["lanes"]:
            self.assertEqual(len(lane["spireSides"]), len(lane["spireDistances"]), lane["lane"])


if __name__ == "__main__":
    unittest.main()
