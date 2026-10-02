"""Build Veyra's provisional structure art in a fresh, background Blender process.

Run with Blender --background --factory-startup --python <this file>.
World.json supplies the current footprint; this script owns visual geometry only.
No gameplay collision, timing, state machine or faction identity is exported.
"""
import hashlib
import json
import math
from pathlib import Path

import bpy
import bmesh
from mathutils import Vector


GAME = Path(__file__).resolve().parents[1]
SOURCE = GAME / "ArtSource" / "Structures"
SAVED = GAME / "Saved" / "StructureKit"
KIT = json.loads((SOURCE / "StructureKit.json").read_text())
WORLD_BYTES = (GAME / "Tuning" / "World.json").read_bytes()
WORLD = json.loads(WORLD_BYTES)
SAVED.mkdir(parents=True, exist_ok=True)
(SOURCE / "FBX").mkdir(exist_ok=True)

# This is an isolated background scene, never an artist's open document.
if not bpy.app.background:
    raise RuntimeError("Run in a separate background Blender process.")
bpy.ops.object.select_all(action="SELECT")
bpy.ops.object.delete(use_global=False)
SCENE = bpy.context.scene
SCENE.unit_settings.system = "METRIC"
SCENE.unit_settings.scale_length = 1.0
MATERIALS = []
for spec in KIT["materials"]:
    mat = bpy.data.materials.new(spec["name"])
    mat.diffuse_color = spec["color"]
    mat.use_nodes = True
    shader = mat.node_tree.nodes.get("Principled BSDF")
    shader.inputs["Base Color"].default_value = spec["color"]
    shader.inputs["Metallic"].default_value = spec["metallic"]
    shader.inputs["Roughness"].default_value = spec["roughness"]
    shader.inputs["Emission Color"].default_value = spec["color"]
    shader.inputs["Emission Strength"].default_value = spec["emission"]
    MATERIALS.append(mat)

PARTS = []


def mesh(name, vertices, faces, material=0):
    data = bpy.data.meshes.new(name)
    data.from_pydata(vertices, [], faces)
    data.update()
    obj = bpy.data.objects.new(name, data)
    SCENE.collection.objects.link(obj)
    for mat in MATERIALS:
        data.materials.append(mat)
    for face in data.polygons:
        face.material_index = material
    PARTS.append(obj)
    return obj


def ring(name, outer, inner, z0, z1, material=1, segments=None, start=0, arc=math.tau):
    """Closed annular solid, optionally an open arc with end caps."""
    n = segments or KIT["geometry"]["radialSegments"]
    full = math.isclose(arc, math.tau)
    count = n if full else n + 1
    vertices = []
    for z, r in [(z0, outer), (z1, outer), (z0, inner), (z1, inner)]:
        vertices += [(r * math.cos(start + arc*i/n), r * math.sin(start + arc*i/n), z) for i in range(count)]
    faces = []
    for i in range(n):
        j = (i + 1) % count
        faces += [(i, j, count+j, count+i), (2*count+j, 2*count+i, 3*count+i, 3*count+j),
                  (count+i, count+j, 3*count+j, 3*count+i), (j, i, 2*count+i, 2*count+j)]
    if not full:
        faces += [(0, count, 3*count, 2*count), (n, 2*count+n, 3*count+n, count+n)]
    return mesh(name, vertices, faces, material)


def drum(name, profile, material=0, segments=12):
    """Solid rotational shell defined by (height, radius) stations."""
    vertices = [(r*math.cos(math.tau*i/segments), r*math.sin(math.tau*i/segments), z)
                for z, r in profile for i in range(segments)]
    faces = [tuple(reversed(range(segments)))]
    for level in range(len(profile)-1):
        for i in range(segments):
            j = (i+1) % segments
            faces.append((level*segments+i, level*segments+j, (level+1)*segments+j, (level+1)*segments+i))
    faces.append(tuple((len(profile)-1)*segments+i for i in range(segments)))
    return mesh(name, vertices, faces, material)


def blade(name, angle, stations, width, material=0):
    """A tapered radial buttress with a broad readable face."""
    vertices = []
    for z, inner, outer in stations:
        for r, t in [(inner, -width/2), (outer, -width/2), (outer, width/2), (inner, width/2)]:
            vertices.append((r*math.cos(angle)-t*math.sin(angle), r*math.sin(angle)+t*math.cos(angle), z))
    faces = [(3, 2, 1, 0)]
    for level in range(len(stations)-1):
        for i in range(4):
            j = (i+1) % 4
            faces.append((4*level+i, 4*level+j, 4*(level+1)+j, 4*(level+1)+i))
    faces.append(tuple(4*(len(stations)-1)+i for i in range(4)))
    return mesh(name, vertices, faces, material)


