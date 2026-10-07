"""The humanoid archetype (ADR-064 §1): its skeleton, its body fitted to a capsule, its props and its animations."""
import math
import random

from mathutils import Euler, Matrix, Vector

from .parts import Body, combine, ease, forward_swing, lean, mix, roll_side, twist, two_bone

# The humanoid skeleton: every humanoid has every bone, so all share one definition. A bone a body does not use (a
# tail, a prop) carries no weight. (name, parent)
BONES = [
    ("root", None), ("pelvis", "root"), ("spine_01", "pelvis"), ("spine_02", "spine_01"), ("spine_03", "spine_02"),
    ("neck_01", "spine_03"), ("head", "neck_01"),
    ("clavicle_l", "spine_03"), ("upperarm_l", "clavicle_l"), ("lowerarm_l", "upperarm_l"), ("hand_l", "lowerarm_l"), ("prop_l", "hand_l"),
    ("clavicle_r", "spine_03"), ("upperarm_r", "clavicle_r"), ("lowerarm_r", "upperarm_r"), ("hand_r", "lowerarm_r"), ("prop_r", "hand_r"),
    ("thigh_l", "pelvis"), ("calf_l", "thigh_l"), ("foot_l", "calf_l"),
    ("thigh_r", "pelvis"), ("calf_r", "thigh_r"), ("foot_r", "calf_r"),
    ("tail_01", "pelvis"), ("tail_02", "tail_01"), ("tail_03", "tail_02"),
    ("cape_01", "spine_03"), ("cape_02", "cape_01"), ("cape_03", "cape_02"),
]
BUILD = {"lean": 0.9, "normal": 1.0, "heavy": 1.3, "round": 1.15}
# A leg's radius as a share of the body's height, and the foot's radius at heel and toe as shares of it.
LEG_SHARE = 0.052
FOOT_HEEL_SHARE = 0.75
FOOT_TOE_SHARE = 0.8
# How far a running thigh swings either side of straight down, in degrees.
RUN_THIGH_SWING = 35.0
# The share of a melee windup spent drawing back; the rest sweeps through to the hit.
MELEE_COCK_SHARE = 0.65
# The share of the Cast animation that raises the hands to the release, held until a cast commits.
CAST_RELEASE_SHARE = 0.4
# How far down a long cloak reaches, as a share of the height from the ground to the top of the shoulders.
CAPE_REACH = 0.82
# A long cloak's tattered hem: how many strips hang from it.
CAPE_STRIPS = 6
# The share of a wave rider's height the wave beneath her takes up, at her own size.
WAVE_SHARE = 0.22
# A blade carried over the right shoulder: the upper arm's swing forward and roll in, and the forearm's fold, in degrees.
CARRY_UPPER_ARM = 20.0
CARRY_ROLL_IN = 8.0
CARRY_FOREARM = 140.0
CARRY_FLAT_UP = 40.0
# A lantern held out at arm's length: the arm's raise forward and out, and the elbow's bend, in degrees.
LANTERN_RAISE = 60.0
LANTERN_OUT = 15.0
LANTERN_ELBOW = 10.0
# The upper body, which alone plays an attack, a cast or a hit while the body runs; the bone a pose lifts; and whether
# the body stands on the ground.
UPPER_BODY_BONE = "spine_01"
LIFT_BONE = "pelvis"
GROUNDED = True


def layout(spec, capsule):
    """Every bone's head and tail in centimetres, the body facing +X with its left at +Y, fitted to the capsule (grown
    by bodyScale for a body a BodyScale status wears)."""
    features = set(spec["features"])
    grown = spec.get("bodyScale", 1.0)
    # A wave grows under a rider who stays her own size: the figure keeps its share of the ungrown height.
    wave = 1.0 - (1.0 - WAVE_SHARE) / grown if "waveBase" in features else 0.0
    full = capsule["capsuleHalfHeight"] * 2.0 * spec["heightShare"] * grown
    base = full * wave
    footprint = capsule["capsuleRadius"] * grown
    height = full - base
    head = height * spec["headShare"]
    # Stylised for a high camera: a grown figure's legs are 0.44 of its height, and they shorten as the head grows
    # past a grown figure's share, as a smaller figure's do. A production model measured from its reference gives its
    # own share (ADR-069 §5).
    leg = height * spec["legShare"] if "legShare" in spec else height * max(0.30, 0.44 - (spec["headShare"] - 0.16) * 1.1)
    neck = height * 0.03
    torso = height - head - leg - neck
    build = BUILD[spec["build"]]
    # Broad shoulders fill the capsule, so the body's footprint reads as the unit's; a figure small in frame for its
    # capsule (shoulderShare) is sized by its own height instead, the disc under it showing the footprint.
    shoulder = (height * spec["shoulderShare"] if "shoulderShare" in spec else max(capsule["capsuleRadius"] * 0.8, height * 0.13)) * build
    hip = height * spec["hipShare"] * build if "hipShare" in spec else shoulder * 0.62
    arm = height * spec.get("armShare", 0.38) * (0.85 if spec["headShare"] > 0.2 else 1.0)
    digitigrade = "digitigrade" in features
    # The foot's radius at its heel (humanoid_body draws it), so the sole stands on the ground.
    sole = height * LEG_SHARE * build * spec.get("limbScale", 1.0) * FOOT_HEEL_SHARE
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
        L["foot_" + side] = (hock, Vector((hock.x + leg * spec.get("footShare", 0.22), hock.y, base + sole * FOOT_TOE_SHARE)))
    tail_start = Vector((-hip * 0.6, 0, pelvis_z - torso * 0.05))
    tail_dir = Vector((-1.0, 0, -0.35)).normalized()
    tail_length = leg * (1.1 if "heavyTail" in features else 0.5)
    for index, name in enumerate(("tail_01", "tail_02", "tail_03")):
        L[name] = (tail_start + tail_dir * tail_length * index / 3, tail_start + tail_dir * tail_length * (index + 1) / 3)
    # A cloak hangs from behind the shoulders, a little out from the back, down toward the calves.
    cape_start = Vector((-shoulder * 0.45, 0, chest_z - torso * 0.05))
    cape_dir = Vector((-0.2, 0, -1.0)).normalized()
    cape_length = (cape_start.z - base) * CAPE_REACH
    for index, name in enumerate(("cape_01", "cape_02", "cape_03")):
        L[name] = (cape_start + cape_dir * cape_length * index / 3, cape_start + cape_dir * cape_length * (index + 1) / 3)
    if "waveBase" in features:
        # The wave's crest rides the cape bones in a cloak's place: from the back of the swell it rises behind the rider
        # and curls forward at its lip, as tall as the swell is deep.
        assert not features & {"cloak", "longCloak", "trailingScarf"}, (spec["id"], "a wave's crest rides the cape bones")
        # Its unit is her own wave grown with the body: the crest rises that far above the swell she stands on, which is
        # as long as it is deep, so a deeper swell spreads rather than towers.
        unit, reach = max(WAVE_SHARE * full, 0.6 * base), max(footprint, 0.75 * base)
        crest = [Vector((-0.9 * reach, 0, base - 0.5 * unit)), Vector((-1.25 * reach, 0, base + 0.5 * unit)),
                 Vector((-1.15 * reach, 0, base + 1.4 * unit)), Vector((-0.7 * reach, 0, base + 1.8 * unit))]
        for index, name in enumerate(("cape_01", "cape_02", "cape_03")):
            L[name] = (crest[index], crest[index + 1])
    stance = spec.get("stance")
    # A gun carried across the body (braced): its breech and muzzle, for the prop to lie between.
    gun = None
    if stance == "aim":
        # Mid-sight: a long arm held two-handed at the shoulder, its optic at the eye and the left hand forward under the
        # barrel, elbows bent out and down. The weapon points the way the body faces.
        aim = spec.get("aimBore")
        if aim:
            # Fitted to a production model's rifle (ADR-069 §5): its bore runs boreShare of the height above the ground,
            # rightShare to the right of the body's middle, under the eye; the right hand holds its grip and the left
            # its fore-end, each elbow bending toward its own direction.
            bore_z, right = height * aim["boreShare"], height * aim["rightShare"]
            grips = {"r": Vector((height * aim["gripShare"], -right, bore_z - height * aim["gripDropShare"])),
                     "l": Vector((height * aim["foreShare"], -right, bore_z - height * aim["foreDropShare"]))}
            poles = {"r": Vector(aim["rightElbow"]), "l": Vector(aim["leftElbow"])}
        else:
            grip_r = Vector((arm * 0.28, -shoulder * 0.2, chest_z - torso * 0.03))
            grips = {"r": grip_r, "l": grip_r + Vector((arm * 0.5, 0.0, -arm * 0.04))}
            poles = {"r": Vector((0.0, -0.7, -0.7)), "l": Vector((0.0, 0.7, -0.7))}
        for side, sign in (("l", 1.0), ("r", -1.0)):
            shoulder_point = L["upperarm_" + side][0]
            wrist = grips[side] - Vector((arm * 0.12, 0.0, 0.0))
            elbow = two_bone(shoulder_point, wrist, arm * 0.48, arm * 0.40, poles[side])
            L["upperarm_" + side] = (shoulder_point, elbow)
            L["lowerarm_" + side] = (elbow, wrist)
            L["hand_" + side] = (wrist, grips[side])
            L["prop_" + side] = (grips[side], grips[side] + Vector((arm * 0.15, 0, 0)))
    elif stance == "braced":
        # A heavy gun carried across the body on both sides of her: its barrel forward and down under the right arm to
        # the muzzle, its breech up over the left shoulder. The right hand grips under the barrel, the left the breech.
        breech = Vector((shoulder * 0.3, shoulder * 0.75, chest_z + torso * 0.1))
        muzzle = Vector((arm * 1.15, -shoulder * 0.7, chest_z - torso * 0.8))
        gun = (breech, muzzle)
        grips = {"r": breech.lerp(muzzle, 0.62) + Vector((0, 0, -height * 0.05)), "l": breech.lerp(muzzle, 0.2) + Vector((height * 0.02, 0, -height * 0.035))}
        for side, sign in (("l", 1.0), ("r", -1.0)):
            shoulder_point = L["upperarm_" + side][0]
            wrist = grips[side] - Vector((arm * 0.08, 0.0, 0.0))
            elbow = two_bone(shoulder_point, wrist, arm * 0.48, arm * 0.40, Vector((-0.3, sign * 0.7, -0.6)))
            L["upperarm_" + side] = (shoulder_point, elbow)
            L["lowerarm_" + side] = (elbow, wrist)
            L["hand_" + side] = (wrist, grips[side])
            L["prop_" + side] = (grips[side], grips[side] + Vector((arm * 0.15, 0, 0)))
    elif stance not in (None, "shoulderCarry", "lanternOut"):
        # A carry (carry_pose) poses the arm; its rest is the figure's own.
        raise AssertionError("Unknown stance: " + stance)
    strike = spec.get("strike", {"style": "swing", "hand": "right"})
    assert strike["style"] in ("swing", "punch") and strike["hand"] in ("left", "right"), ("Unknown strike", strike)
    still = spec.get("stillPose")
    assert still in (None, "slumped"), ("Unknown still pose", still)
    dims = {"height": height, "full": full, "base": base, "head": head, "torso": torso, "leg": leg, "shoulder": shoulder,
            "hip": hip, "arm": arm, "build": build, "stance": stance, "strike": strike, "stillPose": still, "idle": spec.get("idle"), "kneel": spec.get("kneel", False),
            "hunch": spec.get("hunch", 0.0),
            "footprint": footprint, "ride": "wave" if "waveBase" in features else None, "gun": gun,
            "waveUnit": max(WAVE_SHARE * full, 0.6 * base), "waveReach": max(footprint, 0.75 * base)}
    return L, dims


def cupped_ear(body, root, tip, out, width, head_radius, skin, inner_color, tip_color=None):
    """An animal's ear from root to tip: its base runs across the head along out, width either side, and it is folded
    back down its middle into a cup facing forward, so it has depth seen from the side too. Fur outside, inner_color
    within, and tip_color over its last quarter if given."""
    inner = root - out * width + Vector((0, 0, head_radius * 0.2))
    outer = root + out * width - Vector((0, 0, head_radius * 0.3))
    fold = root - Vector((head_radius * 0.4, 0, 0))
    for half in ([inner, tip, fold], [fold, tip, outer]):
        body.pane("head", half, head_radius * 0.14, skin)
        facing = (half[1] - half[0]).cross(half[2] - half[0]).normalized()
        facing = facing if facing.x > 0 else -facing
        middle = sum(half, Vector()) / 3
        body.pane("head", [point.lerp(middle, 0.3) + facing * head_radius * 0.08 for point in half], head_radius * 0.05, inner_color)
    if tip_color:
        for side in (inner, outer):
            body.pane("head", [tip, side.lerp(tip, 0.72), fold.lerp(tip, 0.72)], head_radius * 0.18, tip_color)


PROSTHETIC_STEEL, PROSTHETIC_BRASS, PROSTHETIC_JOINT = [0.50, 0.50, 0.52], [0.78, 0.60, 0.28], [0.20, 0.20, 0.21]


def prosthetic_arm(body, L, side, limb):
    """A freight-grade arm below the coat sleeve: a brass elbow joint, a forearm of steel bands ringed in brass with a
    piston along its outside, and an articulated gripping hand, its jointed fingers curled a little and a thumb set
    against them."""
    steel, brass, joint = PROSTHETIC_STEEL, PROSTHETIC_BRASS, PROSTHETIC_JOINT
    out = Vector((0, 1 if side == "l" else -1, 0))
    e0, e1 = L["lowerarm_" + side]
    body.ball("lowerarm_" + side, e0, limb * 1.3, brass, segments=8)
    for band in range(4):
        a, b = e0.lerp(e1, 0.08 + band * 0.23), e0.lerp(e1, 0.27 + band * 0.23)
        body.limb("lowerarm_" + side, a, b, limb * (1.15 - band * 0.05), limb * (1.1 - band * 0.05), steel if band % 2 == 0 else mix(steel, joint, 0.4), segments=8)
        body.limb("lowerarm_" + side, b - (b - a) * 0.15, b + (b - a) * 0.05, limb * (1.22 - band * 0.05), limb * (1.22 - band * 0.05), brass, segments=8)
    body.limb("lowerarm_" + side, e0.lerp(e1, 0.15) + out * limb * 1.1, e0.lerp(e1, 0.85) + out * limb * 1.0, limb * 0.2, limb * 0.2, brass, segments=6)
    h0, h1 = L["hand_" + side]
    along = (h1 - h0).normalized()
    across = Vector((1, 0, 0))
    body.ball("hand_" + side, h0, limb * 0.75, joint, segments=8)
    palm = h0 + along * limb * 0.9
    body.blob("hand_" + side, palm, (along * limb * 0.85, across * limb * 0.85, out * limb * 0.45), steel, segments=8)
    for finger in range(4):
        base = palm + along * limb * 0.8 + across * limb * (finger - 1.5) * 0.45
        middle = base + along * limb * 0.75 - out * limb * 0.15
        body.limb("hand_" + side, base, middle, limb * 0.2, limb * 0.18, steel, segments=5)
        body.limb("hand_" + side, middle, middle + along * limb * 0.55 - out * limb * 0.35, limb * 0.17, limb * 0.13, mix(steel, joint, 0.3), segments=5)
    thumb = palm + across * limb * 0.75 - out * limb * 0.1
    body.limb("hand_" + side, thumb, thumb + along * limb * 0.6 - out * limb * 0.45, limb * 0.2, limb * 0.15, steel, segments=5)


def prosthetic_leg(body, L, side, leg):
    """A freight-grade leg from the knee down: a heavy plated knee over a dark joint, a shin of steel bands ringed in
    brass with a piston down its back, and a jointed foot, its toe plate hinged."""
    steel, brass, joint = PROSTHETIC_STEEL, PROSTHETIC_BRASS, PROSTHETIC_JOINT
    c0, c1 = L["calf_" + side]
    body.ball("calf_" + side, c0, leg * 0.95, joint, segments=8)
    body.ball("calf_" + side, c0 + Vector((leg * 0.55, 0, 0)), leg * 1.1, mix(brass, joint, 0.25), scale=(0.6, 1.0, 1.15), segments=8)
    for band in range(3):
        a, b = c0.lerp(c1, 0.12 + band * 0.28), c0.lerp(c1, 0.36 + band * 0.28)
        body.limb("calf_" + side, a, b, leg * (0.9 - band * 0.06), leg * (0.84 - band * 0.06), steel, segments=8)
        body.limb("calf_" + side, b - (b - a) * 0.12, b + (b - a) * 0.04, leg * (0.97 - band * 0.06), leg * (0.97 - band * 0.06), brass, segments=8)
    body.limb("calf_" + side, c0.lerp(c1, 0.1) - Vector((leg * 0.85, 0, 0)), c1 - Vector((leg * 0.65, 0, 0)), leg * 0.22, leg * 0.22, brass, segments=6)
    f0, f1 = L["foot_" + side]
    body.ball("foot_" + side, f0, leg * 0.75, joint, segments=8)
    body.limb("foot_" + side, f0 - (f1 - f0) * 0.1, f0.lerp(f1, 0.65), leg * FOOT_HEEL_SHARE * 1.2, leg * FOOT_HEEL_SHARE * 1.1, steel, segments=8)
    body.limb("foot_" + side, f0.lerp(f1, 0.65), f1 + (f1 - f0) * 0.1, leg * FOOT_HEEL_SHARE * 1.05, leg * FOOT_HEEL_SHARE * FOOT_TOE_SHARE * 1.1, mix(steel, joint, 0.3),
              segments=8)


