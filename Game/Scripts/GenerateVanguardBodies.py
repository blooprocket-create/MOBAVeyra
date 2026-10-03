"""Generate the Vanguards' rigged, animated first-pass bodies (ADR-064) from ArtSource/Vanguards/VanguardKit.json.

Run in an isolated background Blender (5.2 LTS):
    blender --background --factory-startup --python-exit-code 1 --python GenerateVanguardBodies.py [-- --only id,id]
BuildVanguardBodies.ps1 runs it, then imports. For each Vanguard it builds its archetype's skeleton fitted to the
capsule in Game/Tuning/Vanguards.json, a body of simple parts rigidly weighted to their bones, coloured through vertex
colours (alpha marks what glows), and the archetype's animations generated on that skeleton; it writes one FBX per
Vanguard to ArtSource/Vanguards/FBX and a manifest of their hashes. The body faces +X and stands on its root at the
ground. Nothing here decides gameplay: the capsule stays the only collision and movement.
"""
import hashlib
import json
import math
import random
import sys
from pathlib import Path

import bmesh
import bpy
from mathutils import Euler, Matrix, Quaternion, Vector

GENERATOR_VERSION = 1
GAME = Path(__file__).resolve().parents[1]
SOURCE = GAME / "ArtSource" / "Vanguards"
SAVED = GAME / "Saved" / "VanguardKit"
KIT_BYTES = (SOURCE / "VanguardKit.json").read_bytes()
KIT = json.loads(KIT_BYTES)
TUNING = json.loads((GAME / "Tuning" / "Vanguards.json").read_bytes())["vanguards"]

if not bpy.app.background:
    raise RuntimeError("Run in an isolated background Blender process.")
assert KIT["schemaVersion"] == 1, "Unknown kit schema"
assert KIT["generatorVersion"] == GENERATOR_VERSION, "The kit was written for another generator version"

ARGS = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
ONLY = set(ARGS[ARGS.index("--only") + 1].split(",")) if "--only" in ARGS else None
PREVIEW = "--preview" in ARGS
# The poses a preview lines up, as (animation, time from 0 to 1).
PREVIEW_POSES = [("Idle", 0.0), ("Run", 0.25), ("Run", 0.75), ("AttackWindup", 1.0), ("AttackStrike", 0.3),
                 ("Cast", 0.5), ("Hit", 0.5), ("Recall", 0.5), ("Death", 1.0)]

# The humanoid skeleton: every humanoid has every bone, so all share one definition. A bone a body does not use (a
# tail, a prop) carries no weight. (name, parent)
HUMANOID_BONES = [
    ("root", None), ("pelvis", "root"), ("spine_01", "pelvis"), ("spine_02", "spine_01"), ("spine_03", "spine_02"),
    ("neck_01", "spine_03"), ("head", "neck_01"),
    ("clavicle_l", "spine_03"), ("upperarm_l", "clavicle_l"), ("lowerarm_l", "upperarm_l"), ("hand_l", "lowerarm_l"), ("prop_l", "hand_l"),
    ("clavicle_r", "spine_03"), ("upperarm_r", "clavicle_r"), ("lowerarm_r", "upperarm_r"), ("hand_r", "lowerarm_r"), ("prop_r", "hand_r"),
    ("thigh_l", "pelvis"), ("calf_l", "thigh_l"), ("foot_l", "calf_l"),
    ("thigh_r", "pelvis"), ("calf_r", "thigh_r"), ("foot_r", "calf_r"),
    ("tail_01", "pelvis"), ("tail_02", "tail_01"), ("tail_03", "tail_02"),
]
BUILD = {"lean": 0.9, "normal": 1.0, "heavy": 1.3, "round": 1.15}
SEGMENTS = 10
# A leg's radius as a share of the body's height, and the foot's radius at heel and toe as shares of it.
LEG_SHARE = 0.052
FOOT_HEEL_SHARE = 0.75
FOOT_TOE_SHARE = 0.8
# How far a running thigh swings either side of straight down, in degrees.
RUN_THIGH_SWING = 35.0


