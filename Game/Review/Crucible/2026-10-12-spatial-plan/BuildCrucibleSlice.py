"""ADR-073 §6 prototype slice: builds a throwaway editor level from Game/Saved/CrucibleSlice/slice.json and captures it.

Run through CaptureCrucibleSlice.ps1 (editor, -ExecutePythonScript, GeometryScripting enabled on the command line).
Masses are Geometry Script extrusions of their stepped layer polygons, flat-shaded with the kit's mossy stone; the floor
is a vertex-coloured grid; keep walls and gates are solids in the ruin stone; structures, trees and a Vanguard are the
real assets. Nothing is saved: the level and its meshes live only for the capture.
"""
import json
import time
import traceback
from pathlib import Path
import sys

import unreal

unreal.EditorPythonScripting.set_keep_python_script_alive(True)

GAME = Path(unreal.Paths.project_dir()).resolve()
sys.path.insert(0, str(GAME / "Scripts"))
from veyra_material_graph import EDIT, Graph, world_aligned  # noqa: E402

ROOT = GAME / "Saved" / "CrucibleSlice"
DATA = json.loads((ROOT / "slice.json").read_text(encoding="utf-8"))
STYLE = json.loads((GAME / "Plugins" / "VeyraWorldTools" / "Config" / "CrucibleStyle.json").read_text(encoding="utf-8"))
OUTPUT = ROOT / "Captures"
OUTPUT.mkdir(parents=True, exist_ok=True)
for old in OUTPUT.glob("*.png"):
    old.unlink()
FOLDER = "/Game/Veyra/Developer/CrucibleSlice"
MAP = FOLDER + f"/L_CrucibleSlice_{int(time.time())}"  # a fresh name per run, so an earlier run's empty level never blocks it

ASSETS = unreal.EditorAssetLibrary
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
LEVEL = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
ACTORS = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
# Geometry Script's libraries go by their ScriptName in Python. Looked up inside the guarded build step, so a missing
# plugin is reported in failure.txt and the editor quits instead of waiting forever.
PRIM = LISTS = EDITS = COLOURS = NORMALS = None


def geometry_script():
    global PRIM, LISTS, EDITS, COLOURS, NORMALS
    PRIM = unreal.GeometryScript_Primitives
    LISTS = unreal.GeometryScript_List
    EDITS = unreal.GeometryScript_MeshEdits
    COLOURS = unreal.GeometryScript_VertexColors
    NORMALS = unreal.GeometryScript_Normals


def log(text):
    unreal.log("VEYRA_SLICE: " + text)


def fresh_level():
    assert LEVEL.new_level(MAP), "Could not create the slice level"
    LEVEL.editor_set_game_view(True)


def colour(values):
    return unreal.LinearColor(values[0], values[1], values[2], 1.0)


def light():
    look = dict(STYLE["lighting"], **DATA.get("lighting", {}))  # the slice may test overrides of the style's light
    sun = ACTORS.spawn_actor_from_class(unreal.DirectionalLight, unreal.Vector(0, 0, 0), unreal.Rotator(0, look["sunPitch"], look["sunYaw"]))
    component = sun.light_component
    component.set_mobility(unreal.ComponentMobility.MOVABLE)
    component.set_intensity(look["sunLux"])
    component.set_editor_property("use_temperature", True)
    component.set_editor_property("temperature", look["sunTemperature"])
    component.set_editor_property("atmosphere_sun_light", True)
    component.set_editor_property("dynamic_shadow_distance_movable_light", look["shadowDistance"])
    ACTORS.spawn_actor_from_class(unreal.SkyAtmosphere, unreal.Vector(0, 0, 0))
    sky = ACTORS.spawn_actor_from_class(unreal.SkyLight, unreal.Vector(0, 0, 0))
    sky.light_component.set_mobility(unreal.ComponentMobility.MOVABLE)
    sky.light_component.set_intensity(look["skyIntensity"])
    sky.light_component.set_editor_property("real_time_capture", True)
    haze = ACTORS.spawn_actor_from_class(unreal.ExponentialHeightFog, unreal.Vector(0, 0, look["hazeBaseZ"]))
    fog = haze.component
    fog.set_fog_density(look["hazeDensity"])
    fog.set_fog_height_falloff(look["hazeFalloff"])
    fog.set_start_distance(look["hazeStartDistance"])
    fog.set_fog_inscattering_color(colour(look["hazeColour"]))
    fog.set_directional_inscattering_color(colour(look["hazeSunColour"]))
    fog.set_volumetric_fog(False)
    grade = ACTORS.spawn_actor_from_class(unreal.PostProcessVolume, unreal.Vector(0, 0, 0))
    grade.set_editor_property("unbound", True)
    s = grade.settings
    s.override_auto_exposure_method = True
    s.auto_exposure_method = unreal.AutoExposureMethod.AEM_MANUAL
    s.override_auto_exposure_apply_physical_camera_exposure = True
    s.auto_exposure_apply_physical_camera_exposure = False
    s.override_auto_exposure_bias = True
    s.auto_exposure_bias = -look["exposureEV100"]
    s.override_color_saturation = True
    s.color_saturation = unreal.Vector4(1, 1, 1, look["saturation"])
    s.override_color_contrast = True
    s.color_contrast = unreal.Vector4(1, 1, 1, look["contrast"])
    s.override_color_gain = True
    s.color_gain = unreal.Vector4(look["gain"][0], look["gain"][1], look["gain"][2], 1)
    s.override_bloom_intensity = True
    s.bloom_intensity = look["bloom"]
    s.override_vignette_intensity = True
    s.vignette_intensity = look["vignette"]
    s.override_ambient_occlusion_intensity = True
    s.ambient_occlusion_intensity = look["ambientOcclusion"]
    grade.settings = s


