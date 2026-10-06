"""The humanoid archetype (ADR-064 §1): its skeleton, its body fitted to a capsule, its props and its animations."""
import math
import random

from mathutils import Vector

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
# The upper body, which alone plays an attack, a cast or a hit while the body runs; the bone a pose lifts; and whether
# the body stands on the ground.
UPPER_BODY_BONE = "spine_01"
LIFT_BONE = "pelvis"
GROUNDED = True


def layout(spec, capsule):
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
    # A cloak hangs from behind the shoulders, a little out from the back, down toward the calves.
    cape_start = Vector((-shoulder * 0.45, 0, chest_z - torso * 0.05))
    cape_dir = Vector((-0.2, 0, -1.0)).normalized()
    cape_length = (cape_start.z - base) * CAPE_REACH
    for index, name in enumerate(("cape_01", "cape_02", "cape_03")):
        L[name] = (cape_start + cape_dir * cape_length * index / 3, cape_start + cape_dir * cape_length * (index + 1) / 3)
    stance = spec.get("stance")
    if stance == "aim":
        # Mid-sight: a long arm held two-handed at the shoulder, its optic at the eye and the left hand forward under the
        # barrel, elbows bent out and down. The weapon points the way the body faces.
        grip_r = Vector((arm * 0.28, -shoulder * 0.2, chest_z - torso * 0.03))
        grips = {"r": grip_r, "l": grip_r + Vector((arm * 0.5, 0.0, -arm * 0.04))}
        for side, sign in (("l", 1.0), ("r", -1.0)):
            shoulder_point = L["upperarm_" + side][0]
            wrist = grips[side] - Vector((arm * 0.12, 0.0, 0.0))
            elbow = two_bone(shoulder_point, wrist, arm * 0.48, arm * 0.40, Vector((0.0, sign * 0.7, -0.7)))
            L["upperarm_" + side] = (shoulder_point, elbow)
            L["lowerarm_" + side] = (elbow, wrist)
            L["hand_" + side] = (wrist, grips[side])
            L["prop_" + side] = (grips[side], grips[side] + Vector((arm * 0.15, 0, 0)))
    elif stance is not None:
        raise AssertionError("Unknown stance: " + stance)
    strike = spec.get("strike", {"style": "swing", "hand": "right"})
    assert strike["style"] in ("swing", "punch") and strike["hand"] in ("left", "right"), ("Unknown strike", strike)
    dims = {"height": height, "full": full, "base": base, "head": head, "torso": torso, "leg": leg, "shoulder": shoulder,
            "hip": hip, "arm": arm, "build": build, "stance": stance, "strike": strike}
    return L, dims


