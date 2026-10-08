"""Patch, The Last Hug (ADR-069): a small stitched toy bear standing upright, read by his silhouette from the game
camera. Character Bible §5 and his splash art: a big round head on a round, pear-shaped body, stubby plush limbs and big
oval feet; cream-tan felted fur worn thin and repaired all over with visible seams and patches; mismatched button eyes,
one dark and one pale, sewn on a little crooked, over a soft pale muzzle with a stitched seam across it; round ears; a
red child's stitch heart marked with a pale cross on his chest; a torn red scarf wound at his neck, its long tail
trailing behind him; a small leather harness of straps crossing his body, a worn leather backpack on his back; and a
lit lantern in one paw, its warm amber
flame the only gentle light on him. His own face stays soft and harmless: plain fabric and flat buttons, never fangs or
glowing eyes. All the menace belongs to the spectral bear his Inside body adds: an enormous bear of crimson energy
rearing behind and above him, burning eyes, an open fanged maw and vast clawed arms.

Low poly and flat-coloured (author 2026-10-07): the big forms that make his outline, each a flat colour the toon
material shades. He rests in the A pose, the lantern hanging from his left paw. His scarf's tail hangs on the cloak's
spring chains and the lantern on its own (ADR-069 §7). Proportions are measured from his splash art (a head near two
fifths of his height, a long round body, short legs) and fitted to his layout; colours are his kit's, moved toward the
art and lifted so they read under the toon light (the art is lit red at night)."""
import math

import numpy as np

from ..sculpt import anatomy, garments, paint, sdf, sheet, tree
from ..sculpt.anatomy import V, unit
from ..sculpt.garments import around, band_z, both, either, keep_to
from ..sculpt.tree import Box, Over, Shell, Subtract, Union, Zone

# The sizes below are his own at his kit's height (a 112 cm toy), in centimetres, scaled with the layout's height.
TOY_HEIGHT = 112.0
# The spectral bear's sizes are given for one rearing 252 cm (his capsule's height times his Inside body's scale).
BEAR_HEIGHT = 252.0

# Sampled from his splash art (sRGB) and moved toward the kit, each lifted so it reads as its colour, not black, under
# the toon light from the game camera.
PALETTE = {
    "fur": (0.84, 0.7, 0.52), "muzzle": (0.93, 0.83, 0.67), "ear": (0.93, 0.77, 0.67), "nose": (0.36, 0.22, 0.18),
    "thread": (0.32, 0.2, 0.16), "red_thread": (0.8, 0.22, 0.2), "button_dark": (0.28, 0.2, 0.17), "button_pale": (0.8, 0.8, 0.78),
    "patch_dark": (0.58, 0.42, 0.28), "patch_rust": (0.7, 0.44, 0.31), "heart": (0.85, 0.18, 0.16), "cross": (0.96, 0.92, 0.82),
    "scarf": (0.66, 0.12, 0.12), "leather": (0.42, 0.27, 0.17), "pack": (0.47, 0.31, 0.2), "pack_flap": (0.36, 0.23, 0.15), "brass": (0.82, 0.64, 0.34), "lantern": (0.5, 0.38, 0.25),
}
# The spectral bear's burning eyes and its fangs and claws (linear, as the generated body gives them).
SPECTRAL_EYES = (1.0, 0.85, 0.4)
SPECTRAL_FANG = (0.95, 0.85, 0.75)


def as_seen(linear):
    """A kit colour (linear, as the generated bodies write it) as seen (sRGB), which a material takes (paint.seen)."""
    return tuple(float(v) for v in paint.seen(linear))


def materials(S, spec):
    """Every material Patch is coloured in, flat (the toon material shades it); his lantern's flame glows in his kit's
    accent."""
    mats = {name: S.material(name, colour) for name, colour in PALETTE.items()}
    mats["flame"] = S.material("flame", as_seen(spec["accent"]), glow=True)
    return mats


def build(S, L, dims, spec):
    """Patch's sculpt on his layout: (the whole, {"body": his plush skin, under his scarf and harness, "sheets": the
    scarf's tail})."""
    mats = materials(S, spec)
    features = set(spec.get("features", []))
    k = dims["height"] / TOY_HEIGHT
    limbs = {}
    trunk = plush(S, L, k, mats, limbs)
    head, face = head_of(S, L, k, mats, features)
    limbs["head"] = head
    fur = Over([trunk, head])
    layers = [fur]
    if "patches" in features:
        layers += repairs(S, L, k, mats, fur, head, limbs)
    if "stitchHeart" in features:
        layers += heart(S, k, mats, fur)
    if "harness" in features:
        layers += harness(S, L, k, mats, fur, limbs)
    worn = Over(layers + [face])
    sheets = []
    if "trailingScarf" in features:
        worn = Over([worn, scarf(S, L, k, mats)])
        sheets.append(scarf_tail(S, L, k, mats, limbs, spec))
    props = [lantern(S, L, k, mats, spec) for prop in spec.get("props", []) if prop["kind"] == "lantern"]
    worn = Over([worn] + props)
    if "spectralBear" in features:
        worn = Over([worn, spectral_bear(S, L, dims, spec)])
    # Cut flat where he meets the ground: nothing of him sinks below it.
    worn = tree.Intersect(worn, Zone(lambda P: -P[:, 2], Box(V(-500, -500, -0.5), V(500, 500, 900))))
    return worn, {"body": fur, "sheets": sheets}


