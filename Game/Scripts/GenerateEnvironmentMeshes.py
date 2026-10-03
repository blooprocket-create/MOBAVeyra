"""Deterministic Veyra environment families; run in background Blender 5.2.

Geometry and styles only: no map coordinates, collision, or gameplay decisions.
The manifest hashes normalized geometry as well as exported files because FBX
container metadata may differ without changing the mesh.
"""
import hashlib
import json
import math
import random
from pathlib import Path

import bpy
import bmesh

GAME = Path(__file__).resolve().parents[1]
SOURCE = GAME / "ArtSource" / "Environment"
PROFILE = SOURCE / "CrucibleKit.json"
KIT = json.loads(PROFILE.read_text(encoding="utf-8"))
if not bpy.app.background:
    raise RuntimeError("Use an isolated background Blender process.")
if bpy.app.version[:2] != (5, 2):
    raise RuntimeError("This source generator is validated against Blender 5.2.")
bpy.ops.object.select_all(action="SELECT")
bpy.ops.object.delete(use_global=False)
bpy.context.scene.unit_settings.system = "METRIC"
bpy.context.scene.unit_settings.scale_length = 1.0
(SOURCE / "FBX").mkdir(parents=True, exist_ok=True)
MATERIALS = []
for spec in KIT["materials"]:
    mat = bpy.data.materials.new(spec["name"])
    mat.diffuse_color = spec["color"]
    MATERIALS.append(mat)


def rock(name, size, rng, roughness, material):
    """Broad stratified masses with an irregular outline and closed ground pivot."""
    rings, segments = KIT["geometry"]["rockRings"], KIT["geometry"]["rockSegments"]
    silhouette = [rng.uniform(1-roughness, 1+roughness) for _ in range(segments)]
    vertices = []
    for ring in range(rings):
        z = ring / (rings-1)
        # Broad shoulders and a narrow cap; strata disturb whole rings, not white noise.
        width = (0.8 + 0.2*math.sin(math.pi*z)) * (1 - 0.45*z**4)
        stratum = rng.uniform(1-roughness/2, 1+roughness/2)
        shear = roughness * math.sin(z*math.pi) * size[0]
        for sector in range(segments):
            angle = math.tau*sector/segments
            radial = silhouette[sector]*width*stratum
            vertices.append((math.cos(angle)*size[0]*radial/2+shear,
                             math.sin(angle)*size[1]*radial/2,
                             z*size[2]))
    faces = [tuple(reversed(range(segments)))]
    for ring in range(rings-1):
        for sector in range(segments):
            next_sector = (sector+1) % segments
            faces.append((ring*segments+sector, ring*segments+next_sector,
                          (ring+1)*segments+next_sector, (ring+1)*segments+sector))
    faces.append(tuple((rings-1)*segments+i for i in range(segments)))
    mesh = bpy.data.meshes.new(name)
    mesh.from_pydata(vertices, [], faces)
    mesh.update()
    obj = bpy.data.objects.new(name, mesh)
    bpy.context.scene.collection.objects.link(obj)
    mesh.materials.append(MATERIALS[material])
    return obj


def foliage(name, size, rng, tree):
    parts = []
    if tree:
        bpy.ops.mesh.primitive_cone_add(vertices=10, radius1=size[0]*0.09,
                                        radius2=size[0]*0.035, depth=size[2]*0.8,
                                        location=(0, 0, size[2]*0.4))
        trunk = bpy.context.object
        trunk.data.materials.append(MATERIALS[3])
        parts.append(trunk)
    # Closed leaf masses keep opacity and silhouette independent of quality settings.
    for index in range(7):
        angle = math.tau*index/7 + rng.uniform(-0.2, 0.2)
        radius = rng.uniform(0.05, 0.25)
        bpy.ops.mesh.primitive_ico_sphere_add(subdivisions=2, radius=1,
            location=(math.cos(angle)*size[0]*radius, math.sin(angle)*size[1]*radius,
                      size[2]*(0.65 if tree else 0.48)+rng.uniform(-0.06, 0.06)*size[2]))
        obj = bpy.context.object
        obj.scale = (size[0]*0.3, size[1]*0.3, size[2]*(0.2 if tree else 0.4))
        obj.data.materials.append(MATERIALS[4])
        parts.append(obj)
    bpy.ops.object.select_all(action="DESELECT")
    for obj in parts:
        obj.select_set(True)
    bpy.context.view_layer.objects.active = parts[0]
    bpy.ops.object.join()
    obj = bpy.context.object
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    obj.name = name
    return obj


