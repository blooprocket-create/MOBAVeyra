"""Cloth as single sheets (ADR-069): a cloak or a coat's tails is a grid surface (u across, v down) draped by a
function, built at the density the game mesh keeps, two-sided, tattered through the mask's opacity, skinned by its
builder. Sheets join the game mesh after it is reduced, and the bake paints them by their own material."""
import bmesh
import bpy
import numpy as np


class Sheet:
    """A draped surface: position(u, v) -> (n, 3) for u, v in [0, 1]; material (a tree.Material whose colour and
    opacity read ctx["uv"] as well as P, N); bones(P, u, v) -> {bone: weights}."""

    def __init__(self, name, position, material, bones, columns, rows):
        self.name, self.position, self.material, self.bones = name, position, material, bones
        self.columns, self.rows = columns, rows


def build(sheet, bone_names, number):
    """A mesh object of sheet: its grid's vertices, quads, a "sheet_uv" attribute (u, v per corner, for its paint),
    and vertex groups weighted by its builder."""
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
    # Its own (u, v) on every vertex, for the paint (a tattered hem, a frayed edge), and which sheet each face is; both
    # are read by the bake and removed before export.
    attribute = mesh.attributes.new("sheet_uv", "FLOAT2", "POINT")
    attribute.data.foreach_set("vector", np.stack([U.ravel(), Vv.ravel()], axis=1).astype(np.float32).ravel())
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


# ---------------------------------------------------------------------------------------------- drapes
def drape(top, bottom_reach, width, depth, flare, folds, fold_depth, sag=0.0, seed=0):
    """A cloak's drape: from a ring of points along top (u -> (n, 3), its shoulder line) it falls bottom_reach (u ->
    cm) straight down, spreading by flare (cm at the hem) and pleated by folds across it fold_depth deep, its pleats
    deepening as it falls. Returns position(u, v)."""
    def position(u, v):
        start = top(u)
        fall = bottom_reach(u) * v
        # Outward from the axis the shoulder line sits around, so the pleats and the flare push away from the body.
        outward = start.copy()
        outward[:, 2] = 0.0
        outward = outward / np.maximum(np.linalg.norm(outward, axis=1, keepdims=True), 1e-6)
        pleat = np.sin(u * folds * np.pi * 2.0 + seed) * fold_depth * (0.25 + 0.75 * v)
        spread = flare * v * v
        p = start + outward * (spread + pleat)[:, None]
        p[:, 2] = start[:, 2] - fall + sag * np.sin(np.pi * u) * v
        return p
    return position

def ribbon(points, normal, width, frame=None, lift=0.15):
    """A hair card's surface along points (head-frame guide, turned into the world by frame: (origin, axes)): its width
    across the lock, tangent to the scalp (square to the lock and to normal), tapering from width at the root to a
    third of it at the tip, lifted off the core by lift (cm). Returns position(u, v)."""
    pts = np.asarray(points, dtype=np.float64)
    n = np.asarray(normal, dtype=np.float64)
    if frame is not None:
        origin, axes = frame
        pts = pts @ np.asarray(axes).T + origin
        n = np.asarray(axes) @ n
    tangents = np.gradient(pts, axis=0)
    tangents /= np.maximum(np.linalg.norm(tangents, axis=1, keepdims=True), 1e-9)
    across = np.cross(tangents, n)
    across /= np.maximum(np.linalg.norm(across, axis=1, keepdims=True), 1e-9)
    up = np.cross(across, tangents)
    samples = np.linspace(0.0, 1.0, len(pts))

    def position(u, v):
        centre = np.stack([np.interp(v, samples, pts[:, k]) for k in range(3)], axis=1)
        side = np.stack([np.interp(v, samples, across[:, k]) for k in range(3)], axis=1)
        out = np.stack([np.interp(v, samples, up[:, k]) for k in range(3)], axis=1)
        w = width * (1.0 - 0.66 * v)
        return centre + side * ((u - 0.5) * w)[:, None] + out * lift
    return position
