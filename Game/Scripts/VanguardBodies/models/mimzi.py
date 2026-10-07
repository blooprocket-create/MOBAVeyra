"""Mimzi, The Pocket Hex (ADR-069): a tiny fennec-fox Bramblekin trinket mage, read by her silhouette from the game
camera. Character Bible §21 and her splash art: an animal, never a person with fox ears; enormous pale fennec ears
standing up through an enormous teal hood, brass rivets at its temples and its point falling behind; a short-muzzled fox
face in pale fur with big amber eyes; a long red scarf, the one hot colour on her, wound at her throat with one end on her
breast and the other trailing behind; a teal coat with a flaring skirt, a brown working harness across it (buckled
straps, a belt, pouches) and a trinket satchel on her right hip hung with a cut blue crystal and a brass cog; fingerless
gloves on paw-like hands, her own pale fur tufting at the wrists and over heavy boots on digitigrade feet; and a big
fluffy brush of a tail. The Tinkertwins, two floating brass rings each set with a cut blue crystal inside a ring of its
own light, orbit her hands rather than being held.

Low poly and flat-coloured (author 2026-10-07): the big forms that make her outline, each a flat colour the toon
material shades; her head and ears fitted large for a high camera. She rests empty-handed in the A pose, a ring at each
hand on its prop bone. Her coat's skirt hangs on the skirt chains and her scarf's trailing end on the cloak's left chain
(ADR-069 §7); her tail rides the tail bones. Colours are her kit's, lifted toward the art where it is lit for evening."""
import numpy as np

from ..sculpt import anatomy, garments, sdf, sheet, tree
from ..sculpt.anatomy import V, unit
from ..sculpt.garments import band_z, both, either, folds, without
from ..sculpt.tree import Box, Over, Placed, Shell, Union, Zone

# The paw, wrist to fingertip, as a share of the height: big, so her hands read at a distance.
HAND_SHARE = 0.11
# Her head and hood drawn this much larger than her head bone (her art's head, with its hood, is wider than her
# shoulders; the layout keeps the kit's share for her neck and the clips).
HEAD_SCALE = 1.25

# Her kit's colours (fur, cream, ear, eyes, teal, gloves, boots, scarf, accent), the dark leathers and her trousers lifted so
# they read as brown, not black, under the toon light; the rest sampled from her art (sRGB).
PALETTE = {
    "fur": (0.88, 0.75, 0.58), "cream": (0.98, 0.94, 0.86), "ear_inner": (0.93, 0.6, 0.56), "eye": (0.95, 0.58, 0.14),
    "pupil": (0.1, 0.06, 0.05), "nose": (0.6, 0.33, 0.33), "teal": (0.14, 0.5, 0.52), "teal_dark": (0.1, 0.36, 0.38),
    "lining": (0.12, 0.3, 0.32), "shirt": (0.32, 0.22, 0.16), "trousers": (0.34, 0.25, 0.21), "leather": (0.44, 0.28, 0.16),
    "leather_dark": (0.32, 0.2, 0.12), "boot": (0.4, 0.26, 0.15), "sole": (0.24, 0.16, 0.1), "glove": (0.36, 0.24, 0.14),
    "scarf": (0.76, 0.14, 0.12), "brass": (0.88, 0.66, 0.28), "crystal": (0.35, 0.7, 1.0), "light": (0.55, 0.82, 1.0),
}


def materials(S):
    """Every material Mimzi is coloured in, flat (the toon material shades it); the rings' crystals and light glow."""
    return {name: S.material(name, colour, glow=name in ("crystal", "light")) for name, colour in PALETTE.items()}


def build(S, L, dims, spec):
    """Mimzi's sculpt on her layout: (the whole, {"body": her furred figure, under her clothes, "sheets": her coat's skirt
    and her scarf's trailing end})."""
    mats = materials(S)
    H = dims["height"]
    # Small and soft: a narrow chest over round hips, plump limbs with no muscle to them.
    figure = anatomy.Figure(S, L, dims, mats["fur"], {"muscle": 0.0, "chest": 0.85, "breadth": 0.9, "hips": 1.1, "limb": 1.15,
                                                       "deltoid": 0.5, "neck": 0.9, "pecs": 0.3}).build()
    # Her legs are an animal's, on her toes: the figure's are replaced by thigh, shank and foot along her bones.
    for side in ("l", "r"):
        figure.parts.remove(figure.limbs["leg_" + side])
        figure.limbs["leg_" + side] = digitigrade_leg(S, L, H, side, mats)
        figure.parts.append(figure.limbs["leg_" + side])
    figure.attach_head(head(S, L, dims, mats))
    for side in ("l", "r"):
        figure.limbs["hand_" + side] = paw(S, L, H, side, mats, figure)
    body = figure.body()
    dressed, under_scarf = clothes(S, L, dims, mats, body, figure.limbs)
    worn = Over([dressed, gear(S, L, dims, mats, dressed), tail(S, L, H, mats)] + [ring(S, L, H, mats, side) for side in ("l", "r")])
    # Cut flat where she meets the ground: nothing of her sinks below it.
    worn = tree.Intersect(worn, Zone(lambda P: -P[:, 2], Box(V(-500, -500, -0.5), V(500, 500, 600))))
    sheets = [piece for side in ("l", "r") for piece in coat_skirt(S, L, dims, mats, dressed, side)] + [scarf_tail(S, L, dims, mats, under_scarf)]
    return worn, {"body": body, "sheets": sheets}


# ---------------------------------------------------------------------------------------------- helpers
def flat_tube(P, points, radii, across, flatten):
    """A tube through points (all square to across from the first) squashed across by flatten: an ear, a scarf's end.
    Its distance is the squashed tube's divided by flatten, so it never reads farther than the true surface."""
    a0 = np.asarray(points[0], dtype=np.float64)
    Q = P - a0
    Q2 = Q + np.outer(Q @ across, across) * (flatten - 1.0)
    return sdf.tube(Q2 + a0, points, radii) / flatten


def octahedron(P, c, up, across, height, width):
    """A cut crystal: a long octahedron at c, height up and down along up, width across and square to it (a bound on the
    distance, so it never reads farther than its true surface)."""
    Q = P - c
    forward = np.cross(up, across)
    q = np.abs(Q @ up) / height + np.abs(Q @ across) / width + np.abs(Q @ forward) / width
    return (q - 1.0) * min(height, width) * 0.57735