# ---------------------------------------------------------------------------------------------- helpers
def smooth(x):
    x = np.clip(x, 0.0, 1.0)
    return x * x * (3.0 - 2.0 * x)


def ellipsoid_leaf(S, name, centre, radii, material, bones, axes=None, protect=0.0):
    c, r = V(*centre), V(*radii)
    return tree.leaf(S, name, lambda P: sdf.ellipsoid(P, c, r, axes), Box(c - r.max() - 1.0, c + r.max() + 1.0), material, bones, protect)


def cone_leaf(S, name, a, b, ra, rb, material, bones, protect=0.0):
    a, b = V(*a), V(*b)
    return tree.leaf(S, name, lambda P: sdf.round_cone(P, a, b, ra, rb), Box.around([a, b], max(ra, rb) + 1.0), material, bones, protect)


def project(node, centre, directions, reach=70.0):
    """Where rays coming in from outside toward centre, one along each of directions (outward), meet node's surface:
    (points, outward normals)."""
    D = np.asarray(directions, dtype=np.float64).reshape(-1, 3)
    D = D / np.linalg.norm(D, axis=1, keepdims=True)
    return garments.surface_points(node, V(*centre)[None, :] + D * reach, -D, reach=reach, steps=70)


def tangent_frame(normal, up=(0.0, 0.0, 1.0), turn=0.0):
    """Axes (columns: the normal, then two across the surface) at a surface point, the across pair turned by turn
    degrees about the normal from up."""
    n = unit(normal)
    hint = V(*up)
    if abs(hint @ n) > 0.95:
        hint = V(1.0, 0.0, 0.0)
    b = unit(hint - n * (hint @ n))
    a = np.cross(b, n)
    c, s = math.cos(math.radians(turn)), math.sin(math.radians(turn))
    return np.stack([n, a * c + b * s, -a * s + b * c], axis=1)


def square(point, axes, width, height, depth):
    """A patch's zone: a rectangle width by height across the surface at point, depth deep either side of it."""
    p = V(*point)
    half = V(depth, width * 0.5, height * 0.5)
    reach = float(np.linalg.norm(half)) + 1.0
    return Zone(lambda P: sdf.box(P, p, half, axes), Box(p - reach, p + reach))


def seam(node, centre, directions, width, stitch, every):
    """A stitched seam's zone over node: a thread line through the points rays from centre (directions, outward, eased
    between) meet node's surface, and short stitches across it every so many of those points."""
    D = np.asarray(directions, dtype=np.float64)
    # Eased between the given directions, so the line follows the surface between them.
    samples = []
    for a, b in zip(D[:-1], D[1:]):
        for t in np.linspace(0.0, 1.0, 4, endpoint=False):
            samples.append(unit(a * (1 - t) + b * t))
    samples.append(unit(D[-1]))
    points, normals = project(node, centre, samples)
    zones = [around(points, [width * 0.5] * len(points))]
    for i in range(1, len(points) - 1, every):
        along = unit(points[i + 1] - points[i - 1])
        across = unit(np.cross(normals[i], along))
        zones.append(around([points[i] - across * stitch, points[i] + across * stitch], [width * 0.45] * 2))
    return either(*zones)


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


# ---------------------------------------------------------------------------------------------- the plush body
def plush(S, L, k, mats, limbs):
    """His stuffed body in one felt: a long pear of a torso (a round belly under a narrower chest) and a short neck; fat
    tapering arms ending in mitten paws; short stout legs on big oval feet. Each part is skinned to its bones (the
    torso up its spine, each limb along its own), the weights eased across the seams where they meet."""
    fur = mats["fur"]
    spine = anatomy.spine_weights(L)
    # The belly is kept clear of the hanging arms below the shoulders (a gap the arms' blend does not bridge), so an arm
    # raised or swung never drags the belly's felt with it.
    belly = ellipsoid_leaf(S, "belly", V(3.0, 0, 38.5) * k, V(17.5, 18.5, 16.5) * k, fur, spine, protect=0.2)
    rear = ellipsoid_leaf(S, "rear", V(-6.0, 0, 30.0) * k, V(12.0, 17.0, 10.5) * k, fur, spine, protect=0.2)
    chest = ellipsoid_leaf(S, "chest", V(-0.5, 0, 57.0) * k, V(13.5, 16.5, 12.5) * k, fur, spine, protect=0.2)
    neck_a, neck_b = V(0, 0, 63.0) * k, V(0.5, 0, 75.0) * k
    neck = cone_leaf(S, "neck", neck_a, neck_b, 8.5 * k, 8.5 * k, fur, anatomy.along("spine_03", "neck_01", neck_a, neck_b, 0.35, 0.8))
    torso = Union([belly, rear, chest, neck], k=6.0 * k)
    limbs["torso"] = torso
    parts, arms = [torso], []
    for side, sign in (("l", 1.0), ("r", -1.0)):
        s0, e = V(*L["upperarm_" + side][0]), V(*L["upperarm_" + side][1])
        w, h1 = V(*L["lowerarm_" + side][1]), V(*L["hand_" + side][1])
        top = s0 + V(0, -sign * 1.5, 1.0) * k
        arm = cone_leaf(S, "arm_" + side, top, w, 7.8 * k, 6.2 * k, fur, arm_weights(side, s0, e, w), protect=0.2)
        paw_c = w + (h1 - w) * 0.55 + V(0.8, 0, 0) * k
        paw = ellipsoid_leaf(S, "paw_" + side, paw_c, V(7.0, 6.6, 7.4) * k, fur, anatomy.rigid("hand_" + side), sdf.frame(h1 - w, (1, 0, 0)), protect=0.4)
        limbs["arm_" + side] = Union([arm, paw], k=2.5 * k)
        hip, knee, ankle = V(*L["thigh_" + side][0]), V(*L["calf_" + side][0]), V(*L["calf_" + side][1])
        a, b = V(0.5 * k, hip[1], hip[2] + 1.0 * k), V(0.8 * k, ankle[1], ankle[2] + 3.0 * k)
        leg = cone_leaf(S, "leg_" + side, a, b, 10.5 * k, 8.8 * k, fur, leg_weights(side, hip[2], knee[2], k), protect=0.2)
        foot = ellipsoid_leaf(S, "foot_" + side, V(4.5 * k, hip[1] + sign * 0.8 * k, 7.0 * k), V(13.5, 9.6, 7.1) * k, fur, anatomy.rigid("foot_" + side),
                              protect=0.3)
        limbs["leg_" + side] = Union([leg, foot], k=3.0 * k)
        parts.append(limbs["leg_" + side])
        arms.append(limbs["arm_" + side])
    # The arms sewn on at the shoulders by a tight seam.
    return Union([Union(parts, k=3.0 * k)] + arms, k=1.5 * k)


