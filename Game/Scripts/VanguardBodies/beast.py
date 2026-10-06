"""The beast archetype (ADR-064 §1): Korruk, a low six-legged predator, and Moro, a large antlered quadruped.

A horizontal skeleton: the hips at the back (the pelvis) and the spine running forward to the chest, the neck and the
head carried ahead; hind legs from the hips, forelegs from the chest, and a middle pair for a six-legged beast. The
forelegs' bones carry the arms' names and the hind legs' the legs', so every archetype's hands and feet are found alike.
Its animations are an animal's: a trot (a tripod gait on six legs), a lunging bite or a spine volley, rearing up to cast,
toppling onto its side, and lying down to recall.
"""
import math
import random

from mathutils import Euler, Vector

from .parts import Body, combine, ease, forward_swing, lean, mix, roll_side, twist

LEGS = ("upperarm", "lowerarm", "hand")
MIDDLE = ("midleg_upper", "midleg_lower", "midleg_foot")
HIND = ("thigh", "calf", "foot")
BONES = [("root", None), ("pelvis", "root"), ("spine_01", "pelvis"), ("spine_02", "spine_01"), ("neck_01", "spine_02"), ("head", "neck_01")]
for _side in ("l", "r"):
    BONES += [(LEGS[0] + "_" + _side, "spine_02"), (LEGS[1] + "_" + _side, LEGS[0] + "_" + _side), (LEGS[2] + "_" + _side, LEGS[1] + "_" + _side)]
    BONES += [(MIDDLE[0] + "_" + _side, "spine_01"), (MIDDLE[1] + "_" + _side, MIDDLE[0] + "_" + _side), (MIDDLE[2] + "_" + _side, MIDDLE[1] + "_" + _side)]
    BONES += [(HIND[0] + "_" + _side, "pelvis"), (HIND[1] + "_" + _side, HIND[0] + "_" + _side), (HIND[2] + "_" + _side, HIND[1] + "_" + _side)]
BONES += [("tail_01", "pelvis"), ("tail_02", "tail_01"), ("tail_03", "tail_02")]
UPPER_BODY_BONE = "neck_01"
LIFT_BONE = "pelvis"
GROUNDED = True
# The share of the Cast animation spent rearing up to the release, held until a cast commits.
CAST_RELEASE_SHARE = 0.4
# How far a trotting leg swings either side of straight down, in degrees.
RUN_LEG_SWING = 30.0
# The share of a melee windup spent gathering; the rest lunges through to the bite.
LUNGE_COCK_SHARE = 0.65
BUILD = {"lean": 0.9, "normal": 1.0, "heavy": 1.25}