# ---------------------------------------------------------------------------------------------- the skeleton
def humanoid_layout(spec, capsule):
    """Every bone's head and tail in centimetres, the body facing +X with its left at +Y, fitted to the capsule."""
    features = set(spec["features"])
    wave = 0.22 if "waveBase" in features else 0.0
    full = capsule["capsuleHalfHeight"] * 2.0 * spec["heightShare"]
    base = full * wave
    height = full - base
    head = height * spec["headShare"]
    # Stylised for a high camera: a grown figure's legs are 0.44 of its height, and they shorten as the head grows
    # past a grown figure's share, as a smaller figure's do.
    leg = height * max(0.30, 0.44 - (spec["headShare"] - 0.16) * 1.1)
    neck = height * 0.03
    torso = height - head - leg - neck
    build = BUILD[spec["build"]]
    # Broad shoulders fill the capsule, so the body's footprint reads as the unit's.
    shoulder = max(capsule["capsuleRadius"] * 0.8, height * 0.13) * build
    hip = shoulder * 0.62
    arm = height * 0.38 * (0.85 if spec["headShare"] > 0.2 else 1.0)
    digitigrade = "digitigrade" in features
    # The foot's radius at its heel (humanoid_body draws it), so the sole stands on the ground.
    sole = height * LEG_SHARE * build * FOOT_HEEL_SHARE
    pelvis_z = base + leg
    chest_z = pelvis_z + torso
    L = {}
    L["root"] = (Vector((0, 0, 0)), Vector((0, 0, base + leg * 0.25)))
    L["pelvis"] = (Vector((0, 0, pelvis_z)), Vector((0, 0, pelvis_z + torso * 0.2)))
    L["spine_01"] = (Vector((0, 0, pelvis_z + torso * 0.2)), Vector((0, 0, pelvis_z + torso * 0.45)))
    L["spine_02"] = (Vector((0, 0, pelvis_z + torso * 0.45)), Vector((0, 0, pelvis_z + torso * 0.72)))
    L["spine_03"] = (Vector((0, 0, pelvis_z + torso * 0.72)), Vector((0, 0, chest_z)))
    L["neck_01"] = (Vector((0, 0, chest_z)), Vector((0, 0, chest_z + neck)))
    L["head"] = (Vector((0, 0, chest_z + neck)), Vector((0, 0, chest_z + neck + head)))
    for side, sign in (("l", 1.0), ("r", -1.0)):
        shoulder_point = Vector((0, sign * shoulder, chest_z - torso * 0.08))
        L["clavicle_" + side] = (Vector((0, sign * shoulder * 0.2, chest_z - torso * 0.08)), shoulder_point)
        # Arms hang a little out from the body, the A pose a rig is skinned in.
        down = Vector((0.0, sign * math.sin(math.radians(18)), -math.cos(math.radians(18))))
        elbow = shoulder_point + down * arm * 0.48
        wrist = elbow + down * arm * 0.40
        L["upperarm_" + side] = (shoulder_point, elbow)
        L["lowerarm_" + side] = (elbow, wrist)
        L["hand_" + side] = (wrist, wrist + down * arm * 0.12)
        L["prop_" + side] = (wrist + down * arm * 0.12, wrist + down * arm * 0.12 + Vector((arm * 0.15, 0, 0)))
        hip_point = Vector((0, sign * hip, pelvis_z))
        if digitigrade:
            # A digitigrade leg: the thigh forward, the long shank back, a long foot on its toes.
            hock_z = base + sole + leg * 0.12
            knee = hip_point + Vector((leg * 0.18, 0, -(pelvis_z - hock_z) * 0.52))
            hock = Vector((knee.x - leg * 0.22, sign * hip, hock_z))
        else:
            hock = Vector((0, sign * hip, base + sole))
            knee = Vector((leg * 0.02, sign * hip, (pelvis_z + hock.z) / 2))
        L["thigh_" + side] = (hip_point, knee)
        L["calf_" + side] = (knee, hock)
        # The foot's sole meets the ground: its axis runs a foot's thickness above it.
        L["foot_" + side] = (hock, Vector((hock.x + leg * 0.22, hock.y, base + sole * FOOT_TOE_SHARE)))
    tail_start = Vector((-hip * 0.6, 0, pelvis_z - torso * 0.05))
    tail_dir = Vector((-1.0, 0, -0.35)).normalized()
    tail_length = leg * (1.1 if "heavyTail" in features else 0.5)
    for index, name in enumerate(("tail_01", "tail_02", "tail_03")):
        L[name] = (tail_start + tail_dir * tail_length * index / 3, tail_start + tail_dir * tail_length * (index + 1) / 3)
    dims = {"height": height, "full": full, "base": base, "head": head, "torso": torso, "leg": leg, "shoulder": shoulder,
            "hip": hip, "arm": arm, "build": build}
    return L, dims


def build_armature(name, layout):
    data = bpy.data.armatures.new(name + "_Skeleton")
    armature = bpy.data.objects.new("Armature", data)
    bpy.context.scene.collection.objects.link(armature)
    bpy.context.view_layer.objects.active = armature
    bpy.ops.object.mode_set(mode="EDIT")
    for bone_name, parent in HUMANOID_BONES:
        bone = data.edit_bones.new(bone_name)
        bone.head, bone.tail = layout[bone_name]
        bone.roll = 0.0
        if parent:
            bone.parent = data.edit_bones[parent]
            bone.use_connect = False
    bpy.ops.object.mode_set(mode="OBJECT")
    return armature


