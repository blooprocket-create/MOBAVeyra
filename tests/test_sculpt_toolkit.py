"""Tests for the sculpt toolkit's garments and regions (ADR-069 §2a): layered cloth with no air beneath it, regions
that keep a garment to its limbs, folds that stay finite to their ends, rays laid onto a surface, and cloth sheets
torn at the hem. numpy only."""
from __future__ import annotations

import sys
import unittest
from pathlib import Path

import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "Game" / "Scripts"))
from VanguardBodies.sculpt import garments, sdf, sheet, tree  # noqa: E402
from VanguardBodies.sculpt.tree import Box  # noqa: E402
from VanguardBodies.models import silt  # noqa: E402

# Fixture sizes (cm): a ball standing in for a body part, the cloth over it, a fold's height above it.
RADIUS = 10.0
CLOTH = 0.8
RIDGE = 2.5
# How far to look along a ray, and how finely.
SPAN = 20.0
STEP = 0.05
# Any flat colour (sRGB): the tests read shapes, not colours.
GREY = (0.5, 0.5, 0.5)


def ball(S, name, centre, radius):
    c = np.asarray(centre, dtype=np.float32)
    return tree.leaf(S, name, lambda P: sdf.sphere(P, c, radius), Box(c - radius, c + radius), S.material(name, GREY), None)


def along(node, direction, start=(0.0, 0.0, 0.0)):
    """node's distances along a ray from start (each sample STEP apart) and the sample spacing."""
    d = np.asarray(direction, dtype=np.float32)
    d /= np.linalg.norm(d)
    ts = np.arange(0.0, SPAN, STEP, dtype=np.float32)
    P = np.asarray(start, dtype=np.float32)[None, :] + ts[:, None] * d[None, :]
    tree.REACH[0] = 1.0
    return ts, tree.evaluate(node, P, Box.around(P, 1.0)).d


class SolidLayers(unittest.TestCase):
    def setUp(self):
        self.S = tree.Sculpt()
        self.body = ball(self.S, "body", (0, 0, 0), RADIUS)
        self.everywhere = tree.Zone(lambda P: np.full(len(P), -1.0, dtype=np.float32), Box((-50, -50, -50), (50, 50, 50)))

    def test_a_garment_leaves_no_air_between_it_and_what_it_covers(self):
        coat = tree.Shell(self.S, "coat", self.body, 0.4, CLOTH - 0.4, self.everywhere, self.S.material("coat", GREY))
        ts, d = along(tree.Over([self.body, coat]), (1, 0.3, 0.2))
        inside = d < 0
        # Solid from the centre out to the cloth's face, then outside: one surface, no gap at the body's.
        self.assertTrue(inside[0])
        edge = np.argmin(inside)
        self.assertFalse(inside[edge:].any(), "a gap of air under the cloth")
        self.assertAlmostEqual(float(ts[edge]), RADIUS + CLOTH, delta=STEP * 2)

    def test_a_fold_raises_the_outer_face_and_stays_solid_beneath(self):
        lifted = tree.Shell(self.S, "fold", self.body, 0.0, CLOTH, self.everywhere, self.S.material("fold", GREY),
                            displace=lambda P: np.full(len(P), RIDGE, dtype=np.float32), reach=RIDGE)
        ts, d = along(tree.Over([self.body, lifted]), (0, 0, 1))
        inside = d < 0
        edge = np.argmin(inside)
        self.assertFalse(inside[edge:].any(), "a fold standing off the body with air beneath it")
        self.assertAlmostEqual(float(ts[edge]), RADIUS + CLOTH + RIDGE, delta=STEP * 2)

    def test_a_layer_over_a_layer_stands_on_its_face(self):
        shirt = tree.Shell(self.S, "shirt", self.body, 0.0, CLOTH, self.everywhere, self.S.material("shirt", GREY))
        coat = tree.Shell(self.S, "coat", shirt, 0.0, CLOTH, self.everywhere, self.S.material("coat", GREY))
        _ts, d = along(tree.Over([self.body, shirt, coat]), (0, 0, 1))
        self.assertAlmostEqual(float(np.argmin(d < 0)) * STEP, RADIUS + 2 * CLOTH, delta=STEP * 2)


