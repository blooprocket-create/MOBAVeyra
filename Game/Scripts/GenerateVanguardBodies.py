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
from VanguardBodies import beast, colossus, construct, humanoid, model, rider  # noqa: E402
from VanguardBodies.inputs import (CONTENT_VERSION, GENERATOR_VERSION, bodies_of, body_name, entries, generator_hash,  # noqa: E402
                                   input_hash, pending_changed, pending_removed, pinned_blender, removed_assets, same_destination,
                                   stale_assets, units)
from VanguardBodies.parts import local  # noqa: E402
GAME = Path(__file__).resolve().parents[1]
SOURCE = GAME / "ArtSource" / "Vanguards"
SAVED = GAME / "Saved" / "VanguardKit"
KIT_BYTES = (SOURCE / "VanguardKit.json").read_bytes()
KIT = json.loads(KIT_BYTES)
# Every unit a body is fitted to: the Vanguards, and the companions that are the other half of a pair (as Nix).
TUNING = units(json.loads((GAME / "Tuning" / "Vanguards.json").read_bytes())["vanguards"],
               json.loads((GAME / "Tuning" / "Abilities.json").read_bytes()).get("companions", {}))
ARCHETYPES = {"humanoid": humanoid, "colossus": colossus, "beast": beast, "construct": construct, "rider": rider}

if not bpy.app.background:
    raise RuntimeError("Run in an isolated background Blender process.")
assert KIT["schemaVersion"] == 1, "Unknown kit schema"
assert KIT["generatorVersion"] == GENERATOR_VERSION, "The kit was written for another generator version"
# Bodies are built by the Blender release the kit pins (ADR-064), whatever blender is on the PATH.
assert pinned_blender(KIT, bpy.app.version_string), "Run Blender " + KIT["blender"] + " (BuildVanguardBodies.ps1 -Blender), not " + bpy.app.version_string

ARGS = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
ONLY = set(ARGS[ARGS.index("--only") + 1].split(",")) if "--only" in ARGS else None
PREVIEW = "--preview" in ARGS
PREVIEW_DIR = Path(ARGS[ARGS.index("--preview-dir") + 1]) if "--preview-dir" in ARGS else SAVED / "Preview"
# The poses a preview lines up, as (animation, time from 0 to 1).
PREVIEW_POSES = [("Idle", 0.0), ("Run", 0.25), ("Run", 0.75), ("AttackWindup", 0.65), ("AttackWindup", 1.0),
                 ("Cast", 0.5), ("Hit", 0.5), ("Recall", 0.5), ("Death", 1.0)]
# The rest-pose take every FBX begins with (see animate); its name sorts before every clip's.
BIND_TAKE = "_Bind"
# The gameplay camera's look down from the horizontal, in degrees (DefaultGame.ini, VeyraCameraSettings.PitchDegrees).
GAMEPLAY_PITCH = 60.0
# How far below the ground a body's lowest point may reach, in centimetres: the slack Veyra.UI.VanguardBodies allows.
GROUND_SLACK = 2.0
# The decimal places a body's content is compared to: past float noise, well short of anything visible.
CONTENT_PLACES = 3
# How a body goes out to FBX; part of its content, since another option writes another file from the same body.
FBX_EXPORT = dict(use_selection=True, object_types={"ARMATURE", "MESH"}, apply_unit_scale=True, apply_scale_options="FBX_SCALE_UNITS",
                  axis_forward="-Y", axis_up="Z", add_leaf_bones=False, primary_bone_axis="Y", secondary_bone_axis="X",
                  use_armature_deform_only=False, bake_anim=True, bake_anim_use_all_actions=True, bake_anim_use_nla_strips=False,
                  bake_anim_force_startend_keying=True, bake_anim_simplify_factor=0.0, mesh_smooth_type="FACE",
                  # Unreal's materials read vertex colours as linear, so they go out linear.
                  colors_type="LINEAR")
