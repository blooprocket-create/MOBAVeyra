"""Cloth as single sheets (ADR-069): a cloak or a coat's tails is a grid surface (u across, v down) draped by a
function, built at the density the game mesh keeps, two-sided, its hem torn in its geometry, skinned by its builder
down its spring chains. Sheets join the game mesh after it is reduced, each coloured by its own material."""
import numpy as np

from . import paint


class Sheet:
    """A draped surface: position(u, v) -> (n, 3) for u, v in [0, 1]; its material (a tree.Material); bones(P, u, v)
    -> {bone: weights}."""

    def __init__(self, name, position, material, bones, columns, rows):
        self.name, self.position, self.material, self.bones = name, position, material, bones
        self.columns, self.rows = columns, rows


def clear_of(points, limbs, margin):
    """points pushed out of each limb (a segment and its radius) to its surface plus margin: cloth over a raised arm."""
    P = points.copy()
    for a, b, radius in limbs:
        a, b = np.asarray(a, dtype=np.float64), np.asarray(b, dtype=np.float64)
        ab = b - a
        t = np.clip(((P - a) @ ab) / (ab @ ab), 0, 1)
        nearest = a + t[:, None] * ab
        away = P - nearest
        dist = np.linalg.norm(away, axis=1)
        inside = dist < radius + margin
        P[inside] = nearest[inside] + away[inside] / np.maximum(dist[inside], 1e-6)[:, None] * (radius + margin)
    return P


def torn(u, strips, cut, point, salt):
    """How much of a sheet's length each column keeps: its hem torn into strips, each ending at its own share of the
    length past cut, pointed at its middle by point. A sheet built with twice as many columns as strips puts a vertex
    at each strip's point and its edges."""
    u = np.asarray(u, dtype=np.float64)
    strip = np.floor(u * strips)
    across = np.abs((u * strips) % 1.0 - 0.5) * 2.0
    return np.clip(cut + (1.0 - cut) * paint.hashed(strip, salt) - point * across, 0.05, 1.0)


def build(sheet, bone_names, number):
    """A mesh object of sheet: its grid's vertices, quads, vertex groups weighted by its builder, and a "sheet" face
    attribute (number) saying which sheet each face is, read when it is coloured and removed before export."""
    # Blender's modules only here: a model's script, which makes sheets, also runs in workers without Blender.
    import bmesh
    import bpy

    cols, rows = sheet.columns, sheet.rows
    u = np.linspace(0.0, 1.0, cols + 1)
    v = np.linspace(0.0, 1.0, rows + 1)
    U, Vv = np.meshgrid(u, v, indexing="ij")
    P = sheet.position(U.ravel(), Vv.ravel()).reshape(-1, 3)
    mesh = bpy.data.meshes.new(sheet.name)
    bm = bmesh.new()
    verts = [bm.verts.new(p) for p in P]
    index = lambda i, j: i * (rows + 1) + j  # noqa: E731
    for i in range(cols):
        for j in range(rows):
            bm.faces.new((verts[index(i, j)], verts[index(i, j + 1)], verts[index(i + 1, j + 1)], verts[index(i + 1, j)]))
    bm.to_mesh(mesh)
    bm.free()
    obj = bpy.data.objects.new(sheet.name, mesh)
    bpy.context.scene.collection.objects.link(obj)
    for name in bone_names:
        obj.vertex_groups.new(name=name)
    weights = sheet.bones(P, U.ravel(), Vv.ravel())
    for bone, w in weights.items():
        group = obj.vertex_groups[bone]
        for value in np.unique(np.round(w[w > 0.005], 3)):
            group.add(np.nonzero(np.round(w, 3) == value)[0].tolist(), float(value), "REPLACE")
    marks = mesh.attributes.new("sheet", "INT", "FACE")
    marks.data.foreach_set("value", np.full(len(mesh.polygons), number, dtype=np.int32))
    for polygon in mesh.polygons:
        polygon.use_smooth = True
    return obj


def sweep_shares(chains, t0, t1):
    """Where round the body each chain hangs, as a share of a sheet's sweep from t0 to t1 (radians from the front
    toward the left, t1 past t0): its first joint's angle measured from t0 and divided by the sweep. A chain hung
    outside the sweep takes its nearer end (0 or 1), the gap between the sweep's ends split at its middle."""
    sweep = t1 - t0
    shares = {}
    for name, points in chains.items():
        offset = (np.arctan2(points[0][1], points[0][0]) - t0) % (2.0 * np.pi)
        if offset > sweep + (2.0 * np.pi - sweep) / 2.0:
            offset -= 2.0 * np.pi
        shares[name] = float(np.clip(offset / sweep, 0.0, 1.0))
    return shares


def down_chains(chains, shares, P, u, hold, holder):
    """Weights for a sheet hung on chains round the body (shares from sweep_shares): across the chains by where round
    the body it hangs (each point shared between the chains either side of it, wholly the end chain's beyond it), down
    each chain's bones (<chain>_01, _02, ...) by height, and its top held by holder as far as hold (0 to 1) says."""
    names = sorted(shares, key=shares.get)
    spots = np.array([shares[n] for n in names])
    w = {}
    for i, name in enumerate(names):
        left = spots[i - 1] if i > 0 else spots[i] - 1.0
        right = spots[i + 1] if i + 1 < len(names) else spots[i] + 1.0
        share = np.where(u <= spots[i], np.clip((u - left) / max(spots[i] - left, 1e-6), 0, 1),
                         np.clip((right - u) / max(right - spots[i], 1e-6), 0, 1))
        if i == 0:
            share = np.where(u <= spots[i], 1.0, share)
        if i + 1 == len(names):
            share = np.where(u >= spots[i], 1.0, share)
        points = chains[name]
        spans = len(points) - 1
        span = points[0][2] - points[-1][2]
        t = np.clip((points[0][2] - P[:, 2]) / span, 0, 1) * spans
        for k in range(spans):
            hat = np.clip(1 - np.abs(np.clip(t, 0.5, spans - 0.5) - (k + 0.5)), 0, 1)
            bone = "%s_%02d" % (name, k + 1)
            w[bone] = w.get(bone, 0.0) + hat * share
    for k in w:
        w[k] = w[k] * (1 - hold)
    w[holder] = hold
    total = sum(w.values())
    return {k: (np.asarray(x) / np.maximum(total, 1e-6)).astype(np.float32) for k, x in w.items()}