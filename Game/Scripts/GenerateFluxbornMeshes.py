"""Author the three current Fluxborn as reproducible, presentation-only mesh studies.

Use Blender --background --factory-startup --python-exit-code 1 --python <file>.
The reference pose faces +X, uses a ground pivot, and reads size from World.json.
Parts remain individually editable in the saved inspection scene. No rigs,
gameplay collision, animation timing, new unit definitions or map edits are made.
"""
import hashlib
import json
import math
import random
import sys
from pathlib import Path

import bmesh
import bpy
from mathutils import Euler, Matrix, Vector


GAME = Path(__file__).resolve().parents[1]
SOURCE = GAME / "ArtSource" / "Fluxborn"
SAVED = GAME / "Saved" / "FluxbornKit"
KIT_BYTES = (SOURCE / "FluxbornKit.json").read_bytes()
KIT = json.loads(KIT_BYTES)
UNITS = json.loads((GAME / "Tuning" / "World.json").read_bytes())["fluxborn"]["units"]
if not bpy.app.background:
    raise RuntimeError("Run in an isolated background Blender process.")
assert KIT["schemaVersion"] == 1
assert {entry["id"] for entry in KIT["assets"]} == set(UNITS), "Art must cover the exact current unit inventory"
assert 0 < KIT["geometry"]["footprintFill"] < 1
assert KIT["geometry"]["radialSegments"] >= 8
for entry in KIT["assets"]:
    assert entry["role"] == UNITS[entry["id"]]["role"]
    assert 0 < entry["visualHeightFraction"] <= 1
    assert UNITS[entry["id"]]["capsuleRadius"] > 0
    assert UNITS[entry["id"]]["capsuleHalfHeight"] > 0
SAVED.mkdir(parents=True, exist_ok=True)
(SOURCE / "FBX").mkdir(exist_ok=True)
bpy.ops.object.select_all(action="SELECT")
bpy.ops.object.delete(use_global=False)
SCENE = bpy.context.scene
SCENE.unit_settings.system = "METRIC"
SCENE.unit_settings.scale_length = 1
MATERIALS = []
for spec in KIT["materials"]:
    mat = bpy.data.materials.new(spec["name"])
    mat.diffuse_color = spec["color"]
    mat.use_nodes = True
    shader = mat.node_tree.nodes.get("Principled BSDF")
    for key, value in [("Base Color", spec["color"]), ("Metallic", spec["metallic"]),
                       ("Roughness", spec["roughness"]), ("Emission Color", spec["color"]),
                       ("Emission Strength", spec["emission"])]:
        shader.inputs[key].default_value = value
    MATERIALS.append(mat)


