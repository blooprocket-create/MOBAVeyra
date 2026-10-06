"""The Crucible's environment kit (ADR-040 C6): deterministic, project-owned meshes for an overgrown highland ruin.

Run in a background Blender 5.2 (BuildEnvironmentArt.ps1). Every family in Game/ArtSource/Environment/CrucibleKit.json
is generated from its seed and parameters alone: slate cliffs and boulders cut into facets and weathered, fluted pillars
broken off, masonry blocks, glyph steles, trees, shrubs, ferns, reeds and grass. Geometry and material slots only: no
map coordinates, no collision, no gameplay. The manifest hashes each mesh's normalised geometry and content as well as
its file, since FBX containers carry metadata that changes without the mesh; an FBX whose content comes out unchanged
is kept as it was (fbx_content.py), so a run that changes no mesh rewrites no FBX.

Usage: blender --background --factory-startup --python GenerateEnvironmentMeshes.py -- [--only Cliff,Tree] [--preview]
"""
import hashlib
import json
import math
import random
import sys
from pathlib import Path

import bmesh
import bpy
from mathutils import Matrix, Vector, noise

GAME = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(GAME / "Scripts"))
from EnvironmentKit.inputs import mesh_profile_sha256  # noqa: E402
from fbx_content import content_sha256, export_keeping_unchanged  # noqa: E402

SOURCE = GAME / "ArtSource" / "Environment"
PROFILE = SOURCE / "CrucibleKit.json"
KIT = json.loads(PROFILE.read_text(encoding="utf-8"))
ARGS = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
ONLY = set(ARGS[ARGS.index("--only") + 1].split(",")) if "--only" in ARGS else set()
PREVIEW = "--preview" in ARGS
if not bpy.app.background:
    raise RuntimeError("Use an isolated background Blender process.")
if bpy.app.version[:2] != (5, 2):
    raise RuntimeError("This source generator is validated against Blender 5.2.")

bpy.ops.wm.read_factory_settings(use_empty=True)
SCENE = bpy.context.scene
SCENE.unit_settings.system = "METRIC"
SCENE.unit_settings.scale_length = 1.0
(SOURCE / "FBX").mkdir(parents=True, exist_ok=True)

MATERIALS = {}
for slot, colour in KIT["preview"].items():
    material = bpy.data.materials.new(slot.title())
    material.diffuse_color = (*colour, 1.0)
    MATERIALS[slot] = material


# Geometry helpers. Everything is built in bmesh, in metres, pivot at the base's centre.

def to_object(name, bm, slots):
    mesh = bpy.data.meshes.new(name)
    bm.to_mesh(mesh)
    bm.free()
    obj = bpy.data.objects.new(name, mesh)
    SCENE.collection.objects.link(obj)
    for slot in slots:
        mesh.materials.append(MATERIALS[slot])
    return obj


def evaluated(obj):
    """Applies obj's modifiers into its mesh, without operators or context."""
    depsgraph = bpy.context.evaluated_depsgraph_get()
    mesh = bpy.data.meshes.new_from_object(obj.evaluated_get(depsgraph))
    old = obj.data
    obj.modifiers.clear()
    obj.data = mesh
    bpy.data.meshes.remove(old)


def remesh(obj, voxel):
    modifier = obj.modifiers.new("Remesh", "REMESH")
    modifier.mode = "VOXEL"
    modifier.voxel_size = voxel
    evaluated(obj)


def decimate(obj, triangles):
    current = sum(len(p.vertices) - 2 for p in obj.data.polygons)
    if current > triangles:
        modifier = obj.modifiers.new("Decimate", "DECIMATE")
        modifier.ratio = triangles / current
        evaluated(obj)


def cut(bm, point, normal):
    """Cuts away everything on the side Normal points to, and closes the cut."""
    geometry = bm.verts[:] + bm.edges[:] + bm.faces[:]
    bmesh.ops.bisect_plane(bm, geom=geometry, plane_co=point, plane_no=normal, clear_outer=True)
    boundary = [edge for edge in bm.edges if edge.is_boundary]
    if boundary:
        bmesh.ops.holes_fill(bm, edges=boundary, sides=0)


