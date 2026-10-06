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
    # A body a BodyScale status wears is fitted to the capsule as grown.
    grown = spec.get("bodyScale", 1.0)
    capsule = dict(capsule, capsuleHalfHeight=capsule["capsuleHalfHeight"] * grown, capsuleRadius=capsule["capsuleRadius"] * grown)
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
    # A body of bark and fur is fur where no timber plates it: its neck, head and tail.
    fur = secondary if "barkBody" in features else primary
    # A body of smoke has no mesh for its trunk, neck, legs or tail: its effect pours them off its bones (the art
    # set's Effect), and only what is solid in it, its mask, eyes and claws, is built.
    smoke = "smokeBody" in features
    # A body of water (a Waterling) has none either: water_body builds it whole, low on the ground; nor has a machine
    # (Picket), which picket_body builds whole.
    bare = smoke or "waterBody" in features or "machineBody" in features
    # The trunk: hips, belly and chest; a broad beast's wider than it is tall, built low over its legs.
    p0, p1 = L["pelvis"]
    if bare:
        pass
    elif "broadBody" in features:
        for bone, start, end, size in (("pelvis", p0 - Vector((trunk * 0.3, 0, 0)), p1, 0.95), ("spine_01", *L["spine_01"], 1.05), ("spine_02", *L["spine_02"], 1.12)):
            centre = start.lerp(end, 0.5)
            along = (end - start) * 0.75
            body.blob(bone, centre, (along, Vector((0, trunk * size * 1.3, 0)), Vector((0, 0, trunk * size * 0.85))), secondary, segments=10)
    elif "barkBody" in features:
        # A predator's body of dark fur, deep through the shoulders, its haunches and shoulders plated with weathered
        # timber the way bark grows.
        for bone, (start, end, radius) in bark_swells(L, trunk).items():
            body.blob(bone, start.lerp(end, 0.5), ((end - start) * 0.8, Vector((0, radius, 0)), Vector((0, 0, radius))), secondary, segments=12)
        # The timber: long pointed plates laid along the grain down each flank of the haunch and the shoulder,
        # overlapping like bark; the ridge of the back stays fur (and moss), and a seam runs between the two.
        timber = [primary, mix(primary, [0.60, 0.58, 0.55], 0.4), mix(primary, [0.25, 0.22, 0.20], 0.3)]
        for bone, at in (("pelvis", 0.5), ("spine_02", 0.7)):
            for sign in (1.0, -1.0):
                for index in range(3):
                    point, normal = on_swell(L, trunk, bone, at, 40 + index * 23 + rng.uniform(-5, 5), sign)
                    grain = Vector((1, 0, rng.uniform(-0.2, 0.2))).normalized()
                    across = normal.cross(grain).normalized()
                    body.blob(bone, point + Vector((rng.uniform(-0.25, 0.25) * trunk, 0, 0)) - normal * trunk * 0.03,
                              (normal * trunk * 0.06, across * trunk * rng.uniform(0.34, 0.42), grain * trunk * rng.uniform(0.5, 0.65)),
                              timber[rng.randrange(len(timber))], segments=4)
    else:
        body.limb("pelvis", p0 - Vector((trunk * 0.3, 0, 0)), p1, trunk * 0.85, trunk * 0.95, primary)
        body.limb("spine_01", *L["spine_01"], trunk * 0.95, trunk * 1.0, primary)
        body.limb("spine_02", *L["spine_02"], trunk * 1.0, trunk * 1.05, primary)
    if not bare:
        body.limb("neck_01", *L["neck_01"], trunk * 0.7, trunk * 0.55, fur)
    # Legs: thick above, slender below, a broad paw.
    pairs = [LEGS, HIND] + ([MIDDLE] if d["six"] else [])
    for side in ("l", "r"):
        for upper, lower, foot in pairs:
            f0, f1 = L[foot + "_" + side]
            if not bare:
                # A heavy predator's legs are thick with muscle above.
                heft = spec.get("legScale", 1.0)
                body.limb(upper + "_" + side, *L[upper + "_" + side], trunk * 0.38 * heft, trunk * 0.3 * heft, secondary)
                body.limb(lower + "_" + side, *L[lower + "_" + side], trunk * 0.28 * heft, trunk * 0.2, secondary)
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
    if "skullMask" in features:
        # A dark smoke head behind a bone-white skull mask, the only pale thing it carries, violet eyes burning behind
        # the mask's sockets; a horned true form's mask grows horns. Of smoke, the head is the effect's too.
        if not smoke:
            body.limb("head", h0, h1, trunk * 0.5, trunk * 0.32, primary, segments=8)
        bone = [0.90, 0.86, 0.78]
        # Big enough to read from the camera: the mask is what tells the pair apart from everything else dark.
        mask = h0.lerp(h1, 0.55) + Vector((trunk * 0.15, 0, trunk * 0.18))
        body.blob("head", mask, (Vector((trunk * 0.45, 0, 0)), Vector((0, trunk * 0.58, 0)), Vector((0, 0, trunk * 0.5))), bone, segments=8)
        body.limb("head", mask, h1 + Vector((trunk * 0.2, 0, -trunk * 0.05)), trunk * 0.45, trunk * 0.2, bone, segments=6)
        for sign in (1, -1):
            body.ball("head", mask + Vector((trunk * 0.4, sign * trunk * 0.24, trunk * 0.1)), trunk * 0.12, accent, glow=True)
            if "horns" in features:
                root = mask + Vector((-trunk * 0.05, sign * trunk * 0.42, trunk * 0.35))
                body.limb("head", root, root + Vector((-trunk * 0.5, sign * trunk * 0.35, trunk * 0.75)), trunk * 0.11, trunk * 0.01, bone, segments=5)
    elif "waterBody" in features:
        water_body(body, spec, L, d)
    elif "machineBody" in features:
        picket_body(body, spec, L, d)
    elif "wedgeSkull" in features:
        # A heavy wedge-shaped skull of faceted plate, a single deep ocular burning in it.
        body.limb("head", h0, h1, trunk * 0.6, trunk * 0.18, detail, segments=5)
        body.ball("head", h0.lerp(h1, 0.4) + Vector((0, 0, trunk * 0.3)), trunk * 0.14, accent, glow=True)
    else:
        body.limb("head", h0, h1, trunk * 0.5, trunk * 0.3, fur)
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
        # A thick cream-white ruff: a deep chest of fur, and tufts standing out and back round the base of the neck.
        n0, n1 = L["neck_01"]
        body.ball("neck_01", n0.lerp(n1, 0.3) + Vector((trunk * 0.15, 0, -trunk * 0.15)), trunk * 0.68, detail, scale=(0.85, 1.05, 1.0))
        creams = [detail, mix(detail, [1.0, 1.0, 1.0], 0.25), mix(detail, secondary, 0.15)]
        for index in range(11):
            around = (index + 0.5) / 11 * math.tau
            out = Vector((0, math.sin(around), math.cos(around)))
            point = (out + Vector((-0.6, 0, 0))).normalized()
            across = out.cross(Vector((1, 0, 0))).normalized()
            body.blob("neck_01", n0.lerp(n1, 0.4) + out * trunk * 0.65 + point * trunk * 0.2,
                      (out.cross(across) * trunk * 0.08, across * trunk * 0.17, point * trunk * rng.uniform(0.38, 0.5)), creams[index % 3], segments=4)
    if "antlers" in features:
        # Branching antlers grown through with living wood: each beam sweeps up, back and out in three lengths, a brow
        # tine forward off the first, a tine out and one forward off the second, and the tip forked; leaves grow on some
        # tines and sprigs along the beam.
        top = h0.lerp(h1, 0.3) + Vector((0, 0, trunk * 0.4))
        # Surging, the Wildlight reaches the living wood: its leaves light up, where the camera above sees them first.
        surging = "wildlightSurge" in features
        leaf = mix(accent, [1.0, 1.0, 1.0], 0.3) if surging else [0.24, 0.46, 0.16]
        for sign in (1, -1):
            beam = [top + Vector((0, sign * trunk * 0.25, 0))]
            # Wide rather than tall: the camera above sees an antler's spread, not its height.
            for step in ((-0.2, 0.75, 0.62), (-0.35, 0.6, 0.48), (-0.25, 0.32, 0.38)):
                beam.append(beam[-1] + Vector((trunk * step[0], sign * trunk * step[1], trunk * step[2])))
            for index, (a, b) in enumerate(zip(beam, beam[1:])):
                body.limb("head", a, b, trunk * (0.2 - index * 0.05), trunk * (0.15 - index * 0.05), spec["horn"], segments=6)
            tines = [(beam[1], (0.55, 0.15, 0.35)), (beam[2], (0.1, 0.55, 0.4)), (beam[2], (0.45, 0.0, 0.45)),
                     (beam[3], (0.25, 0.25, 0.4)), (beam[3], (-0.3, 0.1, 0.4))]
            for index, (root, (x, y, z)) in enumerate(tines):
                tip = root + Vector((trunk * x, sign * trunk * y, trunk * z))
                body.limb("head", root, tip, trunk * 0.1, trunk * 0.03, spec["horn"], segments=5)
                if index in (1, 3):
                    body.blob("head", tip, (Vector((trunk * 0.16, 0, 0)), Vector((0, trunk * 0.12, 0)), Vector((0, 0, trunk * 0.1))), leaf, glow=surging, segments=6)
            sprig = beam[1].lerp(beam[2], 0.5)
            body.blob("head", sprig + Vector((0, sign * trunk * 0.08, 0)), (Vector((trunk * 0.12, 0, 0)), Vector((0, trunk * 0.09, 0)), Vector((0, 0, trunk * 0.07))),
                      leaf if surging else mix(leaf, [0.1, 0.2, 0.05], 0.3), glow=surging, segments=6)
    if "wildlight" in features:
        # Wildlight growing along limb and grain, never laid out like glyphwork: pointed leaf panels down each flank,
        # eye-shaped panels at shoulder and haunch, and veins running down every leg. Surging (in its own ground), the
        # panels swell and pale toward white and veins run along the neck and spine too.
        surge = "wildlightSurge" in features
        grow = 1.35 if surge else 1.0
        light = mix(accent, [1.0, 1.0, 1.0], 0.3) if surge else accent
        heft = spec.get("legScale", 1.0)
        vein = trunk * (0.055 if surge else 0.035)
        for side, sign in (("l", 1.0), ("r", -1.0)):
            # A vein down each side of the back, in the seam above the timber, from the shoulders to the haunch, where
            # the camera above sees it; it sends branches down the flank between the plates, as a leaf's veins do.
            path = [on_swell(L, trunk, bone, at, angle, sign, out=1.03)[0] for bone, at, angle in
                    (("spine_02", 0.9, 30), ("spine_02", 0.5, 26), ("spine_01", 0.5, 24), ("pelvis", 0.5, 26), ("pelvis", 0.0, 32))]
            for bone, a, b in zip(("spine_02", "spine_01", "pelvis", "pelvis"), path, path[1:]):
                body.limb(bone, a, b, vein, vein, light, glow=True, segments=5)
            for bone, at in (("spine_02", 0.5), ("spine_01", 0.5), ("pelvis", 0.5)) + ((("spine_02", 0.2), ("pelvis", 0.15)) if surge else ()):
                start, _ = on_swell(L, trunk, bone, at, 26, sign, out=1.03)
                end, _ = on_swell(L, trunk, bone, at - 0.25, 52, sign, out=1.06)
                body.limb(bone, start, end, vein * 0.8, vein * 0.4, light, glow=True, segments=5)
            # Leaves on the flank, each pointed at both ends and laid along the grain: high on the bare middle, low under
            # the timber of the shoulder and the haunch.
            for bone, at, angle, length, tilt in (("spine_02", 0.35, 104, 0.45, -0.25), ("spine_01", 0.5, 70, 0.5, 0.1), ("pelvis", 0.6, 104, 0.4, 0.25)):
                point, normal = on_swell(L, trunk, bone, at, angle, sign, out=1.02)
                grain = Vector((1, 0, tilt)).normalized()
                body.blob(bone, point, (normal * trunk * 0.05, normal.cross(grain).normalized() * trunk * 0.16 * grow, grain * trunk * length * grow),
                          light, glow=True, segments=4)
            # An eye at each shoulder and haunch: a pointed oval panel round a pale heart.
            for upper, lower in ((LEGS[0], LEGS[1]), (HIND[0], HIND[1])):
                u0, u1 = L[upper + "_" + side]
                out = Vector((0, sign * trunk * 0.4 * heft, 0))
                eye = u0.lerp(u1, 0.15) + out
                body.blob(upper + "_" + side, eye, (Vector((0, sign * trunk * 0.04, 0)), Vector((0, 0, trunk * 0.13 * grow)), Vector((trunk * 0.3 * grow, 0, 0))),
                          light, glow=True, segments=4)
                body.ball(upper + "_" + side, eye + Vector((0, sign * trunk * 0.04, 0)), trunk * 0.06 * grow, mix(light, [1.0, 1.0, 1.0], 0.6), glow=True, segments=6)
                for bone in (upper, lower):
                    a, b = L[bone + "_" + side]
                    reach = Vector((0, sign * trunk * (0.36 if bone == upper else 0.27) * heft, 0))
                    body.limb(bone + "_" + side, a.lerp(b, 0.25) + reach, a.lerp(b, 0.85) + reach, trunk * 0.04 * grow, trunk * 0.025 * grow, light, glow=True, segments=5)
            if surge:
                # Surging, the veins run on up the neck toward the antlers.
                n0, n1 = L["neck_01"]
                lift = Vector((0, sign * trunk * 0.35, trunk * 0.5))
                body.limb("neck_01", n0 + lift * 1.2, n1 + lift * 0.7, vein, vein * 0.6, light, glow=True, segments=5)
    if "moss" in features:
        # Moss and small growing things on its back, as though the forest has begun using it for ground.
        for bone in ("spine_01", "spine_02"):
            b0, b1 = L[bone]
            # On the ridge of the back, however deep the trunk is there.
            top = bark_swells(L, trunk)[bone][2] if "barkBody" in features else trunk
            # In clumps, not a blanket: a few low cushions of differing greens along the ridge.
            for index in range(3):
                spot = b0.lerp(b1, 0.2 + index * 0.3 + rng.uniform(-0.05, 0.05)) + Vector((0, rng.uniform(-0.25, 0.25) * trunk, top * 0.95))
                body.ball(bone, spot, trunk * rng.uniform(0.16, 0.24), mix([0.22, 0.40, 0.14], [0.35, 0.45, 0.15], rng.uniform(0, 1)), scale=(1.3, 1.0, 0.45), segments=8)
            for index in range(3):
                root = b0.lerp(b1, 0.25 + index * 0.25) + Vector((0, (index - 1) * trunk * 0.25, top * 0.98))
                tip = root + Vector((rng.uniform(-0.1, 0.1) * trunk, rng.uniform(-0.1, 0.1) * trunk, trunk * rng.uniform(0.3, 0.5)))
                body.limb(bone, root, tip, trunk * 0.03, trunk * 0.015, [0.30, 0.42, 0.18], segments=4)
                body.blob(bone, tip, (Vector((trunk * 0.09, 0, 0)), Vector((0, trunk * 0.07, 0)), Vector((0, 0, trunk * 0.05))), [0.26, 0.50, 0.18], segments=6)
    # A tail tapering behind.
    for index, name in enumerate(("tail_01", "tail_02", "tail_03")):
        if not bare:
            body.limb(name, *L[name], trunk * (0.35 - index * 0.1), trunk * (0.25 - index * 0.08), fur)
    return body


