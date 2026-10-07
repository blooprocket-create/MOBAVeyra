"""Celandrine, The Wayrunner (ADR-069): a hare-lineage Bramblekin courier, read by her silhouette from the game camera.
Character Bible and her splash art: an adult hare, longer-legged and more athletic than Mimzi, with long ears swept back
from her head, a hare's face (big amber eyes, a pale muzzle and cheeks, a pink nose) and a short round tail; powerful
hare hind legs on their toes with long furred feet; an olive weatherproof courier jacket with its big hood up, the ears
standing through it at brass grommets, its skirt to the thigh; a red scarf wound at her neck that streams behind her;
shorts, wrapped forearms and shins for distance; a map-and-letter satchel on her left hip on a strap across her body.
She carries a springbow in each hand, ready to fire: brass and green-painted steel, exposed springs and limbs, nothing
of them magical.

Low poly and flat-coloured (author 2026-10-07): the big forms that make her outline, each a flat colour the toon
material shades. She rests in the A pose, a springbow in each fist on its prop bone, pointing ahead as her clips aim it.
Her scarf's tails hang on the cloak's chains and her jacket's skirt on the skirt's chains (ADR-069 §7); her ears and
tail are her own body, carried by her head and tail bones. Colours are her kit's, lifted toward her art (lit warm)."""
import numpy as np

from ..sculpt import anatomy, garments, sdf, sheet, tree
from ..sculpt.anatomy import V, unit
from ..sculpt.garments import around, band_z, both, folds
from ..sculpt.tree import Box, Over, Placed, Shell, Union, Zone

# The hand, wrist to fingertip, as a share of the height.
HAND_SHARE = 0.1
# The head bone's length her head is authored at (cm); a longer or shorter head scales it.
HEAD_AUTHORED = 31.7
# Where round her the satchel rides, in degrees from the front toward her left.
SATCHEL_ANGLE = 88.0

PALETTE = {
    "fur": (0.7, 0.5, 0.32), "cream": (0.96, 0.92, 0.84), "ear_inner": (0.88, 0.5, 0.46), "ear_tip": (0.33, 0.23, 0.17),
    "eye": (0.92, 0.55, 0.13), "pupil": (0.08, 0.05, 0.04), "nose": (0.86, 0.5, 0.5),
    "olive": (0.4, 0.47, 0.24), "olive_dark": (0.27, 0.32, 0.16), "trim": (0.82, 0.66, 0.32), "brim": (0.7, 0.6, 0.3),
    "scarf": (0.7, 0.16, 0.13),
    "leather": (0.42, 0.28, 0.17), "leather_dark": (0.3, 0.2, 0.13), "glove": (0.34, 0.23, 0.15), "wrap": (0.86, 0.82, 0.7),
    "shorts": (0.36, 0.26, 0.18), "paper": (0.92, 0.88, 0.76),
    "bow_green": (0.3, 0.46, 0.24), "brass": (0.82, 0.62, 0.28), "steel": (0.42, 0.42, 0.44), "wood": (0.45, 0.3, 0.17),
}


def materials(S):
    """Every material she is coloured in, flat (the toon material shades it); nothing of her glows."""
    return {name: S.material(name, colour) for name, colour in PALETTE.items()}


def build(S, L, dims, spec):
    """Celandrine's sculpt on her layout: (the whole, {"body": her furred body, under her clothes, "sheets": her scarf's
    tails and her jacket's skirt})."""
    mats = materials(S)
    H = dims["height"]
    figure = anatomy.Figure(S, L, dims, mats["fur"], {"muscle": 0.4, "chest": 0.85, "breadth": 0.8, "hips": 1.05, "limb": 0.85,
                                                      "deltoid": 0.75, "leg": 1.0, "neck": 0.9, "bust": 0.45, "pecs": 0.5}).build()
    # A hare's hind legs on their toes: the figure's legs replaced by haunch, shank and long foot along her bones.
    for side in ("l", "r"):
        figure.parts.remove(figure.limbs["leg_" + side])
        figure.limbs["leg_" + side] = hare_leg(S, L, H, side, mats)
        figure.parts.append(figure.limbs["leg_" + side])
    s = (L["head"][1][2] - L["head"][0][2]) / HEAD_AUTHORED
    head_origin = V(*L["head"][0])
    figure.attach_head(Placed(head(S, mats, s), head_origin, np.eye(3)))
    for side in ("l", "r"):
        figure.limbs["hand_" + side] = hand(S, L, H, side, mats, figure)
    # Her tail is a limb of its own, so no garment kept to her torso covers it.
    figure.limbs["tail"] = tail(S, L, H, mats)
    figure.parts.append(figure.limbs["tail"])
    body = figure.body()
    dressed = clothes(S, L, dims, mats, body, figure.limbs)
    hooded = Over([dressed, hood(S, L, dims, mats, s)])
    worn = Over([hooded, satchel(S, L, dims, mats, dressed)] + [springbow(S, L, H, mats, side) for side in ("l", "r")])
    # Cut flat where she meets the ground: nothing of her sinks below it.
    worn = tree.Intersect(worn, Zone(lambda P: -P[:, 2], Box(V(-500, -500, -0.5), V(500, 500, 600))))
    sheets = scarf_tails(S, L, dims, mats) + jacket_skirt(S, L, dims, mats)
    return worn, {"body": body, "sheets": sheets}


# ---------------------------------------------------------------------------------------------- the body
# A haunch: its top's offset from the hip joint, and its radius there and at the knee (shares of the height).
HAUNCH_TOP = V(-0.015, 0.0, -0.06)
HAUNCH_RADII = (0.074, 0.045)


