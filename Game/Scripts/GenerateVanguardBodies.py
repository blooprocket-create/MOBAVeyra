"""Generate the Vanguards' rigged, animated first-pass bodies (ADR-064) from ArtSource/Vanguards/VanguardKit.json.

Run in an isolated background Blender (5.2 LTS):
    blender --background --factory-startup --python-exit-code 1 --python GenerateVanguardBodies.py [-- --only id,id]
        [--preview [--preview-dir path]]
BuildVanguardBodies.ps1 runs it, then imports. Each Vanguard's archetype (the VanguardBodies package: humanoid,
colossus, beast, construct) builds its skeleton fitted to the capsule in Game/Tuning/Vanguards.json, a body of simple
parts rigidly weighted to their bones, coloured through vertex colours (alpha marks what glows), and its animations
generated on that skeleton. One FBX per Vanguard goes to ArtSource/Vanguards/FBX, with a manifest of their hashes. The
body faces +X with its root at the ground. Nothing here decides gameplay: the capsule stays the only collision and
movement.
"""
import hashlib
import json
import math
import random
import sys
from pathlib import Path

import bpy
from mathutils import Vector

sys.path.insert(0, str(Path(__file__).resolve().parent))
from VanguardBodies import beast, colossus, construct, humanoid, rider  # noqa: E402
from VanguardBodies.parts import local  # noqa: E402

GENERATOR_VERSION = 1
GAME = Path(__file__).resolve().parents[1]
SOURCE = GAME / "ArtSource" / "Vanguards"
SAVED = GAME / "Saved" / "VanguardKit"
KIT_BYTES = (SOURCE / "VanguardKit.json").read_bytes()
KIT = json.loads(KIT_BYTES)
TUNING = json.loads((GAME / "Tuning" / "Vanguards.json").read_bytes())["vanguards"]
ARCHETYPES = {"humanoid": humanoid, "colossus": colossus, "beast": beast, "construct": construct, "rider": rider}

if not bpy.app.background:
    raise RuntimeError("Run in an isolated background Blender process.")
assert KIT["schemaVersion"] == 1, "Unknown kit schema"
assert KIT["generatorVersion"] == GENERATOR_VERSION, "The kit was written for another generator version"

ARGS = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
ONLY = set(ARGS[ARGS.index("--only") + 1].split(",")) if "--only" in ARGS else None
PREVIEW = "--preview" in ARGS
PREVIEW_DIR = Path(ARGS[ARGS.index("--preview-dir") + 1]) if "--preview-dir" in ARGS else SAVED / "Preview"
# The poses a preview lines up, as (animation, time from 0 to 1).
PREVIEW_POSES = [("Idle", 0.0), ("Run", 0.25), ("Run", 0.75), ("AttackWindup", 0.65), ("AttackWindup", 1.0),
                 ("Cast", 0.5), ("Hit", 0.5), ("Recall", 0.5), ("Death", 1.0)]
# The gameplay camera's look down from the horizontal, in degrees (DefaultGame.ini, VeyraCameraSettings.PitchDegrees).
GAMEPLAY_PITCH = 60.0
# How far below the ground a body's lowest point may reach, in centimetres: the slack Veyra.UI.VanguardBodies allows.
GROUND_SLACK = 2.0


# ---------------------------------------------------------------------------------------------- the skeleton
def build_armature(name, bones, layout):
    data = bpy.data.armatures.new(name + "_Skeleton")
    armature = bpy.data.objects.new("Armature", data)
    bpy.context.scene.collection.objects.link(armature)
    bpy.context.view_layer.objects.active = armature
    bpy.ops.object.mode_set(mode="EDIT")
    for bone_name, parent in bones:
        bone = data.edit_bones.new(bone_name)
        bone.head, bone.tail = layout[bone_name]
        bone.roll = 0.0
        if parent:
            bone.parent = data.edit_bones[parent]
            bone.use_connect = False
    bpy.ops.object.mode_set(mode="OBJECT")
    return armature