# ---------------------------------------------------------------------------------------------- the body
class Body:
    """One bmesh of parts, each rigidly weighted to a bone and coloured through vertex colours."""

    def __init__(self, bone_names):
        self.bm = bmesh.new()
        self.deform = self.bm.verts.layers.deform.verify()
        self.color = self.bm.loops.layers.color.new("Col")
        self.groups = {name: index for index, name in enumerate(bone_names)}

    def _finish(self, verts, bone, color, glow):
        group = self.groups[bone]
        faces = set()
        for vert in verts:
            vert[self.deform][group] = 1.0
            faces.update(vert.link_faces)
        rgba = (color[0], color[1], color[2], 1.0 if glow else 0.0)
        for face in faces:
            for loop in face.loops:
                loop[self.color] = rgba

    @staticmethod
    def _frame(start, end):
        """A matrix placing a unit shape's +Z along start→end, centred between them."""
        axis = end - start
        length = max(axis.length, 0.01)
        rotation = Vector((0, 0, 1)).rotation_difference(axis.normalized()).to_matrix().to_4x4()
        return Matrix.Translation((start + end) / 2) @ rotation, length

    def limb(self, bone, start, end, radius_start, radius_end, color, glow=False, segments=SEGMENTS):
        frame, length = self._frame(start, end)
        verts = bmesh.ops.create_cone(self.bm, cap_ends=True, cap_tris=False, segments=segments, radius1=radius_start,
                                      radius2=radius_end, depth=length, matrix=frame)["verts"]
        self._finish(verts, bone, color, glow)

    def ball(self, bone, center, radius, color, glow=False, scale=(1, 1, 1)):
        matrix = Matrix.Translation(center) @ Matrix.Diagonal((*scale, 1.0))
        verts = bmesh.ops.create_uvsphere(self.bm, u_segments=SEGMENTS, v_segments=7, radius=radius, matrix=matrix)["verts"]
        self._finish(verts, bone, color, glow)

    def box(self, bone, center, size, color, glow=False, rotation=None):
        matrix = Matrix.Translation(center) @ (rotation.to_matrix().to_4x4() if rotation else Matrix.Identity(4)) @ Matrix.Diagonal((*size, 1.0))
        verts = bmesh.ops.create_cube(self.bm, size=1.0, matrix=matrix)["verts"]
        self._finish(verts, bone, color, glow)


def mix(a, b, share):
    return [a[i] * (1 - share) + b[i] * share for i in range(3)]