def body(spec, L, d, bones=BONES):
    """The figure of simple parts on L's bones. Another archetype's skeleton that holds these bones (a rider's) passes
    its own bone list, so the parts weight to the right vertex groups."""
    body = Body([name for name, _ in bones])
    features = set(spec["features"])
    skin, primary, secondary, hair, accent = spec["skin"], spec["primary"], spec["secondary"], spec["hair"], spec["accent"]
    metal = [0.62, 0.55, 0.42]
    # A plush toy's limbs are stubbier and fatter than a figure's.
    plump = spec.get("limbScale", 1.0)
    limb = d["height"] * 0.036 * d["build"] * plump
    leg = d["height"] * LEG_SHARE * d["build"] * plump
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
    if "buttonEyes" in features:
        # A toy's mismatched button eyes, one dark and one pale, sewn on a little crooked, over a soft muzzle with a
        # stitched seam across it. Plain and kindly: no fangs, no glow.
        for sign, color, drop in ((1, [0.06, 0.05, 0.05], 0.0), (-1, [0.85, 0.82, 0.74], 0.12)):
            eye = head_center + Vector((head_radius * 0.9, sign * head_radius * 0.38, head_radius * (0.15 - drop)))
            body.limb("head", eye - Vector((head_radius * 0.06, 0, 0)), eye + Vector((head_radius * 0.06, 0, 0)), head_radius * 0.17, head_radius * 0.17, color, segments=8)
        muzzle = head_center + Vector((head_radius * 0.82, 0, -head_radius * 0.3))
        body.ball("head", muzzle, head_radius * 0.38, mix(skin, [1.0, 0.95, 0.85], 0.35), scale=(0.8, 1.0, 0.75), segments=8)
        body.ball("head", muzzle + Vector((head_radius * 0.28, 0, head_radius * 0.08)), head_radius * 0.11, [0.15, 0.08, 0.06], segments=6)
        body.limb("head", muzzle + Vector((head_radius * 0.3, -head_radius * 0.2, -head_radius * 0.12)), muzzle + Vector((head_radius * 0.3, head_radius * 0.2, -head_radius * 0.05)),
                  head_radius * 0.025, head_radius * 0.025, [0.25, 0.15, 0.12], segments=4)
    elif "foxFace" in features or "hareFace" in features:
        # An animal's face, never a person's: a fox's short pointed muzzle in pale fur with a dark nose, or a hare's
        # rounder one with a pink nose; big amber eyes set wide, and tufts of pale cheek fur flaring out and down.
        fur = spec.get("fur", [0.97, 0.93, 0.85])
        muzzle = head_center + Vector((head_radius * 0.55, 0, -head_radius * 0.28))
        if "foxFace" in features:
            body.limb("head", muzzle, muzzle + Vector((head_radius * 0.62, 0, -head_radius * 0.08)), head_radius * 0.42, head_radius * 0.12, fur, segments=8)
            body.ball("head", muzzle + Vector((head_radius * 0.64, 0, -head_radius * 0.04)), head_radius * 0.11, [0.08, 0.05, 0.05], segments=6)
        else:
            body.ball("head", muzzle + Vector((head_radius * 0.12, 0, 0)), head_radius * 0.42, fur, scale=(0.8, 1.0, 0.75), segments=8)
            body.ball("head", muzzle + Vector((head_radius * 0.45, 0, head_radius * 0.12)), head_radius * 0.1, [0.85, 0.52, 0.52], segments=6)
        for sign in (1, -1):
            eye = head_center + Vector((head_radius * 0.74, sign * head_radius * 0.38, head_radius * 0.1))
            body.ball("head", eye, head_radius * 0.22, spec.get("eyes", [0.92, 0.55, 0.12]), scale=(0.6, 1.0, 1.1), segments=8)
            body.ball("head", eye + Vector((head_radius * 0.1, 0, 0)), head_radius * 0.12, [0.04, 0.03, 0.03], scale=(0.5, 1.0, 1.2), segments=6)
            cheek = head_center + Vector((head_radius * 0.25, sign * head_radius * 0.72, -head_radius * 0.35))
            body.limb("head", cheek, cheek + Vector((-head_radius * 0.05, sign * head_radius * 0.45, -head_radius * 0.3)), head_radius * 0.3, head_radius * 0.04, fur, segments=6)
    else:
        # Eyes: two dark points on the front, so the face shows which way it looks.
        for sign in (1, -1):
            body.ball("head", head_center + Vector((head_radius * 0.85, sign * head_radius * 0.35, head_radius * 0.1)), head_radius * 0.12, [0.05, 0.05, 0.06])
    hair_style = spec["hairStyle"]
    if hair_style in ("short", "long", "curly", "twinTails", "sideTail", "longBeard", "windblown", "bigTwinTails", "curlyLong"):
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
        tie = head_center + Vector((-head_radius * 0.3, head_radius * 0.8, head_radius * 0.6))
        body.limb("head", tie, head_center + Vector((-head_radius * 0.6, head_radius * 1.3, -head_radius * 0.6)), head_radius * 0.28, head_radius * 0.08, hair)
        if spec.get("ribbon"):
            for wing in (1, -1):
                body.ball("head", tie + Vector((wing * head_radius * 0.22, head_radius * 0.05, head_radius * 0.1)), head_radius * 0.2, spec["ribbon"],
                          scale=(1.3, 0.6, 0.8), segments=6)
    if hair_style == "bigTwinTails":
        # Two big, wild tails tied high and flaring out and back, streaked with the accent, each tied with a bow and a
        # bunny clip: from above, the widest thing about her.
        streak = spec.get("hairStreak", accent)
        for sign in (1, -1):
            # Bushy and drooping as they fly back, with a second wild lock beside each: tails, never horns.
            tie = head_center + Vector((-head_radius * 0.25, sign * head_radius * 0.85, head_radius * 0.45))
            mid = tie + Vector((-head_radius * 0.75, sign * head_radius * 0.8, -head_radius * 0.75))
            tip = mid + Vector((-head_radius * 0.8, sign * head_radius * 0.35, -head_radius * 1.4))
            body.ball("head", tie.lerp(mid, 0.45), head_radius * 0.55, hair, segments=8)
            body.limb("head", tie, mid, head_radius * 0.5, head_radius * 0.55, hair, segments=8)
            body.limb("head", mid, tip, head_radius * 0.55, head_radius * 0.12, hair, segments=8)
            body.limb("head", mid + Vector((0, sign * head_radius * 0.2, head_radius * 0.1)), tip + Vector((head_radius * 0.3, sign * head_radius * 0.6, head_radius * 0.4)),
                      head_radius * 0.3, head_radius * 0.06, streak, segments=6)
            body.limb("head", tie.lerp(mid, 0.5) + Vector((0, 0, head_radius * 0.2)), mid.lerp(tip, 0.8) + Vector((-head_radius * 0.2, 0, 0)), head_radius * 0.22, head_radius * 0.05,
                      streak, segments=6)
            for wing in (1, -1):
                body.ball("head", tie + Vector((wing * head_radius * 0.25, 0, head_radius * 0.15)), head_radius * 0.22, [0.55, 0.08, 0.25], scale=(1.3, 0.6, 0.8), segments=6)
            body.ball("head", tie + Vector((head_radius * 0.15, 0, head_radius * 0.35)), head_radius * 0.2, [0.95, 0.93, 0.92], segments=6)
    if hair_style == "windblown":
        # Long hair blown back: a mass from the crown streaming behind the shoulders, its locks fanning out, so from
        # above it trails the head.
        body.ball("head", head_center + Vector((-head_radius * 0.12, 0, head_radius * 0.18)), head_radius * 1.06, hair, scale=(1.05, 1.0, 0.9))
        for index, spread in enumerate((-0.7, -0.25, 0.25, 0.7)):
            tip = head_center + Vector((-head_radius * (2.9 - abs(spread)), spread * head_radius * 1.6, -head_radius * (1.0 + 0.4 * (index % 2))))
            body.limb("head", head_center + Vector((-head_radius * 0.6, spread * head_radius * 0.7, head_radius * 0.3)), tip,
                      head_radius * 0.42, head_radius * 0.1, mix(hair, accent, 0.25 if index % 2 else 0.0))
    if hair_style == "curlyLong":
        # Long dark curls falling to the shoulders behind and beside the face, each lock ending in a curl, and a full
        # beard over the jaw and chin.
        for index in range(9):
            # From one side round the back to the other, leaving the face clear.
            around = math.radians(-95 + index * 23.75)
            out = Vector((-math.cos(around), math.sin(around), 0))
            start = head_center + out * head_radius * 0.75 + Vector((0, 0, head_radius * 0.2))
            tip = start + out * head_radius * 0.35 - Vector((0, 0, head_radius * (1.5 + 0.25 * (index % 2))))
            body.limb("head", start, tip, head_radius * 0.34, head_radius * 0.2, hair, segments=6)
            body.ball("head", tip, head_radius * 0.24, hair, segments=6)
        body.ball("head", head_center + Vector((head_radius * 0.55, 0, -head_radius * 0.65)), head_radius * 0.5, hair, scale=(0.8, 1.2, 0.9), segments=8)
        body.limb("head", head_center + Vector((head_radius * 0.75, 0, -head_radius * 0.85)), head_center + Vector((head_radius * 0.8, 0, -head_radius * 1.35)),
                  head_radius * 0.32, head_radius * 0.12, hair, segments=6)
    # Headwear and head features.
    if "hood" in features:
        # Set back so the face shows through its opening.
        body.ball("head", head_center + Vector((-head_radius * 0.4, 0, head_radius * 0.15)), head_radius * 1.15, primary, scale=(1.0, 1.0, 1.05))
    if "deepHood" in features:
        # A deep hood that holds most of the face in shadow: a cowl reaching forward past the brow, the opening dark,
        # a band of dark markings across the eyes, and only the chin catching the light.
        hood = spec.get("hood", primary)
        body.ball("head", head_center + Vector((-head_radius * 0.3, 0, head_radius * 0.2)), head_radius * 1.25, hood, scale=(1.05, 1.0, 1.1), segments=10)
        body.limb("head", head_center + Vector((head_radius * 0.2, 0, head_radius * 0.25)), head_center + Vector((head_radius * 1.15, 0, head_radius * 0.35)),
                  head_radius * 1.15, head_radius * 0.95, hood, segments=10)
        body.ball("head", head_center + Vector((head_radius * 0.92, 0, head_radius * 0.05)), head_radius * 0.72, [0.03, 0.03, 0.04], scale=(0.3, 1.0, 0.85), segments=8)
        body.box("head", head_center + Vector((head_radius * 0.98, 0, head_radius * 0.05)), (1.0, head_radius * 1.0, head_radius * 0.16), [0.01, 0.01, 0.02])
        body.limb("neck_01", *L["neck_01"], head_radius * 1.15, head_radius * 1.0, hood, segments=10)
        # Its cloth falls to a soft peak behind the crown and drapes over the shoulders as a mantle.
        body.limb("head", head_center + Vector((-head_radius * 0.6, 0, head_radius * 0.7)), head_center + Vector((-head_radius * 1.6, 0, head_radius * 0.3)),
                  head_radius * 0.75, head_radius * 0.1, hood, segments=8)
        n0, n1 = L["neck_01"]
        body.limb("spine_03", n0 + Vector((0, 0, head_radius * 0.2)), n0 - Vector((0, 0, d["torso"] * 0.22)), head_radius * 1.3, d["shoulder"] * 1.18, hood, segments=10)
    if "hoodDown" in features:
        body.limb("neck_01", *L["neck_01"], head_radius * 1.1, head_radius * 0.9, spec.get("hood", secondary))
    if "wideHat" in features:
        body.limb("head", head_center + Vector((0, 0, head_radius * 0.55)), head_center + Vector((0, 0, head_radius * 0.65)), head_radius * 2.0, head_radius * 2.0, secondary)
        body.limb("head", head_center + Vector((0, 0, head_radius * 0.6)), head_center + Vector((0, 0, head_radius * 1.4)), head_radius * 0.95, head_radius * 0.8, secondary)
    if "burnScar" in features:
        # An old burn across the right cheek, raw red against the skin, and a scar running up through it.
        cheek = head_center + Vector((head_radius * 0.74, -head_radius * 0.45, -head_radius * 0.12))
        body.ball("head", cheek, head_radius * 0.3, mix(skin, [0.65, 0.18, 0.12], 0.55), scale=(0.35, 1.0, 0.8), segments=8)
        body.limb("head", cheek + Vector((0, head_radius * 0.05, -head_radius * 0.2)), cheek + Vector((head_radius * 0.05, head_radius * 0.15, head_radius * 0.35)),
                  head_radius * 0.035, head_radius * 0.03, [0.55, 0.15, 0.10], segments=4)
    if "headband" in features:
        # A band round the head holding the hair back off the face.
        body.limb("head", head_center + Vector((-head_radius * 0.12, 0, head_radius * 0.3)), head_center + Vector((-head_radius * 0.12, 0, head_radius * 0.52)),
                  head_radius * 1.07, head_radius * 1.05, spec.get("band", secondary), segments=10)
    if "workGoggles" in features:
        # Work goggles pushed up on the forehead: two short lensed cups on a strap round the head, the glass catching light.
        strap, rim, lens = [0.12, 0.11, 0.10], [0.45, 0.40, 0.32], [0.55, 0.70, 0.72]
        body.limb("head", head_center + Vector((-head_radius * 0.12, 0, head_radius * 0.6)), head_center + Vector((-head_radius * 0.12, 0, head_radius * 0.72)),
                  head_radius * 1.06, head_radius * 1.05, strap, segments=10)
        for sign in (1, -1):
            cup = head_center + Vector((head_radius * 0.82, sign * head_radius * 0.33, head_radius * 0.68))
            body.limb("head", cup - Vector((head_radius * 0.12, 0, -head_radius * 0.04)), cup + Vector((head_radius * 0.12, 0, head_radius * 0.04)),
                      head_radius * 0.24, head_radius * 0.26, rim, segments=8)
            body.ball("head", cup + Vector((head_radius * 0.14, 0, head_radius * 0.05)), head_radius * 0.2, lens, scale=(0.35, 1.0, 1.0), segments=8)
    if "bigHood" in features:
        # An enormous hood over the head and down onto the shoulders: its crown set back so the face shows through a
        # heavy rolled brim, a brass rivet at each temple, its point falling behind, the ears standing up through it.
        hood = spec.get("hood", primary)
        body.ball("head", head_center + Vector((-head_radius * 0.55, 0, head_radius * 0.25)), head_radius * 1.28, hood, scale=(1.0, 1.08, 1.0), segments=12)
        brim = [head_center + Vector((head_radius * 0.55, math.cos(angle) * head_radius * 1.0, math.sin(angle) * head_radius * 1.05 + head_radius * 0.15))
                for angle in (math.radians(-60 + index * 30) for index in range(9))]
        for a, b in zip(brim, brim[1:]):
            body.limb("head", a, b, head_radius * 0.2, head_radius * 0.2, mix(hood, [0.0, 0.0, 0.0], 0.15), segments=6)
        for sign in (1, -1):
            body.limb("head", head_center + Vector((head_radius * 0.35, sign * head_radius * 1.05, head_radius * 0.15)),
                      head_center + Vector((head_radius * 0.45, sign * head_radius * 1.12, head_radius * 0.15)), head_radius * 0.16, head_radius * 0.16,
                      [0.85, 0.62, 0.25], segments=8)
        body.limb("head", head_center + Vector((-head_radius * 1.2, 0, head_radius * 0.6)), head_center + Vector((-head_radius * 2.0, 0, -head_radius * 0.3)),
                  head_radius * 0.6, head_radius * 0.08, hood, segments=8)
        n0, n1 = L["neck_01"]
        body.limb("spine_03", n0 + Vector((0, 0, head_radius * 0.2)), n0 - Vector((0, 0, d["torso"] * 0.18)), head_radius * 0.95, d["shoulder"] * 1.0, hood, segments=10)
    if "fennecEars" in features or "hareEars" in features:
        # A fennec's ears are enormous, standing up and out through the hood, broad, pale fur outside and pink within:
        # from above the widest thing about her. A hare's are long and narrower, swept back over the shoulders and
        # dark at the tips. Both are tufted with fur at their roots.
        fur = spec.get("fur", [0.97, 0.93, 0.85])
        for sign in (1, -1):
            out = Vector((0, sign, 0))
            if "fennecEars" in features:
                root = head_center + Vector((-head_radius * 0.15, sign * head_radius * 0.55, head_radius * 0.75))
                tip = head_center + Vector((-head_radius * 0.9, sign * head_radius * 2.0, head_radius * 2.7))
                cupped_ear(body, root, tip, out, head_radius * 0.75, head_radius, skin, spec.get("earInner", [0.92, 0.62, 0.58]))
            else:
                root = head_center + Vector((-head_radius * 0.3, sign * head_radius * 0.4, head_radius * 0.8))
                tip = head_center + Vector((-head_radius * 3.4, sign * head_radius * 1.3, head_radius * 1.7))
                cupped_ear(body, root, tip, out, head_radius * 0.55, head_radius, skin, spec.get("earInner", [0.92, 0.62, 0.58]), spec.get("earTips"))
            body.limb("head", root + Vector((head_radius * 0.1, 0, 0)), root + Vector((head_radius * 0.35, sign * head_radius * 0.2, head_radius * 0.45)),
                      head_radius * 0.22, head_radius * 0.04, fur, segments=6)
    if "roundEars" in features:
        for sign in (1, -1):
            body.ball("head", head_center + Vector((0, sign * head_radius * 0.75, head_radius * 0.75)), head_radius * 0.35, skin)
    # Arms; long sleeves and gloves cover the forearms and hands, and prosthetic arms are machined below the sleeve.
    arm_color = spec.get("sleeves", skin)
    for side in ("l", "r"):
        body.limb("clavicle_" + side, *L["clavicle_" + side], limb * 1.2, limb * 1.25, primary)
        body.limb("upperarm_" + side, *L["upperarm_" + side], limb * 1.25, limb * 1.0, primary)
        if "prostheticArms" in features:
            prosthetic_arm(body, L, side, limb)
            continue
        body.limb("lowerarm_" + side, *L["lowerarm_" + side], limb * 1.0, limb * 0.85, arm_color)
        h0, h1 = L["hand_" + side]
        # Big hands read at a distance.
        body.ball("hand_" + side, (h0 + h1) / 2, limb * 1.35, spec.get("gloves", arm_color))
    # Legs and feet; boots in the secondary colour. Shorts leave the legs bare below them.
    bare = "shorts" in features
    for side in ("l", "r"):
        t0, t1 = L["thigh_" + side]
        body.limb("thigh_" + side, t0, t1, leg * 1.15, leg * 0.95, skin if bare else secondary)
        if bare:
            body.limb("thigh_" + side, t0 - (t1 - t0) * 0.1, t0.lerp(t1, 0.4), leg * 1.3, leg * 1.2, secondary)
        if "prostheticLegs" in features:
            prosthetic_leg(body, L, side, leg)
            continue
        body.limb("calf_" + side, *L["calf_" + side], leg * 0.95, leg * 0.75, skin if bare else secondary)
        f0, f1 = L["foot_" + side]
        body.limb("foot_" + side, f0, f1, leg * FOOT_HEEL_SHARE, leg * FOOT_HEEL_SHARE * FOOT_TOE_SHARE, spec.get("feet", mix(secondary, [0.05, 0.05, 0.05], 0.4)))
    if "hareLegs" in features:
        # A hare's powerful hind legs: great furred haunches over the thighs, bunched to spring.
        for side in ("l", "r"):
            t0, t1 = L["thigh_" + side]
            body.limb("thigh_" + side, t0 + Vector((leg * 0.2, 0, leg * 0.3)), t1, leg * 1.6, leg * 1.05, skin, segments=10)
    # Clothing and armour.
    s0, s1 = L["spine_03"]
    if "cloak" in features:
        body.box("spine_03", s0 + Vector((-d["shoulder"] * 0.75, 0, -d["torso"] * 0.55)), (d["shoulder"] * 0.12, d["shoulder"] * 1.6, d["torso"] * 1.5), secondary)
    if "longCloak" in features:
        # A long cloak from the shoulders toward the calves, flaring as it falls, its hem torn into strips of
        # different lengths (seeded). Each length rides its own bone, so it streams back as the body runs.
        cloak = spec.get("cloak", secondary)
        # As wide as the shoulders, or a narrower tail of cloth (cloakWidth, a share of that).
        wide = spec.get("cloakWidth", 1.0)
        top = L["cape_01"][0]
        body.box("cape_01", top + Vector((d["shoulder"] * 0.15, 0, d["torso"] * 0.05)), (d["shoulder"] * 0.5, d["shoulder"] * 2.1 * wide, d["torso"] * 0.22), cloak)
        for index, name in enumerate(("cape_01", "cape_02", "cape_03")):
            c0, c1 = L[name]
            overlap = (c1 - c0) * 0.08
            body.slab(name, c0 - overlap, c1 + overlap, d["shoulder"] * (2.0 + index * 0.15) * wide, 2.5, cloak)
        c0, c1 = L["cape_03"]
        hem = d["shoulder"] * 2.3 * wide
        down = (c1 - c0).normalized()
        for strip in range(CAPE_STRIPS):
            across = Vector((0, -hem / 2 + hem * (strip + 0.5) / CAPE_STRIPS, 0))
            length = d["height"] * random.uniform(0.03, 0.12)
            body.slab("cape_03", c1 + across - down * 1.0, c1 + across + down * length, hem / CAPE_STRIPS * 0.8, 2.0,
                      mix(cloak, [0.05, 0.02, 0.02], random.uniform(0.0, 0.3)))
    if "scarf" in features:
        body.box("neck_01", L["neck_01"][0] + Vector((-limb * 1.6, 0, -d["torso"] * 0.25)), (limb * 0.5, limb * 2.0, d["torso"] * 0.7), secondary)
    if "trailingScarf" in features:
        # A torn scarf wound at the neck, its long tail riding the cape bones so it trails and streams behind.
        scarf = spec.get("scarf", secondary)
        n0, n1 = L["neck_01"]
        body.limb("neck_01", n0 - Vector((0, 0, limb * 0.6)), n0 + Vector((0, 0, limb * 0.9)), limb * 2.4, limb * 2.2, scarf, segments=10)
        for index, name in enumerate(("cape_01", "cape_02", "cape_03")):
            c0, c1 = L[name]
            offset = Vector((0, d["shoulder"] * 0.3, 0))
            body.slab(name, c0 + offset, c1 + offset, limb * (2.2 - index * 0.4), 1.5, mix(scarf, [0.1, 0.02, 0.02], index * 0.15))
    if "stitchHeart" in features:
        # A red child's stitch heart, marked with a pale cross, on the chest.
        c0, c1 = L["spine_02"]
        front = c1 + Vector((d["shoulder"] * 0.82, 0, -d["torso"] * 0.05))
        heart = [0.75, 0.08, 0.08]
        for sign in (1, -1):
            body.ball("spine_02", front + Vector((0, sign * d["shoulder"] * 0.14, d["shoulder"] * 0.08)), d["shoulder"] * 0.16, heart, scale=(0.4, 1.0, 1.0), segments=8)
        body.limb("spine_02", front + Vector((0, 0, d["shoulder"] * 0.05)), front - Vector((0, 0, d["shoulder"] * 0.28)), d["shoulder"] * 0.24, d["shoulder"] * 0.02,
                  heart, segments=8)
        bar, stroke = d["shoulder"] * 0.24, d["shoulder"] * 0.05
        for size in ((1.0, bar, stroke), (1.0, stroke, bar)):
            body.box("spine_02", front + Vector((d["shoulder"] * 0.08, 0, 0)), size, [0.92, 0.88, 0.80])
    if "harness" in features:
        # A small leather harness: two straps crossing the chest and a belt.
        top_l, top_r = L["clavicle_l"][1], L["clavicle_r"][1]
        low = L["pelvis"][1]
        for top, other in ((top_l, -1), (top_r, 1)):
            body.slab("spine_03", top.lerp(L["spine_03"][1], 0.35) + Vector((d["shoulder"] * 0.62, 0, 0)), low + Vector((d["hip"] * 0.98, other * d["hip"] * 0.6, 0)),
                      limb * 0.7, 1.5, [0.30, 0.18, 0.10])
        body.limb("pelvis", p1 - Vector((0, 0, d["torso"] * 0.05)), p1 + Vector((0, 0, d["torso"] * 0.05)), d["hip"] * 1.12, d["hip"] * 1.12, [0.30, 0.18, 0.10])
    if "furCuffs" in features:
        # Her own pale fur tufting out at the wrists and over the boot tops: the animal under the clothes.
        fur = spec.get("fur", [0.97, 0.93, 0.85])
        for side in ("l", "r"):
            e0, e1 = L["lowerarm_" + side]
            body.limb("lowerarm_" + side, e0.lerp(e1, 0.8), e1, limb * 1.3, limb * 1.55, fur, segments=8)
            c0, c1 = L["calf_" + side]
            body.limb("calf_" + side, c0.lerp(c1, 0.42), c0.lerp(c1, 0.56), leg * 1.3, leg * 1.15, fur, segments=8)
    if "trinketSatchel" in features:
        # A tinker's working gear: a satchel on the right hip on a strap across the body from the left shoulder, its
        # flap buckled in brass, a cut blue crystal charm and a brass cog hung from it, and two pouches on the belt.
        leather, dark, brass = [0.36, 0.22, 0.12], [0.22, 0.13, 0.07], [0.85, 0.62, 0.25]
        hip = d["hip"]
        bag = p1 + Vector((hip * 0.35, -hip * 1.3, -d["torso"] * 0.12))
        body.box("pelvis", bag, (hip * 0.7, hip * 0.45, hip * 0.6), leather)
        body.box("pelvis", bag + Vector((0, -hip * 0.04, hip * 0.2)), (hip * 0.74, hip * 0.5, hip * 0.25), dark)
        body.ball("pelvis", bag + Vector((hip * 0.1, -hip * 0.27, hip * 0.05)), hip * 0.08, brass, segments=6)
        body.slab("spine_03", L["clavicle_l"][1] + Vector((d["shoulder"] * 0.2, -d["shoulder"] * 0.1, 0)), bag + Vector((0, hip * 0.1, hip * 0.3)), limb * 0.6, 1.5, leather)
        charm = bag + Vector((hip * 0.3, -hip * 0.22, -hip * 0.5))
        body.limb("pelvis", charm, charm + Vector((0, 0, hip * 0.16)), hip * 0.11, hip * 0.01, accent, glow=True, segments=6)
        body.limb("pelvis", charm, charm - Vector((0, 0, hip * 0.2)), hip * 0.11, hip * 0.01, accent, glow=True, segments=6)
        cog = bag + Vector((hip * 0.3, hip * 0.15, -hip * 0.42))
        body.limb("pelvis", cog - Vector((hip * 0.02, 0, 0)), cog + Vector((hip * 0.02, 0, 0)), hip * 0.13, hip * 0.13, brass, segments=8)
        for sign in (1, 0.3):
            body.box("pelvis", p1 + Vector((hip * 1.0, sign * hip * 0.75, -d["torso"] * 0.08)), (hip * 0.25, hip * 0.35, hip * 0.35), leather)
    if "courierSatchel" in features:
        # A wayrunner's map-and-letter satchel, nothing in it magical: a broad flat bag riding the left hip on a strap
        # across the body from the right shoulder, letters standing out from under its buckled flap, and a rolled map
        # in a leather tube strapped along its top.
        leather, dark, brass, paper = [0.38, 0.24, 0.13], [0.24, 0.15, 0.08], [0.80, 0.60, 0.25], [0.92, 0.88, 0.76]
        hip = d["hip"]
        bag = p1 + Vector((-hip * 0.1, hip * 1.3, -d["torso"] * 0.12))
        body.box("pelvis", bag, (hip * 1.0, hip * 0.4, hip * 0.7), leather)
        body.box("pelvis", bag + Vector((0, hip * 0.04, hip * 0.24)), (hip * 1.04, hip * 0.45, hip * 0.28), dark)
        body.ball("pelvis", bag + Vector((hip * 0.1, hip * 0.25, hip * 0.08)), hip * 0.07, brass, segments=6)
        for index in range(3):
            body.box("pelvis", bag + Vector((-hip * 0.25 + index * hip * 0.2, 0, hip * 0.42)), (hip * 0.16, hip * 0.03, hip * 0.3), paper,
                     rotation=Euler((0, math.radians(-12 + index * 12), 0)))
        tube = bag + Vector((0, 0, hip * 0.52))
        body.limb("pelvis", tube - Vector((hip * 0.65, 0, 0)), tube + Vector((hip * 0.65, 0, 0)), hip * 0.13, hip * 0.13, leather, segments=8)
        body.limb("pelvis", tube + Vector((hip * 0.6, 0, 0)), tube + Vector((hip * 0.72, 0, 0)), hip * 0.11, hip * 0.11, paper, segments=8)
        body.slab("spine_03", L["clavicle_r"][1] + Vector((d["shoulder"] * 0.2, d["shoulder"] * 0.1, 0)), bag + Vector((0, -hip * 0.1, hip * 0.3)), limb * 0.6, 1.5, leather)
    if "toolBelt" in features:
        # An engineer's working tools on her belt: pouches round its front and left, and a heavy wrench and a hammer hung
        # at the right hip.
        leather, steel, handle = [0.28, 0.18, 0.10], [0.55, 0.55, 0.56], [0.40, 0.26, 0.14]
        hip = d["hip"]
        for index in range(3):
            around = math.radians(-20 + index * 45)
            body.box("pelvis", p1 + Vector((math.cos(around) * hip * 1.15, math.sin(around) * hip * 1.15, -d["torso"] * 0.06)), (hip * 0.22, hip * 0.32, hip * 0.38),
                     leather, rotation=Euler((0, 0, around)))
        hang = p1 + Vector((hip * 0.2, -hip * 1.22, -d["torso"] * 0.08))
        bottom = hang - Vector((0, 0, d["leg"] * 0.35))
        body.limb("pelvis", hang, bottom, limb * 0.25, limb * 0.25, steel, segments=6)
        for sign in (1, -1):
            body.limb("pelvis", bottom, bottom + Vector((sign * limb * 0.6, 0, -limb * 0.5)), limb * 0.28, limb * 0.22, steel, segments=6)
        top = hang + Vector((-hip * 0.45, 0, 0))
        body.limb("pelvis", top, top - Vector((0, 0, d["leg"] * 0.3)), limb * 0.2, limb * 0.2, handle, segments=6)
        body.box("pelvis", top + Vector((0, 0, limb * 0.2)), (limb * 1.3, limb * 0.5, limb * 0.5), steel)
    if "tornCoat" in features:
        # The coat's hem torn to ribbons, its red lining showing through every rent: strips of different lengths
        # hanging round the skirt, dark and red in turn.
        lining = spec.get("lining", accent)
        hem = p0 - Vector((0, 0, d["leg"] * 0.5))
        for index in range(10):
            angle = index / 10 * math.tau
            out = Vector((math.cos(angle), math.sin(angle), 0))
            top = hem + out * d["hip"] * 1.42
            length = d["leg"] * random.uniform(0.12, 0.3)
            body.slab("pelvis", top + Vector((0, 0, d["leg"] * 0.08)), top + out * d["hip"] * 0.08 - Vector((0, 0, length)), d["hip"] * 0.42, 1.2,
                      lining if index % 3 == 1 else primary)
    if "gauntlets" in features:
        # Banded gauntlets on both forearms.
        for side in ("l", "r"):
            e0, e1 = L["lowerarm_" + side]
            for band in range(3):
                body.limb("lowerarm_" + side, e0.lerp(e1, 0.35 + band * 0.22), e0.lerp(e1, 0.47 + band * 0.22), limb * 1.25, limb * 1.25, [0.22, 0.21, 0.22], segments=8)
    if "spectralBear" in features:
        spectral_bear(body, spec, L, d)
    if "chainLoops" in features:
        # Loops of heavy chain over the coat: across the chest from the left shoulder to the right hip, and round the
        # waist, witchfire running along them.
        iron = [0.16, 0.14, 0.15]
        size = d["height"] * 0.02
        top = L["clavicle_l"][1] + Vector((d["shoulder"] * 0.35, -d["shoulder"] * 0.1, 0))
        low = p1 + Vector((d["hip"] * 1.0, -d["hip"] * 0.9, -d["torso"] * 0.05))
        chain_links(body, "spine_03", [top, top.lerp(low, 0.5) + Vector((d["shoulder"] * 0.3, 0, 0)), low], size, iron, accent)
        ring = [p1 + Vector((math.cos(angle) * d["hip"] * 1.15, math.sin(angle) * d["hip"] * 1.2, -d["torso"] * 0.05 - math.cos(angle) * d["torso"] * 0.04))
                for angle in [index / 8 * math.tau for index in range(9)]]
        chain_links(body, "pelvis", ring, size, iron)
    if "witchfire" in features:
        # Violet-black witchfire gathered in both hands: a burning core and tongues of flame rising from it.
        for side in ("l", "r"):
            h0, h1 = L["hand_" + side]
            centre = (h0 + h1) / 2 + Vector((d["height"] * 0.02, 0, 0))
            body.ball("hand_" + side, centre, limb * 1.6, accent, glow=True)
            for index in range(4):
                angle = index / 4 * math.tau
                root = centre + Vector((math.cos(angle) * limb * 0.9, math.sin(angle) * limb * 0.9, 0))
                body.limb("hand_" + side, root, root + Vector((math.cos(angle) * limb * 0.6, math.sin(angle) * limb * 0.6, limb * 3.0)), limb * 0.5, limb * 0.05,
                          mix(accent, [0.05, 0.0, 0.1], 0.3 * (index % 2)), glow=True, segments=5)
    if "patches" in features:
        # Repairs all over: square patches of other cloth sewn on.
        for bone, at, size in (("spine_02", 0.6, 0.35), ("upperarm_r", 0.5, 0.18), ("thigh_l", 0.4, 0.2)):
            b0, b1 = L[bone]
            body.box(bone, b0.lerp(b1, at) + Vector((d["shoulder"] * size * 1.6, 0, 0)), (1.5, d["shoulder"] * size, d["shoulder"] * size),
                     mix(skin, [0.55, 0.42, 0.30], 0.6), rotation=Euler((0.3, 0, 0)))
    if "coatSkirt" in features:
        body.limb("pelvis", p0 + Vector((0, 0, d["torso"] * 0.05)), p0 - Vector((0, 0, d["leg"] * 0.55)), d["hip"] * 1.15, d["hip"] * 1.55, primary)
    if "kneeGuards" in features:
        for side in ("l", "r"):
            body.ball("calf_" + side, L["calf_" + side][0] + Vector((leg * 0.5, 0, 0)), leg * 0.8, metal)
    if "heavyShoulderPlate" in features:
        # A heavy plate over the left shoulder, overhanging it: layered, so its edge shows from above.
        top = L["clavicle_l"][1] + Vector((0, 0, limb * 0.8))
        for layer in range(3):
            body.ball("clavicle_l", top + Vector((-limb * 0.3 * layer, limb * 0.4 * layer, -limb * 0.7 * layer)), limb * (2.6 - layer * 0.35),
                      mix(mix(secondary, metal, 0.65), [0.30, 0.10, 0.06], 0.3 * layer), scale=(1.15, 1.0, 0.45))
    if "openJacket" in features:
        # An open riding jacket: its front panels part over a dark top, its hem flaring just past the waist, its
        # collar turned up behind the neck.
        jacket = spec.get("jacket", primary)
        s0, s1 = L["spine_02"]
        top = L["spine_03"][0] + Vector((0, 0, d["torso"] * 0.15))
        # What it is open over: a dark top, or a bare chest.
        under = skin if spec.get("jacketUnder") == "skin" else secondary
        body.box("spine_02", (s0 + s1) / 2 + Vector((d["shoulder"] * 0.62, 0, 0)), (d["shoulder"] * 0.2, d["shoulder"] * 0.55, d["torso"] * 0.45), under)
        if "tattoos" in features and spec.get("jacketUnder") == "skin":
            # Heavy nautical tattoo work across the bare chest: a coil over each side of it.
            ink = spec["tattoo"]
            front = (s0 + s1) / 2 + Vector((d["shoulder"] * 0.72, 0, d["torso"] * 0.08))
            for sign in (1, -1):
                coil = front + Vector((0, sign * d["shoulder"] * 0.24, 0))
                for ring, (radius, color) in enumerate(((0.13, ink), (0.09, skin), (0.05, ink))):
                    body.ball("spine_02", coil + Vector((ring * 0.6, 0, 0)), d["shoulder"] * radius, color, scale=(0.12, 1.0, 1.0), segments=10)
        for sign in (1, -1):
            body.slab("spine_02", top + Vector((d["shoulder"] * 0.58, sign * d["shoulder"] * 0.5, 0)),
                      s0 + Vector((d["shoulder"] * 0.7, sign * d["shoulder"] * 0.72, -d["torso"] * 0.12)), d["shoulder"] * 0.42, 3.0, jacket)
            body.slab("pelvis", p1 + Vector((-d["hip"] * 0.1, sign * d["hip"] * 1.0, 0)),
                      p0 + Vector((-d["hip"] * 0.2, sign * d["hip"] * 1.3, -d["torso"] * 0.1)), d["hip"] * 0.9, 3.0, jacket)
        body.slab("spine_03", L["spine_03"][1] + Vector((-d["shoulder"] * 0.35, 0, -d["torso"] * 0.05)),
                  L["spine_03"][1] + Vector((-d["shoulder"] * 0.45, 0, d["torso"] * 0.12)), d["shoulder"] * 0.9, 3.0, jacket)
    if "tattoos" in features:
        # And down the bare forearms in bands, like waves.
        for side in ("l", "r"):
            e0, e1 = L["lowerarm_" + side]
            for share in (0.2, 0.45, 0.7):
                radius = limb * (1.0 - 0.15 * share) * 1.05
                body.limb("lowerarm_" + side, e0.lerp(e1, share), e0.lerp(e1, share + 0.08), radius, radius, spec["tattoo"])
    if "ropeCoil" in features:
        # A coil of rope over the left shoulder: a loop slung from it across the chest to the right hip and round the
        # back, and loops bundled on the shoulder.
        rope = spec.get("rope", [0.62, 0.52, 0.34])
        s0, s1 = L["spine_02"]
        centre = s0.lerp(s1, 0.6)
        across = Vector((0, d["shoulder"] * 0.95, d["torso"] * 0.5))
        out = Vector((d["shoulder"], 0, 0))
        # Laid on the body all the way round, hugging it rather than hooped about it.
        loop = []
        for k in range(15):
            point = centre + across * math.cos(k / 14 * math.tau) + out * math.sin(k / 14 * math.tau)
            flat = Vector((point.x, point.y, 0))
            point = Vector((0, 0, point.z)) + flat.normalized() * (torso_radius(L, d, point.z) + limb * 0.35)
            loop.append(point)
        for a, b in zip(loop, loop[1:]):
            body.limb("spine_02", a, b, limb * 0.42, limb * 0.42, rope, segments=6)
        top = L["clavicle_l"][1] + Vector((0, -limb * 0.2, limb * 0.9))
        for ring in range(3):
            body.limb("clavicle_l", top + Vector((-limb * 0.6, 0, -limb * 0.5 * ring)), top + Vector((limb * 0.6, 0, -limb * 0.5 * ring - limb * 0.3)),
                      limb * 1.15, limb * 1.15, mix(rope, [0.3, 0.25, 0.18], 0.15 * ring), segments=10)
    if "anchorBelt" in features:
        # A broad leather belt with an anchor cast into its brass plate, and a red sash knotted at the left hip.
        leather, brass, sash = spec.get("belt", [0.30, 0.18, 0.10]), [0.78, 0.60, 0.28], spec.get("sash", [0.55, 0.10, 0.08])
        unit = d["height"]
        belt = p1 - Vector((0, 0, d["torso"] * 0.02))
        body.limb("pelvis", belt - Vector((0, 0, unit * 0.025)), belt + Vector((0, 0, unit * 0.025)), d["hip"] * 1.14, d["hip"] * 1.14, leather)
        plate = belt + Vector((d["hip"] * 1.16, 0, 0))
        body.box("pelvis", plate, (unit * 0.01, unit * 0.075, unit * 0.06), mix(brass, leather, 0.4))
        anchor = plate + Vector((unit * 0.008, 0, 0))
        body.limb("pelvis", anchor + Vector((0, 0, unit * 0.022)), anchor - Vector((0, 0, unit * 0.02)), unit * 0.005, unit * 0.005, brass, segments=6)
        body.limb("pelvis", anchor + Vector((0, unit * 0.016, unit * 0.014)), anchor + Vector((0, -unit * 0.016, unit * 0.014)), unit * 0.004, unit * 0.004, brass, segments=6)
        for sign in (1, -1):
            body.limb("pelvis", anchor - Vector((0, 0, unit * 0.02)), anchor + Vector((0, sign * unit * 0.022, -unit * 0.006)), unit * 0.005, unit * 0.003, brass, segments=6)
        knot = belt + Vector((d["hip"] * 0.4, d["hip"] * 1.1, 0))
        body.ball("pelvis", knot, unit * 0.025, sash, segments=8)
        for spread in (-0.4, 0.4):
            body.limb("pelvis", knot, knot + Vector((d["hip"] * 0.2 * spread, d["hip"] * 0.25, -unit * 0.16)), unit * 0.018, unit * 0.01, sash, segments=6)
    if "ribbonHems" in features:
        # Storm cloth cut into long ribbons at every hem, so her edges move with the air: round the coat's skirt (on the
        # thighs, swinging as she walks), from the sleeves, and down the back (on the cape bones, streaming as she runs).
        cloth = [primary, secondary, mix(primary, secondary, 0.5), mix(primary, [0.75, 0.80, 0.85], 0.2)]
        hem_z = p0.z - d["leg"] * 0.55
        for index in range(12):
            around = index / 12 * math.tau
            side = "l" if math.sin(around) >= 0 else "r"
            out = Vector((math.cos(around), math.sin(around), 0))
            top = Vector((p0.x, p0.y, hem_z)) + out * d["hip"] * 1.45
            length = d["leg"] * random.uniform(0.18, 0.4)
            body.slab("thigh_" + side, top + Vector((0, 0, d["leg"] * 0.06)), top + out * d["hip"] * 0.15 - Vector((0, 0, length)), d["hip"] * 0.36, 1.5,
                      cloth[index % len(cloth)])
        for side in ("l", "r"):
            e0, e1 = L["lowerarm_" + side]
            for strip in range(3):
                start = e0.lerp(e1, 0.75) + Vector((0, (strip - 1) * limb * 0.7, 0))
                body.slab("lowerarm_" + side, start, start + (e1 - e0).normalized() * limb * random.uniform(3.5, 5.5) - Vector((0, 0, limb * 1.5)),
                          limb * 0.6, 1.2, cloth[(strip + 1) % len(cloth)])
        for index, name in enumerate(("cape_01", "cape_02", "cape_03")):
            c0, c1 = L[name]
            for strip in range(3):
                across = Vector((0, (strip - 1) * d["shoulder"] * 0.5, 0))
                body.slab(name, c0 + across, c1 + across + (c1 - c0) * 0.1, d["shoulder"] * 0.32, 1.5, cloth[(index + strip) % len(cloth)])
    if "pilotGear" in features:
        # A heavy chained coat hung with working navigational gear: chains across the chest, tuned bells, sea-glass,
        # keys and small weights on them, and a ship's-wheel charm at the belt. Every piece is a tool.
        iron, brass, glass = [0.22, 0.22, 0.24], [0.72, 0.58, 0.30], spec.get("seaGlass", [0.35, 0.75, 0.70])
        unit = d["height"]
        s0, s1 = L["spine_02"]
        front = (s0 + s1) / 2 + Vector((d["shoulder"] * 0.8, 0, 0))
        for drop in (0.0, 1.0):
            chain = [front + Vector((-d["shoulder"] * 0.25, d["shoulder"] * 0.7, d["torso"] * (0.2 - drop * 0.18))),
                     front + Vector((0, 0, -d["torso"] * (0.05 + drop * 0.18))),
                     front + Vector((-d["shoulder"] * 0.25, -d["shoulder"] * 0.7, d["torso"] * (0.2 - drop * 0.18)))]
            chain_links(body, "spine_02", chain, unit * 0.011, iron)
        hung = [(0.25, "bell"), (0.45, "glass"), (0.6, "key"), (0.8, "bell"), (0.35, "weight")]
        for at, kind in hung:
            spot = front + Vector((0, d["shoulder"] * (0.7 - at * 1.4), -d["torso"] * (0.05 + 0.15 * (1 - abs(at - 0.5) * 2))))
            if kind == "bell":
                body.limb("spine_02", spot, spot - Vector((0, 0, unit * 0.03)), unit * 0.006, unit * 0.017, brass, segments=8)
            elif kind == "glass":
                body.blob("spine_02", spot - Vector((0, 0, unit * 0.015)), (Vector((unit * 0.006, 0, 0)), Vector((0, unit * 0.01, 0)), Vector((0, 0, unit * 0.017))),
                          glass, glow=True, segments=4)
            elif kind == "key":
                body.limb("spine_02", spot, spot - Vector((0, 0, unit * 0.035)), unit * 0.003, unit * 0.003, brass, segments=4)
                body.limb("spine_02", spot - Vector((0, 0, unit * 0.03)), spot - Vector((0, -unit * 0.01, unit * 0.03)), unit * 0.003, unit * 0.003, brass, segments=4)
            else:
                body.ball("spine_02", spot - Vector((0, 0, unit * 0.012)), unit * 0.009, iron, segments=6)
        wheel = p1 + Vector((d["hip"] * 0.9, d["hip"] * 0.75, -unit * 0.04))
        body.limb("pelvis", wheel - Vector((unit * 0.002, 0, 0)), wheel + Vector((unit * 0.002, 0, 0)), unit * 0.025, unit * 0.025, brass, segments=10)
        for spoke in range(4):
            angle = spoke / 4 * math.pi
            reach = Vector((0, math.cos(angle), math.sin(angle))) * unit * 0.034
            body.limb("pelvis", wheel - reach, wheel + reach, unit * 0.003, unit * 0.003, brass, segments=4)
    if "hazardCoat" in features:
        # A heavy layered work coat open at the front over its red lining, its ragged hem painted in red-and-white hazard
        # stripes, and stained and burned from the work.
        lining = spec.get("lining", secondary)
        s0, s1 = L["spine_02"]
        for sign in (1, -1):
            edge = (s0 + s1) / 2 + Vector((d["shoulder"] * 0.82, sign * d["shoulder"] * 0.22, 0))
            body.slab("spine_02", edge + Vector((0, 0, d["torso"] * 0.35)), edge - Vector((0, 0, d["torso"] * 0.7)), d["shoulder"] * 0.14, 2.0, lining)
        hem_z = p0.z - d["leg"] * 0.55
        stripes = [[0.75, 0.10, 0.08], [0.92, 0.90, 0.86]]
        for index in range(14):
            around = index / 14 * math.tau
            out = Vector((math.cos(around), math.sin(around), 0))
            top = Vector((p0.x, p0.y, hem_z)) + out * d["hip"] * 1.5
            length = d["leg"] * random.uniform(0.08, 0.16)
            side = "l" if math.sin(around) >= 0 else "r"
            body.slab("thigh_" + side, top + Vector((0, 0, d["leg"] * 0.04)), top + out * d["hip"] * 0.06 - Vector((0, 0, length)), d["hip"] * 0.36, 1.6,
                      stripes[index % 2])
        grime = [mix(primary, [0.05, 0.04, 0.03], 0.5), mix(primary, [0.30, 0.18, 0.08], 0.4)]
        for bone, at, out in (("spine_01", Vector((d["shoulder"] * 0.75, d["shoulder"] * 0.4, 0)), 0), ("spine_02", Vector((-d["shoulder"] * 0.8, -d["shoulder"] * 0.3, 0)), 1),
                              ("pelvis", Vector((d["hip"] * 1.2, -d["hip"] * 0.6, -d["torso"] * 0.2)), 0), ("upperarm_r", Vector((0, -limb * 1.1, 0)), 1)):
            b0, b1 = L[bone]
            body.ball(bone, (b0 + b1) / 2 + at, limb * random.uniform(0.7, 1.1), grime[out], scale=(0.25, 1.0, 1.3), segments=6)
    if "canisterBelts" in features:
        # Belts of sealed canisters, bottles and reagent hardware crossed over her body: labelled, valved, strapped.
        leather, steel, valve = [0.25, 0.16, 0.10], [0.55, 0.55, 0.56], [0.80, 0.62, 0.20]
        labels = [[0.92, 0.80, 0.15], [0.90, 0.90, 0.88], [0.70, 0.12, 0.10]]
        unit = d["height"]
        s0, s1 = L["spine_02"]
        for sign in (1, -1):
            top = L["clavicle_" + ("l" if sign > 0 else "r")][1] + Vector((0, -sign * d["shoulder"] * 0.2, limb * 0.6))
            low = p1 + Vector((d["hip"] * 0.4, -sign * d["hip"] * 1.05, 0))
            front = [top + Vector((d["shoulder"] * 0.55, 0, -d["torso"] * 0.1)), (s0 + s1) / 2 + Vector((d["shoulder"] * 0.82, -sign * d["shoulder"] * 0.15, 0)), low]
            for a, b in zip([top] + front, front):
                body.limb("spine_02", a, b, unit * 0.009, unit * 0.009, leather, segments=4)
            for index, share in enumerate((0.3, 0.55, 0.8)):
                spot = front[0].lerp(front[2], share) + Vector((unit * 0.012, 0, 0))
                body.limb("spine_02", spot - Vector((0, 0, unit * 0.03)), spot + Vector((0, 0, unit * 0.03)), unit * 0.014, unit * 0.014, steel, segments=8)
                body.limb("spine_02", spot - Vector((0, 0, unit * 0.012)), spot + Vector((0, 0, unit * 0.012)), unit * 0.0145, unit * 0.0145, labels[(index + sign) % 3], segments=8)
                body.limb("spine_02", spot + Vector((0, 0, unit * 0.03)), spot + Vector((0, 0, unit * 0.04)), unit * 0.006, unit * 0.006, valve, segments=6)
    if "wrapBindings" in features:
        # Soft dark cloth wound round the forearms and shins, cut to move silently.
        wrap = spec.get("wrap", mix(primary, [0.35, 0.35, 0.38], 0.18))
        for side in ("l", "r"):
            for bone, radius in (("lowerarm_" + side, limb), ("calf_" + side, leg * 0.85)):
                b0, b1 = L[bone]
                for share in (0.15, 0.32, 0.49, 0.66):
                    body.limb(bone, b0.lerp(b1, share), b0.lerp(b1, share + 0.12), radius * 1.08, radius * 1.06, wrap if share != 0.32 else mix(wrap, primary, 0.5))
    if "formalVest" in features:
        # A structured piece of formal dress over the chest: stiff, straight-edged panels crossing at the front under a
        # high standing collar, in disciplined lines.
        vest = spec.get("vest", mix(primary, [0.30, 0.30, 0.32], 0.25))
        s0, s1 = L["spine_02"]
        front = (s0 + s1) / 2 + Vector((d["shoulder"] * 0.62, 0, 0))
        for sign in (1, -1):
            body.slab("spine_02", front + Vector((0, sign * d["shoulder"] * 0.45, d["torso"] * 0.22)),
                      front + Vector((0, -sign * d["shoulder"] * 0.12, -d["torso"] * 0.18)), d["shoulder"] * 0.32, 2.5, vest)
        n0, n1 = L["neck_01"]
        body.limb("neck_01", n0 - Vector((0, 0, limb * 0.4)), n0 + Vector((0, 0, limb * 1.4)), limb * 1.75, limb * 1.85, vest, segments=8)
    if "cutInsignia" in features:
        # Every house mark cut out of the cloth: clean empty patches, paler than the black round them, on the left
        # shoulder, over the heart and on the back.
        empty = spec.get("insignia", mix(primary, [0.45, 0.43, 0.42], 0.35))
        u0, u1 = L["upperarm_l"]
        body.box("upperarm_l", u0.lerp(u1, 0.3) + Vector((0, limb * 1.25, 0)), (limb * 1.0, 1.0, limb * 1.0), empty)
        s0, s1 = L["spine_02"]
        body.box("spine_02", s0.lerp(s1, 0.8) + Vector((d["shoulder"] * 0.86, d["shoulder"] * 0.3, 0)), (1.0, d["shoulder"] * 0.2, d["shoulder"] * 0.2), empty)
        body.box("spine_03", L["spine_03"][0] + Vector((-d["shoulder"] * 0.86, 0, 0)), (1.0, d["shoulder"] * 0.36, d["shoulder"] * 0.36), empty)
    if "waistSash" in features:
        # A torn sash wound at the waist, knotted at the left hip, its frayed ends hanging.
        sash = spec.get("sash", secondary)
        unit = d["height"]
        waist = p1 - Vector((0, 0, d["torso"] * 0.02))
        body.limb("pelvis", waist - Vector((0, 0, unit * 0.03)), waist + Vector((0, 0, unit * 0.025)), d["hip"] * 1.12, d["hip"] * 1.12, sash)
        knot = waist + Vector((d["hip"] * 0.2, d["hip"] * 1.12, 0))
        body.ball("pelvis", knot, unit * 0.028, sash, segments=8)
        for spread, length in ((-0.4, 0.22), (0.5, 0.15)):
            body.slab("pelvis", knot, knot + Vector((d["hip"] * 0.3 * spread, d["hip"] * 0.2, -unit * length)), unit * 0.05, 1.5, mix(sash, [0.1, 0.02, 0.02], 0.2))
    if "crossedToken" in features:
        # A round token hung at the right hip beside the dagger's sheath, two blades crossed on it.
        unit = d["height"]
        token = p1 + Vector((d["hip"] * 0.75, -d["hip"] * 0.95, -unit * 0.06))
        silver = [0.72, 0.72, 0.74]
        body.limb("pelvis", token - Vector((0, unit * 0.004, 0)), token + Vector((0, unit * 0.004, 0)), unit * 0.03, unit * 0.03, mix(silver, primary, 0.5), segments=10)
        for sign in (1, -1):
            body.limb("pelvis", token + Vector((sign * unit * 0.02, -unit * 0.006, unit * 0.02)), token + Vector((-sign * unit * 0.02, -unit * 0.006, -unit * 0.02)),
                      unit * 0.004, unit * 0.004, silver, segments=4)
    if "gunnerKit" in features:
        # A working gunner's gear at the waist: ammunition pouches on the belt and a coil of rope hung at the left hip.
        unit = d["height"]
        leather, rope = [0.28, 0.18, 0.10], spec.get("rope", [0.62, 0.52, 0.34])
        belt = p1 - Vector((0, 0, d["torso"] * 0.06))
        for index in range(4):
            around = math.radians(-70 + index * 30)
            spot = belt + Vector((math.cos(around) * d["hip"] * 1.18, math.sin(around) * d["hip"] * 1.18, -unit * 0.02))
            body.box("pelvis", spot, (unit * 0.035, unit * 0.045, unit * 0.05), leather, rotation=Euler((0, 0, around)))
        coil = belt + Vector((-d["hip"] * 0.1, d["hip"] * 1.25, -unit * 0.05))
        for ring in range(3):
            body.limb("pelvis", coil + Vector((-unit * 0.035, 0, -unit * 0.012 * ring)), coil + Vector((unit * 0.035, 0, -unit * 0.012 * ring - unit * 0.01)),
                      unit * 0.04, unit * 0.04, mix(rope, [0.3, 0.25, 0.18], 0.15 * ring), segments=10)
    if "heavyBoots" in features:
        boots = spec.get("boots", [0.12, 0.08, 0.06])
        for side in ("l", "r"):
            f0, f1 = L["foot_" + side]
            body.limb("foot_" + side, f0 - Vector((leg * 0.15, 0, 0)), f1 + Vector((leg * 0.15, 0, 0)), leg * FOOT_HEEL_SHARE * 1.25,
                      leg * FOOT_HEEL_SHARE * FOOT_TOE_SHARE * 1.25, boots)
            c0, c1 = L["calf_" + side]
            body.limb("calf_" + side, c0.lerp(c1, 0.55), c1, leg * 1.0, leg * 1.05, boots)
            if "bunnyBoots" in features:
                # A stripe of the accent round the cuff, and a bunny face's ears on each toe.
                body.limb("calf_" + side, c0.lerp(c1, 0.55), c0.lerp(c1, 0.62), leg * 1.08, leg * 1.08, accent)
                toe = f1 + Vector((leg * 0.05, 0, leg * 0.5))
                for sign in (1, -1):
                    body.limb("foot_" + side, toe + Vector((0, sign * leg * 0.25, 0)), toe + Vector((-leg * 0.15, sign * leg * 0.35, leg * 0.75)),
                              leg * 0.18, leg * 0.06, [0.95, 0.92, 0.90], segments=6)
    if "bunnyCharms" in features:
        # Plush bunny charms hung on the jacket: white heads with long ears and the cross-stitched eyes of her mark.
        for bone, at, out in (("spine_02", Vector((d["shoulder"] * 0.7, d["shoulder"] * 0.45, 0)), 1.0),
                              ("spine_03", Vector((-d["shoulder"] * 0.75, -d["shoulder"] * 0.3, 0)), -1.0),
                              ("pelvis", Vector((d["hip"] * 0.4, -d["hip"] * 1.1, 0)), 1.0)):
            b0, b1 = L[bone]
            centre = (b0 + b1) / 2 + at
            size = d["shoulder"] * 0.17
            body.ball(bone, centre, size, [0.95, 0.93, 0.92], segments=8)
            for sign in (1, -1):
                body.limb(bone, centre + Vector((0, sign * size * 0.35, size * 0.6)), centre + Vector((0, sign * size * 0.55, size * 2.0)),
                          size * 0.3, size * 0.12, [0.95, 0.93, 0.92], segments=5)
                body.box(bone, centre + Vector((out * size * 0.95, sign * size * 0.35, size * 0.15)), (1.0, size * 0.35, size * 0.08), [0.1, 0.05, 0.08],
                         rotation=Euler((math.radians(45), 0, 0)))
    # Tails ride the tail bones.
    if "brushTail" in features:
        # A big brush of a tail, as long as she is tall to the shoulder: back from the hips, swelling, then sweeping up
        # behind her, its tip paler. Each tuft rides the tail bone nearest it, so it sways with them.
        fur = spec.get("fur", [0.97, 0.93, 0.85])
        start = L["tail_01"][0]
        reach = (L["neck_01"][0].z - d["base"]) * 0.75
        for index in range(9):
            share = index / 8
            spot = start + Vector((-reach * (0.15 + 0.55 * share), 0, reach * (0.55 * math.sin(share * math.pi * 0.85) - 0.12 * share)))
            radius = leg * (0.9 + 1.6 * math.sin(share * math.pi * 0.9))
            body.ball(("tail_01", "tail_02", "tail_03")[min(2, int(share * 3))], spot, radius, mix(skin, fur, 0.2 + 0.8 * max(0.0, share - 0.6) / 0.4),
                      scale=(1.1, 0.9, 1.0), segments=8)
    if "shortTail" in features:
        t0, t1 = L["tail_01"]
        body.ball("tail_01", t0, leg * 0.9, mix(skin, [1, 1, 1], 0.4))
    if "heavyTail" in features:
        for index, name in enumerate(("tail_01", "tail_02", "tail_03")):
            t0, t1 = L[name]
            body.limb(name, t0, t1, leg * (1.3 - index * 0.4), leg * (0.95 - index * 0.35), skin)
    if "reptile" in features:
        reptile(body, spec, L, d, head_center, head_radius, leg)
    if "miningArmor" in features:
        mining_armor(body, spec, L, d, limb, leg)
    if "rescueGear" in features:
        rescue_gear(body, L, d, limb)
    # The wave Neris rides: a swell under her feet that is the whole silhouette.
    if "waveBase" in features:
        wave_body(body, spec, L, d)
    for prop in spec["props"]:
        add_prop(body, prop, L, d, spec)
    return body


