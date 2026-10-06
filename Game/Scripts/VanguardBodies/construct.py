"""The construct archetype (ADR-064 §1): Torr, Oriel and Aurelisse: bodies of parts held about a core, floating.

The skeleton keeps the humanoid's upper body (its core is the pelvis bone, so every archetype lifts and finds its middle
alike) and replaces the legs: six orbit bones hold parts circling the core, and a trailing chain hangs beneath it, a
column of plates, a point of shards or a turning cyclone. A halo rides over or behind the head. Each body is built in its
own material: rune-carved stone plates, stained glass in a leaded frame, or air, dust and streaming cloth. Its animations
are the humanoid's for its arms, with the floating parts' own motion: a bob, circling, sweeping back as it moves,
gathering to strike, scattering as it falls.
"""
import math
import random

from mathutils import Euler, Vector

from . import humanoid
from .parts import Body, combine, ease, forward_swing, lean, mix, roll_side, twist

ARM = ("clavicle", "upperarm", "lowerarm", "hand")
ORBITS = ["orbit_0" + str(index) for index in range(1, 7)]
TRAIL = ("trail_01", "trail_02", "trail_03")
BONES = [("root", None), ("pelvis", "root"), ("spine_03", "pelvis"), ("neck_01", "spine_03"), ("head", "neck_01"), ("halo", "spine_03")]
for _side in ("l", "r"):
    BONES += [(ARM[0] + "_" + _side, "spine_03"), (ARM[1] + "_" + _side, ARM[0] + "_" + _side), (ARM[2] + "_" + _side, ARM[1] + "_" + _side),
              (ARM[3] + "_" + _side, ARM[2] + "_" + _side)]
BONES += [(name, "pelvis") for name in ORBITS]
BONES += [(TRAIL[0], "pelvis"), (TRAIL[1], TRAIL[0]), (TRAIL[2], TRAIL[1])]
UPPER_BODY_BONE = "spine_03"
LIFT_BONE = "pelvis"
GROUNDED = False
CAST_RELEASE_SHARE = humanoid.CAST_RELEASE_SHARE
# How far a moving construct's trailing parts sweep back, in degrees.
TRAIL_SWEEP = 25.0
# A construct glides: the distance one Run cycle carries it, in its heights, by which its float keeps pace.
GLIDE_STRIDES = 1.2