def arm_weights(side, s0, e, w):
    """An arm's weights along it: the upper arm's to past its middle, then the forearm's beyond the elbow."""
    elbow = float(np.linalg.norm(e - s0) / np.linalg.norm(w - s0))
    ab = w - s0

    def weights(P):
        t = ((P - s0) @ ab) / float(ab @ ab)
        lower = smooth((t - (elbow - 0.12)) / 0.24)
        return {"upperarm_" + side: (1.0 - lower).astype(np.float32), "lowerarm_" + side: lower.astype(np.float32)}
    return weights


def leg_weights(side, hip_z, knee_z, k):
    """A leg's weights by height: the pelvis's at its top, where it is sewn into the body, then the thigh's, then below
    the knee the calf's."""
    def weights(P):
        z = P[:, 2]
        pelvis = smooth((z - (hip_z - 3.0 * k)) / (9.0 * k))
        calf = smooth(((knee_z + 4.0 * k) - z) / (8.0 * k))
        return {"pelvis": pelvis.astype(np.float32), "thigh_" + side: ((1 - pelvis) * (1 - calf)).astype(np.float32),
                "calf_" + side: ((1 - pelvis) * calf).astype(np.float32)}
    return weights


# ---------------------------------------------------------------------------------------------- the head
def head_of(S, L, k, mats, features):
    """His big round head, a little wider than tall, on its bone: the soft pale muzzle standing out low on its front,
    and round cupped ears high on its sides. And what is sewn on it: the mismatched buttons for eyes (the dark one on
    his right larger and higher, the pale one on his left smaller, lower and turned crooked), each held by a cross of red
    thread; the dark nose and stitched mouth. Returns (the head's felt, the face's details)."""
    bones = anatomy.rigid("head")
    hc = head_centre(L, k)
    skull = ellipsoid_leaf(S, "skull", hc, V(19.0, 22.5, 20.5) * k, mats["fur"], bones, protect=0.5)
    mc = hc + V(14.5, 0, -8.3) * k
    muzzle = ellipsoid_leaf(S, "muzzle", mc, V(8.5, 10.5, 7.5) * k, mats["muzzle"], bones, protect=0.7)
    parts = [Union([skull, muzzle], k=3.0 * k)]
    if "roundEars" in features:
        for side, sign in (("l", 1.0), ("r", -1.0)):
            c = hc + V(-2.5, sign * 16.5, 15.0) * k
            axes = sdf.frame(V(1.0, sign * 0.45, 0.25), (0, 0, 1))
            # A frame whose local z faces out of the ear's cup: the disc lies across it.
            ear = ellipsoid_leaf(S, "ear_" + side, c, V(8.4, 8.4, 3.4) * k, mats["fur"], bones, axes, protect=0.6)
            cup_c = c + axes[:, 2] * 2.6 * k
            cup = ellipsoid_leaf(S, "ear_cup_" + side, cup_c, V(5.8, 5.8, 2.4) * k, mats["ear"], bones, axes, protect=0.6)
            parts.append(Subtract(ear, cup, k=0.8 * k, label=cup.part.label))
    head = Union(parts, k=2.0 * k)
    face = []
    if "buttonEyes" in features:
        face += buttons(S, k, mats, head, hc, bones)
    # The nose: a soft dark knob on the muzzle's top front, broader above; the mouth stitched below it, a short line down
    # and a small soft curve either side (a toy's mouth, closed and kindly).
    nose_p, nose_n = project(head, mc, [V(0.75, 0, 0.55)])
    nose_c = nose_p[0] + nose_n[0] * 0.4 * k
    face.append(Union([ellipsoid_leaf(S, "nose", nose_c, V(3.0, 4.6, 2.6) * k, mats["nose"], bones, protect=1.0),
                       ellipsoid_leaf(S, "nose_tip", nose_c + V(0.3, 0, -1.6) * k, V(2.6, 2.4, 2.0) * k, mats["nose"], bones, protect=1.0)], k=1.2 * k))
    for name, directions in (("mouth", [V(1, 0, 0.1), V(1, 0, -0.32)]), ("mouth_l", [V(1, 0, -0.32), V(0.93, 0.3, -0.44), V(0.86, 0.48, -0.36)]),
                             ("mouth_r", [V(1, 0, -0.32), V(0.93, -0.3, -0.44), V(0.86, -0.48, -0.36)])):
        points, _normals = project(head, mc, directions)
        mouth = tree.leaf(S, name, lambda P, pts=points: sdf.tube(P, pts, [0.7 * k] * len(pts)), Box.around(points, 2.0 * k), mats["thread"], bones, protect=1.0)
        face.append(mouth)
    return head, Union(face) if len(face) > 1 else face[0]