def wave_body(body, spec, L, d):
    """The living wave a rider stands on, the whole of her silhouette. A long swell of dark water rests on the ground
    under her, longer than it is wide and lowest at the front. Its back rises into a crest on the cape bones, so the crest
    rolls as she rides. White foam froths along its lip and round the swell's leading edge, and cold light shows in the
    water under her feet. A larger wave lifts her higher on a deeper, broader swell, and its crest rises with it."""
    b, r, u = d["base"], d["waveReach"], d["waveUnit"]
    water = spec.get("water", [0.04, 0.12, 0.20])
    deep = mix(water, [0.0, 0.0, 0.0], 0.3)
    foam = spec.get("foam", [0.85, 0.93, 0.97])
    # The swell, resting on the ground: narrow at the front, broad at the back where it rises into the crest.
    body.blob("root", Vector((0.35 * r, 0, 0.45 * b)), (Vector((1.1 * r, 0, 0)), Vector((0, 0.75 * r, 0)), Vector((0, 0, 0.45 * b))), water, segments=12)
    body.blob("root", Vector((-0.6 * r, 0, 0.55 * b)), (Vector((0.95 * r, 0, 0)), Vector((0, 1.3 * r, 0)), Vector((0, 0, 0.55 * b))), deep, segments=12)
    # The light the water gives where she stands.
    body.blob("root", Vector((0.1 * r, 0, 0.92 * b)), (Vector((0.6 * r, 0, 0)), Vector((0, 0.5 * r, 0)), Vector((0, 0, 0.05 * b))), spec["accent"], glow=True, segments=10)
    # Froth thrown up at the bow, where the wave pushes through.
    body.blob("root", Vector((1.3 * r, 0, 0.5 * b)), (Vector((0.3 * r, 0, 0)), Vector((0, 0.6 * r, 0)), Vector((0, 0, 0.3 * u))), foam, segments=10)
    # The crest: a wall of water curved round behind her, a crescent from above, rounded panels in an arc about her on
    # each length of the crest, brightening toward the lip, which rolls with foam.
    arc = [math.radians(degrees) for degrees in (-60, -30, 0, 30, 60)]
    for index, name in enumerate(("cape_01", "cape_02", "cape_03")):
        c0, c1 = L[name]
        color = mix(mix(water, deep, 0.3 - index * 0.15), foam, 0.1 + index * 0.15)
        for angle in arc:
            turn = Matrix.Rotation(angle, 3, "Z")
            a, z = turn @ c0, turn @ c1
            along = (z - a) / 2
            across = (turn @ Vector((0, 1, 0))) * r * 0.62 * (1 - abs(angle) / math.pi * 0.6)
            thick = along.cross(across).normalized() * u * (0.22 - index * 0.04)
            body.blob(name, (a + z) / 2, (along * 1.2, across, thick), color, segments=12)
    c0, c1 = L["cape_03"]
    for angle in [math.radians(degrees) for degrees in (-65, -42, -20, 0, 20, 42, 65)]:
        body.ball("cape_03", Matrix.Rotation(angle, 3, "Z") @ c1, 0.3 * u * (1 - abs(angle) / math.pi * 0.5), foam, segments=8)