def leaf(S, name, distance, points, pad, material, bones, protect=0.0):
    """A part bounded by the box round points, grown by pad."""
    return tree.leaf(S, name, distance, Box.around(points, pad), material, bones, protect)


def clear_outward(points, limbs, margin, step=0.4, steps=60):
    """points moved straight out from her middle (level, away from the vertical axis) until each is clear of every limb
    (a segment and its radius) by margin: a skirt flaring round a knee without folding back on itself."""
    P = np.array(points, dtype=np.float64)
    out = P * np.array([1.0, 1.0, 0.0])
    out /= np.maximum(np.linalg.norm(out, axis=1, keepdims=True), 1e-6)
    for _ in range(steps):
        inside = np.zeros(len(P), dtype=bool)
        for a, b, radius in limbs:
            ab = b - a
            t = np.clip(((P - a) @ ab) / (ab @ ab), 0, 1)
            inside |= np.linalg.norm(P - (a + t[:, None] * ab), axis=1) < radius + margin
        if not inside.any():
            break
        P[inside] += out[inside] * step
    return P


# ---------------------------------------------------------------------------------------------- head
def head(S, L, dims, mats):
    """Her head in its own frame (x forward, z up, from the head joint), larger than a person's on her small frame, as
    her art draws it: a fox's face (a round tan brow over pale cheeks and a short pointed pale muzzle, a pink nose, big
    amber eyes set wide, a fox's slit pupils); the enormous teal hood over it, its opening framing the face, a heavy rolled brim, a brass rivet
    at each temple and its point falling behind; and the enormous ears standing up and out through it, pale fur behind
    and pink within."""
    o = V(*L["head"][0])
    # Drawn larger than her head bone, so her head and hood read as her art draws them, the widest thing about her bar
    # her ears.
    s = (L["head"][1][2] - L["head"][0][2]) * HEAD_SCALE
    bones = anatomy.rigid("head")
    m = mats

    def e(name, c, r, material, protect=0.8):
        c, r = V(*c) * s, V(*r) * s
        return leaf(S, name, lambda P: sdf.ellipsoid(P, c, r), [c], r.max(), material, bones, protect)

    def cone(name, a, b, ra, rb, material, protect=0.8):
        a, b = V(*a) * s, V(*b) * s
        return leaf(S, name, lambda P: sdf.round_cone(P, a, b, ra * s, rb * s), [a, b], max(ra, rb) * s, material, bones, protect)

    # The face: a round tan skull, round pale cheeks below the eyes either side of a short pointed pale muzzle, a pale
    # chin; tufts of pale cheek fur flaring out under the hood's sides.
    forms = [e("cranium", (-0.02, 0, 0.52), (0.34, 0.38, 0.35), m["fur"]),
             cone("muzzle", (0.2, 0, 0.3), (0.47, 0, 0.27), 0.12, 0.045, m["cream"]),
             e("chin", (0.22, 0, 0.17), (0.13, 0.12, 0.08), m["cream"])]
    for sign in (1.0, -1.0):
        forms.append(e("cheek_%d" % (sign > 0), (0.12, sign * 0.2, 0.28), (0.19, 0.19, 0.14), m["cream"]))
        forms.append(cone("ruff_low_%d" % (sign > 0), (0.1, sign * 0.3, 0.26), (0.03, sign * 0.5, 0.17), 0.075, 0.012, m["cream"]))
        forms.append(cone("ruff_high_%d" % (sign > 0), (0.12, sign * 0.3, 0.34), (0.05, sign * 0.49, 0.37), 0.06, 0.01, m["cream"]))
    face = Union(forms, k=0.05 * s)
    # Big round amber eyes set wide on the skull, each a disc standing a little proud of it (a disc's round edge survives
    # the reduction where a small ellipsoid's collapses to a diamond), an oval pupil and a catch of light on it.
    cranium_c, cranium_r = V(-0.02, 0, 0.52) * s, V(0.34, 0.38, 0.35) * s
    eyes, pupils = [], []
    for sign in (1.0, -1.0):
        d = V(0.86, sign * 0.5, -0.06)
        p = cranium_c + d / np.linalg.norm(d / cranium_r)
        # Turned up a little toward the high camera.
        n = unit(unit(d / cranium_r ** 2) + V(0, 0, 0.15))
        side = unit(np.cross(V(0, 0, 1), n))
        up = np.cross(n, side)
        frame = np.stack([n, side, up], axis=1)
        a, b = p - n * 0.04 * s, p + n * 0.025 * s
        eyes.append(leaf(S, "eye_%d" % (sign > 0), lambda P, a=a, b=b: sdf.cylinder(P, a, b, 0.09 * s, 0.012 * s), [a, b], 0.1 * s, m["eye"], bones, 1.0))
        # The pupil domes well proud of the iris, so the reduction keeps its round edge.
        pc = p + n * 0.025 * s
        pupils.append(leaf(S, "pupil_%d" % (sign > 0), lambda P, c=pc, f=frame: sdf.ellipsoid(P, c, V(0.024, 0.036, 0.05) * s, f), [pc], 0.06 * s,
                           m["pupil"], bones, 1.0))
    nose = e("nose", (0.49, 0, 0.285), (0.04, 0.05, 0.035), m["nose"], 1.0)
    fox = Over([face, Union(eyes), Union(pupils), nose])

    # The hood: a dome over the skull, its point falling behind, and its back falling to her nape where its drape takes
    # over; its opening cut from the front, lined in shadow.
    dome_c, dome_r = V(-0.08, 0, 0.56) * s, V(0.5, 0.58, 0.52) * s
    dome = leaf(S, "hood_dome", lambda P: sdf.ellipsoid(P, dome_c, dome_r), [dome_c], dome_r.max(), m["teal"], bones, 0.3)
    peak = cone("hood_peak", (-0.25, 0, 0.88), (-0.85, 0, 0.32), 0.2, 0.04, m["teal"], 0.3)
    fall_a, fall_b = V(-0.1, 0, 0.3) * s, V(-0.08, 0, -0.08) * s
    fall_shape = leaf(S, "hood_fall", lambda P: sdf.round_cone(P, fall_a, fall_b, 0.38 * s, 0.3 * s), [fall_a, fall_b], 0.4 * s, m["teal"],
                      anatomy.along("head", "spine_03", o + V(0, 0, 0.25) * s, o + V(0, 0, -0.08) * s, 0.0, 1.0), 0.3)
    fall = tree.Intersect(fall_shape, Zone(lambda P: P[:, 0] - 0.05 * s, Box(V(-1, -1, -0.6) * s, V(0.05, 1, 0.8) * s)))
    # Cut high enough that her eyes show from above under the brim.
    open_c, open_r = V(0.42, 0, 0.43) * s, V(0.34, 0.4, 0.4) * s
    opening = leaf(S, "hood_opening", lambda P: sdf.ellipsoid(P, open_c, open_r), [open_c], open_r.max(), m["lining"], bones)
    cowl = tree.Subtract(Union([dome, peak, fall], k=0.12 * s), opening, k=0.03 * s, label=opening.part.label)
    # The brim: a heavy roll round the opening's edge.
    rim = Zone(lambda P: np.maximum(np.abs(sdf.ellipsoid(P, open_c, open_r)) - 0.045 * s, np.abs(sdf.ellipsoid(P, dome_c, dome_r)) - 0.06 * s),
               Box(open_c - open_r - 0.1 * s, open_c + open_r + 0.1 * s))
    brim = Shell(S, "hood_brim", cowl, 0.0, 0.04 * s, rim, m["teal_dark"], hem=0.02 * s, bones=bones)
    # A brass rivet at each temple by the brim, a smaller one under it: flat discs with a raised boss, all brass (a dark
    # centre would read as another eye).
    rivets = []
    for sign in (1.0, -1.0):
        for k, (d, radius) in enumerate(((V(0.62, sign * 0.75, 0.3), 0.075), (V(0.7, sign * 0.72, 0.02), 0.045))):
            g = d / dome_r
            p = dome_c + d / np.linalg.norm(g)
            n = unit(d / dome_r ** 2)
            a, b = p - n * 0.01 * s, p + n * 0.03 * s
            hub_b = b + n * 0.02 * s
            disc = leaf(S, "rivet_%d_%d" % (k, sign > 0), lambda P, a=a, b=b, r=radius * s: sdf.cylinder(P, a, b, r, 0.012 * s), [a, b], radius * s,
                        m["brass"], bones, 0.6)
            boss = leaf(S, "rivet_boss_%d_%d" % (k, sign > 0), lambda P, a=b, b=hub_b, r=radius * 0.45 * s: sdf.cylinder(P, a - (b - a), b, r, 0.008 * s), [b, hub_b],
                        radius * s, m["brass"], bones, 0.6)
            rivets.append(Union([disc, boss], k=0.01 * s))
    hood = Over([cowl, brim, Union(rivets)])
    ears = [ear(S, s, sign, m, bones) for sign in (1.0, -1.0)]
    return Placed(Over([fox, hood] + ears), o, np.eye(3))


