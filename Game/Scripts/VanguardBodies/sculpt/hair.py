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
          wind=(0.0, -0.35, 0.0), fringe=0.8, volume=0.6, core=0.85, unit_scale=1.0):
    """Messy, windswept hair over a skull (an ellipsoid at skull_centre, skull_radii, in the head's frame): a cap to
    hairline (its height above the frame's origin at the brow, falling to the nape at the back) and count locks.
    wind sweeps them; fringe is how long the front locks are, as a share of the others; volume how much the locks high on
    the head stand up and out. Returns (the solid core, the locks' guides for hair cards): core is how much of each lock
    the solid keeps under its cards."""
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
    cap = tree.leaf(S, "hair_cap", cap_distance, Box(c - r - 2 * unit_scale, c + r + 2 * unit_scale), material, bones)
    nodes.append(cap)
    crown = c + V(-0.25 * r[0], 0, 0.95 * r[2])
    guides = []
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
        # The solid core is a little smaller than the lock its cards lay over, so the cards' strands make its surface.
        nodes.append(clump(S, "hair_lock_%02d" % i, root, flow, normal, reach * core, thick * core, droop, curl, material, bones))
        guides.append({"points": guide(root, flow, normal, reach, droop, curl), "normal": normal, "radius": thick})
    return Union(nodes, k=0.7 * unit_scale), guides


def card_material(S, name, colour, highlight, strands=14, preview=None):
    """Hair cards' paint (ADR-069): strands running along each card (v from root to tip), darker at the root and lit in
    streaks, each strand ending at its own length so the tips fray; between strands the card is open (its opacity)."""
    from . import paint
    base = paint.linear(colour)
    light = paint.linear(highlight)

    def strand_index(u):
        return np.floor(u * strands)

    def hashed(x, salt):
        return np.modf(np.sin(x * 12.9898 + salt * 78.233) * 43758.5453)[0] % 1.0

    def colour_of(ctx):
        u, v = ctx["uv"][:, 0], ctx["uv"][:, 1]
        index = strand_index(u)
        streak = np.abs(hashed(index, 1.0))
        tone = np.clip(0.55 + 0.45 * v, 0, 1)[:, None] * (base[None, :] * (1 - 0.35 * streak[:, None]) + light[None, :] * 0.35 * streak[:, None])
        return np.clip(tone * (0.6 + 0.4 * ctx["ao"])[:, None], 0, 1).astype(np.float32)

    def opacity_of(ctx):
        u, v = ctx["uv"][:, 0], ctx["uv"][:, 1]
        index = strand_index(u)
        across = (u * strands) % 1.0
        # Each strand a little narrower toward its tip; the edge strands sparser.
        width = 0.7 - 0.35 * v
        edge = np.minimum(u, 1 - u) * 2.0
        keep = np.abs(across - 0.5) < width * 0.5 * np.clip(edge * 1.6, 0.4, 1.0)
        ends = 0.72 + 0.28 * np.abs(hashed(index, 2.0))
        return (keep & (v < ends)).astype(np.float32)
    return S.material(name, colour_of, opacity=opacity_of, preview=preview or colour)