def reptile(body, spec, L, d, head_center, head_radius, leg):
    """A living reptile, never armour or a machine: a long jaw dropped open in a snarl, full of teeth, slag hanging
    from it; one burning eye and one clouded, a scar through it; a great curved horn and a broken stump; a frill on
    spines behind the jaw, torn on the right; a pale scaled belly; dark spines down the back and tail; claws on the
    feet. Its hide is the skin colour, its belly the belly colour."""
    hide, belly = spec["skin"], spec.get("belly", [0.84, 0.76, 0.64])
    dark, horn, tooth, slag = mix(hide, [0.0, 0.0, 0.0], 0.6), [0.80, 0.74, 0.62], [0.93, 0.89, 0.78], spec.get("slag", [0.03, 0.03, 0.03])
    hc, hr = head_center, head_radius
    # The jaw: a long upper snout over a lower jaw dropped open, the mouth dark between them, teeth in both.
    drop = math.radians(18)
    body.blob("head", hc + Vector((hr * 0.95, 0, -hr * 0.05)), (Vector((hr * 1.05, 0, 0)), Vector((0, hr * 0.58, 0)), Vector((0, 0, hr * 0.4))), hide, segments=10)
    jaw = hc + Vector((hr * 0.8, 0, -hr * 0.55))
    body.blob("head", jaw, (Vector((hr * 0.95 * math.cos(drop), 0, -hr * 0.95 * math.sin(drop))), Vector((0, hr * 0.5, 0)), Vector((0, 0, hr * 0.22))), belly, segments=10)
    body.blob("head", hc + Vector((hr * 0.85, 0, -hr * 0.33)), (Vector((hr * 0.8, 0, 0)), Vector((0, hr * 0.42, 0)), Vector((0, 0, hr * 0.13))), [0.32, 0.05, 0.04], segments=8)
    for index in range(6):
        along = hr * (0.35 + index * 0.25)
        for sign in (1, -1):
            upper = hc + Vector((along, sign * hr * 0.4 * (1 - index * 0.08), -hr * 0.32))
            body.limb("head", upper, upper - Vector((0, 0, hr * 0.24)), hr * 0.06, hr * 0.01, tooth, segments=4)
            if index < 5:
                lower = jaw + Vector((along - hr * 0.7, sign * hr * 0.34 * (1 - index * 0.08), hr * 0.16 - (along - hr * 0.7) * math.tan(drop)))
                body.limb("head", lower, lower + Vector((0, 0, hr * 0.2)), hr * 0.05, hr * 0.01, tooth, segments=4)
    for index, across in enumerate((0.3, -0.2, 0.05)):
        start = jaw + Vector((hr * (0.1 + 0.25 * index), across * hr, -hr * 0.25))
        body.limb("head", start, start - Vector((0, 0, hr * (0.5 + 0.2 * index))), hr * 0.08, hr * 0.02, slag, segments=6)
    # One eye burning, the other clouded under a scar that runs on across the snout; heavy dark brows.
    for sign, color, lit in ((1, spec["accent"], True), (-1, [0.74, 0.77, 0.78], False)):
        eye = hc + Vector((hr * 0.5, sign * hr * 0.5, hr * 0.22))
        body.ball("head", eye, hr * 0.14, color, glow=lit, segments=8)
        body.blob("head", eye + Vector((-hr * 0.05, 0, hr * 0.14)), (Vector((hr * 0.3, 0, 0)), Vector((0, hr * 0.16, 0)), Vector((0, 0, hr * 0.08))), dark, segments=6)
    body.limb("head", hc + Vector((hr * 0.35, -hr * 0.62, hr * 0.5)), hc + Vector((hr * 1.25, -hr * 0.2, hr * 0.32)), hr * 0.04, hr * 0.03, mix(belly, hide, 0.3), segments=4)
    # A great horn curving up from the left temple and forward at its tip; on the right, a stump broken off ragged.
    base = hc + Vector((-hr * 0.1, hr * 0.55, hr * 0.5))
    curve = [base, base + Vector((-hr * 0.25, hr * 0.18, hr * 0.65)), base + Vector((-hr * 0.4, hr * 0.25, hr * 1.35)),
             base + Vector((-hr * 0.2, hr * 0.25, hr * 1.95)), base + Vector((hr * 0.3, hr * 0.2, hr * 2.3))]
    for index, (a, b) in enumerate(zip(curve, curve[1:])):
        body.limb("head", a, b, hr * (0.34 - index * 0.075), hr * (0.34 - (index + 1) * 0.075), mix(horn, dark, index * 0.15), segments=8)
    stump = hc + Vector((-hr * 0.1, -hr * 0.55, hr * 0.5))
    stump_top = stump + Vector((-hr * 0.1, -hr * 0.12, hr * 0.55))
    body.limb("head", stump, stump_top, hr * 0.34, hr * 0.27, horn, segments=8)
    for angle in (0.4, 2.2, 4.0):
        edge = stump_top + Vector((math.cos(angle) * hr * 0.18, math.sin(angle) * hr * 0.18, 0))
        body.limb("head", edge, edge + Vector((0, 0, hr * (0.12 + 0.05 * angle))), hr * 0.09, hr * 0.01, mix(horn, dark, 0.4), segments=4)
    # The frill: skin stretched between spines fanned behind each side of the jaw. On the right one panel is ripped
    # away and the last spine snapped short.
    for sign in (1, -1):
        hinge = hc + Vector((-hr * 0.15, sign * hr * 0.55, -hr * 0.2))
        tips = []
        for index in range(4):
            angle = math.radians(-35 + index * 30)
            reach = hr * (0.45 if sign < 0 and index == 3 else 0.95)
            tips.append(hinge + Vector((-math.cos(angle) * reach * 0.8, sign * reach * 0.55, math.sin(angle) * reach)))
            body.limb("head", hinge, tips[-1], hr * 0.06, hr * 0.015, dark, segments=4)
        for index, (a, b) in enumerate(zip(tips, tips[1:])):
            if not (sign < 0 and index == 1):
                body.pane("head", [hinge, a, b], hr * 0.04, mix(hide, [0.3, 0.05, 0.03], 0.35))
    # A pale belly of broad scales banded across, down the front of the trunk.
    s, torso = d["shoulder"], d["torso"]
    s0, s1 = L["spine_02"]
    front = (s0 + s1) / 2 + Vector((s * 0.62, 0, 0))
    body.blob("spine_02", front, (Vector((s * 0.28, 0, 0)), Vector((0, s * 0.55, 0)), Vector((0, 0, torso * 0.42))), belly, segments=10)
    for index in range(4):
        height = (index - 1.5) * torso * 0.16
        middle = front + Vector((s * 0.27, 0, height))
        for sign in (1, -1):
            body.limb("spine_02", middle, front + Vector((s * 0.16, sign * s * 0.44, height)), s * 0.02, s * 0.02, mix(belly, hide, 0.4), segments=4)
    # Dark spines down the back from the neck to the hips, longest over the shoulders, and on down the tail.
    neck = L["neck_01"][0] + Vector((-s * 0.6, 0, -torso * 0.05))
    hips = L["pelvis"][1] + Vector((-d["hip"] * 0.95, 0, 0))
    for index in range(7):
        share = index / 6
        bone = "spine_03" if share < 0.35 else "spine_02" if share < 0.7 else "spine_01"
        root = neck.lerp(hips, share)
        body.limb(bone, root, root + Vector((-0.6, 0, 0.8)).normalized() * s * (0.45 - 0.25 * share), s * 0.09, s * 0.01, dark, segments=4)
    for index, name in enumerate(("tail_01", "tail_02", "tail_03")):
        t0, t1 = L[name]
        for share in (0.25, 0.75):
            root = t0.lerp(t1, share) + Vector((0, 0, leg * (1.25 - index * 0.4) * 0.85))
            body.limb(name, root, root + Vector((-0.6, 0, 0.8)).normalized() * s * (0.25 - index * 0.06), s * 0.07, s * 0.01, dark, segments=4)
    # Claws: three curved talons at each foot's toes.
    for side in ("l", "r"):
        f0, f1 = L["foot_" + side]
        for spread in (-1, 0, 1):
            toe = f1 + Vector((leg * 0.2, spread * leg * 0.45, -leg * 0.1))
            body.limb("foot_" + side, toe, toe + Vector((leg * 0.7, spread * leg * 0.2, -leg * 0.35)), leg * 0.22, leg * 0.03, horn, segments=4)


