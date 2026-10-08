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

from . import humanoid, motions, springs
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
# A skill's parts (ADR-072 §2): how far its orbiting parts turn and tilt as they gather in or fly out, in degrees; how
# far the halo turns at a full rise; how far the core tips forward at a full crouch; and how high a full rise lifts the
# core and a full crouch dips it, as shares of its height.
SKILL_GATHER_TURN = 40.0
SKILL_GATHER_TILT = 20.0
SKILL_HALO_TURN = 60.0
SKILL_CROUCH_TILT = 12.0
SKILL_RISE_LIFT = 0.05
SKILL_CROUCH_DIP = 0.03
# Loose parts a construct's kit may give it spring chains for (ADR-069 §7): long streamers of cloth flowing back and out
# off each shoulder, each on a chain from its clavicle; and a sash's ends falling back from the core, from the pelvis.
SPRING_PARTS = {
    "streamers": [("streamer_" + side, ["streamer_%s_%02d" % (side, i) for i in (1, 2, 3)] + ["streamer_%s_end" % side], "clavicle_" + side)
                  for side in ("l", "r")],
    "sash": [("sash_" + side, ["sash_%s_01" % side, "sash_%s_02" % side, "sash_%s_end" % side], "pelvis") for side in ("l", "r")],
}
# A streamer's sweep, as shares of the height: where it leaves the back of the shoulder (back, out, up), how far it flows
# back and out by its end, and how far it falls.
STREAMER_START = (-0.05, 0.02, 0.01)
STREAMER_BACK, STREAMER_OUT, STREAMER_FALL = 0.3, 0.22, 0.08
# A sash end's fall from the core: where it leaves (behind and to its side, as shares of the shoulders, so clear of the
# core), and the way it falls back, out and down, and how far (a share of the height).
SASH_START = (-0.6, 0.35)
SASH_FALL = (-0.55, 0.25, -0.8)
SASH_LENGTH = 0.32


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
    loose = spec.get("springs", {})
    for side, sign in (("l", 1.0), ("r", -1.0)):
        if "streamers" in loose:
            # Each streamer flows back and out from the back of its shoulder, falling a little as it goes.
            start = L["upperarm_" + side][0] + Vector((STREAMER_START[0], sign * STREAMER_START[1], STREAMER_START[2])) * full
            arc = [start + Vector((-STREAMER_BACK * v, sign * STREAMER_OUT * v, -STREAMER_FALL * v ** 1.5)) * full for v in (0.0, 1 / 3, 2 / 3, 1.0)]
            for index in range(3):
                L["streamer_%s_%02d" % (side, index + 1)] = (arc[index], arc[index + 1])
            L["streamer_%s_end" % side] = (arc[3], arc[3] + (arc[3] - arc[2]) * 0.05)
        if "sash" in loose:
            # Each sash end falls back, out and down from behind the core, in two spans.
            top = L["pelvis"][0] + Vector((SASH_START[0] * shoulder, sign * SASH_START[1] * shoulder, 0.0))
            fall = Vector((SASH_FALL[0], sign * SASH_FALL[1], SASH_FALL[2])).normalized() * SASH_LENGTH * full
            joints = [top + fall * (k / 2) for k in range(3)]
            L["sash_%s_01" % side] = (joints[0], joints[1])
            L["sash_%s_02" % side] = (joints[1], joints[2])
            L["sash_%s_end" % side] = (joints[2], joints[2] + fall * 0.03)
    dims = {"height": full, "full": full, "base": 0.0, "leg": core_z - hover, "head": head, "shoulder": shoulder, "arm": arm,
            "hover": hover, "core": core_z}
    return L, dims


def bones_of(spec):
    """Its bones: every construct's, and the chains its loose parts hang on."""
    return springs.bones_with(BONES, SPRING_PARTS, spec)


def springs_of(spec, L, dims):
    """Its loose parts' chains and the capsule they hang outside (its core and chest), for its art: a plain dict."""
    colliders = [{"from": "pelvis", "to": "spine_03", "radius": dims["shoulder"] * 0.5}]
    return springs.records(SPRING_PARTS, spec, L, colliders)


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


