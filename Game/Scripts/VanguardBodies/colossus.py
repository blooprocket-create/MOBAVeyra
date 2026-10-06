"""The colossus archetype (ADR-064 §1): Silt, Relay, Varkesh and Cairn.

The humanoid's skeleton without its tail, in colossal proportions: a towering, forward-leaning trunk, enormous shoulders
and long arms over short legs, the head small and set low between the shoulders, or none. Each body is built in its own
material: sediment sheets over a dark wet core, bone-white slabs over dark mechanism, iron plates over a glowing
interior, or rough riverstone. Its animations are the humanoid's, heavier: a stomping run (on its knuckles where its arms
reach the ground), an overhead throw for a ranged attack and a two-fisted slam to cast.
"""
import math
import random

from mathutils import Euler, Vector

from . import humanoid
from .parts import Body, combine, ease, forward_swing, lean, mix, roll_side, twist

BONES = [bone for bone in humanoid.BONES if not bone[0].startswith(("tail_", "cape_"))]
UPPER_BODY_BONE = "spine_01"
LIFT_BONE = "pelvis"
GROUNDED = True
CAST_RELEASE_SHARE = humanoid.CAST_RELEASE_SHARE
# A colossus's heavy stride: how far a thigh swings either side of straight down, in degrees.
RUN_THIGH_SWING = 22.0
# How far a ranged colossus raises its throwing arm from hanging, over and behind its head, in degrees.
THROW_RAISE = 150.0
# The share of a throw spent bringing the arm over before the release.
THROW_SHARE = 0.3
# Moss on the upper surfaces of an overgrown colossus.
MOSS = [0.22, 0.40, 0.14]


def arm_scale(features, side):
    """An asymmetrical colossus's hook arm is the larger, its other arm the shorter support."""
    if "hookArm" in features:
        return 1.2 if side == "r" else 0.8
    return 1.0


def layout(spec, capsule):
    """Every bone's head and tail in centimetres, the body facing +X with its left at +Y, fitted to the capsule."""
    features = set(spec["features"])
    full = capsule["capsuleHalfHeight"] * 2.0 * spec["heightShare"]
    height = full
    build = humanoid.BUILD[spec["build"]]
    head = height * spec["headShare"]
    leg = height * spec["legShare"]
    torso = height - leg - head * 0.5
    shoulder = max(capsule["capsuleRadius"], height * 0.2) * build
    hip = shoulder * 0.55
    arm = height * spec["armShare"]
    lean_share = spec["lean"]
    sole = height * 0.05 * build
    pelvis_z = leg
    chest_z = pelvis_z + torso

    def at(z, y=0.0):
        """A point up the forward-leaning trunk."""
        return Vector(((z - pelvis_z) * lean_share, y, z))

    L = {}
    L["root"] = (Vector((0, 0, 0)), Vector((0, 0, leg * 0.25)))
    L["pelvis"] = (at(pelvis_z), at(pelvis_z + torso * 0.2))
    L["spine_01"] = (at(pelvis_z + torso * 0.2), at(pelvis_z + torso * 0.45))
    L["spine_02"] = (at(pelvis_z + torso * 0.45), at(pelvis_z + torso * 0.72))
    L["spine_03"] = (at(pelvis_z + torso * 0.72), at(chest_z))
    # The head sits low between the shoulders, ahead of them.
    neck_base = at(chest_z - max(head, height * 0.04) * 0.6)
    head_base = neck_base + Vector((shoulder * 0.35, 0, height * 0.01))
    L["neck_01"] = (neck_base, head_base)
    L["head"] = (head_base, head_base + Vector((0, 0, max(head, height * 0.04))))
    for side, sign in (("l", 1.0), ("r", -1.0)):
        shoulder_point = at(chest_z - torso * 0.12, sign * shoulder)
        L["clavicle_" + side] = (at(chest_z - torso * 0.12, sign * shoulder * 0.2), shoulder_point)
        # Arms hang a little forward and out; a knuckle-walker's reach the ground.
        down = Vector((math.sin(math.radians(12)), sign * math.sin(math.radians(10)), -1.0)).normalized()
        length = arm * arm_scale(features, side)
        if "knuckleWalk" in features:
            length = (shoulder_point.z - sole * 2.0) / -down.z
        elbow = shoulder_point + down * length * 0.45
        wrist = elbow + down * length * 0.40
        hand_end = wrist + down * length * 0.15
        L["upperarm_" + side] = (shoulder_point, elbow)
        L["lowerarm_" + side] = (elbow, wrist)
        L["hand_" + side] = (wrist, hand_end)
        L["prop_" + side] = (hand_end, hand_end + Vector((length * 0.15, 0, 0)))
        hip_point = Vector((0, sign * hip, pelvis_z))
        hock = Vector((0, sign * hip * 1.1, sole))
        knee = Vector((leg * 0.06, sign * hip * 1.05, (pelvis_z + sole) / 2))
        L["thigh_" + side] = (hip_point, knee)
        L["calf_" + side] = (knee, hock)
        L["foot_" + side] = (hock, Vector((hock.x + leg * 0.35, hock.y, sole * 0.8)))
    dims = {"height": height, "full": full, "base": 0.0, "head": head, "torso": torso, "leg": leg, "shoulder": shoulder,
            "hip": hip, "arm": arm, "build": build, "sole": sole, "knuckle": "knuckleWalk" in features}
    return L, dims