class KeepingToLimbs(unittest.TestCase):
    def setUp(self):
        self.S = tree.Sculpt()
        # A broad torso and a thin arm hanging close beside it, as an arm hangs in an A-pose.
        self.torso = ball(self.S, "torso", (0, 0, 0), RADIUS)
        self.arm = ball(self.S, "arm", (0, RADIUS + 4.0, 0), 3.0)
        self.limbs = {"torso": self.torso, "arm": self.arm}

    def distance(self, region, point):
        P = np.asarray([point], dtype=np.float32)
        return float(tree.evaluate(region, P, Box.around(P, 1.0)).d[0])

    def test_each_surface_belongs_to_its_own_limb(self):
        torso_only = garments.keep_to(self.limbs, ["torso"])
        arm_only = garments.keep_to(self.limbs, ["arm"])
        on_torso = (RADIUS, 0.0, 0.0)
        on_arm = (0.0, RADIUS + 7.0, 0.0)
        self.assertLess(self.distance(torso_only, on_torso), 0.0)
        self.assertGreater(self.distance(arm_only, on_torso), 0.0)
        self.assertLess(self.distance(arm_only, on_arm), 0.0)
        self.assertGreater(self.distance(torso_only, on_arm), 0.0)

    def test_regions_of_limbs_combine(self):
        band = garments.band_z(-2.0, 2.0)
        sleeve = garments.both(band, garments.keep_to(self.limbs, ["arm"]))
        coat = garments.without(garments.either(garments.keep_to(self.limbs, ["torso"]), sleeve), garments.keep_to(self.limbs, ["arm"]))
        self.assertLess(self.distance(sleeve, (0.0, RADIUS + 7.0, 0.0)), 0.0)
        self.assertGreater(self.distance(sleeve, (0.0, RADIUS + 7.0, 5.0)), 0.0)
        self.assertGreater(self.distance(coat, (0.0, RADIUS + 7.0, 0.0)), 0.0)
        self.assertLess(self.distance(coat, (RADIUS, 0.0, 0.0)), 0.0)


class Folds(unittest.TestCase):
    def test_a_curve_passes_through_its_points(self):
        points = np.array([[0, 0, 0], [5, 2, 1], [9, 6, 0], [12, 12, 3]], dtype=np.float64)
        per_span = 5
        line = garments.curve(points, (len(points) - 1) * per_span + 1)
        for i, p in enumerate(points):
            np.testing.assert_allclose(line[i * per_span], p, atol=1e-9)

    def test_nearest_on_a_line_measures_along_it(self):
        line = np.array([[0, 0, 0], [10, 0, 0], [10, 10, 0]], dtype=np.float32)
        P = np.array([[0, 3, 0], [10, 0, 4], [10, 10, 0], [12, 5, 0]], dtype=np.float32)
        dist, t, nearest = garments.nearest_on(P, line)
        np.testing.assert_allclose(dist, [3.0, 4.0, 0.0, 2.0], atol=1e-5)
        np.testing.assert_allclose(t, [0.0, 0.5, 1.0, 0.75], atol=1e-5)
        np.testing.assert_allclose(nearest, [[0, 0, 0], [10, 0, 0], [10, 10, 0], [10, 5, 0]], atol=1e-5)

    def test_a_fold_is_finite_to_its_ends(self):
        S = tree.Sculpt()
        body = ball(S, "body", (0, 0, 0), RADIUS)
        points = [(RADIUS, -6.0, 0.0), (RADIUS, 0.0, 2.0), (RADIUS, 6.0, 0.0)]
        piece = garments.fold(S, "fold", body, garments.laid_on(body, points, tilt=0.0), 3.0, RIDGE, S.material("cloth", GREY))
        # Around both ends, where the curve's parameter reaches 0 and 1 (single precision overshoots pi there).
        grid = np.stack(np.meshgrid(np.linspace(-14, 14, 15), np.linspace(-14, 14, 15), np.linspace(-6, 6, 7), indexing="ij"), axis=-1).reshape(-1, 3)
        P = (grid + np.array([RADIUS, 0.0, 0.0])).astype(np.float32)
        tree.REACH[0] = 1.0
        d = tree.evaluate(tree.Over([body, piece]), P, Box.around(P, 1.0)).d
        self.assertTrue(np.all(np.isfinite(d)))
        # It stands high at its middle, and its crest hangs below its line: a short steep side under it.
        _ts, mid = along(tree.Over([body, piece]), (1, 0, 0.2))
        self.assertGreater(float(np.argmin(mid < 0)) * STEP, RADIUS + RIDGE * 0.5)
        line_z = 2.0
        _ts, above = along(tree.Over([body, piece]), (1, 0, 0), start=(0.0, 0.0, line_z + 1.6))
        _ts, below = along(tree.Over([body, piece]), (1, 0, 0), start=(0.0, 0.0, line_z - 1.6))
        self.assertLess(float(np.argmin(above < 0)), float(np.argmin(below < 0)))