def humanoid_body(spec, L, d):
    body = Body([name for name, _ in HUMANOID_BONES])
    features = set(spec["features"])
    skin, primary, secondary, hair, accent = spec["skin"], spec["primary"], spec["secondary"], spec["hair"], spec["accent"]
    metal = [0.62, 0.55, 0.42]
    limb = d["height"] * 0.036 * d["build"]
    leg = d["height"] * LEG_SHARE * d["build"]
    # Torso: hips, belly and chest, the chest broadest.
    p0, p1 = L["pelvis"]
    body.limb("pelvis", p0 - Vector((0, 0, d["torso"] * 0.08)), p1, d["hip"] * 1.05, d["hip"] * 1.0, secondary)
    body.limb("spine_01", *L["spine_01"], d["hip"] * 1.0, d["shoulder"] * 0.72, primary)
    body.limb("spine_02", *L["spine_02"], d["shoulder"] * 0.72, d["shoulder"] * 0.86, primary)
    body.limb("spine_03", *L["spine_03"], d["shoulder"] * 0.86, d["shoulder"] * 0.62, primary)
    # Neck and head.
    body.limb("neck_01", *L["neck_01"], limb * 1.1, limb * 1.0, skin)
    h0, h1 = L["head"]
    head_radius = d["head"] * 0.52
    head_center = (h0 + h1) / 2
    body.ball("head", head_center, head_radius, skin, scale=(1.0, 0.9, 1.0))
    # Eyes: two dark points on the front, so the face shows which way it looks.
    for sign in (1, -1):
        body.ball("head", head_center + Vector((head_radius * 0.85, sign * head_radius * 0.35, head_radius * 0.1)), head_radius * 0.12, [0.05, 0.05, 0.06])
    hair_style = spec["hairStyle"]
    if hair_style in ("short", "long", "curly", "twinTails", "sideTail", "longBeard"):
        body.ball("head", head_center + Vector((-head_radius * 0.12, 0, head_radius * 0.18)), head_radius * 1.04, hair, scale=(1.0, 0.95, 0.9))
    if hair_style in ("long", "longBeard"):
        body.box("head", head_center + Vector((-head_radius * 0.75, 0, -head_radius * 0.9)), (head_radius * 0.5, head_radius * 1.6, head_radius * 2.0), hair)
    if hair_style == "longBeard":
        body.limb("head", head_center + Vector((head_radius * 0.6, 0, -head_radius * 0.4)), head_center + Vector((head_radius * 0.7, 0, -head_radius * 1.4)),
                  head_radius * 0.5, head_radius * 0.15, hair)
    if hair_style == "curly":
        for index in range(6):
            angle = index / 6 * math.tau
            body.ball("head", head_center + Vector((math.cos(angle) * head_radius * 0.6 - head_radius * 0.2, math.sin(angle) * head_radius * 0.8, head_radius * 0.7)),
                      head_radius * 0.35, hair)
    if hair_style == "twinTails":
        for sign in (1, -1):
            body.limb("head", head_center + Vector((-head_radius * 0.2, sign * head_radius * 0.9, head_radius * 0.3)),
                      head_center + Vector((-head_radius * 0.5, sign * head_radius * 1.4, -head_radius * 1.6)), head_radius * 0.3, head_radius * 0.08, hair)
    if hair_style == "sideTail":
        body.limb("head", head_center + Vector((-head_radius * 0.3, head_radius * 0.8, head_radius * 0.6)),
                  head_center + Vector((-head_radius * 0.6, head_radius * 1.3, -head_radius * 0.6)), head_radius * 0.28, head_radius * 0.08, hair)
    # Headwear and head features.
    if "hood" in features:
        # Set back so the face shows through its opening.
        body.ball("head", head_center + Vector((-head_radius * 0.4, 0, head_radius * 0.15)), head_radius * 1.15, primary, scale=(1.0, 1.0, 1.05))
    if "hoodDown" in features:
        body.limb("neck_01", *L["neck_01"], head_radius * 1.1, head_radius * 0.9, secondary)
    if "wideHat" in features:
        body.limb("head", head_center + Vector((0, 0, head_radius * 0.55)), head_center + Vector((0, 0, head_radius * 0.65)), head_radius * 2.0, head_radius * 2.0, secondary)
        body.limb("head", head_center + Vector((0, 0, head_radius * 0.6)), head_center + Vector((0, 0, head_radius * 1.4)), head_radius * 0.95, head_radius * 0.8, secondary)
    if "goggles" in features:
        for sign in (1, -1):
            body.ball("head", head_center + Vector((head_radius * 0.75, sign * head_radius * 0.35, head_radius * 0.65)), head_radius * 0.22, accent, glow=True)
    if "horn" in features:
        body.limb("head", head_center + Vector((head_radius * 0.3, head_radius * 0.3, head_radius * 0.7)),
                  head_center + Vector((head_radius * 0.2, head_radius * 0.6, head_radius * 1.7)), head_radius * 0.3, head_radius * 0.02, [0.85, 0.80, 0.70])
    if "longEars" in features or "roundEars" in features or "sweptEars" in features:
        for sign in (1, -1):
            if "roundEars" in features:
                body.ball("head", head_center + Vector((0, sign * head_radius * 0.75, head_radius * 0.75)), head_radius * 0.35, skin)
            else:
                lean = -0.9 if "sweptEars" in features else -0.2
                body.limb("head", head_center + Vector((0, sign * head_radius * 0.5, head_radius * 0.6)),
                          head_center + Vector((head_radius * lean, sign * head_radius * 0.8, head_radius * 2.3)), head_radius * 0.3, head_radius * 0.05, skin)
    # Arms; mechanical arms are metal from the shoulder.
    arm_color = metal if "mechanicalArms" in features else skin
    for side in ("l", "r"):
        body.limb("clavicle_" + side, *L["clavicle_" + side], limb * 1.2, limb * 1.25, primary)
        body.limb("upperarm_" + side, *L["upperarm_" + side], limb * 1.25, limb * 1.0, primary)
        body.limb("lowerarm_" + side, *L["lowerarm_" + side], limb * 1.0, limb * 0.85, arm_color)
        h0, h1 = L["hand_" + side]
        # Big hands read at a distance.
        body.ball("hand_" + side, (h0 + h1) / 2, limb * 1.35, arm_color)
    # Legs and feet; boots in the secondary colour.
    for side in ("l", "r"):
        body.limb("thigh_" + side, *L["thigh_" + side], leg * 1.15, leg * 0.95, secondary)
        body.limb("calf_" + side, *L["calf_" + side], leg * 0.95, leg * 0.75, secondary)
        f0, f1 = L["foot_" + side]
        body.limb("foot_" + side, f0, f1, leg * FOOT_HEEL_SHARE, leg * FOOT_HEEL_SHARE * FOOT_TOE_SHARE, mix(secondary, [0.05, 0.05, 0.05], 0.4))
    # Clothing and armour.
    s0, s1 = L["spine_03"]
    if "cloak" in features:
        body.box("spine_03", s0 + Vector((-d["shoulder"] * 0.75, 0, -d["torso"] * 0.55)), (d["shoulder"] * 0.12, d["shoulder"] * 1.6, d["torso"] * 1.5), secondary)
    if "scarf" in features:
        body.box("neck_01", L["neck_01"][0] + Vector((-limb * 1.6, 0, -d["torso"] * 0.25)), (limb * 0.5, limb * 2.0, d["torso"] * 0.7), secondary)
    if "coatSkirt" in features:
        body.limb("pelvis", p0 + Vector((0, 0, d["torso"] * 0.05)), p0 - Vector((0, 0, d["leg"] * 0.55)), d["hip"] * 1.15, d["hip"] * 1.55, primary)
    if "shoulderPlate" in features:
        body.ball("clavicle_l", L["clavicle_l"][1] + Vector((0, 0, limb)), limb * 2.2, mix(secondary, metal, 0.6), scale=(1.0, 1.0, 0.6))
    if "kneeGuards" in features:
        for side in ("l", "r"):
            body.ball("calf_" + side, L["calf_" + side][0] + Vector((leg * 0.5, 0, 0)), leg * 0.8, metal)
    # Tails ride the tail bones.
    if "fluffyTail" in features:
        for index, name in enumerate(("tail_01", "tail_02", "tail_03")):
            t0, t1 = L[name]
            body.ball(name, (t0 + t1) / 2, leg * (1.2 + index * 0.4), mix(skin, [1, 1, 1], 0.3))
    if "shortTail" in features:
        t0, t1 = L["tail_01"]
        body.ball("tail_01", t0, leg * 0.9, mix(skin, [1, 1, 1], 0.4))
    if "heavyTail" in features:
        for index, name in enumerate(("tail_01", "tail_02", "tail_03")):
            t0, t1 = L[name]
            body.limb(name, t0, t1, leg * (1.3 - index * 0.4), leg * (0.95 - index * 0.35), skin)
    # The wave Neris rides: a swell under her feet that is the whole silhouette.
    if "waveBase" in features:
        base = d["base"]
        # Flattened to 0.55 of its radius and resting on the ground.
        body.ball("root", Vector((0, 0, base * 1.4 * 0.55)), base * 1.4, mix(accent, [0.02, 0.06, 0.12], 0.7), scale=(1.6, 1.1, 0.55))
        body.ball("root", Vector((-base * 0.5, 0, base * 0.85)), base * 0.6, accent, glow=True, scale=(1.4, 0.9, 0.35))
    for prop in spec["props"]:
        add_prop(body, prop, L, d, spec)
    return body