def segmented(name, outer, inner, z0, z1, count, gap, material=0, phase=0):
    for i in range(count):
        ring(name, outer, inner, z0, z1, material, 6, phase + math.tau*i/count + gap/2, math.tau/count-gap)


def foundation():
    drum("Foundation", [(0, .88), (.025, .98), (.065, .98), (.09, .86)], 0, 12)
    ring("FoundationBand", .92, .82, .062, .081, 2, 48)
    segmented("NetworkPorts", .985, .9, .035, .05, 6, .75, 3)


def spire(ruined):
    foundation()
    drum("Foot", [(.085, .70), (.14, .74), (.21, .52)], 0)
    ring("LowerCollar", .55, .36, .19, .24, 2)
    if ruined:
        ring("RupturedSocket", .47, .30, .22, .34, 1, 12)
        for i, top in enumerate([.45, .36, .50]):
            blade("SplitShell", math.tau*i/3, [(.12, .49, .8), (.24, .30, .55), (top, .33, .48)], .24, 0)
        segmented("BrokenCrown", .7, .55, .09, .145, 3, 1.4, 2, .4)
        return
    drum("ConduitHousing", [(.20, .38), (.34, .42), (.62, .30), (.75, .35)], 1)
    for i in range(3):
        angle = math.tau*i/3
        blade("SpireShell", angle, [(.12, .45, .82), (.31, .30, .63), (.69, .22, .48), (.85, .25, .38)], .27)
        blade("FluxConduit", angle+.23, [(.24, .32, .41), (.62, .24, .34), (.75, .29, .38)], .06, 3)
        blade("CrownProng", angle, [(.72, .30, .51), (.86, .39, .58), (1, .41, .49)], .21, 2)
    ring("DischargeRing", .50, .33, .73, .77, 2)
    ring("DischargeAperture", .33, .19, .77, .81, 3)
    drum("DischargeLens", [(.75, .18), (.84, .23), (.87, .08)], 3, 16)
    segmented("InsulatorBlocks", .51, .40, .55, .61, 6, .28, 0)


def tower(ruined):
    foundation()
    drum("ArmoredPedestal", [(.08, .74), (.21, .78), (.29, .55)], 0, 8)
    ring("PedestalCollar", .73, .52, .19, .225, 2, 32)
    if ruined:
        ring("OpenChamber", .57, .37, .26, .36, 1, 16)
        for i in range(4):
            blade("ShearedPylon", math.tau*i/4+math.pi/4,
                  [(.1, .51, .75), (.31, .4, .63), (.43 if i%2 else .34, .43, .58)], .30)
        segmented("BrokenEmitter", .59, .41, .08, .15, 3, 1.2, 2)
        return
    drum("CentralChamber", [(.25, .43), (.65, .43), (.73, .53)], 1, 16)
    ring("ChamberCharge", .44, .34, .49, .54, 3)
    for i in range(4):
        angle = math.tau*i/4+math.pi/4
        blade("DefensePylon", angle, [(.1, .51, .76), (.35, .43, .67), (.7, .36, .55), (.92, .40, .54)], .33)
        blade("PylonCap", angle, [(.71, .35, .60), (.88, .37, .58), (1, .42, .53)], .38, 2)
        blade("ChargeChannel", angle+.36, [(.29, .43, .47), (.70, .43, .47)], .07, 3)
    ring("EmitterMantle", .64, .34, .71, .77, 2, 16)
    ring("EmitterRim", .48, .34, .81, .86, 1)
    ring("FluxEmitter", .34, .17, .78, .85, 3)
    drum("EmitterCore", [(.75, .16), (.90, .16), (.92, .04)], 3, 16)


