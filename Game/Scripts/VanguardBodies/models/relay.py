"""Relay, The Last Conductor (ADR-069): an ancient power and maintenance machine, read by his silhouette from the game
camera. Character Bible §4 and his splash art: a colossal machine of blocky bone-white slab plating over dark exposed
mechanism, top-heavy and forward-leaning; huge rounded pauldrons wider than his trunk, the right one stencilled R-01; a
small head set low between them, ahead, with a single large ringed amber optic; smaller amber points at the chest and on
each hand. His arms are enormous segmented manipulators, plated in segments over dark mechanism, ending in heavy
articulated hands built for lifting; round joint housings at elbows, hips and knees; short heavy legs on broad feet. A
slender antenna mast rises from his back, lit at its tip. Moss has colonised his upper surfaces (shoulders, joint
housings, forearms and thighs) and a faded crimson expedition banner with a pale sigil is tied at his chest, under his
left pauldron. He carries nothing: the hands are manipulators and the mast an antenna, never weapons.

Low poly and flat-coloured (author 2026-10-07): the big forms that make his outline, each a flat colour the toon
material shades. He rests as his archetype lays him out (ADR-064), arms hanging; his clips stomp, throw and slam. The
banner stays geometry riding his chest: his archetype's only spring chains (ribbons) trail back and out behind the
shoulders, never where a banner tied at the chest hangs. Colours are his kit's, the mechanism lifted so it reads dark,
not black, under the toon light."""
import numpy as np

from ..sculpt import anatomy, garments, sdf, tree
from ..sculpt.anatomy import V, unit
from ..sculpt.tree import Box, Over, Shell, Union, Zone

# The height his sizes are written at (cm, his kit's): every size below is in centimetres at this height, so a size reads
# as his art measures it and scales with any other.
REFERENCE_HEIGHT = 203.7


def mix(a, b, t):
    return tuple(float(x + (y - x) * t) for x, y in zip(a, b))


def palette(spec):
    """His flat colours (sRGB): the kit's plating, a weathered tone of it, the mechanism lifted so it reads dark rather
    than black, the amber light, and the banner's faded crimson; the rest sampled from his art."""
    plate = mix(spec["primary"], (0.86, 0.8, 0.68), 0.3)
    return {"plate": plate, "worn": mix(plate, (0.55, 0.47, 0.37), 0.28), "mech": mix(spec["secondary"], (0.36, 0.31, 0.27), 0.5),
            "steel": (0.5, 0.47, 0.43), "bronze": (0.66, 0.45, 0.24), "amber": tuple(spec["accent"]),
            "banner": mix(spec["detail"], (0.75, 0.4, 0.34), 0.2), "sigil": (0.9, 0.86, 0.76), "moss": (0.36, 0.5, 0.19),
            "stencil": (0.17, 0.16, 0.16)}


def materials(S, spec):
    return {name: S.material(name, colour, glow=name == "amber") for name, colour in palette(spec).items()}


# ---------------------------------------------------------------------------------------------- flat shapes
def polygon(Q, points):
    """Signed distance from 2D points Q (n, 2) to a closed polygon (k, 2), negative inside."""
    pts = np.asarray(points, dtype=np.float64)
    Q = np.asarray(Q, dtype=np.float64)
    d = np.sum((Q - pts[0]) ** 2, axis=1)
    s = np.ones(len(Q))
    j = len(pts) - 1
    for i in range(len(pts)):
        e = pts[j] - pts[i]
        w = Q - pts[i]
        b = w - np.outer(np.clip((w @ e) / max(e @ e, 1e-12), 0.0, 1.0), e)
        d = np.minimum(d, np.sum(b * b, axis=1))
        c1, c2 = Q[:, 1] >= pts[i][1], Q[:, 1] < pts[j][1]
        c3 = e[0] * w[:, 1] > e[1] * w[:, 0]
        s = np.where((c1 & c2 & c3) | (~c1 & ~c2 & ~c3), -s, s)
        j = i
    return s * np.sqrt(d)