def fractal(position, scale, octaves=4):
    return noise.fractal(position * scale, 0.6, 2.0, octaves, noise_basis="PERLIN_ORIGINAL")


def displace(obj, field):
    """Moves every vertex along its normal by field(position, normal)."""
    bm = bmesh.new()
    bm.from_mesh(obj.data)
    bm.normal_update()
    offsets = [field(v.co.copy(), v.normal.copy()) for v in bm.verts]
    for vertex, offset in zip(bm.verts, offsets):
        vertex.co += vertex.normal * offset
    bm.to_mesh(obj.data)
    bm.free()


def ground_pivot(obj, sink=0.0):
    """The pivot at the base's centre: the lowest point sits Sink below the ground."""
    vertices = obj.data.vertices
    centre_x = (max(v.co.x for v in vertices) + min(v.co.x for v in vertices)) / 2
    centre_y = (max(v.co.y for v in vertices) + min(v.co.y for v in vertices)) / 2
    bottom = min(v.co.z for v in vertices)
    for vertex in vertices:
        vertex.co -= Vector((centre_x, centre_y, bottom + sink))


def colour_layer(obj, colour_of):
    """Each corner's vertex colour from colour_of(position, polygon index)."""
    mesh = obj.data
    layer = mesh.color_attributes.new("Col", "BYTE_COLOR", "CORNER")
    for polygon in mesh.polygons:
        for loop_index in polygon.loop_indices:
            position = mesh.vertices[mesh.loops[loop_index].vertex_index].co
            layer.data[loop_index].color = colour_of(position, polygon.index)


def box_uvs(obj, metres):
    """World-scale box-projected UVs: each face along its dominant axis, Metres to a tile."""
    mesh = obj.data
    layer = mesh.uv_layers.new(name="UV0")
    for polygon in mesh.polygons:
        axis = max(range(3), key=lambda a: abs(polygon.normal[a]))
        for loop_index in polygon.loop_indices:
            co = mesh.vertices[mesh.loops[loop_index].vertex_index].co
            u, v = [(co.y, co.z), (co.x, co.z), (co.x, co.y)][axis]
            layer.data[loop_index].uv = (u / metres, v / metres)


def shade(obj, sharp_degrees):
    """Smooth shading, with edges sharper than Sharp Degrees kept hard."""
    mesh = obj.data
    for polygon in mesh.polygons:
        polygon.use_smooth = True
    bm = bmesh.new()
    bm.from_mesh(mesh)
    limit = math.radians(sharp_degrees)
    for edge in bm.edges:
        if edge.is_manifold and edge.calc_face_angle(0.0) > limit:
            edge.smooth = False
    bm.to_mesh(mesh)
    bm.free()


def tube(bm, points, radii, sides, wobble, rng, flare=None):
    """A closed tube through Points with Radii, Sides round, its rings jittered by Wobble; Flare widens its base."""
    rings = []
    for index, (point, radius) in enumerate(zip(points, radii)):
        ahead = points[min(index + 1, len(points) - 1)] - points[max(index - 1, 0)]
        ahead.normalize()
        side = ahead.orthogonal().normalized()
        other = ahead.cross(side)
        ring = []
        for k in range(sides):
            angle = math.tau * k / sides
            r = radius * (1 + rng.uniform(-wobble, wobble))
            if flare and index == 0:
                r *= flare(angle)
            ring.append(bm.verts.new(point + (side * math.cos(angle) + other * math.sin(angle)) * r))
        rings.append(ring)
    for a, b in zip(rings, rings[1:]):
        for k in range(sides):
            bm.faces.new((a[k], a[(k + 1) % sides], b[(k + 1) % sides], b[k]))
    bm.faces.new(list(reversed(rings[0])))
    bm.faces.new(rings[-1])


# Families.