def texture(name):
    asset = unreal.load_asset(f"/Game/Veyra/World/Environment/Terrain/Textures/T_Crucible_{name}")
    assert asset, name
    return asset


def floor_material():
    """Vertex colour picks the ground: R causeway, G moss, B camp earth, A shore. World-aligned, so the grid needs no UVs."""
    path = FOLDER + "/M_SliceFloor"
    if ASSETS.does_asset_exist(path):
        ASSETS.delete_asset(path)
    material = TOOLS.create_asset("M_SliceFloor", FOLDER, unreal.Material, unreal.MaterialFactoryNew())
    material.set_editor_property("two_sided", True)
    g = Graph(material)
    weights = g.node(unreal.MaterialExpressionVertexColor, 1)
    layers = []
    for name, channel, tile, tint in (("Causeway", "R", 380, (1.0, 1.0, 1.0)), ("Moss", "G", 520, (1.0, 1.0, 1.0)),
                                      ("Shore", "B", 420, (0.78, 0.58, 0.42)), ("Shore", "A", 420, (1.0, 1.0, 1.0))):
        call, out = world_aligned(g, 3, "WorldAlignedTexture", texture(name + "_BaseColor"), tile, "XYZ Texture")
        tinted = g.op(unreal.MaterialExpressionMultiply, 4, call, g.node(unreal.MaterialExpressionConstant3Vector, 3, constant=unreal.LinearColor(*tint, 1)), a_out=out)
        weighted = g.node(unreal.MaterialExpressionMultiply, 5)
        g.link(tinted, "", weighted, "A")
        g.link(weights, channel, weighted, "B")
        layers.append(weighted)
    total = layers[0]
    for layer in layers[1:]:
        total = g.op(unreal.MaterialExpressionAdd, 6, total, layer)
    EDIT.connect_material_property(total, "", unreal.MaterialProperty.MP_BASE_COLOR)
    EDIT.connect_material_property(g.node(unreal.MaterialExpressionConstant, 6, r=0.9), "", unreal.MaterialProperty.MP_ROUGHNESS)
    EDIT.recompile_material(material)
    return material


def water_material():
    water = unreal.load_asset("/Game/Veyra/World/Environment/Terrain/M_CrucibleWater")
    return water


def mesh_actor(label, material):
    actor = ACTORS.spawn_actor_from_class(unreal.DynamicMeshActor, unreal.Vector(0, 0, 0))
    actor.set_actor_label(label)
    component = actor.dynamic_mesh_component
    component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
    return actor, component, component.get_dynamic_mesh(), material


def finish(component, mesh, material, flat=True):
    if flat:
        NORMALS.set_per_face_normals(mesh)
    else:
        NORMALS.recompute_normals(mesh, unreal.GeometryScriptCalculateNormalsOptions())
    component.set_material(0, material)