class Body:
    """An art assembly in footprint-relative coordinates; all measurements are visual."""
    def __init__(self, name, radius, height):
        self.name, self.radius, self.height = name, radius, height
        self.parts = []

    def position(self, v):
        return Vector((v[0]*self.radius, v[1]*self.radius, v[2]*self.height))

    def finish(self, obj, name, material):
        obj.name = self.name + "_" + name
        for mat in MATERIALS:
            obj.data.materials.append(mat)
        for polygon in obj.data.polygons:
            polygon.material_index = material
        self.parts.append(obj)
        return obj

    def box(self, name, centre, size, material=0, rotation=(0, 0, 0)):
        bpy.ops.mesh.primitive_cube_add(size=1, location=self.position(centre))
        obj = bpy.context.object
        obj.scale = self.position(size)
        bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
        obj.rotation_euler = Euler(rotation)
        return self.finish(obj, name, material)

    def joint(self, name, centre, size, material=1):
        bpy.ops.mesh.primitive_ico_sphere_add(subdivisions=1, radius=1, location=self.position(centre))
        obj = bpy.context.object
        obj.scale = self.position(size)
        bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
        return self.finish(obj, name, material)

    def beam(self, name, start, end, radius, material=1):
        a, b = self.position(start), self.position(end)
        bpy.ops.mesh.primitive_cylinder_add(vertices=KIT["geometry"]["radialSegments"],
                                           radius=radius*self.radius, depth=(b-a).length,
                                           location=(a+b)/2)
        obj = bpy.context.object
        obj.rotation_euler = (b-a).to_track_quat("Z", "Y").to_euler()
        return self.finish(obj, name, material)

    def shell(self, name, stations, material=0):
        # Octagonal cross sections bevel the silhouette before the small edge bevel.
        outline = [(1, .55), (.55, 1), (-.55, 1), (-1, .55), (-1, -.55), (-.55, -1), (.55, -1), (1, -.55)]
        verts = [self.position((cx+x*rx, cy+y*ry, z)) for z, cx, cy, rx, ry in stations for x, y in outline]
        faces = [tuple(reversed(range(8)))]
        for level in range(len(stations)-1):
            for i in range(8):
                j = (i+1) % 8
                faces.append((8*level+i, 8*level+j, 8*(level+1)+j, 8*(level+1)+i))
        faces.append(tuple(8*(len(stations)-1)+i for i in range(8)))
        data = bpy.data.meshes.new(self.name+"_"+name)
        data.from_pydata(verts, [], faces)
        data.update()
        obj = bpy.data.objects.new(data.name, data)
        SCENE.collection.objects.link(obj)
        return self.finish(obj, name, material)

    def ring(self, name, centre, outer, inner, depth, material=2, axis="Z"):
        n = KIT["geometry"]["radialSegments"]
        vertices = []
        # Rings are circular in physical space, independent of the body's aspect.
        origin = self.position(centre)
        for z, r in [(-depth/2, outer), (depth/2, outer), (-depth/2, inner), (depth/2, inner)]:
            for i in range(n):
                a = math.tau*i/n
                local = (z, r*math.cos(a), r*math.sin(a)) if axis == "X" else (r*math.cos(a), r*math.sin(a), z)
                vertices.append(origin + Vector(local)*self.radius)
        faces = []
        for i in range(n):
            j = (i+1) % n
            faces += [(i,j,n+j,n+i), (2*n+j,2*n+i,3*n+i,3*n+j),
                      (n+i,n+j,3*n+j,3*n+i), (j,i,2*n+i,2*n+j)]
        data = bpy.data.meshes.new(self.name+"_"+name)
        data.from_pydata(vertices, [], faces)
        data.update()
        obj = bpy.data.objects.new(data.name, data)
        SCENE.collection.objects.link(obj)
        return self.finish(obj, name, material)


def strider(b):
    for sign, side in [(-1, "R"), (1, "L")]:
        y = sign*.36
        b.box("Foot"+side, (.14,y,.055), (.69,.38,.11), 1)
        b.box("ToeCap"+side, (.31,y,.085), (.35,.40,.11))
        b.beam("Shin"+side, (.10,y,.11), (.04,y,.32), .13)
        b.box("ShinPlate"+side, (.19,y,.23), (.21,.30,.20))
        b.joint("Knee"+side, (.04,y,.33), (.16,.16,.055), 2)
        b.beam("Thigh"+side, (.02,y,.34), (-.07,sign*.27,.49), .145)
        b.box("Shoulder"+side, (-.015,sign*.60,.735), (.55,.40,.16))
        b.box("ShoulderInlay"+side, (.04,sign*.60,.820), (.36,.25,.018), 2)
        b.beam("UpperArm"+side, (-.01,sign*.62,.70), (.10,sign*.65,.59), .12)
        b.joint("Elbow"+side, (.10,sign*.65,.58), (.13,.13,.055), 2)
        b.box("StrikingGauntlet"+side, (.30,sign*.64,.54), (.51,.34,.22))
        b.box("ImpactFace"+side, (.57,sign*.64,.54), (.09,.36,.19), 2)
        b.box("FistConduit"+side, (.41,sign*.64,.66), (.29,.09,.018), 3)
    b.shell("HipFrame", [(.43,-.04,0,.26,.37),(.51,-.04,0,.29,.36)],1)
    b.shell("ChestShell", [(.51,0,0,.24,.28),(.72,-.03,0,.36,.49),(.79,-.06,0,.29,.45)])
    b.box("ChestSocket", (.32,0,.68), (.09,.47,.10),1)
    b.box("ChestFlux", (.375,0,.68), (.018,.31,.045),3)
    b.beam("Neck", (0,0,.77),(0,0,.83),.17,1)
    b.shell("Head", [(.82,.03,0,.27,.26),(.93,.07,0,.30,.28),(.97,.03,0,.18,.21)])
    b.box("SensorRecess", (.36,0,.894),(.065,.43,.055),1)
    b.box("SensorSlit", (.397,0,.895),(.012,.32,.019),3)
    b.box("Crest", (-.02,0,.965),(.36,.09,.07),2)
    b.box("BackHousing", (-.37,0,.66),(.17,.42,.20),1)
    for side in [-1,1]:
        b.box("BackFeed", (-.465,side*.12,.66),(.015,.045,.15),3)