def rock(spec, rng):
    sx, sy, sz = spec["sizeMetres"]
    scale = Vector((sx, sy, sz)) * rng.uniform(0.85, 1.15)
    bm = bmesh.new()
    blocky = spec["base"] == "block"
    if blocky:
        # A block of slate: steep sides, a broad top for moss.
        bmesh.ops.create_cube(bm, size=1.0)
    else:
        # A river stone, rounded by the water.
        bmesh.ops.create_icosphere(bm, subdivisions=3, radius=0.5)
    for vertex in bm.verts:
        vertex.co = Vector((vertex.co.x * scale.x, vertex.co.y * scale.y, vertex.co.z * scale.z))
    # Facets: plane cuts from every side, mostly near-vertical, so the rock reads as split stone.
    for _ in range(spec["facets"]):
        angle = rng.uniform(0, math.tau)
        tilt = rng.uniform(-0.25, 0.35) if blocky else rng.uniform(-0.35, 0.6)
        normal = Vector((math.cos(angle), math.sin(angle), tilt)).normalized()
        reach = 0.5 * abs(normal.x * scale.x) + 0.5 * abs(normal.y * scale.y) + 0.5 * abs(normal.z * scale.z)
        cut(bm, normal * reach * (rng.uniform(0.7, 0.92) if blocky else rng.uniform(0.62, 0.85)), normal)
    if blocky:
        # A broken top, tilted a little, and a corner or two shorn off it.
        top = Vector((rng.uniform(-0.18, 0.18), rng.uniform(-0.18, 0.18), 1)).normalized()
        cut(bm, Vector((0, 0, scale.z * rng.uniform(0.36, 0.44))), top)
        for _ in range(2):
            angle = rng.uniform(0, math.tau)
            corner = Vector((math.cos(angle), math.sin(angle), 1.1)).normalized()
            cut(bm, Vector((math.cos(angle) * scale.x * 0.42, math.sin(angle) * scale.y * 0.42, scale.z * 0.3)), corner)
    cut(bm, Vector((0, 0, -scale.z * (0.45 if blocky else 0.32))), Vector((0, 0, -1)))
    obj = to_object("rock", bm, ["rock"])
    remesh(obj, spec["voxelMetres"])
    offset = Vector((rng.uniform(-100, 100), rng.uniform(-100, 100), rng.uniform(-100, 100)))
    strata, per_metre, rough = spec["strata"], spec["strataPerMetre"], spec["roughness"]

    def weathering(co, normal):
        # Terraced ledges on the sides: within each layer the face steps back as it rises, and breaks off at the next.
        side = max(0.0, 1.0 - abs(normal.z) * 1.4)
        layer = co.z * per_metre + 0.35 * fractal(co + offset, 0.5, 2)
        within = layer - math.floor(layer)
        ledges = -strata * side * within ** 1.5
        # Broad lumps and chips, so no two faces are flat alike, and fine grain.
        return ledges + rough * fractal(co + offset, 0.9, 3) + rough * 0.3 * fractal(co + offset, 5.0, 2)

    displace(obj, weathering)
    decimate(obj, spec["triangles"])
    ground_pivot(obj, sink=sz * 0.08)
    shade(obj, 50)
    return obj