def mass(body, style, bone, start, end, r0, r1, spec, rng):
    """One segment of a colossus in its material."""
    primary, secondary, accent = spec["primary"], spec["secondary"], spec["accent"]
    if style == "sediment":
        # Flaking sheets of drying sediment over a darker, wetter interior.
        body.limb(bone, start, end, r0 * 0.85, r1 * 0.85, secondary)
        body.plates(bone, start, end, (r0 + r1) / 2, mix(primary, accent, rng.uniform(0.0, 0.4)), rng, count=5)
    elif style == "slab":
        # Bone-white slab plating over dark exposed mechanism.
        body.limb(bone, start, end, r0 * 0.55, r1 * 0.55, secondary)
        body.slab(bone, start + (end - start) * 0.06, end - (end - start) * 0.06, (r0 + r1) * 0.95, (r0 + r1) * 0.85, primary,
                  roll=rng.uniform(-6, 6))
    elif style == "iron":
        # Interlocking iron plates; the molten interior glows through the seams between them.
        body.limb(bone, start, end, r0 * 0.7, r1 * 0.7, accent, glow=True)
        body.slab(bone, start + (end - start) * 0.1, end - (end - start) * 0.1, (r0 + r1) * 0.95, (r0 + r1) * 0.9, primary,
                  roll=rng.uniform(-8, 8))
    elif style == "riverstone":
        body.rocks(bone, start, end, r0, r1, [primary, secondary, mix(primary, secondary, 0.5)], rng)
    else:
        raise AssertionError("Unknown colossus style: " + style)


def body(spec, L, d):
    rng = random.Random(spec["seed"])
    body = Body([name for name, _ in BONES])
    features = set(spec["features"])
    style = spec["style"]
    primary, secondary, accent, detail = spec["primary"], spec["secondary"], spec["accent"], spec["detail"]
    shoulder, hip = d["shoulder"], d["hip"]
    # The trunk, broadest at the chest: a top-heavy colossus.
    mass(body, style, "pelvis", L["pelvis"][0] - Vector((0, 0, d["torso"] * 0.08)), L["pelvis"][1], hip * 1.1, hip * 1.0, spec, rng)
    mass(body, style, "spine_01", *L["spine_01"], hip * 1.0, shoulder * 0.75, spec, rng)
    mass(body, style, "spine_02", *L["spine_02"], shoulder * 0.8, shoulder * 0.95, spec, rng)
    mass(body, style, "spine_03", *L["spine_03"], shoulder * 0.95, shoulder * 0.8, spec, rng)
    # Enormous shoulders.
    for side in ("l", "r"):
        s0, s1 = L["clavicle_" + side]
        scale = arm_scale(features, side)
        mass(body, style, "clavicle_" + side, s0, s1 + (s1 - s0).normalized() * shoulder * 0.15, shoulder * 0.35 * scale, shoulder * 0.42 * scale, spec, rng)
    # Arms; their hands by the colossus's kind.
    for side in ("l", "r"):
        scale = arm_scale(features, side)
        thick = shoulder * 0.3 * scale
        mass(body, style, "upperarm_" + side, *L["upperarm_" + side], thick, thick * 0.9, spec, rng)
        mass(body, style, "lowerarm_" + side, *L["lowerarm_" + side], thick * 0.9, thick * 0.85, spec, rng)
        hand(body, spec, features, side, L, thick, rng)
    # Short, heavy legs.
    for side in ("l", "r"):
        mass(body, style, "thigh_" + side, *L["thigh_" + side], hip * 0.6, hip * 0.55, spec, rng)
        # Riverstone's rough chunks turn about the shin, so it stops a sole above the ankle and none sinks below the ground.
        knee, ankle = L["calf_" + side]
        ankle = ankle + Vector((0, 0, d["sole"])) if style == "riverstone" else ankle
        mass(body, style, "calf_" + side, knee, ankle, hip * 0.55, hip * 0.5, spec, rng)
        f0, f1 = L["foot_" + side]
        # Flat on the ground: its depth (along the frame's X, here upright) is the sole's thickness.
        body.slab("foot_" + side, f0 - Vector((d["leg"] * 0.08, 0, 0)), f1, hip * 0.9, d["sole"] * 1.6, mix(secondary, primary, 0.3))
    head(body, spec, features, L, d, rng)
    extras(body, spec, features, L, d, rng)
    return body