def rump(L, H, side):
    """The mass of side's haunch behind and below the hip: its centre, axes (its long axis down the thigh) and radii."""
    hp, knee = V(*L["thigh_" + side][0]), V(*L["thigh_" + side][1])
    sign = 1.0 if side == "l" else -1.0
    c = hp + (knee - hp) * 0.45 + V(-H * 0.03, sign * H * 0.008, 0)
    return c, sdf.frame(unit(knee - hp)), V(H * 0.072, H * 0.07, H * 0.09)


def hare_leg(S, L, H, side, mats):
    """A hare's hind leg on its toes along side's bones: a great furred haunch over the thigh, bunched to spring, a lean
    shank back to the high hock, and a long furred foot down to a broad paw on the ground, its toes pale."""
    hp, knee = V(*L["thigh_" + side][0]), V(*L["thigh_" + side][1])
    hock = V(*L["calf_" + side][1])
    toe = V(*L["foot_" + side][1])
    sign = 1.0 if side == "l" else -1.0
    thigh_w = anatomy.along("pelvis", "thigh_" + side, hp + V(0, 0, H * 0.04), hp - V(0, 0, H * 0.07), 0.2, 0.7)
    top = hp + HAUNCH_TOP * H
    parts = [tree.leaf(S, "haunch_" + side, lambda P: sdf.round_cone(P, top, knee, H * HAUNCH_RADII[0], H * HAUNCH_RADII[1]),
                       Box.around([top, knee], H * 0.1), mats["fur"], thigh_w)]
    # The haunch's mass bulging back, out and down behind the thigh: the hare's spring.
    c, axes, radii = rump(L, H, side)
    parts.append(tree.leaf(S, "rump_" + side, lambda P: sdf.ellipsoid(P, c, radii, axes), Box(c - radii.max(), c + radii.max()), mats["fur"], thigh_w))
    parts.append(tree.leaf(S, "knee_" + side, lambda P: sdf.sphere(P, knee, H * 0.034), Box(knee - H * 0.04, knee + H * 0.04), mats["fur"],
                           anatomy.rigid("calf_" + side)))
    parts.append(tree.leaf(S, "shank_" + side, lambda P: sdf.round_cone(P, knee, hock, H * 0.031, H * 0.023), Box.around([knee, hock], H * 0.035),
                           mats["fur"], anatomy.rigid("calf_" + side)))
    # The long foot: from behind the hock to the toes, broadening into the paw.
    along = unit(toe - hock)
    heel, tip = hock - along * H * 0.01, toe + along * H * 0.012
    foot = sdf.Loft(heel, tip, (0, 0, 1), [(0.0, 0, 0, H * 0.023, H * 0.024), (0.4, -H * 0.002, 0, H * 0.019, H * 0.026),
                                           (0.78, 0, 0, H * 0.021, H * 0.032), (1.0, 0, 0, H * 0.016, H * 0.026)], H * 0.008)
    parts.append(tree.leaf(S, "foot_" + side, foot, Box.around(foot.bounds_points()), mats["fur"], anatomy.rigid("foot_" + side)))
    paw = V(toe[0] + H * 0.004, toe[1], H * 0.031)
    parts.append(tree.leaf(S, "paw_" + side, lambda P: sdf.ellipsoid(P, paw, V(H * 0.058, H * 0.047, H * 0.031)), Box(paw - H * 0.06, paw + H * 0.06),
                           mats["fur"], anatomy.rigid("foot_" + side)))
    # Tufts of fur standing off the back of the haunch.
    back, down = -axes[:, 0], axes[:, 2]
    for k, (share, out) in enumerate(((-0.35, 0.8), (0.1, 0.95), (0.55, 0.75))):
        base = c + down * radii[2] * share + back * radii[0] * out
        end = base + back * H * 0.032 + down * H * 0.03
        parts.append(tree.leaf(S, "haunch_tuft_%s_%d" % (side, k), lambda P, a=base, b=end: sdf.round_cone(P, a, b, H * 0.022, H * 0.003),
                               Box.around([base, end], H * 0.025), mats["fur"], thigh_w, protect=0.3))
    leg = Union(parts, k=H * 0.02)
    # Pale toes: three fat furred pads at the front of the paw.
    toes = []
    for k, spread in enumerate((-1.0, 0.0, 1.0)):
        c_toe = paw + V(H * 0.043 - abs(spread) * H * 0.009, spread * H * 0.026, H * 0.002)
        toes.append(tree.leaf(S, "toe_%s_%d" % (side, k), lambda P, c=c_toe: sdf.ellipsoid(P, c, V(H * 0.022, H * 0.016, H * 0.019)),
                              Box(c_toe - H * 0.025, c_toe + H * 0.025), mats["cream"], anatomy.rigid("foot_" + side), protect=0.4))
    return Over([leg, Union(toes, k=H * 0.004)])


def tail(S, L, H, mats):
    """Her short round tail: a pale puff at the root of the tail bones, out behind her hips."""
    t0, t1 = V(*L["tail_01"][0]), V(*L["tail_01"][1])
    c = t0 + unit(t1 - t0) * H * 0.052
    puff = tree.leaf(S, "tail_puff", lambda P: sdf.ellipsoid(P, c, V(H * 0.036, H * 0.04, H * 0.04)), Box(c - H * 0.05, c + H * 0.05), mats["cream"],
                     anatomy.rigid("tail_01"), protect=0.4)
    tuft_tip = c + V(-H * 0.03, 0, H * 0.012)
    tuft = tree.leaf(S, "tail_tuft", lambda P: sdf.round_cone(P, c, tuft_tip, H * 0.028, H * 0.008), Box.around([c, tuft_tip], H * 0.03), mats["cream"],
                     anatomy.rigid("tail_01"), protect=0.3)
    return Union([puff, tuft], k=H * 0.01)