def mining_armor(body, spec, L, d, limb, leg):
    """Mismatched, scorched mining armour over a living hide: a layered riveted iron pauldron on the right shoulder and
    a leather one on the left, a heavy chain across the chest, a wide riveted belt with a torn red tabard hanging from
    it, riveted bracers and knee plates, and black slag clinging in the gaps and dripping."""
    iron, scorch, rivet = spec.get("armor", [0.30, 0.28, 0.26]), [0.11, 0.10, 0.09], [0.55, 0.50, 0.42]
    leather, tabard, slag = spec.get("leather", [0.30, 0.19, 0.11]), spec.get("tabard", [0.45, 0.08, 0.06]), spec.get("slag", [0.03, 0.03, 0.03])
    s, hip, torso = d["shoulder"], d["hip"], d["torso"]

    def clot(bone, at, size, hang):
        """Slag clinging at a seam, a rope of it hanging."""
        body.blob(bone, at, (Vector((size, 0, 0)), Vector((0, size * 1.3, 0)), Vector((0, 0, size * 0.7))), slag, segments=6)
        body.limb(bone, at, at - Vector((0, 0, hang)), size * 0.5, size * 0.12, slag, segments=5)

    # The right pauldron: three overlapping scorched plates, rivets round their outer edges, slag in the seam.
    top = L["clavicle_r"][1] + Vector((0, 0, limb * 0.8))
    for layer in range(3):
        centre = top + Vector((-limb * 0.2 * layer, -limb * 0.45 * layer, -limb * 0.75 * layer))
        size = limb * (2.8 - layer * 0.4)
        body.ball("clavicle_r", centre, size, mix(iron, scorch, 0.15 + 0.25 * layer), scale=(1.15, 1.0, 0.42), segments=10)
        for index in range(4):
            angle = math.radians(-150 + index * 40)
            body.ball("clavicle_r", centre + Vector((math.cos(angle) * size * 1.05, math.sin(angle) * size * 0.92, size * 0.12)), limb * 0.2, rivet, segments=6)
    clot("clavicle_r", top + Vector((limb * 0.6, -limb * 0.9, -limb * 0.5)), limb * 0.55, limb * 1.6)
    # A smaller leather pauldron on the left, strapped on.
    body.ball("clavicle_l", L["clavicle_l"][1] + Vector((0, 0, limb * 0.6)), limb * 2.0, leather, scale=(1.1, 1.0, 0.45), segments=8)
    # A heavy chain across the chest from the left shoulder to the right hip.
    chain_links(body, "spine_03", [L["clavicle_l"][1] + Vector((s * 0.35, -s * 0.1, 0)), L["spine_02"][1] + Vector((s * 0.95, -s * 0.2, 0)),
                                   L["pelvis"][1] + Vector((hip * 1.05, -hip * 0.9, -torso * 0.05))], d["height"] * 0.018, iron)
    # A wide riveted belt, its iron buckle plate clotted with slag, and a torn red tabard hanging from it.
    p1 = L["pelvis"][1]
    body.limb("pelvis", p1 - Vector((0, 0, torso * 0.07)), p1 + Vector((0, 0, torso * 0.07)), hip * 1.2, hip * 1.2, leather, segments=12)
    buckle = p1 + Vector((hip * 1.2, 0, 0))
    body.box("pelvis", buckle, (hip * 0.15, hip * 0.6, torso * 0.2), mix(iron, scorch, 0.3))
    for sign in (1, -1):
        body.ball("pelvis", buckle + Vector((hip * 0.08, sign * hip * 0.22, 0)), limb * 0.2, rivet, segments=6)
    clot("pelvis", buckle + Vector((hip * 0.12, hip * 0.3, -torso * 0.08)), limb * 0.45, limb * 1.8)
    for index, length in enumerate((0.55, 0.8, 0.45)):
        across = (index - 1) * hip * 0.32
        body.slab("pelvis", buckle + Vector((0, across, -torso * 0.1)), buckle + Vector((hip * 0.15, across, -torso * 0.1 - d["leg"] * length)), hip * 0.3, 1.5,
                  mix(tabard, [0.1, 0.02, 0.02], 0.2 * index))
    # Riveted iron bracers, slag running off them, and knee plates.
    for side in ("l", "r"):
        e0, e1 = L["lowerarm_" + side]
        for band in (0.35, 0.7):
            body.limb("lowerarm_" + side, e0.lerp(e1, band), e0.lerp(e1, band + 0.2), limb * 1.35, limb * 1.35, mix(iron, scorch, 0.3), segments=8)
        clot("lowerarm_" + side, e0.lerp(e1, 0.6) + Vector((limb * 1.2, 0, 0)), limb * 0.35, limb * 1.2)
        c0, c1 = L["calf_" + side]
        body.ball("calf_" + side, c0 + Vector((leg * 0.55, 0, 0)), leg * 0.85, mix(iron, scorch, 0.4), scale=(0.7, 1.0, 1.1), segments=8)


