"""Garments and gear for a sculpt (ADR-069): regions a garment covers, layered shells with fabric folds, straps that
wrap a body along a band, and hard-surface pieces (buckle frames, studs, pouches) set onto a surface facing out."""
import math

import numpy as np

from . import sdf, tree
from .tree import Box, Shell, Union, Zone

BIG = 1.0e4


def V(*values):
    return np.asarray(values, dtype=np.float64)


def unit(v):
    v = np.asarray(v, dtype=np.float64)
    return v / max(np.linalg.norm(v), 1e-9)


# ---------------------------------------------------------------------------------------------- regions
def band_z(lo, hi, bounds_xy=(-200.0, 200.0)):
    """Everything between heights lo and hi."""
    return Zone(lambda P: np.maximum(lo - P[:, 2], P[:, 2] - hi), Box((bounds_xy[0], bounds_xy[0], lo), (bounds_xy[1], bounds_xy[1], hi)))


def band_plane(point, normal, width, box):
    """Everything within width / 2 of the plane through point across normal (a sash or strap's band), within box."""
    n = unit(normal)
    p = V(*point)
    return Zone(lambda P: np.abs((P - p) @ n) - width * 0.5, box)


def tilted_band(centre, normal, width, box):
    """A belt's band: within width / 2 of the plane through centre across normal (tilt the normal to drop a belt to one
    side)."""
    return band_plane(centre, normal, width, box)


def around(points, radii, box_pad=0.0):
    """Everything within a limb: the polyline points with radius radii[i] at each, eased between them."""
    pts = [V(*p) for p in points]

    def distance(P):
        d = None
        for i in range(len(pts) - 1):
            piece = sdf.round_cone(P, pts[i], pts[i + 1], radii[i], radii[i + 1])
            d = piece if d is None else np.minimum(d, piece)
        return d
    return Zone(distance, Box.around(pts, max(radii) + box_pad))


def half(point, normal, box):
    """Everything on the side of the plane through point that normal points away from."""
    n = unit(normal)
    p = V(*point)
    return Zone(lambda P: (P - p) @ n, box)


class _Regions(tree.Node):
    """Regions combined: each read through its field, so a region that evaluates nodes (tree.Nearer) combines too."""

    def __init__(self, regions, combine, bounds):
        self.regions, self.combine, self.bounds = regions, combine, bounds

    def eval(self, P, box):
        d = self.combine([region.field(P, box).d for region in self.regions])
        return sdf.Field(d.astype(np.float32), np.full(len(P), -1, dtype=np.int16))


def both(*zones):
    """Where every zone holds."""
    zones = [z for z in zones if z is not None]
    box = zones[0].bounds
    for z in zones[1:]:
        box = box.intersection(z.bounds)
    return _Regions(zones, lambda ds: np.max(np.stack(ds), axis=0), box)


def either(*zones):
    """Where any zone holds."""
    box = zones[0].bounds
    for z in zones[1:]:
        box = box.union(z.bounds)
    return _Regions(list(zones), lambda ds: np.min(np.stack(ds), axis=0), box)


def without(zone, cut):
    """zone with cut taken out."""
    return _Regions([zone, cut], lambda ds: np.maximum(ds[0], -ds[1]), zone.bounds)


def keep_to(limbs, names, margin=6.0):
    """Where a point lies nearer the named limbs' surfaces than any other limb's (limbs: a figure's, each part of the
    body in one): a garment that keeps to its limbs in any pose."""
    return tree.Nearer([limbs[n] for n in names], [node for n, node in limbs.items() if n not in names], margin)


# ---------------------------------------------------------------------------------------------- folds
def folds(axis, count, depth, along=None, period=None, phase=0.0, seed=0, noise=0.4):
    """A fabric fold displacement: ridges running along axis (unit), count of them around it (or every period cm
    along along), depth deep, jittered by noise. Returns displace(P) -> cm outward."""
    a = unit(axis)

    def displace(P):
        if along is not None:
            t = (P - V(*along[0])) @ unit(V(*along[1]) - V(*along[0]))
            wave = np.sin(t / period * 2 * math.pi + phase)
        else:
            # Around the axis: the angle about it, from any perpendicular.
            ref = unit(np.cross(a, V(0, 0, 1)) if abs(a[2]) < 0.9 else np.cross(a, V(1, 0, 0)))
            other = np.cross(a, ref)
            ang = np.arctan2(P @ other, P @ ref)
            wave = np.sin(ang * count + phase)
        jitter = sdf.fbm(P.astype(np.float32), 12.0, 2, seed) - 0.5
        return (depth * (0.5 + 0.5 * wave) * (1.0 + noise * 2 * jitter)).astype(np.float32)
    return displace