def across(direction):
    """A unit vector square to direction."""
    return direction.cross(Vector((1, 0, 0)) if abs(direction.x) < 0.9 else Vector((0, 1, 0))).normalized()


def stained_glass(body, spec, L, d, rng):
    """Oriel: memory held in glass, nonhuman and asymmetrical. A faceted bodice of irregular jewel-coloured panes in
    gold leading, lit from within and warmest over the heart, tapers to the waist; below it a skirt of ragged shards
    narrows to a trailing point that floats clear of the ground. Her limbs are framed glass the whole way through, her
    fingers long and gold. Her head is a pointed gem of panes open at the face, where her core burns in a gold frame,
    with a crest of shards; behind it stands a ring of gold framework throwing rays. Broad panes, each cut in four,
    fan from her shoulders like a window opened out, more on her left than her right, and shards drift about her."""
    gold, jewels, core_color = spec["primary"], spec["jewels"], spec["accent"]
    lead = mix(gold, spec["secondary"], 0.35)
    shoulder = d["shoulder"]
    depth = shoulder * 0.04
    framed = set()

    def frame(bone, a, b, width, color):
        """Leading along one edge, once however many panes share it."""
        key = tuple(sorted((tuple(round(value, 1) for value in a), tuple(round(value, 1) for value in b))))
        if key not in framed:
            framed.add(key)
            body.limb(bone, a, b, width, width, color, segments=4)

    def glass(bone, outline, color=None, glow_share=0.7):
        """One pane of glass in its leading."""
        body.pane(bone, outline, depth, color or jewels[rng.randrange(len(jewels))], rng.random() < glow_share)
        for a, b in zip(outline, outline[1:] + outline[:1]):
            frame(bone, a, b, shoulder * 0.022, lead)

    def crossed(bone, start, end, start_radius, end_radius, pointed):
        """A glass limb: two panes crossed along start→end, so it has width seen from any side, pointed at both ends
        or cut square."""
        along = end - start
        first = across(along.normalized())
        for width in (first, along.normalized().cross(first)):
            if pointed:
                glass(bone, [start, start + along * 0.2 + width * start_radius, start + along * 0.8 + width * end_radius, end,
                             start + along * 0.8 - width * end_radius, start + along * 0.2 - width * start_radius])
            else:
                glass(bone, [start + width * start_radius, end + width * end_radius, end - width * end_radius, start - width * start_radius]
                      if end_radius > 0 else [start + width * start_radius, end, start - width * start_radius])

    def kite(bone, base, tip, width_axis, width, edge_width):
        """A long pane from base to tip, widest a third of the way, cut in four along and across in two colours
        crosswise (a harlequin), its outer edge in heavier gold."""
        middle = base.lerp(tip, 0.32)
        left, right = middle + width_axis * width * 0.5, middle - width_axis * width * 0.5
        for a, b in ((base, right), (right, tip), (tip, left), (left, base)):
            frame(bone, a, b, edge_width, gold)
        first, second = jewels[rng.randrange(len(jewels))], jewels[rng.randrange(len(jewels))]
        for outline, color in (([base, right, middle], first), ([base, middle, left], second), ([middle, right, tip], second), ([left, middle, tip], first)):
            glass(bone, outline, color, glow_share=0.75)

    # The bodice: rings of irregular points from the waist to the collar, each ring a little askew, panes between
    # them, some cut corner to corner. Over the heart the panes burn with her core's warmth.
    waist = L["pelvis"][0] + Vector((0, 0, shoulder * 0.15))
    chest = L["spine_03"][1]
    count = 8
    rings = []
    for height, deep, wide in ((waist.z, 0.20, 0.24), (waist.z + (chest.z - waist.z) * 0.45, 0.30, 0.38),
                               (chest.z - shoulder * 0.2, 0.44, 0.70), (chest.z + shoulder * 0.12, 0.24, 0.40)):
        ring = []
        for index in range(count):
            angle = index / count * math.tau + rng.uniform(-0.12, 0.12)
            scale = rng.uniform(0.92, 1.08)
            ring.append(Vector((math.cos(angle) * shoulder * deep * scale, math.sin(angle) * shoulder * wide * scale,
                                height + rng.uniform(-0.06, 0.06) * shoulder)))
        rings.append(ring)
    for row in range(len(rings) - 1):
        bone = "pelvis" if row == 0 else "spine_03"
        for index in range(count):
            following = (index + 1) % count
            quad = [rings[row][index], rings[row][following], rings[row + 1][following], rings[row + 1][index]]
            heart = row >= 1 and index in (0, count - 1)
            color = mix(core_color, jewels[rng.randrange(len(jewels))], 0.25) if heart else None
            if rng.random() < 0.4:
                cut = rng.randrange(2)
                for outline in ([quad[cut], quad[cut + 1], quad[(cut + 2) % 4]], [quad[(cut + 2) % 4], quad[(cut + 3) % 4], quad[cut]]):
                    glass(bone, outline, color, glow_share=1.0 if heart else 0.7)
            else:
                glass(bone, quad, color, glow_share=1.0 if heart else 0.7)
    collar = L["neck_01"][0] + Vector((0, 0, shoulder * 0.15))
    for index in range(count):
        glass("spine_03", [rings[-1][index], rings[-1][(index + 1) % count], collar])

    # Below the waist: ragged shards hang apart from each other, splayed out and swept back, about a narrow point
    # trailing beneath her that floats clear of the ground. They hang from the trail, so they sweep back as she glides.
    bottom = L[TRAIL[-1]][1]
    for index in range(count):
        a, b = rings[0][index], rings[0][(index + 1) % count]
        a, b = a.lerp(b, 0.12), a.lerp(b, 0.88)
        middle = (a + b) / 2
        out = Vector((middle.x, middle.y, 0)).normalized()
        drop = rng.uniform(0.45, 0.95)
        tip = middle.lerp(bottom, drop) + out * shoulder * 0.35 * drop + Vector((-shoulder * 0.25 * drop, 0, shoulder * 0.3))
        glass(TRAIL[0], [a, b, b.lerp(tip, 0.3) + out * shoulder * 0.06, tip, a.lerp(tip, 0.3) + out * shoulder * 0.06], glow_share=0.5)
    points = [Vector((0, 0, waist.z))] + [L[name][1] for name in TRAIL]
    radii = (0.16, 0.11, 0.06, 0.0)
    for index, name in enumerate(TRAIL):
        crossed(name, points[index], points[index + 1], shoulder * radii[index], shoulder * radii[index + 1], pointed=False)

    # Framed glass limbs the whole way through, gold at the joints, and long gold fingers spread from a glass palm.
    for side, sign in (("l", 1), ("r", -1)):
        for part, radius in (("upperarm", 0.12), ("lowerarm", 0.10)):
            b0, b1 = L[part + "_" + side]
            crossed(part + "_" + side, b0, b1, shoulder * radius, shoulder * radius * 0.8, pointed=True)
            body.limb(part + "_" + side, b0, b1, shoulder * 0.03, shoulder * 0.025, gold, segments=6)
            body.ball(part + "_" + side, b0, shoulder * 0.06, gold, segments=8)
        h0, h1 = L["hand_" + side]
        reach = h1 - h0
        palm = across(reach.normalized())
        body.ball("hand_" + side, h0, shoulder * 0.05, gold, segments=8)
        glass("hand_" + side, [h0, h0 + reach * 0.5 + palm * shoulder * 0.07, h1, h0 + reach * 0.5 - palm * shoulder * 0.07], glow_share=1.0)
        for finger in range(4):
            spread = (finger - 1.5) * 0.28
            base = h0 + reach * 0.55
            body.limb("hand_" + side, base, base + (reach.normalized() * math.cos(spread) + palm * math.sin(spread)) * reach.length * 1.2,
                      shoulder * 0.018, shoulder * 0.006, gold, segments=4)

    # Broad panes fanned from the shoulders like a window opened out, their faces turned forward and up: four on her
    # left, three shorter on her right.
    up_back = Vector((-0.6, 0, 0.8)).normalized()
    facing = Vector((0.8, 0, 0.6)).normalized()
    fans = {"l": ((12, 1.75), (34, 2.2), (56, 2.4), (78, 1.9)), "r": ((22, 1.95), (48, 2.3), (72, 1.7))}
    for side, sign in (("l", 1), ("r", -1)):
        base = L["clavicle_" + side][1] + Vector((-shoulder * 0.25, -sign * shoulder * 0.05, shoulder * 0.12))
        for elevation, length in fans[side]:
            rise = math.radians(elevation)
            direction = (Vector((0, sign * math.cos(rise), 0)) + up_back * math.sin(rise)).normalized()
            kite("clavicle_" + side, base, base + direction * shoulder * length, facing.cross(direction).normalized(), shoulder * 0.58, shoulder * 0.04)

    # The head: a pointed gem of panes, open at the face, where her core burns in a gold frame; a crest of shards.
    h0, h1 = L["head"]
    head = d["head"]
    middle = h0.lerp(h1, 0.45)
    crown = h1 + Vector((-head * 0.15, 0, head * 0.35))
    chin = h0 + Vector((head * 0.05, 0, 0))
    ring = [middle + Vector((math.cos(angle) * head * 0.32, math.sin(angle) * head * 0.27, 0))
            for angle in (math.radians(30 + index * 60) for index in range(6))]
    for index in range(6):
        if index == 5:
            continue  # the face: the segment between 330 and 30 degrees, open
        a, b = ring[index], ring[(index + 1) % 6]
        glass("head", [a, b, crown])
        glass("head", [b, a, chin])
    face = middle + Vector((head * 0.2, 0, 0))
    body.ball("head", face, head * 0.26, core_color, glow=True, segments=12)
    for index in range(10):
        point = lambda angle: face + Vector((head * 0.08, math.cos(angle) * head * 0.32, math.sin(angle) * head * 0.36))
        frame("head", point(index / 10 * math.tau), point((index + 1) / 10 * math.tau), head * 0.05, gold)
    for spread, height in ((0.0, 0.85), (0.3, 0.6), (-0.3, 0.6)):
        tip = crown + Vector((-head * 0.45, spread * head, head * height))
        kite("head", crown, tip, across((tip - crown).normalized()), head * 0.22, head * 0.04)

    # The ring of gold framework behind her head, tilted back a little, its core at the head's own: a heavy outer ring,
    # a lighter inner one, and rays.
    centre = Vector((-shoulder * 0.4, 0, middle.z))
    up = Vector((-math.sin(math.radians(15)), 0, math.cos(math.radians(15))))
    radius = head * 1.05
    at = lambda angle, share: centre + Vector((0, math.cos(angle), 0)) * radius * share + up * math.sin(angle) * radius * share
    for index in range(20):
        a, b = index / 20 * math.tau, (index + 1) / 20 * math.tau
        body.limb("halo", at(a, 1.0), at(b, 1.0), shoulder * 0.07, shoulder * 0.07, gold, segments=6)
        body.limb("halo", at(a, 0.8), at(b, 0.8), shoulder * 0.02, shoulder * 0.02, gold, segments=4)
    for index in range(12):
        angle = index / 12 * math.tau
        body.limb("halo", at(angle, 1.05), at(angle, 1.35 if index % 2 else 1.18), shoulder * 0.035, shoulder * 0.004, gold, segments=4)

    # Shards drifting about her, two to a part.
    for name in ORBITS:
        o0, o1 = L[name]
        for offset in (-0.15, 0.2):
            along = Vector((rng.uniform(-0.4, 0.4), rng.uniform(-0.4, 0.4), 1.0)).normalized()
            width = across(along)
            spot = o1 + Vector((0, 0, offset * shoulder))
            size = shoulder * rng.uniform(0.25, 0.4)
            glass(name, [spot - along * size, spot + width * size * 0.35, spot + along * size, spot - width * size * 0.35], glow_share=0.8)