def layout(spec, capsule):
    """Every bone's head and tail in centimetres, the body facing +X with its left at +Y, fitted to the capsule (grown
    by bodyScale for a body a BodyScale status wears)."""
    grown = spec.get("bodyScale", 1.0)
    capsule = dict(capsule, capsuleHalfHeight=capsule["capsuleHalfHeight"] * grown, capsuleRadius=capsule["capsuleRadius"] * grown)
    full = capsule["capsuleHalfHeight"] * 2.0 * spec["heightShare"]
    build = humanoid.BUILD[spec["build"]]
    hover = full * spec["hoverShare"]
    head = full * spec["headShare"]
    core_z = hover + (full - hover) * spec["coreShare"]
    chest_z = full - head - full * 0.04
    shoulder = max(capsule["capsuleRadius"] * 0.85, full * 0.14) * build
    arm = full * 0.38
    L = {}
    L["root"] = (Vector((0, 0, 0)), Vector((0, 0, hover * 0.5 + 1.0)))
    L["pelvis"] = (Vector((0, 0, core_z)), Vector((0, 0, (core_z + chest_z) / 2)))
    L["spine_03"] = (L["pelvis"][1], Vector((0, 0, chest_z)))
    L["neck_01"] = (Vector((0, 0, chest_z)), Vector((0, 0, chest_z + full * 0.04)))
    L["head"] = (L["neck_01"][1], L["neck_01"][1] + Vector((0, 0, head)))
    # The halo: over the shoulders, or behind the head.
    halo_center = Vector((-shoulder * 0.3, 0, chest_z + head * (0.5 if spec["haloBehind"] else 1.25)))
    L["halo"] = (halo_center, halo_center + Vector((0, 0, head * 0.3)))
    for side, sign in (("l", 1.0), ("r", -1.0)):
        shoulder_point = Vector((0, sign * shoulder, chest_z - head * 0.2))
        L["clavicle_" + side] = (Vector((0, sign * shoulder * 0.2, chest_z - head * 0.2)), shoulder_point)
        down = Vector((0.0, sign * math.sin(math.radians(18)), -math.cos(math.radians(18))))
        elbow = shoulder_point + down * arm * 0.48
        wrist = elbow + down * arm * 0.40
        L["upperarm_" + side] = (shoulder_point, elbow)
        L["lowerarm_" + side] = (elbow, wrist)
        L["hand_" + side] = (wrist, wrist + down * arm * 0.12)
    # Orbits: each bone reaches from the core to where its part circles; fortified, they stand in a wall before it.
    for index, name in enumerate(ORBITS):
        reach = shoulder * spec["orbitShare"]
        if spec.get("fortified"):
            across = (index % 3 - 1) * shoulder * 0.9
            rise = (index // 3) * (chest_z - core_z) * 0.9
            L[name] = (L["pelvis"][0], L["pelvis"][0] + Vector((reach * 0.8, across, rise)))
            continue
        angle = index / len(ORBITS) * math.tau
        rise = (index % 3 - 1) * (chest_z - core_z) * 0.4
        L[name] = (L["pelvis"][0], L["pelvis"][0] + Vector((math.cos(angle) * reach, math.sin(angle) * reach, rise)))
    # The trail: down from the core toward the ground, a little behind.
    bottom = Vector((-shoulder * 0.15, 0, hover))
    for index, name in enumerate(TRAIL):
        L[name] = (L["pelvis"][0].lerp(bottom, index / 3), L["pelvis"][0].lerp(bottom, (index + 1) / 3))
    dims = {"height": full, "full": full, "base": 0.0, "leg": core_z - hover, "head": head, "shoulder": shoulder, "arm": arm,
            "hover": hover, "core": core_z}
    return L, dims


def body(spec, L, d):
    rng = random.Random(spec["seed"])
    body = Body([name for name, _ in BONES])
    style = spec["style"]
    {"runePlates": rune_plates, "stainedGlass": stained_glass, "air": air}[style](body, spec, L, d, rng)
    return body


def stone_plate(body, bone, centre, normal, size, stone, moss, flux, rng, rune=True):
    """One rounded, rune-carved stone plate facing out along normal: sandstone shading a little lighter or darker,
    moss on it if it faces up, and a rune on its face lit by the Flux behind it."""
    normal = normal.normalized()
    side = normal.cross(Vector((0, 0, 1)) if abs(normal.z) < 0.9 else Vector((1, 0, 0))).normalized()
    up = side.cross(normal).normalized()
    shade = mix(stone, [0.25, 0.22, 0.20] if rng.random() < 0.5 else [1.0, 0.95, 0.85], rng.uniform(0.0, 0.25))
    body.blob(bone, centre, (normal * size * 0.45, side * size * rng.uniform(0.85, 1.15), up * size * rng.uniform(0.8, 1.1)), shade, segments=8)
    if up.z > 0.3 or normal.z > 0.5:
        body.blob(bone, centre + (normal * 0.2 + up * 0.55) * size, (normal * size * 0.2, side * size * 0.7, up * size * 0.25), moss, segments=6)
    if rune:
        mark = centre + normal * size * 0.46
        # A carved stroke across its face, at an angle of its own.
        body.box(bone, mark, (size * 0.06, size * 0.45, size * 0.07), flux, glow=True,
                 rotation=Euler((rng.uniform(-0.8, 0.8), 0, math.atan2(normal.y, normal.x))))


def rune_plates(body, spec, L, d, rng):
    """Torr: rune-carved stone plates held around a blue-violet crystalline core by Flux light, a broken stone ring over
    the shoulders, the runes lit from inside; the gaps between the plates are the design. A fortified body gathers its
    circling plates into a wall before it; an overcapacity body's core flares and its plates spread."""
    stone, moss, flux = spec["primary"], spec["secondary"], spec["accent"]
    shoulder = d["shoulder"]
    flare = spec.get("coreFlare", 1.0)
    core = L["pelvis"][1]
    chest = L["spine_03"][1]
    # The core, deep in the chest, and the web of Flux it throws to every plate.
    body.ball("spine_03", core + Vector((shoulder * 0.1, 0, 0)), shoulder * 0.42 * flare, flux, glow=True, segments=10)
    body.ball("spine_03", core + Vector((shoulder * 0.1, 0, 0)), shoulder * 0.55 * flare, mix(flux, [0.15, 0.10, 0.35], 0.6), scale=(0.9, 0.9, 1.1), segments=10)
    # The trunk: two tiers of plates standing clear of the core on every side, wider at the chest.
    for tier, (height, reach, count) in enumerate(((0.0, 0.85, 7), (0.55, 1.05, 8))):
        for index in range(count):
            angle = (index + tier * 0.5) / count * math.tau
            out = Vector((math.cos(angle), math.sin(angle), 0.15 + tier * 0.25))
            spot = core.lerp(chest, height) + Vector(out.xy.to_3d()) * shoulder * reach + Vector((0, 0, rng.uniform(-0.1, 0.2) * shoulder))
            stone_plate(body, "spine_03", spot, out, shoulder * 0.42, stone, moss, flux, rng)
            body.limb("spine_03", core, spot - out.normalized() * shoulder * 0.2, shoulder * 0.035 * flare, shoulder * 0.02, flux, glow=True, segments=4)
    # The head: a cluster of plates over a single lit slit.
    h0, h1 = L["head"]
    centre = (h0 + h1) / 2
    for index in range(4):
        angle = index / 4 * math.tau + 0.4
        out = Vector((math.cos(angle) * 0.6, math.sin(angle) * 0.8, 0.6))
        stone_plate(body, "head", centre + out * d["head"] * 0.4, out, d["head"] * 0.55, stone, moss, flux, rng, rune=False)
    body.box("head", centre + Vector((d["head"] * 0.5, 0, 0)), (d["head"] * 0.08, d["head"] * 0.6, d["head"] * 0.12), flux, glow=True)
    # Huge shoulders, arms of separate plates held clear of each other by Flux, and great fists of clustered stone.
    for side, sign in (("l", 1), ("r", -1)):
        s0, s1 = L["clavicle_" + side]
        for index in range(3):
            out = Vector((rng.uniform(-0.4, 0.4), sign * 0.6, 0.8))
            stone_plate(body, "clavicle_" + side, s1 + out * shoulder * 0.35, out, shoulder * 0.5, stone, moss, flux, rng)
        for part, size in (("upperarm", 0.4), ("lowerarm", 0.45)):
            b0, b1 = L[part + "_" + side]
            for index in range(2):
                spot = b0.lerp(b1, 0.3 + index * 0.45)
                out = Vector((0.5, sign * 0.8, 0.3 if index else -0.2))
                stone_plate(body, part + "_" + side, spot, out, shoulder * size, stone, moss, flux, rng)
            body.ball(part + "_" + side, b0, shoulder * 0.12 * flare, flux, glow=True, segments=6)
        h0, h1 = L["hand_" + side]
        fist = h0.lerp(h1, 0.6)
        for index in range(5):
            angle = index / 5 * math.tau
            out = Vector((0.5 + math.cos(angle) * 0.5, math.sin(angle) * 0.7, -0.3 + math.sin(angle) * 0.4))
            stone_plate(body, "hand_" + side, fist + out * shoulder * 0.3, out, shoulder * 0.42, stone, moss, flux, rng, rune=index % 2 == 0)
        body.ball("hand_" + side, fist, shoulder * 0.18 * flare, flux, glow=True, segments=6)
    # The circling plates: around him, or, fortified, gathered into a wall before him.
    for index, name in enumerate(ORBITS):
        o0, o1 = L[name]
        out = (o1 - o0).normalized()
        stone_plate(body, name, o1, out, shoulder * (0.75 if spec.get("fortified") else 0.5), stone, moss, flux, rng)
        body.limb(name, o0, o1 - out * shoulder * 0.3, shoulder * 0.025 * flare, shoulder * 0.015, flux, glow=True, segments=4)
    # Columns of stacked plates under the core, for legs.
    for index, name in enumerate(TRAIL):
        t0, t1 = L[name]
        for sign in (1, -1):
            spot = t0.lerp(t1, 0.5) + Vector((0, sign * shoulder * 0.5, 0))
            # Its rounded bottom (up to 1.1 times its size below its centre) floats at the hover height, never under it.
            spot.z = max(spot.z, d["hover"] + shoulder * 0.55 * 1.15)
            stone_plate(body, name, spot, Vector((0.6, sign * 0.8, 0.1)), shoulder * 0.55, stone, moss, flux, rng, rune=index != 2)
            body.ball(name, t0 + Vector((0, sign * shoulder * 0.5, 0)), shoulder * 0.1 * flare, flux, glow=True, segments=6)
    # The broken ring of stone over the shoulders, lit along its inner edge: the one part that holds still.
    halo(body, L["halo"][0], shoulder * 0.9, stone, rng, gap=2, size=shoulder * 0.24)
    for index in range(12):
        a, b = index / 14 * math.tau, (index + 1) / 14 * math.tau
        point = lambda angle: L["halo"][0] + Vector((math.cos(angle), math.sin(angle), 0)) * shoulder * 0.78
        body.limb("halo", point(a), point(b), shoulder * 0.03, shoulder * 0.03, flux, glow=True, segments=4)


def stained_glass(body, spec, L, d, rng):
    """Oriel: irregular stained-glass panes in fractured leading about a warm luminous core, a gold ring behind the head,
    panes fanned from the shoulders like a window opened out, and a trailing point of shards for legs."""
    gold, lead, core_color = spec["primary"], spec["secondary"], spec["accent"]
    jewels = spec["jewels"]
    shoulder = d["shoulder"]

    def pane(bone, center, width, height, turn):
        body.box(bone, center, (shoulder * 0.05, width, height), jewels[rng.randrange(len(jewels))], glow=rng.random() < 0.5, rotation=turn)
        body.box(bone, center + Vector((-shoulder * 0.03, 0, 0)), (shoulder * 0.04, width * 1.08, height * 1.08), lead, rotation=turn)

    # The warm luminous core at her middle, panes about it at the waist.
    p0, p1 = L["pelvis"]
    body.ball("pelvis", p0.lerp(p1, 0.6) + Vector((shoulder * 0.1, 0, 0)), shoulder * 0.3, core_color, glow=True)
    for index in range(4):
        angle = (index - 1.5) * 0.7
        pane("pelvis", p0.lerp(p1, 0.5) + Vector((math.cos(angle) * shoulder * 0.42, math.sin(angle) * shoulder * 0.42, 0)),
             shoulder * 0.4, (p1 - p0).length * 1.1, Euler((0, 0, angle)))

    # The torso: panes about the core.
    s0, s1 = L["spine_03"]
    for index in range(5):
        angle = (index - 2) * 0.55
        pane("spine_03", s0.lerp(s1, 0.5) + Vector((math.cos(angle) * shoulder * 0.55, math.sin(angle) * shoulder * 0.55, rng.uniform(-0.2, 0.3) * shoulder)),
             shoulder * 0.5, shoulder * 0.9, Euler((0, 0, angle)))
    # The head: a narrow pane with a lit face.
    h0, h1 = L["head"]
    pane("head", (h0 + h1) / 2, d["head"] * 0.7, d["head"], Euler((0, 0, 0)))
    # Broad panes fanned from the shoulders.
    for side, sign in (("l", 1), ("r", -1)):
        c0, c1 = L["clavicle_" + side]
        for index in range(3):
            fan = math.radians(15 + index * 25) * sign
            pane("clavicle_" + side, c1 + Vector((-shoulder * 0.2, sign * shoulder * 0.3 * (index + 1), shoulder * (0.5 + index * 0.1))),
                 shoulder * 0.45, shoulder * 1.0, Euler((fan, 0, math.radians(-15) * sign)))
        # Framed glass limbs the whole way through.
        for part in ARM[1:]:
            b0, b1 = L[part + "_" + side]
            body.slab(part + "_" + side, b0, b1, shoulder * 0.22, shoulder * 0.12, jewels[rng.randrange(len(jewels))], glow=True)
            body.slab(part + "_" + side, b0, b1, shoulder * 0.26, shoulder * 0.08, lead)
    # Floating shards about her.
    for name in ORBITS:
        o0, o1 = L[name]
        body.limb(name, o1 - Vector((0, 0, shoulder * 0.2)), o1 + Vector((0, 0, shoulder * 0.2)), shoulder * 0.12, shoulder * 0.01,
                  jewels[rng.randrange(len(jewels))], glow=True, segments=3)
    # Narrowing below the waist to a trailing point of shards.
    for index, name in enumerate(TRAIL):
        t0, t1 = L[name]
        width = shoulder * (0.6 - index * 0.18)
        body.limb(name, t0, t1, width, max(width - shoulder * 0.18, shoulder * 0.02), jewels[index % len(jewels)], glow=index == 0, segments=4)
    # The ring of gold framework behind her head, the core's light at its centre.
    halo(body, L["halo"][0], d["head"] * 1.1, gold, rng, gap=0, size=d["head"] * 0.2, upright=True)


def air(body, spec, L, d, rng):
    """Aurelisse: shifting air, pale mineral dust and drifting crystals; brass chimes and blue crystal pendants on fine
    chains; long white and teal cloth streaming away; eyes glimmering in a face only suggested; a cyclone for legs."""
    dust, cloth, crystal, brass = spec["primary"], spec["secondary"], spec["accent"], spec["detail"]
    shoulder = d["shoulder"]
    s0, s1 = L["spine_03"]
    # A soft body of dust from her middle to her shoulders: overlapping pale puffs, narrowing at the waist.
    p0 = L["pelvis"][0]
    for index in range(7):
        share = index / 6
        bone = "pelvis" if share < 0.4 else "spine_03"
        spot = p0.lerp(s1, share) + Vector((rng.uniform(-0.1, 0.1), rng.uniform(-0.15, 0.15), 0)) * shoulder
        body.ball(bone, spot, shoulder * (0.35 + 0.3 * share), mix(dust, [1, 1, 1], rng.uniform(0, 0.3)), scale=(0.8, 1.0, 0.7))
    h0, h1 = L["head"]
    center = (h0 + h1) / 2
    body.ball("head", center, d["head"] * 0.5, dust, scale=(0.9, 0.85, 1.0))
    for sign in (1, -1):
        body.ball("head", center + Vector((d["head"] * 0.42, sign * d["head"] * 0.17, d["head"] * 0.05)), d["head"] * 0.08, crystal, glow=True)
    for side, sign in (("l", 1), ("r", -1)):
        for part in ARM[1:]:
            b0, b1 = L[part + "_" + side]
            body.limb(part + "_" + side, b0, b1, shoulder * 0.16, shoulder * 0.12, dust)
        # Cloth streaming away from the shoulders and arms.
        c0, c1 = L["clavicle_" + side]
        for index in range(2):
            start = c1 + Vector((0, 0, -index * shoulder * 0.4))
            body.slab("clavicle_" + side, start, start + Vector((-shoulder * 2.4, sign * shoulder * 0.5, -shoulder * 0.5)), shoulder * 0.35, shoulder * 0.03,
                      cloth if index == 0 else mix(cloth, [1, 1, 1], 0.8), roll=90)
        # Brass tube chimes on fine chains.
        for index in range(3):
            top = c1.lerp(c0, index * 0.3) + Vector((shoulder * 0.2, 0, -shoulder * 0.1))
            body.limb("clavicle_" + side, top, top - Vector((0, 0, shoulder * (0.5 + index * 0.15))), shoulder * 0.04, shoulder * 0.04, brass)
    # Drifting crystals and pendants.
    for name in ORBITS:
        o0, o1 = L[name]
        body.limb(name, o1 - Vector((0, 0, shoulder * 0.12)), o1 + Vector((0, 0, shoulder * 0.12)), shoulder * 0.08, shoulder * 0.01, crystal, glow=True, segments=4)
    # A compact cyclone below her: rings narrowing to the ground, ribbons trailing.
    for index, name in enumerate(TRAIL):
        t0, t1 = L[name]
        body.limb(name, t0, t1, shoulder * (0.55 - index * 0.13), shoulder * (0.42 - index * 0.13), mix(dust, cloth, 0.2 + index * 0.25))
        body.slab(name, t1, t1 + Vector((-shoulder * 1.2, (index - 1) * shoulder * 0.4, -shoulder * 0.1)), shoulder * 0.2, shoulder * 0.02, cloth, roll=90)


def halo(body, center, radius, color, rng, gap, size, upright=False):
    """A ring of blocks about centre, flat over the shoulders or upright behind the head, missing gap blocks."""
    count = 14
    for index in range(count - gap):
        angle = index / count * math.tau
        if upright:
            point = center + Vector((0, math.cos(angle) * radius, math.sin(angle) * radius))
            turn = Euler((angle, 0, 0))
        else:
            point = center + Vector((math.cos(angle) * radius, math.sin(angle) * radius, rng.uniform(-0.05, 0.05) * radius))
            turn = Euler((0, 0, angle))
        body.box("halo", point, (size * 0.6, size * 1.4, size), color, rotation=turn)


def run_stride(d):
    """How far one Run cycle carries the body: it glides, its float and sweep keeping pace."""
    return d["height"] * GLIDE_STRIDES


def pose(name, t, melee, d):
    """The humanoid's arms and head, with the floating parts' own motion and the core's lift, at t from 0 to 1."""
    arms, lift = humanoid.pose(name, t, melee, d) if name != "Death" else ({}, 0.0)
    pose = {bone: rotation for bone, rotation in arms.items() if bone.split("_")[0] in ARM + ("spine", "head")}
    tau = math.tau
    bob = math.sin(t * tau)
    if name == "Idle":
        for index, orbit in enumerate(ORBITS):
            pose[orbit] = twist(15 * math.sin(t * tau + index))
        pose[TRAIL[0]] = lean(5 * math.sin(t * tau))
        lift = d["height"] * 0.02 * bob
    elif name == "Run":
        pose["pelvis"] = lean(12)
        pose[TRAIL[0]] = lean(TRAIL_SWEEP)
        pose[TRAIL[1]] = lean(TRAIL_SWEEP * 0.5)
        for index, orbit in enumerate(ORBITS):
            pose[orbit] = combine(twist(25 * math.sin(t * tau + index)), lean(10))
        for side in ("l", "r"):
            pose["upperarm_" + side] = forward_swing(-15)
        lift = d["height"] * 0.015 * math.sin(t * tau * 2)
    elif name in ("AttackWindup", "AttackStrike"):
        # The circling parts gather behind as it draws back, and fly forward with the blow.
        gather = ease(min(1.0, t / 0.65)) if name == "AttackWindup" else 1.0 - ease(t)
        for index, orbit in enumerate(ORBITS):
            pose[orbit] = twist((35 if index % 2 else -35) * gather)
    elif name == "Cast":
        rise = ease(min(1.0, t / CAST_RELEASE_SHARE)) * (1 - ease(max(0.0, (t - 0.6) / 0.4)))
        for index, orbit in enumerate(ORBITS):
            pose[orbit] = twist(60 * rise * (1 if index % 2 else -1))
        pose["halo"] = twist(45 * rise)
        lift = d["height"] * 0.04 * rise
    elif name == "Hit":
        k = math.sin(min(1.0, t) * math.pi)
        pose["pelvis"] = lean(-10 * k)
        for index, orbit in enumerate(ORBITS):
            pose[orbit] = twist(20 * k * (1 if index % 2 else -1))
    elif name == "Death":
        # The hold on its parts gives out: the core sinks to the ground, the parts scatter and the arms fall.
        k = ease(min(1.0, t / 0.8))
        for index, orbit in enumerate(ORBITS):
            pose[orbit] = combine(twist(40 * k * (1 if index % 2 else -1)), lean(30 * k))
        for side, sign in (("l", 1), ("r", -1)):
            pose["upperarm_" + side] = roll_side(-10 * k, sign)
        pose["spine_03"] = lean(35 * k)
        pose["head"] = lean(20 * k)
        pose[TRAIL[0]] = lean(-60 * k)
        lift = -(d["core"] - d["hover"]) * 0.7 * k
    elif name == "Recall":
        settle = ease(min(1.0, t * 4))
        for orbit in ORBITS:
            pose[orbit] = twist(360 * t)
        pose["halo"] = twist(-360 * t)
        lift = d["height"] * 0.03 * settle * (1 + 0.3 * bob)
    return pose, lift