# The code that builds a body (VanguardBodies.inputs): a body built by other code is stale.
GENERATOR = generator_hash(Path(__file__).resolve().parent)
# The Vanguards whose bodies changed since the last import, which the next import takes; the rest kept their FBX and their
# imported assets.
CHANGED = SAVED / "changed.json"
# The bodies dropped (removed or renamed in the kit) since the last import: their FBX is deleted here, their imported
# assets by the import.
REMOVED = SAVED / "removed.json"


def read_list(path):
    """A JSON list this generator wrote earlier, or an empty one."""
    return json.loads(path.read_text()) if path.exists() else []


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


def keyed(digest, armature, archetype):
    """Adds the pose about to be keyed, rounded past float noise, to digest: what the clip holds at that frame."""
    for bone in armature.pose.bones:
        values = list(bone.rotation_quaternion) + (list(bone.location) if bone.name in moved_bones(archetype) else [])
        digest.update(json.dumps([bone.name] + [round(value, CONTENT_PLACES) for value in values]).encode())


def animate(armature, spec, archetype, d, melee, digest):
    """Keys the rest take and every clip of the archetype on armature, adding each keyed pose to digest."""
    fps = KIT["fps"]
    rest = rest_quaternions(armature)
    armature.animation_data_create()
    # The FBX's first take is the one an importer reads its time zero from, and Blender writes takes in name order:
    # this one, named to come first, holds the rest pose, so a skeleton rebound at time zero is bound at rest. The
    # importer removes its sequence.
    bind = bpy.data.actions.new(BIND_TAKE)
    armature.animation_data.action = bind
    for frame in (1, 2):
        pose_rig(armature, rest, archetype, {}, 0.0)
        keyed(digest, armature, archetype)
        for bone in armature.pose.bones:
            bone.keyframe_insert("rotation_quaternion", frame=frame)
            if bone.name in moved_bones(archetype):
                bone.keyframe_insert("location", frame=frame)
    bind.use_fake_user = True
    actions = []
    for name, clip in KIT["archetypes"][spec["archetype"]]["animations"].items():
        action = bpy.data.actions.new(name)
        armature.animation_data.action = action
        frames = max(2, round(clip["seconds"] * fps))
        digest.update(json.dumps([name, frames, clip["loop"]]).encode())
        for frame in range(frames + 1):
            # A loop's last frame repeats its first.
            t = (frame % frames) / frames if clip["loop"] else frame / frames
            pose_rig(armature, rest, archetype, *archetype.pose(name, t, melee, d))
            keyed(digest, armature, archetype)
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


# What the body's mesh may hold for the exporter to write, by attribute: anything else it writes the content hash would
# not see, so a body holding it is refused until content_of covers it. Names beginning "." are Blender's own topology.
COVERED_ATTRIBUTES = {"position", "Col", "UVMap", "sharp_face", "material_index"}


def assert_covered(obj):
    """Refuses a body whose FBX would hold something its content hash does not cover: another mesh attribute, custom
    normals, shape keys or another material slot."""
    mesh = obj.data
    extra = sorted(attribute.name for attribute in mesh.attributes if not attribute.name.startswith(".") and attribute.name not in COVERED_ATTRIBUTES)
    assert not extra, (obj.name, "the content hash does not cover these mesh attributes; extend content_of first", extra)
    assert not mesh.has_custom_normals, (obj.name, "the content hash does not cover custom normals; extend content_of first")
    assert mesh.shape_keys is None, (obj.name, "the content hash does not cover shape keys; extend content_of first")
    assert len(mesh.materials) == 1, (obj.name, "the content hash covers one material slot")