def head_centre(L, k):
    """The head's middle: its crown at the head bone's tip."""
    return V(1.0 * k, 0.0, L["head"][1][2] - 21.2 * k)


def buttons(S, k, mats, head, hc, bones):
    """His mismatched button eyes: flat discs sewn on, each crossed by red thread; the dark one on his right, the pale
    one on his left smaller, lower and turned crooked."""
    out = []
    for side, direction, radius, material, turn in (("r", V(0.8, -0.52, 0.12), 5.0 * k, mats["button_dark"], 8.0),
                                                     ("l", V(0.82, 0.5, -0.06), 4.2 * k, mats["button_pale"], -22.0)):
        p, n = project(head, hc, [direction])
        p, n = p[0], n[0]
        a, b = p - n * 0.8 * k, p + n * 1.5 * k
        out.append(tree.leaf(S, "button_" + side, lambda P, a=a, b=b, r=radius: sdf.cylinder(P, a, b, r, 0.6 * k), Box.around([a, b], radius + 1.0),
                             material, bones, protect=1.0))
        face = p + n * 1.55 * k
        for j, angle in enumerate((45.0 + turn, -45.0 + turn)):
            axes = tangent_frame(n, turn=angle)
            half = V(0.35 * k, radius * 0.55, 0.45 * k)
            out.append(tree.leaf(S, "button_thread_%s_%d" % (side, j), lambda P, c=face, h=half, ax=axes: sdf.box(P, c, h, ax, 0.2 * k),
                                 Box(face - radius, face + radius), mats["red_thread"], bones, protect=1.0))
    return out


# ---------------------------------------------------------------------------------------------- repairs, heart, harness
def repairs(S, L, k, mats, fur, head, limbs):
    """Repairs all over him: patches of other felt sewn on (on the crown of his head, his right arm, his right leg, his
    belly and his right flank behind the arm), and stitched seams: one down his forehead, one across his muzzle and one
    down his left leg."""
    shells = []
    hc = head_centre(L, k)
    s0, w = V(*L["upperarm_r"][0]), V(*L["lowerarm_r"][1])
    patches = [
        # (name, its limb, the point rays come in toward, the way out from it to the patch, width, height, turn, felt)
        ("patch_crown", "head", hc, V(0.25, 0.45, 0.86), 9.5, 8.5, 18.0, "patch_dark"),
        ("patch_arm", "arm_r", s0 + (w - s0) * 0.32, V(0.5, -0.86, 0.0), 7.0, 7.5, -12.0, "patch_rust"),
        ("patch_thigh", "leg_r", V(0.0, -12.0, 19.0) * k, V(0.85, -0.5, 0.0), 7.5, 7.0, 10.0, "patch_dark"),
        ("patch_belly", "torso", V(0.0, 0.0, 44.0) * k, V(0.85, -0.55, 0.0), 9.0, 8.0, -14.0, "patch_dark"),
        ("patch_back", "torso", V(0.0, 0.0, 50.0) * k, V(-0.55, -0.84, 0.05), 9.0, 9.0, 8.0, "patch_rust"),
    ]
    for name, limb, centre, direction, width, height, turn, felt in patches:
        p, n = project(limbs[limb], centre, [direction])
        region = both(square(p[0], tangent_frame(n[0], turn=turn), width * k, height * k, 3.0 * k), keep_to(limbs, [limb]))
        shell = Shell(S, name, fur, 0.0, 0.4 * k, region, mats[felt], hem=0.2 * k)
        shell.part.protect = 0.4
        shells.append(shell)
    lines = [
        # Down his forehead from the crown to above his dark eye.
        ("seam_brow", head, hc, [V(0.1, -0.05, 1.0), V(0.42, -0.2, 0.88), V(0.66, -0.38, 0.62)], 2),
        # Across his muzzle, from its top beside his nose down across its right side to its edge.
        ("seam_muzzle", head, hc + V(14.5, 0, -8.3) * k, [V(0.62, -0.42, 0.66), V(0.72, -0.62, 0.2), V(0.4, -0.9, -0.2)], 2),
        # Down the front of his left leg.
        ("seam_leg", limbs["leg_l"], V(0.5, 12.0, 17.0) * k, [V(1.0, 0.25, 0.7), V(1.0, 0.3, 0.0), V(1.0, 0.25, -0.55)], 2),
    ]
    for name, node, centre, directions, every in lines:
        shell = Shell(S, name, fur, 0.0, 0.35 * k, seam(node, centre, directions, 1.1 * k, 1.7 * k, every), mats["thread"], hem=0.15 * k)
        shell.part.protect = 0.6
        shells.append(shell)
    return shells