def layout(spec, capsule):
    """Every bone's head and tail in centimetres, the body facing +X with its left at +Y, fitted to the capsule."""
    features = set(spec["features"])
    build = BUILD[spec["build"]]
    length = capsule["capsuleRadius"] * 2.0 * spec["lengthShare"]
    withers = capsule["capsuleHalfHeight"] * 2.0 * spec["heightShare"]
    trunk = withers * spec["trunkShare"] * build
    sole = trunk * 0.18
    leg = withers - trunk * 0.4
    hips_x, chest_x = -length * 0.32, length * 0.22
    hips_z, chest_z = withers * spec["hipShare"], withers
    L = {}
    L["root"] = (Vector((0, 0, 0)), Vector((0, 0, leg * 0.25)))
    L["pelvis"] = (Vector((hips_x, 0, hips_z)), Vector((hips_x + length * 0.15, 0, hips_z)))
    mid = Vector(((hips_x + chest_x) / 2, 0, (hips_z + chest_z) / 2))
    L["spine_01"] = (L["pelvis"][1], mid)
    L["spine_02"] = (mid, Vector((chest_x, 0, chest_z)))
    head_low = "lowHead" in features
    neck_end = Vector((chest_x + length * 0.18, 0, chest_z + (withers * -0.05 if head_low else withers * 0.25)))
    L["neck_01"] = (L["spine_02"][1], neck_end)
    head = length * spec["headShare"]
    L["head"] = (neck_end, neck_end + Vector((head, 0, -head * (0.25 if head_low else 0.15))))
    for side, sign in (("l", 1.0), ("r", -1.0)):
        wide = trunk * 0.75 * sign
        # Forelegs from the chest: straight down to the paw.
        shoulder = Vector((chest_x - trunk * 0.2, wide, chest_z - trunk * 0.4))
        elbow = shoulder + Vector((-leg * 0.08, 0, -(shoulder.z - sole) * 0.5))
        wrist = Vector((shoulder.x, wide, sole + leg * 0.12))
        L["upperarm_" + side] = (shoulder, elbow)
        L["lowerarm_" + side] = (elbow, wrist)
        L["hand_" + side] = (wrist, Vector((wrist.x + leg * 0.12, wide, sole)))
        # The middle pair, for six legs, splayed a little.
        root = Vector((mid.x, wide, mid.z - trunk * 0.4))
        knee = root + Vector((0, sign * trunk * 0.3, -(root.z - sole) * 0.45))
        ankle = Vector((root.x - leg * 0.05, wide + sign * trunk * 0.35, sole + leg * 0.1))
        L["midleg_upper_" + side] = (root, knee)
        L["midleg_lower_" + side] = (knee, ankle)
        L["midleg_foot_" + side] = (ankle, Vector((ankle.x + leg * 0.1, ankle.y, sole)))
        # Hind legs from the hips: the stifle forward, the hock back, the paw under it.
        hip = Vector((hips_x, wide, hips_z - trunk * 0.3))
        stifle = Vector((hips_x + leg * 0.18, wide, hips_z - (hips_z - sole) * 0.45))
        hock = Vector((hips_x - leg * 0.12, wide, sole + leg * 0.22))
        L["thigh_" + side] = (hip, stifle)
        L["calf_" + side] = (stifle, hock)
        L["foot_" + side] = (hock, Vector((hock.x + leg * 0.14, wide, sole)))
    tail_dir = Vector((-1.0, 0, -0.3)).normalized()
    tail = length * spec["tailShare"]
    for index, name in enumerate(("tail_01", "tail_02", "tail_03")):
        L[name] = (L["pelvis"][0] + tail_dir * tail * index / 3, L["pelvis"][0] + tail_dir * tail * (index + 1) / 3)
    dims = {"height": withers, "full": withers + head, "length": length, "trunk": trunk, "leg": leg, "sole": sole, "base": 0.0,
            "head": head, "six": "sixLegs" in features}
    return L, dims