def hand(body, spec, features, side, L, thick, rng):
    primary, secondary, accent = spec["primary"], spec["secondary"], spec["accent"]
    h0, h1 = L["hand_" + side]
    bone = "hand_" + side
    reach = h1 - h0
    if "claws" in features:
        # Long tapering clawed digits splayed forward along the ground.
        for index in (-1, 0, 1):
            spread = Vector((0, index * thick * 0.5, 0))
            tip = h1 + spread + Vector((reach.length * 0.7, 0, -h1.z * 0.8))
            body.limb(bone, h0 + spread * 0.4, tip, thick * 0.35, thick * 0.05, mix(primary, secondary, 0.3))
    elif "manipulators" in features or "openHands" in features:
        # A broad palm and spread fingers: Relay's lifting manipulators, or Varkesh's open, empty hands.
        body.slab(bone, h0, h0 + reach * 0.6, thick * 1.6, thick * 0.9, primary)
        fingers = 3 if "manipulators" in features else 5
        for index in range(fingers):
            across = (index - (fingers - 1) / 2) / max(1, fingers - 1)
            tip = h0 + reach * 1.3 + Vector((reach.length * 0.25, across * thick * 1.6, 0))
            body.limb(bone, h0 + reach * 0.5 + Vector((0, across * thick * 1.2, 0)), tip, thick * 0.22, thick * 0.12, secondary)
        if "openHands" in features:
            # Molten fragments torn from his plating, held ready to throw.
            for index in range(4):
                angle = index / 4 * math.tau + rng.uniform(-0.3, 0.3)
                around = Vector((math.cos(angle), math.sin(angle), rng.uniform(-0.3, 0.3))) * thick * 1.6
                body.box("prop_" + side, h1 + reach * 0.4 + around, (thick * 0.35,) * 3, accent, glow=True,
                         rotation=Euler((rng.uniform(0, 3), rng.uniform(0, 3), 0)))
    else:
        # A fist of stone.
        body.rocks(bone, h0, h1 + reach * 0.3, thick * 1.1, thick * 1.0, [primary, secondary], rng, chunks=2)
    if "hookArm" in features and side == "r":
        # The rusted iron crescent, the size of his head, hung on heavy chain from the hook arm.
        grip = L["prop_" + side][0]
        iron = mix(accent, [0.1, 0.08, 0.07], 0.2)
        radius = thick * 0.9
        # Hung short below the hand, and never into the ground.
        drop = min(thick * 1.3, grip.z - radius - thick * 0.3)
        for index in range(2):
            body.ball("prop_" + side, grip + Vector((0, 0, -drop * (index + 1) / 3)), thick * 0.22, mix(accent, secondary, 0.5))
        center = grip + Vector((thick * 0.6, 0, -drop))
        for index in range(7):
            angle = math.radians(-40 + index * 35)
            point = center + Vector((math.cos(angle) * radius, 0, math.sin(angle) * radius))
            body.box("prop_" + side, point, (thick * 0.55, thick * 0.35, thick * 0.6), iron, rotation=Euler((0, -angle, 0)))