def heart(S, k, mats, fur):
    """The red child's stitch heart sewn on his chest, over his left breast, marked with a pale cross."""
    # Low enough on the chest that the game camera sees it under his big head.
    centre = V(0, 8.0, 51.5) * k
    t = np.linspace(0.0, 2.0 * np.pi, 33)[:-1]
    shape = np.stack([16 * np.sin(t) ** 3, 13 * np.cos(t) - 5 * np.cos(2 * t) - 2 * np.cos(3 * t) - np.cos(4 * t)], axis=1)
    shape -= (shape.max(axis=0) + shape.min(axis=0)) * 0.5
    shape *= 12.5 * k / (shape[:, 0].max() - shape[:, 0].min())
    front = 4.0 * k

    def outline(P):
        Q = np.stack([P[:, 1] - centre[1], P[:, 2] - centre[2]], axis=1)
        return np.maximum(polygon(Q, shape), front - P[:, 0])
    region = Zone(outline, Box(V(front, centre[1] - 8 * k, centre[2] - 8 * k), V(40 * k, centre[1] + 8 * k, centre[2] + 8 * k)))
    patch = Shell(S, "heart", fur, 0.0, 0.6 * k, region, mats["heart"], hem=0.25 * k)
    patch.part.protect = 0.8
    # The pale cross: two stitched bars sewn across the heart's middle, standing a little proud of it (their skinning
    # the chest's beneath them).
    p, n = project(Over([fur, patch]), centre + V(0, 0, 0.8 * k), [V(1.0, 0.0, 0.0)])
    mark = p[0] + n[0] * 0.25 * k
    bars = []
    for angle in (45.0, -45.0):
        axes = tangent_frame(n[0], turn=angle)
        bars.append(tree.leaf(S, "heart_cross_%d" % len(bars), lambda P, ax=axes: sdf.box(P, mark, V(0.55 * k, 3.0 * k, 0.8 * k), ax, 0.3 * k),
                              Box(mark - 4 * k, mark + 4 * k), mats["cross"], None, protect=1.0))
    return [patch, Union(bars)]


def harness(S, L, k, mats, fur, limbs):
    """His small leather harness: a strap from his right shoulder across his chest (clear of the heart) to his left
    hip and back over his back; a second from his left shoulder down his back to his right hip, so the two cross behind
    him; a belt low round his belly; a brass buckle on the chest strap and studs on the belt; and the worn leather
    backpack the straps carry on his back."""
    torso = keep_to(limbs, ["torso"])
    box = Box(V(-40, -40, 25) * k, V(40, 40, 78) * k)
    shells = []

    def strap(name, top, low, zone=None):
        normal = unit(np.cross(low - top, V(1.0, 0.0, 0.0)))
        region = both(garments.band_plane(top, normal, 4.4 * k, box), torso, zone)
        shell = Shell(S, name, fur, 0.2 * k, 0.75 * k, region, mats["leather"], hem=0.3 * k)
        shell.part.protect = 0.4
        shells.append(shell)
        return normal
    sash_top, sash_low = V(0, -17.0, 69.0) * k, V(0, 17.0, 30.0) * k
    strap("sash", sash_top, sash_low)
    strap("back_strap", V(0, 17.0, 69.0) * k, V(0, -17.0, 30.0) * k, Zone(lambda P: P[:, 0] - 3.0 * k, box))
    belt_z = 34.0 * k
    belt = Shell(S, "belt", fur, 0.2 * k, 0.8 * k, both(band_z(belt_z - 1.8 * k, belt_z + 1.8 * k), torso), mats["leather"], hem=0.3 * k)
    belt.part.protect = 0.4
    shells.append(belt)
    # The buckle, on the chest strap below his right shoulder: a brass frame across the strap.
    worn = Over([fur] + shells)
    at = sash_top + (sash_low - sash_top) * 0.26
    p, n = project(worn, V(0, at[1], at[2]), [V(1.0, 0.0, 0.0)])
    along = unit(sash_low - sash_top)
    axes = tangent_frame(n[0], up=along)
    c = p[0] + n[0] * 0.2 * k

    def frame(P):
        outer = sdf.box(P, c, V(0.7 * k, 3.4 * k, 2.6 * k), axes, 0.3 * k)
        inner = sdf.box(P, c, V(1.5 * k, 2.2 * k, 1.4 * k), axes, 0.2 * k)
        return np.maximum(outer, -inner)
    bones = anatomy.spine_weights(L)
    gear = [tree.leaf(S, "buckle", frame, Box(c - 5 * k, c + 5 * k), mats["brass"], bones, protect=1.0)]
    # Studs on the belt's front.
    for j, y in enumerate((-8.0, 0.0, 8.0)):
        p, n = project(worn, V(0, y * k, belt_z), [V(1.0, 0.0, 0.0)])
        stud = p[0] + n[0] * 0.3 * k
        gear.append(tree.leaf(S, "stud_%d" % j, lambda P, s=stud: sdf.sphere(P, s, 1.0 * k), Box(stud - 2 * k, stud + 2 * k), mats["brass"], bones, protect=0.8))
    gear += backpack(S, k, mats, worn, bones)
    return shells + gear


