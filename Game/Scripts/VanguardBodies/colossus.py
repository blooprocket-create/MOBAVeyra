"""The colossus archetype (ADR-064 §1): Silt, Relay, Varkesh and Cairn.

The humanoid's skeleton without its tail, in colossal proportions: a towering, forward-leaning trunk, enormous shoulders
and long arms over short legs, the head small and set low between the shoulders, or none. Each body is built in its own
material: sediment sheets over a dark wet core, bone-white slabs over dark mechanism, iron plates over a glowing
interior, or rough riverstone. Its animations are the humanoid's, heavier: a stomping run (on its knuckles where its arms
reach the ground), an overhead throw for a ranged attack and a two-fisted slam to cast.
"""
import math
import random

from mathutils import Euler, Matrix, Vector

from . import humanoid, springs
from .parts import Body, combine, ease, forward_swing, lean, mix, roll_side, twist

BONES = [bone for bone in humanoid.BONES if not bone[0].startswith(("tail_", "cape_"))]
# Loose parts a colossus's kit may give it spring chains for (ADR-069 §7): ribbons of material flung back off each
# shoulder, each on a chain from its clavicle; and a banner tied at the chest, on a chain from it.
SPRING_PARTS = {"ribbons": [("ribbon_" + side, ["ribbon_%s_%02d" % (side, i) for i in (1, 2, 3)] + ["ribbon_%s_end" % side], "clavicle_" + side)
                            for side in ("l", "r")],
                "banner": [("banner", ["banner_01", "banner_02", "banner_end"], "spine_03")],
                "drape": [("drape_" + end, ["drape_%s_01" % end, "drape_%s_02" % end, "drape_%s_end" % end], "pelvis") for end in ("f", "b")],
                "chains": [("chain_hip", ["chain_hip_01", "chain_hip_02", "chain_hip_end"], "pelvis"),
                           ("chain_arm", ["chain_arm_01", "chain_arm_end"], "lowerarm_l")]}