def head(S, mats, s):
    """A hare's head in the head's frame (x forward, z up, the head joint at the origin), authored at a head bone of
    HEAD_AUTHORED and scaled by s: a round skull; a pale muzzle and full pale cheeks; a pink nose; big amber eyes in pale
    rings, set wide; and her long ears swept back through the hood, broad and pink within, dark at the tips."""
    bones = anatomy.rigid("head")
    parts = []

    def leaf(name, distance, lo, hi, material, protect=0.8):
        parts.append(tree.leaf(S, name, distance, Box(V(*lo) * s, V(*hi) * s), material, bones, protect))

    sk_c, sk_r = V(0.5, 0, 14.5) * s, V(12.0, 11.0, 12.0) * s
    leaf("skull", lambda P: sdf.ellipsoid(P, sk_c, sk_r), (-13, -12, 1), (14, 12, 28), mats["fur"], 0.6)
    face = []
    # A pale, rounded muzzle and chin, and full pale cheeks beside it.
    mz_c, mz_r = V(11.2, 0, 7.6) * s, V(4.8, 5.2, 4.2) * s
    face.append(tree.leaf(S, "muzzle", lambda P: sdf.ellipsoid(P, mz_c, mz_r), Box(mz_c - mz_r - 1, mz_c + mz_r + 1), mats["cream"], bones, 0.8))
    for sign in (1.0, -1.0):
        ck_c, ck_r = V(7.4, sign * 6.0, 7.4) * s, V(5.0, 5.0, 4.4) * s
        face.append(tree.leaf(S, "cheek_%d" % (sign > 0), lambda P, c=ck_c, r=ck_r: sdf.ellipsoid(P, c, r), Box(ck_c - ck_r - 1, ck_c + ck_r + 1),
                              mats["cream"], bones, 0.6))
    nose_c = V(15.8, 0, 9.3) * s
    face.append(tree.leaf(S, "nose", lambda P: sdf.ellipsoid(P, nose_c, V(1.3, 1.8, 1.3) * s), Box(nose_c - 2.5 * s, nose_c + 2.5 * s), mats["nose"], bones, 1.0))
    # Big amber eyes set wide in pale rings of fur, a round dark pupil in each. The eye and its pupil are flat discs, each
    # standing proud of what lies under it on a sharp rim: the reduction keeps a rim, so their colours keep their shapes
    # (a dome's colour would break into a star of triangles).
    eyes = []
    skull = Zone(lambda P: sdf.ellipsoid(P, sk_c, sk_r), Box(sk_c - sk_r - 1, sk_c + sk_r + 1))
    for sign in (1.0, -1.0):
        facing = sdf.rotation(yaw=sign * 18.0) @ unit(V(1, 0, 0.12))
        e_c = V(9.6, sign * 5.6, 13.0) * s
        ring_c = e_c - facing * 1.0 * s
        ring = tree.leaf(S, "eye_ring_%d" % (sign > 0), lambda P, c=ring_c, ax=sdf.rotation(yaw=sign * 18.0): sdf.ellipsoid(P, c, V(2.6, 4.6, 5.2) * s, ax),
                         Box(ring_c - 6 * s, ring_c + 6 * s), mats["cream"], bones, 0.8)
        eyes.append(ring)
        # Where the eye sits: the ring's surface (or the skull's) along the way the eye faces.
        surface, _normal = garments.surface_point(Union([skull, ring]), e_c - facing * 6.0 * s, facing, reach=12.0 * s)
        eyes.append(disc(S, "eye_%d" % (sign > 0), surface, facing, 3.3 * s, 4.0 * s, 1.0 * s, 3.0 * s, mats["eye"], bones))
        eyes.append(disc(S, "pupil_%d" % (sign > 0), surface + V(0, -sign * 0.2, -0.3) * s, facing, 2.1 * s, 2.7 * s, 1.8 * s, 2.0 * s, mats["pupil"], bones))
    ears = [ear(S, mats, s, sign, bones) for sign in (1.0, -1.0)]
    return Over([Union(parts, k=1.0 * s), Union(face, k=1.2 * s), Over(eyes)] + ears)


def disc(S, name, centre, facing, half_wide, half_high, front, back, material, bones):
    """A flat oval disc facing out along facing from centre: half_wide across, half_high up, its face front beyond centre
    and its body back behind it (sunk into what it sits on), its rim sharp."""
    f = unit(facing)
    a, b = centre - f * back, centre + f * front
    shape = sdf.Loft(a, b, (0, 0, 1), [(0.0, 0, 0, half_high, half_wide), (1.0, 0, 0, half_high, half_wide)], min(half_wide, half_high) * 0.08)
    return tree.leaf(S, name, shape, Box.around(shape.bounds_points()), material, bones, 1.0)


def ear_frame(s, sign):
    """An ear's root, tip and frame (along it, across its breadth, out of its pink face) in the head's frame."""
    root = V(-3.0, sign * 5.0, 23.0) * s
    tip = V(-57.0, sign * 28.0, 40.0) * s
    w = unit(tip - root)
    hint = unit(V(0.2, sign * 0.75, 0.6))
    n = unit(hint - w * (hint @ w))
    return root, tip, w, unit(np.cross(n, w)), n