def backpack(S, k, mats, worn, bones):
    """A worn leather backpack on his back where the straps cross: a soft rounded body standing off his back, a darker
    flap over its top half with a brass buckle, and a pouch on each side; it rides his spine as the straps do."""
    p, n = project(worn, V(0, 0, 52.0 * k), [V(-1.0, 0.0, 0.0)])
    back = p[0]
    depth, width, height = 5.5 * k, 11.0 * k, 13.0 * k
    c = back + V(-depth * 0.85, 0.0, 0.0)
    parts = [tree.leaf(S, "pack", lambda P: sdf.box(P, c, V(depth, width, height), None, 3.0 * k), Box(c - 16 * k, c + 16 * k), mats["pack"], bones, protect=0.5)]
    flap_c = c + V(-depth * 0.35, 0.0, height * 0.45)
    parts.append(tree.leaf(S, "pack_flap", lambda P: sdf.box(P, flap_c, V(depth * 0.75, width * 1.04, height * 0.6), None, 2.5 * k),
                           Box(flap_c - 16 * k, flap_c + 16 * k), mats["pack_flap"], bones, protect=0.5))
    clasp = c + V(-depth * 1.15, 0.0, -height * 0.12)
    parts.append(tree.leaf(S, "pack_buckle", lambda P: sdf.box(P, clasp, V(0.6 * k, 1.8 * k, 1.8 * k), None, 0.3 * k), Box(clasp - 3 * k, clasp + 3 * k),
                           mats["brass"], bones, protect=0.8))
    for sign in (1.0, -1.0):
        pc = c + V(-depth * 0.1, sign * width * 1.05, -height * 0.3)
        parts.append(tree.leaf(S, "pack_pouch_%d" % (sign > 0), lambda P, pc=pc: sdf.box(P, pc, V(depth * 0.7, 3.0 * k, height * 0.45), None, 1.5 * k),
                               Box(pc - 10 * k, pc + 10 * k), mats["pack_flap"], bones, protect=0.5))
    return [Union(parts, k=0.6 * k)]


# ---------------------------------------------------------------------------------------------- scarf
def scarf(S, L, k, mats):
    """The torn red scarf wound thick at his neck under his chin, bunched as it goes round, knotted at his left side
    where its long tail leaves it."""
    red = mats["scarf"]
    c = V(1.0, 0, 69.5) * k
    axes = sdf.rotation(pitch=6.0)
    bones = wrap_weights(L, k)
    ring = tree.leaf(S, "scarf_wrap", lambda P: sdf.torus(P, c, 13.8 * k, 4.8 * k, axes), Box(c - 20 * k, c + 20 * k), red, bones, protect=0.4)
    lumps = []
    for j, (degrees, size) in enumerate(((20.0, 5.4), (-35.0, 5.6), (-110.0, 5.2), (160.0, 5.4), (215.0, 5.0))):
        a = math.radians(degrees)
        at = c + axes @ V(math.cos(a) * 13.8 * k, math.sin(a) * 13.8 * k, 0.0)
        lumps.append(ellipsoid_leaf(S, "scarf_fold_%d" % j, at, V(size, size, size * 0.85) * k, red, bones))
    knot_a = math.radians(95.0)
    knot = c + axes @ V(math.cos(knot_a) * 15.5 * k, math.sin(knot_a) * 15.5 * k, -1.5 * k)
    lumps.append(ellipsoid_leaf(S, "scarf_knot", knot, V(6.2, 5.6, 6.6) * k, red, bones, protect=0.4))
    return Union([ring] + lumps, k=3.0 * k)


def wrap_weights(L, k):
    """The scarf's wrap: held by the chest, its upper edge following the neck."""
    chest, neck = L["spine_03"][1][2], L["neck_01"][1][2]

    def weights(P):
        up = smooth((P[:, 2] - chest) / max(neck - chest + 4.0 * k, 1e-6))
        return {"spine_03": (1 - 0.5 * up).astype(np.float32), "neck_01": (0.5 * up).astype(np.float32)}
    return weights