def spark(b):
    for i, angle in enumerate([0, math.tau/3, 2*math.tau/3]):
        dx,dy = math.cos(angle), math.sin(angle)
        b.beam("UpperLeg"+str(i),(.22*dx,.22*dy,.43),(.50*dx,.50*dy,.26),.075)
        b.joint("Knee"+str(i),(.50*dx,.50*dy,.26),(.11,.11,.038),2)
        b.beam("LowerLeg"+str(i),(.50*dx,.50*dy,.26),(.70*dx,.70*dy,.045),.067)
        b.box("Foot"+str(i),(.70*dx,.70*dy,.035),(.32,.26,.07),1, (0,0,angle))
    b.shell("Spindle",[(.32,0,0,.15,.15),(.44,0,0,.27,.27),(.68,0,0,.20,.20)],1)
    b.shell("FluxChamber",[(.46,0,0,.20,.20),(.63,0,0,.27,.27),(.76,0,0,.16,.16)],3)
    b.ring("LowerCage",(0,0,.46),.33,.22,.13,2)
    b.ring("UpperCage",(0,0,.72),.35,.23,.11,2)
    for sign in [-1,1]:
        b.box("SideCasing",(-.02,sign*.30,.60),(.35,.15,.30))
        b.box("ResonatorVane",(-.25,sign*.43,.835),(.19,.13,.29),0, (sign*-.15,-.1,0))
        b.box("VaneTerminal",(-.25,sign*.43,.982),(.20,.14,.036),2)
    b.box("DorsalVane",(-.45,0,.81),(.19,.14,.31))
    b.box("DorsalTerminal",(-.45,0,.965),(.20,.15,.025),2)
    b.shell("SensorHood",[(.75,.11,0,.25,.31),(.84,.10,0,.28,.34),(.90,.05,0,.20,.23)])
    b.beam("EmitterBarrel",(.20,0,.68),(.65,0,.68),.23,1)
    b.ring("EmitterCollar",(.46,0,.68),.29,.21,.12,2,"X")
    b.ring("EmitterMouth",(.69,0,.68),.24,.14,.10,0,"X")
    b.beam("EmitterLens",(.69,0,.68),(.705,0,.68),.125,3)
    b.box("SensorRecess",(.39,0,.82),(.045,.35,.05),1)
    b.box("SensorSlit",(.417,0,.82),(.01,.26,.018),3)