# An ear's shape along it (t from root to tip): its bow away from its pink face, its half breadth and half thickness (cm
# at the authored head).
EAR_T = [0.0, 0.1, 0.3, 0.5, 0.7, 0.85, 0.95, 1.0]
EAR_BOW = [0.0, -0.3, -0.8, -1.0, -0.9, -0.6, -0.3, 0.0]
EAR_HALF_BREADTH = [3.0, 4.2, 6.0, 6.5, 5.9, 4.5, 2.6, 0.9]
EAR_HALF_THICK = [1.8, 1.7, 1.6, 1.5, 1.4, 1.2, 1.0, 0.8]


def ear_loft(root, tip, across, t0, t1, s, breadth=1.0, thick=1.0, lift=0.0, grow=0.0):
    """The ear's shape (or a share of it, t0 to t1 of its length) as a loft: breadth and thick scale it, lift moves it
    toward the pink face (as a share of its thickness), grow swells it all round (cm)."""
    a, b = root + (tip - root) * t0, root + (tip - root) * t1
    stations = []
    for t in np.linspace(t0, t1, 7):
        bow = np.interp(t, EAR_T, EAR_BOW)
        hb = np.interp(t, EAR_T, EAR_HALF_BREADTH) * breadth + grow
        ht = np.interp(t, EAR_T, EAR_HALF_THICK) * thick + grow
        stations.append(((t - t0) / (t1 - t0), 0.0, (bow + np.interp(t, EAR_T, EAR_HALF_THICK) * lift) * s, hb * s, ht * s))
    return sdf.Loft(a, b, across, stations, 0.6 * s)


def ear(S, mats, s, sign, bones):
    """One long ear swept back and out from the crown: fur behind, its broad face pink within, its tip dark."""
    root, tip, w, across, n = ear_frame(s, sign)
    name = "ear_%d" % (sign > 0)
    outer = ear_loft(root, tip, across, 0.0, 1.0, s)
    inner = ear_loft(root, tip, across, 0.1, 0.9, s, breadth=0.68, thick=0.5, lift=0.85)
    dark = ear_loft(root, tip, across, 0.8, 1.0, s, grow=0.25)
    layers = [tree.leaf(S, name, outer, Box.around(outer.bounds_points()), mats["fur"], bones, 0.8),
              tree.leaf(S, name + "_inner", inner, Box.around(inner.bounds_points()), mats["ear_inner"], bones, 0.8),
              tree.leaf(S, name + "_tip", dark, Box.around(dark.bounds_points()), mats["ear_tip"], bones, 0.8)]
    return Over(layers)


def hand(S, L, H, side, mats, figure):
    """A gloved hand closed on its springbow's stock, which runs forward through the fist."""
    w0, w1 = V(*L["hand_" + side][0]), V(*L["hand_" + side][1])
    sign = 1.0 if side == "l" else -1.0
    x = unit(w1 - w0)
    out = V(0, sign, 0)
    z = unit(out - x * (out @ x))
    y = np.cross(z, x)
    curl = {"index": (60, 85, 50), "middle": (70, 95, 50), "ring": (75, 95, 50), "little": (78, 95, 45)}
    built = anatomy.Hand(S, mats["fur"], side, H * HAND_SHARE, "hand_" + side, curl=curl, thumb=(60, 30, 30, 25)).build()
    glove = Shell(S, "glove_" + side, built, 0.08, 0.35, Zone(lambda P: -np.ones(len(P)), Box((-30, -30, -30), (30, 30, 30))), mats["glove"], hem=0.1, reach=0.5)
    placed = Placed(Over([built, glove]), w0, np.stack([x, y, z], axis=1))
    figure.parts.append(placed)
    return placed


