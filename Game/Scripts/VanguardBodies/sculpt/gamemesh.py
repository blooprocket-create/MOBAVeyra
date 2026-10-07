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
    """Smooth shading throughout: the baked normal map carries the hard edges of buckles, plates and soles."""
    for polygon in obj.data.polygons:
        polygon.use_smooth = True
    obj.data.update()

# The directions a chart faces, for cutting a part into charts that unfold flat.
DIRECTIONS = np.array([[1, 0, 0], [-1, 0, 0], [0, 1, 0], [0, -1, 0], [0, 0, 1], [0, 0, -1]], dtype=np.float64)


def charts_of(obj, groups, smoothing=6):
    """A chart for every face: its group (a sculpt part's label, or a sheet's own number) split by which of six
    directions its normal, smoothed over its neighbours, faces most. A chart unfolds flat with little stretch, and its
    borders are where the model's own structure changes."""
    mesh = obj.data
    normals = np.zeros(len(mesh.polygons) * 3, dtype=np.float64)
    mesh.polygons.foreach_get("normal", normals)
    normals = normals.reshape(-1, 3)
    # Faces sharing an edge, for smoothing the normals so the charts' borders do not fray.
    bm = bmesh.new()
    bm.from_mesh(mesh)
    pairs = np.array([[e.link_faces[0].index, e.link_faces[1].index] for e in bm.edges if len(e.link_faces) == 2], dtype=np.int64).reshape(-1, 2)
    bm.free()
    for _ in range(smoothing):
        total = normals.copy()
        np.add.at(total, pairs[:, 0], normals[pairs[:, 1]])
        np.add.at(total, pairs[:, 1], normals[pairs[:, 0]])
        normals = total / np.maximum(np.linalg.norm(total, axis=1, keepdims=True), 1e-9)
    direction = np.argmax(normals @ DIRECTIONS.T, axis=1)
    charts = np.asarray(groups, dtype=np.int64) * len(DIRECTIONS) + direction
    return _merge_fragments(charts, pairs, len(mesh.polygons))


def _merge_fragments(charts, pairs, count, smallest=24, passes=6):
    """Charts with every connected run of faces under smallest faces given to the chart most of their neighbours are
    in, so a chart is one solid region rather than a scatter that unfolds into many islands."""
    charts = charts.copy()
    neighbours = [[] for _ in range(count)]
    for a, b in pairs:
        neighbours[a].append(b)
        neighbours[b].append(a)
    for _ in range(passes):
        seen = np.zeros(count, dtype=bool)
        changed = False
        for start in range(count):
            if seen[start]:
                continue
            region, stack = [], [start]
            seen[start] = True
            while stack:
                face = stack.pop()
                region.append(face)
                for other in neighbours[face]:
                    if not seen[other] and charts[other] == charts[start]:
                        seen[other] = True
                        stack.append(other)
            if len(region) >= smallest:
                continue
            around = [charts[o] for f in region for o in neighbours[f] if charts[o] != charts[start]]
            if around:
                values, counts = np.unique(around, return_counts=True)
                charts[region] = values[np.argmax(counts)]
                changed = True
        if not changed:
            break
    return charts


def drop_specks(obj, smallest=16):
    """Removes loose pieces of fewer than smallest faces: bits of the sculpt (a lock's tip, a prong) the reduction left
    as specks, each of which would be an island of its own."""
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


def unwrap(obj, charts, dense_faces=None, dense_scale=2.0):
    """Unwraps the UVs chart by chart (charts: an id per face; seams where they differ), sets every island to one texel
    density (islands holding dense_faces, the face and hands seen close, dense_scale times it) and packs them."""
    bm = bmesh.new()
    bm.from_mesh(obj.data)
    bm.faces.ensure_lookup_table()
    for edge in bm.edges:
        faces = edge.link_faces
        edge.seam = bool(len(faces) != 2 or charts[faces[0].index] != charts[faces[1].index])
    bm.to_mesh(obj.data)
    bm.free()
    bpy.context.view_layer.objects.active = obj
    bpy.ops.object.mode_set(mode="EDIT")
    bpy.ops.mesh.select_all(action="SELECT")
    bpy.ops.uv.unwrap(method="ANGLE_BASED", fill_holes=True, correct_aspect=True, margin=ISLAND_MARGIN)
    # Where an awkward chart (a buckle's ring, a strap round a limb) folded over itself, its faces are projected
    # instead, so no two faces share a texel.
    bpy.context.scene.tool_settings.use_uv_select_sync = False
    bpy.ops.mesh.select_all(action="SELECT")
    bpy.ops.uv.select_all(action="DESELECT")
    bpy.ops.uv.select_overlap()
    bpy.ops.object.mode_set(mode="OBJECT")
    # Blender 5 keeps the UV selection in its own face attribute.
    marks = obj.data.attributes.get(".uv_select_face")
    folded = []
    if marks is not None:
        selected = np.zeros(len(obj.data.polygons), dtype=bool)
        marks.data.foreach_get("value", selected)
        folded = np.nonzero(selected)[0].tolist()
    print("unwrap: %d of %d faces folded over others" % (len(folded), len(obj.data.polygons)))
    # Only a fold is re-projected: a selection that took most faces would shatter the charts into a projection's islands.
    if folded and len(folded) < 0.25 * len(obj.data.polygons):
        bpy.ops.object.mode_set(mode="EDIT")
        bpy.ops.mesh.select_all(action="DESELECT")
        bpy.ops.object.mode_set(mode="OBJECT")
        for index in folded:
            obj.data.polygons[index].select = True
        bpy.ops.object.mode_set(mode="EDIT")
        bpy.ops.uv.smart_project(angle_limit=math.radians(50.0), island_margin=ISLAND_MARGIN, area_weight=0.0, correct_aspect=True,
                                 scale_to_bounds=False)
    bpy.ops.object.mode_set(mode="OBJECT")
    # Seams were only for the unwrap: the content hash does not cover them.
    for edge in obj.data.edges:
        edge.use_seam = False
    if ".uv_seam" in obj.data.attributes or "uv_seam" in obj.data.attributes:
        name = "uv_seam" if "uv_seam" in obj.data.attributes else ".uv_seam"
        obj.data.attributes.remove(obj.data.attributes[name])
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