def scarf_tail(S, L, k, mats, limbs, spec):
    """The scarf's long torn tail: from the knot at his left side down his back, widening and flaring out to his left as
    it falls to the backs of his knees, its end torn into tongues. It hangs on the cloak's chains down his back (on the
    left chain at its left edge, the middle one at its right), held at its top by his chest."""
    material = S.material("scarf_tail", PALETTE["scarf"])
    columns, rows, strips = 10, 8, 5
    t0, t1 = math.radians(114.0), math.radians(180.0)
    top_z, hem_z = 70.0 * k, 22.0 * k
    # His back's reach round the tail's sweep at several heights (down to his belly's underside), so the tail lies on
    # it: hanging from its top, it rests on the widest of his back above each height and falls plumb below it.
    angles = np.linspace(t0, t1, 7)
    heights = np.linspace(top_z, 30.0 * k, 9)
    A, Z = np.meshgrid(angles, heights, indexing="ij")
    outward = np.stack([np.cos(A).ravel(), np.sin(A).ravel(), np.zeros(A.size)], axis=1)
    centres = np.stack([np.zeros(A.size), np.zeros(A.size), Z.ravel()], axis=1)
    hits, _normals = garments.surface_points(limbs["torso"], centres + outward * 45.0, -outward, reach=45.0)
    reach = np.maximum.accumulate(np.linalg.norm(hits[:, :2], axis=1).reshape(A.shape), axis=1)
    arms = []
    for side in ("l", "r"):
        arms += [(L["upperarm_" + side][0], L["upperarm_" + side][1], 7.6 * k), (L["lowerarm_" + side][0], L["lowerarm_" + side][1], 7.0 * k)]
    mid = 0.5 * (t0 + t1)

    def position(u, v):
        theta = mid + ((t0 + (t1 - t0) * u) - mid) * (1.0 + 0.4 * v)
        z = top_z + (hem_z - top_z) * v
        # The back's reach there (bilinear over the samples), clear of it by a little, flaring out as it falls.
        ai = np.clip((theta - t0) / (t1 - t0), 0.0, 1.0) * (len(angles) - 1)
        zi = np.clip((top_z - z) / (top_z - heights[-1]), 0.0, 1.0) * (len(heights) - 1)
        a0, z0 = np.minimum(ai.astype(int), len(angles) - 2), np.minimum(zi.astype(int), len(heights) - 2)
        fa, fz = ai - a0, zi - z0
        r = (reach[a0, z0] * (1 - fa) * (1 - fz) + reach[a0 + 1, z0] * fa * (1 - fz) + reach[a0, z0 + 1] * (1 - fa) * fz
             + reach[a0 + 1, z0 + 1] * fa * fz)
        r = np.maximum(r + 1.4 * k + (1.5 * k + 5.0 * k * (1.0 - u)) * v ** 1.3, 15.5 * k * (1.0 - v))
        p = np.stack([r * np.cos(theta), r * np.sin(theta), z], axis=1)
        return sheet.clear_of(p, arms, 1.2 * k)
    if "cape_l_01" in L:
        chains = {name: [V(*L["%s_%02d" % (name, i)][0]) for i in (1, 2, 3)] + [V(*L[name + "_end"][0])] for name in ("cape_l", "cape")}
    else:
        chains = {"cape": [V(*L["cape_%02d" % i][0]) for i in (1, 2, 3)] + [V(*L["cape_end"][0])]}
    across = sheet.sweep_shares(chains, t0, t1)

    def bones(P, u, v):
        hold = np.clip(1 - v / 0.12, 0, 1)
        return sheet.down_chains(chains, across, P, u, hold, "spine_03")
    return sheet.Sheet("scarf_tail", position, material, bones, columns, rows, reach=lambda u: sheet.torn(u, strips, 0.6, 0.22, 3.0))


# ---------------------------------------------------------------------------------------------- lantern
def lantern(S, L, k, mats, spec):
    """His lantern, held by its ring in his left paw: a small old brass lantern, a domed roof and a heavy base round a
    glass glowing warm amber (the only gentle light on him; no bars across it, which at this density read as a face). It
    swings on its own chain where his kit gives it one (ADR-069 §7)."""
    bones = anatomy.rigid("lantern_01" if "lantern" in spec.get("springs", {}) else "prop_l")
    g = V(*L["prop_l"][0])
    down = lambda cm: g - V(0, 0, cm * k)  # noqa: E731
    metal, flame = mats["lantern"], mats["flame"]
    parts = [cone_leaf(S, "lantern_ring", g, down(7.0), 0.6 * k, 0.6 * k, metal, bones, protect=1.0),
             tree.leaf(S, "lantern_knob", lambda P: sdf.sphere(P, down(7.2), 1.3 * k), Box(down(7.2) - 2 * k, down(7.2) + 2 * k), metal, bones, protect=1.0),
             cone_leaf(S, "lantern_roof", down(7.8), down(11.2), 1.6 * k, 5.0 * k, metal, bones, protect=1.0),
             tree.leaf(S, "lantern_rim", lambda P: sdf.cylinder(P, down(10.8), down(12.0), 5.3 * k, 0.4 * k), Box(down(12.0) - 6 * k, down(10.8) + 6 * k),
                       metal, bones, protect=1.0),
             tree.leaf(S, "lantern_glass", lambda P: sdf.cylinder(P, down(11.6), down(20.6), 4.4 * k, 1.0 * k), Box(down(20.6) - 5 * k, down(11.6) + 5 * k),
                       flame, bones, protect=1.0),
             tree.leaf(S, "lantern_base", lambda P: sdf.cylinder(P, down(20.3), down(22.9), 5.1 * k, 0.6 * k), Box(down(22.9) - 6 * k, down(20.3) + 6 * k),
                       metal, bones, protect=1.0)]
    return Union(parts, k=0.3 * k)