def rescue_gear(body, L, d, limb):
    """A rescuer's working gear over the coat: a heavy shackle at the chest, a coil of line at the right hip, and a
    metal signal whistle on a chain."""
    iron, rope, brass = [0.20, 0.19, 0.20], [0.62, 0.52, 0.34], [0.70, 0.60, 0.35]
    p0, p1 = L["pelvis"]
    chest = L["spine_03"][0] + Vector((d["shoulder"] * 0.85, 0, 0))
    # The shackle: a heavy iron loop on the strap at the chest.
    loop = [chest + Vector((0, math.cos(angle) * limb * 1.6, math.sin(angle) * limb * 1.6)) for angle in [index / 8 * math.tau for index in range(9)]]
    chain_links(body, "spine_03", loop, limb * 0.8, iron)
    # The whistle hangs from it on a short chain.
    hang = [chest - Vector((0, 0, limb * 1.6)), chest - Vector((-limb * 0.4, 0, limb * 4.0))]
    chain_links(body, "spine_03", hang, limb * 0.5, iron)
    body.limb("spine_03", hang[1], hang[1] + Vector((limb * 0.3, 0, -limb * 2.0)), limb * 0.45, limb * 0.35, brass, segments=8)
    # The coil of line: rope wound in turns at the right hip.
    hip = p1 + Vector((0, -d["hip"] * 1.2, -d["torso"] * 0.05))
    for turn in range(3):
        ring = [hip + Vector((math.cos(angle) * limb * 2.4, -limb * (0.5 + turn * 0.7), math.sin(angle) * limb * 2.4))
                for angle in [index / 10 * math.tau for index in range(11)]]
        for a, b in zip(ring, ring[1:]):
            body.limb("pelvis", a, b, limb * 0.45, limb * 0.45, rope, segments=5)


def chain_links(body, bone, points, size, iron, fire=None):
    """Heavy iron chain along points: links of size, each turned a quarter about the chain from the last, with
    witchfire running along it if fire is given."""
    for a, b in zip(points, points[1:]):
        span = (b - a).length
        count = max(1, round(span / (size * 1.6)))
        axis = (b - a).normalized()
        for index in range(count):
            centre = a.lerp(b, (index + 0.5) / count)
            across = axis.cross(Vector((0, 0, 1)) if abs(axis.z) < 0.9 else Vector((1, 0, 0))).normalized()
            if index % 2:
                across = axis.cross(across).normalized()
            depth = axis.cross(across).normalized()
            body.blob(bone, centre, (axis * size, across * size * 0.6, depth * size * 0.2), iron, segments=6)
        if fire:
            body.limb(bone, a, b, size * 0.14, size * 0.14, fire, glow=True, segments=4)