def bark_swells(L, trunk):
    """A bark-and-fur trunk's three swells by bone, as (start, end, radius): the haunch, the belly and the deep chest,
    each round in section."""
    p0, p1 = L["pelvis"]
    return {"pelvis": (p0 - Vector((trunk * 0.35, 0, 0)), p1, trunk * 1.05), "spine_01": (*L["spine_01"], trunk * 0.95 * 1.05),
            "spine_02": (*L["spine_02"], trunk * 1.15 * 1.05)}


def on_swell(L, trunk, bone, at, degrees, sign, out=1.0):
    """A point on a bark-and-fur trunk's swell (at along it, degrees round from the top of the back toward the side
    sign gives), out times its radius from its spine, and the way out there. A swell narrows toward its ends, being an
    ellipsoid reaching 0.8 of its length either way of its middle."""
    start, end, radius = bark_swells(L, trunk)[bone]
    angle = math.radians(degrees)
    normal = Vector((0, sign * math.sin(angle), math.cos(angle)))
    narrowing = math.sqrt(max(0.0, 1.0 - ((at - 0.5) / 0.8) ** 2))
    return start.lerp(end, at) + normal * radius * narrowing * out, normal


def water_body(body, spec, L, d):
    """A little living wave (a Waterling): a low teardrop of water resting on the ground along its spine, its back rising
    into a foam-tipped crest that curls forward over a small rounded head with two points of light, and a wake tapering
    behind along its tail. Primary is its water, secondary the deeper water under it, detail its foam."""
    trunk, tall, long = d["trunk"], d["height"], d["length"]
    water, deep, foam, light = spec["primary"], spec["secondary"], spec["detail"], spec["accent"]
    hips, chest = L["pelvis"][0], L["spine_02"][1]
    # One swell resting on the ground from the hips to the chest, its back half on the hips and its front on the chest,
    # so it heaves as the body moves: lower and deeper-coloured behind, tallest at the front.
    body.blob("pelvis", Vector((hips.x + long * 0.12, 0, tall * 0.35)), (Vector((long * 0.3, 0, 0)), Vector((0, trunk * 1.15, 0)), Vector((0, 0, tall * 0.35))),
              mix(water, deep, 0.35), segments=12)
    body.blob("spine_02", Vector((chest.x - long * 0.1, 0, tall * 0.45)), (Vector((long * 0.32, 0, 0)), Vector((0, trunk * 1.25, 0)), Vector((0, 0, tall * 0.45))),
              water, segments=12)
    # The head, the front of the swell, with its two lights looking ahead.
    front = Vector((chest.x + long * 0.14, 0, tall * 0.35))
    body.blob("head", front, (Vector((long * 0.18, 0, 0)), Vector((0, trunk * 0.95, 0)), Vector((0, 0, tall * 0.35))), mix(water, foam, 0.08), segments=10)
    for sign in (1, -1):
        body.ball("head", front + Vector((long * 0.15, sign * trunk * 0.4, tall * 0.18)), trunk * 0.14, light, glow=True, segments=8)
    # The crest: the back rises into it and it curls forward over the head, foam rolling along its lip.
    crest = Vector((chest.x - long * 0.1, 0, tall * 0.95))
    body.blob("neck_01", crest, (Vector((long * 0.12, 0, tall * 0.32)), Vector((0, trunk * 1.0, 0)), Vector((long * 0.1, 0, -tall * 0.04))),
              mix(water, foam, 0.3), segments=10)
    for index in range(5):
        across = (index / 4 - 0.5) * trunk * 1.5
        body.ball("neck_01", crest + Vector((long * 0.16, across, tall * 0.3 - abs(across) * 0.2)), trunk * 0.3, foam, segments=8)
    # The wake: flatter and narrower toward its end.
    for index, name in enumerate(("tail_01", "tail_02", "tail_03")):
        t0, t1 = L[name]
        centre = t0.lerp(t1, 0.5)
        rise = tall * (0.22 - index * 0.06)
        body.blob(name, Vector((centre.x, 0, rise)), (Vector(((t1 - t0).length * 0.75, 0, 0)), Vector((0, trunk * (1.0 - index * 0.25), 0)), Vector((0, 0, rise))),
                  mix(water, foam, 0.12 * index), segments=10)


