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

    def test_no_mass_corner_is_sharper_than_a_right_angle(self):
        # Each edge box lies flush along its span: at a sharper corner it would stand out of its mass (ADR-073 §3).
        for mass in LAYOUT["masses"]["items"]:
            pts = [(p["x"], p["y"]) for p in mass["points"]]
            sharp = [pts[i] for i in range(len(pts)) if geometry.is_acute(pts[i - 1], pts[i], pts[(i + 1) % len(pts)])]
            self.assertEqual(sharp, [], mass["id"])

    @unittest.skipUnless(HAS_SHAPELY, "shapely (Game/Scripts/Crucible/requirements.txt) rounds the corners")
    def test_rounding_blunts_a_spike_but_keeps_a_neck(self):
        from shapely.geometry import Point, Polygon, box
        from shapely.ops import unary_union

        # Two blocks joined by a neck narrower than twice the radius, a thin spike off the second: the spike goes, the
        # neck stays, so no new way opens through the mass.
        spike = Polygon([(2200, 450), (2900, 500), (2200, 550)])
        solid = unary_union([box(0, 0, 1000, 1000), box(1000, 450, 1200, 550), box(1200, 0, 2200, 1000), spike])
        parts = geometry.round_corners(solid, 150)
        self.assertEqual(len(parts), 1)
        self.assertTrue(parts[0].contains(Point(1100, 500)), "the neck is kept")
        self.assertFalse(parts[0].contains(Point(2600, 500)), "the spike is cut back")
        self.assertTrue(parts[0].within(solid.buffer(0.5)), "it only removes mass")

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

    def test_top_and_bottom_spires_share_their_sides(self):
        # The half turn carries Team A's top lane onto Team B's bottom, keeping left as left along each team's walk, so
        # Team B's bottom Spires stand where Team A's top ones do only if the two lanes list the same sides.
        lanes = {lane["lane"]: lane for lane in LAYOUT["lanes"]}
        self.assertEqual(lanes["Top"]["spireSides"], lanes["Bottom"]["spireSides"])
        self.assertEqual(lanes["Top"]["spireDistances"], lanes["Bottom"]["spireDistances"])


if __name__ == "__main__":
    unittest.main()