def ear(S, s, sign, m, bones):
    """One enormous fennec ear (in the head's frame), standing up and out through the hood: broad at its root, leaf-shaped
    and pointed, pale fur behind and round its rim, its cup facing forward and pink within."""
    root = V(-0.08, sign * 0.38, 0.86) * s
    a = unit(V(-0.18, sign * 0.68, 0.72))
    forward = unit(V(1.0, sign * 0.3, 0.0))
    c = unit(forward - a * (forward @ a))
    length = 1.05 * s
    points = [root, root + a * length * 0.33, root + a * length]
    radii = [0.27 * s, 0.3 * s, 0.025 * s]
    flatten = 4.0
    side = "l" if sign > 0 else "r"
    shell = leaf(S, "ear_" + side, lambda P: flat_tube(P, points, radii, c, flatten), points, max(radii), m["cream"], bones, 0.6)
    cup_points = [root + a * length * 0.06 + c * 0.05 * s, root + a * length * 0.33 + c * 0.05 * s, root + a * length * 0.92 + c * 0.05 * s]
    cup_radii = [0.21 * s, 0.24 * s, 0.012 * s]
    cup = leaf(S, "ear_inner_" + side, lambda P: flat_tube(P, cup_points, cup_radii, c, flatten), cup_points, max(cup_radii), m["ear_inner"], bones, 0.6)
    return tree.Subtract(shell, cup, k=0.015 * s, label=cup.part.label)


# ---------------------------------------------------------------------------------------------- limbs
def digitigrade_leg(S, L, H, side, mats):
    """An animal's leg on its toes along side's bones, in her trousers: a stout thigh forward to the knee, a shank back to
    the high hock, and a short foot down to the toes."""
    hp, k = V(*L["thigh_" + side][0]), V(*L["thigh_" + side][1])
    hock, toe = V(*L["calf_" + side][1]), V(*L["foot_" + side][1])
    parts = [tree.leaf(S, "thigh_" + side, lambda P: sdf.round_cone(P, hp + V(0, 0, H * 0.02), k, H * 0.05, H * 0.036), Box.around([hp, k], H * 0.07),
                       mats["trousers"], anatomy.along("pelvis", "thigh_" + side, hp + V(0, 0, H * 0.05), hp - V(0, 0, H * 0.06), 0.2, 0.7)),
             tree.leaf(S, "shank_" + side, lambda P: sdf.round_cone(P, k, hock, H * 0.033, H * 0.024), Box.around([k, hock], H * 0.04),
                       mats["trousers"], anatomy.rigid("calf_" + side)),
             tree.leaf(S, "foot_" + side, lambda P: sdf.round_cone(P, hock, toe, H * 0.024, H * 0.021), Box.around([hock, toe], H * 0.03),
                       mats["trousers"], anatomy.rigid("foot_" + side))]
    return Union(parts, k=H * 0.018)