def add_prop(body, prop, L, d, spec):
    """A prop of the small library, held in its hand, carried on the back, or orbiting both hands."""
    kind, hand = prop["kind"], prop["hand"]
    accent, secondary = spec["accent"], spec["secondary"]
    metal = [0.55, 0.52, 0.48]
    wood = [0.35, 0.22, 0.12]
    unit = d["height"]
    side = "r" if hand in ("right", "both") else "l"
    grip = L["hand_" + side][1]
    forward = Vector((1, 0, 0))
    down = Vector((0, 0, -1))
    bone = "prop_" + side
    if kind == "bracer":
        e0, e1 = L["lowerarm_" + side]
        body.limb("lowerarm_" + side, e0 + (e1 - e0) * 0.3, e1, unit * 0.04, unit * 0.035, metal)
        body.ball("lowerarm_" + side, e1, unit * 0.022, accent, glow=True)
    elif kind == "rifle":
        body.limb(bone, grip - forward * unit * 0.12, grip + forward * unit * 0.45, unit * 0.022, unit * 0.016, [0.15, 0.14, 0.14])
        body.limb(bone, grip, grip + forward * unit * 0.4, unit * 0.008, unit * 0.008, accent, glow=True)
        body.limb(bone, grip + forward * unit * 0.05 + Vector((0, 0, unit * 0.035)), grip + forward * unit * 0.12 + Vector((0, 0, unit * 0.035)), unit * 0.02, unit * 0.02, accent, glow=True)
    elif kind == "lantern":
        body.limb(bone, grip, grip + down * unit * 0.08, unit * 0.004, unit * 0.004, metal)
        body.ball(bone, grip + down * unit * 0.13, unit * 0.045, accent, glow=True, scale=(1.0, 1.0, 1.3))
    elif kind == "ball":
        body.ball(bone, grip + forward * unit * 0.06, unit * 0.07, accent, glow=True)
    elif kind == "siegeArm":
        body.limb(bone, grip - forward * unit * 0.25, grip + forward * unit * 0.55, unit * 0.04, unit * 0.03, wood)
        body.limb(bone, grip + forward * unit * 0.55, grip + forward * unit * 0.72, unit * 0.025, unit * 0.002, metal)
        body.limb(bone, grip - forward * unit * 0.2, grip + forward * unit * 0.5, unit * 0.01, unit * 0.01, accent, glow=True)
    elif kind == "chain":
        for index in range(7):
            angle = index * 0.9
            body.ball(bone, grip + Vector((math.cos(angle) * unit * 0.06, math.sin(angle) * unit * 0.06, -index * unit * 0.03)), unit * 0.018, accent, glow=True)
    elif kind == "boardingBlade":
        body.limb(bone, grip, grip + forward * unit * 0.08, unit * 0.015, unit * 0.015, wood)
        body.box(bone, grip + forward * unit * 0.4 + Vector((0, 0, unit * 0.02)), (unit * 0.62, unit * 0.015, unit * 0.11), metal)
        body.limb(bone, grip + forward * unit * 0.66 + Vector((0, 0, -unit * 0.04)), grip + forward * unit * 0.6 + Vector((0, 0, -unit * 0.14)), unit * 0.04, unit * 0.005, metal)
    elif kind == "swordBack":
        s0, s1 = L["spine_03"]
        body.limb("spine_03", s0 + Vector((-d["shoulder"] * 0.55, d["shoulder"] * 0.5, d["torso"] * 0.35)),
                  s0 + Vector((-d["shoulder"] * 0.55, -d["shoulder"] * 0.6, -d["torso"] * 0.55)), unit * 0.018, unit * 0.014, [0.08, 0.08, 0.09])
    elif kind == "dagger":
        body.limb(bone, grip, grip + forward * unit * 0.14, unit * 0.012, unit * 0.002, metal)
    elif kind == "dispenserRig":
        s0, s1 = L["spine_03"]
        tank = s0 + Vector((-d["shoulder"] * 0.75, 0, -d["torso"] * 0.05))
        body.limb("spine_03", tank - Vector((0, 0, d["torso"] * 0.4)), tank + Vector((0, 0, d["torso"] * 0.35)), d["shoulder"] * 0.4, d["shoulder"] * 0.4, metal)
        body.limb("spine_03", tank - Vector((0, 0, d["torso"] * 0.2)), tank + Vector((0, 0, d["torso"] * 0.2)), d["shoulder"] * 0.42, d["shoulder"] * 0.42, accent, glow=True)
        body.limb("prop_r", L["hand_r"][1], L["hand_r"][1] + forward * unit * 0.12, unit * 0.02, unit * 0.025, metal)
    elif kind == "cannon":
        body.limb(bone, grip - forward * unit * 0.2, grip + forward * unit * 0.4, unit * 0.06, unit * 0.075, [0.62, 0.48, 0.25])
        body.limb(bone, grip + forward * unit * 0.36, grip + forward * unit * 0.41, unit * 0.05, unit * 0.05, accent, glow=True)
        body.box(bone, grip - forward * unit * 0.18 + Vector((0, 0, unit * 0.08)), (unit * 0.16, unit * 0.12, unit * 0.12), metal)
    elif kind == "rings":
        for ring_side in ("l", "r"):
            center = L["hand_" + ring_side][1] + forward * unit * 0.06
            for index in range(3):
                angle = index / 3 * math.tau
                body.ball("prop_" + ring_side, center + Vector((0, math.cos(angle) * unit * 0.05, math.sin(angle) * unit * 0.05)), unit * 0.014, accent, glow=True)
            body.ball("prop_" + ring_side, center, unit * 0.022, [0.85, 0.65, 0.20], glow=True)
    elif kind == "springbow":
        body.limb(bone, grip + Vector((0, 0, unit * 0.12)), grip - Vector((0, 0, unit * 0.12)), unit * 0.012, unit * 0.012, [0.20, 0.35, 0.18])
        body.limb(bone, grip, grip + forward * unit * 0.12, unit * 0.01, unit * 0.01, [0.62, 0.48, 0.25])
        body.ball(bone, grip + forward * unit * 0.12, unit * 0.014, accent, glow=True)
    elif kind == "cleaver":
        body.limb(bone, grip, grip + forward * unit * 0.06, unit * 0.015, unit * 0.015, wood)
        body.box(bone, grip + forward * unit * 0.22, (unit * 0.3, unit * 0.02, unit * 0.14), [0.18, 0.17, 0.16])
        body.box(bone, grip + forward * unit * 0.22 + Vector((0, 0, unit * 0.075)), (unit * 0.3, unit * 0.024, unit * 0.02), accent, glow=True)
    elif kind == "scroll":
        body.limb(bone, grip + Vector((0, unit * 0.1, 0)), grip - Vector((0, unit * 0.1, 0)), unit * 0.025, unit * 0.025, [0.90, 0.86, 0.75])
    else:
        raise AssertionError("Unknown prop: " + kind)