def extrude(mesh, points, bottom, top):
    polygon = [unreal.Vector2D(p[0], p[1]) for p in points]
    transform = unreal.Transform(location=unreal.Vector(0, 0, bottom))
    PRIM.append_simple_extrude_polygon(mesh, unreal.GeometryScriptPrimitiveOptions(), transform, polygon,
                                       float(top - bottom), 0, True, unreal.GeometryScriptPrimitiveOriginMode.BASE)


def build_floor():
    floor = DATA["floor"]
    actor, component, mesh, _ = mesh_actor("Slice_Floor", None)
    vectors = LISTS.convert_array_to_vector_list([unreal.Vector(*v) for v in floor["vertices"]])
    EDITS.add_vertices_to_mesh(mesh, vectors)
    triangles = LISTS.convert_array_to_triangle_list([unreal.IntVector(*t) for t in floor["triangles"]])
    EDITS.add_triangles_to_mesh(mesh, triangles, 0)
    colours = LISTS.convert_array_to_color_list([unreal.LinearColor(*c) for c in floor["colours"]])
    COLOURS.set_mesh_per_vertex_colors(mesh, colours)
    finish(component, mesh, floor_material(), flat=False)


def build_water():
    actor, component, mesh, _ = mesh_actor("Slice_Water", None)
    z = DATA["water"]["z"]
    for points in DATA["water"]["polygons"]:
        PRIM.append_triangulated_polygon(mesh, unreal.GeometryScriptPrimitiveOptions(), unreal.Transform(location=unreal.Vector(0, 0, z)),
                                         [unreal.Vector2D(p[0], p[1]) for p in points], True)
    finish(component, mesh, water_material(), flat=False)


# Each mass kind's look over the kit's mossy stone: blocked ground must never share the walkable floor's moss.
# Rock and cliff tops are bare slate (moss pushed past any normal's reach); forest tops keep moss, hidden by canopy.
MASS_LOOKS = {
    "cliff": {"MossFrom": 1.1, "MossTo": 1.2, "StoneTint": (1.55, 1.62, 1.75)},
    "rock": {"MossFrom": 1.1, "MossTo": 1.2, "StoneTint": (1.85, 1.9, 2.0)},
    "forest": {"MossFrom": 0.55, "MossTo": 0.82, "StoneTint": (1.4, 1.45, 1.5)},
}