def breaker(b):
    for fore in [-1,1]:
        for side in [-1,1]:
            tag = str(fore)+str(side)
            b.beam("UpperLeg"+tag,(fore*.33,side*.32,.50),(fore*.45,side*.67,.30),.145)
            b.joint("Knee"+tag,(fore*.45,side*.67,.30),(.20,.16,.065),2)
            b.beam("LowerLeg"+tag,(fore*.45,side*.67,.28),(fore*.57,side*.64,.09),.125)
            b.box("Foot"+tag,(fore*.57+.04,side*.64,.055),(.41,.33,.11),1)
            b.box("LegArmor"+tag,(fore*.42,side*.58,.405),(.36,.25,.19))
    b.shell("Undercarriage",[(.34,-.06,0,.44,.43),(.47,-.06,0,.58,.55),(.62,-.09,0,.51,.52)],1)
    b.shell("SiegeCarapace",[(.50,-.15,0,.52,.58),(.77,-.20,0,.52,.65),(.86,-.22,0,.35,.50)])
    b.box("DorsalArmor",(-.27,0,.87),(.66,.58,.14))
    for y in [-.20,0,.20]:
        b.box("DorsalVent",(-.25,y,.948),(.43,.085,.015),1)
        b.box("DorsalFlux",(-.25,y,.96),(.32,.035,.011),3)
    for side in [-1,1]:
        b.box("ShoulderCowl",(-.08,side*.54,.74),(.70,.23,.17),0,(side*-.07,0,0))
        b.box("CowlTrim",(.22,side*.54,.73),(.14,.25,.18),2)
        b.beam("Capacitor",(-.43,side*.37,.76),(-.43,side*.37,.965),.11,1)
        b.ring("CapacitorRing",(-.43,side*.37,.94),.135,.09,.08,2)
        b.box("CowlConduit",(-.12,side*.673,.74),(.41,.014,.035),3)
    b.beam("SiegeBore",(.09,0,.58),(.69,0,.58),.28,1)
    b.ring("BreechCollar",(.30,0,.58),.37,.265,.20,2,"X")
    b.ring("MuzzleMantle",(.68,0,.58),.36,.24,.20,0,"X")
    b.ring("MuzzleTrim",(.79,0,.58),.32,.22,.055,2,"X")
    b.beam("DischargeLens",(.70,0,.58),(.715,0,.58),.195,3)
    b.box("SensorHousing",(.35,0,.82),(.16,.45,.10),1)
    b.box("SensorSlit",(.438,0,.82),(.015,.32,.029),3)


def select_only(objects):
    bpy.ops.object.select_all(action="DESELECT")
    for obj in objects:
        obj.select_set(True)
    bpy.context.view_layer.objects.active = objects[0]


def collapse(body):
    rng = random.Random(KIT["collapsed"]["seed"])
    bpy.context.view_layer.update()
    for obj in body.parts:
        # Bake each part into world space, centre it, then lay it down independently.
        vertices = [obj.matrix_world @ v.co for v in obj.data.vertices]
        centre = sum(vertices,Vector())/len(vertices)
        rotation = Euler((rng.uniform(-1.2,1.2),rng.uniform(-1.2,1.2),rng.uniform(-math.pi,math.pi))).to_matrix()
        vertices = [rotation @ (v-centre) for v in vertices]
        bottom = min(v.z for v in vertices)
        angle = rng.uniform(0,math.tau)
        scatter = KIT["collapsed"]["radialScatter"]*body.radius
        offset = Vector((centre.x*.6+math.cos(angle)*scatter,centre.y*.6+math.sin(angle)*scatter,
                         -bottom+rng.uniform(0,KIT["collapsed"]["stackHeightFraction"])*body.height))
        obj.matrix_world = Matrix.Identity(4)
        for v,co in zip(obj.data.vertices,vertices):
            v.co = co+offset
        for p in obj.data.polygons:
            if p.material_index == 3:
                p.material_index = 1