def content_of(obj, armature, digest, version=CONTENT_VERSION):
    """What a body is, whenever it was written: its faces with the UVs they export and their smoothing and material slot,
    its vertices with their weights and colours, its skeleton at rest, (already in digest) every key of its takes,
    rounded to CONTENT_PLACES, and the options it is exported with. The same content exports the same body, so an FBX of
    it need not be written again. An earlier version gives the content as that version recorded it: 1 without the UVs,
    2 without the smoothing, material slot and export options."""
    mesh = obj.data
    groups = [group.name for group in obj.vertex_groups]
    for vertex in mesh.vertices:
        weights = sorted((groups[item.group], round(item.weight, CONTENT_PLACES)) for item in vertex.groups if item.weight > 0)
        digest.update(json.dumps([[round(value, CONTENT_PLACES) for value in vertex.co], weights]).encode())
    # Faces as a set, not a sequence: Blender may lay out the same faces in another order from one run to the next. Each
    # is its corners (vertex, colour and the UV it exports) from its lowest vertex on, keeping its winding, then whether
    # it is shaded smooth and its material slot.
    colors = mesh.color_attributes["Col"].data
    uv = mesh.uv_layers["UVMap"].data if version >= 2 else None
    faces = []
    for polygon in mesh.polygons:
        corners = [[mesh.loops[index].vertex_index, [round(value, CONTENT_PLACES) for value in colors[index].color]]
                   + ([[round(value, CONTENT_PLACES) for value in uv[index].uv]] if uv else []) for index in polygon.loop_indices]
        first = min(range(len(corners)), key=lambda corner: corners[corner][0])
        faces.append(json.dumps(corners[first:] + corners[:first] + ([polygon.use_smooth, polygon.material_index] if version >= 3 else [])))
    for face in sorted(faces):
        digest.update(face.encode())
    for bone in armature.data.bones:
        digest.update(json.dumps([bone.name, bone.parent.name if bone.parent else None]
                                 + [round(value, CONTENT_PLACES) for value in list(bone.head_local) + list(bone.tail_local)]).encode())
    if version >= 3:
        digest.update(json.dumps(FBX_EXPORT, sort_keys=True, default=sorted).encode())
    return digest.hexdigest()