def body(spec, L, d):
    rng = random.Random(spec["seed"])
    body = Body([name for name, _ in BONES])
    features = set(spec["features"])
    primary, secondary, accent, detail = spec["primary"], spec["secondary"], spec["accent"], spec["detail"]
    trunk, leg = d["trunk"], d["leg"]
    # The trunk: hips, belly and chest; a broad beast's wider than it is tall, built low over its legs.
    p0, p1 = L["pelvis"]
    if "broadBody" in features:
        for bone, start, end, size in (("pelvis", p0 - Vector((trunk * 0.3, 0, 0)), p1, 0.95), ("spine_01", *L["spine_01"], 1.05), ("spine_02", *L["spine_02"], 1.12)):
            centre = start.lerp(end, 0.5)
            along = (end - start) * 0.75
            body.blob(bone, centre, (along, Vector((0, trunk * size * 1.3, 0)), Vector((0, 0, trunk * size * 0.85))), secondary, segments=10)
    else:
        body.limb("pelvis", p0 - Vector((trunk * 0.3, 0, 0)), p1, trunk * 0.85, trunk * 0.95, primary)
        body.limb("spine_01", *L["spine_01"], trunk * 0.95, trunk * 1.0, primary)
        body.limb("spine_02", *L["spine_02"], trunk * 1.0, trunk * 1.05, primary)
    body.limb("neck_01", *L["neck_01"], trunk * 0.7, trunk * 0.55, primary)
    # Legs: thick above, slender below, a broad paw.
    pairs = [LEGS, HIND] + ([MIDDLE] if d["six"] else [])
    for side in ("l", "r"):
        for upper, lower, foot in pairs:
            body.limb(upper + "_" + side, *L[upper + "_" + side], trunk * 0.38, trunk * 0.3, secondary)
            body.limb(lower + "_" + side, *L[lower + "_" + side], trunk * 0.28, trunk * 0.2, secondary)
            f0, f1 = L[foot + "_" + side]
            body.limb(foot + "_" + side, f0, f1, trunk * 0.22, trunk * 0.18, mix(secondary, [0.05, 0.05, 0.05], 0.3))
            if "claws" in features:
                # Pale claws splayed ahead of each foot.
                for claw in (-1, 0, 1):
                    root = f1 + Vector((0, claw * trunk * 0.12, 0))
                    body.limb(foot + "_" + side, root, root + Vector((trunk * 0.22, claw * trunk * 0.05, -trunk * 0.08)), trunk * 0.06, trunk * 0.01,
                              detail, segments=4)
            if "limbCrystals" in features:
                # Crimson crystal set into the plates of the upper limb.
                u0, u1 = L[upper + "_" + side]
                out = Vector((0, trunk * (0.3 if side == "l" else -0.3), 0))
                body.limb(upper + "_" + side, u0.lerp(u1, 0.4) + out, u0.lerp(u1, 0.4) + out * 1.8 + Vector((0, 0, trunk * 0.25)), trunk * 0.07, trunk * 0.01,
                          accent, glow=True, segments=4)
    # The head.
    h0, h1 = L["head"]
    if "wedgeSkull" in features:
        # A heavy wedge-shaped skull of faceted plate, a single deep ocular burning in it.
        body.limb("head", h0, h1, trunk * 0.6, trunk * 0.18, detail, segments=5)
        body.ball("head", h0.lerp(h1, 0.4) + Vector((0, 0, trunk * 0.3)), trunk * 0.14, accent, glow=True)
    else:
        body.limb("head", h0, h1, trunk * 0.5, trunk * 0.3, primary)
        for sign in (1, -1):
            body.ball("head", h0.lerp(h1, 0.45) + Vector((0, sign * trunk * 0.35, trunk * 0.25)), trunk * 0.09, accent, glow="glowEyes" in features)
    if "plates" in features:
        # Pale bone-coloured mineral plating in rounded overlapping layers over the back and flanks, scuffed lighter
        # and darker, like a pangolin's.
        plating = [primary, mix(primary, detail, 0.5), mix(primary, secondary, 0.2), mix(primary, [1.0, 0.97, 0.9], 0.15)]
        for bone, size, count in (("pelvis", 0.95, 8), ("spine_01", 1.05, 9), ("spine_02", 1.12, 9), ("neck_01", 0.7, 6)):
            b0, b1 = L[bone]
            # Lifted a little so the shingles cover the top and flanks, not the belly.
            lift = Vector((0, 0, trunk * 0.25))
            body.shingles(bone, b0 + lift, b1 + lift, trunk * size, plating, rng, count=count, lift=0.25)
    if "crest" in features:
        # A dense crest of crimson crystal spines, faceted and fanned across the back: longest over the shoulders,
        # tapering toward the tail. They are what he fires, so they glow.
        for bone, longest, count in (("spine_02", 1.6, 7), ("spine_01", 1.15, 6), ("pelvis", 0.7, 5)):
            b0, b1 = L[bone]
            for index in range(count):
                across = (index % 3 - 1) * 0.35 + rng.uniform(-0.1, 0.1)
                base = b0.lerp(b1, (index + 0.5) / count) + Vector((0, across * trunk, trunk * 0.9))
                rise = trunk * longest * rng.uniform(0.75, 1.15)
                tilt = Vector((-rise * 0.4, across * rise * 0.5, rise))
                body.limb(bone, base, base + tilt, trunk * 0.16, trunk * 0.02, accent, glow=True, segments=4)
        for index in range(3):
            spot = L["tail_01"][0].lerp(L["tail_03"][1], (index + 0.5) / 3) + Vector((0, 0, trunk * 0.3))
            body.limb("tail_0" + str(index + 1), spot, spot + Vector((-trunk * 0.25, 0, trunk * (0.45 - index * 0.1))), trunk * 0.1, trunk * 0.01, accent,
                      glow=True, segments=4)
    if "chestCrystals" in features:
        n0, n1 = L["neck_01"]
        for sign in (1, -1):
            spot = n0.lerp(n1, 0.2) + Vector((trunk * 0.3, sign * trunk * 0.4, -trunk * 0.1))
            body.limb("neck_01", spot, spot + Vector((trunk * 0.25, sign * trunk * 0.15, trunk * 0.2)), trunk * 0.08, trunk * 0.01, accent, glow=True, segments=4)
    if "ruff" in features:
        # A thick cream-white ruff at the chest.
        n0, n1 = L["neck_01"]
        body.ball("neck_01", n0.lerp(n1, 0.3) + Vector((trunk * 0.15, 0, -trunk * 0.1)), trunk * 0.78, detail, scale=(0.85, 1.05, 1.0))
    if "antlers" in features:
        # Branching antlers grown through with living wood.
        top = h0.lerp(h1, 0.3) + Vector((0, 0, trunk * 0.4))
        for sign in (1, -1):
            beam_end = top + Vector((-trunk * 0.5, sign * trunk * 1.5, trunk * 2.2))
            body.limb("head", top + Vector((0, sign * trunk * 0.25, 0)), beam_end, trunk * 0.16, trunk * 0.06, spec["horn"])
            for branch in range(4):
                start = top.lerp(beam_end, 0.3 + branch * 0.18)
                body.limb("head", start, start + Vector((trunk * 0.55, sign * trunk * 0.3, trunk * 0.6)), trunk * 0.09, trunk * 0.02, spec["horn"])
    if "wildlight" in features:
        # Wildlight markings along the limbs and flanks, leaf-shaped, glowing.
        for bone in ("spine_01", "spine_02", "upperarm_l", "upperarm_r", "thigh_l", "thigh_r"):
            b0, b1 = L[bone]
            side = -1.0 if bone.endswith("_r") else 1.0
            body.ball(bone, b0.lerp(b1, 0.5) + Vector((0, side * trunk * (0.98 if bone.startswith("spine") else 0.36), 0)), trunk * 0.18,
                      accent, glow=True, scale=(1.6, 0.3, 0.7))
    if "moss" in features:
        for bone in ("spine_01", "spine_02"):
            b0, b1 = L[bone]
            body.ball(bone, b0.lerp(b1, 0.5) + Vector((0, 0, trunk * 0.9)), trunk * 0.4, [0.22, 0.40, 0.14], scale=(1.4, 1.0, 0.4))
    # A tail tapering behind.
    for index, name in enumerate(("tail_01", "tail_02", "tail_03")):
        body.limb(name, *L[name], trunk * (0.35 - index * 0.1), trunk * (0.25 - index * 0.08), primary)
    return body


