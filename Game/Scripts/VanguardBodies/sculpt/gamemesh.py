"""The game mesh made from a sculpt's meshed surface: reduced to a triangle budget (the face, hands and weapon kept
denser), shaded smooth, and cleared of the specks the reduction leaves."""
import bmesh
import bpy
import numpy as np


def reduce(obj, budget, protect=None, protect_strength=0.5):
    """Collapses obj to about budget triangles. protect (n,) in [0, 1] per vertex keeps detail where it is high."""
    mesh = obj.data
    triangles = sum(len(p.vertices) - 2 for p in mesh.polygons)
    ratio = min(1.0, budget / max(triangles, 1))
    mod = obj.modifiers.new("Reduce", "DECIMATE")
    mod.decimate_type = "COLLAPSE"
    mod.ratio = ratio
    mod.use_collapse_triangulate = True
    if protect is not None:
        group = obj.vertex_groups.new(name="_protect")
        # Decimation collapses a weighted vertex less as its weight falls: weight what may go.
        for value in np.unique(np.round(protect, 2)):
            idx = np.nonzero(np.round(protect, 2) == value)[0].tolist()
            group.add(idx, float(1.0 - value * protect_strength), "REPLACE")
        mod.vertex_group = group.name
        mod.vertex_group_factor = 1.0
    bpy.context.view_layer.objects.active = obj
    bpy.ops.object.modifier_apply(modifier=mod.name)
    if protect is not None:
        obj.vertex_groups.remove(obj.vertex_groups["_protect"])
    triangles = sum(len(p.vertices) - 2 for p in obj.data.polygons)
    if triangles > budget:
        # Protection held back more than the ratio allowed for: the rest comes off evenly.
        trim = obj.modifiers.new("Trim", "DECIMATE")
        trim.decimate_type = "COLLAPSE"
        trim.ratio = budget / triangles
        trim.use_collapse_triangulate = True
        bpy.ops.object.modifier_apply(modifier=trim.name)
        triangles = sum(len(p.vertices) - 2 for p in obj.data.polygons)
    return triangles


def shade(obj):
    """Smooth shading throughout: the toon material bands the light, so the low poly's facets do not show."""
    for polygon in obj.data.polygons:
        polygon.use_smooth = True
    obj.data.update()


def drop_specks(obj, smallest=16):
    """Removes loose pieces of fewer than smallest faces: bits of the sculpt (a lock's tip, a prong) the reduction left
    as specks, each a stray fleck that spends triangles."""
    bm = bmesh.new()
    bm.from_mesh(obj.data)
    seen, doomed = set(), []
    for face in bm.faces:
        if face.index in seen:
            continue
        piece, stack = [], [face]
        seen.add(face.index)
        while stack:
            f = stack.pop()
            piece.append(f)
            for edge in f.edges:
                for g in edge.link_faces:
                    if g.index not in seen:
                        seen.add(g.index)
                        stack.append(g)
        if len(piece) < smallest:
            doomed.extend(piece)
    removed = len(doomed)
    bmesh.ops.delete(bm, geom=doomed, context="FACES")
    bm.to_mesh(obj.data)
    bm.free()
    return removed