def body(spec, L, d, bones=BONES):
    """The figure of simple parts on L's bones. Another archetype's skeleton that holds these bones (a rider's) passes
    its own bone list, so the parts weight to the right vertex groups."""
    body = Body([name for name, _ in bones])
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
    if hair_style in ("short", "long", "curly", "twinTails", "sideTail", "longBeard", "windblown"):
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
    if hair_style == "windblown":
        # Long hair blown back: a mass from the crown streaming behind the shoulders, its locks fanning out, so from
        # above it trails the head.
        body.ball("head", head_center + Vector((-head_radius * 0.12, 0, head_radius * 0.18)), head_radius * 1.06, hair, scale=(1.05, 1.0, 0.9))
        for index, spread in enumerate((-0.7, -0.25, 0.25, 0.7)):
            tip = head_center + Vector((-head_radius * (2.9 - abs(spread)), spread * head_radius * 1.6, -head_radius * (1.0 + 0.4 * (index % 2))))
            body.limb("head", head_center + Vector((-head_radius * 0.6, spread * head_radius * 0.7, head_radius * 0.3)), tip,
                      head_radius * 0.42, head_radius * 0.1, mix(hair, accent, 0.25 if index % 2 else 0.0))
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
    if "longCloak" in features:
        # A long cloak from the shoulders toward the calves, flaring as it falls, its hem torn into strips of
        # different lengths (seeded). Each length rides its own bone, so it streams back as the body runs.
        cloak = spec.get("cloak", secondary)
        top = L["cape_01"][0]
        body.box("cape_01", top + Vector((d["shoulder"] * 0.15, 0, d["torso"] * 0.05)), (d["shoulder"] * 0.5, d["shoulder"] * 2.1, d["torso"] * 0.22), cloak)
        for index, name in enumerate(("cape_01", "cape_02", "cape_03")):
            c0, c1 = L[name]
            overlap = (c1 - c0) * 0.08
            body.slab(name, c0 - overlap, c1 + overlap, d["shoulder"] * (2.0 + index * 0.15), 2.5, cloak)
        c0, c1 = L["cape_03"]
        hem = d["shoulder"] * 2.3
        down = (c1 - c0).normalized()
        for strip in range(CAPE_STRIPS):
            across = Vector((0, -hem / 2 + hem * (strip + 0.5) / CAPE_STRIPS, 0))
            length = d["height"] * random.uniform(0.03, 0.12)
            body.slab("cape_03", c1 + across - down * 1.0, c1 + across + down * length, hem / CAPE_STRIPS * 0.8, 2.0,
                      mix(cloak, [0.05, 0.02, 0.02], random.uniform(0.0, 0.3)))
    if "scarf" in features:
        body.box("neck_01", L["neck_01"][0] + Vector((-limb * 1.6, 0, -d["torso"] * 0.25)), (limb * 0.5, limb * 2.0, d["torso"] * 0.7), secondary)
    if "coatSkirt" in features:
        body.limb("pelvis", p0 + Vector((0, 0, d["torso"] * 0.05)), p0 - Vector((0, 0, d["leg"] * 0.55)), d["hip"] * 1.15, d["hip"] * 1.55, primary)
    if "shoulderPlate" in features:
        body.ball("clavicle_l", L["clavicle_l"][1] + Vector((0, 0, limb)), limb * 2.2, mix(secondary, metal, 0.6), scale=(1.0, 1.0, 0.6))
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
        body.box("spine_02", (s0 + s1) / 2 + Vector((d["shoulder"] * 0.62, 0, 0)), (d["shoulder"] * 0.2, d["shoulder"] * 0.55, d["torso"] * 0.45), secondary)
        for sign in (1, -1):
            body.slab("spine_02", top + Vector((d["shoulder"] * 0.58, sign * d["shoulder"] * 0.5, 0)),
                      s0 + Vector((d["shoulder"] * 0.7, sign * d["shoulder"] * 0.72, -d["torso"] * 0.12)), d["shoulder"] * 0.42, 3.0, jacket)
            body.slab("pelvis", p1 + Vector((-d["hip"] * 0.1, sign * d["hip"] * 1.0, 0)),
                      p0 + Vector((-d["hip"] * 0.2, sign * d["hip"] * 1.3, -d["torso"] * 0.1)), d["hip"] * 0.9, 3.0, jacket)
        body.slab("spine_03", L["spine_03"][1] + Vector((-d["shoulder"] * 0.35, 0, -d["torso"] * 0.05)),
                  L["spine_03"][1] + Vector((-d["shoulder"] * 0.45, 0, d["torso"] * 0.12)), d["shoulder"] * 0.9, 3.0, jacket)
    if "heavyBoots" in features:
        for side in ("l", "r"):
            f0, f1 = L["foot_" + side]
            body.limb("foot_" + side, f0 - Vector((leg * 0.15, 0, 0)), f1 + Vector((leg * 0.15, 0, 0)), leg * FOOT_HEEL_SHARE * 1.25,
                      leg * FOOT_HEEL_SHARE * FOOT_TOE_SHARE * 1.25, [0.12, 0.08, 0.06])
            c0, c1 = L["calf_" + side]
            body.limb("calf_" + side, c0.lerp(c1, 0.55), c1, leg * 1.0, leg * 1.05, [0.12, 0.08, 0.06])
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


def run_stride(d):
    """How far one Run cycle carries the body: two steps, each the planted foot sweeping from one swing to the other."""
    return 2 * 2 * (d["leg"]) * math.sin(math.radians(RUN_THIGH_SWING))


def pose(name, t, melee, d):
    """Each bone's rotation (about the armature's axes, in radians) and the pelvis's lift at t from 0 to 1."""
    pose, lift = {}, 0.0
    tau = math.tau
    if name == "Idle":
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
    if d.get("stance") == "aim":
        aim_pose(pose, name, t, melee)
    pose.update(cape_pose(name, t))
    return pose, lift


def aim_pose(pose, name, t, melee):
    """A weapon held two-handed keeps both hands on it: the arms keep their hold and the upper body carries it. Aiming
    it, the chest settles over the sight; a cast raises it high; a recall lowers it."""
    for bone in [bone for bone in pose if bone.startswith(("clavicle_", "upperarm_", "lowerarm_", "hand_"))]:
        del pose[bone]
    if name not in ("Death", "Cast"):
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
