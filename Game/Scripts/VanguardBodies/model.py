"""A production model's body (ADR-069): its script's sculpt meshed, reduced, unwrapped, baked and skinned, ready for
the generator to rig, animate and export like any body. Its textures are written beside the FBX."""
import hashlib
import importlib
import time

import bmesh
import bpy
import numpy as np

from .parts import ANCHOR_RADIUS
from .sculpt import bake, gamemesh, mesher, sheet, skin, surface, tree

# The texture each bake writes, whether it is colour (sRGB), and its file suffix.
TEXTURES = (("colour", True, "BaseColor"), ("mask", False, "Mask"), ("normal", False, "Normal"))


def build(spec, layout, dims, name, bones, texture_dir, log=print):
    """The body object of spec's model on layout, skinned to bones (names), with its textures written to texture_dir:
    (object, {kind: path}, triangles)."""
    settings = spec["model"]
    script = importlib.import_module("VanguardBodies.models." + settings["script"])
    sculpt = tree.Sculpt()
    started = time.time()
    stage = lambda what: log("model %s: %s (%.1f s)" % (name, what, time.time() - started))  # noqa: E731
    root, info = script.build(sculpt, {bone: (tuple(a), tuple(b)) for bone, (a, b) in layout.items()}, dims, spec)
    stage("sculpted, %d parts" % len(sculpt.parts))
    grid = mesher.level_set(root, settings["voxelCm"], log=log)
    points, triangles, quads = mesher.polygons(grid)
    obj = surface.to_object(name, points, triangles, quads)
    stage("meshed, %d points" % len(points))
    labels = surface.labels_at(root, points)
    protect = np.array([sculpt.parts[label].protect if label >= 0 else 0.0 for label in labels], dtype=np.float32)
    sheets = info.get("sheets", [])
    # The sculpt's share of the budget; its cloth sheets are built at the density they keep.
    budget = settings["triangleBudget"] - sum(2 * s.columns * s.rows for s in sheets)
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
        count += sum(2 * s.columns * s.rows for s in sheets)
    co = np.zeros(len(obj.data.vertices) * 3, dtype=np.float32)
    obj.data.vertices.foreach_get("co", co)
    near = np.array([sculpt.parts[label].protect if label >= 0 else 0.0 for label in surface.labels_at(root, co.reshape(-1, 3))])
    dense = [polygon.index for polygon in obj.data.polygons if np.mean(near[list(polygon.vertices)]) > 0.5]
    # Charts: each face's material (its centre's part's), split by the way it faces; each sheet whole, numbered past
    # every material and direction.
    centres = np.zeros(len(obj.data.polygons) * 3, dtype=np.float32)
    obj.data.polygons.foreach_get("center", centres)
    material_index = {name: i for i, name in enumerate(sculpt.materials)}
    part_material = np.array([material_index[part.material.name] for part in sculpt.parts] + [len(material_index)], dtype=np.int64)
    groups = part_material[surface.labels_at(root, centres.reshape(-1, 3)).astype(np.int64)]
    charts = gamemesh.charts_of(obj, groups)
    if "sheet" in obj.data.attributes:
        marks = np.zeros(len(obj.data.polygons), dtype=np.int32)
        obj.data.attributes["sheet"].data.foreach_get("value", marks)
        charts = np.where(marks > 0, (len(material_index) + 1) * len(gamemesh.DIRECTIONS) + marks, charts)
    gamemesh.unwrap(obj, charts, dense, dense_scale=settings["denseTexels"])
    stage("unwrapped")
    maps = bake.bake(obj, root, sculpt, settings["textureSize"], sheets=sheets, log=log)
    # What only the bake read goes before export (the content hash covers what the FBX holds).
    for attribute in ("sheet", "sheet_uv"):
        if attribute in obj.data.attributes:
            obj.data.attributes.remove(obj.data.attributes[attribute])
    texture_dir.mkdir(parents=True, exist_ok=True)
    paths = {}
    for kind, srgb, suffix in TEXTURES:
        path = texture_dir / ("T_%s_%s.png" % (name[3:] if name.startswith("SK_") else name, suffix))
        bake.save(maps[kind], path, srgb)
        paths[kind] = path
    _anchor(obj, layout, bones)
    # Its colour is its textures': the vertex colour stays white, and its alpha lets the mask say what glows.
    colour = obj.data.color_attributes.new("Col", "FLOAT_COLOR", "CORNER")
    colour.data.foreach_set("color", np.ones(len(obj.data.loops) * 4, dtype=np.float32))
    obj.data.color_attributes.active_color = colour
    obj.data.uv_layers.active.name = "UVMap"
    return obj, paths, count


def _anchor(obj, layout, bones):
    """A speck at the head of each bone no vertex is weighted to (ADR-064's anchors), so every bone binds at rest."""
    weighted = {obj.vertex_groups[g.group].name for v in obj.data.vertices for g in v.groups if g.weight > 0}
    bm = bmesh.new()
    bm.from_mesh(obj.data)
    deform = bm.verts.layers.deform.verify()
    for bone in bones:
        if bone in weighted:
            continue
        head = layout[bone][0]
        made = bmesh.ops.create_icosphere(bm, subdivisions=1, radius=ANCHOR_RADIUS)
        group = obj.vertex_groups[bone].index
        for vert in made["verts"]:
            vert.co = vert.co + type(vert.co)(head)
            vert[deform][group] = 1.0
    bm.to_mesh(obj.data)
    bm.free()


def texture_records(paths, source):
    """The manifest's record of a body's textures: each file relative to source and its SHA-256."""
    return {kind: {"file": path.relative_to(source).as_posix(), "sha256": hashlib.sha256(path.read_bytes()).hexdigest()}
            for kind, path in paths.items()}