class Rays(unittest.TestCase):
    def test_many_rays_meet_the_surface_as_one_ray_does(self):
        S = tree.Sculpt()
        body = ball(S, "body", (1.0, -2.0, 0.5), RADIUS)
        starts = np.array([[30, 0, 0], [0, 30, 4], [-25, -10, 20], [5, 5, -30]], dtype=np.float64)
        directions = -starts
        points, normals = garments.surface_points(body, starts, directions, reach=45.0)
        for start, direction, point, normal in zip(starts, directions, points, normals):
            one, one_normal = garments.surface_point(body, start, direction, reach=45.0)
            np.testing.assert_allclose(point, one, atol=1e-3)
            np.testing.assert_allclose(normal, one_normal, atol=1e-3)
            self.assertAlmostEqual(float(np.linalg.norm(point - np.array([1.0, -2.0, 0.5]))), RADIUS, delta=1e-2)


class TornHems(unittest.TestCase):
    # Fixture values: a hem torn into strips, kept to at least cut of its length, each pointed by point.
    STRIPS, CUT, POINT = 6, 0.6, 0.15

    def test_each_strip_ends_at_its_own_length_pointed_at_its_middle(self):
        u = np.linspace(0.0, 1.0, self.STRIPS * 40, endpoint=False)
        kept = sheet.torn(u, self.STRIPS, self.CUT, self.POINT, 3.0)
        self.assertTrue(np.all((kept >= self.CUT - self.POINT) & (kept <= 1.0)))
        strips = kept.reshape(self.STRIPS, -1)
        # It comes to a point: its middle hangs longest, its edges point shorter.
        np.testing.assert_allclose(strips[:, 20] - strips[:, 0], self.POINT, atol=1e-6)
        # And the strips do not all end alike.
        self.assertGreater(float(np.ptp(strips[:, 20])), 0.05)
        # The same seed tears it the same way.
        np.testing.assert_array_equal(kept, sheet.torn(u, self.STRIPS, self.CUT, self.POINT, 3.0))


class SheetsOnChains(unittest.TestCase):
    # Fixture: a cloak swept round her back from 105 to 255 degrees (from the front toward the left), on three chains
    # hung from 104 (just before its sweep begins), 180 and 256 degrees (just past its end), each three bones down.
    T0, T1 = np.radians(105.0), np.radians(255.0)

    def chains(self):
        def hung(degrees):
            a = np.radians(degrees)
            top = np.array([np.cos(a), np.sin(a), 0.0]) * 10.0 + np.array([0, 0, 100.0])
            return [top - np.array([0, 0, 20.0 * k]) for k in range(4)]
        return {"cape_l": hung(104.0), "cape": hung(180.0), "cape_r": hung(256.0)}

    def test_a_chain_just_outside_the_sweep_takes_its_nearer_end(self):
        shares = sheet.sweep_shares(self.chains(), self.T0, self.T1)
        self.assertAlmostEqual(shares["cape_l"], 0.0, places=6)
        self.assertAlmostEqual(shares["cape"], 0.5, places=6)
        self.assertAlmostEqual(shares["cape_r"], 1.0, places=6)

    def test_every_chain_carries_part_of_the_sheet(self):
        chains = self.chains()
        shares = sheet.sweep_shares(chains, self.T0, self.T1)
        u = np.linspace(0.0, 1.0, 41)
        P = np.stack([np.zeros_like(u), np.zeros_like(u), np.full_like(u, 70.0)], axis=1)
        weights = sheet.down_chains(chains, shares, P, u, np.zeros_like(u), "spine_03")
        for name in chains:
            carried = sum(w.sum() for bone, w in weights.items() if bone.startswith(name + "_0"))
            self.assertGreater(carried, 1.0, name)
        np.testing.assert_allclose(sum(weights.values()), 1.0, atol=1e-5)


class TornSheetsOnChains(unittest.TestCase):
    def test_a_torn_ribbon_is_weighted_where_its_tongues_end(self):
        # Fixture: one ribbon's chain, three 20 cm spans straight back from a shoulder, on a 180 cm body.
        joints = [np.array([-10.0 - 20.0 * k, 40.0, 150.0]) for k in range(4)]
        L = {"ribbon_l_%02d" % (k + 1): (joints[k], joints[k + 1]) for k in range(3)}
        L["ribbon_l_end"] = (joints[3], joints[3] + np.array([-1.0, 0, 0]))
        S = tree.Sculpt()
        ribbon = silt.flung(S, L, {"height": 180.0}, silt.materials(S), "l")
        u, v, P = ribbon.grid()
        weights = ribbon.bones(P, u, v)
        # Every vertex is weighted most to the span it lies along, however short its tongue is torn.
        along = np.clip((-10.0 - P[:, 0]) / 20.0, 0, 2.999).astype(int)
        names = sorted(weights)
        heaviest = np.array(names)[np.argmax(np.stack([weights[n] for n in names]), axis=0)]
        held = weights["clavicle_l"] > 0.5
        for k, name in enumerate(heaviest):
            if not held[k]:
                self.assertEqual(name, "ribbon_l_%02d" % (along[k] + 1), (k, P[k]))


if __name__ == "__main__":
    unittest.main()