# ---------------------------------------------------------------------------------------------- the animations
def rest_quaternions(armature):
    return {bone.name: bone.matrix_local.to_quaternion() for bone in armature.data.bones}


def moved_bones(archetype):
    """The bones an archetype's poses may move as well as turn: its lift bone, or the bones it names."""
    return getattr(archetype, "MOVED_BONES", (archetype.LIFT_BONE,))


def pose_rig(rig, rest, archetype, pose, lift):
    """Poses every bone: its rotation about the armature's axes, and its moved bones' offsets about them. A pose's lift
    is the lift bone's rise, or each moved bone's offset by name (a rider's mount and the rider it throws)."""
    moved = lift if isinstance(lift, dict) else {archetype.LIFT_BONE: (0.0, 0.0, lift)}
    for bone in rig.pose.bones:
        bone.rotation_mode = "QUATERNION"
        bone.rotation_quaternion = local(rest[bone.name], pose.get(bone.name, (0.0, 0.0, 0.0)))
        if bone.name in moved_bones(archetype):
            bone.location = rest[bone.name].inverted() @ Vector(moved.get(bone.name, (0.0, 0.0, 0.0)))


def animate(armature, spec, archetype, d, melee):
    fps = KIT["fps"]
    rest = rest_quaternions(armature)
    armature.animation_data_create()
    actions = []
    for name, clip in KIT["archetypes"][spec["archetype"]]["animations"].items():
        action = bpy.data.actions.new(name)
        armature.animation_data.action = action
        frames = max(2, round(clip["seconds"] * fps))
        for frame in range(frames + 1):
            # A loop's last frame repeats its first.
            t = (frame % frames) / frames if clip["loop"] else frame / frames
            pose_rig(armature, rest, archetype, *archetype.pose(name, t, melee, d))
            for bone in armature.pose.bones:
                bone.keyframe_insert("rotation_quaternion", frame=frame + 1)
                if bone.name in moved_bones(archetype):
                    bone.keyframe_insert("location", frame=frame + 1)
        action.use_fake_user = True
        actions.append({"name": name, "frames": frames + 1, "loop": clip["loop"]})
    armature.animation_data.action = None
    pose_rig(armature, rest, archetype, {}, 0.0)
    return actions


def render_preview(name, armature, obj, archetype, d, melee):
    """Rows of the body in its animations' key poses, left to right, for review (not exported): one row turned
    three-quarters toward the camera, one in profile, and one seen from the gameplay camera's pitch, as players see it."""
    scene = bpy.context.scene
    rest = rest_quaternions(armature)
    # Framed on the body as built, so whatever towers over or reaches past its figure (a mount, a manifested spirit)
    # stays in frame.
    tall = max(d["full"], obj.dimensions.z)
    span = max(tall, d.get("length", 0.0), obj.dimensions.x)
    gap = span * (0.9 if span == tall else 1.15)
    d = dict(d, full=tall)
    armature.hide_render = obj.hide_render = True
    scene.render.engine = "BLENDER_WORKBENCH"
    scene.display.shading.light = "STUDIO"
    scene.display.shading.color_type = "VERTEX"
    scene.display.shading.show_object_outline = True
    scene.world.color = (0.08, 0.09, 0.11)
    camera = bpy.data.objects.new("PreviewCamera", bpy.data.cameras.new("PreviewCamera"))
    scene.collection.objects.link(camera)
    scene.camera = camera
    camera.data.type = "ORTHO"
    camera.data.ortho_scale = len(PREVIEW_POSES) * gap
    camera.data.clip_end = span * 100
    # The camera looks back along -X, so +Y is its right.
    center = Vector((0.0, (len(PREVIEW_POSES) - 1) * gap / 2, d["full"] * 0.5))
    scene.render.resolution_x = 2400
    PREVIEW_DIR.mkdir(parents=True, exist_ok=True)
    for suffix, turn, elevation in (("", 35.0, math.degrees(math.atan2(3, 20))), ("_side", 90.0, math.degrees(math.atan2(3, 20))),
                                    ("_game", 35.0, GAMEPLAY_PITCH)):
        up = math.radians(elevation)
        camera.location = center + Vector((math.cos(up), 0.0, math.sin(up))) * span * 20
        camera.rotation_euler = (center - camera.location).to_track_quat("-Z", "Y").to_euler()
        # Tall enough for the body's height and, seen from above, its depth.
        extent = d["full"] * math.cos(up) + span * math.sin(up)
        scene.render.resolution_y = round(2400 * max(d["full"], extent) * 1.3 / camera.data.ortho_scale)
        made = []
        for index, (clip, t) in enumerate(PREVIEW_POSES):
            rig = armature.copy()
            rig.animation_data_clear()
            scene.collection.objects.link(rig)
            body = obj.copy()
            scene.collection.objects.link(body)
            body.parent = rig
            body.modifiers["Armature"].object = rig
            rig.location = (0.0, index * gap, 0.0)
            rig.rotation_euler = (0.0, 0.0, math.radians(turn))
            # Copies of the hidden originals start hidden too.
            rig.hide_render = body.hide_render = False
            pose_rig(rig, rest, archetype, *archetype.pose(clip, t, melee, d))
            made += [rig, body]
        scene.render.filepath = str(PREVIEW_DIR / (name + suffix + ".png"))
        bpy.ops.render.render(write_still=True)
        for made_object in made:
            bpy.data.objects.remove(made_object)
    bpy.data.objects.remove(camera)
    armature.hide_render = obj.hide_render = False