def paw(S, L, H, side, mats, figure):
    """A paw-like hand hanging open beside her thigh, palm in and thumb forward: a broad pad of a palm, four short thick
    digits curled a little, an opposable thumb; a fingerless glove over the palm and the digits' first joints, her fur at
    their tips."""
    w0, w1 = V(*L["hand_" + side][0]), V(*L["hand_" + side][1])
    sign = 1.0 if side == "l" else -1.0
    x = unit(w1 - w0)
    out = V(0, sign, 0)
    z = unit(out - x * (out @ x))
    y = np.cross(z, x)
    n = H * HAND_SHARE
    s = -sign
    bones = anatomy.rigid("hand_" + side)
    glove, tip = mats["glove"], mats["fur"]
    parts = [tree.leaf(S, "palm_" + side, lambda P: sdf.ellipsoid(P, V(0.32, 0, 0) * n, V(0.3, 0.27, 0.16) * n), Box(V(-0.1, -0.4, -0.3) * n, V(0.7, 0.4, 0.3) * n),
                       glove, bones, 1.0),
             tree.leaf(S, "wrist_" + side, lambda P: sdf.ellipsoid(P, V(0.04, 0, 0) * n, V(0.17, 0.23, 0.17) * n), Box(V(-0.3, -0.3, -0.3) * n, V(0.3, 0.3, 0.3) * n),
                       glove, bones, 1.0)]
    for k, across in enumerate((0.17, 0.06, -0.05, -0.16)):
        base = V(0.55, s * across, 0.02) * n
        spread = s * across * 0.6
        knuckle = base + unit(V(1.0, spread, -0.35)) * 0.19 * n
        end = knuckle + unit(V(0.8, spread, -1.1)) * 0.16 * n
        parts.append(tree.leaf(S, "digit_%s_%d" % (side, k), lambda P, a=base, b=knuckle: sdf.round_cone(P, a, b, 0.08 * n, 0.07 * n), Box.around([base, knuckle], 0.1 * n),
                               glove, bones, 1.0))
        parts.append(tree.leaf(S, "digit_tip_%s_%d" % (side, k), lambda P, a=knuckle, b=end: sdf.round_cone(P, a, b, 0.068 * n, 0.055 * n), Box.around([knuckle, end], 0.1 * n),
                               tip, bones, 1.0))
    t0, t1, t2 = V(0.2, s * 0.2, -0.06) * n, V(0.38, s * 0.34, -0.14) * n, V(0.52, s * 0.36, -0.24) * n
    parts.append(tree.leaf(S, "thumb_" + side, lambda P: sdf.round_cone(P, t0, t1, 0.09 * n, 0.075 * n), Box.around([t0, t1], 0.12 * n), glove, bones, 1.0))
    parts.append(tree.leaf(S, "thumb_tip_" + side, lambda P: sdf.round_cone(P, t1, t2, 0.07 * n, 0.055 * n), Box.around([t1, t2], 0.1 * n), tip, bones, 1.0))
    placed = Placed(Union(parts, k=0.04 * n), w0, np.stack([x, y, z], axis=1))
    figure.parts.append(placed)
    return placed


# ---------------------------------------------------------------------------------------------- clothes
def clothes(S, L, dims, mats, body, limbs):
    """Her clothes over her fur, each keeping to its limbs: trousers at her hips, heavy boots over her feet and shanks
    with her fur tufting out over their tops; a dark shirt; the teal coat over the torso and down her arms, open at the
    breast, gold at the cuffs, her fur tufting out at the wrists; the hood's teal drape over her shoulders; a belt and
    the harness straps crossed over her breast, buckled in brass; and the red scarf wound thick at her throat, knotted
    at her left, its short end on her breast."""
    H = dims["height"]
    pz = L["pelvis"][0][2]
    cz = L["spine_03"][1][2]
    torso = cz - pz
    belt_z = pz + torso * 0.12
    trousers = Shell(S, "trousers", body, 0.25, 0.5, both(band_z(pz - H * 0.08, belt_z + H * 0.01), garments.keep_to(limbs, ["torso", "leg_l", "leg_r"])),
                     mats["trousers"], hem=0.3, reach=0.5)
    lower = Over([body, trousers] + [boot(S, L, H, side, mats) for side in ("l", "r")])
    shirt = Shell(S, "shirt", lower, 0.2, 0.4, both(band_z(belt_z - H * 0.01, cz + H * 0.01), garments.keep_to(limbs, ["torso"])), mats["shirt"], hem=0.3)
    # The coat: over the torso and down both arms to near the wrist, open in a V at the breast over the shirt.
    v_cut = Zone(lambda P: np.maximum(np.maximum(2.0 - P[:, 0], np.abs(P[:, 1]) - np.clip((P[:, 2] - (cz - torso * 0.45)) * 0.32, 0.5, 6.0)),
                                      (cz - torso * 0.45) - P[:, 2]), Box((0, -12, cz - torso * 0.5), (40, 12, cz + 12)))
    sleeves = []
    for side in ("l", "r"):
        e, w = V(*L["lowerarm_" + side][0]), V(*L["lowerarm_" + side][1])
        sleeves.append(both(garments.keep_to(limbs, ["forearm_" + side]), garments.half(e + (w - e) * 0.8, w - e, Box.around([e, w], H * 0.08))))
    coat_region = without(either(both(band_z(belt_z - H * 0.02, cz + H * 0.012), garments.keep_to(limbs, ["torso"])),
                                 garments.keep_to(limbs, ["upperarm_l", "upperarm_r"]), *sleeves), v_cut)
    coat = Shell(S, "coat", Over([lower, shirt]), 0.5, 1.0, coat_region, mats["teal"], hem=0.4, displace=folds((0, 0, 1), 9, 0.4, seed=211), reach=0.8)
    dressed = Over([lower, shirt, coat])
    # Gold cuffs at the sleeves' ends, and her fur tufting out below them at the wrists.
    extras = []
    for side in ("l", "r"):
        e, w = V(*L["lowerarm_" + side][0]), V(*L["lowerarm_" + side][1])
        along = unit(w - e)
        a, b = e + (w - e) * 0.68, e + (w - e) * 0.8
        extras.append(leaf(S, "cuff_" + side, lambda P, a=a, b=b: sdf.cylinder(P, a, b, H * 0.034, H * 0.005), [a, b], H * 0.04, mats["brass"], anatomy.rigid("lowerarm_" + side), 0.4))
        a, b = e + (w - e) * 0.8, w + along * H * 0.01
        extras.append(leaf(S, "wrist_fur_" + side, lambda P, a=a, b=b: sdf.round_cone(P, a, b, H * 0.03, H * 0.036), [a, b], H * 0.04, mats["cream"],
                           anatomy.rigid("lowerarm_" + side), 0.4))
    dressed = Over([dressed, Union(extras)])
    # The hood's drape over her shoulders and upper arms, open at the breast where the scarf lies.
    shoulder = abs(L["upperarm_l"][0][1])

    def drape_region(P):
        angle = np.arctan2(P[:, 1], P[:, 0])
        hem = cz - H * 0.06 - H * 0.045 * np.abs(np.sin(angle))
        front = (np.cos(angle) - 0.8) * 20.0
        return np.max(np.stack([hem - P[:, 2], P[:, 2] - (cz + H * 0.03), front]), axis=0)
    drape_zone = both(Zone(drape_region, Box((-30, -shoulder - 15, cz - H * 0.14), (30, shoulder + 15, cz + H * 0.04))),
                      garments.keep_to(limbs, ["torso", "upperarm_l", "upperarm_r"]))
    drape = Shell(S, "hood_drape", dressed, 0.3, 0.8, drape_zone, mats["teal"], hem=0.5, displace=folds((0, 0, 1), 10, 0.6, seed=212), reach=0.8)
    draped = Over([dressed, drape])
    # The belt, and the harness straps crossed over her breast from each shoulder to the other hip.
    belt = Shell(S, "belt", draped, 0.2, 0.9, both(band_z(belt_z - H * 0.014, belt_z + H * 0.014), garments.keep_to(limbs, ["torso"])), mats["leather"], hem=0.2)
    straps = []
    for sign in (1.0, -1.0):
        a, b = V(0, sign * (shoulder - 3.0), cz - 1.0), V(0, -sign * H * 0.09, belt_z)
        across = unit(np.cross(b - a, V(1, 0, 0)))
        band = both(garments.band_plane((a + b) / 2, across, H * 0.024, Box((-30, -40, belt_z - 4), (30, 40, cz + 4))), garments.keep_to(limbs, ["torso"]))
        straps.append(Shell(S, "strap_%d" % (sign > 0), draped, 0.2, 0.7, band, mats["leather_dark" if sign > 0 else "leather"], hem=0.2, reach=0.5))
    harnessed = Over([draped, belt] + straps)
    # Brass where the straps cross and on the belt's buckle.
    a_y, b_y = shoulder - 3.0, -H * 0.09
    cross_z = (cz - 1.0) + (belt_z - (cz - 1.0)) * a_y / (a_y - b_y)
    hits, normals = garments.surface_points(harnessed, np.array([[40.0, 0.0, cross_z], [40.0, 0.0, belt_z]]), np.array([[-1.0, 0, 0], [-1.0, 0, 0]]), reach=45.0)
    brass = []
    for k, (p, nrm) in enumerate(zip(hits, normals)):
        a, b = p - nrm * 0.3, p + nrm * 0.6
        brass.append(leaf(S, "buckle_%d" % k, lambda P, a=a, b=b: sdf.cylinder(P, a, b, H * (0.017 if k == 0 else 0.02), H * 0.003), [a, b], H * 0.03,
                          mats["brass"], anatomy.rigid("spine_02" if k == 0 else "pelvis"), 0.5))
    buckled = Over([harnessed, Union(brass)])
    return Over([buckled, scarf(S, L, dims, mats, buckled)]), buckled