def picket_body(body, spec, L, d):
    """A four-legged work machine (Picket), equipment and never a creature: a riveted armoured chassis of brass-yellow
    plate over black iron, a red gear stencilled on each flank and hazard stripes along its front, on four heavy legs,
    each with a piston that telescopes as it bends, and broad foot pads; a turret on its front carrying a heavy rivet
    cannon, a lens glowing in its muzzle and an ammunition drum at its side; and a wide directional steel shield. As a
    gun platform the shield is folded flat across its back and the cannon trained ahead. Braced as a bulwark (its body's
    bulwark entry), the shield stands raised across its front in three angled panels with a sight slit, and the
    cannon is drawn back and down behind it. Primary is its plate, secondary its iron, detail its steel, accent the
    lens."""
    plate, iron, steel, lens = spec["primary"], spec["secondary"], spec["detail"], spec["accent"]
    stencil, rivet = spec.get("stencil", [0.55, 0.12, 0.08]), mix(spec["primary"], [0.2, 0.2, 0.2], 0.4)
    trunk, long, tall = d["trunk"], d["length"], d["height"]
    bulwark = spec.get("bulwark", False)
    # The chassis: an iron frame in three sections down the spine, plated on its flanks and top, rivets along the top.
    p0, p1 = L["pelvis"]
    for bone, start, end in (("pelvis", p0 - Vector((trunk * 0.3, 0, 0)), p1), ("spine_01", *L["spine_01"]), ("spine_02", *L["spine_02"])):
        centre, span = (start + end) / 2, (end - start).length * 1.15
        body.box(bone, centre, (span, trunk * 1.6, trunk * 1.2), iron)
        body.box(bone, centre + Vector((0, 0, trunk * 0.62)), (span * 0.95, trunk * 1.5, trunk * 0.06), plate)
        for sign in (1, -1):
            body.box(bone, centre + Vector((0, sign * trunk * 0.82, trunk * 0.05)), (span * 0.9, trunk * 0.06, trunk * 0.9), plate)
            for index in range(3):
                body.ball(bone, centre + Vector((span * (index - 1) * 0.35, sign * trunk * 0.7, trunk * 0.66)), trunk * 0.04, rivet, segments=5)
    # A red gear stencilled on each flank, and hazard stripes along the front edge.
    middle = L["spine_01"][0].lerp(L["spine_01"][1], 0.5)
    for sign in (1, -1):
        hub = middle + Vector((0, sign * trunk * 0.86, trunk * 0.05))
        out = Vector((0, sign, 0))
        body.limb("spine_01", hub, hub + out * trunk * 0.02, trunk * 0.28, trunk * 0.28, stencil, segments=10)
        body.limb("spine_01", hub, hub + out * trunk * 0.03, trunk * 0.1, trunk * 0.1, plate, segments=8)
        for tooth in range(8):
            angle = tooth / 8 * math.tau
            body.box("spine_01", hub + Vector((math.cos(angle) * trunk * 0.32, 0, math.sin(angle) * trunk * 0.32)), (trunk * 0.1, trunk * 0.03, trunk * 0.1), stencil,
                     rotation=Euler((0, -angle, 0)))
    front = L["spine_02"][1] + Vector((trunk * 0.25, 0, trunk * 0.45))
    for stripe in range(6):
        body.box("spine_02", front + Vector((0, (stripe - 2.5) * trunk * 0.26, 0)), (trunk * 0.05, trunk * 0.26, trunk * 0.18), [0.9, 0.75, 0.15] if stripe % 2 else iron)
    # Four heavy legs: a plated upper, a piston cylinder on it whose rod rides the lower so it telescopes as the leg
    # bends, a steel lower and a broad iron foot pad.
    for side, sign in (("l", 1), ("r", -1)):
        for upper, lower, foot in (LEGS, HIND):
            u0, u1 = L[upper + "_" + side]
            l0, l1 = L[lower + "_" + side]
            f0, f1 = L[foot + "_" + side]
            out = Vector((0, sign * trunk * 0.3, 0))
            body.ball(upper + "_" + side, u0, trunk * 0.32, iron, segments=8)
            body.slab(upper + "_" + side, u0, u1, trunk * 0.45, trunk * 0.35, plate)
            body.ball(upper + "_" + side, u1, trunk * 0.22, iron, segments=8)
            body.limb(upper + "_" + side, u0.lerp(u1, 0.25) + out, u1 + out, trunk * 0.1, trunk * 0.1, steel, segments=8)
            body.limb(lower + "_" + side, l0 + out - (l1 - l0) * 0.1, l0.lerp(l1, 0.6) + out, trunk * 0.05, trunk * 0.05, [0.85, 0.85, 0.82], segments=6)
            body.limb(lower + "_" + side, l0, l1, trunk * 0.17, trunk * 0.13, steel, segments=8)
            body.box(foot + "_" + side, (f0 + f1) / 2, ((f1 - f0).length * 1.4, trunk * 0.55, trunk * 0.16), iron)
    # The turret and its rivet cannon: trained ahead as a gun platform, drawn back and down behind a raised shield.
    n0, n1 = L["neck_01"]
    h0, h1 = L["head"]
    body.limb("neck_01", n0 + Vector((0, 0, trunk * 0.5)), n1, trunk * 0.55, trunk * 0.5, iron, segments=10)
    body.limb("neck_01", n1 - Vector((0, 0, trunk * 0.08)), n1, trunk * 0.6, trunk * 0.6, plate, segments=10)
    aim = (h1 - h0).normalized()
    if bulwark:
        aim = (aim + Vector((0, 0, -1.2))).normalized()
    reach = long * (0.12 if bulwark else 0.45)
    body.box("head", h0 + aim * trunk * 0.2, (trunk * 1.0, trunk * 0.8, trunk * 0.65), plate, rotation=Vector((1, 0, 0)).rotation_difference(aim).to_euler())
    muzzle = h0 + aim * (trunk * 0.5 + reach)
    body.limb("head", h0 + aim * trunk * 0.5, muzzle, trunk * 0.2, trunk * 0.18, steel, segments=10)
    body.limb("head", muzzle - aim * trunk * 0.15, muzzle + aim * trunk * 0.05, trunk * 0.27, trunk * 0.27, iron, segments=10)
    body.ball("head", muzzle + aim * trunk * 0.03, trunk * 0.17, lens, glow=True, segments=10)
    drum = h0 + aim * trunk * 0.1 + Vector((0, trunk * 0.5, -trunk * 0.05))
    body.limb("head", drum, drum + Vector((0, trunk * 0.3, 0)), trunk * 0.32, trunk * 0.32, mix(plate, [0.6, 0.4, 0.1], 0.3), segments=10)
    # The shield: folded flat across the back, or raised across the front in three angled panels with a sight slit.
    if bulwark:
        base = L["spine_02"][1] + Vector((trunk * 1.6, 0, -tall * 0.55))
        height = tall * 1.3
        for panel, turn in ((-1, 25), (0, 0), (1, -25)):
            angle = math.radians(turn)
            centre = base + Vector((-abs(panel) * trunk * 0.35, panel * trunk * 1.35, height * 0.5))
            body.box("spine_02", centre, (trunk * 0.1, trunk * 1.4, height), steel, rotation=Euler((0, 0, angle)))
            body.box("spine_02", centre + Vector((trunk * 0.06, 0, height * 0.46)), (trunk * 0.06, trunk * 1.4, height * 0.08), [0.9, 0.75, 0.15], rotation=Euler((0, 0, angle)))
            for row in (-0.35, 0.0, 0.35):
                body.ball("spine_02", centre + Vector((trunk * 0.07, 0, height * row)), trunk * 0.05, rivet, segments=5)
        body.box("spine_02", base + Vector((trunk * 0.06, 0, height * 0.72)), (trunk * 0.06, trunk * 0.7, trunk * 0.1), [0.05, 0.05, 0.05])
    else:
        top = L["spine_01"][0] + Vector((0, 0, trunk * 0.72))
        body.box("spine_01", top, (long * 0.5, trunk * 2.4, trunk * 0.1), steel)
        body.box("spine_01", top + Vector((long * 0.24, 0, trunk * 0.03)), (trunk * 0.1, trunk * 2.4, trunk * 0.1), [0.9, 0.75, 0.15])


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