# A flung ribbon's arc, as shares of the height: where it leaves the back of the shoulder (back, out, up), how far it
# trails back and out, how high it rises and how far it falls by its end.
RIBBON_START = (-0.09, 0.02, 0.03)
RIBBON_BACK, RIBBON_OUT, RIBBON_RISE, RIBBON_FALL = 0.34, 0.2, 0.06, 0.18
# A chest banner's chain, as shares of the height: its top centre, tied under the left pauldron (forward of, inward from
# and a little below the left shoulder), and how far it falls straight down.
BANNER_TOP = (0.161, -0.154, -0.01)
BANNER_FALL = 0.42
# A waist drape's chains, before and behind him: where each leaves the belt (out from the hips' middle by a share of the
# shoulders, so clear of the trunk), and how far it falls straight down (a share of the leg: to the shins).
DRAPE_OUT, DRAPE_FALL = 0.62, 0.72
# Loose chain hung from him: one from behind his right hip (back by a share of the shoulders, out by a share of the
# hips, so clear of the trunk and thigh), swinging back and down a share of the leg toward the ground; and one from his
# left forearm (where along it), hanging straight down (a share of the height).
CHAIN_HIP_START = (-0.62, -0.5)
CHAIN_HIP_FALL = (-0.35, -0.15, -1.0)
CHAIN_HIP_LENGTH = 0.85
CHAIN_ARM_ALONG, CHAIN_ARM_LENGTH = 0.25, 0.17
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
        # Feet planted wider apart for a braced body (stanceSpread).
        spread = spec.get("stanceSpread", 1.0)
        hock = Vector((0, sign * hip * 1.1 * spread, sole))
        knee = Vector((leg * 0.06, sign * hip * 1.05 * spread, (pelvis_z + sole) / 2))
        L["thigh_" + side] = (hip_point, knee)
        L["calf_" + side] = (knee, hock)
        L["foot_" + side] = (hock, Vector((hock.x + leg * 0.35, hock.y, sole * 0.8)))
    if "ribbons" in spec.get("springs", {}):
        # Each ribbon arcs from the back of its shoulder, back and out, rising a little before it falls behind him.
        for side, sign in (("l", 1.0), ("r", -1.0)):
            start = L["upperarm_" + side][0] + Vector((RIBBON_START[0], sign * RIBBON_START[1], RIBBON_START[2])) * height
            arc = [start + Vector((-RIBBON_BACK * v, sign * RIBBON_OUT * v, RIBBON_RISE * math.sin(math.pi * v * 0.9) - RIBBON_FALL * v ** 1.5)) * height
                   for v in (0.0, 1 / 3, 2 / 3, 1.0)]
            for index in range(3):
                L["ribbon_%s_%02d" % (side, index + 1)] = (arc[index], arc[index + 1])
            L["ribbon_%s_end" % side] = (arc[3], arc[3] + (arc[3] - arc[2]) * 0.05)
    if "banner" in spec.get("springs", {}):
        # The banner hangs from its tie rod under the left pauldron, in two spans down the front of the chest.
        top = L["clavicle_l"][1] + Vector(BANNER_TOP) * height
        joints = [top + Vector((0.0, 0.0, -BANNER_FALL * height * k / 2)) for k in range(3)]
        L["banner_01"] = (joints[0], joints[1])
        L["banner_02"] = (joints[1], joints[2])
        L["banner_end"] = (joints[2], joints[2] + Vector((0.0, 0.0, -height * 0.01)))
    if "drape" in spec.get("springs", {}):
        # The drape hangs from the belt before and behind him, each in two spans.
        for end, sign in (("f", 1.0), ("b", -1.0)):
            top = L["pelvis"][0] + Vector((sign * shoulder * DRAPE_OUT, 0.0, 0.0))
            joints = [top + Vector((0.0, 0.0, -DRAPE_FALL * leg * k / 2)) for k in range(3)]
            L["drape_%s_01" % end] = (joints[0], joints[1])
            L["drape_%s_02" % end] = (joints[1], joints[2])
            L["drape_%s_end" % end] = (joints[2], joints[2] + Vector((0.0, 0.0, -height * 0.01)))
    if "chains" in spec.get("springs", {}):
        # The hip's chain in two spans from behind his right hip; the arm's in one, hanging from his left forearm.
        top = L["pelvis"][0] + Vector((CHAIN_HIP_START[0] * shoulder, CHAIN_HIP_START[1] * hip, 0.0))
        fall = Vector(CHAIN_HIP_FALL).normalized() * CHAIN_HIP_LENGTH * leg
        joints = [top + fall * (k / 2) for k in range(3)]
        L["chain_hip_01"] = (joints[0], joints[1])
        L["chain_hip_02"] = (joints[1], joints[2])
        L["chain_hip_end"] = (joints[2], joints[2] + fall * 0.03)
        e, w = L["lowerarm_l"]
        top = e.lerp(w, CHAIN_ARM_ALONG)
        bottom = top + Vector((0.0, 0.0, -CHAIN_ARM_LENGTH * height))
        L["chain_arm_01"] = (top, bottom)
        L["chain_arm_end"] = (bottom, bottom + Vector((0.0, 0.0, -height * 0.01)))
    dims = {"height": height, "full": full, "base": 0.0, "head": head, "torso": torso, "leg": leg, "shoulder": shoulder,
            "hip": hip, "arm": arm, "build": build, "sole": sole, "knuckle": "knuckleWalk" in features}
    return L, dims


def bones_of(spec):
    """Its bones: every colossus's, and the chains its loose parts hang on."""
    return springs.bones_with(BONES, SPRING_PARTS, spec)


def springs_of(spec, L, dims):
    """Its loose parts' chains and the capsules they hang outside (its trunk and legs), for its art: a plain dict."""
    colliders = [{"from": "pelvis", "to": "spine_03", "radius": dims["shoulder"] * 0.55}]
    for side in ("l", "r"):
        colliders.append({"from": "thigh_" + side, "to": "calf_" + side, "radius": dims["hip"] * 0.9})
    return springs.records(SPRING_PARTS, spec, L, colliders)