def boot(S, L, H, side, mats):
    """A heavy brown boot on a digitigrade foot: a shaft over the lower shank, the foot's upper down to a rounded toe on a
    thick sole, a strap buckled in brass round the shaft; her own pale fur tufting out over its top."""
    k, hock = V(*L["calf_" + side][0]), V(*L["calf_" + side][1])
    toe = V(*L["foot_" + side][1])
    sign = 1.0 if side == "l" else -1.0
    shank = unit(hock - k)
    top = k + (hock - k) * 0.42
    foot_dir = unit(toe - hock)
    toe_end = toe + foot_dir * H * 0.012
    boot_bones = anatomy.rigid("calf_" + side)
    foot_bones = anatomy.rigid("foot_" + side)
    parts = [leaf(S, "boot_shaft_" + side, lambda P: sdf.round_cone(P, top, hock, H * 0.04, H * 0.035), [top, hock], H * 0.045, mats["boot"], boot_bones, 0.3),
             leaf(S, "boot_upper_" + side, lambda P: sdf.round_cone(P, hock, toe_end, H * 0.034, H * 0.03), [hock, toe_end], H * 0.04, mats["boot"], foot_bones, 0.3)]
    cap_c = V(toe_end[0] - H * 0.005, toe_end[1], H * 0.03)
    parts.append(leaf(S, "boot_toe_" + side, lambda P: sdf.ellipsoid(P, cap_c, V(0.042, 0.034, 0.027) * H), [cap_c], H * 0.045, mats["boot"], foot_bones, 0.3))
    upper = Union(parts, k=H * 0.012)
    # A thick sole under the ball of the foot, which bears her on her toes.
    sole_c = V(toe_end[0] - H * 0.01, toe_end[1], H * 0.008)
    sole = leaf(S, "boot_sole_" + side, lambda P: sdf.box(P, sole_c, V(0.035, 0.034, 0.008) * H, None, H * 0.004), [sole_c], H * 0.045, mats["sole"], foot_bones, 0.3)
    # The strap round the shaft and its buckle on the outside.
    sa, sb = k + (hock - k) * 0.68, k + (hock - k) * 0.76
    strap = leaf(S, "boot_strap_" + side, lambda P: sdf.cylinder(P, sa, sb, H * 0.04, H * 0.004), [sa, sb], H * 0.05, mats["leather_dark"], boot_bones, 0.3)
    bc = (sa + sb) / 2 + V(0, sign * H * 0.04, 0)
    buckle = leaf(S, "boot_buckle_" + side, lambda P: sdf.box(P, bc, V(0.012, 0.004, 0.012) * H, None, H * 0.002), [bc], H * 0.02, mats["brass"], boot_bones, 0.5)
    # Her fur over the boot's top: a thick ruff round the shank, tufts falling over the leather.
    fa, fb = k + (hock - k) * 0.3, k + (hock - k) * 0.46
    fur = [leaf(S, "boot_fur_" + side, lambda P: sdf.round_cone(P, fa, fb, H * 0.04, H * 0.046), [fa, fb], H * 0.05, mats["cream"], boot_bones, 0.3)]
    for j, degrees in enumerate((0.0, 70.0, 140.0, 220.0, 290.0)):
        r = np.radians(degrees)
        across = unit(np.cross(shank, V(0, 0, 1)))
        out = unit(np.cross(across, shank))
        radial = out * np.cos(r) + across * np.sin(r)
        root = fb + radial * H * 0.035
        tip_p = root + radial * H * 0.012 + shank * H * 0.03
        fur.append(leaf(S, "boot_tuft_%s_%d" % (side, j), lambda P, a=root, b=tip_p: sdf.round_cone(P, a, b, H * 0.016, H * 0.004), [root, tip_p], H * 0.02,
                        mats["cream"], boot_bones, 0.3))
    return Over([upper, sole, strap, buckle, Union(fur, k=H * 0.006)])