def head(body, spec, features, L, d, rng):
    if "faceless" in features and d["head"] <= 0:
        return
    primary, secondary, accent = spec["primary"], spec["secondary"], spec["accent"]
    h0, h1 = L["head"]
    size = max(d["head"], d["height"] * 0.06)
    center = (h0 + h1) / 2
    if spec["style"] == "slab":
        body.box("head", center, (size * 1.2, size * 1.1, size), primary)
        if "optic" in features:
            # A single large ringed amber optic.
            body.limb("head", center + Vector((size * 0.6, 0, 0)), center + Vector((size * 0.72, 0, 0)), size * 0.38, size * 0.38, secondary)
            body.ball("head", center + Vector((size * 0.72, 0, 0)), size * 0.26, accent, glow=True)
    elif spec["style"] == "iron":
        # A faceless wedge of plate, lit from inside, crested with short spines.
        body.limb("head", center - Vector((size * 0.5, 0, 0)), center + Vector((size * 0.9, 0, 0)), size * 0.7, size * 0.15, primary, segments=4)
        body.ball("head", center + Vector((size * 0.3, 0, -size * 0.1)), size * 0.25, accent, glow=True, scale=(1.0, 1.6, 0.5))
        if "headSpines" in features:
            for index in range(4):
                base = center + Vector((size * (0.4 - index * 0.3), 0, size * 0.35))
                body.limb("head", base, base + Vector((-size * 0.25, 0, size * 0.6)), size * 0.14, size * 0.01, secondary)
    else:
        body.rocks("head", h0, h1, size * 0.55, size * 0.5, [primary, secondary], rng, chunks=1)


def extras(body, spec, features, L, d, rng):
    primary, secondary, accent, detail = spec["primary"], spec["secondary"], spec["accent"], spec["detail"]
    shoulder = d["shoulder"]
    s0, s1 = L["spine_03"]
    if "moss" in features:
        # Old growth on the upper surfaces: shoulders, forearms and thighs.
        for bone in ("clavicle_l", "clavicle_r", "lowerarm_l", "lowerarm_r", "thigh_l", "thigh_r"):
            b0, b1 = L[bone]
            top = (b0 + b1) / 2 + Vector((0, 0, shoulder * 0.3 if bone.startswith("clavicle") else shoulder * 0.18))
            body.ball(bone, top, shoulder * rng.uniform(0.18, 0.26), MOSS, scale=(1.3, 1.3, 0.45))
    if "chainWrap" in features:
        # Heavy chain wound about his torso and shoulders.
        c0, c1 = L["spine_02"]
        for index in range(18):
            angle = index / 18 * math.tau * 1.5
            z = c0.z + (c1.z - c0.z) * (index / 18)
            point = Vector((c0.x + math.cos(angle) * shoulder * 0.95, math.sin(angle) * shoulder * 0.95, z))
            body.ball("spine_02", point, shoulder * 0.09, mix(accent, secondary, 0.4))
    if "mast" in features:
        # A slender antenna mast from his back, and the faded crimson expedition banner tied to it.
        foot = s1 + Vector((-shoulder * 0.6, shoulder * 0.3, -d["torso"] * 0.1))
        tip = foot + Vector((-shoulder * 0.15, 0, d["height"] * 0.28))
        body.limb("spine_03", foot, tip, shoulder * 0.05, shoulder * 0.03, secondary)
        body.ball("spine_03", tip, shoulder * 0.07, accent, glow=True)
        if "banner" in features:
            body.box("spine_03", tip + Vector((-shoulder * 0.05, -shoulder * 0.35, -d["height"] * 0.12)),
                     (shoulder * 0.04, shoulder * 0.7, d["height"] * 0.22), detail)
    if "coreSpiral" in features:
        # The bright core spiral set into his chest.
        c0, c1 = L["spine_02"]
        core = c1 + Vector((shoulder * 0.85, 0, 0))
        body.ball("spine_02", core, shoulder * 0.28, accent, glow=True)
        for index in range(5):
            angle = index * 1.3
            body.ball("spine_02", core + Vector((shoulder * 0.08, math.cos(angle) * shoulder * 0.3 * (index + 1) / 5,
                                                  math.sin(angle) * shoulder * 0.3 * (index + 1) / 5)), shoulder * 0.07, accent, glow=True)
    if "clothDrape" in features:
        # A heavy cloth drape at the waist, held by iron rings.
        p0, p1 = L["pelvis"]
        for sign in (1, -1):
            body.box("pelvis", p0 + Vector((sign * d["hip"] * 0.95, 0, -d["leg"] * 0.25)), (d["hip"] * 0.15, d["hip"] * 1.6, d["leg"] * 0.6), detail)
        body.limb("pelvis", p0 + Vector((0, 0, d["torso"] * 0.05)), p0 + Vector((0, 0, d["torso"] * 0.1)), d["hip"] * 1.2, d["hip"] * 1.2, secondary)
    if "flakes" in features:
        # Sheets of wet sediment peeling off his back as he moves.
        for index in range(5):
            spot = s0.lerp(s1, rng.uniform(0, 1)) + Vector((-shoulder * rng.uniform(0.6, 0.9), rng.uniform(-shoulder, shoulder) * 0.6, 0))
            body.box("spine_03", spot, (shoulder * 0.08, shoulder * rng.uniform(0.4, 0.6), shoulder * rng.uniform(0.3, 0.5)),
                     mix(primary, accent, 0.3), rotation=Euler((rng.uniform(-0.6, 0.6), rng.uniform(-0.6, 0.6), rng.uniform(-0.6, 0.6))))