def mass_material(kind):
    name = f"MI_SliceMass_{kind}"
    instance = TOOLS.create_asset(name, FOLDER, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    instance.set_editor_property("parent", unreal.load_asset("/Game/Veyra/World/Environment/Materials/M_CrucibleRock"))
    look = MASS_LOOKS[kind]
    EDIT.set_material_instance_scalar_parameter_value(instance, "MossFrom", look["MossFrom"])
    EDIT.set_material_instance_scalar_parameter_value(instance, "MossTo", look["MossTo"])
    EDIT.set_material_instance_vector_parameter_value(instance, "StoneTint", unreal.LinearColor(*look["StoneTint"], 1.0))
    EDIT.update_material_instance(instance)
    return instance


def build_masses():
    materials = {kind: mass_material(kind) for kind in MASS_LOOKS}
    for mass in DATA["masses"]:
        material = materials[mass["kind"]]
        actor, component, mesh, _ = mesh_actor(f"Mass_{mass['id']}_{mass['kind']}", material)
        for layer in mass["layers"]:
            extrude(mesh, layer["points"], layer["bottom"], layer["top"])
        finish(component, mesh, material)


def build_solids():
    ruin = unreal.load_asset("/Game/Veyra/World/Environment/Materials/M_CrucibleRuin")
    actor, component, mesh, _ = mesh_actor("Keep_Architecture", ruin)
    for solid in DATA["solids"]:
        extrude(mesh, solid["points"], solid["bottom"], solid["top"])
    finish(component, mesh, ruin)


def static_mesh(path):
    asset = unreal.load_asset(path)
    assert asset, path
    return asset


def place(mesh, location, yaw=0.0, scale=1.0, label=None):
    actor = ACTORS.spawn_actor_from_object(mesh, unreal.Vector(*location), unreal.Rotator(0, 0, yaw))
    actor.set_actor_scale3d(unreal.Vector(scale, scale, scale))
    if label:
        actor.set_actor_label(label)
    return actor


def build_structures():
    meshes = {kind: static_mesh(f"/Game/Veyra/World/Structures/Greybox/Meshes/SM_{kind}_Standing")
              for kind in ("PrimeWell", "BaseTower", "Inhibitor", "LaneSpire")}
    for index, s in enumerate(DATA["structures"]):
        z = 70 if s["kind"] in ("PrimeWell", "BaseTower", "Inhibitor") else 0
        place(meshes[s["kind"]], (s["at"][0], s["at"][1], z), label=f"{s['kind']}_{index}")


def height_of(mesh):
    return max(1.0, mesh.get_bounds().box_extent.z * 2.0)


def build_trees():
    found = [a for a in ASSETS.list_assets("/Game/Veyra/World/Environment/Meshes", recursive=True, include_folder=False)]
    trees = [static_mesh(a.split(".")[0]) for a in found if "SM_Crucible_Tree" in a]
    shrubs = [static_mesh(a.split(".")[0]) for a in found if "SM_Crucible_Shrub" in a]
    assert trees and shrubs, "kit trees and shrubs"
    target = {"tree": 620.0, "shrub": 230.0}
    count = 0
    for mass in DATA["masses"]:
        for i, t in enumerate(mass["trees"]):
            family = trees if t["kind"] == "tree" else shrubs
            mesh = family[i % len(family)]
            scale = target[t["kind"]] / height_of(mesh) * t["scale"]
            place(mesh, t["at"], t["yaw"], scale)
            count += 1
    log(f"{count} trees and shrubs")


def ink_material():
    """The toon ink: near-black, matte, so it reads as a drawn line under any light."""
    material = TOOLS.create_asset("M_SliceInk", FOLDER, unreal.Material, unreal.MaterialFactoryNew())
    g = Graph(material)
    EDIT.connect_material_property(g.node(unreal.MaterialExpressionConstant3Vector, 1, constant=unreal.LinearColor(0.018, 0.026, 0.024, 1.0)), "", unreal.MaterialProperty.MP_BASE_COLOR)
    EDIT.connect_material_property(g.node(unreal.MaterialExpressionConstant, 1, r=1.0), "", unreal.MaterialProperty.MP_ROUGHNESS)
    EDIT.connect_material_property(g.node(unreal.MaterialExpressionConstant, 1, r=0.0), "", unreal.MaterialProperty.MP_SPECULAR)
    EDIT.recompile_material(material)
    return material


def build_inks():
    """A square tube swept along each ink path: the line where a mass meets the floor, and its top lip."""
    material = ink_material()
    actor, component, mesh, _ = mesh_actor("Slice_Ink", material)
    for ink in DATA.get("inks", []):
        points = ink["points"]
        if len(points) > 2 and points[0] == points[-1]:
            points = points[:-1]
        s = ink["size"] / 2.0
        profile = [unreal.Vector2D(-s, -s), unreal.Vector2D(s, -s), unreal.Vector2D(s, s), unreal.Vector2D(-s, s)]
        PRIM.append_simple_swept_polygon(mesh, unreal.GeometryScriptPrimitiveOptions(), unreal.Transform(), profile,
                                         [unreal.Vector(*p) for p in points], True, False, 1.0, 1.0, 0.0, 1.0)
    finish(component, mesh, material)


def build_clutter():
    found = ASSETS.list_assets("/Game/Veyra/World/Environment/Meshes", recursive=True, include_folder=False)
    families = {kind: [static_mesh(a.split(".")[0]) for a in found if f"SM_Crucible_{name}" in a]
                for kind, name in (("grass", "Grass"), ("fern", "Fern"), ("reeds", "Reeds"))}
    target = {"grass": 70.0, "fern": 85.0, "reeds": 150.0}
    for i, c in enumerate(DATA.get("clutter", [])):
        family = families[c["kind"]]
        mesh = family[i % len(family)]
        place(mesh, c["at"], c["yaw"], target[c["kind"]] / height_of(mesh) * c["scale"])
    log(f"{len(DATA.get('clutter', []))} clutter pieces")

def build_scale_figures():
    body = unreal.load_asset("/Game/Veyra/Vanguards/Cairn/SK_Cairn")
    if not body:
        return
    for name, target in (("Scale_Cairn_Crossroads", "TopCrossroads"), ("Scale_Cairn_Gate", "Gate_Top"), ("Scale_Cairn_Lane", "TopLane_Mid")):
        cam = next(c for c in DATA["cameras"] if c["name"] == target)
        actor = ACTORS.spawn_actor_from_class(unreal.SkeletalMeshActor, unreal.Vector(cam["target"][0], cam["target"][1], 50))
        actor.set_actor_label(name)
        component = actor.skeletal_mesh_component
        try:
            component.set_skinned_asset_and_update(body)
        except AttributeError:
            component.set_editor_property("skeletal_mesh_asset", body)
        actor.set_actor_scale3d(unreal.Vector(1.6, 1.6, 1.6))


def build_cameras():
    camera = DATA["camera"]
    import math
    pitch = math.radians(camera["pitch"])
    back = camera["distance"] * math.cos(pitch)
    up = -camera["distance"] * math.sin(pitch)
    cameras = []
    for spec in DATA["cameras"]:
        x, y = spec["target"]
        if spec["mode"] == "overview":
            location, rotation, fov = unreal.Vector(x, y, 17000), unreal.Rotator(0, -90, 0), 55.0
        else:
            location, rotation, fov = unreal.Vector(x - back, y, 50 + up), unreal.Rotator(0, camera["pitch"], 0), float(camera["fov"])
        actor = ACTORS.spawn_actor_from_class(unreal.CameraActor, location, rotation)
        actor.set_actor_label("Slice_" + spec["name"])
        actor.camera_component.set_editor_property("field_of_view", fov)
        actor.camera_component.set_editor_property("constrain_aspect_ratio", False)
        cameras.append(actor)
    return cameras


STATE = {"phase": "build", "index": 0, "pass": 0, "task": None, "next": 0.0, "started": time.monotonic(), "captures": [], "compiling": False}
CAMERAS = []
WORLD = None


def tick(delta):
    global CAMERAS, WORLD
    if STATE["compiling"]:
        return
    try:
        if time.monotonic() - STATE["started"] > 1500:
            raise RuntimeError("Slice capture timed out")
        if STATE["phase"] == "build":
            STATE["phase"] = "building"
            geometry_script()
            fresh_level()
            WORLD = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
            for name in ("ViewDistance", "AntiAliasing", "Shadow", "GlobalIllumination", "Reflection", "PostProcess", "Texture", "Effects", "Foliage", "Shading"):
                unreal.SystemLibrary.execute_console_command(WORLD, f"sg.{name}Quality 2")
            unreal.SystemLibrary.execute_console_command(WORLD, "r.ScreenPercentage 100")
            light()
            log("light"); build_floor()
            log("floor"); build_water()
            log("water"); build_masses()
            log("masses"); build_solids()
            log("keep"); build_structures()
            log("structures"); build_trees()
            build_inks(); log("inks")
            build_clutter()
            build_scale_figures()
            CAMERAS = build_cameras()
            STATE["phase"] = "capture"
            STATE["next"] = time.monotonic() + 20
            return
        if STATE["phase"] != "capture" or time.monotonic() < STATE["next"]:
            return
        if STATE["task"] and not Path(STATE["task"]).is_file():
            return
        STATE["task"] = None
        if STATE["index"] >= len(CAMERAS):
            (OUTPUT / "manifest.json").write_text(json.dumps({"status": "captured", "captures": STATE["captures"]}, indent=2), encoding="utf-8")
            unreal.unregister_slate_post_tick_callback(HANDLE)
            unreal.SystemLibrary.quit_editor()
            return
        camera = CAMERAS[STATE["index"]]
        if STATE["pass"] < 2:
            if STATE["pass"] == 0:
                LEVEL.pilot_level_actor(camera)
            STATE["compiling"] = True
            try:
                unreal.SystemLibrary.execute_console_command(WORLD, "Editor.AsyncAssetCompilationFinishAll")
            finally:
                STATE["compiling"] = False
            STATE["pass"] += 1
            STATE["next"] = time.monotonic() + 5
            return
        filename = OUTPUT / (camera.get_actor_label() + ".png")
        unreal.SystemLibrary.execute_console_command(WORLD, f'HighResShot 1920x1080 filename="{filename.as_posix()}"')
        STATE["task"] = str(filename)
        STATE["captures"].append({"camera": camera.get_actor_label(), "file": str(filename)})
        STATE["index"] += 1
        STATE["pass"] = 0
        STATE["next"] = time.monotonic() + 4
    except Exception:
        (OUTPUT / "failure.txt").write_text(traceback.format_exc(), encoding="utf-8")
        unreal.unregister_slate_post_tick_callback(HANDLE)
        unreal.SystemLibrary.quit_editor()


HANDLE = unreal.register_slate_post_tick_callback(tick)