def prepare(body, name):
    select_only(body.parts)
    bpy.ops.object.join()
    obj = bpy.context.object
    obj.name = obj.data.name = name
    SCENE.cursor.location = (0,0,0)
    bpy.ops.object.origin_set(type="ORIGIN_CURSOR")
    bpy.ops.object.transform_apply(location=False,rotation=True,scale=True)
    # A conservative visual footprint makes silhouettes agree with current hit bodies.
    max_radius = max(math.hypot(v.co.x,v.co.y) for v in obj.data.vertices)
    fit = min(1,body.radius*KIT["geometry"]["footprintFill"]/max_radius)
    floor = min(v.co.z for v in obj.data.vertices)
    height = max(v.co.z for v in obj.data.vertices)-floor
    fit_z = min(1,body.height/height)
    for v in obj.data.vertices:
        v.co.x *= fit
        v.co.y *= fit
        v.co.z = (v.co.z-floor)*fit_z
    bevel = obj.modifiers.new("MachinedEdges","BEVEL")
    bevel.width = KIT["geometry"]["bevelWidthMetres"]
    bevel.segments = KIT["geometry"]["bevelSegments"]
    bpy.ops.object.modifier_apply(modifier=bevel.name)
    bm = bmesh.new()
    bm.from_mesh(obj.data)
    bmesh.ops.dissolve_degenerate(bm,dist=1e-6,edges=list(bm.edges))
    bmesh.ops.recalc_face_normals(bm,faces=list(bm.faces))
    bm.to_mesh(obj.data)
    bm.free()
    bpy.ops.object.mode_set(mode="EDIT")
    bpy.ops.mesh.select_all(action="SELECT")
    bpy.ops.uv.smart_project(island_margin=KIT["geometry"]["uvMargin"])
    bpy.ops.object.mode_set(mode="OBJECT")
    obj.data.uv_layers[0].name = "UV0"
    obj.data.uv_layers.new(name="UV1_Lightmap",do_init=True)
    bm = bmesh.new()
    bm.from_mesh(obj.data)
    bmesh.ops.triangulate(bm,faces=list(bm.faces))
    bm.to_mesh(obj.data)
    open_edges = sum(not e.is_manifold for e in bm.edges)
    bm.free()
    obj.data.update()
    # Beveling an angled fallen piece can lift its lowest vertex. Re-seat the
    # completed geometry so a ground pivot means actual contact, for every pose.
    ground = min(v.co.z for v in obj.data.vertices)
    for v in obj.data.vertices:
        v.co.z -= ground
    obj.data.update()
    bpy.context.view_layer.update()
    assert open_edges == 0, (name,"Nonmanifold part geometry",open_edges)
    assert abs(min(v.co.z for v in obj.data.vertices)) < 1e-5
    assert max(math.hypot(v.co.x,v.co.y) for v in obj.data.vertices) <= body.radius+1e-5
    assert all(p.area > 1e-10 for p in obj.data.polygons), name
    assert all(math.isfinite(c) for uv in obj.data.uv_layers for loop in uv.data for c in loop.uv)
    return obj


BUILDERS = {"strider":strider,"spark":spark,"breaker":breaker}
MANIFEST = {"schemaVersion":1,"kitSha256":hashlib.sha256(KIT_BYTES).hexdigest(),
            "blenderVersion":bpy.app.version_string,"units":"centimetres in FBX",
            "footprintsCm":{key:{f:value[f] for f in ("capsuleRadius","capsuleHalfHeight")} for key,value in UNITS.items()},
            "assets":[]}