# ---------------------------------------------------------------------------------------------- the animations
def rest_quaternions(armature):
    return {bone.name: bone.matrix_local.to_quaternion() for bone in armature.data.bones}


def local(rest, world_euler):
    """A rotation given about the armature's axes (X forward, Y left, Z up), as the bone's own rotation."""
    q_world = Euler(world_euler, "XYZ").to_quaternion()
    return rest.inverted() @ q_world @ rest


def forward_swing(angle):
    """A swing that carries a hanging limb forward (+X) by angle degrees."""
    return (0.0, -math.radians(angle), 0.0)


def twist(angle):
    return (0.0, 0.0, math.radians(angle))


def roll_side(angle, sign):
    """Raises a hanging limb out to its side by angle degrees; sign is +1 for the left (+Y), -1 for the right."""
    return (math.radians(angle) * sign, 0.0, 0.0)


def ease(x):
    return x * x * (3 - 2 * x)


def run_stride(d):
    """How far one Run cycle carries the body: two steps, each the planted foot sweeping from one swing to the other."""
    return 2 * 2 * (d["leg"]) * math.sin(math.radians(RUN_THIGH_SWING))


def humanoid_pose(name, t, melee, d):
    """Each bone's rotation (about the armature's axes, in radians) and the pelvis's lift at t from 0 to 1."""
    pose, lift = {}, 0.0
    tau = math.tau
    if name == "Idle":
        breath = math.sin(t * tau)
        pose["spine_03"] = forward_swing(1.5 * breath)
        pose["head"] = twist(4 * math.sin(t * tau + 1.0))
        for side, sign in (("l", 1), ("r", -1)):
            pose["upperarm_" + side] = forward_swing(3 * math.sin(t * tau + sign))
        lift = d["height"] * 0.006 * breath
    elif name == "Run":
        stride = math.sin(t * tau)
        pose["spine_01"] = forward_swing(10)
        pose["spine_03"] = twist(8 * stride)
        for side, sign in (("l", 1), ("r", -1)):
            phase = stride * sign
            pose["thigh_" + side] = forward_swing(RUN_THIGH_SWING * phase)
            pose["calf_" + side] = forward_swing(-40 * max(0.0, -phase) - 15)
            pose["upperarm_" + side] = forward_swing(-30 * phase)
            pose["lowerarm_" + side] = forward_swing(45)
        lift = d["height"] * 0.025 * abs(math.cos(t * tau))
    elif name in ("AttackWindup", "AttackStrike"):
        # Melee: the chest turns right and the right arm cocks out and back, weapon up; the strike sweeps it
        # forward across the body, then settles. Ranged: both arms raise to aim, hands level so the weapon points
        # ahead; the release is a short recoil.
        if name == "AttackWindup":
            cock, sweep, settle, recoil = ease(t), 0.0, 0.0, 0.0
        else:
            cock = 1.0
            sweep = ease(min(1.0, t / 0.35))
            settle = ease(max(0.0, (t - 0.35) / 0.65))
            recoil = math.sin(min(1.0, t / 0.4) * math.pi)
        hold = 1.0 - settle
        if melee:
            pose["spine_02"] = twist((-30 * cock + 65 * sweep) * hold)
            pose["upperarm_r"] = (roll_side((75 * cock - 15 * sweep) * hold, -1)[0], 0.0, math.radians((-35 * cock + 95 * sweep) * hold))
            pose["lowerarm_r"] = forward_swing((55 * cock - 45 * sweep) * hold)
            pose["upperarm_l"] = forward_swing(25 * cock * hold)
        else:
            raise_ = 75 * cock * hold - 12 * recoil
            for side in ("l", "r"):
                pose["upperarm_" + side] = forward_swing(raise_)
                pose["lowerarm_" + side] = forward_swing(10 * cock * hold)
                pose["hand_" + side] = forward_swing(-(raise_ + 10 * cock * hold))
            # Turned a little right to aim, rocked back by the recoil.
            pose["spine_02"] = (0.0, forward_swing(-6 * recoil)[1], twist(-10 * cock * hold)[2])
    elif name == "Cast":
        rise = ease(min(1.0, t / 0.4))
        fall = ease(max(0.0, (t - 0.6) / 0.4))
        k = rise * (1 - fall)
        for side, sign in (("l", 1), ("r", -1)):
            pose["upperarm_" + side] = tuple(a + b for a, b in zip(forward_swing(85 * k), roll_side(20 * k, sign)))
            pose["lowerarm_" + side] = forward_swing(10 * k)
        pose["spine_03"] = forward_swing(-8 * k)
        lift = d["height"] * 0.02 * k
    elif name == "Hit":
        k = math.sin(min(1.0, t) * math.pi)
        pose["spine_02"] = forward_swing(-18 * k)
        pose["head"] = forward_swing(-12 * k)
        for side, sign in (("l", 1), ("r", -1)):
            pose["upperarm_" + side] = roll_side(20 * k, sign)
    elif name == "Death":
        # Knocked back off its feet; a rider on a base falls back off it, and the base stays.
        k = ease(min(1.0, t / 0.8))
        if d["base"] > 0:
            pose["pelvis"] = forward_swing(80 * k)
            lift = -d["leg"] * 0.6 * k
        else:
            pose["root"] = forward_swing(80 * k)
            lift = -d["height"] * 0.05 * k
        pose["spine_02"] = forward_swing(15 * k)
        for side in ("l", "r"):
            pose["thigh_" + side] = forward_swing(30 * k)
            pose["calf_" + side] = forward_swing(-50 * k)
    elif name == "Recall":
        settle = ease(min(1.0, t * 4))
        glow = math.sin(t * tau)
        for side, sign in (("l", 1), ("r", -1)):
            pose["upperarm_" + side] = tuple(a + b for a, b in zip(forward_swing(45 * settle), roll_side(-10 * settle, sign)))
            pose["lowerarm_" + side] = forward_swing(70 * settle)
            pose["thigh_" + side] = forward_swing(20 * settle)
            pose["calf_" + side] = forward_swing(-30 * settle)
        pose["head"] = forward_swing(10 * settle + 2 * glow)
        lift = -d["height"] * 0.06 * settle
    else:
        raise AssertionError("Unknown animation: " + name)
    return pose, lift


