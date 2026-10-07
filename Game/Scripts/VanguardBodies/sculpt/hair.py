"""Stylised hair (ADR-069): a cap over the skull to the hairline and tapered clumps rooted across the scalp, each
flowing from the crown, falling with gravity and flicking at its tip, swept by wind to one side. Clumps read as
the large painted locks of a toon character, not strands. Built in a head's own frame."""
import math
import random

import numpy as np

from . import sdf, tree
from .tree import Box, Union


def V(*values):
    return np.asarray(values, dtype=np.float64)


def unit(v):
    v = np.asarray(v, dtype=np.float64)
    return v / max(np.linalg.norm(v), 1e-9)


def _bezier(p0, p1, p2, t):
    return (1 - t) ** 2 * p0 + 2 * (1 - t) * t * p1 + t * t * p2


def clump(S, name, root, direction, normal, length, radius, droop, curl, material, bones):
    """One lock from root: out along direction (in the scalp's plane), lifting off it along normal at first,
    drooping droop of its length toward the ground and curling curl at its tip. Tapers from radius to a point."""
    p0 = V(*root)
    d = unit(direction)
    n = unit(normal)
    points = guide(root, direction, normal, length, droop, curl)
    # Full at the root, swelling a little a third along, then tapering to a point.
    radii = [radius * r for r in (0.85, 1.0, 0.95, 0.78, 0.55, 0.3, 0.06)]
    shape = lambda P, pts=points, rs=radii: sdf.tube(P, pts, rs)  # noqa: E731
    return tree.leaf(S, name, shape, Box.around(points, radius * 1.2), material, bones)


def guide(root, direction, normal, length, droop, curl, samples=7):
    """A lock's guide curve: from root out along direction, lifting off the scalp along normal at first, drooping
    droop of its length and curling curl at its tip."""
    p0 = V(*root)
    d = unit(direction)
    n = unit(normal)
    p1 = p0 + d * length * 0.45 + n * length * 0.18
    p2 = p0 + d * length + V(0, 0, -droop * length) + n * curl * length
    return [_bezier(p0, p1, p2, t) for t in np.linspace(0.0, 1.0, samples)]


def messy(S, material, bones, skull_centre, skull_radii, hairline, seed, count=56, length=(7.0, 13.0), radius=(1.3, 2.0),
          wind=(0.0, -0.35, 0.0), fringe=0.8, volume=0.6, unit_scale=1.0, cap_bones=None):
    """Messy, windswept hair over a skull (an ellipsoid at skull_centre, skull_radii, in the head's frame): a cap to
    hairline (its height above the frame's origin at the brow, falling to the nape at the back) and count locks.
    wind sweeps them; fringe is how long the front locks are, as a share of the others; volume how much the locks high on
    the head stand up and out. bones skins the locks; cap_bones the cap (bones when None). Returns its node."""
    rng = random.Random(seed)
    c = V(*skull_centre) * unit_scale
    r = V(*skull_radii) * unit_scale
    nodes = []
    # The cap: the skull grown, kept above the hairline (which falls from the brow to the nape behind).
    brow, nape = hairline

    def cap_distance(P):
        Q = (P - c) / (r + 2.0 * unit_scale)
        k0 = np.linalg.norm(Q, axis=1)
        k1 = np.linalg.norm((P - c) / (r + 2.0 * unit_scale) ** 2, axis=1)
        shell = k0 * (k0 - 1.0) / np.maximum(k1, 1e-6)
        # The hairline: a plane tilted from the brow (front) down to the nape (back).
        front = np.clip((P[:, 0] - c[0]) / r[0], -1, 1)
        line = nape + (brow - nape) * (front * 0.5 + 0.5)
        return np.maximum(shell, (line * unit_scale) - P[:, 2])
    cap = tree.leaf(S, "hair_cap", cap_distance, Box(c - r - 2 * unit_scale, c + r + 2 * unit_scale), material, cap_bones or bones)
    nodes.append(cap)
    crown = c + V(-0.25 * r[0], 0, 0.95 * r[2])
    for i in range(count):
        # Roots over the scalp above the hairline: azimuth all round, elevation from near the hairline to the crown.
        azimuth = rng.uniform(-math.pi, math.pi)
        elevation = rng.uniform(0.05, 1.2)
        direction_out = V(math.cos(azimuth) * math.cos(elevation), math.sin(azimuth) * math.cos(elevation), math.sin(elevation))
        root = c + direction_out * (r + 0.6 * unit_scale)
        front = math.cos(azimuth)
        line = nape + (brow - nape) * (front * 0.5 + 0.5)
        if root[2] < line * unit_scale - 1.0 * unit_scale:
            continue
        normal = unit(direction_out / r)
        # Flow: away from the crown along the scalp, down with gravity, the front locks forward over the brow.
        away = root - crown
        away = away - normal * (away @ normal)
        flow = unit(away) if np.linalg.norm(away) > 1e-3 else unit(V(1, 0, 0))
        # Locks high on the head stand up and out (volume); lower ones fall; the front ones fall over the brow.
        height = (root[2] - c[2]) / r[2]
        lift = float(np.clip(height, 0.0, 1.0)) * volume
        flow = unit(flow + V(*wind) + V(0, 0, -0.45 + 0.5 * lift) + normal * lift + (V(0.6 * fringe, 0, -0.25) if front > 0.6 else V(0, 0, 0)))
        lo, hi = length
        reach = rng.uniform(lo, hi) * (fringe if front > 0.6 else 1.0) * unit_scale
        thick = rng.uniform(*radius) * unit_scale * (0.8 if front > 0.6 else 1.0)
        droop = rng.uniform(0.1, 0.35) * (1.0 - 0.6 * lift)
        # Tips flick out from the head at the sides and back, as wind-tossed hair does.
        curl = rng.uniform(-0.05, 0.35) + (0.2 if abs(math.sin(azimuth)) > 0.5 else 0.0)
        nodes.append(clump(S, "hair_lock_%02d" % i, root, flow, normal, reach, thick, droop, curl, material, bones))
    return Union(nodes, k=0.7 * unit_scale)