def pillar(spec, rng):
    radius = spec["radiusMetres"] * rng.uniform(0.9, 1.1)
    height = rng.uniform(*spec["heightMetres"])
    flutes, depth = spec["flutes"], spec["fluteDepth"]
    sides, ring_step = 64, 0.08
    bm = bmesh.new()
    rings = []
    count = max(2, int(height / ring_step))
    for index in range(count + 1):
        z = height * index / count
        taper = 1.0 - 0.08 * z / height
        ring = []
        for k in range(sides):
            angle = math.tau * k / sides
            flute = depth * max(0.0, math.cos(flutes * angle)) ** 0.6
            r = radius * taper - flute
            ring.append(bm.verts.new((math.cos(angle) * r, math.sin(angle) * r, z + 0.32)))
        rings.append(ring)
    for a, b in zip(rings, rings[1:]):
        for k in range(sides):
            bm.faces.new((a[k], a[(k + 1) % sides], b[(k + 1) % sides], b[k]))
    bm.faces.new(list(reversed(rings[0])))
    bm.faces.new(rings[-1])
    # A plinth under it.
    plinth = bmesh.ops.create_cube(bm, size=1.0)["verts"]
    for vertex in plinth:
        vertex.co = Vector((vertex.co.x * radius * 2.6, vertex.co.y * radius * 2.6, (vertex.co.z + 0.5) * 0.34))
    # Broken off: a tilted cut near the top, then its face made jagged.
    tilt = Vector((rng.uniform(-0.5, 0.5), rng.uniform(-0.5, 0.5), 1)).normalized()
    cut(bm, Vector((0, 0, 0.32 + height * rng.uniform(0.82, 0.95))), tilt)
    for _ in range(2):
        angle = rng.uniform(0, math.tau)
        normal = Vector((math.cos(angle), math.sin(angle), rng.uniform(-0.2, 0.4))).normalized()
        cut(bm, normal * radius * 1.25 + Vector((0, 0, rng.uniform(0.4, height))), normal)
    obj = to_object("pillar", bm, ["ruin"])
    offset = Vector((rng.uniform(-100, 100), rng.uniform(-100, 100), 0))
    top = max(v.co.z for v in obj.data.vertices)

    def jagged(co, normal):
        broken = max(0.0, 1.0 - (top - co.z) / 0.25)
        return 0.06 * broken * fractal(co + offset, 5.0) + 0.006 * fractal(co + offset, 12.0, 2)

    displace(obj, jagged)
    decimate(obj, spec["triangles"])
    ground_pivot(obj, sink=0.05)
    shade(obj, 40)
    return obj


def block(spec, rng):
    sx, sy, sz = (s * rng.uniform(0.8, 1.2) for s in spec["sizeMetres"])
    bm = bmesh.new()
    verts = bmesh.ops.create_cube(bm, size=1.0)["verts"]
    for vertex in verts:
        vertex.co = Vector((vertex.co.x * sx, vertex.co.y * sy, vertex.co.z * sz))
    for _ in range(3):
        corner = Vector((rng.choice((-1, 1)) * sx / 2, rng.choice((-1, 1)) * sy / 2, rng.choice((-1, 1)) * sz / 2))
        normal = corner.normalized()
        cut(bm, corner * rng.uniform(0.72, 0.88), normal)
    obj = to_object("block", bm, ["ruin"])
    remesh(obj, spec["voxelMetres"])
    offset = Vector((rng.uniform(-100, 100), rng.uniform(-100, 100), rng.uniform(-100, 100)))
    displace(obj, lambda co, n: spec["roughness"] * fractal(co + offset, 3.0) + spec["roughness"] * 0.5 * fractal(co + offset, 9.0, 2))
    decimate(obj, spec["triangles"])
    ground_pivot(obj, sink=0.04)
    shade(obj, 45)
    return obj