def bunch(axis_start, axis_end, rings, depth, seed=0):
    """Rings of bunched cloth round a limb (trousers over a boot, a rolled sleeve): ridges across the axis, irregular."""
    a, b = V(*axis_start), V(*axis_end)
    ab = unit(b - a)
    length = np.linalg.norm(b - a)

    def displace(P):
        s = ((P - a) @ ab) / length
        # Only within its span, easing in and out at its ends.
        window = np.clip(np.minimum(s, 1.0 - s) * 6.0, 0.0, 1.0)
        jitter = sdf.fbm(P.astype(np.float32), 7.0, 2, seed)
        wave = np.abs(np.sin((np.clip(s, 0, 1) * rings + jitter * 0.5) * math.pi))
        return (depth * wave * window).astype(np.float32)
    return displace


def combine(*displacements):
    return lambda P: sum(d(P) for d in displacements)


# ---------------------------------------------------------------------------------------------- drapes
def curve(points, count):
    """count points along a Catmull-Rom curve through points, its ends held."""
    P = np.asarray(points, dtype=np.float64)
    ext = np.vstack([2 * P[0] - P[1], P, 2 * P[-1] - P[-2]])
    spans = len(P) - 1
    out = []
    for s in np.linspace(0.0, spans, count):
        i = min(int(s), spans - 1)
        u = s - i
        p0, p1, p2, p3 = ext[i], ext[i + 1], ext[i + 2], ext[i + 3]
        out.append(0.5 * (2 * p1 + (p2 - p0) * u + (2 * p0 - 5 * p1 + 4 * p2 - p3) * u * u + (3 * p1 - p0 - 3 * p2 + p3) * u ** 3))
    return np.array(out)


def nearest_on(P, line):
    """Each of P's distance to the polyline line, the line's parameter at its nearest point (0 at its start, 1 at its
    end, by length), and that nearest point."""
    A, AB = line[:-1], line[1:] - line[:-1]
    lengths = np.linalg.norm(AB, axis=1)
    start = np.concatenate([[0.0], np.cumsum(lengths)[:-1]]).astype(np.float32)
    rel = P[:, None, :] - A[None, :, :]
    u = np.clip(np.einsum("nmk,mk->nm", rel, AB) / np.maximum(lengths ** 2, 1e-9), 0.0, 1.0)
    dist = np.linalg.norm(rel - u[:, :, None] * AB[None, :, :], axis=2)
    j = np.argmin(dist, axis=1)
    rows = np.arange(len(P))
    nearest = A[j] + u[rows, j][:, None] * AB[j]
    return dist[rows, j], (start[j] + u[rows, j] * lengths[j]) / max(float(lengths.sum()), 1e-9), nearest


def fold(S, name, base, points, width, height, material, bones=None, seed=0, samples=24, crease=0.35):
    """A fold of draped cloth lying along a curve over base: a ridge height high at its middle and width either side
    of the curve, tapering toward both ends, solid down into base. Hanging cloth folds over its own weight, so the
    ridge's crest lies crease of its width below the curve: rounded above, falling sharply into the crease beneath.
    points: the curve's control points, on base's surface (laid_on)."""
    line = curve(points, samples).astype(np.float32)

    def shape(P):
        s, t, nearest = nearest_on(P.astype(np.float32), line)
        # Clamped: pi in single precision overshoots, so sin reads a hair below zero at the curve's end.
        bell = np.clip(np.sin(np.pi * np.clip(t, 0.0, 1.0)), 0.0, 1.0)
        below = P[:, 2] < nearest[:, 2]
        return np.where(below, -s, s), width * (0.4 + 0.6 * bell ** 0.5), height * bell ** 0.6

    def region(P):
        s, w, _h = shape(P)
        return np.abs(s) - w

    def ridge(P):
        s, w, h = shape(P)
        u = np.clip(s / w, -1.0, 1.0)
        # Above the crest a long rounded side; below it a short steep one.
        side = np.where(u >= -crease, (u + crease) / (1.0 + crease), (-crease - u) / (1.0 - crease))
        across = np.clip(1.0 - side ** 2, 0.0, 1.0) ** np.where(u >= -crease, 0.8, 0.45)
        wobble = 1.0 + 0.3 * (sdf.fbm(P.astype(np.float32), 6.0, 2, seed) - 0.5)
        return (h * across * wobble).astype(np.float32)
    zone = Zone(region, Box.around(line, max(width, height) + 1.0))
    return Shell(S, name, base, 0.0, 0.3, zone, material, hem=0.6, bones=bones, displace=ridge, reach=height + 0.5)