def air(body, spec, L, d, rng):
    """Aurelisse: a tall figure of moving air and pale mineral dust, nonhuman, all ribbon and current. A slender
    pale-blue body with currents of light spiralling down it and a face only suggested, its eyes glimmering; streams of
    air pouring back from her head like hair. White and teal cloth streams away behind her in long waving ribbons that
    lie flat to the sky, so her silhouette is horizontal and soft-edged. A brass ring stands behind her shoulders, hung
    with tube chimes and blue crystal pendants on fine chains, and more chimes hang from her sash. Crystals drift about
    her. Below the waist she is a compact cyclone of spiralling currents narrowing to a point. Soft cloth and cool air,
    never faceted glass."""
    air_color, teal, crystal, brass = spec["primary"], spec["secondary"], spec["accent"], spec["detail"]
    cloth = spec.get("cloth", [0.95, 0.95, 0.92])
    current = mix(air_color, [1.0, 1.0, 1.0], 0.6)
    shoulder, head = d["shoulder"], d["head"]
    p0, p1 = L["pelvis"]
    s0, s1 = L["spine_03"]

    def bipyramid(bone, centre, axis, size, color):
        """A cut crystal: two points meeting at its girdle."""
        body.limb(bone, centre, centre + axis * size, size * 0.4, size * 0.04, color, glow=True, segments=6)
        body.limb(bone, centre, centre - axis * size * 0.8, size * 0.4, size * 0.04, color, glow=True, segments=6)

    def hanging(bone, top, drop, tube):
        """A brass chime tube, or a crystal pendant, hung from top on a fine chain drop long."""
        body.limb(bone, top, top - Vector((0, 0, drop)), shoulder * 0.006, shoulder * 0.006, brass, segments=4)
        end = top - Vector((0, 0, drop))
        if tube:
            body.limb(bone, end, end - Vector((0, 0, shoulder * 0.38)), shoulder * 0.04, shoulder * 0.04, brass, segments=8)
        else:
            bipyramid(bone, end - Vector((0, 0, shoulder * 0.12)), Vector((0, 0, -1)), shoulder * 0.12, crystal)

    def ribbon(bone, start, back, length, width, color, phase):
        """A long ribbon of cloth streaming back from start, waving from side to side and up and down as it goes, and
        lying flat to the sky."""
        side = back.cross(Vector((0, 0, 1))).normalized()
        points = [start + back * length * k / 6 + side * math.sin(phase + k * 0.9) * width * 0.9 + Vector((0, 0, math.sin(phase * 0.7 + k * 1.1) * width * 0.5))
                  for k in range(7)]
        for k, (a, b) in enumerate(zip(points, points[1:])):
            body.slab(bone, a, b, width * (1.0 - 0.06 * k), 1.2, color)

    # The trunk: a slender waisted body of air, broad at the shoulders.
    body.limb("pelvis", p0, p1, shoulder * 0.3, shoulder * 0.34, air_color, segments=10)
    body.limb("spine_03", s0, s1 - Vector((0, 0, shoulder * 0.1)), shoulder * 0.34, shoulder * 0.46, air_color, segments=10)
    body.ball("spine_03", s1 - Vector((0, 0, shoulder * 0.2)), shoulder * 0.5, air_color, scale=(0.7, 1.0, 0.6), segments=10)
    body.limb("neck_01", *L["neck_01"], shoulder * 0.13, shoulder * 0.12, air_color, segments=8)
    # Currents of light spiralling down her from the shoulders to the waist.
    for strand in range(3):
        points = []
        for k in range(9):
            share = k / 8
            angle = strand / 3 * math.tau + share * 2.2
            radius = shoulder * (0.36 + 0.14 * share)
            points.append(p0.lerp(s1 - Vector((0, 0, shoulder * 0.15)), share) + Vector((math.cos(angle) * radius * 0.8, math.sin(angle) * radius, 0)))
        for k, (a, b) in enumerate(zip(points, points[1:])):
            body.limb("pelvis" if k < 3 else "spine_03", a, b, shoulder * 0.022, shoulder * 0.022, current, glow=True, segments=4)
    # The head: a face only suggested, two eyes glimmering in it; streams of air pour back from it like hair.
    h0, h1 = L["head"]
    centre = (h0 + h1) / 2
    body.ball("head", centre, head * 0.42, air_color, scale=(0.95, 0.82, 1.1), segments=10)
    for sign in (1, -1):
        body.ball("head", centre + Vector((head * 0.36, sign * head * 0.15, head * 0.05)), head * 0.07, [0.85, 0.95, 1.0], glow=True, scale=(0.5, 1.4, 0.7), segments=6)
    for index, spread in enumerate((-0.6, -0.3, 0.0, 0.3, 0.6)):
        start = centre + Vector((-head * 0.2, spread * head * 0.35, head * 0.3))
        stream = [start + Vector((-head * 0.75 * k, spread * head * 0.4 * k, head * (0.3 * math.sin(k * 1.2 + index) - 0.3 * k))) for k in range(6)]
        for k, (a, b) in enumerate(zip(stream, stream[1:])):
            body.limb("head", a, b, head * (0.11 - k * 0.018), head * (0.09 - k * 0.016), mix(cloth, current, 0.3 * (index % 2)), segments=6)
    # Slender arms of air with brass bangles at the wrists.
    for side, sign in (("l", 1), ("r", -1)):
        for part, radius in (("upperarm", 0.11), ("lowerarm", 0.085)):
            b0, b1 = L[part + "_" + side]
            body.limb(part + "_" + side, b0, b1, shoulder * radius, shoulder * radius * 0.8, air_color, segments=8)
        e0, e1 = L["lowerarm_" + side]
        body.limb("lowerarm_" + side, e0.lerp(e1, 0.78), e0.lerp(e1, 0.88), shoulder * 0.1, shoulder * 0.1, brass, segments=8)
        h0, h1 = L["hand_" + side]
        body.ball("hand_" + side, h0.lerp(h1, 0.5), shoulder * 0.08, air_color, scale=(1.0, 0.6, 1.3), segments=8)
        # Cloth streaming back from the shoulders and the forearms, flat to the sky.
        c1 = L["clavicle_" + side][1]
        back = Vector((-1.0, sign * 0.25, -0.05)).normalized()
        ribbon("clavicle_" + side, c1 + Vector((-shoulder * 0.2, 0, shoulder * 0.05)), back, d["height"] * 0.6, shoulder * 0.42, cloth, 0.5 * sign)
        ribbon("clavicle_" + side, c1 + Vector((-shoulder * 0.3, -sign * shoulder * 0.2, -shoulder * 0.15)), back, d["height"] * 0.48, shoulder * 0.3, teal, 1.7 * sign)
        ribbon("lowerarm_" + side, e0.lerp(e1, 0.5), Vector((-1.0, sign * 0.4, 0.0)).normalized(), d["height"] * 0.28, shoulder * 0.18, teal if side == "l" else cloth, 2.5)
    # A sash at the waist, white wound over teal, a brass disc on the hip, chimes and pendants hanging from it.
    body.limb("pelvis", p1 - Vector((0, 0, shoulder * 0.12)), p1 + Vector((0, 0, shoulder * 0.05)), shoulder * 0.4, shoulder * 0.38, cloth, segments=10)
    body.limb("pelvis", p1 - Vector((0, 0, shoulder * 0.2)), p1 - Vector((0, 0, shoulder * 0.1)), shoulder * 0.41, shoulder * 0.41, teal, segments=10)
    body.limb("pelvis", p1 + Vector((shoulder * 0.25, shoulder * 0.32, -shoulder * 0.08)), p1 + Vector((shoulder * 0.29, shoulder * 0.36, -shoulder * 0.08)),
              shoulder * 0.14, shoulder * 0.14, brass, segments=10)
    for index in range(6):
        angle = math.radians(-100 + index * 40)
        top = p1 + Vector((math.cos(angle) * shoulder * 0.42, math.sin(angle) * shoulder * 0.42, -shoulder * 0.2))
        hanging("pelvis", top, shoulder * (0.1 + 0.08 * (index % 3)), tube=index % 3 != 1)
    for side in ("l", "r"):
        ribbon(TRAIL[0], p1 + Vector((-shoulder * 0.3, 0, -shoulder * 0.15)), Vector((-1.0, 0.2 if side == "l" else -0.2, -0.15)).normalized(),
               d["height"] * 0.68, shoulder * 0.36, teal if side == "l" else cloth, 0.9 if side == "l" else 2.2)
    # A brass ring standing behind her shoulders, tilted back, hung with chimes and pendants on fine chains.
    ring_centre = Vector((-shoulder * 0.75, 0, s1.z + head * 0.25))
    up = Vector((-math.sin(math.radians(25)), 0, math.cos(math.radians(25))))
    radius = shoulder * 1.05
    at = lambda angle, share: ring_centre + Vector((0, math.cos(angle), 0)) * radius * share + up * math.sin(angle) * radius * share
    for index in range(20):
        a, b = index / 20 * math.tau, (index + 1) / 20 * math.tau
        body.limb("halo", at(a, 1.0), at(b, 1.0), shoulder * 0.035, shoulder * 0.035, brass, segments=6)
        body.limb("halo", at(a, 0.86), at(b, 0.86), shoulder * 0.015, shoulder * 0.015, brass, segments=4)
    for index in range(7):
        angle = math.radians(200 + index * 20)
        hanging("halo", at(angle, 1.0), shoulder * (0.12 + 0.1 * (index % 2)), tube=index % 2 == 0)
    # Crystals drifting about her.
    for name in ORBITS:
        o0, o1 = L[name]
        bipyramid(name, o1, Vector((rng.uniform(-0.3, 0.3), rng.uniform(-0.3, 0.3), 1.0)).normalized(), shoulder * rng.uniform(0.14, 0.2), crystal)
    # Below the waist, a compact cyclone: a column of air narrowing to a point, currents spiralling round it.
    radii = (0.34, 0.22, 0.12, 0.03)
    for index, name in enumerate(TRAIL):
        t0, t1 = L[name]
        body.limb(name, t0, t1, shoulder * radii[index], shoulder * radii[index + 1], mix(air_color, cloth, 0.25 * index), segments=10)
        for strand in range(3):
            turn = lambda share: strand / 3 * math.tau + (index + share) * 2.4
            width = lambda share: shoulder * (radii[index] + (radii[index + 1] - radii[index]) * share + 0.06)
            ring = [t0.lerp(t1, share) + Vector((math.cos(turn(share)) * width(share), math.sin(turn(share)) * width(share), 0)) for share in (0.0, 0.5, 1.0)]
            for a, b in zip(ring, ring[1:]):
                body.limb(name, a, b, shoulder * 0.025, shoulder * 0.025, current, glow=True, segments=4)


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