def stele(spec, rng):
    sx, sy, sz = (s * rng.uniform(0.9, 1.1) for s in spec["sizeMetres"])
    bm = bmesh.new()
    verts = bmesh.ops.create_cube(bm, size=1.0)["verts"]
    for vertex in verts:
        vertex.co = Vector((vertex.co.x * sx, vertex.co.y * sy, (vertex.co.z + 0.5) * sz))
    # A shaped head: two shoulders cut away.
    for side in (-1, 1):
        normal = Vector((side, 0, 0.9)).normalized()
        cut(bm, Vector((side * sx * 0.32, 0, sz * 0.9)), normal)
    obj = to_object("stele", bm, ["ruin"])
    remesh(obj, 0.03)
    offset = Vector((rng.uniform(-100, 100), rng.uniform(-100, 100), 0))
    displace(obj, lambda co, n: 0.012 * fractal(co + offset, 3.0) + 0.006 * fractal(co + offset, 10.0, 2))
    decimate(obj, spec["triangles"] - 600)
    # Glyphs: raised strokes between the points of a small grid on the face, on their own material slot.
    glyph = bmesh.new()
    columns, rows = 3, 5
    left, right, low, high = -sx * 0.3, sx * 0.3, sz * 0.25, sz * 0.78
    nodes = [(left + (right - left) * c / (columns - 1), low + (high - low) * r / (rows - 1)) for r in range(rows) for c in range(columns)]
    strokes = set()
    while len(strokes) < rng.randint(*spec["glyphStrokes"]):
        a = rng.randrange(len(nodes))
        neighbours = [b for b in range(len(nodes)) if 0 < abs(nodes[a][0] - nodes[b][0]) + abs(nodes[a][1] - nodes[b][1]) <= max((right - left) / (columns - 1), (high - low) / (rows - 1)) * 1.5]
        strokes.add(tuple(sorted((a, rng.choice(neighbours)))))
    face_y = -sy / 2 - 0.004
    for a, b in strokes:
        (ax, az), (bx, bz) = nodes[a], nodes[b]
        along = Vector((bx - ax, 0, bz - az))
        length = along.length
        along.normalize()
        across = Vector((-along.z, 0, along.x)) * 0.028
        start = Vector((ax, face_y, az)) - along * 0.028
        end = Vector((bx, face_y, bz)) + along * 0.028
        quad = [glyph.verts.new(p) for p in (start - across, end - across, end + across, start + across)]
        lifted = [glyph.verts.new(v.co + Vector((0, -0.012, 0))) for v in quad]
        glyph.faces.new(list(reversed(lifted)))
        for k in range(4):
            glyph.faces.new((quad[k], quad[(k + 1) % 4], lifted[(k + 1) % 4], lifted[k]))
    mesh = obj.data
    mesh.materials.append(MATERIALS["glyph"])
    glyph_obj = to_object("glyph", glyph, ["glyph"])
    for polygon in glyph_obj.data.polygons:
        polygon.material_index = 0
    join(obj, [glyph_obj])
    ground_pivot(obj, sink=0.12)
    shade(obj, 45)
    return obj


def join(target, others):
    """Joins Others into Target, keeping each part's material by name."""
    bm = bmesh.new()
    bm.from_mesh(target.data)
    for other in others:
        index = {}
        for slot, material in enumerate(other.data.materials):
            if material.name not in [m.name for m in target.data.materials]:
                target.data.materials.append(material)
            index[slot] = [m.name for m in target.data.materials].index(material.name)
        part = bmesh.new()
        part.from_mesh(other.data)
        for face in part.faces:
            face.material_index = index.get(face.material_index, 0)
        temp = bpy.data.meshes.new("temp")
        part.to_mesh(temp)
        part.free()
        bm.from_mesh(temp)
        bpy.data.meshes.remove(temp)
        bpy.data.objects.remove(other, do_unlink=True)
    bm.to_mesh(target.data)
    bm.free()


def lumpy_mass(bm, centre, radii, rng, offset, lumps):
    """A foliage mass: a squashed icosphere pushed out in lumps."""
    made = bmesh.ops.create_icosphere(bm, subdivisions=2, radius=1.0)["verts"]
    for vertex in made:
        direction = vertex.co.normalized()
        bump = 1.0 + lumps * fractal(direction * 1.7 + offset, 1.0, 3)
        vertex.co = centre + Vector((direction.x * radii.x, direction.y * radii.y, direction.z * radii.z)) * bump
    return made


