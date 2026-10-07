"""A production model's body (ADR-069): its script's sculpt meshed, reduced, skinned and flat-coloured, ready for the
generator to rig, animate and export like any body."""
import importlib
import time

import bmesh
import bpy
import numpy as np

from .parts import ANCHOR_RADIUS
from .sculpt import gamemesh, mesher, paint, sheet, skin, surface, tree, workers


def build(spec, layout, dims, name, bones, log=print):
    """The body object of spec's model on layout, skinned to bones (names): (object, triangles)."""
    settings = spec["model"]
    shaped = {bone: (tuple(a), tuple(b)) for bone, (a, b) in layout.items()}
    # Its sculpt is evaluated in worker processes, each building it as the generator does (sculpt/workers.py).
    with workers.Pool({"script": settings["script"], "layout": shaped, "dims": dims, "spec": spec}) as pool:
        return _build(spec, shaped, layout, dims, name, bones, log, pool)


def _build(spec, shaped, layout, dims, name, bones, log, pool):
    settings = spec["model"]
    script = importlib.import_module("VanguardBodies.models." + settings["script"])
    sculpt = tree.Sculpt()
    started = time.time()
    stage = lambda what: log("model %s: %s (%.1f s)" % (name, what, time.time() - started))  # noqa: E731
    # Built at the default reach, as its workers build it: whatever an earlier body in this process was meshed or
    # labelled at never reaches what its script probes as it builds.
    with tree.reaching(tree.DEFAULT_REACH):
        root, info = script.build(sculpt, shaped, dims, spec)
    pool.adopt("root", root)
    pool.adopt("body", info["body"])
    stage("sculpted, %d parts" % len(sculpt.parts))
    grid = mesher.level_set(root, settings["voxelCm"], log=log)
    points, triangles, quads = mesher.polygons(grid)
    obj = surface.to_object(name, points, triangles, quads)
    stage("meshed, %d points" % len(points))
    # Labelled, skinned and coloured within the labels' band, each part's weights and colour read where it is found.
    with tree.reaching(surface.LABEL_REACH):
        return _finish(settings, layout, bones, sculpt, root, info, obj, points, stage)


def _finish(settings, layout, bones, sculpt, root, info, obj, points, stage):
    """The meshed body reduced to its budget, skinned, joined with its cloth and flat-coloured: (object, triangles)."""
    labels = surface.labels_at(root, points)
    protect = np.array([sculpt.parts[label].protect if label >= 0 else 0.0 for label in labels], dtype=np.float32)
    sheets = info.get("sheets", [])
    # The sculpt's share of the budget: its cloth sheets are built at the density they keep, and each bone may need an
    # anchor's one triangle.
    budget = settings["triangleBudget"] - sum(2 * s.columns * s.rows for s in sheets) - len(bones)
    count = gamemesh.reduce(obj, budget, protect)
    count -= gamemesh.drop_specks(obj)
    gamemesh.shade(obj)
    stage("reduced to %d triangles" % count)
    weights = skin.weights(obj, root, info["body"], sculpt, bones)
    skin.assign(obj, weights, bones)
    stage("skinned")
    if sheets:
        # Cloth joins skinned by its builder, after the sculpt's own weights.
        cloth = [sheet.build(s, bones, number) for number, s in enumerate(sheets, start=1)]
        bpy.ops.object.select_all(action="DESELECT")
        for piece in cloth:
            piece.select_set(True)
        obj.select_set(True)
        bpy.context.view_layer.objects.active = obj
        bpy.ops.object.join()
    # Flat colour (ADR-069 §2): each face its material's, read by the toon material through the vertex colour.
    faces = _flat_colours(obj, root, sculpt, sheets)
    if "sheet" in obj.data.attributes:
        obj.data.attributes.remove(obj.data.attributes["sheet"])
    _project_uvs(obj)
    _anchor(obj, layout, bones)
    # Counted as exported, anchors and all, so the budget's check and the manifest see every triangle.
    obj.data.calc_loop_triangles()
    count = len(obj.data.loop_triangles)
    _set_colours(obj, faces)
    stage("coloured")
    return obj, count


