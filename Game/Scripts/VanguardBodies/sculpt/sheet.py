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