# ---------------------------------------------------------------------------------------------- the run
def reset_scene():
    bpy.ops.object.select_all(action="SELECT")
    bpy.ops.object.delete(use_global=False)
    for collection in (bpy.data.meshes, bpy.data.armatures, bpy.data.actions, bpy.data.materials):
        for block in list(collection):
            collection.remove(block)
    scene = bpy.context.scene
    scene.unit_settings.system = "METRIC"
    # Centimetres, as Unreal's units: built in centimetres and exported one to one.
    scene.unit_settings.scale_length = 0.01
    scene.render.fps = KIT["fps"]


def bodies_of(spec):
    """A Vanguard's bodies, as (spec, status, name suffix): its own, then each it wears while it holds a status, built
    from its own entry with the status body's entries over it (a rider's ride, ADR-064 §1)."""
    yield spec, None, ""
    for status_body in spec.get("statusBodies", []):
        yield dict(spec, **status_body["body"]), status_body["status"], "_" + status_body["name"]


def build(spec, status=None, suffix=""):
    random.seed(spec["seed"])
    archetype = ARCHETYPES[spec["archetype"]]
    capsule = TUNING[spec["id"]]["body"]
    melee = not TUNING[spec["id"]]["basicAttack"].get("projectile")
    reset_scene()
    layout, dims = archetype.layout(spec, capsule)
    name = "SK_" + spec["id"].title().replace("_", "") + suffix
    armature = build_armature(name, archetype.BONES, layout)
    body = archetype.body(spec, layout, dims)
    mesh = bpy.data.meshes.new(name)
    body.bm.to_mesh(mesh)
    triangles = sum(len(face.verts) - 2 for face in body.bm.faces)
    body.bm.free()
    # The exporter writes the active colour attribute: the body's colours, and in alpha what glows.
    mesh.color_attributes.active_color = mesh.color_attributes["Col"]
    obj = bpy.data.objects.new(name, mesh)
    bpy.context.scene.collection.objects.link(obj)
    for bone_name, _ in archetype.BONES:
        obj.vertex_groups.new(name=bone_name)
    material = bpy.data.materials.new("M_VeyraVanguardBody")
    mesh.materials.append(material)
    obj.parent = armature
    modifier = obj.modifiers.new("Armature", "ARMATURE")
    modifier.object = armature
    actions = animate(armature, spec, archetype, dims, melee)
    budget = KIT["archetypes"][spec["archetype"]]["triangleBudget"]
    assert triangles <= budget, (spec["id"], "triangle budget", triangles, budget)
    # Measured on the rest geometry, as Unreal imports it. Nothing sinks below the ground, and a body that walks stands
    # on it; one that floats hovers clear of it.
    heights = [vertex.co.z for vertex in mesh.vertices]
    height = max(heights) - min(heights)
    assert min(heights) > -GROUND_SLACK, (spec["id"], "the body sinks below the ground", min(heights))
    assert not archetype.GROUNDED or min(heights) < dims["full"] * 0.02, (spec["id"], "the body does not stand on the ground", min(heights))
    (SOURCE / "FBX").mkdir(parents=True, exist_ok=True)
    path = SOURCE / "FBX" / (name + ".fbx")
    bpy.ops.object.select_all(action="DESELECT")
    armature.select_set(True)
    obj.select_set(True)
    bpy.context.view_layer.objects.active = armature
    bpy.ops.export_scene.fbx(filepath=str(path), use_selection=True, object_types={"ARMATURE", "MESH"},
                             apply_unit_scale=True, apply_scale_options="FBX_SCALE_UNITS", axis_forward="-Y", axis_up="Z",
                             add_leaf_bones=False, primary_bone_axis="Y", secondary_bone_axis="X", use_armature_deform_only=False,
                             bake_anim=True, bake_anim_use_all_actions=True, bake_anim_use_nla_strips=False,
                             bake_anim_force_startend_keying=True, bake_anim_simplify_factor=0.0, mesh_smooth_type="FACE",
                             # Unreal's materials read vertex colours as linear, so they go out linear.
                             colors_type="LINEAR")
    if PREVIEW:
        render_preview(name, armature, obj, archetype, dims, melee)
    asset = {"id": spec["id"], "name": name, "archetype": spec["archetype"], "file": "FBX/" + path.name,
             "sha256": hashlib.sha256(path.read_bytes()).hexdigest(), "triangles": triangles, "triangleBudget": budget,
             "bones": len(archetype.BONES), "heightCm": round(height, 2), "capsuleHalfHeightCm": capsule["capsuleHalfHeight"],
             "melee": melee, "runStrideCm": round(archetype.run_stride(dims), 2), "upperBodyBone": archetype.UPPER_BODY_BONE,
             "castReleaseShare": archetype.CAST_RELEASE_SHARE, "animations": actions}
    if status:
        asset["status"] = status
    return asset