def tree(spec, rng):
    height = rng.uniform(*spec["heightMetres"])
    canopy = rng.uniform(*spec["canopyRadiusMetres"])
    trunk_top = height * rng.uniform(0.45, 0.55)
    lean = Vector((rng.uniform(-0.25, 0.25), rng.uniform(-0.25, 0.25), 0))
    points = [Vector((0, 0, 0)) + lean * (t * t) * trunk_top + Vector((0, 0, t * trunk_top)) for t in (i / 8 for i in range(9))]
    radius = spec["trunkRadiusMetres"]
    radii = [radius * (1.0 - 0.55 * i / 8) for i in range(9)]
    wood = bmesh.new()
    lobes = rng.randint(4, 6)
    tube(wood, points, radii, 14, 0.06, rng, flare=lambda a: 1.0 + 0.55 * max(0.0, math.cos(lobes * a)))
    tips = []
    for index in range(rng.randint(3, 4)):
        angle = math.tau * index / 4 + rng.uniform(-0.4, 0.4)
        start = points[rng.randint(5, 7)]
        out = Vector((math.cos(angle), math.sin(angle), rng.uniform(0.6, 1.1))).normalized()
        reach = canopy * rng.uniform(0.45, 0.7)
        branch = [start + out * reach * t + Vector((0, 0, 0.3 * math.sin(t * math.pi))) for t in (k / 4 for k in range(5))]
        tube(wood, branch, [radii[6] * (0.6 - 0.4 * k / 4) for k in range(5)], 8, 0.05, rng)
        tips.append(branch[-1])
    tips.append(points[-1] + Vector((0, 0, 0.5)))
    obj = to_object("tree", wood, ["bark"])
    leaves = bmesh.new()
    offset = Vector((rng.uniform(-100, 100), rng.uniform(-100, 100), 0))
    masses = rng.randint(*spec["masses"])
    for index in range(masses):
        anchor = tips[index % len(tips)]
        spread = Vector((rng.uniform(-1, 1), rng.uniform(-1, 1), rng.uniform(-0.2, 0.6))) * canopy * 0.35
        size = canopy * rng.uniform(0.38, 0.55)
        lumpy_mass(leaves, anchor + spread + Vector((0, 0, size * 0.3)), Vector((size, size, size * 0.72)), rng, offset + Vector((index * 7.1, 0, 0)), 0.35)
    leaf_obj = to_object("leaves", leaves, ["leaves"])
    join(obj, [leaf_obj])
    ground_pivot(obj, sink=0.2)
    shade(obj, 70)
    tint(obj, rng, foliage_slot=1)
    return obj


def tint(obj, rng, foliage_slot):
    """Vertex colour: per mass a slight tint on foliage, darker underneath; plain white elsewhere."""
    mesh = obj.data
    seed = rng.uniform(0, 100)
    top = max(v.co.z for v in mesh.vertices)

    def colour(position, polygon_index):
        polygon = mesh.polygons[polygon_index]
        if polygon.material_index != foliage_slot:
            return (1.0, 1.0, 1.0, 1.0)
        shade_under = 0.55 + 0.45 * max(0.0, polygon.normal.z * 0.5 + 0.5)
        hue = 0.5 + 0.5 * noise.noise(position * 0.6 + Vector((seed, 0, 0)))
        return (shade_under * (0.85 + 0.25 * hue), shade_under * (0.95 + 0.1 * hue), shade_under * 0.9, position.z / top)

    colour_layer(obj, colour)


def shrub(spec, rng):
    radius = rng.uniform(*spec["radiusMetres"])
    leaves = bmesh.new()
    offset = Vector((rng.uniform(-100, 100), rng.uniform(-100, 100), 0))
    for index in range(rng.randint(*spec["masses"])):
        angle = rng.uniform(0, math.tau)
        distance = radius * rng.uniform(0.0, 0.6)
        size = radius * rng.uniform(0.45, 0.7)
        centre = Vector((math.cos(angle) * distance, math.sin(angle) * distance, size * rng.uniform(0.55, 0.8)))
        lumpy_mass(leaves, centre, Vector((size, size, size * 0.8)), rng, offset + Vector((index * 5.3, 0, 0)), 0.3)
    obj = to_object("shrub", leaves, ["leaves"])
    decimate(obj, spec["triangles"])
    ground_pivot(obj, sink=0.08)
    shade(obj, 80)
    tint(obj, rng, foliage_slot=0)
    return obj