def on_chains(chains, head, centre, radii, reach):
    """Skinning for locks that sway (ADR-069 §7): a point at the scalp holds to head; out along its lock it passes, by
    how far beyond the skull it lies (reach: where it has passed whole), to the chains it lies nearest, and down each
    chain's bones (<chain>_01, _02, ..., one per span) by how far along it it lies. chains: {chain: its joints, root to
    tip, world}; centre and radii: the skull, an ellipsoid. Returns bones(P) -> {bone: weights}."""
    c, r = V(*centre), V(*radii)
    joints = {name: np.asarray(points, dtype=np.float64) for name, points in chains.items()}

    def bones(P):
        P = np.asarray(P, dtype=np.float64)
        out = np.clip((np.linalg.norm((P - c) / r, axis=1) - 1.0) * r.mean() / reach, 0.0, 1.0)
        weights = {head: 1.0 - out}
        nearness, spans_of = {}, {}
        for name, points in joints.items():
            # How near the chain each point lies, and where along it (in spans from its root).
            best = np.full(len(P), np.inf)
            along = np.zeros(len(P))
            for k in range(len(points) - 1):
                a, ab = points[k], points[k + 1] - points[k]
                t = np.clip(((P - a) @ ab) / max(ab @ ab, 1e-9), 0.0, 1.0)
                d = np.linalg.norm(P - (a + t[:, None] * ab), axis=1)
                along = np.where(d < best, k + t, along)
                best = np.minimum(best, d)
            # Shared among the chains by inverse square distance, so a lock between two blends.
            nearness[name] = 1.0 / np.maximum(best, 1e-3) ** 2
            spans_of[name] = along
        total = sum(nearness.values())
        for name, points in joints.items():
            share = out * nearness[name] / total
            spans = len(points) - 1
            # Each span's bone most at its middle, blending into its neighbours'; the root's and tip's ends wholly theirs.
            x = np.clip(spans_of[name], 0.5, spans - 0.5)
            hats = [np.clip(1.0 - np.abs(x - (k + 0.5)), 0.0, 1.0) for k in range(spans)]
            whole = sum(hats)
            for k, hat in enumerate(hats):
                bone = "%s_%02d" % (name, k + 1)
                weights[bone] = weights.get(bone, 0.0) + share * hat / whole
        return {bone: np.asarray(w, dtype=np.float32) for bone, w in weights.items()}
    return bones