def inhibitor(ruined):
    foundation()
    ring("ReconstructionBed", .82, .36, .10, .23, 1)
    ring("LowerTrack", .77, .60, .23, .30, 2)
    if ruined:
        segmented("SeparatedRebuildPlates", .82, .51, .29, .42, 6, .44, 0)
        ring("EmptyRebuildSocket", .40, .28, .12, .31, 1)
        return
    segmented("ReconstructionPetals", .88, .52, .31, .64, 6, .18, 0)
    segmented("PetalCaps", .83, .58, .64, .71, 6, .18, 2)
    ring("NetworkLoop", .55, .43, .37, .42, 3)
    drum("RebuildSpindle", [(.18, .29), (.60, .22), (.69, .35), (.81, .35), (.90, .26)], 1)
    ring("SpindleCharge", .26, .12, .90, .94, 3)
    ring("SpindleCap", .30, .22, .94, 1, 2, 24)
    for i in range(3):
        blade("AnchorRib", math.tau*i/3, [(.13, .65, .88), (.56, .64, .81), (.80, .60, .66)], .12, 2)


def well(ruined):
    foundation()
    ring("ReservoirWall", .84, .64, .09, .23, 0)
    ring("ReservoirRim", .86, .62, .23, .28, 2)
    drum("ReservoirFloor", [(.07, .63), (.11, .63)], 1, 48)
    ring("InnerCircuit", .55, .48, .13, .17, 1 if ruined else 3)
    segmented("OuterCircuit", .88, .79, .14, .17, 12, .15, 1 if ruined else 3)
    if ruined:
        for i, top in enumerate([.42, .55, .36]):
            blade("FracturedStabilizer", math.tau*i/3,
                  [(.1, .68, .87), (.25, .60, .82), (top, .55, .72)], .14)
        segmented("CollapsedCollector", .6, .44, .13, .22, 3, .8, 2)
        return
    drum("ContainedFlux", [(.12, .42), (.22, .45), (.38, .29), (.60, .20), (.79, .20)], 3, 32)
    for z, r in [(.30, .39), (.48, .29), (.65, .27)]:
        ring("ContainmentHoop", r+.025, r, z, z+.035, 1)
    for i in range(3):
        a = math.tau*i/3
        blade("StabilizerArch", a, [(.10, .67, .86), (.34, .61, .82), (.64, .43, .63), (.83, .30, .48)], .15, 0)
        blade("ArchInlay", a, [(.30, .60, .64), (.61, .42, .46), (.79, .30, .34)], .06, 3)
        blade("AnchorCasing", a, [(.1, .65, .92), (.24, .64, .87), (.35, .60, .76)], .26, 2)
    ring("CollectorCrown", .50, .30, .82, .91, 2)
    ring("CollectorCap", .47, .32, .91, .95, 0)
    ring("OpenFluxOculus", .32, .23, .91, 1, 3)
    segmented("ConductorTeeth", .52, .44, .77, .88, 12, .3, 1)


BUILDERS = {"laneSpire": spire, "baseTower": tower, "inhibitor": inhibitor, "primeWell": well}
REPORT = {"schemaVersion": 1, "worldSha256": hashlib.sha256(WORLD_BYTES).hexdigest(),
          "blenderVersion": bpy.app.version_string, "units": "centimetres in FBX", "assets": []}