def build(spec, status=None, suffix="", previous=None):
    """Generates one body; its manifest asset, and whether its content changed from previous (its earlier asset), whose
    FBX it keeps if not."""
    random.seed(spec["seed"])
    archetype = ARCHETYPES[spec["archetype"]]
    capsule = TUNING[spec["id"]]["body"]
    melee = not TUNING[spec["id"]]["basicAttack"].get("projectile")
    reset_scene()
    layout, dims = archetype.layout(spec, capsule)
    name = body_name(spec["id"], suffix)
    # Its archetype's bones, and any its kit adds (a loose part's spring chains, ADR-069).
    bones = archetype.bones_of(spec) if hasattr(archetype, "bones_of") else archetype.BONES
    armature = build_armature(name, bones, layout)
    if spec.get("model"):
        # A production model (ADR-069): its script's sculpt, meshed, reduced, skinned and flat-coloured.
        obj, triangles = model.build(spec, layout, dims, name, [bone for bone, _ in bones])
        mesh = obj.data
    else:
        body = archetype.body(spec, layout, dims)
        body.anchor_unweighted({bone: heads[0] for bone, heads in layout.items()})
        mesh = bpy.data.meshes.new(name)
        body.bm.to_mesh(mesh)
        triangles = sum(len(face.verts) - 2 for face in body.bm.faces)
        body.bm.free()
        # The exporter writes the active colour attribute: the body's colours, and in alpha what glows.
        mesh.color_attributes.active_color = mesh.color_attributes["Col"]
        obj = bpy.data.objects.new(name, mesh)
        bpy.context.scene.collection.objects.link(obj)
        for bone_name, _ in bones:
            obj.vertex_groups.new(name=bone_name)
    material = bpy.data.materials.new("M_VeyraVanguardBody")
    mesh.materials.append(material)
    obj.parent = armature
    modifier = obj.modifiers.new("Armature", "ARMATURE")
    modifier.object = armature
    digest = hashlib.sha256()
    actions = animate(armature, spec, archetype, dims, melee, digest)
    budget = spec["model"]["triangleBudget"] if spec.get("model") else KIT["archetypes"][spec["archetype"]]["triangleBudget"]
    assert triangles <= budget, (spec["id"], "triangle budget", triangles, budget)
    # Measured on the rest geometry, as Unreal imports it. Nothing sinks below the ground, and a body that walks stands
    # on it; one that floats hovers clear of it.
    heights = [vertex.co.z for vertex in mesh.vertices]
    height = max(heights) - min(heights)
    assert min(heights) > -GROUND_SLACK, (spec["id"], "the body sinks below the ground", min(heights))
    # A body of smoke stands on feet its effect pours, not on its mesh.
    grounded = archetype.GROUNDED and "smokeBody" not in spec.get("features", [])
    assert not grounded or min(heights) < dims["full"] * 0.02, (spec["id"], "the body does not stand on the ground", min(heights))
    (SOURCE / "FBX").mkdir(parents=True, exist_ok=True)
    path = SOURCE / "FBX" / (name + ".fbx")
    assert_covered(obj)
    content = content_of(obj, armature, digest.copy())
    # Built again as it was (new generator code or a new Blender that changes nothing in it): its FBX and imported
    # assets stand, and only its record of what built it moves on. A body recorded under an earlier CONTENT_VERSION is
    # compared as that version recorded it, when the same Blender built it: what the later versions add has not changed
    # since any such body's FBX was exported (the UV projection in VanguardBodies.parts; faces never shaded smooth, one
    # material slot, and today's export options, unchanged since vertex colours went out linear).
    version = previous.get("contentVersion", 1) if previous is not None else CONTENT_VERSION
    if version != CONTENT_VERSION:
        same = previous.get("blender") == bpy.app.version_string and previous.get("contentSha256") == content_of(obj, armature, digest.copy(), version)
    else:
        same = previous is not None and previous.get("contentSha256") == content
    kept = same and path.exists() and hashlib.sha256(path.read_bytes()).hexdigest() == previous.get("sha256")
    if not kept:
        bpy.ops.object.select_all(action="DESELECT")
        armature.select_set(True)
        obj.select_set(True)
        bpy.context.view_layer.objects.active = armature
        bpy.ops.export_scene.fbx(filepath=str(path), **FBX_EXPORT)
    if PREVIEW:
        render_preview(name, armature, obj, archetype, dims, melee)
    asset = {"id": spec["id"], "name": name, "archetype": spec["archetype"], "file": "FBX/" + path.name,
             "sha256": hashlib.sha256(path.read_bytes()).hexdigest(), "triangles": triangles, "triangleBudget": budget,
             "bones": len(bones), "heightCm": round(height, 2), "capsuleHalfHeightCm": capsule["capsuleHalfHeight"],
             "melee": melee, "runStrideCm": round(archetype.run_stride(dims), 2), "upperBodyBone": archetype.UPPER_BODY_BONE,
             "castReleaseShare": archetype.CAST_RELEASE_SHARE, "animations": actions,
             # What it was made from and by, so a later partial build cannot pass it off as current (VanguardBodies.inputs),
             # and what it is, so a rebuild that changes nothing in it keeps it.
             "inputSha256": input_hash(KIT, TUNING, spec), "generatorSha256": GENERATOR, "blender": bpy.app.version_string,
             "contentSha256": content, "contentVersion": CONTENT_VERSION}
    if getattr(archetype, "IK_FEET", None):
        # The limbs the engine's inverse kinematics holds (ADR-069): its legs on the ground, and an off hand on a weapon
        # its stance holds in both hands.
        asset["ik"] = {"feet": [list(chain) for chain in archetype.IK_FEET]}
        if spec.get("stance") in getattr(archetype, "IK_TWO_HANDED_STANCES", ()):
            asset["ik"]["offHand"] = {"chain": list(archetype.IK_OFF_HAND), "anchor": archetype.IK_OFF_HAND_ANCHOR}
    springs = archetype.springs_of(spec, layout, dims) if hasattr(archetype, "springs_of") else None
    if springs:
        # Its loose parts' chains and the capsules they hang outside, for the engine's secondary motion (ADR-069).
        asset["springs"] = springs
    if spec.get("effect"):
        # What it is made of where no mesh shows it, poured off its bones in the game (the art set's Effect).
        effect = spec["effect"]
        missing = [bone for bone in effect["bones"] if bone not in dict(bones)]
        assert not missing, (spec["id"], "the effect pours from bones it lacks", missing)
        # Sized as the body is grown (a larger form pours larger smoke).
        asset["effect"] = {"system": effect["system"], "bones": effect["bones"], "color": effect["color"],
                           "scale": spec.get("bodyScale", 1.0)}
    if status:
        asset["status"] = status
        # Which status body wins when its unit holds several (the art set's Priority): a brief burst's over one held
        # all the while in some ground.
        if spec.get("priority"):
            asset["priority"] = spec["priority"]
    return asset, not kept