def scarf(S, L, dims, mats, under):
    """The long red scarf wound thick round her throat, under the hood, knotted at her left; its short end hangs over her
    breast (its long end trails behind on its own chain: scarf_tail)."""
    H = dims["height"]
    cz = L["spine_03"][1][2]
    bones = anatomy.along("spine_03", "neck_01", V(0, 0, cz - 2.0), V(0, 0, cz + 4.0), 0.3, 0.8)
    red = mats["scarf"]
    tilt = np.stack([unit(V(1, 0, -0.25)), V(0, 1, 0), unit(V(0.25, 0, 1))], axis=1)
    rolls = [leaf(S, "scarf_roll_0", lambda P: sdf.torus(P, V(0.6, 0, cz + 0.6), H * 0.054, H * 0.024, tilt), [V(0.6, 0, cz + 0.6)], H * 0.085, red, bones, 0.3),
             leaf(S, "scarf_roll_1", lambda P: sdf.torus(P, V(1.0, 0, cz + 3.2), H * 0.048, H * 0.02, tilt), [V(1.0, 0, cz + 3.2)], H * 0.075, red, bones, 0.3)]
    # The knot at her left, and the short end from it down over her breast, lying on what is under it.
    starts = np.array([[40.0, H * 0.035, cz + 0.5], [40.0, H * 0.052, cz - H * 0.07], [40.0, H * 0.064, cz - H * 0.14], [40.0, H * 0.072, cz - H * 0.21]])
    hits, _normals = garments.surface_points(under, starts, np.tile([-1.0, 0, 0], (len(starts), 1)), reach=45.0)
    knot_c = hits[0] + V(1.2, 0, 0)
    rolls.append(leaf(S, "scarf_knot", lambda P: sdf.ellipsoid(P, knot_c, V(0.026, 0.028, 0.024) * H), [knot_c], H * 0.03, red, bones, 0.4))
    end = [hits[0] + V(1.0, 0, -H * 0.01)] + [p + V(1.3, 0, 0) for p in hits[1:]]
    radii = [H * 0.02, H * 0.021, H * 0.022, H * 0.016]
    # Flat across its face, each length square to its own fall and to her side, so it lies over her breast's curve.
    for k in range(len(end) - 1):
        a, b = end[k], end[k + 1]
        across = unit(np.cross(b - a, V(0, 1, 0)))
        across = across if across[0] > 0 else -across
        rolls.append(leaf(S, "scarf_end_%d" % k, lambda P, a=a, b=b, ra=radii[k], rb=radii[k + 1], n=across: flat_tube(P, [a, b], [ra, rb], n, 3.0),
                          [a, b], H * 0.03, red, anatomy.rigid("spine_03"), 0.3))
    return Union(rolls, k=H * 0.012)


# ---------------------------------------------------------------------------------------------- gear
def gear(S, L, dims, mats, under):
    """Her working gear: the trinket satchel on her right hip under its buckled flap, a cut blue crystal charm and a brass
    cog hung from it; and two pouches on the belt at her left."""
    H = dims["height"]
    pz = L["pelvis"][0][2]
    belt_z = pz + (L["spine_03"][1][2] - pz) * 0.12
    bones = anatomy.rigid("pelvis")
    parts = []
    # The satchel rides low on the hip, standing off the coat's skirt; the pouches sit on the belt.
    for k, (degrees, drop, stand, size) in enumerate(((-60.0, 0.03, 0.025, (0.105, 0.08, 0.045)), (35.0, 0.004, 0.004, (0.045, 0.05, 0.028)),
                                                      (65.0, 0.004, 0.004, (0.04, 0.045, 0.026)))):
        angle = np.radians(degrees)
        centre = V(0.0, 0.0, belt_z - H * drop)
        c, normal = garments.surface_point(under, centre + V(np.cos(angle), np.sin(angle), 0.0) * 40.0, -V(np.cos(angle), np.sin(angle), 0.0), reach=45.0)
        out = unit(normal * V(1, 1, 0))
        c = c + out * H * stand
        axes = np.stack([unit(np.cross(V(0, 0, 1), out)), V(0, 0, 1), out], axis=1)
        flap = mats["leather_dark"]
        parts.append(garments.pouch(S, "satchel" if k == 0 else "pouch_%d" % k, c, axes, tuple(x * H for x in size), size[1] * H * 0.45,
                                    mats["leather"], flap, bones=bones))
        if k == 0:
            # The satchel's brass buckle, and the charm and cog hung from its front.
            front = c + out * (size[2] * H + 1.0)
            b0, b1 = front + V(0, 0, H * 0.012), front + V(0, 0, H * 0.012) + out * H * 0.006
            parts.append(leaf(S, "satchel_buckle", lambda P, a=b0, b=b1: sdf.cylinder(P, a, b, H * 0.012, H * 0.002), [b0, b1], H * 0.02, mats["brass"], bones, 0.6))
            # Each hangs on a short brass link from the satchel's bottom edge, so it stays one piece with it (a loose
            # charm this small would be lost to the reduction).
            side = unit(np.cross(V(0, 0, 1), out))
            bottom = c + out * size[2] * H * 0.8 - V(0, 0, size[1] * H * 0.5 - 0.6)
            gem = bottom + side * H * 0.022 - V(0, 0, H * 0.05)
            cog_c = bottom - side * H * 0.025 - V(0, 0, H * 0.03)
            for name, hang, top in (("charm_link", bottom + side * H * 0.022, gem + V(0, 0, H * 0.03)), ("cog_link", bottom - side * H * 0.025, cog_c)):
                parts.append(leaf(S, name, lambda P, a=hang, b=top: sdf.capsule(P, a, b, H * 0.0045), [hang, top], H * 0.01, mats["brass"], bones, 0.6))
            parts.append(leaf(S, "charm", lambda P, c=gem: octahedron(P, c, V(0, 0, 1), side, H * 0.036, H * 0.021), [gem], H * 0.04, mats["crystal"], bones, 0.9))
            ca, cb = cog_c - out * H * 0.004, cog_c + out * H * 0.004
            parts.append(leaf(S, "cog", lambda P, a=ca, b=cb: sdf.cylinder(P, a, b, H * 0.018, H * 0.002), [ca, cb], H * 0.025, mats["brass"], bones, 0.6))
    return Union(parts, k=0.3)