EXPORTED = []
for spec in KIT["assets"]:
    tuning = WORLD["structures"][spec["id"]]
    radius = tuning["capsuleRadius"] / 100
    height = 2 * tuning["capsuleHalfHeight"] / 100 * spec["visualHeightFraction"]
    for ruined in (False, True):
        PARTS.clear()
        BUILDERS[spec["id"]](ruined)
        name = "SM_" + spec["name"] + ("_Destroyed" if ruined else "_Standing")
        bpy.ops.object.select_all(action="DESELECT")
        for obj in PARTS:
            for v in obj.data.vertices:
                v.co.x *= radius
                v.co.y *= radius
                v.co.z *= height
            obj.select_set(True)
        bpy.context.view_layer.objects.active = PARTS[0]
        bpy.ops.object.join()
        obj = bpy.context.object
        obj.name = name
        obj.data.name = name
        bevel = obj.modifiers.new("ManufacturedEdge", "BEVEL")
        bevel.width = KIT["geometry"]["bevelWidthMetres"]
        bevel.segments = KIT["geometry"]["bevelSegments"]
        bpy.ops.object.modifier_apply(modifier=bevel.name)
        bm = bmesh.new()
        bm.from_mesh(obj.data)
        bmesh.ops.recalc_face_normals(bm, faces=list(bm.faces))
        bm.to_mesh(obj.data)
        bm.free()
        bpy.ops.object.mode_set(mode="EDIT")
        bpy.ops.mesh.select_all(action="SELECT")
        bpy.ops.uv.smart_project(island_margin=KIT["geometry"]["uvMargin"])
        bpy.ops.object.mode_set(mode="OBJECT")
        obj.data.uv_layers[0].name = "UV0"
        # The smart-projected atlas is unique per face island. Keep an explicit
        # second channel so both importers and commandlets can verify lightmap UVs.
        obj.data.uv_layers.new(name="UV1_Lightmap", do_init=True)
        triangulate = obj.modifiers.new("ExportTriangles", "TRIANGULATE")
        bpy.ops.object.modifier_apply(modifier=triangulate.name)
        bm = bmesh.new()
        bm.from_mesh(obj.data)
        bmesh.ops.dissolve_degenerate(bm, dist=1e-6, edges=list(bm.edges))
        bmesh.ops.triangulate(bm, faces=list(bm.faces))
        bm.to_mesh(obj.data)
        bm.free()
        obj.data.update()
        bpy.context.view_layer.update()
        assert min(v.co.z for v in obj.data.vertices) >= -1e-5
        assert max(math.hypot(v.co.x, v.co.y) for v in obj.data.vertices) <= radius + 1e-5
        assert len(obj.data.uv_layers) == 2
        assert all(p.area > 1e-10 for p in obj.data.polygons)
        path = SOURCE / "FBX" / (name + ".fbx")
        bpy.ops.export_scene.fbx(filepath=str(path), use_selection=True, object_types={"MESH"},
                                 apply_unit_scale=True, apply_scale_options="FBX_SCALE_UNITS",
                                 axis_forward="-Y", axis_up="Z", bake_anim=False, mesh_smooth_type="FACE")
        dims = [round(v*100, 3) for v in obj.dimensions]
        REPORT["assets"].append({"name": name, "kind": spec["id"], "state": "Destroyed" if ruined else "Standing",
                                 "file": "FBX/"+path.name, "sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
                                 "triangles": len(obj.data.polygons), "dimensionsCm": dims,
                                 "groundPivot": True, "uvChannels": 2, "materialSlots": [m.name for m in obj.data.materials]})
        obj.location = (spec["previewX"], KIT["preview"]["ruinRowY"] if ruined else 0, 0)
        EXPORTED.append(obj)

(SOURCE / "manifest.json").write_text(json.dumps(REPORT, indent=2)+"\n")

# An orthographic render of the exported geometry, not an in-game screenshot.
floor = bpy.data.materials.new("PreviewGround")
floor.diffuse_color = (.042, .057, .075, 1)
bpy.ops.mesh.primitive_plane_add(size=200)
plane = bpy.context.object
plane.name = "PreviewGround"
plane.location.z = -.025
plane.data.materials.append(floor)


def aim(obj, point):
    obj.rotation_euler = (Vector(point)-obj.location).to_track_quat("-Z", "Y").to_euler()


for name, loc, energy, size, color in [
    ("Key", (0, -5, 15), 3000, 12, (1, .90, .76)),
    ("Fill", (-8, 1, 8), 1600, 9, (.52, .73, 1)),
    ("Rim", (7, 10, 14), 3400, 10, (.65, .82, 1))]:
    data = bpy.data.lights.new(name, "AREA")
    data.energy, data.shape, data.size, data.color = energy, "DISK", size, color
    obj = bpy.data.objects.new(name, data)
    SCENE.collection.objects.link(obj)
    obj.location = loc
    aim(obj, (0, 3, 1))

camera_data = bpy.data.cameras.new("StructureReview")
camera = bpy.data.objects.new("StructureReview", camera_data)
SCENE.collection.objects.link(camera)
camera.location = (13, -26, 25)
aim(camera, (1, 3.0, 1.8))
camera_data.type = "ORTHO"
camera_data.ortho_scale = 25
SCENE.camera = camera
SCENE.render.engine = "CYCLES"
SCENE.cycles.samples = KIT["preview"]["samples"]
SCENE.cycles.use_denoising = True
SCENE.render.resolution_x = KIT["preview"]["width"]
SCENE.render.resolution_y = KIT["preview"]["height"]
SCENE.render.resolution_percentage = 100
SCENE.world.color = (.20, .20, .20)
SCENE.render.image_settings.file_format = "PNG"
SCENE.render.filepath = str(SAVED / "StructureKit.png")
bpy.ops.wm.save_as_mainfile(filepath=str(SAVED / "StructureKit.blend"))
bpy.ops.render.render(write_still=True)
print("VEYRA_STRUCTURE_KIT_COMPLETE " + json.dumps(REPORT))