# ---------------------------------------------------------------------------------------------- her clothes
def clothes(S, L, dims, mats, body, limbs):
    """Her clothes over her fur, each keeping to its limbs: leather shorts; the sleeveless olive jacket from her hips to
    her neck; a belt, the satchel's strap across her body and a pouch; the hood's mantle over her shoulders; cream wraps
    bound with leather straps round her forearms and shins; her pale fur tufting at the elbows."""
    H = dims["height"]
    pz = L["pelvis"][0][2]
    cz = L["spine_03"][1][2]
    torso = cz - pz
    shorts = Shell(S, "shorts", body, 0.3, 0.6, both(band_z(pz - H * 0.05, pz + torso * 0.14), garments.keep_to(limbs, ["leg_l", "leg_r", "torso"])),
                   mats["shorts"], hem=0.3, reach=0.6)
    lower = Over([body, shorts])
    # The jacket is sleeveless: her furred arms show from under the hood's mantle.
    jacket_region = both(band_z(pz + torso * 0.02, cz + H * 0.02), garments.keep_to(limbs, ["torso"]))
    jacket = Shell(S, "jacket", lower, 0.7, 1.0, jacket_region, mats["olive"], hem=0.4, displace=folds((0, 0, 1), 8, 0.5, seed=221), reach=1.0)
    dressed = Over([lower, jacket])
    belt_z = pz + torso * 0.12
    belt = Shell(S, "belt", dressed, 0.2, 1.2, both(band_z(belt_z - H * 0.013, belt_z + H * 0.013), garments.keep_to(limbs, ["torso"])), mats["leather"], hem=0.2)
    # The satchel's strap: from her right shoulder across her chest and back to her left hip.
    shoulder = abs(L["upperarm_l"][0][1])
    a, b = V(0, -shoulder + 3.0, cz - 1.0), V(0, dims["hip"] * 1.25, pz + torso * 0.08)
    cross = unit(np.cross(b - a, V(1, 0, 0)))
    strap = Shell(S, "satchel_strap", dressed, 0.2, 1.0, both(garments.band_plane((a + b) / 2, cross, H * 0.026, Box((-40, -50, pz - 5), (40, 50, cz + 6))),
                                                             garments.keep_to(limbs, ["torso"])), mats["leather_dark"], hem=0.2, reach=0.5)
    # The hood's mantle over her shoulders and the tops of her arms, falling to a point on her breast.
    def mantle_region(P):
        angle = np.arctan2(P[:, 1], P[:, 0])
        hem = cz - H * 0.045 - H * 0.04 * np.clip(np.cos(angle), 0, 1) ** 4 - H * 0.025 * np.abs(np.sin(angle))
        return np.maximum(hem - P[:, 2], P[:, 2] - (cz + H * 0.005))
    region = both(Zone(mantle_region, Box((-40, -shoulder - 20, cz - H * 0.13), (40, shoulder + 20, cz + H * 0.05))),
                  garments.keep_to(limbs, ["torso", "upperarm_l", "upperarm_r"]))
    mantle = Shell(S, "mantle", Over([dressed, belt, strap]), 0.5, 1.1, region, mats["olive"], hem=0.5, displace=folds((0, 0, 1), 10, 0.8, seed=222), reach=1.0)
    belted = Over([dressed, belt, strap, mantle])
    # Wraps for distance: cream cloth round the forearms and shins, bound with leather straps.
    wraps, straps, tufts = [], [], []
    for side in ("l", "r"):
        e, w = V(*L["lowerarm_" + side][0]), V(*L["lowerarm_" + side][1])
        region = both(around([w, w + (e - w) * 0.78], [H * 0.04, H * 0.045]), garments.keep_to(limbs, ["forearm_" + side]))
        wraps.append(Shell(S, "arm_wrap_" + side, body, 0.4, 0.8, region, mats["wrap"], hem=0.3, displace=folds(w - e, 5, 0.25, seed=231), reach=0.5))
        knee, hock = V(*L["calf_" + side][0]), V(*L["calf_" + side][1])
        region = both(around([knee + (hock - knee) * 0.18, hock + (hock - knee) * 0.04], [H * 0.05, H * 0.04]), garments.keep_to(limbs, ["leg_" + side]))
        wraps.append(Shell(S, "shin_wrap_" + side, body, 0.4, 0.8, region, mats["wrap"], hem=0.3, displace=folds(hock - knee, 5, 0.25, seed=232), reach=0.5))
        for k, share in enumerate((0.1, 0.62)):
            c = w + (e - w) * share
            straps.append((c, unit(e - w), "forearm_" + side, "arm_strap_%s_%d" % (side, k)))
        for k, share in enumerate((0.3, 0.62, 0.92)):
            c = knee + (hock - knee) * share
            straps.append((c, unit(hock - knee), "leg_" + side, "shin_strap_%s_%d" % (side, k)))
        # Her own pale fur tufting out above the wrap at the elbow.
        axes = sdf.frame(unit(w - e))
        tufts.append(tree.leaf(S, "elbow_tuft_" + side, lambda P, c=e + (w - e) * 0.06, ax=axes: sdf.ellipsoid(P, c, V(H * 0.036, H * 0.036, H * 0.026), ax),
                               Box(e - H * 0.04, e + H * 0.04), mats["cream"], anatomy.rigid("lowerarm_" + side), protect=0.3))
    wrapped = Over([belted] + wraps)
    bands = [Shell(S, name, wrapped, 0.3, 0.6, both(garments.band_plane(c, axis, H * 0.014, Box(c - H * 0.06, c + H * 0.06)), garments.keep_to(limbs, [limb])),
                   mats["leather_dark"], hem=0.2) for c, axis, limb, name in straps]
    # A pouch on the belt at her right front.
    angle = np.radians(-35.0)
    hit, normal = garments.surface_point(belted, V(0, 0, belt_z - H * 0.02), V(np.cos(angle), np.sin(angle), 0.0))
    out = unit(normal * V(1, 1, 0))
    axes = np.stack([unit(np.cross(V(0, 0, 1), out)), V(0, 0, 1), out], axis=1)
    pouch = garments.pouch(S, "pouch", hit, axes, (H * 0.045, H * 0.05, H * 0.026), H * 0.018, mats["leather"], mats["leather_dark"], bones=anatomy.rigid("pelvis"))
    return Over([wrapped] + bands + [Union(tufts, k=0.0), pouch])