report = {"generator":"GenerateEnvironmentMeshes.v1", "blender":bpy.app.version_string,
          "seed":KIT["seed"], "profileSha256":hashlib.sha256(PROFILE.read_bytes()).hexdigest(),
          "collision":"None; gameplay terrain is owned by World.json and VeyraWorld", "assets":[]}
for family_index, family in enumerate(KIT["families"]):
    for variant in range(family["variants"]):
        seed = KIT["seed"] + family_index*100 + variant
        rng = random.Random(seed)
        name = f'SM_Crucible_{family["id"]}_{variant:02d}'
        if family["kind"] == "rock":
            obj = rock(name, family["sizeMetres"], rng, family["roughness"], family["material"])
        else:
            obj = foliage(name, family["sizeMetres"], rng, family["kind"] == "tree")
        bpy.ops.object.select_all(action="DESELECT")
        obj.select_set(True)
        bpy.context.view_layer.objects.active = obj
        bevel = obj.modifiers.new("WeatheredEdges", "BEVEL")
        bevel.width = KIT["geometry"]["bevelMetres"]
        bevel.segments = KIT["geometry"]["bevelSegments"]
        bpy.ops.object.modifier_apply(modifier=bevel.name)
        bottom = min(v.co.z for v in obj.data.vertices)
        for vertex in obj.data.vertices:
            vertex.co.z -= bottom
        bpy.ops.object.mode_set(mode="EDIT")
        bpy.ops.mesh.select_all(action="SELECT")
        bpy.ops.uv.smart_project(island_margin=KIT["geometry"]["uvMargin"])
        bpy.ops.object.mode_set(mode="OBJECT")
        obj.data.uv_layers.new(name="UV1_Lightmap", do_init=True)
        bm = bmesh.new()
        bm.from_mesh(obj.data)
        bmesh.ops.triangulate(bm, faces=list(bm.faces))
        bmesh.ops.recalc_face_normals(bm, faces=list(bm.faces))
        assert all(face.calc_area() > 1e-10 for face in bm.faces), name
        assert all(edge.is_manifold for edge in bm.edges), name
        bm.to_mesh(obj.data)
        bm.free()
        obj.data.update()
        bpy.context.view_layer.update()
        geometry = json.dumps({"vertices":[[round(c,7) for c in v.co] for v in obj.data.vertices],
                               "faces":[list(p.vertices) for p in obj.data.polygons]},separators=(",",":"))
        filename = SOURCE / "FBX" / (name+".fbx")
        bpy.ops.export_scene.fbx(filepath=str(filename), use_selection=True, object_types={"MESH"},
            apply_unit_scale=True, apply_scale_options="FBX_SCALE_UNITS", axis_forward="-Y", axis_up="Z",
            bake_anim=False, mesh_smooth_type="FACE")
        report["assets"].append({"name":name,"family":family["id"],"seed":seed,
            "file":"FBX/"+filename.name,"sha256":hashlib.sha256(filename.read_bytes()).hexdigest(),
            "geometrySha256":hashlib.sha256(geometry.encode()).hexdigest(),
            "triangles":len(obj.data.polygons),"dimensionsCm":[round(v*100,3) for v in obj.dimensions],
            "materialSlots":[m.name for m in obj.data.materials],"uvChannels":len(obj.data.uv_layers)})
        bpy.data.objects.remove(obj, do_unlink=True)
(SOURCE / "manifest.json").write_text(json.dumps(report,indent=2)+"\n",encoding="utf-8",newline="\n")
print(f'VEYRA_ENVIRONMENT_GENERATED: {len(report["assets"])} assets')