def spectral_bear(body, spec, L, d):
    """The enormous spectral bear of crimson energy that rears up around a small body: burning eyes, an open fanged
    maw, vast clawed arms. All of it glows. Its arms ride the small body's arms, so their swing is its swipe; it
    stands spec["spectralScale"] times the body's own capsule height, as its status grows the capsule."""
    energy, eyes, fang = spec["spectral"], [1.0, 0.85, 0.4], [0.95, 0.85, 0.75]
    tall = d["full"] / spec["heightShare"] * spec["spectralScale"]
    back = Vector((-tall * 0.12, 0, 0))
    # A towering trunk rising behind the toy, from its hips to above its head.
    body.blob("spine_01", back + Vector((0, 0, tall * 0.42)), (Vector((tall * 0.2, 0, 0)), Vector((0, tall * 0.24, 0)), Vector((0, 0, tall * 0.3))), energy, glow=True, segments=10)
    body.blob("spine_03", back + Vector((tall * 0.04, 0, tall * 0.66)), (Vector((tall * 0.18, 0, 0)), Vector((0, tall * 0.28, 0)), Vector((0, 0, tall * 0.16))), energy, glow=True, segments=10)
    # Its head, thrust forward over the toy: round ears, burning eyes, an open maw with fangs.
    head = back + Vector((tall * 0.2, 0, tall * 0.84))
    body.blob("spine_03", head, (Vector((tall * 0.14, 0, 0)), Vector((0, tall * 0.12, 0)), Vector((0, 0, tall * 0.11))), energy, glow=True, segments=10)
    for sign in (1, -1):
        body.ball("spine_03", head + Vector((-tall * 0.03, sign * tall * 0.1, tall * 0.1)), tall * 0.05, energy, glow=True, segments=8)
        body.ball("spine_03", head + Vector((tall * 0.12, sign * tall * 0.045, tall * 0.03)), tall * 0.02, eyes, glow=True, segments=6)
    jaw = head + Vector((tall * 0.12, 0, -tall * 0.08))
    body.blob("spine_03", jaw, (Vector((tall * 0.09, 0, 0)), Vector((0, tall * 0.08, 0)), Vector((0, 0, tall * 0.03))), mix(energy, [0.05, 0.0, 0.0], 0.5), glow=True, segments=8)
    for index in range(4):
        across = (index - 1.5) * tall * 0.03
        for top, z, tip in ((1, tall * 0.02, -tall * 0.045), (-1, -tall * 0.02, tall * 0.035)):
            root = head + Vector((tall * 0.18, across, -tall * 0.04 + z))
            body.limb("spine_03", root, root + Vector((0, 0, tip)), tall * 0.012, tall * 0.002, fang, segments=4)
    # Vast clawed arms reaching forward from its shoulders.
    for side, sign in (("l", 1), ("r", -1)):
        shoulder = back + Vector((tall * 0.02, sign * tall * 0.26, tall * 0.68))
        elbow = shoulder + Vector((tall * 0.2, sign * tall * 0.1, -tall * 0.18))
        paw = elbow + Vector((tall * 0.24, -sign * tall * 0.02, -tall * 0.12))
        body.limb("upperarm_" + side, shoulder, elbow, tall * 0.09, tall * 0.075, energy, glow=True, segments=8)
        body.limb("lowerarm_" + side, elbow, paw, tall * 0.075, tall * 0.065, energy, glow=True, segments=8)
        body.ball("lowerarm_" + side, paw, tall * 0.08, energy, glow=True, segments=8)
        for claw in (-1, 0, 1):
            root = paw + Vector((tall * 0.05, claw * tall * 0.04, 0))
            body.limb("lowerarm_" + side, root, root + Vector((tall * 0.1, claw * tall * 0.02, -tall * 0.06)), tall * 0.018, tall * 0.002, fang, segments=4)


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
        # A segmented mechanical bracer sheathing the forearm, a channel of light along it and a Flux core burning at
        # the wrist: from above, the brightest point on the body, on the arm that strikes.
        e0, e1 = L["lowerarm_" + side]
        for segment in range(3):
            a, b = e0.lerp(e1, 0.1 + segment * 0.3), e0.lerp(e1, 0.36 + segment * 0.3)
            body.limb("lowerarm_" + side, a, b, unit * (0.042 + segment * 0.004), unit * (0.046 + segment * 0.004), metal)
        body.limb("lowerarm_" + side, e0.lerp(e1, 0.15) + Vector((0, 0, unit * 0.04)), e1 + Vector((0, 0, unit * 0.042)), unit * 0.009, unit * 0.009, accent, glow=True)
        body.ball("lowerarm_" + side, e1, unit * 0.04, accent, glow=True)
    elif kind == "rifle":
        # A long, ornate energy rifle: dark steel and brass, channels glowing its length, a large round optic with a lit
        # lens. Its barrel reaches well ahead of the body, so from above it points the way its bearer faces.
        steel, brass = [0.14, 0.13, 0.14], [0.75, 0.55, 0.25]
        up = Vector((0, 0, 1))
        body.slab(bone, grip - forward * unit * 0.15 - up * unit * 0.01, grip - forward * unit * 0.01, unit * 0.025, unit * 0.06, steel)
        body.limb(bone, grip - forward * unit * 0.02, grip + forward * unit * 0.24, unit * 0.026, unit * 0.022, steel)
        body.limb(bone, grip + forward * unit * 0.24, grip + forward * unit * 0.56, unit * 0.013, unit * 0.012, steel)
        body.limb(bone, grip + up * unit * 0.014, grip + forward * unit * 0.5 + up * unit * 0.008, unit * 0.007, unit * 0.006, accent, glow=True)
        for share in (0.1, 0.24, 0.4, 0.56):
            body.limb(bone, grip + forward * unit * (share - 0.008), grip + forward * unit * (share + 0.008), unit * 0.03, unit * 0.03, brass)
        optic = grip + forward * unit * 0.04 + up * unit * 0.05
        body.limb(bone, optic, optic + forward * unit * 0.17, unit * 0.026, unit * 0.034, steel)
        body.limb(bone, optic + forward * unit * 0.17, optic + forward * unit * 0.18, unit * 0.03, unit * 0.03, accent, glow=True)
        body.limb(bone, optic - forward * unit * 0.005, optic, unit * 0.018, unit * 0.018, accent, glow=True)
    elif kind == "lantern":
        body.limb(bone, grip, grip + down * unit * 0.08, unit * 0.004, unit * 0.004, metal)
        body.ball(bone, grip + down * unit * 0.13, unit * 0.045, accent, glow=True, scale=(1.0, 1.0, 1.3))
    elif kind == "pilotLantern":
        # A large pilot's lantern hung on its chain from the hand, burning cold blue-white inside an iron cage: the
        # brightest thing about her, and the point allies steer by.
        iron = [0.20, 0.20, 0.22]
        chain_links(body, bone, [grip, grip + down * unit * 0.1], unit * 0.008, iron)
        centre = grip + down * unit * 0.19
        body.limb(bone, centre + down * unit * 0.08, centre + down * unit * 0.095, unit * 0.06, unit * 0.06, iron, segments=8)
        body.limb(bone, centre - down * unit * 0.07, centre - down * unit * 0.1, unit * 0.055, unit * 0.025, iron, segments=8)
        body.ball(bone, centre, unit * 0.055, mix(accent, [1.0, 1.0, 1.0], 0.4), glow=True, scale=(1.0, 1.0, 1.35), segments=10)
        for bar in range(4):
            angle = bar / 4 * math.tau + math.pi / 4
            out = Vector((math.cos(angle), math.sin(angle), 0)) * unit * 0.058
            body.limb(bone, centre + out - down * unit * 0.07, centre + out + down * unit * 0.08, unit * 0.004, unit * 0.004, iron, segments=4)
    elif kind == "ball":
        # A big black, pink and white ball with a cross-eyed bunny face, trailing ribbons of light.
        centre = grip + forward * unit * 0.1 + Vector((0, 0, unit * 0.02))
        radius = unit * 0.1
        body.ball(bone, centre, radius, [0.08, 0.06, 0.07], segments=12)
        body.limb(bone, centre - Vector((0, 0, radius * 0.25)), centre + Vector((0, 0, radius * 0.25)), radius * 1.02, radius * 1.02, accent, segments=12)
        face = centre + forward * radius * 0.8
        body.ball(bone, face, radius * 0.45, [0.95, 0.93, 0.92], scale=(0.35, 1.0, 1.0), segments=8)
        for sign in (1, -1):
            body.limb(bone, face + Vector((0, sign * radius * 0.2, radius * 0.3)), face + Vector((-radius * 0.1, sign * radius * 0.3, radius * 0.85)),
                      radius * 0.13, radius * 0.05, [0.95, 0.93, 0.92], segments=5)
        for index, sign in enumerate((1, -1)):
            arc = [centre - forward * radius * (0.8 + k * 1.2) + Vector((0, sign * radius * (0.6 + k * 0.5), radius * math.sin(k * 2.5) * 0.6))
                   for k in (0.0, 0.5, 1.0, 1.5)]
            for a, b in zip(arc, arc[1:]):
                body.limb(bone, a, b, radius * 0.07, radius * 0.04, accent, glow=True, segments=5)
    elif kind == "siegeArm":
        # An enormous two-handed siege arm: a heavy dark timber stock bound with riveted iron, a thick barrel, a bladed
        # spike running forward past the muzzle, a hot Flux line burning its length, and a pennant hung from the fore-end.
        up = Vector((0, 0, 1))
        iron = [0.20, 0.19, 0.19]
        timber = [0.20, 0.12, 0.07]
        body.slab(bone, grip - forward * unit * 0.28 - up * unit * 0.02, grip + forward * unit * 0.05, unit * 0.05, unit * 0.09, timber)
        body.limb(bone, grip, grip + forward * unit * 0.5, unit * 0.045, unit * 0.04, iron, segments=8)
        for share in (0.05, 0.18, 0.32, 0.46):
            body.limb(bone, grip + forward * unit * (share - 0.012), grip + forward * unit * (share + 0.012), unit * 0.052, unit * 0.052, [0.32, 0.30, 0.28], segments=8)
        body.limb(bone, grip + forward * unit * 0.5, grip + forward * unit * 0.72, unit * 0.03, unit * 0.003, [0.55, 0.53, 0.50], segments=4)
        body.limb(bone, grip - forward * unit * 0.22 + up * unit * 0.04, grip + forward * unit * 0.5 + up * unit * 0.042, unit * 0.01, unit * 0.01, accent, glow=True, segments=5)
        pennant = grip + forward * unit * 0.4 - up * unit * 0.05
        body.slab(bone, pennant, pennant - up * unit * 0.18 - forward * unit * 0.05, unit * 0.12, 1.0, mix(accent, [0.3, 0.02, 0.02], 0.4))
        body.box(bone, pennant - up * unit * 0.08 + Vector((0, unit * 0.001, 0)), (unit * 0.05, 1.5, unit * 0.05), [0.92, 0.90, 0.86],
                 rotation=Euler((0, math.radians(45), 0)))
    elif kind == "chain":
        # A length of heavy chain hanging from the hand in a loose curve, witchfire running along it: the pact made
        # physical, and what his kit throws and leashes with.
        sweep = [grip + Vector((unit * 0.06 * k, -unit * 0.04 * math.sin(k * 1.4), -unit * (0.05 * k + 0.005 * k * k))) for k in (0.0, 1.0, 2.0, 3.0, 4.0)]
        chain_links(body, bone, sweep, unit * 0.022, [0.16, 0.14, 0.15], accent)
    elif kind == "boardingBlade":
        # An oversized single-edged boarding blade on a long cord-bound haft, broadening like a cleaver toward its tip,
        # its pale edge down at rest (up, carried over the shoulder). The back of it is an anchor's fluke: harbour
        # salvage reforged into a weapon.
        up = Vector((0, 0, 1))
        steel, edge, iron, cord = [0.50, 0.50, 0.50], [0.85, 0.85, 0.82], [0.22, 0.21, 0.21], [0.68, 0.58, 0.40]
        body.limb(bone, grip - forward * unit * 0.1, grip + forward * unit * 0.1, unit * 0.017, unit * 0.017, wood)
        for share in (-0.07, -0.02, 0.03):
            body.limb(bone, grip + forward * unit * share, grip + forward * unit * (share + 0.025), unit * 0.021, unit * 0.021, cord)
        back = grip + forward * unit * 0.1 + up * unit * 0.03
        # Its back straight; a narrow blade from the haft, then the broad cleaver's head over its last third.
        for start, end, depth in ((0.0, 0.34, 0.1), (0.34, 0.52, 0.17)):
            a, b, depth = back + forward * unit * start, back + forward * unit * end, unit * depth
            body.slab(bone, a - up * depth * 0.5, b - up * depth * 0.5, unit * 0.016, depth, steel)
            body.slab(bone, a - up * depth * 0.97, b - up * depth * 0.97, unit * 0.018, unit * 0.016, edge)
        # The fluke: a shank off the back near the tip, its arm sweeping back toward the haft to a pointed palm.
        root = back + forward * unit * 0.4
        crown = root + up * unit * 0.07 + forward * unit * 0.03
        body.limb(bone, root, crown, unit * 0.014, unit * 0.012, iron, segments=6)
        palm = crown - forward * unit * 0.13 + up * unit * 0.015
        body.limb(bone, crown, palm, unit * 0.012, unit * 0.006, iron, segments=6)
        body.blob(bone, palm, (forward * unit * 0.035, Vector((0, unit * 0.008, 0)), up * unit * 0.03), iron, segments=4)
    elif kind == "swordBack":
        # A long single-edged sword sheathed across the back, its hilt over the left shoulder: a black lacquered
        # scabbard with pale fittings, a round guard and a cord-wrapped grip.
        s0, s1 = L["spine_03"]
        hilt = s0 + Vector((-d["shoulder"] * 0.75, d["shoulder"] * 0.55, d["torso"] * 0.42))
        tip = s0 + Vector((-d["shoulder"] * 0.75, -d["shoulder"] * 0.8, -d["torso"] * 0.75))
        along = (tip - hilt).normalized()
        lacquer, fitting = [0.05, 0.05, 0.06], [0.74, 0.73, 0.70]
        body.limb("spine_03", hilt + along * unit * 0.01, tip, unit * 0.017, unit * 0.013, lacquer)
        if not prop.get("empty"):
            # Sheathed: its grip and guard stand over the shoulder. Drawn (empty), only the scabbard is left.
            body.limb("spine_03", hilt - along * unit * 0.13, hilt, unit * 0.011, unit * 0.011, spec.get("grip", [0.30, 0.05, 0.05]), segments=6)
            body.limb("spine_03", hilt - along * unit * 0.004, hilt + along * unit * 0.008, unit * 0.032, unit * 0.032, fitting, segments=10)
        for share in (0.04, 0.97):
            point = hilt.lerp(tip, share)
            body.limb("spine_03", point, point + along * unit * 0.015, unit * 0.02, unit * 0.02, fitting, segments=8)
    elif kind == "harborGun":
        # One heavy engineered gun carried on both sides of her (the braced stance): a banded brass-and-iron barrel
        # running forward under her right arm to a wide muzzle with Flux light standing in the bore, exposed blue-lit
        # Flux chambers along it, and the breech and recoil assembly riding up over her left shoulder. It rides her upper
        # body, which carries it with her hands on it.
        breech, muzzle = d["gun"]
        along = (muzzle - breech).normalized()
        side = along.cross(Vector((0, 0, 1))).normalized()
        up = side.cross(along).normalized()
        iron, brass, dark = [0.20, 0.20, 0.22], [0.78, 0.60, 0.28], [0.10, 0.10, 0.11]
        radius = unit * 0.045
        point = lambda share: breech.lerp(muzzle, share)
        body.limb("spine_03", point(0.32), point(0.96), radius, radius * 1.05, iron, segments=12)
        for share in (0.4, 0.55, 0.7, 0.85):
            body.limb("spine_03", point(share), point(share + 0.03), radius * 1.18, radius * 1.18, brass, segments=12)
        body.limb("spine_03", point(0.94), muzzle + along * radius * 0.6, radius * 1.35, radius * 1.6, brass, segments=12)
        body.limb("spine_03", muzzle + along * radius * 0.5, muzzle + along * radius * 0.65, radius * 1.15, radius * 1.15, accent, glow=True, segments=12)
        for offset in (side, -side):
            chamber = offset * radius * 1.05 + up * radius * 0.3
            body.limb("spine_03", point(0.46) + chamber, point(0.66) + chamber, radius * 0.38, radius * 0.38, accent, glow=True, segments=8)
            body.limb("spine_03", point(0.44) + chamber, point(0.46) + chamber, radius * 0.48, radius * 0.48, brass, segments=8)
            body.limb("spine_03", point(0.66) + chamber, point(0.68) + chamber, radius * 0.48, radius * 0.48, brass, segments=8)
        body.limb("spine_03", point(0.02), point(0.34), radius * 1.45, radius * 1.4, dark, segments=8)
        for offset in (side * 0.7 + up * 0.9, -side * 0.7 + up * 0.9):
            body.limb("spine_03", point(0.05) + offset * radius * 1.4, point(0.45) + offset * radius * 1.1, radius * 0.28, radius * 0.28, iron, segments=6)
        body.box("spine_03", point(0.12) + up * radius * 1.5, (radius * 1.4, radius * 1.2, radius * 0.5), brass,
                 rotation=Vector((0, 0, 1)).rotation_difference(along).to_euler())
    elif kind == "longSword":
        # A long single-edged sword drawn in the hand: a cord-wrapped grip, a round guard, and a long blade sweeping
        # forward in a slight curve, its edge down and pale.
        up = Vector((0, 0, 1))
        steel, edge, fitting = [0.62, 0.63, 0.66], [0.88, 0.89, 0.92], [0.74, 0.73, 0.70]
        body.limb(bone, grip - forward * unit * 0.06, grip + forward * unit * 0.05, unit * 0.011, unit * 0.011, spec.get("grip", [0.30, 0.05, 0.05]), segments=6)
        body.limb(bone, grip + forward * unit * 0.05, grip + forward * unit * 0.062, unit * 0.03, unit * 0.03, fitting, segments=10)
        root = grip + forward * unit * 0.062
        bend = root + forward * unit * 0.3 + up * unit * 0.008
        tip = bend + forward * unit * 0.28 + up * unit * 0.035
        for a, b, taper in ((root, bend, 1.0), (bend, tip, 0.6)):
            body.slab(bone, a, b, unit * 0.007, unit * 0.034 * taper, steel)
            body.slab(bone, a - up * unit * 0.015 * taper, b - up * unit * 0.015 * taper, unit * 0.008, unit * 0.006, edge)
    elif kind == "needles":
        # Thin throwing needles fanned between the fingers.
        steel = [0.80, 0.80, 0.84]
        for spread in (-1, 0, 1):
            body.limb(bone, grip, grip + forward * unit * 0.11 + Vector((0, spread * unit * 0.03, -unit * 0.015 * abs(spread))), unit * 0.004, unit * 0.001,
                      steel, segments=4)
    elif kind == "dagger":
        body.limb(bone, grip, grip + forward * unit * 0.14, unit * 0.012, unit * 0.002, metal)
    elif kind == "dispenserRig":
        # A large pressurised dispenser rig on her back: a banded steel cylinder with a glass window onto the glowing
        # orange reagent inside, a lozenge hazard mark on the glass, a pump housing on top with a yellow-and-black placard,
        # and armoured hoses running over her shoulder to the nozzle in her hand, one of them lit from within.
        steel, band, dark = [0.48, 0.48, 0.50], [0.30, 0.30, 0.32], [0.12, 0.12, 0.13]
        limb = unit * 0.036 * d["build"] * spec.get("limbScale", 1.0)
        s0, s1 = L["spine_03"]
        tank = s0 + Vector((-d["shoulder"] * 1.1, 0, -d["torso"] * 0.1))
        radius = d["shoulder"] * 0.55
        body.limb("spine_03", tank - Vector((0, 0, d["torso"] * 0.5)), tank + Vector((0, 0, d["torso"] * 0.42)), radius, radius, steel, segments=12)
        for share in (-0.45, -0.12, 0.3):
            ring = tank + Vector((0, 0, d["torso"] * share))
            body.limb("spine_03", ring, ring + Vector((0, 0, d["torso"] * 0.05)), radius * 1.06, radius * 1.06, band, segments=12)
        # The reagent shows through a tall window on the back and a gauge-glass band round the top, so it reads from any side.
        window = tank + Vector((-radius * 0.9, 0, d["torso"] * 0.05))
        body.box("spine_03", window, (radius * 0.3, radius * 1.1, d["torso"] * 0.42), accent, glow=True)
        gauge = tank + Vector((0, 0, d["torso"] * 0.35))
        body.limb("spine_03", gauge, gauge + Vector((0, 0, d["torso"] * 0.06)), radius * 1.02, radius * 1.02, accent, glow=True, segments=12)
        body.box("spine_03", window + Vector((-radius * 0.16, 0, 0)), (1.0, radius * 0.32, radius * 0.32), dark, rotation=Euler((math.radians(45), 0, 0)))
        housing = tank + Vector((0, 0, d["torso"] * 0.48))
        body.box("spine_03", housing, (radius * 1.2, radius * 1.2, d["torso"] * 0.14), band)
        for stripe in range(4):
            body.box("spine_03", housing + Vector((-radius * 0.61, radius * (-0.375 + stripe * 0.25), 0)), (1.0, radius * 0.25, d["torso"] * 0.1),
                     [0.95, 0.80, 0.10] if stripe % 2 == 0 else dark)
        nozzle = L["hand_r"][1]
        over = L["clavicle_r"][1] + Vector((-d["shoulder"] * 0.1, 0, limb * 1.4))
        hose = [housing + Vector((0, -radius * 0.3, 0)), over, L["upperarm_r"][1] + Vector((-limb * 1.2, 0, 0))]
        for a, b in zip(hose, hose[1:]):
            body.limb("spine_03", a, b, unit * 0.014, unit * 0.014, dark, segments=6)
        body.limb("lowerarm_r", hose[-1], nozzle, unit * 0.014, unit * 0.012, dark, segments=6)
        lit = [housing + Vector((0, radius * 0.3, 0)), over + Vector((0, d["shoulder"] * 0.25, limb * 0.3)), L["upperarm_r"][0] + Vector((-limb * 1.4, 0, -d["torso"] * 0.1))]
        for a, b in zip(lit, lit[1:]):
            body.limb("spine_03", a, b, unit * 0.008, unit * 0.008, accent, glow=True, segments=5)
        body.limb("prop_r", nozzle, nozzle + forward * unit * 0.14, unit * 0.02, unit * 0.014, steel, segments=8)
        body.limb("prop_r", nozzle + forward * unit * 0.12, nozzle + forward * unit * 0.15, unit * 0.012, unit * 0.012, accent, glow=True, segments=6)
    elif kind == "cannon":
        body.limb(bone, grip - forward * unit * 0.2, grip + forward * unit * 0.4, unit * 0.06, unit * 0.075, [0.62, 0.48, 0.25])
        body.limb(bone, grip + forward * unit * 0.36, grip + forward * unit * 0.41, unit * 0.05, unit * 0.05, accent, glow=True)
        body.box(bone, grip - forward * unit * 0.18 + Vector((0, 0, unit * 0.08)), (unit * 0.16, unit * 0.12, unit * 0.12), metal)
    elif kind == "rings":
        # The Tinkertwins, orbiting her hands rather than held: each a brass ring with a gimbal crossed inside it and a
        # cut blue crystal at its heart, turning inside a ring of its own light with star-sparks on it. Tilted back to
        # the camera and as broad as her head, so from above each reads as a ring.
        brass, spark = [0.85, 0.62, 0.25], [1.0, 0.92, 0.70]
        radius = unit * 0.085
        across_axis, up_axis = Vector((0, 1, 0)), Vector((-0.5, 0, 0.87)).normalized()
        for ring_side, sign in (("l", 1), ("r", -1)):
            center = L["hand_" + ring_side][1] + forward * unit * 0.07 + Vector((0, sign * unit * 0.03, unit * 0.06))
            for first, second, share, color, thickness, glow in ((across_axis, up_axis, 1.0, brass, 0.008, False),
                                                                  (up_axis.cross(across_axis), up_axis, 0.72, brass, 0.006, False),
                                                                  (across_axis, up_axis, 1.4, accent, 0.004, True)):
                points = [center + (first * math.cos(index / 16 * math.tau) + second * math.sin(index / 16 * math.tau)) * radius * share for index in range(17)]
                for a, b in zip(points, points[1:]):
                    body.limb("prop_" + ring_side, a, b, unit * thickness, unit * thickness, color, glow=glow, segments=6)
            body.limb("prop_" + ring_side, center, center + up_axis * radius * 0.5, radius * 0.28, radius * 0.02, accent, glow=True, segments=6)
            body.limb("prop_" + ring_side, center, center - up_axis * radius * 0.45, radius * 0.28, radius * 0.02, accent, glow=True, segments=6)
            for angle in (0.6, 2.3, 4.1):
                body.ball("prop_" + ring_side, center + (across_axis * math.cos(angle) + up_axis * math.sin(angle)) * radius * 1.4, unit * 0.009, spark, glow=True, segments=6)
    elif kind == "springbow":
        # A compact spring-loaded bolt launcher in the fist, ready to fire, mechanical and unlit: a green-painted steel
        # stock reaching back along the forearm and forward past the fist, brass limbs swept back across its front with
        # a cord drawn between them, exposed coil springs at their roots, a winding drum on its side and a bolt laid in.
        green, brass, steel, cord = [0.22, 0.34, 0.16], [0.80, 0.60, 0.25], [0.32, 0.32, 0.34], [0.85, 0.82, 0.72]
        across = Vector((0, 1, 0))
        body.slab(bone, grip - forward * unit * 0.07, grip + forward * unit * 0.17, unit * 0.026, unit * 0.032, green)
        front = grip + forward * unit * 0.15
        nock = grip + forward * unit * 0.03
        for sign in (1, -1):
            limb_tip = front + across * sign * unit * 0.1 - forward * unit * 0.04
            body.limb(bone, front + across * sign * unit * 0.012, limb_tip, unit * 0.009, unit * 0.005, brass, segments=6)
            body.limb(bone, limb_tip, nock, unit * 0.0025, unit * 0.0025, cord, segments=4)
            for coil in range(3):
                ring = front + across * sign * unit * (0.022 + coil * 0.012)
                body.limb(bone, ring - forward * unit * 0.004, ring + forward * unit * 0.004, unit * 0.012, unit * 0.012, steel, segments=8)
        body.limb(bone, grip + forward * unit * 0.07 + across * unit * 0.018, grip + forward * unit * 0.07 + across * unit * 0.03, unit * 0.02, unit * 0.02, brass, segments=8)
        body.limb(bone, nock + Vector((0, 0, unit * 0.018)), front + forward * unit * 0.06 + Vector((0, 0, unit * 0.018)), unit * 0.004, unit * 0.004, wood, segments=4)
        body.limb(bone, front + forward * unit * 0.06 + Vector((0, 0, unit * 0.018)), front + forward * unit * 0.085 + Vector((0, 0, unit * 0.018)), unit * 0.008,
                  unit * 0.001, steel, segments=4)
    elif kind == "cleaver":
        # An oversized hooked slag cleaver made from a mining tool: an iron-bound haft, a huge broad blade bolted to it,
        # its back hooked forward and up at the tip, its cutting edge burning molten, slag clinging to its flat and
        # roping off its edge.
        iron, scorch, slag = [0.30, 0.29, 0.28], [0.12, 0.11, 0.10], spec.get("slag", [0.03, 0.03, 0.03])
        side_axis = Vector((0, 1, 0))
        root = grip + forward * unit * 0.1
        body.limb(bone, grip - forward * unit * 0.05, root, unit * 0.016, unit * 0.018, wood, segments=8)
        body.limb(bone, root - forward * unit * 0.02, root + forward * unit * 0.01, unit * 0.024, unit * 0.024, iron, segments=8)
        at = lambda ahead, below: root + forward * unit * ahead + down * unit * below
        # Its profile from the haft round the hooked back and down the edge, in the plane of forward and down.
        body.pane(bone, [at(0.0, -0.035), at(0.22, -0.05), at(0.34, -0.11), at(0.31, -0.02), at(0.33, 0.07), at(0.26, 0.14), at(0.08, 0.13), at(0.0, 0.07)],
                  unit * 0.012, mix(iron, scorch, 0.35))
        edge = [at(0.33, 0.075), at(0.26, 0.145), at(0.08, 0.135), at(0.005, 0.075)]
        for a, b in zip(edge, edge[1:]):
            body.limb(bone, a, b, unit * 0.007, unit * 0.007, accent, glow=True, segments=5)
        for ahead, below in ((0.03, -0.01), (0.03, 0.06), (0.1, 0.02)):
            for sign in (1, -1):
                body.ball(bone, at(ahead, below) + side_axis * sign * unit * 0.007, unit * 0.007, [0.55, 0.50, 0.42], segments=5)
        for sign in (1, -1):
            body.blob(bone, at(0.18, 0.0) + side_axis * sign * unit * 0.008, (Vector((unit * 0.06, 0, 0)), Vector((0, unit * 0.006, 0)), Vector((0, 0, unit * 0.035))),
                      slag, segments=6)
        for ahead, length in ((0.22, 0.09), (0.12, 0.06)):
            drip = at(ahead, 0.14)
            body.limb(bone, drip, drip + down * unit * length, unit * 0.008, unit * 0.002, slag, segments=5)
    elif kind == "drawings":
        # A bundle of rolled drawings carried in the hand: three rolls of pale paper of different lengths, tied round
        # the middle.
        paper = [0.92, 0.88, 0.76]
        along = (forward * 0.8 + Vector((0, 0, 0.6))).normalized()
        for index, (length, offset) in enumerate(((0.32, 0.0), (0.26, 0.022), (0.29, -0.02))):
            centre = grip + Vector((0, offset * unit, offset * unit * 0.5))
            body.limb(bone, centre - along * unit * length * 0.45, centre + along * unit * length * 0.55, unit * 0.018, unit * 0.018,
                      mix(paper, [0.75, 0.70, 0.58], 0.15 * index), segments=8)
        body.limb(bone, grip - along * unit * 0.012, grip + along * unit * 0.012, unit * 0.04, unit * 0.04, [0.45, 0.15, 0.10], segments=8)
    else:
        raise AssertionError("Unknown prop: " + kind)