def run_stride(d):
    """How far one Run cycle carries the body: two sets of steps, each planted foot sweeping from one swing to the other."""
    return 2 * 2 * d["leg"] * math.sin(math.radians(RUN_LEG_SWING))


def legs(d):
    """Each leg as (upper, lower bone, phase): diagonal pairs in step on four legs, alternating tripods on six."""
    four = [("upperarm_l", "lowerarm_l", 0.0), ("thigh_r", "calf_r", 0.0), ("upperarm_r", "lowerarm_r", 0.5), ("thigh_l", "calf_l", 0.5)]
    if not d["six"]:
        return four
    return [("upperarm_l", "lowerarm_l", 0.0), ("midleg_upper_r", "midleg_lower_r", 0.0), ("thigh_l", "calf_l", 0.0),
            ("upperarm_r", "lowerarm_r", 0.5), ("midleg_upper_l", "midleg_lower_l", 0.5), ("thigh_r", "calf_r", 0.5)]


def pose(name, t, melee, d):
    """Each bone's rotation (about the armature's axes, in radians) and the hips' lift at t from 0 to 1."""
    pose, lift = {}, 0.0
    tau = math.tau
    if name == "Idle":
        breath = math.sin(t * tau)
        pose["spine_01"] = lean(1.0 * breath)
        pose["neck_01"] = twist(6 * math.sin(t * tau + 1.0))
        pose["head"] = lean(-3 * math.sin(t * tau * 2))
        pose["tail_01"] = twist(10 * math.sin(t * tau))
        lift = d["height"] * 0.01 * breath
    elif name == "Run":
        for upper, lower, phase in legs(d):
            swing = math.sin((t + phase) * tau)
            hind = upper.startswith("thigh")
            pose[upper] = forward_swing(RUN_LEG_SWING * swing)
            # A leg folds as it comes forward off the ground: a foreleg at the wrist, a hind leg at the hock.
            pose[lower] = forward_swing((-35 if hind else 45) * max(0.0, math.cos((t + phase) * tau)))
        pose["spine_02"] = lean(4 * math.sin(t * tau * 2))
        pose["neck_01"] = lean(-6)
        pose["tail_01"] = lean(-15)
        lift = d["height"] * 0.03 * abs(math.sin(t * tau))
    elif name in ("AttackWindup", "AttackStrike"):
        if melee:
            # A lunging bite: it gathers back on its haunches, head raised, then lunges through to the bite as the windup
            # ends; the strike settles back.
            if name == "AttackWindup":
                cock = ease(min(1.0, t / LUNGE_COCK_SHARE))
                lunge = ease(max(0.0, (t - LUNGE_COCK_SHARE) / (1.0 - LUNGE_COCK_SHARE)))
                settle = 0.0
            else:
                cock, lunge, settle = 1.0, 1.0, ease(t)
            hold = 1.0 - settle
            pose["spine_02"] = lean((-10 * cock + 22 * lunge) * hold)
            pose["neck_01"] = lean((-25 * cock + 45 * lunge) * hold)
            pose["head"] = lean((-10 * cock + 15 * lunge) * hold)
            for side in ("l", "r"):
                pose["upperarm_" + side] = forward_swing((-15 * cock + 45 * lunge) * hold)
                pose["thigh_" + side] = forward_swing((20 * cock - 15 * lunge) * hold)
                pose["calf_" + side] = forward_swing(-25 * cock * hold)
            lift = -d["height"] * 0.08 * cock * hold
        else:
            # A volley of spines: it crouches and arches, the crest flaring, and jolts back as they leave it.
            if name == "AttackWindup":
                arch, jolt, settle = ease(t), 0.0, 0.0
            else:
                arch = 1.0
                jolt = math.sin(min(1.0, t / 0.4) * math.pi)
                settle = ease(max(0.0, (t - 0.3) / 0.7))
            hold = 1.0 - settle
            pose["spine_01"] = lean(-12 * arch * hold)
            pose["spine_02"] = lean((-10 * arch * hold) + 8 * jolt)
            pose["neck_01"] = lean(20 * arch * hold - 15 * jolt)
            for side in ("l", "r"):
                pose["thigh_" + side] = forward_swing(15 * arch * hold)
                pose["calf_" + side] = forward_swing(-20 * arch * hold)
            lift = -d["height"] * 0.06 * arch * hold
    elif name == "Cast":
        # Rearing up to the release, forelegs lifted, head high; it holds there while the cast holds, then drops.
        rise = ease(min(1.0, t / CAST_RELEASE_SHARE))
        fall = ease(max(0.0, (t - 0.6) / 0.4))
        k = rise * (1 - fall)
        pose["spine_01"] = lean(-18 * k)
        pose["spine_02"] = lean(-12 * k)
        pose["neck_01"] = lean(-20 * k)
        for side in ("l", "r"):
            pose["upperarm_" + side] = forward_swing(55 * k)
            pose["lowerarm_" + side] = forward_swing(60 * k)
            pose["thigh_" + side] = forward_swing(-10 * k)
    elif name == "Hit":
        k = math.sin(min(1.0, t) * math.pi)
        pose["spine_02"] = twist(10 * k)
        pose["neck_01"] = lean(-12 * k)
        lift = -d["height"] * 0.03 * k
    elif name == "Death":
        # Toppling onto its side, legs gone slack.
        k = ease(min(1.0, t / 0.8))
        pose["root"] = roll_side(85 * k, 1)
        for upper, lower, _ in legs(d):
            pose[upper] = forward_swing(20 * k)
            pose[lower] = forward_swing(-25 * k)
        pose["neck_01"] = lean(15 * k)
        lift = -d["height"] * 0.1 * k
    elif name == "Recall":
        # Lying down, legs folded under it, head lowered and breathing.
        settle = ease(min(1.0, t * 4))
        breath = math.sin(t * math.tau)
        for side in ("l", "r"):
            pose["upperarm_" + side] = forward_swing(-50 * settle)
            pose["lowerarm_" + side] = forward_swing(100 * settle)
            pose["thigh_" + side] = forward_swing(55 * settle)
            pose["calf_" + side] = forward_swing(-100 * settle)
            pose["midleg_upper_" + side] = roll_side(50 * settle, 1 if side == "l" else -1)
        pose["neck_01"] = lean(15 * settle + 2 * breath)
        lift = -(d["height"] - d["trunk"] * 1.1) * settle
    else:
        raise AssertionError("Unknown animation: " + name)
    return pose, lift