def tail(S, L, H, mats):
    """Her big brush of a tail from the base of her spine: back and out to her right and up in a sweep, as her art carries
    it, swelling to a great pale brush and curling up at its tip, tan at its root, so it shows beside her from the front
    as well as behind. Each length rides its own tail bone."""
    root = V(*L["tail_01"][0])
    # Its line from the root, as shares of her height (back, right, up), and its radius along it.
    line = [(0.01, 0.0, 0.01), (-0.07, -0.04, -0.03), (-0.15, -0.13, 0.0), (-0.19, -0.22, 0.09), (-0.19, -0.27, 0.2), (-0.15, -0.27, 0.29)]
    points = [root + V(*p) * H for p in line]
    radii = [r * H for r in (0.028, 0.055, 0.09, 0.1, 0.076, 0.012)]
    parts = []
    for k in range(len(points) - 1):
        a, b, ra, rb = points[k], points[k + 1], radii[k], radii[k + 1]
        bone = "tail_%02d" % min(k + 1, 3)
        parts.append(leaf(S, "tail_%d" % k, lambda P, a=a, b=b, ra=ra, rb=rb: sdf.round_cone(P, a, b, ra, rb), [a, b], max(ra, rb),
                          mats["fur"] if k < 2 else mats["cream"], anatomy.rigid(bone), 0.2))
        if k >= 2:
            # Tufts of the brush standing out from its back and sides, so its outline reads as fur.
            along = unit(b - a)
            top = -unit(np.cross(along, V(0, 1, 0)))
            flank = unit(np.cross(along, top))
            for j, (share, turn) in enumerate(((0.3, 0.0), (0.75, 1.6), (0.55, -1.6))):
                radial = unit(top * np.cos(turn) + flank * np.sin(turn))
                r = ra + (rb - ra) * share
                root_p = a + (b - a) * share + radial * r * 0.6
                tip_p = root_p + radial * r * 0.55 + along * r * 0.35
                parts.append(leaf(S, "tail_tuft_%d_%d" % (k, j), lambda P, p=root_p, q=tip_p, rr=r * 0.45: sdf.round_cone(P, p, q, rr, rr * 0.12), [root_p, tip_p], r * 0.5,
                                  mats["cream"], anatomy.rigid(bone), 0.2))
    return Union(parts, k=H * 0.03)


def ring(S, L, H, mats, side):
    """One of the Tinkertwins orbiting her hand on its prop bone: a brass ring with a gimbal crossed inside it, a cut blue
    crystal set at its heart, turning inside a ring of its own light. Tilted back to the camera and as broad as her head,
    so from above it reads as a ring."""
    sign = 1.0 if side == "l" else -1.0
    bones = anatomy.rigid("prop_" + side)
    centre = V(*L["hand_" + side][1]) + V(H * 0.085, sign * H * 0.03, H * 0.06)
    across = V(0, 1, 0)
    up = unit(V(-0.5, 0, 0.87))
    normal = np.cross(across, up)
    radius = H * 0.085
    face_on = np.stack([across, up, normal], axis=1)
    edge_on = np.stack([normal, up, across], axis=1)
    parts = [leaf(S, "ring_" + side, lambda P: sdf.torus(P, centre, radius, H * 0.009, face_on), [centre], radius * 1.1, mats["brass"], bones, 0.7),
             leaf(S, "gimbal_" + side, lambda P: sdf.torus(P, centre, radius * 0.72, H * 0.006, edge_on), [centre], radius * 0.8, mats["brass"], bones, 0.7)]
    # The crystal, set between the gimbal's top and bottom.
    for k, sgn in enumerate((1.0, -1.0)):
        a, b = centre + up * sgn * radius * 0.3, centre + up * sgn * radius * 0.72
        parts.append(leaf(S, "setting_%s_%d" % (side, k), lambda P, a=a, b=b: sdf.capsule(P, a, b, H * 0.005), [a, b], H * 0.01, mats["brass"], bones, 0.7))
    parts.append(leaf(S, "crystal_" + side, lambda P: octahedron(P, centre, up, across, radius * 0.5, radius * 0.36), [centre], radius * 0.55, mats["crystal"], bones, 1.0))
    held = Union(parts, k=H * 0.003)
    light = leaf(S, "ring_light_" + side, lambda P: sdf.torus(P, centre, radius * 1.25, H * 0.0055, face_on), [centre], radius * 1.35, mats["light"], bones, 0.6)
    return Over([held, light])