def run_stride(d):
    """How far one Run cycle carries the body: two heavy steps, each the planted foot sweeping from one swing to the other."""
    return 2 * 2 * d["leg"] * math.sin(math.radians(RUN_THIGH_SWING))


def pose(name, t, melee, d):
    """Each bone's rotation (about the armature's axes, in radians) and the pelvis's lift at t from 0 to 1."""
    if name == "Run":
        return run(t, d)
    if name in ("AttackWindup", "AttackStrike") and not melee:
        return throw(name, t)
    if name == "Cast":
        return slam(t)
    return humanoid.pose(name, t, melee, d)


def run(t, d):
    """A heavy stomp, the trunk leaning into it; a knuckle-walker's arms swing as forelegs."""
    pose = {}
    stride = math.sin(t * math.tau)
    pose["spine_01"] = lean(8)
    pose["spine_03"] = twist(6 * stride)
    for side, sign in (("l", 1), ("r", -1)):
        phase = stride * sign
        pose["thigh_" + side] = forward_swing(RUN_THIGH_SWING * phase)
        pose["calf_" + side] = forward_swing(-30 * max(0.0, -phase) - 8)
        if d["knuckle"]:
            pose["upperarm_" + side] = forward_swing(-28 * phase)
        else:
            pose["upperarm_" + side] = forward_swing(-20 * phase)
            pose["lowerarm_" + side] = forward_swing(25)
    return pose, d["height"] * 0.015 * abs(math.cos(t * math.tau))


def throw(name, t):
    """A ranged attack: the right arm comes up over and behind the head through the windup, and is flung forward at the
    release, the trunk leaning into it, then settles."""
    pose = {}
    if name == "AttackWindup":
        raised, flung, settle = ease(t), 0.0, 0.0
    else:
        raised = 1.0
        flung = ease(min(1.0, t / THROW_SHARE))
        settle = ease(max(0.0, (t - THROW_SHARE) / (1.0 - THROW_SHARE)))
    hold = 1.0 - settle
    pose["upperarm_r"] = combine(forward_swing((THROW_RAISE * raised - 100 * flung) * hold), roll_side(15 * raised * hold, -1))
    pose["lowerarm_r"] = forward_swing((40 * raised - 40 * flung) * hold)
    pose["spine_02"] = combine(lean((-10 * raised + 25 * flung) * hold), twist((-20 * raised + 30 * flung) * hold))
    pose["upperarm_l"] = forward_swing(20 * raised * hold)
    return pose, 0.0


def slam(t):
    """A cast: both fists rise overhead to the release, then come down together into the ground, and recover."""
    pose = {}
    rise = ease(min(1.0, t / CAST_RELEASE_SHARE))
    down = ease(min(1.0, max(0.0, (t - CAST_RELEASE_SHARE) / 0.2)))
    recover = ease(max(0.0, (t - 0.7) / 0.3))
    for side in ("l", "r"):
        pose["upperarm_" + side] = forward_swing((140 * rise - 110 * down) * (1 - recover))
        pose["lowerarm_" + side] = forward_swing((30 * rise - 20 * down) * (1 - recover))
    pose["spine_02"] = lean((-12 * rise + 30 * down) * (1 - recover))
    return pose, 0.0