def laid_on(base, points, tilt=30.0, reach=45.0):
    """points moved onto base's surface, each along a ray coming in toward the vertical axis through the origin and
    down at tilt degrees: where cloth laid there would rest."""
    P = np.asarray(points, dtype=np.float64).reshape(-1, 3)
    radial = P * np.array([1.0, 1.0, 0.0])
    radial /= np.maximum(np.linalg.norm(radial, axis=1, keepdims=True), 1e-9)
    out = radial * math.cos(math.radians(tilt)) + np.array([0.0, 0.0, math.sin(math.radians(tilt))])
    hits, _normals = surface_points(base, P + out * 25.0, -out, reach=reach)
    return hits

def quilt(spacing, depth, radius=14.0):
    """Quilted leather's pillows, for a garment's fine relief (baked, never meshed): a diamond grid round the body's
    height axis, spacing apart (cm, measured round it at radius), each pillow depth proud, sinking to its seams."""
    def displace(P):
        around = np.arctan2(P[:, 1], P[:, 0]) * radius
        u = (around + P[:, 2]) / spacing
        v = (around - P[:, 2]) / spacing
        edge = np.maximum(np.abs(u % 1.0 - 0.5), np.abs(v % 1.0 - 0.5)) * 2.0
        return (depth * (1.0 - edge ** 4)).astype(np.float32)
    return displace


# ---------------------------------------------------------------------------------------------- placing on a surface
def surface_point(node, start, direction, reach=60.0, steps=48, refine=12):
    """Where a ray from start along direction first meets node's surface, and the outward normal there: the ray's
    samples evaluated together, then the crossing bisected."""
    d0 = unit(direction)
    s = V(*start)
    box = Box.around([s, s + d0 * reach], 2.0)
    ts = np.linspace(0.0, reach, steps + 1)
    values = tree.evaluate(node, (s[None, :] + ts[:, None] * d0[None, :]).astype(np.float32), box).d
    inside = values[0] < 0
    crossing = np.nonzero((values < 0) != inside)[0]
    if len(crossing):
        lo, hi = ts[crossing[0] - 1], ts[crossing[0]]
    else:
        lo, hi = ts[-2], ts[-1]
    for _ in range(refine):
        mid = (lo + hi) * 0.5
        if (f_at(node, s + d0 * mid) < 0) == inside:
            lo = mid
        else:
            hi = mid
    p = s + d0 * hi
    e = 0.05
    probes = np.array([p + V(*o) * e for o in ((1, 0, 0), (0, 1, 0), (0, 0, 1))] + [p - V(*o) * e for o in ((1, 0, 0), (0, 1, 0), (0, 0, 1))])
    f = tree.evaluate(node, probes.astype(np.float32), Box.around(probes, 1.0)).d
    return p, unit(f[:3] - f[3:])

def surface_points(node, starts, directions, reach=60.0, steps=48, refine=12):
    """surface_point for many rays at once (each from its start along its direction): their points and outward normals,
    (n, 3) each. One evaluation a step, so a garment laid round a body by dozens of rays costs about one."""
    S = np.asarray(starts, dtype=np.float64).reshape(-1, 3)
    D = np.asarray(directions, dtype=np.float64).reshape(-1, 3)
    D = D / np.maximum(np.linalg.norm(D, axis=1, keepdims=True), 1e-9)
    ts = np.linspace(0.0, reach, steps + 1)
    samples = S[:, None, :] + ts[None, :, None] * D[:, None, :]
    values = tree.evaluate(node, samples.reshape(-1, 3).astype(np.float32), Box.around(samples, 2.0)).d.reshape(len(S), -1)
    inside = values[:, 0] < 0
    change = (values < 0) != inside[:, None]
    first = np.argmax(change, axis=1)
    found = change.any(axis=1)
    lo = np.where(found, ts[np.maximum(first - 1, 0)], ts[-2])
    hi = np.where(found, ts[first], ts[-1])
    for _ in range(refine):
        mid = (lo + hi) * 0.5
        Q = S + D * mid[:, None]
        same = (tree.evaluate(node, Q.astype(np.float32), Box.around(Q, 2.0)).d < 0) == inside
        lo, hi = np.where(same, mid, lo), np.where(same, hi, mid)
    p = S + D * hi[:, None]
    e = 0.05
    offsets = np.concatenate([np.eye(3), -np.eye(3)]) * e
    probes = (p[:, None, :] + offsets[None, :, :]).reshape(-1, 3)
    f = tree.evaluate(node, probes.astype(np.float32), Box.around(probes, 1.0)).d.reshape(len(S), 6)
    g = f[:, :3] - f[:, 3:]
    return p, g / np.maximum(np.linalg.norm(g, axis=1, keepdims=True), 1e-9)