def pane(P, origin, ax_u, ax_v, points, half):
    """A flat plate: the polygon points (in the u, v plane through origin) extruded half either side."""
    Q = np.asarray(P, dtype=np.float64) - origin
    d2 = polygon(np.stack([Q @ ax_u, Q @ ax_v], axis=1), points)
    t = np.abs(Q @ unit(np.cross(ax_u, ax_v))) - half
    w = np.stack([d2, t], axis=1)
    return (np.minimum(np.max(w, axis=1), 0.0) + np.linalg.norm(np.maximum(w, 0.0), axis=1)).astype(np.float32)


def strokes(Q, segments, half):
    """Distance from 2D points Q (n, 2) to strokes half wide along segments ((x0, y0, x1, y1), ...): stencilled letters."""
    d = np.full(len(Q), np.inf)
    for x0, y0, x1, y1 in segments:
        a, b = np.array([x0, y0]), np.array([x1, y1])
        ab = b - a
        t = np.clip(((Q - a) @ ab) / max(ab @ ab, 1e-9), 0.0, 1.0)
        d = np.minimum(d, np.linalg.norm(Q - a - np.outer(t, ab), axis=1))
    return d - half


# The stencilled unit marking, letter by letter in a 10-unit-high cell: each letter's strokes and its advance.
GLYPHS = {"R": ([(0, 0, 0, 10), (0, 10, 4, 10), (4, 10, 5.2, 8.8), (5.2, 8.8, 5.2, 6.7), (5.2, 6.7, 4, 5.5), (0, 5.5, 4, 5.5),
                 (2.6, 5.5, 5.4, 0)], 5.4),
          "-": ([(0.4, 5, 3.6, 5)], 4.0),
          "0": ([(0, 1.6, 0, 8.4), (5, 1.6, 5, 8.4), (1.6, 10, 3.4, 10), (1.6, 0, 3.4, 0), (0, 8.4, 1.6, 10), (3.4, 10, 5, 8.4),
                 (5, 1.6, 3.4, 0), (1.6, 0, 0, 1.6)], 5.0),
          "1": ([(2.4, 0, 2.4, 10), (2.4, 10, 0.2, 7.6)], 3.2)}
MARKING = "R-01"
LETTER_GAP = 2.2
STROKE = 1.1
# Where on the right pauldron the marking lies: on its front face, a little up and out.
MARKING_FACING = unit(V(1.0, -0.12, 0.25))


def marking_segments():
    """The marking's strokes, centred on its middle (units: a letter is 10 high)."""
    out, x = [], 0.0
    for letter in MARKING:
        segments, advance = GLYPHS[letter]
        out += [(x0 + x, y0, x1 + x, y1) for x0, y0, x1, y1 in segments]
        x += advance + LETTER_GAP
    width = x - LETTER_GAP
    return [(x0 - width / 2, y0 - 5, x1 - width / 2, y1 - 5) for x0, y0, x1, y1 in out], width