def fern(spec, rng):
    bm = bmesh.new()
    fronds = rng.randint(*spec["fronds"])
    for index in range(fronds):
        angle = math.tau * index / fronds + rng.uniform(-0.2, 0.2)
        length = rng.uniform(*spec["lengthMetres"])
        out = Vector((math.cos(angle), math.sin(angle), 0))
        side = Vector((-out.y, out.x, 0))
        steps = 9
        rise, droop = rng.uniform(0.5, 0.8), rng.uniform(0.6, 1.0)
        spine = [out * length * t + Vector((0, 0, length * (rise * math.sin(t * math.pi * 0.7) - droop * 0.35 * t * t))) for t in (k / steps for k in range(steps + 1))]
        for k in range(steps):
            width = length * 0.16 * (1.0 - k / steps) ** 0.7
            base, tip = spine[k], spine[k + 1]
            for sign in (-1, 1):
                leaf = base + side * sign * width + Vector((0, 0, -0.02))
                a, b, c = bm.verts.new(base), bm.verts.new(tip), bm.verts.new(leaf)
                bm.faces.new((a, c, b) if sign > 0 else (a, b, c))
    obj = to_object("fern", bm, ["leaves"])
    ground_pivot(obj, sink=0.0)
    shade(obj, 89)
    tint(obj, rng, foliage_slot=0)
    return obj


def grass(spec, rng):
    bm = bmesh.new()
    spread = spec["spreadMetres"]
    for _ in range(rng.randint(*spec["blades"])):
        angle = rng.uniform(0, math.tau)
        distance = spread * math.sqrt(rng.random())
        root = Vector((math.cos(angle) * distance, math.sin(angle) * distance, 0))
        height = rng.uniform(*spec["heightMetres"])
        facing = rng.uniform(0, math.tau)
        side = Vector((math.cos(facing), math.sin(facing), 0))
        lean = Vector((root.x, root.y, 0)).normalized() * height * rng.uniform(0.15, 0.4) if distance > 0 else Vector()
        width = height * rng.uniform(0.03, 0.05)
        levels = 3
        row = []
        for k in range(levels + 1):
            t = k / levels
            centre = root + lean * t * t + Vector((0, 0, height * t))
            half = width * (1.0 - t) * 0.5
            row.append((bm.verts.new(centre - side * half), bm.verts.new(centre + side * half)) if k < levels else (bm.verts.new(centre),))
        for k in range(levels - 1):
            (a, b), (c, d) = row[k], row[k + 1]
            bm.faces.new((a, b, d, c))
        (a, b), (tip,) = row[levels - 1], row[levels]
        bm.faces.new((a, b, tip))
    obj = to_object("grass", bm, ["grass"])
    ground_pivot(obj, sink=0.0)
    shade(obj, 89)
    top = max(v.co.z for v in obj.data.vertices)
    seed = rng.uniform(0, 100)
    colour_layer(obj, lambda position, _: (0.85 + 0.15 * noise.noise(position * 3 + Vector((seed, 0, 0))), 1.0, 1.0, position.z / top))
    return obj


BUILDERS = {"rock": rock, "pillar": pillar, "block": block, "stele": stele, "tree": tree, "shrub": shrub, "fern": fern, "grass": grass}


def finish(obj, name):
    obj.name = name
    obj.data.name = name
    box_uvs(obj, 1.0)
    bm = bmesh.new()
    bm.from_mesh(obj.data)
    bmesh.ops.triangulate(bm, faces=bm.faces[:])
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    degenerate = [face for face in bm.faces if face.calc_area() < 1e-10]
    bmesh.ops.delete(bm, geom=degenerate, context="FACES")
    bm.to_mesh(obj.data)
    bm.free()
    obj.data.update()


def export(obj, filename):
    bpy.ops.object.select_all(action="DESELECT")
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj
    bpy.ops.export_scene.fbx(filepath=str(filename), use_selection=True, object_types={"MESH"}, apply_unit_scale=True,
                             apply_scale_options="FBX_SCALE_UNITS", axis_forward="-Y", axis_up="Z", bake_anim=False,
                             mesh_smooth_type="EDGE", colors_type="SRGB", use_mesh_modifiers=False)