def f_at(node, p):
    """node's distance at the one point p."""
    box = Box.around([p], 2.0)
    return float(tree.evaluate(node, np.asarray(p, dtype=np.float32)[None, :], box).d[0])


def facing_frame(normal, up=(0, 0, 1)):
    """Axes (columns x, y, z) of a piece set on a surface: z out along normal, y as near up as allowed."""
    z = unit(normal)
    y = V(*up) - z * (V(*up) @ z)
    y = unit(y) if np.linalg.norm(y) > 1e-3 else unit(np.cross(z, V(1, 0, 0)))
    x = np.cross(y, z)
    return np.stack([x, y, z], axis=1)


# ---------------------------------------------------------------------------------------------- hard-surface pieces
def buckle(S, name, centre, axes, width, height, bar, depth, material, bones=None, detail=True):
    """A buckle's frame on a strap: a rounded rectangle ring width x height (cm), its bar bar thick, depth proud of the
    strap, with a prong across it. axes: columns x (across the strap), y (along it), z (out)."""
    c = V(*centre)
    R = np.asarray(axes)

    def distance(P):
        Q = (P - c) @ R
        outer = sdf.box(Q, (0, 0, depth * 0.5), (width * 0.5, height * 0.5, depth * 0.5), None, bar * 0.4)
        inner = sdf.box(Q, (0, 0, depth * 0.5), (width * 0.5 - bar, height * 0.5 - bar, depth), None, bar * 0.3)
        ring = np.maximum(outer, -inner)
        prong = sdf.capsule(Q, (0, -height * 0.5 + bar, depth * 0.6), (0, height * 0.15, depth * 0.75), bar * 0.28)
        return np.minimum(ring, prong)
    r = max(width, height)
    return tree.leaf(S, name, distance, Box(c - r, c + r), material, bones, detail)


def studs(S, name, points, normals, radius, material, bones=None, detail=True):
    """Round studs (rivets) at points, each a flattened dome along its normal."""
    pts = [V(*p) for p in points]
    ns = [unit(n) for n in normals]

    def distance(P):
        d = None
        for p, n in zip(pts, ns):
            piece = sdf.ellipsoid(P, p, (radius, radius, radius * 0.55), facing_frame(n))
            d = piece if d is None else np.minimum(d, piece)
        return d
    return tree.leaf(S, name, distance, Box.around(pts, radius * 2), material, bones, detail)


def pouch(S, name, centre, axes, size, flap, material, flap_material=None, bones=None, rounding=0.6):
    """A leather pouch: a rounded box size (x across, y up, z out) at centre, its flap over the top third, a little
    proud of the face."""
    c = V(*centre)
    R = np.asarray(axes)
    hx, hy, hz = (s * 0.5 for s in size)

    def body_distance(P):
        Q = (P - c) @ R
        return sdf.box(Q, (0, 0, hz), (hx, hy, hz), None, rounding)

    def flap_distance(P):
        Q = (P - c) @ R
        return sdf.box(Q, (0, hy - flap * 0.5, hz + 0.25), (hx + 0.15, flap * 0.5, hz + 0.1), None, rounding * 0.8)
    r = max(size) * 1.2
    body = tree.leaf(S, name, body_distance, Box(c - r, c + r), material, bones)
    lid = tree.leaf(S, name + "_flap", flap_distance, Box(c - r, c + r), flap_material or material, bones)
    return Union([body, lid])