# ---------------------------------------------------------------------------------------------- the sculpt
def build(S, L, dims, spec):
    """Relay's sculpt on his layout: (the whole, {"body": his plated mechanism, "sheets": none})."""
    mats = materials(S, spec)
    features = set(spec["features"])
    H = dims["height"]
    u = H / REFERENCE_HEIGHT
    p = lambda bone, i: V(*L[bone][i])  # noqa: E731
    U = lambda *values: V(*values) * u  # noqa: E731
    parts = []

    def leaf(name, distance, bounds, material, bones, protect=0.0):
        node = tree.leaf(S, name, distance, bounds, mats[material], bones, protect)
        parts.append(node)
        return node

    def rbox(name, centre, half, axes, rounding, material, bones, protect=0.0):
        """A rounded slab: half (world cm) along the columns of axes, its edges rounded by rounding."""
        centre, half = V(*centre), V(*half)
        axes = np.asarray(axes, dtype=np.float64)
        reach = float(np.linalg.norm(half))
        return leaf(name, lambda P: sdf.box(P, centre, half, axes, rounding), Box(centre - reach, centre + reach), material, bones, protect)

    def cyl(name, a, b, radius, material, bones, rounding=0.0, protect=0.0):
        a, b = V(*a), V(*b)
        return leaf(name, lambda P: sdf.cylinder(P, a, b, radius, rounding), Box.around([a, b], radius), material, bones, protect)

    def cone(name, a, b, ra, rb, material, bones, protect=0.0):
        a, b = V(*a), V(*b)
        return leaf(name, lambda P: sdf.round_cone(P, a, b, ra, rb), Box.around([a, b], max(ra, rb)), material, bones, protect)

    def ball(name, c, radii, material, bones, axes=None, protect=0.0):
        c, radii = V(*c), V(*radii)
        return leaf(name, lambda P: sdf.ellipsoid(P, c, radii, axes), Box(c - radii.max(), c + radii.max()), material, bones, protect)

    def disc(name, face, normal, radius, bones):
        """A round joint housing on a limb's side face: a dark wheel of mechanism inside a pale rim, a hub at its
        middle, as an old machine's are."""
        n = unit(normal)
        axes = sdf.frame(n)
        cyl(name + "_wheel", face - n * 5.0 * u, face + n * 0.5 * u, radius * 0.86, "mech", bones)
        major, minor = radius * 0.86, radius * 0.17
        leaf(name + "_rim", lambda P: sdf.torus(P, face, major, minor, axes), Box(face - radius * 1.1, face + radius * 1.1), "plate", bones, 0.3)
        cyl(name + "_hub", face - n * 1.0 * u, face + n * 2.6 * u, radius * 0.3, "steel", bones, rounding=0.6 * u, protect=0.3)

    def plated(name, a, b, spans, half, rounding, bones, hint=(1.0, 0.0, 0.0)):
        """A segmented limb's plates: a rounded slab over each span (shares along a to b), half (front-back, across)
        wide, dark mechanism showing between them; alternate segments weathered."""
        axes = sdf.frame(b - a, hint)
        length = float(np.linalg.norm(b - a))
        for index, (t0, t1) in enumerate(spans):
            centre = a + (b - a) * ((t0 + t1) / 2)
            rbox("%s_%d" % (name, index), centre, (half[0], half[1], length * (t1 - t0) / 2), axes, rounding, "plate" if index % 2 == 0 else "worn", bones)

    # ------------------------------------------------------------------------------------------ the trunk
    pelvis0, chest1 = p("pelvis", 0), p("spine_03", 1)
    axis = unit(chest1 - pelvis0)
    fwd = unit(V(axis[2], 0.0, -axis[0]))
    trunk = np.stack([fwd, V(0, 1, 0), axis], axis=1)

    def at(t, ahead=0.0, left=0.0):
        """A point up the leaning trunk (t: a share from the pelvis to the top of the chest), ahead of it and left of it
        (cm at his reference height)."""
        return pelvis0 + (chest1 - pelvis0) * t + fwd * ahead * u + V(0, left * u, 0)

    spine = anatomy.spine_weights(L)
    chest_bones = anatomy.rigid("spine_03")
    # The dark mechanism: a column up the leaning spine swelling into a broad, deep chest, a drum of workings round the
    # waist, and a shaft out to each shoulder.
    cone("core", at(0.2), at(0.75), 28 * u, 36 * u, "mech", spine)
    ball("chest_core", at(0.77), U(32, 50, 30), "mech", spine, axes=trunk)
    ball("waist_core", at(0.36), U(27, 40, 20), "mech", spine, axes=trunk)
    rbox("hip_core", at(0.0, 0, 0) - U(0, 0, 5), U(14, 20, 8), trunk, 5 * u, "mech", anatomy.rigid("pelvis"))
    for side, sign in (("l", 1.0), ("r", -1.0)):
        c0, c1 = p("clavicle_" + side, 0), p("clavicle_" + side, 1)
        cone("shoulder_core_" + side, c0, c1, 16 * u, 15 * u, "mech", anatomy.along("spine_03", "clavicle_" + side, c0, c1, 0.2, 0.6))
        # A plate over each flank, under the arm.
        rbox("flank_" + side, at(0.6, 2, sign * 38), U(18, 7, 16), trunk, 5 * u, "worn", anatomy.rigid("spine_02"))
    # Bone-white slabs over it: the yoke across his shoulders behind the head, the plate on his chest, the great plate
    # of his back, a plate over the belly and the small of the back, and the girdle round his hips.
    rbox("yoke", at(0.93, -10), U(18, 28, 9), trunk, 7 * u, "plate", chest_bones)
    rbox("chest_plate", at(0.71, 30), U(8, 28, 20), trunk, 6 * u, "plate", chest_bones)
    rbox("back_plate", at(0.8, -27), U(11, 42, 17), trunk, 9 * u, "plate", chest_bones)
    rbox("back_plate_low", at(0.58, -29), U(9, 37, 9), trunk, 6 * u, "worn", anatomy.rigid("spine_02"))
    rbox("belly_plate", at(0.4, 30), U(6, 20, 8), trunk, 4 * u, "worn", anatomy.rigid("spine_01"))
    rbox("lower_back", at(0.38, -28), U(6, 28, 12), trunk, 5 * u, "worn", anatomy.rigid("spine_01"))
    rbox("girdle", at(0.1), U(29, 35, 11), trunk, 7 * u, "plate", anatomy.rigid("pelvis"))
    if "amberPoints" in features:
        # The smaller amber point on his chest, set into the chest plate's lower right.
        ball("chest_light", at(0.655, 37.5, -15), U(4.4, 4.4, 4.4), "amber", chest_bones, protect=0.8)

    # ------------------------------------------------------------------------------------------ the head
    head_bones = anatomy.rigid("head")
    h0, h1 = p("head", 0), p("head", 1)
    n0 = p("neck_01", 0)
    cone("neck", n0 + U(2, 0, 2), h0 + U(-2, 0, 4), 13 * u, 11 * u, "mech", anatomy.along("neck_01", "head", n0, h0, 0.3, 0.9))
    hc = (h0 + h1) / 2 + U(2, 0, -1)
    rbox("head", hc, U(16, 15, 12), np.eye(3), 5 * u, "plate", head_bones, protect=0.4)
    # A crest of blocky plates heaped on top of it, toward the back.
    for index, (offset, half, yaw) in enumerate((((-8, 0, 14), (9, 10, 5), 8.0), ((-2, 7, 18.5), (7, 6, 4), -14.0), ((-12, -7, 19), (6.5, 6, 4), 20.0))):
        rbox("crest_%d" % index, hc + U(*offset), U(*half), sdf.rotation(yaw=yaw), 2.8 * u, "worn" if index else "plate", head_bones, protect=0.4)
    if "optic" in features:
        # The single large ringed amber optic: a bronze ring, a dark bezel and the domed amber lens, tilted up a little
        # so it reads from above.
        # The lens stands well proud of its ring, so its colour ends on an edge the reduction keeps.
        look = unit(V(np.cos(np.radians(14)), 0, np.sin(np.radians(14))))
        o0 = hc + U(13, 0, -1)
        cyl("optic_ring", o0, o0 + look * 6.5 * u, 11.5 * u, "bronze", head_bones, rounding=1.0 * u, protect=1.0)
        cyl("optic", o0 + look * 3 * u, o0 + look * 10.5 * u, 7.8 * u, "amber", head_bones, rounding=1.2 * u, protect=1.0)

    # ------------------------------------------------------------------------------------------ shoulders and arms
    caps = {}
    for side, sign in (("l", 1.0), ("r", -1.0)):
        clav = anatomy.rigid("clavicle_" + side)
        c1 = p("clavicle_" + side, 1)
        if "pauldrons" in features:
            # A huge rounded pauldron, wider than the trunk, tipped down to the outside: its dome over a broader
            # weathered rim, so it reads layered.
            pc = c1 + U(-3, sign * 4, 9)
            tilt = sdf.rotation(roll=-sign * 22.0)
            caps[side] = (pc, tilt, rbox("pauldron_" + side, pc + tilt @ U(0, 0, 1), U(32, 30, 18), tilt, 16 * u, "plate", clav))
            rbox("pauldron_rim_" + side, pc + tilt @ U(0, sign * 3, -12), U(34.5, 31.5, 6.5), tilt, 5 * u, "worn", clav)
        else:
            rbox("shoulder_plate_" + side, c1 + U(0, sign * 4, 6), U(22, 20, 12), np.eye(3), 8 * u, "plate", clav)
        s, e = p("upperarm_" + side, 0), p("upperarm_" + side, 1)
        w, hand_end = p("lowerarm_" + side, 1), p("hand_" + side, 1)
        upper, fore = anatomy.rigid("upperarm_" + side), anatomy.rigid("lowerarm_" + side)
        cone("upperarm_core_" + side, s, e, 11 * u, 10 * u, "mech", upper)
        ball("elbow_" + side, e, U(12, 12, 12), "mech", fore)
        cone("forearm_core_" + side, e, w, 12 * u, 11 * u, "mech", fore)
        forearm = spec.get("forearmScale", 1.0)
        if "segmentedArms" in features:
            # Manipulator arms: plated in segments over the mechanism, the forearms the heavier, for lifting.
            plated("upperarm_" + side, s, e, [(0.1, 0.48), (0.55, 0.93)], U(17, 18), 6 * u, upper)
            plated("forearm_" + side, e, w, [(0.06, 0.33), (0.37, 0.64), (0.68, 0.95)], U(16, 16.5) * forearm, 6 * u, fore)
        else:
            plated("upperarm_" + side, s, e, [(0.08, 0.92)], U(17, 18), 6 * u, upper)
            plated("forearm_" + side, e, w, [(0.06, 0.95)], U(16, 16.5) * forearm, 6 * u, fore)
        if "jointDiscs" in features:
            elbow_out = unit(V(0.25, sign, 0.0))
            disc("elbow_disc_" + side, e + elbow_out * 23 * u, elbow_out, 15 * u, fore)
        hand(u, side, sign, w, hand_end, features, rbox, cyl, ball)

    # ------------------------------------------------------------------------------------------ legs
    for side, sign in (("l", 1.0), ("r", -1.0)):
        hp, k = p("thigh_" + side, 0), p("thigh_" + side, 1)
        ankle = p("calf_" + side, 1)
        thigh_w = anatomy.along("pelvis", "thigh_" + side, hp + U(0, 0, 5), hp - U(0, 0, 6), 0.2, 0.7)
        thigh, calf, foot = anatomy.rigid("thigh_" + side), anatomy.rigid("calf_" + side), anatomy.rigid("foot_" + side)
        cone("thigh_core_" + side, hp + U(0, 0, 3), k, 15 * u, 13 * u, "mech", thigh_w)
        # Thigh and shin plates stop short of the knee, where its housing turns.
        thigh_length, calf_length = float(np.linalg.norm(k - hp)), float(np.linalg.norm(ankle - k))
        thigh_axes, calf_axes = sdf.frame(k - hp, (1, 0, 0)), sdf.frame(ankle - k, (1, 0, 0))
        rbox("thigh_plate_" + side, hp + (k - hp) * 0.38 + U(4, sign * 2, 0), (16 * u, 18 * u, thigh_length * 0.4), thigh_axes, 6 * u, "plate", thigh)
        rbox("thigh_back_" + side, hp + (k - hp) * 0.4 + U(-14, sign * 2, 0), (4.5 * u, 14 * u, thigh_length * 0.32), thigh_axes, 3 * u, "worn", thigh)
        ball("knee_" + side, k, U(12, 12, 12), "mech", calf)
        rbox("kneecap_" + side, k + U(14, 0, 1), U(5, 8, 8), np.eye(3), 4 * u, "worn", calf)
        cone("calf_core_" + side, k, ankle, 13 * u, 12 * u, "mech", calf)
        rbox("shin_" + side, k + (ankle - k) * 0.6 + U(1, 0, 0), (16 * u, 18 * u, calf_length * 0.38), calf_axes, 6 * u, "plate", calf)
        rbox("calf_back_" + side, k + (ankle - k) * 0.5 + U(-15, 0, 0), (4.5 * u, 13 * u, calf_length * 0.3), calf_axes, 3 * u, "worn", calf)
        ball("ankle_" + side, ankle + U(0, 0, 3), U(11, 11, 11), "mech", foot)
        # A broad flat foot on the ground and a toe plate ahead of it.
        rbox("foot_" + side, V(ankle[0] + 4 * u, ankle[1], 8 * u), U(22, 16, 8), np.eye(3), 4 * u, "plate", foot)
        rbox("toe_" + side, V(ankle[0] + 29.5 * u, ankle[1], 6.5 * u), U(7, 15, 6.5), np.eye(3), 3.5 * u, "worn", foot)
        if "jointDiscs" in features:
            # Housings turned a little forward, so they read from before him as well as beside him.
            hip_out = unit(V(0.3, sign, 0.0))
            disc("hip_disc_" + side, hp + hip_out * 21 * u, hip_out, 14 * u, thigh)
            for face in (1.0, -1.0):
                knee_out = unit(V(0.6, sign * face, 0.0))
                disc("knee_disc_%s_%d" % (side, face > 0), k + knee_out * (19 if face > 0 else 16) * u, knee_out, (15 if face > 0 else 11) * u, calf)

    # ------------------------------------------------------------------------------------------ mast and banner
    if "mast" in features:
        # The slender antenna mast rising from his back on the right, lit at its tip.
        base = chest1 + U(-37, -20, -8)
        tip = base + U(-8, -2, 56)
        rbox("mast_base", base, U(6, 6, 5), trunk, 2.5 * u, "worn", chest_bones)
        cone("mast", base, tip, 2.6 * u, 1.7 * u, "mech", chest_bones, protect=1.0)
        cyl("mast_collar", base + (tip - base) * 0.86, base + (tip - base) * 0.9, 3.4 * u, "bronze", chest_bones, protect=0.8)
        ball("mast_light", tip, U(4.4, 4.4, 4.4), "amber", chest_bones, protect=1.0)
    if "banner" in features:
        banner(u, p, leaf, cyl)
    if "moss" in features:
        hanging_moss(u, caps, cone)

    armour = Union(parts, k=0.6 * u)
    layers = [armour]
    if "pauldrons" in features and "r" in caps:
        layers.append(marking(S, mats, u, caps["r"]))
    if "moss" in features:
        layers += moss(S, u, p, mats, armour, caps, at)
    worn = Over(layers)
    # Cut flat where he meets the ground: nothing of him sinks below it.
    worn = tree.Intersect(worn, Zone(lambda P: -P[:, 2], Box(V(-500, -500, -0.5), V(500, 500, 600))))
    return worn, {"body": armour, "sheets": []}