def animate(armature, spec, d, melee):
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
            pose_rig(armature, rest, *humanoid_pose(name, t, melee, d))
            for bone in armature.pose.bones:
                bone.keyframe_insert("rotation_quaternion", frame=frame + 1)
                if bone.name == "pelvis":
                    bone.keyframe_insert("location", frame=frame + 1)
        action.use_fake_user = True
        actions.append({"name": name, "frames": frames + 1, "loop": clip["loop"]})
    armature.animation_data.action = None
    pose_rig(armature, rest, {}, 0.0)
    return actions


def pose_rig(rig, rest, pose, lift):
    for bone in rig.pose.bones:
        bone.rotation_mode = "QUATERNION"
        bone.rotation_quaternion = local(rest[bone.name], pose.get(bone.name, (0.0, 0.0, 0.0)))
        if bone.name == "pelvis":
            bone.location = rest[bone.name].inverted() @ Vector((0.0, 0.0, lift))


def render_preview(name, armature, obj, d, melee):
    """Rows of the body in its animations' key poses, left to right, for review (not exported): one row turned
    three-quarters toward the camera, one in profile."""
    scene = bpy.context.scene
    rest = rest_quaternions(armature)
    gap = d["full"] * 0.8
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
    camera.data.clip_end = d["full"] * 100
    # The camera looks back along -X, so +Y is its right.
    center = Vector((0.0, (len(PREVIEW_POSES) - 1) * gap / 2, d["full"] * 0.5))
    camera.location = center + Vector((d["full"] * 20, 0.0, d["full"] * 3))
    camera.rotation_euler = (center - camera.location).to_track_quat("-Z", "Y").to_euler()
    scene.render.resolution_x = 2400
    scene.render.resolution_y = round(2400 * d["full"] * 1.3 / camera.data.ortho_scale)
    for suffix, turn in (("", 35.0), ("_side", 90.0)):
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
            pose_rig(rig, rest, *humanoid_pose(clip, t, melee, d))
            made += [rig, body]
        scene.render.filepath = str(SAVED / "Preview" / (name + suffix + ".png"))
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