# ---------------------------------------------------------------------------------------------- cloth sheets
def coat_skirt(S, L, dims, mats, dressed, side):
    """One half of her coat's flaring skirt: from the belt to below her knees, open at the front over her legs and at the
    back where her tail comes out, flaring wide as it falls, a band of gold trim round its hem. Each half hangs on its
    side's front and back skirt chains (ADR-069 §7). Returns [the skirt, its trim]."""
    H = dims["height"]
    pz = L["pelvis"][0][2]
    belt_z = pz + (L["spine_03"][1][2] - pz) * 0.12
    columns, rows = 10, 4
    t0, t1 = (np.radians(34.0), np.radians(167.0)) if side == "l" else (np.radians(193.0), np.radians(326.0))
    hem_z = H * 0.15
    # Its top: round her at the belt, where a ray in toward her middle first meets it, a little inside the belt (which
    # covers the seam) and outside the coat.
    samples = np.linspace(0.0, 1.0, 9)
    theta_s = t0 + (t1 - t0) * samples
    inward = -np.stack([np.cos(theta_s), np.sin(theta_s), np.zeros_like(theta_s)], axis=1)
    centres = np.stack([np.zeros_like(theta_s), np.zeros_like(theta_s), np.full_like(theta_s, belt_z)], axis=1)
    hits, _normals = garments.surface_points(dressed, centres - inward * 45.0, inward, reach=45.0)
    tops = hits + inward * 0.6
    limbs = []
    for s in ("l", "r"):
        hp, k = V(*L["thigh_" + s][0]), V(*L["thigh_" + s][1])
        limbs += [(hp, k, H * 0.055), (k, V(*L["calf_" + s][1]), H * 0.048)]
    phase = np.random.default_rng(213 if side == "l" else 214).uniform(0, 2 * np.pi, 2)

    def position(u, v, proud=0.0):
        theta = t0 + (t1 - t0) * u
        top = np.stack([np.interp(u, samples, tops[:, k]) for k in range(3)], axis=1)
        radial = np.stack([np.cos(theta), np.sin(theta), np.zeros_like(theta)], axis=1)
        flare = H * (0.07 + 0.05 * np.abs(np.sin(theta))) * v ** 1.2
        pleat = (0.3 + 1.2 * v) * (0.6 * np.sin(theta * 8.0 + phase[0]) + 0.4 * np.sin(theta * 13.0 + phase[1]))
        p = top + radial * (flare + pleat + proud)[:, None]
        p[:, 2] = top[:, 2] + (hem_z - top[:, 2]) * v
        return clear_outward(p, limbs, 1.2 + proud)
    names = ("skirt_fl", "skirt_bl") if side == "l" else ("skirt_br", "skirt_fr")
    chains = {name: [V(*L["%s_%02d" % (name, i)][0]) for i in (1, 2, 3)] + [V(*L[name + "_end"][0])] for name in names}
    shares = sheet.sweep_shares(chains, t0, t1)

    def bones(P, u, v):
        hold = np.clip(1 - v / 0.12, 0, 1)
        return sheet.down_chains(chains, shares, P, u, hold, "pelvis")
    skirt = sheet.Sheet("coat_skirt_" + side, position, S.material("coat_skirt_" + side, PALETTE["teal"]), bones, columns, rows)
    # The trim: the skirt's last tenth, a little proud of it.
    band = lambda v: 0.9 + 0.1 * v  # noqa: E731
    trim = sheet.Sheet("coat_trim_" + side, lambda u, v: position(u, band(v), 0.3), S.material("coat_trim_" + side, PALETTE["brass"]),
                       lambda P, u, v: bones(P, u, band(v)), columns, 1)
    return [skirt, trim]


def scarf_tail(S, L, dims, mats, under):
    """The scarf's long end: from under the wrap at the back of her neck it lies over her left shoulder blade, then falls
    free behind her left arm to her waist, its end torn into two tongues. It hangs on the cloak's left chain (ADR-069 §7),
    so it streams behind her as she runs."""
    H = dims["height"]
    cz = L["spine_03"][1][2]
    pz = L["pelvis"][0][2]
    material = S.material("scarf_tail", PALETTE["scarf"])
    columns, rows = 4, 7
    chain = [V(*L["cape_l_%02d" % i][0]) for i in (1, 2, 3)] + [V(*L["cape_l_end"][0])]
    # Where it lies: on what is under it at the back of her neck and over her shoulder blade, a little proud.
    angles = np.radians([118.0, 138.0])
    heights = [cz + H * 0.004, cz - H * 0.075]
    starts = np.stack([np.cos(angles) * 40.0, np.sin(angles) * 40.0, heights], axis=1)
    hits, normals = garments.surface_points(under, starts, -starts * V(1, 1, 0), reach=45.0)
    a, b = hits[0] + normals[0] * 0.8, hits[1] + normals[1] * 1.2
    # Then down the chain's way, free of her, to her waist.
    fall = unit(chain[1] - chain[0])
    end = b + fall * (b[2] - (pz + H * 0.08)) / -fall[2]
    line = garments.curve([a, b, b + (end - b) * 0.5, end], 24)
    lengths = np.concatenate([[0.0], np.cumsum(np.linalg.norm(np.diff(line, axis=0), axis=1))])
    share = lengths / lengths[-1]
    limbs = [(L["upperarm_l"][0], L["upperarm_l"][1], H * 0.045), (L["lowerarm_l"][0], L["lowerarm_l"][1], H * 0.04)]

    def position(u, v):
        mid = np.stack([np.interp(v, share, line[:, k]) for k in range(3)], axis=1)
        ahead = np.stack([np.interp(np.minimum(v + 0.05, 1.0), share, line[:, k]) for k in range(3)], axis=1) - \
            np.stack([np.interp(np.maximum(v - 0.05, 0.0), share, line[:, k]) for k in range(3)], axis=1)
        # Across it: square to its fall and to her outward direction there, narrowing a little toward its end.
        out = mid * V(1, 1, 0)
        out = out / np.maximum(np.linalg.norm(out, axis=1, keepdims=True), 1e-6)
        across = np.cross(out, ahead)
        across = across / np.maximum(np.linalg.norm(across, axis=1, keepdims=True), 1e-6)
        width = H * (0.034 - 0.008 * v)[:, None]
        p = mid + across * width * (2.0 * u[:, None] - 1.0)
        return sheet.clear_of(p, limbs, 1.2)

    def bones(P, u, v):
        # Lying on her shoulder it moves with her chest; where it falls free, down its chain.
        hold = np.clip((0.3 - v) / 0.15, 0, 1)
        return sheet.down_chains({"cape_l": chain}, {"cape_l": 0.5}, P, u, hold, "spine_03")
    return sheet.Sheet("scarf_tail", position, material, bones, columns, rows, reach=lambda u: sheet.torn(u, 2, 0.82, 0.12, 5.0))