def hand(u, side, sign, wrist, end, features, rbox, cyl, ball):
    """A heavy articulated hand built for lifting, hanging at rest: a broad palm of mechanism, a bone-white plate over
    its back with an amber point set in it, four thick plated fingers in three segments each, curled a little toward the
    palm, and a thumb from its front edge."""
    bones = anatomy.rigid("hand_" + side)
    U = lambda *values: V(*values) * u  # noqa: E731
    d = unit(end - wrist)
    n = unit(V(-0.55, -sign * 0.85, 0.0))
    n = unit(n - d * (n @ d))
    c = np.cross(d, n)
    front = -sign * c
    axes = np.stack([n, c, d], axis=1)
    palm = wrist + d * 12 * u
    if "manipulators" not in features:
        rbox("fist_" + side, palm + d * 4 * u, U(12, 17, 15), axes, 6 * u, "plate", bones)
        return
    rbox("palm_" + side, palm, U(9, 17, 13), axes, 4 * u, "mech", bones)
    rbox("hand_plate_" + side, palm - n * 8 * u + d * u, U(2.8, 16.5, 12.5), axes, 2.4 * u, "plate", bones)
    if "amberPoints" in features:
        ball("hand_light_" + side, wrist + d * 5 * u - n * 10.9 * u, U(3.8, 3.8, 3.8), "amber", bones, protect=0.8)
    root = wrist + d * 25 * u
    for index, across in enumerate((-1.5, -0.5, 0.5, 1.5)):
        spread = np.radians(across * 9.0)
        direction = unit(d * np.cos(spread) + c * np.sin(spread))
        point = root + c * across * 10.0 * u
        for joint, (length, bend) in enumerate(((14.5, 8.0), (12.0, 18.0), (10.0, 22.0))):
            # Each segment turned further toward the palm, on a dark hinge pin across the joint.
            direction = unit(direction * np.cos(np.radians(bend)) + n * np.sin(np.radians(bend)))
            normal = unit(n - direction * (n @ direction))
            side_axis = np.cross(direction, normal)
            frame = np.stack([normal, side_axis, direction], axis=1)
            a, b = point + direction * 1.2 * u, point + direction * (length - 1.2) * u
            rbox("finger_%s_%d_%d" % (side, index, joint), (a + b) / 2, (5.8 * u, 4.4 * u, float(np.linalg.norm(b - a)) / 2), frame, 2.4 * u,
                 "plate" if joint != 1 else "worn", bones, protect=0.5)
            if joint:
                cyl("hinge_%s_%d_%d" % (side, index, joint), point - side_axis * 4.0 * u, point + side_axis * 4.0 * u, 3.6 * u, "mech", bones, protect=0.3)
            point = point + direction * length * u
    # The thumb, from the palm's front edge, turned in toward the fingers.
    point = wrist + d * 9 * u + front * 15 * u + n * 3 * u
    direction = unit(d * 0.75 + front * 0.45 + n * 0.35)
    for joint, (length, bend) in enumerate(((11.5, 10.0), (9.5, 25.0))):
        direction = unit(direction * np.cos(np.radians(bend)) - front * np.sin(np.radians(bend)))
        normal = unit(n - direction * (n @ direction))
        frame = np.stack([normal, np.cross(direction, normal), direction], axis=1)
        a, b = point + direction * 1.0 * u, point + direction * (length - 1.0) * u
        rbox("thumb_%s_%d" % (side, joint), (a + b) / 2, (4.8 * u, 4.5 * u, float(np.linalg.norm(b - a)) / 2), frame, 2.2 * u, "plate", bones, protect=0.5)
        point = point + direction * length * u