def run_stride(d):
    """How far one Run cycle carries the body: two steps, each the planted foot sweeping from one swing to the other."""
    return 2 * 2 * (d["leg"]) * math.sin(math.radians(RUN_THIGH_SWING))


def slumped(d):
    """A toy slumped where it was dropped: sat down, legs out, its body tipped forward, head lolling, arms limp. Every
    clip of a still body holds it."""
    pose = {"spine_01": lean(25), "spine_02": lean(10), "head": combine(lean(35), twist(18))}
    for side, sign in (("l", 1), ("r", -1)):
        pose["thigh_" + side] = combine(forward_swing(85), roll_side(12, sign))
        pose["calf_" + side] = forward_swing(-5)
        pose["upperarm_" + side] = combine(roll_side(18, sign), forward_swing(10))
    return pose, -d["leg"] * 0.8


def pose(name, t, melee, d):
    """Each bone's rotation (about the armature's axes, in radians) and the pelvis's lift at t from 0 to 1."""
    if d.get("stillPose") == "slumped":
        return slumped(d)
    pose, lift = {}, 0.0
    tau = math.tau
    if name == "Idle" and d.get("idle") == "bouncy":
        # Never still: bouncing on her toes, leaning in, ready to lunge.
        bounce = abs(math.sin(t * tau * 2))
        pose["spine_01"] = lean(10)
        pose["head"] = twist(12 * math.sin(t * tau))
        for side, sign in (("l", 1), ("r", -1)):
            pose["upperarm_" + side] = combine(forward_swing(20 + 10 * math.sin(t * tau * 2 + sign)), roll_side(15, sign))
            pose["lowerarm_" + side] = forward_swing(50)
            pose["thigh_" + side] = forward_swing(12)
            pose["calf_" + side] = forward_swing(-24)
        lift = d["height"] * (0.03 * bounce - 0.02)
    elif name == "Idle":
        breath = math.sin(t * tau)
        pose["spine_03"] = lean(1.5 * breath)
        pose["head"] = twist(4 * math.sin(t * tau + 1.0))
        for side, sign in (("l", 1), ("r", -1)):
            pose["upperarm_" + side] = forward_swing(3 * math.sin(t * tau + sign))
        lift = d["height"] * 0.006 * breath
    elif name == "Run":
        stride = math.sin(t * tau)
        pose["spine_01"] = lean(10)
        pose["spine_03"] = twist(8 * stride)
        for side, sign in (("l", 1), ("r", -1)):
            phase = stride * sign
            pose["thigh_" + side] = forward_swing(RUN_THIGH_SWING * phase)
            pose["calf_" + side] = forward_swing(-40 * max(0.0, -phase) - 15)
            pose["upperarm_" + side] = forward_swing(-30 * phase)
            pose["lowerarm_" + side] = forward_swing(45)
        lift = d["height"] * 0.025 * abs(math.cos(t * tau))
    elif name in ("AttackWindup", "AttackStrike"):
        # The windup ends on the moment the attack commits, the strike follows through from it. Melee: the chest
        # turns right and the right arm cocks out and back, weapon up, then sweeps forward across the body, landing
        # as the windup ends; the strike settles back. Ranged: both arms raise to aim, hands level so the weapon
        # points ahead; the strike is the release's recoil, then the arms lower.
        if name == "AttackWindup":
            if melee:
                cock = ease(min(1.0, t / MELEE_COCK_SHARE))
                sweep = ease(max(0.0, (t - MELEE_COCK_SHARE) / (1.0 - MELEE_COCK_SHARE)))
            else:
                cock, sweep = ease(t), 0.0
            settle, recoil = 0.0, 0.0
        elif melee:
            cock, sweep, settle, recoil = 1.0, 1.0, ease(t), 0.0
        else:
            cock, sweep = 1.0, 0.0
            settle = ease(max(0.0, (t - 0.35) / 0.65))
            recoil = math.sin(min(1.0, t / 0.4) * math.pi)
        hold = 1.0 - settle
        # Archetypes that borrow these poses (the colossi) swing, as a figure without a strike of its own does.
        if melee and d.get("strike", {}).get("style") == "punch":
            # A brawler's punch: the shoulder loads back with the fist by the jaw and the other hand up in guard, then
            # the body lunges and the fist drives straight out, landing as the windup ends.
            side = d["strike"]["hand"][0]
            sign = 1 if side == "l" else -1
            guard = "r" if side == "l" else "l"
            pose["spine_01"] = lean(15 * sweep * hold)
            pose["spine_02"] = twist(sign * (30 * cock - 70 * sweep) * hold)
            pose["upperarm_" + side] = tuple(a + b for a, b in zip(forward_swing((-30 * cock + 120 * sweep) * hold), roll_side((25 * cock - 30 * sweep) * hold, sign)))
            pose["lowerarm_" + side] = forward_swing((100 * cock - 100 * sweep) * hold)
            pose["upperarm_" + guard] = forward_swing(40 * cock * hold)
            pose["lowerarm_" + guard] = forward_swing(90 * cock * hold)
            lift = d["height"] * 0.02 * sweep * hold
        elif melee:
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
            pose["spine_02"] = combine(lean(-6 * recoil), twist(-10 * cock * hold))
    elif name == "Cast":
        rise = ease(min(1.0, t / CAST_RELEASE_SHARE))
        fall = ease(max(0.0, (t - 0.6) / 0.4))
        k = rise * (1 - fall)
        for side, sign in (("l", 1), ("r", -1)):
            pose["upperarm_" + side] = tuple(a + b for a, b in zip(forward_swing(85 * k), roll_side(20 * k, sign)))
            pose["lowerarm_" + side] = forward_swing(10 * k)
        pose["spine_03"] = lean(-6 * k)
        lift = d["height"] * 0.02 * k
    elif name == "Hit":
        k = math.sin(min(1.0, t) * math.pi)
        pose["spine_02"] = lean(-18 * k)
        pose["head"] = lean(-12 * k)
        for side, sign in (("l", 1), ("r", -1)):
            pose["upperarm_" + side] = roll_side(20 * k, sign)
    elif name == "Death":
        # Knocked back off its feet; a rider on a base falls back off it, and the base stays.
        k = ease(min(1.0, t / 0.8))
        if d["base"] > 0:
            pose["pelvis"] = lean(-80 * k)
            lift = -d["leg"] * 0.6 * k
        else:
            pose["root"] = lean(-80 * k)
            lift = -d["height"] * 0.05 * k
        pose["spine_02"] = lean(15 * k)
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
        pose["head"] = lean(10 * settle + 2 * glow)
        lift = -d["height"] * 0.06 * settle
    else:
        raise AssertionError("Unknown animation: " + name)
    if d.get("hunch") and name != "Death":
        # A brute's hunch, held through everything it does: the back bowed forward by hunch degrees, the head raised
        # against it to keep looking ahead, the arms hanging forward of the body.
        bow = d["hunch"]
        pose["spine_01"] = combine(pose.get("spine_01", (0.0, 0.0, 0.0)), lean(bow * 0.6))
        pose["spine_02"] = combine(pose.get("spine_02", (0.0, 0.0, 0.0)), lean(bow * 0.4))
        pose["head"] = combine(pose.get("head", (0.0, 0.0, 0.0)), lean(-bow * 0.85))
    if d.get("stance") in ("aim", "braced"):
        aim_pose(pose, name, t, melee, sight=d["stance"] == "aim")
    if d.get("stance") == "shoulderCarry":
        carry_pose(pose, name, t)
    if d.get("stance") == "lanternOut":
        lantern_pose(pose, name, t)
    if d.get("kneel") and name != "Death":
        # Dug in: down on one knee, braced, whatever the upper body does.
        pose["thigh_l"] = forward_swing(80)
        pose["calf_l"] = forward_swing(-85)
        pose["thigh_r"] = forward_swing(-10)
        pose["calf_r"] = forward_swing(-90)
        lift = -d["leg"] * 0.42
    if d.get("ride") == "wave":
        if name != "Death":
            # Carried, not running: the wave's motion replaces the stride's.
            lift = (0.0 if name == "Run" else lift) + wave_ride(pose, name, t, d)
        pose.update(crest_pose(name, t))
    else:
        pose.update(cape_pose(name, t))
    return pose, lift


def wave_ride(pose, name, t, d):
    """Braced on the wave whatever the upper body does: hips turned, the left foot forward and the right back, knees bent
    and weight forward. Riding, she leans into it with her arms out for balance and rises and falls with the swell; at
    rest it lifts her gently. Her lift."""
    swell = math.sin(t * math.tau)
    pose["pelvis"] = twist(18)
    pose["spine_02"] = combine(pose.get("spine_02", (0.0, 0.0, 0.0)), twist(-14))
    pose["thigh_l"] = combine(forward_swing(28), roll_side(6, 1))
    pose["calf_l"] = forward_swing(-38)
    pose["thigh_r"] = combine(forward_swing(-18), roll_side(6, -1))
    pose["calf_r"] = forward_swing(-34)
    lift = -d["leg"] * 0.14
    if name == "Run":
        pose["spine_01"] = lean(16)
        for side, sign in (("l", 1), ("r", -1)):
            pose["upperarm_" + side] = combine(roll_side(40 + 6 * math.sin(t * math.tau + sign), sign), forward_swing(15))
            pose["lowerarm_" + side] = forward_swing(20)
        lift += d["height"] * 0.02 * swell
    elif name == "Idle":
        lift += d["height"] * 0.012 * swell
    return lift


def crest_pose(name, t):
    """A wave's crest on the cape bones, which point up it: rolling forward over itself and back at rest, curling
    harder and faster as it carries her, rearing then breaking as she casts or strikes, and falling flat as she falls."""
    roll = lambda speed, phase: math.sin(t * math.tau * speed + phase)
    # An upright bone's tip goes forward as it swings back.
    curl = lambda degrees: forward_swing(-degrees)
    if name in ("Idle", "Recall"):
        return {"cape_01": curl(3 * roll(1, 0)), "cape_02": curl(4 * roll(1, 1)), "cape_03": curl(8 + 6 * roll(1, 2))}
    if name == "Run":
        return {"cape_01": curl(8 + 4 * roll(2, 0)), "cape_02": curl(12 + 6 * roll(2, 1)), "cape_03": curl(22 + 10 * roll(2, 2))}
    if name == "Death":
        k = ease(min(1.0, t / 0.8))
        return {"cape_01": curl(45 * k), "cape_02": curl(35 * k), "cape_03": curl(25 * k)}
    if name == "Cast":
        rise = ease(min(1.0, t / CAST_RELEASE_SHARE))
        fall = ease(max(0.0, (t - CAST_RELEASE_SHARE) / (1.0 - CAST_RELEASE_SHARE)))
        return {"cape_01": curl(-10 * rise * (1 - fall) + 15 * fall), "cape_02": curl(10 * fall), "cape_03": curl(10 * rise + 30 * fall)}
    k = math.sin(min(1.0, t) * math.pi)
    return {"cape_01": curl(6 * k), "cape_02": curl(10 * k), "cape_03": curl(18 * k)}


def torso_radius(L, d, z):
    """How far the torso body() draws reaches from the spine at height z: its pelvis, belly and chest, each a cone."""
    sections = [(L["pelvis"][0].z, d["hip"] * 1.05), (L["pelvis"][1].z, d["hip"]), (L["spine_01"][1].z, d["shoulder"] * 0.72),
                (L["spine_02"][1].z, d["shoulder"] * 0.86), (L["spine_03"][1].z, d["shoulder"] * 0.62)]
    if z <= sections[0][0]:
        return sections[0][1]
    for (z0, r0), (z1, r1) in zip(sections, sections[1:]):
        if z <= z1:
            return r0 + (r1 - r0) * (z - z0) / (z1 - z0)
    return sections[-1][1]


def carry_pose(pose, name, t):
    """A heavy blade carried over the right shoulder: the upper arm a little forward and in, the forearm folded up so the
    fist sits before the shoulder and the blade, which points ahead of a hanging hand, lies back over it. Held so at rest,
    running (rocking with the stride), flinching and recalling; an attack or a cast takes it off the shoulder, and a
    fall drops it."""
    if name not in ("Idle", "Run", "Hit", "Recall"):
        return
    rock = 6 * math.sin(t * math.tau) if name == "Run" else 0.0
    pose["upperarm_r"] = combine(forward_swing(CARRY_UPPER_ARM + rock), roll_side(-CARRY_ROLL_IN, -1))
    pose["lowerarm_r"] = forward_swing(CARRY_FOREARM)
    # The blade's flat turned up a little about its length, so the camera above sees its breadth, not its edge.
    pose["hand_r"] = roll_side(CARRY_FLAT_UP, -1)


def lantern_pose(pose, name, t):
    """A lantern held out at arm's length before her on its chain: the left arm raised forward and out, the hand turned
    back as far so the lantern hangs plumb beneath it, swaying a little as she walks or runs. A cast lifts it with both
    hands; a fall drops it."""
    if name not in ("Idle", "Run", "Hit", "Recall", "AttackWindup", "AttackStrike"):
        return
    sway = (3 if name == "Idle" else 7 if name == "Run" else 0) * math.sin(t * math.tau)
    raise_ = LANTERN_RAISE + sway
    pose["upperarm_l"] = combine(forward_swing(raise_), roll_side(LANTERN_OUT, 1))
    pose["lowerarm_l"] = forward_swing(LANTERN_ELBOW)
    pose["hand_l"] = combine(forward_swing(-(raise_ + LANTERN_ELBOW)), roll_side(-LANTERN_OUT, 1))


def aim_pose(pose, name, t, melee, sight=True):
    """A weapon held two-handed keeps both hands on it: the arms keep their hold and the upper body carries it. Aiming
    it, the chest settles over the sight (a gun held low across the body has none to look down); a cast raises it high;
    a recall lowers it."""
    for bone in [bone for bone in pose if bone.startswith(("clavicle_", "upperarm_", "lowerarm_", "hand_"))]:
        del pose[bone]
    if sight and name not in ("Death", "Cast"):
        # One eye down the sight.
        pose["head"] = combine(pose.get("head", (0.0, 0.0, 0.0)), lean(12))
    if name == "Idle":
        pose["spine_02"] = lean(4 + 1.5 * math.sin(t * math.tau))
    elif name == "AttackWindup" and not melee:
        pose["spine_02"] = combine(lean(6 * ease(t)), twist(-4 * ease(t)))
    elif name == "Cast":
        k = ease(min(1.0, t / CAST_RELEASE_SHARE)) * (1 - ease(max(0.0, (t - 0.6) / 0.4)))
        pose["spine_02"] = lean(-28 * k)
    elif name == "Recall":
        pose["spine_02"] = lean(25 * ease(min(1.0, t * 4)))


def cape_pose(name, t):
    """A cloak's three lengths, each turned back from the one above: stirring at rest, streaming as the body runs,
    flicked by a blow, an attack or a cast. A body without a cloak has the bones but nothing on them."""
    wave = lambda speed, phase: math.sin(t * math.tau * speed + phase)
    if name in ("Idle", "Recall"):
        return {"cape_01": forward_swing(-3 - 2 * wave(1, 0)), "cape_02": forward_swing(-2 - 2 * wave(1, 1)), "cape_03": forward_swing(-2 - 3 * wave(1, 2))}
    if name == "Run":
        return {"cape_01": forward_swing(-30 - 4 * wave(2, 0)), "cape_02": forward_swing(-14 - 7 * wave(2, 1)), "cape_03": forward_swing(-10 - 10 * wave(2, 2))}
    if name == "Death":
        return {}
    k = math.sin(min(1.0, t) * math.pi)
    return {"cape_01": forward_swing(-12 * k), "cape_02": forward_swing(-8 * k), "cape_03": forward_swing(-8 * k)}