# ---------------------------------------------------------------------------------------------- boots
def boot(S, name, L, side, top, materials, bones, strap_heights=(), shaft=1.0, toe=1.0):
    """A heavy boot on one leg: a shaft fitted over the calf from the ankle to top (cm), the foot's upper over the
    instep tapering to a rounded toe, a thick sole with a raised heel, and straps round the shaft at strap_heights,
    each buckled on the outside. materials: {"boot", "sole", "strap", "buckle"}. Returns its node."""
    sign = 1.0 if side == "l" else -1.0
    knee, ankle = V(*L["calf_" + side][0]), V(*L["calf_" + side][1])
    f0, f1 = V(*L["foot_" + side][0]), V(*L["foot_" + side][1])
    ground_x = f0[0]
    # The foot's length (heel to toe) from its bone, a little past each end.
    heel_x, toe_x = ground_x - 5.5, f1[0] + 4.5 * toe
    y = f0[1]
    up = V(0, 0, 1)
    top_point = ankle + (knee - ankle) * ((top - ankle[2]) / max(knee[2] - ankle[2], 1e-6))
    shaft_loft = sdf.Loft(V(ankle[0] - 0.5, y, 4.0), top_point, (1, 0, 0),
                          [(0.0, 0.6, 0, 6.6 * shaft, 5.6 * shaft, 2.4), (0.35, 0.0, 0, 6.0 * shaft, 5.4 * shaft, 2.4),
                           (0.8, -0.4, 0, 6.6 * shaft, 6.0 * shaft, 2.4), (1.0, -0.4, 0, 7.0 * shaft, 6.4 * shaft, 2.2)], cap=1.0)
    # The upper over the foot: along it from heel to toe, tall at the ankle and low at the toe, wide across the ball.
    upper = sdf.Loft(V(heel_x, y, 4.6), V(toe_x, y, 4.6), (0, 0, 1),
                     [(0.0, 1.8, 0, 4.4, 4.8, 2.6), (0.3, 2.6, 0, 5.4, 5.4, 2.6), (0.62, 0.4, 0, 3.6, 5.8, 2.6),
                      (0.88, -0.4, sign * -0.3, 2.6, 5.2, 2.4), (1.0, -0.8, sign * -0.4, 2.0, 3.6, 2.2)], cap=1.6)
    sole = sdf.Loft(V(heel_x - 0.6, y, 1.4), V(toe_x + 0.6, y, 1.4), (0, 0, 1),
                    [(0.0, 0.2, 0, 1.6, 5.2, 4.0), (0.3, 0.0, 0, 1.6, 5.0, 4.0), (0.5, -0.1, 0, 1.3, 5.4, 4.0),
                     (0.88, 0.0, sign * -0.3, 1.3, 5.6, 3.6), (1.0, 0.0, sign * -0.4, 1.3, 4.0, 3.0)], cap=0.5)
    heel = sdf.Loft(V(heel_x - 0.4, y, 0.0), V(heel_x - 0.4, y, 3.4), (1, 0, 0), [(0.0, 3.0, 0, 3.8, 4.9, 4.0), (1.0, 3.0, 0, 3.6, 4.8, 4.0)], cap=0.4)
    nodes = [tree.leaf(S, name + "_shaft", shaft_loft, Box.around(shaft_loft.bounds_points()), materials["boot"], bones),
             tree.leaf(S, name + "_upper", upper, Box.around(upper.bounds_points()), materials["boot"], bones)]
    body = Union(nodes, k=1.6)
    toe_cap = tree.leaf(S, name + "_toe", lambda P, c=V(toe_x - 3.2, y + sign * -0.2, 5.2): sdf.ellipsoid(P, c, (4.4, 5.2, 2.9)),
                        Box((toe_x - 9, y - 7, 0), (toe_x + 2, y + 7, 10)), materials["boot"], bones)
    soles = Union([tree.leaf(S, name + "_sole", sole, Box.around(sole.bounds_points()), materials["sole"], bones),
                   tree.leaf(S, name + "_heel", heel, Box.around(heel.bounds_points()), materials["sole"], bones)], k=0.3)
    # Lugs cut across the tread (detail, baked into the textures).
    lugs = tree.leaf(S, name + "_lugs", lambda P: np.maximum(np.abs(((P[:, 0] - heel_x) % 2.4) - 1.2) - 0.35, P[:, 2] - 0.8),
                     Box((heel_x - 2, y - 7, -1), (toe_x + 2, y + 7, 1.2)), materials["sole"], bones, True)
    soles = tree.Subtract(soles, lugs, k=0.15)
    straps = []
    for i, height in enumerate(strap_heights):
        point = ankle + (knee - ankle) * ((height - ankle[2]) / max(knee[2] - ankle[2], 1e-6))
        band = band_z(height - 1.3, height + 1.3)
        straps.append(Shell(S, "%s_strap_%d" % (name, i), body, 0.0, 0.7, band, materials["strap"], hem=0.15, bones=None, detail=True))
        out = unit(V(0.25, sign, 0.0))
        p = point + out * (6.6 * shaft + 0.7)
        straps.append(buckle(S, "%s_buckle_%d" % (name, i), p, facing_frame(out, (0, 0, 1)), 2.3, 2.9, 0.42, 0.55, materials["buckle"], bones))
    return Union([body, toe_cap, soles] + straps, k=0.0)