def main():
    assert set(KIT["archetypes"]) <= set(ARCHETYPES), ("Unknown archetypes", set(KIT["archetypes"]) - set(ARCHETYPES))
    selected = [spec for spec in entries(KIT) if ONLY is None or spec["id"] in ONLY]
    assert ONLY is None or len(selected) == len(ONLY), "Unknown Vanguard or companion in --only"
    for spec in selected:
        assert spec["id"] in TUNING, spec["id"] + " is no Vanguard in Vanguards.json nor companion in Abilities.json"
        for body, status, _ in bodies_of(spec):
            assert body["archetype"] in KIT["archetypes"], (spec["id"], status, "has no archetype in the kit")
    manifest_path = SOURCE / "manifest.json"
    manifest = json.loads(manifest_path.read_text()) if manifest_path.exists() else {"assets": []}
    # The bodies were imported where the manifest says. Moving them is no build step (the editor moves them and fixes
    # their references), so a kit naming another destination is refused before anything is written.
    assert same_destination(KIT, manifest), ("The kit moves the bodies from " + str(manifest.get("destination")) + " to " + KIT["destination"]
                                             + ": move them in the editor and the manifest's destination with them, or delete them and the manifest and build afresh")
    previous = {(asset["id"], asset.get("status")): asset for asset in manifest["assets"]}
    kept = [asset for asset in manifest["assets"] if ONLY and asset["id"] not in ONLY]
    results = [build(*body, previous=previous.get((spec["id"], body[1]))) for spec in selected for body in bodies_of(spec)]
    built = [asset for asset, _ in results]
    changed = sorted({asset["id"] for asset, fresh in results if fresh})
    companions = {spec["id"] for spec in KIT.get("companions", [])}
    for asset in built:
        if asset["id"] in companions:
            asset["companion"] = True
    manifest = {"generatorVersion": GENERATOR_VERSION, "blender": bpy.app.version_string, "destination": KIT["destination"],
                "kitSha256": hashlib.sha256(KIT_BYTES).hexdigest(),
                "assets": sorted(kept + built, key=lambda asset: (asset["id"], asset.get("status", "")))}
    manifest_path.write_text(json.dumps(manifest, indent=2) + "\n", newline="\n")
    SAVED.mkdir(parents=True, exist_ok=True)
    # Added to what earlier builds left to import, until an import takes it (BuildVanguardBodies.ps1 clears both): a body
    # this build kept as it was is current only if it was imported, and a run of this generator alone imports nothing.
    CHANGED.write_text(json.dumps(pending_changed(read_list(CHANGED), changed, manifest["assets"])) + "\n")
    dropped = removed_assets(list(previous.values()), manifest["assets"], ONLY)
    for asset in dropped:
        (SOURCE / asset["file"]).unlink(missing_ok=True)
        print("VEYRA_VANGUARD_BODY_REMOVED: " + asset["name"])
    REMOVED.write_text(json.dumps(pending_removed(read_list(REMOVED), [asset["name"] for asset in dropped], manifest["assets"])) + "\n")
    # A body kept from an earlier build whose inputs have changed since (a shared setting, its generator's code, the
    # Blender that built it) is stale: say which to rebuild, rather than let the importer take it.
    stale = stale_assets(KIT, TUNING, manifest["assets"], GENERATOR, manifest["blender"])
    if stale:
        print("VEYRA_VANGUARD_BODIES_STALE: " + ", ".join(stale))
    for asset, fresh in results:
        print(("VEYRA_VANGUARD_BODY: " if fresh else "VEYRA_VANGUARD_BODY_UNCHANGED: ") + asset["name"] + " " + str(asset["triangles"])
              + " triangles, " + str(asset["heightCm"]) + " cm")
    print("VEYRA_VANGUARD_BODIES_PASSED: " + str(len(built)))


main()