def mass(body, style, bone, start, end, r0, r1, spec, rng):
    """One segment of a colossus in its material."""
    primary, secondary, accent = spec["primary"], spec["secondary"], spec["accent"]
    if style == "sediment":
        # A dark, wet core of rounded clumps, shingled over with flaking sheets of drying sediment, some peeling away.
        for share in (0.15, 0.5, 0.85):
            radius = r0 + (r1 - r0) * share
            body.ball(bone, start.lerp(end, share), radius * 0.86, secondary, segments=8)
        flakes = [primary, mix(primary, accent, 0.35), mix(primary, secondary, 0.25), mix(primary, [1.0, 0.95, 0.8], 0.2)]
        body.shingles(bone, start, end, (r0 + r1) / 2, flakes, rng, count=8)
    elif style == "slab":
        # Bone-white slab plating over dark exposed mechanism.
        body.limb(bone, start, end, r0 * 0.55, r1 * 0.55, secondary)
        body.slab(bone, start + (end - start) * 0.06, end - (end - start) * 0.06, (r0 + r1) * 0.95, (r0 + r1) * 0.85, primary,
                  roll=rng.uniform(-6, 6))
    elif style == "iron":
        # The plates of his forearms and shins ride a little clear of him.
        iron_plates(body, bone, start, end, r0, r1, spec, rng, clear=0.15 if bone.startswith(("lowerarm_", "calf_")) else 0.0)
    elif style == "riverstone":
        body.rocks(bone, start, end, r0, r1, [primary, secondary, mix(primary, secondary, 0.5)], rng)
    else:
        raise AssertionError("Unknown colossus style: " + style)


def iron_plates(body, bone, start, end, r0, r1, spec, rng, clear=0.0):
    """Dark interlocking iron plates over a molten interior: rings of plates staggered along the segment, each a little
    askew, the interior glowing through the seams between them (seamGap of each plate's share of the ring). A plate
    riding clear of the body (clear, a share of its radius) leaves a lit gap under it, as at his extremities."""
    primary, secondary, accent = spec["primary"], spec["secondary"], spec["accent"]
    axis = end - start
    length = max(axis.length, 0.01)
    axis = axis / length
    across = axis.cross(Vector((0, 0, 1)) if abs(axis.z) < 0.9 else Vector((1, 0, 0))).normalized()
    up = axis.cross(across).normalized()
    body.limb(bone, start, end, r0 * 0.78, r1 * 0.78, accent, glow=True)
    radius = (r0 + r1) / 2
    count = max(4, min(8, round(radius / max(length, 1.0) * 9)))
    gap = spec.get("seamGap", 0.2)
    rows = 2
    for row in range(rows):
        share = (row + 0.5) / rows
        ring_radius = r0 + (r1 - r0) * share
        for index in range(count):
            angle = (index + 0.5 * row) / count * math.tau + rng.uniform(-0.12, 0.12)
            normal = across * math.cos(angle) + up * math.sin(angle)
            tangent = axis.cross(normal).normalized()
            center = start + axis * length * share + normal * ring_radius * (0.9 + clear)
            tilt = Matrix.Rotation(rng.uniform(-0.15, 0.15), 3, tangent) @ Matrix.Rotation(rng.uniform(-0.1, 0.1), 3, normal)
            basis = tilt @ Matrix((normal, tangent, axis)).transposed()
            size = (ring_radius * 0.24, ring_radius * math.tau / count * (1.0 - gap) * rng.uniform(0.9, 1.1),
                    length / rows * (1.0 - gap * 0.6) * rng.uniform(0.9, 1.15))
            shade = mix(primary, secondary, rng.uniform(0.0, 0.5))
            body.box(bone, center, size, shade, rotation=basis.to_euler())


def segmented(body, bone, start, end, r0, r1, spec, rng, count=3):
    """A slab limb in count plated segments with dark mechanism showing between them: a manipulator arm."""
    primary, secondary = spec["primary"], spec["secondary"]
    body.limb(bone, start, end, r0 * 0.5, r1 * 0.5, secondary)
    for index in range(count):
        a = start.lerp(end, index / count + 0.05)
        b = start.lerp(end, (index + 1) / count - 0.05)
        radius = r0 + (r1 - r0) * (index + 0.5) / count
        body.slab(bone, a, b, radius * 2.0, radius * 1.85, primary, roll=rng.uniform(-5, 5))