# ---------------------------------------------------------------------------------------------- the spectral bear
def spectral_bear(S, L, dims, spec):
    """The enormous spectral bear of crimson energy that his Inside body adds, rearing up out of his back and over him:
    a hunched mass rising from behind him to great shoulders, its head thrust forward above his with round ears, burning
    eyes under a scowl and a maw dropped open on its fangs; vast arms reaching forward either side of him, each paw
    hooked with long claws. All of it glows in its colour (spec's spectral), the eyes burning yellow. It stands
    spectralScale times his capsule's height, as his status grows the capsule; its arms ride his arms' bones, so their
    swing is its swipe, and the rest rides his spine."""
    tall = dims["full"] / spec["heightShare"] * spec["spectralScale"]
    q = tall / BEAR_HEIGHT
    energy = S.material("spectral", as_seen(spec["spectral"]), glow=True)
    deep = S.material("spectral_maw", as_seen(np.asarray(spec["spectral"]) * 0.35), glow=True)
    eyes = S.material("spectral_eyes", as_seen(SPECTRAL_EYES), glow=True)
    fang = S.material("spectral_fang", as_seen(SPECTRAL_FANG))
    lower, upper = anatomy.rigid("spine_01"), anatomy.rigid("spine_03")
    at = lambda x, y, z: V(x, y, z) * q  # noqa: E731
    mass = [ellipsoid_leaf(S, "bear_root", at(-36, 0, 74), at(22, 28, 38), energy, lower),
            ellipsoid_leaf(S, "bear_body", at(-48, 0, 122), at(38, 50, 52), energy, lower),
            ellipsoid_leaf(S, "bear_shoulders", at(-40, 0, 166), at(36, 70, 32), energy, upper),
            ellipsoid_leaf(S, "bear_hump", at(-56, 0, 184), at(28, 40, 22), energy, upper),
            cone_leaf(S, "bear_neck", at(-30, 0, 178), at(0, 0, 196), 30 * q, 27 * q, energy, upper),
            ellipsoid_leaf(S, "bear_head", at(8, 0, 202), at(31, 30, 27), energy, upper),
            cone_leaf(S, "bear_snout", at(24, 0, 198), at(54, 0, 191), 16 * q, 10.5 * q, energy, upper),
            cone_leaf(S, "bear_jaw", at(14, 0, 180), at(46, 0, 166), 12.5 * q, 7.5 * q, energy, upper)]
    for sign in (1.0, -1.0):
        ear_axes = sdf.frame(V(1.0, sign * 0.5, 0.2), (0, 0, 1))
        mass.append(ellipsoid_leaf(S, "bear_ear", at(-10, sign * 24, 224), at(11, 11, 5), energy, upper, ear_axes))
    for side, sign in (("l", 1.0), ("r", -1.0)):
        shoulder, elbow, paw = at(-34, sign * 60, 168), at(-6, sign * 84, 128), at(46, sign * 82, 102)
        mass += [cone_leaf(S, "bear_arm_" + side, shoulder, elbow, 22 * q, 17 * q, energy, anatomy.rigid("upperarm_" + side)),
                 cone_leaf(S, "bear_forearm_" + side, elbow, paw, 17 * q, 15 * q, energy, anatomy.rigid("lowerarm_" + side)),
                 ellipsoid_leaf(S, "bear_paw_" + side, paw + at(6, 0, -2), at(18, 16, 13), energy, anatomy.rigid("lowerarm_" + side))]
    body = Union(mass, k=9.0 * q)
    details = [ellipsoid_leaf(S, "bear_maw", at(32, 0, 181), at(15, 10, 7), deep, upper, protect=0.6),
               ellipsoid_leaf(S, "bear_nose", at(63, 0, 193), at(5, 7, 4.5), deep, upper, protect=0.6)]
    for sign in (1.0, -1.0):
        # A scowl: each brow low over the inner corner of its eye, rising outward (laid on the head, not blended into
        # it, so the broad blend of its mass does not smooth it away).
        details.append(cone_leaf(S, "bear_brow", at(35.5, sign * 4, 211.5), at(29.5, sign * 19, 214.5), 3.8 * q, 3.0 * q, energy, upper, protect=0.6))
        # Set half into the head's surface under the brow, slanting down toward the snout.
        details.append(ellipsoid_leaf(S, "bear_eye", at(35.3, sign * 12.5, 207.5), at(3.6, 5.8, 3.0), eyes, upper, sdf.rotation(roll=sign * 20.0),
                                      protect=0.9))
        # Fangs: two long ones down from its upper jaw and a shorter pair up from its lower.
        for j, (x, y) in enumerate(((50, 5.5), (40, 8.5))):
            root = at(x, sign * y, 184)
            details.append(cone_leaf(S, "bear_fang", root, root - at(0, 0, 11 - 3 * j), 2.4 * q, 0.4 * q, fang, upper, protect=0.8))
        root = at(42, sign * 5.0, 171)
        details.append(cone_leaf(S, "bear_tusk", root, root + at(1, 0, 7), 2.0 * q, 0.4 * q, fang, upper, protect=0.8))
    for side, sign in (("l", 1.0), ("r", -1.0)):
        paw = at(46, sign * 82, 102)
        for j in range(4):
            base = paw + at(18, sign * (-9 + 6 * j), -6)
            bend = base + at(10, sign * (j - 1.5) * 1.2, -4)
            tip = bend + at(6, sign * (j - 1.5) * 1.0, -14)
            bones = anatomy.rigid("lowerarm_" + side)
            details.append(cone_leaf(S, "bear_claw", base, bend, 3.4 * q, 2.4 * q, fang, bones, protect=0.8))
            details.append(cone_leaf(S, "bear_claw_tip", bend, tip, 2.4 * q, 0.4 * q, fang, bones, protect=0.8))
    return Over([body, Union(details, k=1.5 * q)])
