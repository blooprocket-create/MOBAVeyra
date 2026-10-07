"""The game mesh made from a sculpt's meshed surface: reduced to a triangle budget (the face and hands kept denser),
shaded smooth, and unwrapped, the face's and hands' islands given more of the texture."""
import math

import bmesh
import bpy
import numpy as np

# The UV islands' margin, as a share of the texture (room for the bake's dilation and mip levels).
ISLAND_MARGIN = 0.004


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
    return sum(len(p.vertices) - 2 for p in obj.data.polygons)


def shade(obj):
    """Smooth shading throughout: the baked normal map carries the hard edges of buckles, plates and soles."""
    for polygon in obj.data.polygons:
        polygon.use_smooth = True
    obj.data.update()

def unwrap(obj, dense_faces=None, dense_scale=2.0, angle=60.0):
    """Smart-projects the UVs, sets every island to one texel density (islands holding dense_faces, the face and hands
    seen close, dense_scale times it) and packs them."""
    bpy.context.view_layer.objects.active = obj
    bpy.ops.object.mode_set(mode="EDIT")
    bpy.ops.mesh.select_all(action="SELECT")
    bpy.ops.uv.smart_project(angle_limit=math.radians(angle), island_margin=ISLAND_MARGIN, area_weight=0.0, correct_aspect=True,
                             scale_to_bounds=False)
    bpy.ops.object.mode_set(mode="OBJECT")
    dense = set(int(f) for f in (dense_faces if dense_faces is not None else []))
    bm = bmesh.new()
    bm.from_mesh(obj.data)
    layer = bm.loops.layers.uv.active
    bm.faces.ensure_lookup_table()
    for island in _islands(bm, layer):
        area3d = sum(f.calc_area() for f in island)
        uvs = [l[layer].uv for f in island for l in f.loops]
        area_uv = sum(_uv_area(f, layer) for f in island)
        if area3d <= 0 or area_uv <= 0:
            continue
        scale = math.sqrt(area3d / area_uv) * (dense_scale if any(f.index in dense for f in island) else 1.0)
        cx = sum(u.x for u in uvs) / len(uvs)
        cy = sum(u.y for u in uvs) / len(uvs)
        for f in island:
            for l in f.loops:
                uv = l[layer].uv
                uv.x = (uv.x - cx) * scale + cx
                uv.y = (uv.y - cy) * scale + cy
    bm.to_mesh(obj.data)
    bm.free()
    bpy.ops.object.mode_set(mode="EDIT")
    bpy.ops.mesh.select_all(action="SELECT")
    bpy.ops.uv.select_all(action="SELECT")
    bpy.ops.uv.pack_islands(rotate=True, margin=ISLAND_MARGIN, scale=True)
    bpy.ops.object.mode_set(mode="OBJECT")


def _uv_area(face, layer):
    pts = [l[layer].uv for l in face.loops]
    area = 0.0
    for i in range(1, len(pts) - 1):
        a, b, c = pts[0], pts[i], pts[i + 1]
        area += abs((b.x - a.x) * (c.y - a.y) - (c.x - a.x) * (b.y - a.y)) * 0.5
    return area


def _islands(bm, layer):
    """Faces grouped into UV islands: neighbours across an edge whose UVs agree on both its ends."""
    seen = set()
    for start in bm.faces:
        if start.index in seen:
            continue
        island, stack = [], [start]
        seen.add(start.index)
        while stack:
            face = stack.pop()
            island.append(face)
            for loop in face.loops:
                other = loop.link_loop_radial_next
                if other == loop or other.face.index in seen:
                    continue
                # The same edge seen from the other face: its loops run the other way.
                a0, a1 = loop[layer].uv, loop.link_loop_next[layer].uv
                b0, b1 = other.link_loop_next[layer].uv, other[layer].uv
                if (a0 - b0).length < 1e-5 and (a1 - b1).length < 1e-5:
                    seen.add(other.face.index)
                    stack.append(other.face)
        yield island