def hood(S, L, dims, mats, s):
    """Her big hood, up: a roomy olive dome over her head, its point falling behind, its crown set back so her face
    shows through a heavy rolled brim worked in gold; brass grommets where her ears stand through it; its cloth gathered
    round her neck into a cowl; and her red scarf wound once round her neck over the cowl, knotted behind."""
    H = dims["height"]
    o = V(*L["head"][0])
    cz = L["spine_03"][1][2]
    head_bones = anatomy.rigid("head")
    dome_c, dome_r = o + V(-2.5, 0, 16.0) * s, V(15.0, 14.0, 14.8) * s
    dome = tree.leaf(S, "hood", lambda P: sdf.ellipsoid(P, dome_c, dome_r), Box(dome_c - dome_r - 1, dome_c + dome_r + 1), mats["olive"], head_bones, 0.5)
    # The hood's point, falling behind the crown.
    pa, pb = o + V(-12.0, 0, 21.0) * s, o + V(-19.5, 0, 9.0) * s
    point = tree.leaf(S, "hood_point", lambda P: sdf.round_cone(P, pa, pb, 6.0 * s, 1.8 * s), Box.around([pa, pb], 7.0 * s), mats["olive"], head_bones, 0.3)
    shell = Union([dome, point], k=2.0 * s)
    # Its opening, cut flat across the front so her face stands out of it; the cut's face is the hood's shadowed lining.
    # The opening faces forward and a little up, so the camera above sees her eyes under the brim.
    brim_c = o + V(5.0, 0, 13.8) * s
    facing = unit(V(1, 0, 0.22))
    lining = tree.leaf(S, "hood_lining", lambda P: np.full(len(P), 1e3, dtype=np.float32), Box(brim_c - 1, brim_c + 1), mats["olive_dark"], head_bones)
    opening = tree.leaf(S, "hood_opening", lambda P: sdf.cylinder(P, brim_c - facing * 0.6 * s, brim_c + facing * 30.0 * s, 11.4 * s),
                        Box(brim_c - V(13, 13, 13) * s, brim_c + V(31, 13, 20) * s), mats["olive"], head_bones)
    shell = tree.Subtract(shell, opening, k=0.8 * s, label=lining.part.label)
    ring_axes = np.stack([V(0, 1, 0), unit(np.cross(facing, V(0, 1, 0))), facing], axis=1)
    # The rolled brim round the top and sides of the opening, open under her chin where the hood meets the cowl.
    roll = tree.leaf(S, "hood_brim", lambda P: sdf.torus(P, brim_c, 12.0 * s, 1.9 * s, ring_axes), Box(brim_c - 15 * s, brim_c + 15 * s), mats["brim"],
                     head_bones, 0.7)
    low = brim_c[2] - 6.0 * s
    brim = tree.Intersect(roll, Zone(lambda P: low - P[:, 2], Box(brim_c - 16 * s, brim_c + 16 * s)), k=1.0 * s)
    parts = [shell, brim]
    # Brass grommets where her ears pass through the hood.
    for sign in (1.0, -1.0):
        root, tip, w, across, n = ear_frame(s, sign)
        collar = ear_loft(root, tip, across, 0.1, 0.17, s, grow=0.9)
        parts.append(Placed(tree.leaf(S, "grommet_%d" % (sign > 0), collar, Box.around(collar.bounds_points()), mats["brass"], head_bones, 0.6), o, np.eye(3)))
    # The cowl: the hood's cloth gathered round her neck and spread over her shoulders, the scarf wound over it.
    ca, cb = V(-1.0, 0, o[2] + 2.5 * s), V(-0.5, 0, cz - H * 0.03)
    cowl = tree.leaf(S, "cowl", lambda P: sdf.round_cone(P, ca, cb, H * 0.058, H * 0.085), Box.around([ca, cb], H * 0.1), mats["olive"], None, 0.2)
    scarf_axes = np.stack([unit(V(1, 0, -0.28)), V(0, 1, 0), unit(V(0.28, 0, 1))], axis=1)
    sc = V(-0.5, 0, cz + H * 0.024)
    scarf = tree.leaf(S, "scarf_roll", lambda P: sdf.torus(P, sc, H * 0.08, H * 0.027, scarf_axes), Box(sc - H * 0.13, sc + H * 0.13), mats["scarf"],
                      anatomy.rigid("spine_03"), 0.3)
    knot_c = sc + V(-H * 0.06, H * 0.035, -H * 0.012)
    knot = tree.leaf(S, "scarf_knot", lambda P: sdf.ellipsoid(P, knot_c, V(H * 0.022, H * 0.026, H * 0.022)), Box(knot_c - H * 0.03, knot_c + H * 0.03), mats["scarf"],
                     anatomy.rigid("spine_03"), 0.3)
    return Over([cowl, Union(parts, k=0.3 * s), Union([scarf, knot], k=H * 0.006)])