def joint_disc(body, bone, center, radius, offset, spec):
    """A round joint housing on each side face of a limb (offset out from its centre): a dark wheel of mechanism
    inside a pale rim, as an old machine's are."""
    primary, secondary = spec["primary"], spec["secondary"]
    for sign in (1, -1):
        face = center + Vector((0, sign * offset, 0))
        body.limb(bone, face - Vector((0, sign * radius * 0.25, 0)), face, radius, radius, secondary, segments=12)
        body.limb(bone, face, face + Vector((0, sign * radius * 0.08, 0)), radius * 1.08, radius * 1.08, primary, segments=12)
        body.limb(bone, face, face + Vector((0, sign * radius * 0.12, 0)), radius * 0.35, radius * 0.35, mix(primary, secondary, 0.5), segments=8)


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
    if style == "iron":
        # Plated over the top of his shoulders too, so the camera above sees iron with molten seams, not the open heart
        # of him: a ring of plates tipped over the crown and one across its middle.
        top = L["spine_03"][1]
        for index in range(7):
            angle = index / 7 * math.tau + rng.uniform(-0.1, 0.1)
            out = Vector((math.cos(angle), math.sin(angle), 0))
            normal = (out * 0.55 + Vector((0, 0, 1))).normalized()
            tangent = Vector((0, 0, 1)).cross(out).normalized()
            basis = Matrix((normal, tangent, normal.cross(tangent))).transposed()
            body.box("spine_03", top + out * shoulder * 0.55, (shoulder * 0.2, shoulder * 0.55, shoulder * 0.42),
                     mix(primary, secondary, rng.uniform(0.0, 0.5)), rotation=basis.to_euler())
        body.box("spine_03", top + Vector((0, 0, shoulder * 0.12)), (shoulder * 0.6, shoulder * 0.6, shoulder * 0.16), primary,
                 rotation=Euler((0, 0, rng.uniform(0, math.pi / 2))))
    # Enormous shoulders.
    for side in ("l", "r"):
        s0, s1 = L["clavicle_" + side]
        scale = arm_scale(features, side)
        mass(body, style, "clavicle_" + side, s0, s1 + (s1 - s0).normalized() * shoulder * 0.15, shoulder * 0.35 * scale, shoulder * 0.42 * scale, spec, rng)
    # Arms; their hands by the colossus's kind. A forearm may be the bigger, for lifting.
    forearm = spec.get("forearmScale", 0.9)
    for side in ("l", "r"):
        scale = arm_scale(features, side)
        thick = shoulder * 0.3 * scale
        if "segmentedArms" in features:
            segmented(body, "upperarm_" + side, *L["upperarm_" + side], thick, thick * 0.9, spec, rng, count=2)
            segmented(body, "lowerarm_" + side, *L["lowerarm_" + side], thick * forearm, thick * forearm * 0.95, spec, rng, count=3)
        else:
            mass(body, style, "upperarm_" + side, *L["upperarm_" + side], thick, thick * 0.9, spec, rng)
            mass(body, style, "lowerarm_" + side, *L["lowerarm_" + side], thick * forearm, thick * forearm * 0.85 / 0.9, spec, rng)
        hand(body, spec, features, side, L, thick * (forearm / 0.9 if "segmentedArms" in features else 1.0), rng)
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
        # A broad, sodden palm and five long tapering clawed digits splayed forward along the ground.
        body.ball(bone, h0.lerp(h1, 0.6), thick * 0.85, mix(primary, secondary, 0.4), scale=(1.3, 1.25, 0.7), segments=8)
        for index in (-2, -1, 0, 1, 2):
            spread = Vector((0, index * thick * 0.42, 0))
            tip = h1 + spread * 1.6 + Vector((reach.length * (0.95 - abs(index) * 0.15), 0, -h1.z * 0.85))
            body.limb(bone, h0.lerp(h1, 0.5) + spread * 0.5, tip, thick * 0.3, thick * 0.03, mix(primary, secondary, 0.35), segments=6)
    elif "manipulators" in features or "openHands" in features:
        # A broad palm and spread fingers: Relay's lifting manipulators, or Varkesh's open, empty hands.
        body.slab(bone, h0, h0 + reach * 0.6, thick * 1.6, thick * 0.9, primary)
        fingers = 4 if "manipulators" in features else 5
        for index in range(fingers):
            across = (index - (fingers - 1) / 2) / max(1, fingers - 1)
            tip = h0 + reach * 1.3 + Vector((reach.length * 0.25, across * thick * 1.6, 0))
            body.limb(bone, h0 + reach * 0.5 + Vector((0, across * thick * 1.2, 0)), tip, thick * 0.22, thick * 0.12, secondary)
        if "openHands" in features:
            # Molten fragments torn from his plating orbit the open hand, ready to throw: jagged shards, the larger
            # still dark plate on one face.
            centre = h1 + reach * 0.4
            for index in range(6):
                angle = index / 6 * math.tau + rng.uniform(-0.3, 0.3)
                around = Vector((math.cos(angle), math.sin(angle), rng.uniform(-0.4, 0.6))) * thick * rng.uniform(1.5, 2.1)
                spin = Vector((rng.uniform(-1, 1), rng.uniform(-1, 1), rng.uniform(-1, 1))).normalized()
                flat = spin.cross(Vector((0, 0, 1)) if abs(spin.z) < 0.9 else Vector((1, 0, 0))).normalized()
                size = thick * rng.uniform(0.25, 0.4)
                shard = centre + around
                body.blob("prop_" + side, shard, (spin * size * 0.35, flat * size, spin.cross(flat) * size * 1.6), accent, glow=True, segments=4)
                if index % 2 == 0:
                    body.blob("prop_" + side, shard - spin * size * 0.25, (spin * size * 0.2, flat * size * 1.05, spin.cross(flat) * size * 1.5), primary, segments=4)
    else:
        # A fist of stone.
        body.rocks(bone, h0, h1 + reach * 0.3, thick * 1.1, thick * 1.0, [primary, secondary], rng, chunks=2)
    if "hookArm" in features and side == "r":
        # The rusted iron crescent hung on heavy chain from the hook arm: outsized, the thing his silhouette is, a broad
        # band of iron sweeping round to a sharpened point.
        grip = L["prop_" + side][0]
        iron = mix(accent, [0.1, 0.08, 0.07], 0.2)
        radius = thick * 1.45
        # Hung short below the hand, and never into the ground.
        drop = min(thick * 1.1, grip.z - radius - thick * 0.6)
        center = grip + Vector((thick * 0.9, 0, -drop))
        humanoid.chain_links(body, "prop_" + side, [grip, grip + Vector((thick * 0.3, 0, -drop * 0.5)), center + Vector((-radius * 0.55, 0, radius * 0.8))],
                             thick * 0.3, mix(accent, secondary, 0.35))
        for index in range(9):
            angle = math.radians(-60 + index * 30)
            point = center + Vector((math.cos(angle) * radius, 0, math.sin(angle) * radius))
            body.box("prop_" + side, point, (thick * 0.7, thick * 0.5, thick * 0.95), mix(iron, [0.32, 0.14, 0.06], rng.uniform(0, 0.4)),
                     rotation=Euler((0, -angle, 0)))
        tip_angle = math.radians(-60 + 8 * 30)
        tip = center + Vector((math.cos(tip_angle) * radius, 0, math.sin(tip_angle) * radius))
        body.limb("prop_" + side, tip, tip + Vector((-thick * 0.6, 0, thick * 0.55)), thick * 0.42, thick * 0.03, iron, segments=4)


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
        # Heavy rusted chain wound about his torso and up over the shoulder to the hook arm, its loose end trailing behind
        # him along the ground, as though he has just dragged it up out of the river.
        rust = mix(accent, secondary, 0.35)
        link = shoulder * 0.18
        c0, c1 = L["spine_02"]
        coil = [Vector((c0.x + math.cos(index / 12 * math.tau * 1.25) * shoulder * 1.0, math.sin(index / 12 * math.tau * 1.25) * shoulder * 1.0,
                        c0.z + (c1.z - c0.z) * index / 12)) for index in range(13)]
        humanoid.chain_links(body, "spine_02", coil, link, rust)
        shoulder_r = L["clavicle_r"][1] + Vector((0, 0, shoulder * 0.35))
        humanoid.chain_links(body, "clavicle_r", [coil[-1], shoulder_r, L["upperarm_r"][1] + Vector((0, 0, shoulder * 0.2))], link, rust)
        p0, p1 = L["pelvis"]
        trail = [p0 + Vector((-d["hip"] * 1.1, d["hip"] * 0.4, 0)), p0 + Vector((-d["hip"] * 2.0, d["hip"] * 0.5, -p0.z * 0.6)),
                 Vector((p0.x - d["hip"] * 3.0, d["hip"] * 0.6, link * 0.8)), Vector((p0.x - d["hip"] * 4.2, d["hip"] * 0.4, link * 0.8))]
        humanoid.chain_links(body, "pelvis", trail, link, rust)
    if "masonry" in features:
        # Collapsed masonry worked into him: dressed blocks from an old bridge, squared and carved with inset panels, on
        # the hook arm's shoulder and a thigh.
        dressed = mix(primary, [0.62, 0.56, 0.46], 0.35)
        for bone, offset, size in (("clavicle_r", Vector((0, -shoulder * 0.2, shoulder * 0.42)), shoulder * 0.62), ("thigh_l", Vector((d["hip"] * 0.45, d["hip"] * 0.3, 0)), d["hip"] * 0.62)):
            b0, b1 = L[bone]
            centre = b0.lerp(b1, 0.55) + offset
            body.box(bone, centre, (size * 0.9, size * 1.1, size * 0.7), dressed, rotation=Euler((0, 0, rng.uniform(-0.15, 0.15))))
            for inset in (0.6, 0.32):
                body.box(bone, centre + Vector((0, 0, size * 0.36)), (size * 0.9 * inset, size * 1.1 * inset, 1.0), mix(dressed, [0.15, 0.13, 0.11], 0.35 if inset > 0.5 else 0.1))
    if spec.get("reinforced"):
        # Planted and reinforced: further layers of riverstone heaved up over his trunk, shoulders and legs.
        layer = [mix(secondary, primary, 0.4), mix(secondary, [0.18, 0.16, 0.14], 0.3), primary]
        for bone, r0, r1 in (("spine_02", shoulder * 1.0, shoulder * 1.1), ("clavicle_l", shoulder * 0.5, shoulder * 0.55), ("clavicle_r", shoulder * 0.55, shoulder * 0.6),
                             ("thigh_l", d["hip"] * 0.75, d["hip"] * 0.7), ("thigh_r", d["hip"] * 0.75, d["hip"] * 0.7), ("calf_l", d["hip"] * 0.7, d["hip"] * 0.65),
                             ("calf_r", d["hip"] * 0.7, d["hip"] * 0.65)):
            b0, b1 = L[bone]
            # Turned chunks stop well above the ankle, so none sinks below the ground.
            body.rocks(bone, b0.lerp(b1, 0.15), b0.lerp(b1, 0.55 if bone.startswith("calf") else 0.85), r0, r1, layer, rng, chunks=2)
    if "mast" in features:
        # A slender antenna mast from his back, lit at its tip.
        foot = s1 + Vector((-shoulder * 0.6, shoulder * 0.3, -d["torso"] * 0.1))
        tip = foot + Vector((-shoulder * 0.15, 0, d["height"] * 0.28))
        body.limb("spine_03", foot, tip, shoulder * 0.05, shoulder * 0.03, secondary)
        body.ball("spine_03", tip, shoulder * 0.07, accent, glow=True)
    if "banner" in features:
        # The faded crimson expedition banner someone tied across his chest, from his right shoulder, its pale sigil
        # showing; its torn end hangs below his waist.
        c0, c1 = L["spine_02"]
        top = L["clavicle_r"][1] + Vector((shoulder * 0.55, shoulder * 0.1, -d["torso"] * 0.05))
        low = c0 + Vector((shoulder * 0.95, shoulder * 0.25, -d["torso"] * 0.45))
        body.slab("spine_03", top, low, shoulder * 0.5, 2.5, detail)
        body.box("spine_03", top.lerp(low, 0.45) + Vector((2.0, 0, 0)), (1.5, shoulder * 0.16, shoulder * 0.16), [0.90, 0.86, 0.78],
                 rotation=Euler((math.radians(45), 0, 0)))
    if "pauldrons" in features:
        # Huge rounded pauldrons, wider than the trunk; the left one still carries its stencilled unit marking.
        for side, sign in (("l", 1), ("r", -1)):
            c0, c1 = L["clavicle_" + side]
            dome = c1 + Vector((0, sign * shoulder * 0.1, shoulder * 0.2))
            body.ball("clavicle_" + side, dome, shoulder * 0.56, primary, scale=(1.15, 1.0, 0.68), segments=8)
            body.limb("clavicle_" + side, dome + Vector((0, 0, -shoulder * 0.3)), dome + Vector((0, 0, -shoulder * 0.22)),
                      shoulder * 0.66, shoulder * 0.66, mix(primary, secondary, 0.6))
            if side == "l":
                body.box("clavicle_l", dome + Vector((0, shoulder * 0.6, shoulder * 0.05)), (shoulder * 0.42, 1.0, shoulder * 0.16), secondary)
    if "jointDiscs" in features:
        # Round joint housings at shoulders, elbows, hips and knees.
        forearm = spec.get("forearmScale", 0.9) * 0.3 * shoulder
        for side in ("l", "r"):
            joint_disc(body, "lowerarm_" + side, L["lowerarm_" + side][0], forearm * 0.8, forearm * 1.02, spec)
            joint_disc(body, "thigh_" + side, L["thigh_" + side][0], d["hip"] * 0.5, d["hip"] * 0.58, spec)
            joint_disc(body, "calf_" + side, L["calf_" + side][0], d["hip"] * 0.48, d["hip"] * 0.56, spec)
    if "amberPoints" in features:
        # Smaller amber points at the chest and on each hand, the optic's echoes.
        c0, c1 = L["spine_02"]
        body.ball("spine_02", c1 + Vector((shoulder * 0.9, -shoulder * 0.3, 0)), shoulder * 0.09, accent, glow=True)
        for side in ("l", "r"):
            h0, h1 = L["hand_" + side]
            body.ball("hand_" + side, h0.lerp(h1, 0.4) + Vector((shoulder * 0.18, 0, shoulder * 0.12)), shoulder * 0.06, accent, glow=True)
    if "coreSpiral" in features:
        # The bright orange-white core spiral set into his chest, in a dark socket of plate: a coil winding out from a
        # white-hot heart. It stays lit when his outer body cools.
        c0, c1 = L["spine_02"]
        core_color = spec.get("core", mix(accent, [1.0, 1.0, 1.0], 0.45))
        face = c1 + Vector((shoulder * 0.9, 0, -d["torso"] * 0.05))
        body.limb("spine_02", face - Vector((shoulder * 0.12, 0, 0)), face, shoulder * 0.42, shoulder * 0.42, mix(primary, [0, 0, 0], 0.3), segments=12)
        body.ball("spine_02", face, shoulder * 0.1, mix(core_color, [1.0, 1.0, 1.0], 0.4), glow=True, segments=8)
        coil = [face + Vector((shoulder * 0.02, math.cos(k * 0.55) * shoulder * 0.035 * k, math.sin(k * 0.55) * shoulder * 0.035 * k)) for k in range(1, 12)]
        for a, b in zip(coil, coil[1:]):
            body.limb("spine_02", a, b, shoulder * 0.045, shoulder * 0.045, core_color, glow=True, segments=6)
    if "clothDrape" in features:
        # A heavy, tattered drape hanging from the waist front and back to below the knee, held on a dark iron band by
        # iron rings.
        p0, p1 = L["pelvis"]
        hip, leg = d["hip"], d["leg"]
        band = p0 + Vector((0, 0, d["torso"] * 0.08))
        body.limb("pelvis", band - Vector((0, 0, d["torso"] * 0.04)), band + Vector((0, 0, d["torso"] * 0.04)), hip * 1.25, hip * 1.25, mix(primary, [0, 0, 0], 0.3))
        cloth = [detail, mix(detail, [0.15, 0.04, 0.03], 0.35), mix(detail, [0.55, 0.25, 0.18], 0.2)]
        for facing in (1, -1):
            for strip in range(5):
                across = (strip - 2) * hip * 0.38
                top = band + Vector((facing * hip * 1.25, across, -d["torso"] * 0.02))
                drop = leg * rng.uniform(0.55, 0.8)
                bottom = top + Vector((facing * hip * 0.25, across * 0.15, -drop))
                body.slab("pelvis", top, bottom, hip * 0.42, 2.0, cloth[(strip + facing) % len(cloth)])
        iron = mix(secondary, [0.05, 0.05, 0.05], 0.3)
        for across in (-0.55, 0.0, 0.55):
            ring = band + Vector((hip * 1.3, across * hip, -hip * 0.1))
            points = [ring + Vector((0, math.cos(k / 8 * math.tau) * hip * 0.16, math.sin(k / 8 * math.tau) * hip * 0.16)) for k in range(9)]
            for a, b in zip(points, points[1:]):
                body.limb("pelvis", a, b, hip * 0.035, hip * 0.035, iron, segments=5)
    if "flakes" in features:
        # The hump is shingled over on top too, so from above it reads as layered sediment, not as its wet core; a few
        # sheets peel up off the back as he moves.
        crown = s1 + Vector((-shoulder * 0.15, 0, shoulder * 0.55))
        flakes = [primary, mix(primary, accent, 0.35), mix(primary, [1.0, 0.95, 0.8], 0.2)]
        for index in range(9):
            angle = index / 9 * math.tau
            spot = crown + Vector((math.cos(angle) * shoulder * 0.55, math.sin(angle) * shoulder * 0.6, rng.uniform(-0.1, 0.1) * shoulder))
            tilt = Vector((math.cos(angle) * 0.4, math.sin(angle) * 0.4, 1.0)).normalized()
            side = tilt.cross(Vector((0, 0, 1)) if abs(tilt.z) < 0.99 else Vector((1, 0, 0))).normalized()
            body.blob("spine_03", spot, (tilt * shoulder * 0.08, side * shoulder * rng.uniform(0.35, 0.5), tilt.cross(side) * shoulder * rng.uniform(0.3, 0.45)),
                      flakes[index % len(flakes)])
        body.ball("spine_03", crown, shoulder * 0.5, mix(primary, secondary, 0.3), scale=(1.2, 1.3, 0.55), segments=8)
        for index in range(4):
            spot = s0.lerp(s1, rng.uniform(0.2, 1.0)) + Vector((-shoulder * rng.uniform(0.7, 0.95), rng.uniform(-shoulder, shoulder) * 0.6, shoulder * 0.2))
            body.blob("spine_03", spot, (Vector((-0.3, 0, 1)).normalized() * shoulder * 0.06, Vector((0, shoulder * rng.uniform(0.3, 0.45), 0)),
                                          Vector((-1, 0, -0.3)).normalized() * shoulder * rng.uniform(0.35, 0.5)), mix(primary, accent, 0.3))
    if "ribbons" in features:
        # Broad ribbons of wet material flung up and back off his shoulders, mid-motion: he is always throwing
        # himself through the air. Lighter and glossier than the mass, they fall away behind him.
        for side, sign in (("l", 1), ("r", -1)):
            c0, c1 = L["clavicle_" + side]
            # Thrown out and back, falling as they go: a spray behind and beside him, never a pair of horns.
            base = c1 + Vector((-shoulder * 0.3, 0, shoulder * 0.15))
            points = [base + Vector((-shoulder * 1.3 * k, sign * shoulder * 1.4 * k, shoulder * 0.35 * math.sin(math.pi * k * 0.6) - shoulder * 0.6 * k * k))
                      for k in (0.0, 0.25, 0.5, 0.75, 1.0)]
            for index in range(len(points) - 1):
                # Overlapping flattened lozenges, so the sheet reads as flung mud rather than plates.
                a, b = points[index], points[index + 1]
                along = (b - a) * 0.75
                across = along.cross(Vector((0, 0, 1))).normalized() * shoulder * (0.42 - index * 0.08)
                body.blob("clavicle_" + side, (a + b) / 2, (along, across, along.cross(across).normalized() * 2.0),
                          mix(primary, secondary, 0.15 + index * 0.08), segments=8)
            for drop in range(3):
                body.ball("clavicle_" + side, points[-1] + Vector((-shoulder * 0.1 * drop, sign * shoulder * 0.05 * drop, -shoulder * (0.15 + 0.2 * drop))),
                          shoulder * (0.09 - drop * 0.02), mix(primary, accent, 0.5), segments=6)
    if "drips" in features:
        # Rivulets running off his forearms as he drags them.
        for side in ("l", "r"):
            e0, e1 = L["lowerarm_" + side]
            spot = e0.lerp(e1, 0.45) + Vector((0, 0, -shoulder * 0.25))
            for drop in range(3):
                body.ball("lowerarm_" + side, spot + Vector((-shoulder * 0.05 * drop, 0, -shoulder * (0.12 + 0.16 * drop))),
                          shoulder * (0.1 - drop * 0.025), mix(secondary, [0.05, 0.04, 0.02], 0.3), scale=(1.0, 1.0, 1.4), segments=6)


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