def main():
    assert set(KIT["archetypes"]) <= set(ARCHETYPES), ("Unknown archetypes", set(KIT["archetypes"]) - set(ARCHETYPES))
    entries = [spec for spec in KIT["vanguards"] if ONLY is None or spec["id"] in ONLY]
    assert ONLY is None or len(entries) == len(ONLY), "Unknown Vanguard in --only"
    for spec in entries:
        assert spec["id"] in TUNING, spec["id"] + " is no Vanguard in Vanguards.json"
        for body, status, _ in bodies_of(spec):
            assert body["archetype"] in KIT["archetypes"], (spec["id"], status, "has no archetype in the kit")
    manifest_path = SOURCE / "manifest.json"
    manifest = json.loads(manifest_path.read_text()) if ONLY and manifest_path.exists() else {"assets": []}
    kept = [asset for asset in manifest["assets"] if ONLY and asset["id"] not in ONLY]
    built = [build(*body) for spec in entries for body in bodies_of(spec)]
    manifest = {"generatorVersion": GENERATOR_VERSION, "blender": bpy.app.version_string,
                "kitSha256": hashlib.sha256(KIT_BYTES).hexdigest(),
                "assets": sorted(kept + built, key=lambda asset: (asset["id"], asset.get("status", "")))}
    manifest_path.write_text(json.dumps(manifest, indent=2) + "\n", newline="\n")
    for asset in built:
        print("VEYRA_VANGUARD_BODY: " + asset["name"] + " " + str(asset["triangles"]) + " triangles, " + str(asset["heightCm"]) + " cm")
    print("VEYRA_VANGUARD_BODIES_PASSED: " + str(len(built)))


main()