def satchel(S, L, dims, mats, under):
    """Her map-and-letter satchel, nothing in it magical: a broad flat leather bag riding her left hip, its flap darker
    and buckled in brass, letters standing out from under the flap, a rolled map in a leather tube strapped along its
    top."""
    H = dims["height"]
    pz = L["pelvis"][0][2]
    bones = anatomy.rigid("pelvis")
    # Set on her hip at its side, before her hanging arm, standing off her haunch over the jacket's skirt.
    angle = np.radians(SATCHEL_ANGLE)
    out = V(np.cos(angle), np.sin(angle), 0.0)
    z = pz + H * 0.008
    hit, _normal = garments.surface_point(under, V(0, 0, z), out)
    half = V(H * 0.064, H * 0.018, H * 0.05)
    c = hit + out * (half[1] + H * 0.016)
    along = unit(np.cross(V(0, 0, 1), out))
    axes = np.stack([along, out, V(0, 0, 1)], axis=1)
    at = lambda x, y, zz: c + axes @ V(x, y, zz)  # noqa: E731
    parts = [tree.leaf(S, "satchel", lambda P: sdf.box(P, c, half, axes, H * 0.01), Box(c - half.max() - 2, c + half.max() + 2), mats["leather"], bones, 0.4)]
    flap_c = at(0, H * 0.003, half[2] * 0.4)
    flap_half = V(half[0] + 0.4, half[1] + 0.4, half[2] * 0.62)
    parts.append(tree.leaf(S, "satchel_flap", lambda P: sdf.box(P, flap_c, flap_half, axes, H * 0.008), Box(flap_c - flap_half.max() - 2, flap_c + flap_half.max() + 2),
                           mats["leather_dark"], bones, 0.4))
    buckle = at(0, half[1] + H * 0.006, -half[2] * 0.15)
    parts.append(tree.leaf(S, "satchel_buckle", lambda P: sdf.box(P, buckle, (H * 0.012, H * 0.004, H * 0.012), axes, H * 0.002), Box(buckle - 3, buckle + 3),
                           mats["brass"], bones, 0.7))
    for k, (dx, tilt) in enumerate(((-0.026, -14.0), (0.004, 8.0))):
        top = at(H * dx, H * 0.007, half[2] + H * 0.016)
        ax = axes @ sdf.rotation(pitch=tilt)
        parts.append(tree.leaf(S, "letter_%d" % k, lambda P, t=top, a=ax: sdf.box(P, t, (H * 0.014, H * 0.004, H * 0.018), a, 0.3), Box(top - 6, top + 6),
                               mats["paper"], bones, 0.5))
    tube_a, tube_b = at(-half[0] * 1.05, -H * 0.008, half[2] + H * 0.02), at(half[0] * 0.95, -H * 0.008, half[2] + H * 0.02)
    parts.append(tree.leaf(S, "map_tube", lambda P: sdf.cylinder(P, tube_a, tube_b, H * 0.017, H * 0.004), Box.around([tube_a, tube_b], H * 0.022),
                           mats["leather"], bones, 0.5))
    cap_b = tube_b + along * H * 0.008
    parts.append(tree.leaf(S, "map_end", lambda P: sdf.cylinder(P, tube_b - along * 0.5, cap_b, H * 0.012, H * 0.003), Box.around([tube_b, cap_b], H * 0.02),
                           mats["paper"], bones, 0.5))
    return Union(parts, k=0.3)


def springbow(S, L, H, mats, side):
    """A springbow in side's fist on its prop bone, ready to fire, mechanical and unlit: a green-painted steel stock
    reaching back past the fist and forward ahead of it with a brass rail along its top, a brass prod at its front with
    exposed coil springs at the roots of its brass limbs, swept back and canted a little, a cord drawn between them, a
    winding drum on its outer flank and a bolt laid in."""
    bones = anatomy.rigid("prop_" + side)
    sign = 1.0 if side == "l" else -1.0
    grip = V(*L["prop_" + side][0])
    fwd, up = V(1, 0, 0), V(0, 0, 1)
    # Across the limbs, canted so the inner limb rises clear of her thigh.
    cant = np.radians(22.0)
    inward = V(0, -sign * np.cos(cant), np.sin(cant))
    green, brass, steel = mats["bow_green"], mats["brass"], mats["steel"]
    parts = []

    def add(name, distance, points, pad, material, protect=1.0):
        parts.append(tree.leaf(S, "%s_%s" % (name, side), distance, Box.around(points, pad), material, bones, protect))

    stock_c = grip + fwd * H * 0.045
    add("stock", lambda P: sdf.box(P, stock_c, (H * 0.13, H * 0.014, H * 0.019), None, H * 0.006), [grip - fwd * H * 0.09, grip + fwd * H * 0.18], H * 0.025, green)
    rail_a, rail_b = grip + up * H * 0.019, grip + fwd * H * 0.16 + up * H * 0.019
    add("rail", lambda P: sdf.cylinder(P, rail_a, rail_b, H * 0.007, H * 0.002), [rail_a, rail_b], H * 0.012, brass)
    front = grip + fwd * H * 0.17
    add("prod", lambda P: sdf.box(P, front, (H * 0.014, H * 0.026, H * 0.02), None, H * 0.004), [front], H * 0.035, brass)
    nock = grip + fwd * H * 0.02 + up * H * 0.02
    for k, way in enumerate((inward, -inward)):
        tip = front + way * H * 0.1 - fwd * H * 0.05
        add("limb_%d" % k, lambda P, a=front + way * H * 0.015, b=tip: sdf.round_cone(P, a, b, H * 0.01, H * 0.0055), [front, tip], H * 0.014, brass)
        add("cord_%d" % k, lambda P, a=tip, b=nock: sdf.capsule(P, a, b, H * 0.003), [tip, nock], H * 0.005, mats["wrap"])
        for j, out in enumerate((0.03, 0.048)):
            ring = front + way * H * out
            add("spring_%d_%d" % (k, j), lambda P, r=ring, w=way: sdf.cylinder(P, r - w * H * 0.005, r + w * H * 0.005, H * 0.013, H * 0.002), [ring], H * 0.016, steel)
    drum = grip + fwd * H * 0.07 + V(0, sign * H * 0.014, 0)
    add("drum", lambda P: sdf.cylinder(P, drum, drum + V(0, sign * H * 0.013, 0), H * 0.021, H * 0.003), [drum], H * 0.035, brass)
    bolt_b = front + fwd * H * 0.05 + up * H * 0.02
    add("bolt", lambda P: sdf.capsule(P, nock, bolt_b, H * 0.0038), [nock, bolt_b], H * 0.006, mats["wood"])
    head_b = bolt_b + fwd * H * 0.025
    add("bolt_head", lambda P: sdf.round_cone(P, bolt_b, head_b, H * 0.007, H * 0.001), [bolt_b, head_b], H * 0.009, steel)
    return Union(parts, k=0.15)