def preview(objects):
    """A contact sheet of every generated mesh, for review before import."""
    spacing = 0.0
    x = 0.0
    for obj in objects:
        width = max(obj.dimensions.x, obj.dimensions.y)
        obj.location = (x + width / 2, 0, 0)
        x += width + 1.0
        spacing = max(spacing, obj.dimensions.z)
    camera_data = bpy.data.cameras.new("Preview")
    camera_data.type = "ORTHO"
    camera_data.ortho_scale = max(x * 1.02, spacing * 2.6)
    camera = bpy.data.objects.new("Preview", camera_data)
    SCENE.collection.objects.link(camera)
    camera.location = (x / 2, -x, x * 0.55)
    camera.rotation_euler = ((Vector((x / 2, 0, spacing * 0.4)) - camera.location).to_track_quat("-Z", "Y").to_euler())
    SCENE.camera = camera
    SCENE.render.engine = "BLENDER_WORKBENCH"
    SCENE.display.shading.light = "STUDIO"
    SCENE.display.shading.color_type = "MATERIAL"
    SCENE.display.shading.show_cavity = True
    SCENE.render.resolution_x, SCENE.render.resolution_y = 2400, 900
    SCENE.render.filepath = str(GAME / "Saved" / "EnvironmentKit" / "kit_preview.png")
    (GAME / "Saved" / "EnvironmentKit").mkdir(parents=True, exist_ok=True)
    bpy.ops.render.render(write_still=True)


# The kit's look is the importer's alone (EnvironmentKit/inputs.py), so the manifest hashes the rest: what the meshes are made from.
report = {"generator": "GenerateEnvironmentMeshes.v3", "blender": bpy.app.version_string, "seed": KIT["seed"],
          "meshProfileSha256": mesh_profile_sha256(KIT),
          "collision": "None; gameplay terrain is owned by World.json and VeyraWorld", "assets": []}
made = []
kept = []
for family_index, family in enumerate(KIT["families"]):
    if ONLY and family["id"] not in ONLY:
        continue
    for variant in range(family["variants"]):
        seed = KIT["seed"] + family_index * 100 + variant
        rng = random.Random(seed)
        noise.seed_set(seed)
        name = f'SM_Crucible_{family["id"]}_{variant:02d}'
        obj = BUILDERS[family["kind"]](family, rng)
        finish(obj, name)
        geometry = json.dumps({"vertices": [[round(c, 6) for c in v.co] for v in obj.data.vertices],
                               "faces": [list(p.vertices) for p in obj.data.polygons]}, separators=(",", ":"))
        filename = SOURCE / "FBX" / (name + ".fbx")
        # Its FBX stands, bytes and hash, when the mesh comes out as it was: only an export's time stamp would differ.
        if export_keeping_unchanged(lambda path, mesh=obj: export(mesh, path), filename):
            kept.append(name)
        report["assets"].append({"name": name, "family": family["id"], "kind": family["kind"], "seed": seed,
                                 "file": "FBX/" + filename.name, "sha256": hashlib.sha256(filename.read_bytes()).hexdigest(),
                                 "geometrySha256": hashlib.sha256(geometry.encode()).hexdigest(),
                                 "contentSha256": content_sha256(filename),
                                 "triangles": len(obj.data.polygons), "dimensionsCm": [round(v * 100, 1) for v in obj.dimensions],
                                 "materialSlots": [m.name for m in obj.data.materials]})
        made.append(obj)
        print(f"VEYRA_KIT {name}: {len(obj.data.polygons)} triangles, {[round(v, 2) for v in obj.dimensions]} m")
if PREVIEW:
    preview(made)
if not ONLY:
    (SOURCE / "manifest.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8", newline="\n")
print(f'VEYRA_ENVIRONMENT_GENERATED: {len(report["assets"])} assets, {len(report["assets"]) - len(kept)} written, {len(kept)} kept unchanged')