SOURCE_PARTS = bpy.data.collections.new("EditableSourceParts")
SCENE.collection.children.link(SOURCE_PARTS)
for spec in KIT["assets"]:
    unit = UNITS[spec["id"]]
    for state in ("Active","Collapsed"):
        body = Body(spec["name"],unit["capsuleRadius"]/100,unit["capsuleHalfHeight"]/50*spec["visualHeightFraction"])
        BUILDERS[spec["id"]](body)
        if state == "Collapsed":
            collapse(body)
        else:
            for obj in body.parts:
                copy = obj.copy()
                copy.data = obj.data.copy()
                copy.name = "SOURCE_"+obj.name
                SOURCE_PARTS.objects.link(copy)
        name = "SM_Fluxborn_"+spec["name"]+"_"+state
        obj = prepare(body,name)
        assert len(obj.data.polygons) <= spec["triangleBudget"], (name,len(obj.data.polygons))
        # The facing socket provides a machine-checkable orientation after FBX conversion.
        socket = bpy.data.objects.new("SOCKET_Facing",None)
        SCENE.collection.objects.link(socket)
        socket.parent = obj
        socket.location = (body.radius*.8,0,body.height*.5)
        select_only([obj,socket])
        path = SOURCE/"FBX"/(name+".fbx")
        # Serialize centimetre coordinates with centimetre metadata. This avoids
        # depending on the legacy importer's different new/reimport unit handling.
        for vertex in obj.data.vertices:
            vertex.co *= 100
        socket.location *= 100
        SCENE.unit_settings.scale_length = .01
        obj.data.update()
        bpy.context.view_layer.update()
        bpy.ops.export_scene.fbx(filepath=str(path),use_selection=True,object_types={"MESH","EMPTY"},
                                 apply_unit_scale=True,apply_scale_options="FBX_SCALE_UNITS",
                                 axis_forward="-Y",axis_up="Z",bake_anim=False,mesh_smooth_type="FACE")
        for vertex in obj.data.vertices:
            vertex.co /= 100
        SCENE.unit_settings.scale_length = 1
        obj.data.update()
        bpy.context.view_layer.update()
        bpy.data.objects.remove(socket,do_unlink=True)
        MANIFEST["assets"].append({"name":name,"kind":spec["id"],"role":spec["role"],"state":state,
                                   "file":"FBX/"+path.name,"sha256":hashlib.sha256(path.read_bytes()).hexdigest(),
                                   "triangles":len(obj.data.polygons),"triangleBudget":spec["triangleBudget"],
                                   "dimensionsCm":[round(v*100,3) for v in obj.dimensions],
                                   "groundPivot":True,"uvChannels":2,"forwardSocket":"Facing",
                                   "materialSlots":[m.name for m in obj.data.materials]})
        obj.location = (KIT["preview"]["collapsedRowX"] if state == "Collapsed" else 0,spec["previewY"],0)
SOURCE_PARTS.hide_render = True
SOURCE_PARTS.hide_viewport = True
(SOURCE/"manifest.json").write_text(json.dumps(MANIFEST,indent=2)+"\n")


def aim(obj,point):
    obj.rotation_euler = (Vector(point)-obj.location).to_track_quat("-Z","Y").to_euler()


ground_mat = bpy.data.materials.new("PreviewGround")
ground_mat.diffuse_color = (.12,.15,.19,1)
ground_mat.use_nodes = True
ground_mat.node_tree.nodes.get("Principled BSDF").inputs["Base Color"].default_value = (.12,.15,.19,1)
bpy.ops.mesh.primitive_plane_add(size=200,location=(0,0,-.01))
bpy.context.object.name = "PreviewGround"
bpy.context.object.data.materials.append(ground_mat)
for name,loc,power,size,color in [
    ("Key",(4,-3,6),550,5,(1,.90,.76)),
    ("Fill",(1,4,4),350,4,(.55,.72,1)),
    ("Rim",(-4,0,5),650,4,(.70,.82,1))]:
    data = bpy.data.lights.new(name,"AREA")
    data.energy,data.shape,data.size,data.color = power,"DISK",size,color
    obj = bpy.data.objects.new(name,data)
    SCENE.collection.objects.link(obj)
    obj.location = loc
    aim(obj,(-.8,0,.4))
data = bpy.data.cameras.new("FluxbornReview")
camera = bpy.data.objects.new("FluxbornReview",data)
SCENE.collection.objects.link(camera)
camera.location = (7,-4,6)
aim(camera,(-.9,0,.40))
data.type = "ORTHO"
data.ortho_scale = KIT["preview"]["orthoScale"]
SCENE.camera = camera
SCENE.render.engine = "CYCLES"
SCENE.cycles.samples = KIT["preview"]["samples"]
SCENE.cycles.use_denoising = True
SCENE.render.resolution_x = KIT["preview"]["width"]
SCENE.render.resolution_y = KIT["preview"]["height"]
SCENE.render.resolution_percentage = 100
SCENE.world.color = (.18,.18,.18)
SCENE.render.image_settings.file_format = "PNG"
SCENE.render.filepath = str(SAVED/"FluxbornKit.png")
bpy.ops.wm.save_as_mainfile(filepath=str(SAVED/"FluxbornKit.blend"))
if "--skip-render" not in sys.argv:
    bpy.ops.render.render(write_still=True)
print("VEYRA_FLUXBORN_GENERATION_PASSED: " + str(len(MANIFEST["assets"])) + " meshes")