def _flat_colours(obj, root, sculpt, sheets):
    """Each face's colour (linear) and glow (alpha 1 where it glows): its sheet's material, or the material of the part
    nearest its centre. (faces, 4) float32."""
    mesh = obj.data
    centres = np.zeros(len(mesh.polygons) * 3, dtype=np.float32)
    mesh.polygons.foreach_get("center", centres)
    labels = surface.labels_at(root, centres.reshape(-1, 3)).astype(np.int64)
    marks = np.zeros(len(mesh.polygons), dtype=np.int32)
    if "sheet" in mesh.attributes:
        mesh.attributes["sheet"].data.foreach_get("value", marks)
    # One row per part and then per sheet; a face nearest no part (none should be) stays white.
    def row(material):
        return list(paint.linear(material.colour)) + [1.0 if material.glow else 0.0]
    table = np.array([row(part.material) for part in sculpt.parts] + [row(s.material) for s in sheets] + [[1.0, 1.0, 1.0, 0.0]], dtype=np.float32)
    index = np.where(marks > 0, len(sculpt.parts) + marks - 1, np.where(labels >= 0, labels, len(table) - 1))
    return table[index]


def _set_colours(obj, faces):
    """The vertex colour "Col" at every corner: its face's (faces made after them, the anchors, white)."""
    mesh = obj.data
    totals = np.zeros(len(mesh.polygons), dtype=np.int32)
    mesh.polygons.foreach_get("loop_total", totals)
    per_face = np.vstack([faces, np.tile([1.0, 1.0, 1.0, 0.0], (len(mesh.polygons) - len(faces), 1))]).astype(np.float32)
    colour = mesh.color_attributes.new("Col", "FLOAT_COLOR", "CORNER")
    colour.data.foreach_set("color", np.repeat(per_face, totals, axis=0).ravel())
    mesh.color_attributes.active_color = colour


def _project_uvs(obj):
    """A UV map projected from the side (nothing samples it: a flat-coloured body reads only its vertex colour), so the
    import has one to compute tangents from."""
    mesh = obj.data
    co = np.zeros(len(mesh.vertices) * 3, dtype=np.float32)
    mesh.vertices.foreach_get("co", co)
    co = co.reshape(-1, 3)
    loops = np.zeros(len(mesh.loops), dtype=np.int64)
    mesh.loops.foreach_get("vertex_index", loops)
    span = max(float(np.ptp(co[:, 1])), float(np.ptp(co[:, 2])), 1e-6)
    uv = np.stack([(co[:, 0] - co[:, 0].min()) / span, (co[:, 2] - co[:, 2].min()) / span], axis=1)[loops]
    layer = mesh.uv_layers.new(name="UVMap") if not mesh.uv_layers else mesh.uv_layers.active
    layer.data.foreach_set("uv", uv.astype(np.float32).ravel())


def _anchor(obj, layout, bones):
    """A speck at the head of each bone no vertex is weighted to (ADR-064's anchors), so every bone binds at rest: one
    tiny triangle, as a low-poly body counts every one."""
    weighted = {obj.vertex_groups[g.group].name for v in obj.data.vertices for g in v.groups if g.weight > 0}
    bm = bmesh.new()
    bm.from_mesh(obj.data)
    deform = bm.verts.layers.deform.verify()
    for bone in bones:
        if bone in weighted:
            continue
        head = layout[bone][0]
        group = obj.vertex_groups[bone].index
        corners = []
        for offset in ((ANCHOR_RADIUS, 0.0, 0.0), (0.0, ANCHOR_RADIUS, 0.0), (0.0, 0.0, ANCHOR_RADIUS)):
            vert = bm.verts.new([h + o for h, o in zip(head, offset)])
            vert[deform][group] = 1.0
            corners.append(vert)
        bm.faces.new(corners)
    bm.to_mesh(obj.data)
    bm.free()
