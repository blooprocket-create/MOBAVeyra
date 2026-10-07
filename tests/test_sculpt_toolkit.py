"""Tests for the sculpt toolkit's garments and regions (ADR-069 §2a): layered cloth with no air beneath it, regions
that keep a garment to its limbs, folds that stay finite to their ends, and rays laid onto a surface. numpy only."""
from __future__ import annotations

import sys
import unittest
from pathlib import Path

import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "Game" / "Scripts"))
from VanguardBodies.sculpt import garments, sdf, tree  # noqa: E402
from VanguardBodies.sculpt.tree import Box  # noqa: E402

# Fixture sizes (cm): a ball standing in for a body part, the cloth over it, a fold's height above it.
RADIUS = 10.0
CLOTH = 0.8
RIDGE = 2.5
# How far to look along a ray, and how finely.
SPAN = 20.0
STEP = 0.05


def ball(S, name, centre, radius):
    c = np.asarray(centre, dtype=np.float32)
    return tree.leaf(S, name, lambda P: sdf.sphere(P, c, radius), Box(c - radius, c + radius), S.material(name), None)


def along(node, direction, start=(0.0, 0.0, 0.0), mode="form"):
    """node's distances along a ray from start (each sample STEP apart) and the sample spacing."""
    d = np.asarray(direction, dtype=np.float32)
    d /= np.linalg.norm(d)
    ts = np.arange(0.0, SPAN, STEP, dtype=np.float32)
    P = np.asarray(start, dtype=np.float32)[None, :] + ts[:, None] * d[None, :]
    tree.MODE[0] = mode
    tree.REACH[0] = 1.0
    return ts, tree.evaluate(node, P, Box.around(P, 1.0)).d


class SolidLayers(unittest.TestCase):
    def setUp(self):
        self.S = tree.Sculpt()
        self.body = ball(self.S, "body", (0, 0, 0), RADIUS)
        self.everywhere = tree.Zone(lambda P: np.full(len(P), -1.0, dtype=np.float32), Box((-50, -50, -50), (50, 50, 50)))

    def test_a_garment_leaves_no_air_between_it_and_what_it_covers(self):
        coat = tree.Shell(self.S, "coat", self.body, 0.4, CLOTH - 0.4, self.everywhere, self.S.material("coat"))
        ts, d = along(tree.Over([self.body, coat]), (1, 0.3, 0.2))
        inside = d < 0
        # Solid from the centre out to the cloth's face, then outside: one surface, no gap at the body's.
        self.assertTrue(inside[0])
        edge = np.argmin(inside)
        self.assertFalse(inside[edge:].any(), "a gap of air under the cloth")
        self.assertAlmostEqual(float(ts[edge]), RADIUS + CLOTH, delta=STEP * 2)

    def test_a_fold_raises_the_outer_face_and_stays_solid_beneath(self):
        lifted = tree.Shell(self.S, "fold", self.body, 0.0, CLOTH, self.everywhere, self.S.material("fold"),
                            displace=lambda P: np.full(len(P), RIDGE, dtype=np.float32), reach=RIDGE)
        ts, d = along(tree.Over([self.body, lifted]), (0, 0, 1))
        inside = d < 0
        edge = np.argmin(inside)
        self.assertFalse(inside[edge:].any(), "a fold standing off the body with air beneath it")
        self.assertAlmostEqual(float(ts[edge]), RADIUS + CLOTH + RIDGE, delta=STEP * 2)

    def test_fine_relief_is_baked_but_never_meshed(self):
        quilted = tree.Shell(self.S, "quilted", self.body, 0.0, CLOTH, self.everywhere, self.S.material("quilted"),
                             fine=lambda P: np.full(len(P), 0.3, dtype=np.float32), reach=0.3)
        surfaces = {}
        for mode in ("form", "detail"):
            ts, d = along(tree.Over([self.body, quilted]), (0, 1, 0), mode=mode)
            surfaces[mode] = float(ts[np.argmin(d < 0)])
        self.assertAlmostEqual(surfaces["form"], RADIUS + CLOTH, delta=STEP * 2)
        self.assertAlmostEqual(surfaces["detail"], RADIUS + CLOTH + 0.3, delta=STEP * 2)


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
        piece = garments.fold(S, "fold", body, garments.laid_on(body, points, tilt=0.0), 3.0, RIDGE, S.material("cloth"))
        # Around both ends, where the curve's parameter reaches 0 and 1 (single precision overshoots pi there).
        grid = np.stack(np.meshgrid(np.linspace(-14, 14, 15), np.linspace(-14, 14, 15), np.linspace(-6, 6, 7), indexing="ij"), axis=-1).reshape(-1, 3)
        P = (grid + np.array([RADIUS, 0.0, 0.0])).astype(np.float32)
        tree.MODE[0] = "form"
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


if __name__ == "__main__":
    unittest.main()