# The banner's outline (cm across, cm down from its top edge), its end torn into tongues, and its pale sigil: a long
# four-pointed star.
BANNER = [(0, 0), (26, 0), (26, 72), (24, 84), (21, 74), (17.5, 88), (14, 75), (10, 85), (6.5, 73), (3.5, 81), (0, 70)]
SIGIL = [(13, 10), (15.2, 27), (21.5, 30), (15.2, 33), (13, 52), (10.8, 33), (4.5, 30), (10.8, 27)]
BANNER_LENGTH = 89


def banner(u, p, leaf, cyl):
    """The faded crimson expedition banner someone tied at his chest, under his left pauldron, its pale sigil showing
    and its torn end hanging to his waist: a flat cloth on a tie rod, riding his chest."""
    bones = anatomy.rigid("spine_03")
    yaw = np.radians(12.0)
    facing = V(np.cos(yaw), np.sin(yaw), 0.0)
    across = V(-np.sin(yaw), np.cos(yaw), 0.0)
    down = unit(V(-0.12, 0.0, -1.0))
    down = unit(down - facing * (down @ facing))
    origin = p("clavicle_l", 1) + V(35.5, -44, -2) * u
    outline = [(x * u, y * u) for x, y in BANNER]
    sigil = [(x * u, y * u) for x, y in SIGIL]
    bottom = down * BANNER_LENGTH * u
    box = Box.around([origin, origin + across * 27 * u, origin + bottom, origin + across * 27 * u + bottom], 3 * u)
    leaf("banner", lambda P: pane(P, origin, across, down, outline, 1.0 * u), box, "banner", bones, 0.3)
    # The sigil a little proud of its front face.
    face = origin + facing * 0.7 * u
    leaf("sigil", lambda P: pane(P, face, across, down, sigil, 0.9 * u), box, "sigil", bones, 0.8)
    cyl("banner_rod", origin - across * 2.5 * u + down * 0.5 * u, origin + across * 28.5 * u + down * 0.5 * u, 1.7 * u, "steel", bones, rounding=0.5 * u, protect=0.5)