# ---------------------------------------------------------------------------------------------- cloth sheets
def scarf_tails(S, L, dims, mats):
    """The red scarf's two tails, from its knot behind her neck down her back, one longer, their ends cut ragged; they
    hang on the cloak's chains (ADR-069 §7), so they stream behind her as she runs."""
    H = dims["height"]
    cz = L["spine_03"][1][2]
    pz = L["pelvis"][0][2]
    material = S.material("scarf_sheet", PALETTE["scarf"])
    chains = {prefix: [V(*L["%s_%02d" % (prefix, i)][0]) for i in (1, 2, 3)] + [V(*L[prefix + "_end"][0])] for prefix in ("cape_l", "cape", "cape_r")}
    sheets = []
    for k, (y, width, end, lean, share_l, salt) in enumerate(((H * 0.03, H * 0.06, pz + H * 0.01, 1.0, 0.4, 3.0), (-H * 0.02, H * 0.05, pz + H * 0.09, -1.0, 0.0, 7.0))):
        top = V(-H * 0.085, y, cz + H * 0.004)
        bottom = V(-H * 0.12, y + lean * H * 0.025, end)

        def position(u, v, top=top, bottom=bottom, width=width):
            line = top[None, :] + (bottom - top)[None, :] * v[:, None]
            across = (u - 0.5) * width * (0.85 + 0.3 * v)
            p = line + np.stack([np.zeros_like(u), across, np.zeros_like(u)], axis=1)
            # Bowed off her back as it falls, and twisted a little.
            p[:, 0] -= H * 0.015 * np.sin(np.pi * v) + (u - 0.5) * H * 0.012 * v
            return p

        def bones(P, u, v, share_l=share_l):
            w = sheet.down_chains({"cape": chains["cape"]}, {"cape": 0.5}, P, u, np.clip(1 - v / 0.12, 0, 1), "spine_03")
            if share_l:
                side = sheet.down_chains({"cape_l": chains["cape_l"]}, {"cape_l": 0.5}, P, u, np.clip(1 - v / 0.12, 0, 1), "spine_03")
                w = {b: w.get(b, 0.0) * (1 - share_l) + side.get(b, 0.0) * share_l for b in set(w) | set(side)}
            return {b: np.asarray(x, dtype=np.float32) for b, x in w.items()}
        sheets.append(sheet.Sheet("scarf_tail_%d" % k, position, material, bones, 4, 7, reach=lambda u, salt=salt: sheet.torn(u, 2, 0.8, 0.12, salt)))
    return sheets


def jacket_skirt(S, L, dims, mats):
    """The jacket's skirt: from the belt to her upper thigh in two panels, open at the front for her striding legs and
    vented at the back round her tail, flaring over her haunches, its hem trimmed gold; each panel hangs on the skirt's
    chains on its side (ADR-069 §7)."""
    H = dims["height"]
    pz = L["pelvis"][0][2]
    belt_z = pz + (L["spine_03"][1][2] - pz) * 0.12
    hip = dims["hip"]
    hem_z = pz - dims["leg"] * 0.17
    limbs = []
    for s in ("l", "r"):
        hp, knee = V(*L["thigh_" + s][0]), V(*L["thigh_" + s][1])
        c, axes, radii = rump(L, H, s)
        along = axes[:, 2]
        limbs += [(hp + HAUNCH_TOP * H, knee, H * HAUNCH_RADII[0]), (c - along * (radii[2] - radii[0]), c + along * (radii[2] - radii[0]), radii[0])]
    chains = {name: [V(*L["%s_%02d" % (name, i)][0]) for i in (1, 2, 3)] + [V(*L[name + "_end"][0])] for name in ("skirt_fl", "skirt_bl", "skirt_br", "skirt_fr")}
    colour = {"coat": S.material("skirt_sheet", PALETTE["olive"]), "trim": S.material("skirt_trim", PALETTE["trim"])}
    sheets = []
    for side, (t0, t1), names in (("l", (24.0, 154.0), ("skirt_fl", "skirt_bl")), ("r", (-154.0, -24.0), ("skirt_br", "skirt_fr"))):
        t0, t1 = np.radians(t0), np.radians(t1)
        mine = {n: chains[n] for n in names}
        shares = sheet.sweep_shares(mine, t0, t1)

        def position(u, v, t0=t0, t1=t1):
            theta = t0 + (t1 - t0) * u
            radial = np.stack([np.cos(theta), np.sin(theta), np.zeros_like(theta)], axis=1)
            top = np.stack([hip * 1.0 * np.cos(theta), hip * 1.35 * np.sin(theta), np.full_like(theta, belt_z)], axis=1)
            p = top + radial * (H * 0.05 * v ** 1.1 + (0.3 + 0.6 * v) * np.sin(theta * 6.0))[:, None]
            p[:, 2] = belt_z + (hem_z - belt_z) * v - H * 0.015 * v * np.clip(-np.cos(theta), 0, 1)
            return sheet.clear_of(p, limbs, 2.2)

        def bones(P, u, v, mine=mine, shares=shares):
            return sheet.down_chains(mine, shares, P, u, np.clip(1 - v / 0.15, 0, 1), "pelvis")

        def trim_position(u, v, position=position, t0=t0, t1=t1):
            p = position(u, 0.9 + 0.1 * v)
            theta = t0 + (t1 - t0) * u
            return p + np.stack([np.cos(theta), np.sin(theta), np.zeros_like(theta)], axis=1) * 0.25

        def trim_bones(P, u, v, bones=bones):
            return bones(P, u, 0.9 + 0.1 * v)
        sheets.append(sheet.Sheet("skirt_" + side, position, colour["coat"], bones, 8, 4))
        sheets.append(sheet.Sheet("skirt_trim_" + side, trim_position, colour["trim"], trim_bones, 8, 1))
    return sheets