def build(spec):
    random.seed(spec["seed"])
    capsule = TUNING[spec["id"]]["body"]
    melee = not TUNING[spec["id"]]["basicAttack"].get("projectile")
    reset_scene()
    layout, dims = humanoid_layout(spec, capsule)
    name = "SK_" + spec["id"].title().replace("_", "")
    armature = build_armature(name, layout)
    body = humanoid_body(spec, layout, dims)
    mesh = bpy.data.meshes.new(name)
    body.bm.to_mesh(mesh)
    triangles = sum(len(face.verts) - 2 for face in body.bm.faces)
    body.bm.free()
    # The exporter writes the active colour attribute: the body's colours, and in alpha what glows.
    mesh.color_attributes.active_color = mesh.color_attributes["Col"]
    obj = bpy.data.objects.new(name, mesh)
    bpy.context.scene.collection.objects.link(obj)
    for bone_name, _ in HUMANOID_BONES:
        obj.vertex_groups.new(name=bone_name)
    material = bpy.data.materials.new("M_VeyraVanguardBody")
    mesh.materials.append(material)
    obj.parent = armature
    modifier = obj.modifiers.new("Armature", "ARMATURE")
    modifier.object = armature
    actions = animate(armature, spec, dims, melee)
    budget = KIT["archetypes"][spec["archetype"]]["triangleBudget"]
    assert triangles <= budget, (spec["id"], "triangle budget", triangles, budget)
    # Measured on the rest geometry, as Unreal imports it.
    heights = [vertex.co.z for vertex in mesh.vertices]
    height = max(heights) - min(heights)
    assert abs(min(heights)) < dims["full"] * 0.02, (spec["id"], "the body does not stand on the ground", min(heights))
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
                             colors_type="SRGB")
    if PREVIEW:
        render_preview(name, armature, obj, dims, melee)
    return {"id": spec["id"], "name": name, "archetype": spec["archetype"], "file": "FBX/" + path.name,
            "sha256": hashlib.sha256(path.read_bytes()).hexdigest(), "triangles": triangles, "triangleBudget": budget,
            "bones": len(HUMANOID_BONES), "heightCm": round(height, 2), "capsuleHalfHeightCm": capsule["capsuleHalfHeight"],
            "melee": melee, "runStrideCm": round(run_stride(dims), 2), "upperBodyBone": "spine_01", "animations": actions}


def main():
    assert set(KIT["archetypes"]) == {"humanoid"}, "This generator builds the humanoid archetype"
    entries = [spec for spec in KIT["vanguards"] if ONLY is None or spec["id"] in ONLY]
    assert ONLY is None or len(entries) == len(ONLY), "Unknown Vanguard in --only"
    for spec in entries:
        assert spec["id"] in TUNING, spec["id"] + " is no Vanguard in Vanguards.json"
    SAVED.mkdir(parents=True, exist_ok=True)
    manifest_path = SOURCE / "manifest.json"
    manifest = json.loads(manifest_path.read_text()) if ONLY and manifest_path.exists() else {"assets": []}
    kept = [asset for asset in manifest["assets"] if ONLY and asset["id"] not in ONLY]
    built = [build(spec) for spec in entries]
    manifest = {"generatorVersion": GENERATOR_VERSION, "blender": bpy.app.version_string,
                "kitSha256": hashlib.sha256(KIT_BYTES).hexdigest(),
                "assets": sorted(kept + built, key=lambda asset: asset["id"])}
    manifest_path.write_text(json.dumps(manifest, indent=2) + "\n", newline="\n")
    for asset in built:
        print("VEYRA_VANGUARD_BODY: " + asset["name"] + " " + str(asset["triangles"]) + " triangles, " + str(asset["heightCm"]) + " cm")
    print("VEYRA_VANGUARD_BODIES_PASSED: " + str(len(built)))


main()