def marking(S, mats, u, cap):
    """The stencilled unit marking R-01, still legible on the front of his right pauldron: dark letters lying in its
    surface, facing forward, out and up."""
    pc, tilt, node = cap
    hit, normal = garments.surface_point(node, pc, MARKING_FACING, reach=60 * u)
    right = unit(np.cross(-normal, V(0, 0, 1)))
    up = unit(np.cross(right, -normal))
    segments, width = marking_segments()
    scale = 1.25 * u

    def zone(P):
        Q = np.asarray(P, dtype=np.float64) - hit
        flat = np.stack([Q @ right, Q @ up], axis=1) / scale
        return (strokes(flat, segments, STROKE) * scale).astype(np.float32)
    region = Zone(zone, Box(hit - (width * 0.6 + 6) * scale, hit + (width * 0.6 + 6) * scale))
    stencil = Shell(S, "stencil", node, 0.0, 1.0 * u, region, mats["stencil"], hem=0.15 * u)
    stencil.part.protect = 1.0
    return stencil


def moss(S, u, p, mats, armour, caps, at):
    """Old moss and green growth colonising his upper surfaces (shoulders, joint housings, forearms and thighs; a
    patch on his chest plate): mats lying over the plates with ragged edges, never spots."""
    U = lambda *values: V(*values) * u  # noqa: E731

    def ragged(P, seed, scale=16.0):
        return sdf.fbm(np.asarray(P, dtype=np.float32), scale * u, 3, seed) - 0.5

    def band(name, a, b, radius, seed, floor=None):
        """Growth gathered along an edge from a to b, about radius either side of it, its border ragged; only above
        floor (a height) if given."""
        def zone(P):
            ab = b - a
            t = np.clip(((P - a) @ ab) / max(ab @ ab, 1e-9), 0.0, 1.0)
            d = np.linalg.norm(P - a - np.outer(t, ab), axis=1) - radius * (1.0 + 1.3 * ragged(P, seed, radius * 0.9 / u))
            return d if floor is None else np.maximum(d, floor - P[:, 2])
        return name, zone, Box.around([a, b], radius * 1.8)

    zones = []
    for side, sign in (("l", 1.0), ("r", -1.0)):
        if side in caps:
            # Along the inner edge of each pauldron's crown, where it meets the yoke, spilling out over the top; on
            # the left lapping over its front edge too (the right's front carries the marking).
            pc, tilt, _node = caps[side]
            at_cap = lambda x, y, z: pc + tilt @ U(x, sign * y, z)  # noqa: E731
            zones.append(band("moss_pauldron_" + side, at_cap(-24, -24, 14), at_cap(20, -24, 14), 10 * u, 61 + (side == "r"), pc[2] + 2 * u))
            spill = (at_cap(6, -18, 17), at_cap(-10, 14, 17)) if side == "l" else (at_cap(-18, -18, 16), at_cap(-22, 10, 15))
            zones.append(band("moss_spill_" + side, spill[0], spill[1], 6.5 * u, 63 + (side == "r"), pc[2] + 4 * u))
            if side == "l":
                zones.append(band("moss_lip_" + side, at_cap(28, -22, 10), at_cap(31, 0, 7), 7 * u, 65))
        e, w = p("lowerarm_" + side, 0), p("lowerarm_" + side, 1)
        fore_out = unit(V(0.35, sign, 0.25))
        zones.append(band("moss_forearm_" + side, e + (w - e) * 0.05 + fore_out * 19 * u, e + (w - e) * 0.3 + fore_out * 19 * u, 7.5 * u, 71 + (side == "r")))
        hp, k = p("thigh_" + side, 0), p("thigh_" + side, 1)
        zones.append(band("moss_thigh_" + side, hp + U(19, -sign * 4, -6), hp + U(14, sign * 15, -7), 6 * u, 81 + (side == "r")))
        # Over the top of the knee and elbow housings' rims.
        for joint, centre, out, radius in (("knee", k, unit(V(0.6, sign, 0.0)), 15.0), ("elbow", e, unit(V(0.25, sign, 0.0)), 15.0)):
            face = centre + out * (19 if joint == "knee" else 23) * u
            tangent = unit(np.cross(out, V(0, 0, 1)))
            top = face + U(0, 0, radius * 0.84)
            zones.append(band("moss_%s_%s" % (joint, side), top - tangent * 7 * u, top + tangent * 7 * u, 5 * u, 91 + (side == "r") + 2 * (joint == "elbow")))
    # Along the top edge of the chest plate.
    top_edge = lambda left: at(0.71, 37, left) + unit(at(1.0) - at(0.0)) * 19 * u  # noqa: E731
    zones.append(band("moss_chest", top_edge(-4), top_edge(-22), 6 * u, 101))
    return [Shell(S, name, armour, 0.0, 1.8 * u, Zone(fn, box), mats["moss"], hem=0.6 * u) for name, fn, box in zones]


def hanging_moss(u, caps, cone):
    """Strands of growth hanging from the pauldrons' rims."""
    for side, sign in (("l", 1.0), ("r", -1.0)):
        if side not in caps:
            continue
        pc, tilt, _node = caps[side]
        bones = anatomy.rigid("clavicle_" + side)
        for index, (x, y, length) in enumerate(((26, 28, 18), (-8, 32, 14), (30, 6, 13))):
            if side == "r" and index == 2:
                continue
            top = pc + tilt @ (V(x, sign * y, -16) * u)
            cone("moss_strand_%s_%d" % (side, index), top, top + V(1.5, sign * 1.5, -length) * u, 3.2 * u, 1.2 * u, "moss", bones, protect=0.4)