def skill_pose(skill, t, d):
    """A skill's own clip (ADR-072 §2) at t from 0 to 1: its motion's arms and head (the chest's turn on the core's one
    spine bone), the orbiting parts gathered in behind it or flung out ahead and wheeled about it, the halo turning and
    the core lifting with its rise and dipping with its crouch."""
    motion, hints = motions.motion(skill, t)
    pose = {}
    for bone, rotation in motion.items():
        target = "spine_03" if bone.startswith("spine") else bone
        if target.split("_")[0] in ARM + ("spine", "head"):
            pose[target] = combine(pose.get(target, (0.0, 0.0, 0.0)), rotation)
    gather, spin, rise, crouch = hints.get("gather", 0.0), hints.get("spin", 0.0), hints.get("rise", 0.0), hints.get("crouch", 0.0)
    for index, orbit in enumerate(ORBITS):
        alternate = 1 if index % 2 else -1
        pose[orbit] = combine(twist(SKILL_GATHER_TURN * gather * alternate + spin), lean(SKILL_GATHER_TILT * gather))
    pose["halo"] = twist(SKILL_HALO_TURN * rise + spin * 0.5)
    pose["pelvis"] = lean(SKILL_CROUCH_TILT * crouch)
    pose[TRAIL[0]] = lean(-SKILL_GATHER_TILT * gather)
    return pose, d["height"] * (SKILL_RISE_LIFT * rise - SKILL_CROUCH_DIP * crouch)


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
