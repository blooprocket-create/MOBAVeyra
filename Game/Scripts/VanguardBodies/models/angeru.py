"""Angeru, The Housebreaker (ADR-069): a swordsman of two houses who belongs to neither, read by his silhouette from
the game camera. Character Bible §15 and his splash art: a lean black figure wearing the gear of both houses at once,
stripped of their marks. From the Blade House, structured formal pieces in disciplined lines (a stiff crossed vest under
a standing collar) and the long single-edged sword sheathed across his back, its hilt over his left shoulder. From the
Veil House, soft dark wrapping: bindings wound round the forearms and shins, the hood worn down round his shoulders on a
short torn mantle, cloth cut to move silently, and the thin throwing needles. A torn red sash and a red cloak-tail
trail behind him, the one loud colour on him. A round crossed-blades token hangs at his belt beside the dagger's
sheath. Every house mark is cut out of the cloth, leaving clean empty patches (his left sleeve, over his heart, the
back of his hood). His face is uncovered, flat and level; black hair pulled up into a topknot, loose locks round it.

Low poly and flat-coloured (author 2026-10-07): the big forms that make his outline, each a flat colour the toon
material shades. He stands as his layout rests him (the canon corrects the art's crouch): empty-handed in the A pose
but for what his kit puts in his hands (props), which his clips carry; never a pose sculpted into the rest. Every loose
part hangs on spring chains (ADR-069 §7): the cloak-tail, his robe's torn tails and the sash's ends, and his loose
locks. His Blade body (Forsake the Schools) reads its own props: the scabbard emptied and the long sword drawn in his
right hand.

Proportions: the art crouches, so his are set against his head (the kit's head share, a little under it, the topknot
rising past it): a lean swordsman's long legs, narrow hips and shoulders broadened by the mantle, the head kept large
enough to read from a high camera. Colours are the kit's, sampled from the art and lifted where its night light would
read black under the toon light."""
import numpy as np

from ..sculpt import anatomy, garments, hair, paint, sdf, sheet, tree
from ..sculpt.anatomy import V, unit
from ..sculpt.garments import around, band_z, both, either, folds, without
from ..sculpt.tree import Box, Over, Placed, Shell, Union, Zone

# The hand, wrist to fingertip, as a share of the height.
HAND_SHARE = 0.105
# The skull his hair grows over, in the head's frame at a 24 cm template's scale, and how far beyond it a lock has
# passed wholly to its hair chain (cm).
SKULL_CENTRE = (-1.2, 0.0, 14.4)
SKULL_RADII = (9.9, 7.5, 8.0)
HAIR_REACH = 3.5
# The head's crown as a share of the head bone: the hair's cap rises over it to the bone's top, the topknot past it.
CROWN_SHARE = 0.95
# Where a closed fist's grip runs, in the hand's frame at an 18.5 cm template's scale (x toward the knuckles, z out of
# the back of the hand): across the palm under the curled fingers.
FIST_CHANNEL = (8.5, 0.0, -2.8)
# A closed fist's finger curls (degrees at each knuckle) and its thumb (reach, base, middle, tip).
FIST = {"index": (60, 85, 45), "middle": (65, 90, 50), "ring": (68, 90, 50), "little": (70, 90, 45)}
FIST_THUMB = (55, 30, 30, 20)
# An empty hand at rest, loosely curled.
OPEN = {"index": (12, 18, 10), "middle": (16, 22, 12), "ring": (18, 25, 14), "little": (22, 28, 15)}
OPEN_THUMB = (30, 15, 18, 12)
# What his hands may hold (props), closed round its grip.
HELD = ("dagger", "longSword", "needles")

# The kit's colours, sampled from the art and lifted so the black reads as cloth, not a hole, under the toon light;
# the red stays the one loud note.
PALETTE = {
    "skin": (0.82, 0.67, 0.56), "hair": (0.12, 0.115, 0.135), "eye": (0.36, 0.25, 0.22), "cloth": (0.17, 0.16, 0.17),
    "hood": (0.13, 0.13, 0.15), "vest": (0.24, 0.24, 0.26), "binding": (0.29, 0.28, 0.29), "shoe": (0.11, 0.105, 0.115),
    "red": (0.66, 0.08, 0.08), "patch": (0.44, 0.42, 0.41), "lacquer": (0.1, 0.095, 0.105), "fitting": (0.74, 0.73, 0.7),
    "grip": (0.24, 0.07, 0.08), "steel": (0.6, 0.61, 0.65), "edge": (0.9, 0.9, 0.93), "leather": (0.24, 0.17, 0.13),
}


def materials(S):
    """Every material Angeru is coloured in, flat (the toon material shades it)."""
    return {name: S.material(name, colour) for name, colour in PALETTE.items()}


def build(S, L, dims, spec):
    """Angeru's sculpt on his layout: (the whole, {"body": the skin alone, under his clothes, "sheets": his cloth})."""
    mats = materials(S)
    H = dims["height"]
    features = set(spec.get("features", []))
    props = spec.get("props", [])
    held = {side: [p for p in props if p["kind"] in HELD and side_of(p) == side] for side in ("l", "r")}
    figure = anatomy.Figure(S, L, dims, mats["skin"], {"muscle": 0.45, "chest": 0.9, "breadth": 0.86, "hips": 0.95, "limb": 0.86,
                                                        "deltoid": 0.8, "leg": 0.9, "neck": 0.86}).build()
    head_origin = V(*L["head"][0])
    size = (L["head"][1][2] - L["head"][0][2]) * CROWN_SHARE * 24.0 / 21.9
    figure.attach_head(Placed(Over([anatomy.Head(S, mats["skin"], size, look={"jaw": 0.88}).build(), brows(S, mats, size),
                                    hairdo(S, L, mats, spec, size)]), head_origin, np.eye(3)))
    grips = {}
    for side in ("l", "r"):
        node, grips[side] = hand(S, L, H, side, mats, bool(held[side]), "wrapBindings" in features)
        figure.parts.append(node)
        figure.limbs["hand_" + side] = node
    body = figure.body()
    dressed = clothes(S, L, dims, mats, body, figure.limbs, features)
    mantled = Over([dressed, mantle(S, L, dims, mats, dressed, figure.limbs)]) if "hoodDown" in features else dressed
    hooded = Over([mantled, hood(S, L, dims, mats, mantled, features)]) if "hoodDown" in features else mantled
    parts = [hooded, belt_gear(S, L, dims, mats, dressed, features)]
    for prop in props:
        parts.append(weapon(S, L, dims, mats, prop, grips, hooded))
    worn = Over([p for p in parts if p is not None])
    # Cut flat where he stands: nothing of him sinks below the ground.
    worn = tree.Intersect(worn, Zone(lambda P: -P[:, 2], Box(V(-300, -300, -0.5), V(300, 300, 400))))
    sheets = []
    if "longCloak" in features and "cape_01" in L and "cape_l_01" in L:
        sheets.append(cloak(S, L, dims, mats, mantled))
    if "coat_l_01" in L:
        sheets += [robe_tails(S, L, dims, mats, dressed, side) for side in ("l", "r")]
        if "waistSash" in features:
            sheets += [sash_end(S, L, dims, mats, dressed, k) for k in (0, 1)]
    return worn, {"body": body, "sheets": sheets}


def side_of(prop):
    return "l" if prop["hand"] == "left" else "r"


# ---------------------------------------------------------------------------------------------- head
def brows(S, mats, size):
    """Level dark brows over level, narrow eyes, in the head's frame: the calm that carries him (never knit, never
    raised, never wide)."""
    u = size / 24.0
    parts = []
    for sign, side in ((1.0, "l"), (-1.0, "r")):
        a, b = V(9.7, sign * 1.0, 12.4) * u, V(8.8, sign * 4.5, 12.5) * u
        parts.append(tree.leaf(S, "brow_" + side, lambda P, a=a, b=b: sdf.round_cone(P, a, b, 0.6 * u, 0.45 * u), Box.around([a, b], 1.0 * u),
                               mats["hair"], anatomy.rigid("head"), protect=1.0))
        c = V(8.9, sign * 3.0, 10.2) * u
        parts.append(tree.leaf(S, "eye_" + side, lambda P, c=c: sdf.ellipsoid(P, c, V(0.6, 1.7, 0.5) * u), Box(c - 2.0 * u, c + 2.0 * u),
                               mats["eye"], anatomy.rigid("head"), protect=1.0))
    return Union(parts)


def hairdo(S, L, mats, spec, size):
    """His black hair in the head's frame: pulled up into a round topknot tied at the crown, its tail falling back from
    the tie (never standing up as horns would); loose locks round his face and over his nape, those over his brow short
    so his face stays clear. The loose locks sway on the hair's chains where his kit gives them (ADR-069 §7); the cap and
    the topknot stay with his head."""
    u = size / 24.0
    head_origin = V(*L["head"][0])
    locks_bones = anatomy.rigid("head")
    chains = sorted(bone[:-3] for bone in L if bone.startswith("hair_") and bone.endswith("_01"))
    if chains:
        joints = {chain: [L["%s_%s" % (chain, joint)][0] for joint in ("01", "02", "end")] for chain in chains}
        locks_bones = hair.on_chains(joints, "head", head_origin + V(*SKULL_CENTRE) * u, V(*SKULL_RADII) * u, HAIR_REACH)
    locks = hair.messy(S, mats["hair"], locks_bones, SKULL_CENTRE, SKULL_RADII, (15.5, 6.5), seed=spec["seed"], unit_scale=u, count=24,
                       length=(6.0, 10.0), radius=(1.4, 2.0), wind=(0.0, 0.1, 0.0), fringe=0.3, volume=0.15, cap_bones=anatomy.rigid("head"))
    # The topknot: a knot of hair at the crown, set back, tied with a red cord, its tail a few locks falling back.
    head = anatomy.rigid("head")
    knot = V(-4.6, 0.0, 25.6) * u
    tie = V(-3.9, 0.0, 23.7) * u
    parts = [tree.leaf(S, "topknot", lambda P: sdf.ellipsoid(P, knot, V(3.4, 3.1, 2.7) * u), Box(knot - 4 * u, knot + 4 * u), mats["hair"], head)]
    cord_a, cord_b = tie - V(-0.2, 0, 0.6) * u, tie + V(-0.2, 0, 0.6) * u
    parts.append(tree.leaf(S, "topknot_tie", lambda P: sdf.cylinder(P, cord_a, cord_b, 2.1 * u, 0.4 * u), Box.around([cord_a, cord_b], 2.6 * u),
                           mats["red"], head, protect=0.6))
    for i, (direction, length) in enumerate(((V(-1.0, 0.0, 0.25), 8.0), (V(-0.9, 0.4, 0.15), 6.5), (V(-0.9, -0.4, 0.15), 6.5))):
        root = knot + V(-1.6, 0, 0.6) * u
        parts.append(hair.clump(S, "topknot_tail_%d" % i, root, direction, V(0, 0, 1), length * u, 1.8 * u, 0.55, 0.05, mats["hair"], head))
    return Union([locks] + parts, k=0.6 * u)


# ---------------------------------------------------------------------------------------------- hands
def hand_frame(L, side):
    """A hand's wrist and frame (columns x toward the knuckles, y across, z out of the back of the hand): hanging at
    his side, palm in."""
    w0, w1 = V(*L["hand_" + side][0]), V(*L["hand_" + side][1])
    sign = 1.0 if side == "l" else -1.0
    x = unit(w1 - w0)
    out = V(0, sign, 0)
    z = unit(out - x * (out @ x))
    y = np.cross(z, x)
    return w0, np.stack([x, y, z], axis=1)


def hand(S, L, H, side, mats, closed, wrapped):
    """A hand at rest beside his thigh, palm in: closed round the grip of what it holds (its grip runs forward through
    the fist), else loose; the bindings wound over its back to the knuckles, the fingers bare. Returns (its node,
    where its grip runs)."""
    w0, axes = hand_frame(L, side)
    bare = anatomy.Hand(S, mats["skin"], side, H * HAND_SHARE, "hand_" + side, curl=FIST if closed else OPEN, thumb=FIST_THUMB if closed else OPEN_THUMB)
    built = bare.build()
    layers = [built]
    if wrapped:
        region = Zone(lambda P, k=bare.u: P[:, 0] - 8.6 * k, Box((-5, -10, -6), (8.6 * bare.u, 10, 6)))
        layers.append(Shell(S, "hand_wrap_" + side, built, 0.1, 0.3, region, mats["binding"], hem=0.12, reach=0.5))
    placed = Placed(Over(layers), w0, axes)
    grip = w0 + axes @ (V(*FIST_CHANNEL) * bare.u)
    return placed, grip


# ---------------------------------------------------------------------------------------------- clothes
def ramp(a, b, start, depth, reach):
    """A displacement rising from nothing at start of the way from a to b to depth at b and past it, only within reach
    of the line a to b (fading out over a further half of reach): a sleeve widening toward its cuff."""
    a, b = V(*a), V(*b)
    ab = b - a
    length2 = max(ab @ ab, 1e-9)

    def displace(P):
        t = ((P - a) @ ab) / length2
        near = a + np.clip(t, 0.0, 1.0)[:, None] * ab
        window = np.clip(1.0 - (np.linalg.norm(P - near, axis=1) - reach) / (reach * 0.5), 0.0, 1.0)
        return (depth * window * np.clip((t - start) / (1.0 - start), 0.0, 1.0) ** 1.5).astype(np.float32)
    return displace


def wraps(a, b, period, depth):
    """Bindings wound round a limb from a to b: a ridge spiralling round it every period cm, depth proud."""
    a, b = V(*a), V(*b)
    ab = unit(b - a)
    ref = unit(np.cross(ab, V(1, 0, 0))) if abs(ab[0]) < 0.9 else unit(np.cross(ab, V(0, 1, 0)))
    other = np.cross(ab, ref)

    def displace(P):
        Q = P - a
        phase = (Q @ ab) / period + np.arctan2(Q @ other, Q @ ref) / (2.0 * np.pi)
        return (depth * (0.5 + 0.5 * np.cos(2.0 * np.pi * phase)) ** 0.6).astype(np.float32)
    return displace


def shoe(S, L, side, H, material):
    """A soft low shoe on one foot, its sole flat on the ground: heel to toe along the foot, tall at the ankle."""
    f0, f1 = V(*L["foot_" + side][0]), V(*L["foot_" + side][1])
    sign = 1.0 if side == "l" else -1.0
    h = H / 171.0
    heel_x, toe_x = f0[0] - 4.4 * h, f1[0] + 5.6 * h
    y = f0[1]
    shape = sdf.Loft(V(heel_x, y, 0.0), V(toe_x, y, 0.0), (0, 0, 1),
                     [(0.0, 3.3 * h, 0, 3.3 * h, 3.2 * h, 2.4), (0.22, 4.6 * h, 0, 4.6 * h, 3.6 * h, 2.4), (0.55, 3.5 * h, sign * 0.2 * h, 3.5 * h, 4.0 * h, 2.4),
                      (0.8, 2.8 * h, sign * -0.3 * h, 2.8 * h, 4.1 * h, 2.4), (1.0, 2.2 * h, sign * -0.5 * h, 2.2 * h, 3.0 * h, 2.2)], cap=1.0 * h)
    return tree.leaf(S, "shoe_" + side, shape, Box.around(shape.bounds_points()), material, anatomy.rigid("foot_" + side))


def clothes(S, L, dims, mats, body, limbs, features):
    """His clothes in layers, each over those beneath and each keeping to its limbs: soft shoes; loose black trousers
    bloused into bindings wound round his shins; a black robe with wide sleeves cut off below the elbow, the bindings
    wound from there to the wrist; over it the formal vest, its stiff panels crossed at the front under a standing collar;
    the red sash at his waist; and the patches where every house mark was cut out of the cloth."""
    H = dims["height"]
    h = H / 171.0
    pz = L["pelvis"][0][2]
    cz = L["spine_03"][1][2]
    torso = cz - pz
    knee_z = L["calf_l"][0][2]
    shoes = [shoe(S, L, side, H, mats["shoe"]) for side in ("l", "r")]
    # The trousers: loose, from inside the bindings to under the sash, bunched where they blouse over the bindings.
    legs = both(band_z(knee_z - H * 0.1, pz + H * 0.04), garments.keep_to(limbs, ["leg_l", "leg_r", "torso"]))
    blouse = garments.combine(folds((0, 0, 1), 9, 0.7, seed=151), garments.bunch((0, 0, knee_z - H * 0.08), (0, 0, knee_z + H * 0.03), 2.0, 1.3, seed=152))
    trousers = Shell(S, "trousers", body, 0.6, 1.2, legs, mats["cloth"], hem=0.4, displace=blouse, reach=2.2)
    lower = Over([body, trousers] + shoes)
    layers = [lower]
    if "wrapBindings" in features:
        # Soft dark bindings wound round each shin, from inside the shoe to under the knee.
        for side in ("l", "r"):
            k, a = V(*L["calf_" + side][0]), V(*L["calf_" + side][1])
            top = a + (k - a) * 0.8
            region = both(around([a - V(0, 0, 2.0 * h), top], [5.5 * h, 7.0 * h]), garments.keep_to(limbs, ["leg_" + side]))
            layers.append(Shell(S, "shin_binding_" + side, lower, 0.15, 0.5, region, mats["binding"], hem=0.3, displace=wraps(a, top, 3.4 * h, 0.5),
                                reach=0.6))
    legged = Over(layers)
    # The robe: black, over the torso to the hips and down the arms in wide sleeves cut off below the elbow.
    sleeves = []
    for side in ("l", "r"):
        e, w = V(*L["lowerarm_" + side][0]), V(*L["lowerarm_" + side][1])
        cuff = e + (w - e) * 0.28
        sleeves.append(either(garments.keep_to(limbs, ["upperarm_" + side]),
                              both(around([e, cuff], [7.0 * h, 7.0 * h]), garments.keep_to(limbs, ["forearm_" + side, "upperarm_" + side]))))
    robe_region = either(both(band_z(pz - H * 0.035, cz + H * 0.01), garments.keep_to(limbs, ["torso"])), *sleeves)
    flare = garments.combine(*[ramp(V(*L["upperarm_" + s][0]), V(*L["lowerarm_" + s][0]) + (V(*L["lowerarm_" + s][1]) - V(*L["lowerarm_" + s][0])) * 0.28,
                                    0.55, 1.1, 7.0 * h) for s in ("l", "r")])
    robe = Shell(S, "robe", legged, 0.35, 0.7, robe_region, mats["cloth"], hem=0.4, displace=garments.combine(flare, folds((0, 0, 1), 9, 0.3, seed=153)), reach=2.0)
    robed = Over([legged, robe])
    layers = [robed]
    if "wrapBindings" in features:
        # The bindings wound round each forearm from under the sleeve to the wrist.
        for side in ("l", "r"):
            e, w = V(*L["lowerarm_" + side][0]), V(*L["lowerarm_" + side][1])
            region = both(around([w + (w - e) * 0.04, e + (w - e) * 0.2], [5.0 * h, 6.0 * h]), garments.keep_to(limbs, ["forearm_" + side]))
            layers.append(Shell(S, "arm_binding_" + side, robed, 0.12, 0.45, region, mats["binding"], hem=0.3, displace=wraps(e, w, 3.0 * h, 0.45), reach=0.5))
    bound = Over(layers)
    layers = [bound]
    vest = None
    if "formalVest" in features:
        # The formal vest: stiff dark-grey panels over the torso from the sash to the shoulders, crossed at the front
        # (the left panel over the right, its edge a straight line from his neck to his right hip), open in a narrow V
        # at the throat. Sleeveless; its lines stay straight.
        v_cut = Zone(lambda P: np.maximum(np.maximum(1.0 - P[:, 0], np.abs(P[:, 1] + 0.9 * h) - np.clip((P[:, 2] - (cz - torso * 0.42)) * 0.32, 0.0, 6.0 * h)),
                                          (cz - torso * 0.42) - P[:, 2]), Box((0, -14 * h, cz - torso * 0.5), (30 * h, 14 * h, cz + 12 * h)))
        region = without(both(band_z(waist_z(L) - H * 0.03, cz + H * 0.005), garments.keep_to(limbs, ["torso"])), v_cut)
        vest = Shell(S, "vest", bound, 0.3, 0.55, region, mats["vest"], hem=0.25, reach=0.2)
        # The crossing: the over panel's edge, piped in black, a stiff straight line down the V's left side from his
        # throat and on across the V's foot to the sash at his right hip.
        points = [V(10.0 * h, 4.4 * h, cz - torso * 0.06), V(11.5 * h, 1.6 * h, cz - torso * 0.26), V(12.0 * h, -0.9 * h, cz - torso * 0.42),
                  V(11.5 * h, -5.0 * h, cz - torso * 0.58), V(10.8 * h, -9.0 * h, cz - torso * 0.68)]
        base = Over([bound, vest])
        lapel = garments.fold(S, "lapel", base, garments.laid_on(base, points, tilt=0.0), 1.2 * h, 0.9 * h, mats["hood"], seed=154, crease=0.0)
        # The standing collar: a stiff band standing round the root of his neck, clear of his jaw, open at the throat.
        collar = Zone(lambda P: np.max(np.stack([cz - 1.0 * h - P[:, 2], P[:, 2] - (cz + 3.2 * h), np.hypot(P[:, 0], P[:, 1]) - 8.0 * h,
                                                 np.cos(np.arctan2(P[:, 1], P[:, 0] - 1.0)) - 0.62]), axis=0),
                      Box((-9 * h, -9 * h, cz - 2 * h), (9 * h, 9 * h, cz + 6 * h)))
        layers += [vest, lapel, Shell(S, "collar", base, 0.3, 0.8, collar, mats["vest"], hem=0.3)]
    dressed = Over(layers)
    if "waistSash" in features:
        # The red sash wound at his waist over the vest, creased round him; its knot and torn ends hang at his left hip.
        waist = waist_z(L)
        sash = Shell(S, "sash", dressed, 0.3, 0.9, both(band_z(waist - H * 0.028, waist + H * 0.026), garments.keep_to(limbs, ["torso"])), mats["red"],
                     hem=0.35, displace=folds((0, 0, 1), 10, 0.5, seed=155), reach=0.8)
        sashed = Over([dressed, sash])
        # The knot at his left hip, where its ends hang from.
        angle = np.radians(76.0)
        out = V(np.cos(angle), np.sin(angle), 0.0)
        hit, _n = garments.surface_points(sashed, (V(0, 0, waist) + out * 40.0)[None, :], (-out)[None, :], reach=40.0)
        c = hit[0] + out * 0.6 * h
        axes = np.stack([unit(np.cross(V(0, 0, 1), out)), V(0, 0, 1), out], axis=1)
        knot = tree.leaf(S, "sash_knot", lambda P: sdf.ellipsoid(P, c, V(2.6 * h, 2.4 * h, 1.8 * h), axes), Box(c - 4.0 * h, c + 4.0 * h), mats["red"],
                         anatomy.rigid("pelvis"), protect=0.4)
        dressed = Over([sashed, knot])
    if "cutInsignia" in features:
        # Every house mark cut out of the cloth: a clean empty patch, paler than the black round it, on his left sleeve
        # and over his heart (the third is on the back of his hood).
        s0, e = V(*L["upperarm_l"][0]), V(*L["upperarm_l"][1])
        arm_point = s0 + (e - s0) * 0.64
        dressed = cut_patch(S, dressed, "patch_sleeve", arm_point + V(0, 25.0, 0), V(0, -1, 0), unit(e - s0), (2.4 * h, 3.0 * h), 0.45, mats["patch"],
                            anatomy.rigid("upperarm_l"))
        dressed = cut_patch(S, dressed, "patch_heart", V(25.0, 5.6 * h, cz - torso * 0.36), V(-1, 0, 0), V(0, 0, 1), (2.3 * h, 2.8 * h), 0.4, mats["patch"],
                            None)
    return dressed


def waist_z(L):
    pz = L["pelvis"][0][2]
    return pz + (L["spine_03"][1][2] - pz) * 0.18


def cut_patch(S, under, name, start, inward, up, half, depth, material, bones):
    """under with a clean patch cut out of its surface where a ray from start along inward meets it: a recess depth deep,
    half (across, up) in size, its floor material's colour (the cloth beneath a cut-out mark)."""
    hit, normal = garments.surface_points(under, np.asarray(start, dtype=np.float64)[None, :], np.asarray(inward, dtype=np.float64)[None, :], reach=40.0)
    p, n = hit[0], normal[0]
    up = unit(V(*up) - n * (n @ V(*up)))
    across = np.cross(n, up)
    thick = 3.0
    centre = p + n * (thick - depth)
    axes = np.stack([across, up, n], axis=1)
    shape = lambda P, c=centre, R=axes: sdf.box(P, c, (half[0], half[1], thick), R, 0.35)  # noqa: E731
    cutter = tree.leaf(S, name, shape, Box(centre - (max(half) + thick + 1.0), centre + (max(half) + thick + 1.0)), material, bones, protect=0.6)
    return tree.Subtract(under, cutter, k=0.1, label=cutter.part.label)


def mantle(S, L, dims, mats, under, limbs):
    """The hood's short mantle of soft black cloth over his shoulders and upper arms, open at the front, its hem torn
    into tongues (cloth cut to move silently)."""
    H = dims["height"]
    cz = L["spine_03"][1][2]
    shoulder = abs(L["upperarm_l"][0][1])
    tongues = 14

    def region(P):
        angle = np.arctan2(P[:, 1], P[:, 0])
        strip = (angle + np.pi) / (2.0 * np.pi) * tongues
        tip = 1.0 - np.abs(2.0 * (strip % 1.0) - 1.0)
        hem = cz - H * 0.055 - H * 0.02 * np.abs(np.sin(angle)) - (H * 0.012 + H * 0.025 * paint.hashed(np.floor(strip), 3.0)) * tip ** 0.7
        return np.max(np.stack([hem - P[:, 2], P[:, 2] - (cz + H * 0.04), (np.cos(angle) - 0.82) * 15.0]), axis=0)
    h = H / 171.0
    # Kept off the standing collar: nothing within a hand of his neck above its root.
    collar = Zone(lambda P: np.maximum(np.hypot(P[:, 0], P[:, 1]) - 9.0 * h, (cz - 2.0 * h) - P[:, 2]), Box((-10 * h, -10 * h, cz - 3 * h), (10 * h, 10 * h, cz + 12 * h)))
    zone = without(both(Zone(region, Box((-40, -shoulder - 20, cz - H * 0.16), (40, shoulder + 20, cz + H * 0.05))),
                        garments.keep_to(limbs, ["torso", "upperarm_l", "upperarm_r"])), collar)
    return Shell(S, "mantle", under, 0.3, 0.7, zone, mats["hood"], hem=0.45, displace=folds((0, 0, 1), 11, 0.6, seed=156), reach=0.8)


def hood(S, L, dims, mats, under, features):
    """His hood worn down: its cloth bunched in a soft roll round the back of his neck, thinning out over his shoulders,
    and its crown lying flat on his upper back, where the house's mark was cut from it."""
    H = dims["height"]
    h = H / 171.0
    cz = L["spine_03"][1][2]

    # The roll: a soft tube of cloth round the back of his neck outside the standing collar, resting on his shoulders,
    # thinning toward its ends at his shoulders.
    degrees = np.array([95.0, 125.0, 155.0, 180.0, 205.0, 235.0, 265.0])
    radius = np.array([10.0, 10.6, 10.9, 11.0, 10.9, 10.6, 10.0]) * h
    thick = np.array([1.5, 2.6, 3.0, 3.1, 3.0, 2.6, 1.5]) * h
    a = np.radians(degrees)
    xy = np.stack([radius * np.cos(a), radius * np.sin(a)], axis=1)
    rest, _n = garments.surface_points(under, np.column_stack([xy, np.full(len(xy), cz + 7.0 * h)]), np.tile(V(0, 0, -1), (len(xy), 1)), reach=24.0)
    # Resting on his shoulders (no higher than they rise beside his neck, whatever a ray met first).
    line = [V(x, y, min(z, cz + 2.0 * h) + t * 0.55) for (x, y), z, t in zip(xy, rest[:, 2], thick)]
    neck = anatomy.rigid("spine_03")
    roll = tree.leaf(S, "hood_roll", lambda P: sdf.tube(P, line, list(thick)), Box.around(line, thick.max() + 1.0), mats["hood"], neck)
    # The crown: flattened on the upper back below the roll, narrowing to its point.
    back, _n = garments.surface_points(under, V(-40.0, 0, cz - H * 0.05)[None, :], V(1, 0, 0)[None, :], reach=40.0)
    bx = back[0][0]
    top, tip = V(bx - 0.8 * h, 0, cz - 1.0 * h), V(bx + 0.4 * h, 0, cz - H * 0.13)
    shape = sdf.Loft(top, tip, (-1, 0, 0), [(0.0, 0.0, 0, 2.6 * h, 8.5 * h), (0.45, 0.4 * h, 0, 2.4 * h, 7.5 * h), (1.0, 0.0, 0, 1.0 * h, 1.5 * h)], cap=0.8 * h)
    crown = tree.leaf(S, "hood_crown", shape, Box.around(shape.bounds_points()), mats["hood"], neck)
    if "cutInsignia" in features:
        crown = cut_patch(S, crown, "patch_hood", V(bx - 30.0, 0, cz - H * 0.055), V(1, 0, 0), V(0, 0, 1), (3.2 * h, 3.6 * h), 0.5, mats["patch"], neck)
    return Union([roll, crown], k=1.2 * h)


# ---------------------------------------------------------------------------------------------- gear
def belt_gear(S, L, dims, mats, under, features):
    """At his right hip below the sash: the round crossed-blades token on its cord and, behind it, the dagger's sheath."""
    H = dims["height"]
    h = H / 171.0
    waist = waist_z(L)
    pelvis = anatomy.rigid("pelvis")
    parts = []
    if "crossedToken" in features:
        angle = np.radians(-26.0)
        out = V(np.cos(angle), np.sin(angle), 0.0)
        z = waist - H * 0.09
        hit, normal = garments.surface_points(under, (V(0, 0, z) + out * 40.0)[None, :], (-out)[None, :], reach=40.0)
        n = unit(V(normal[0][0], normal[0][1], 0.0) * 0.6 + out * 0.4)
        # Hung clear of his hip on its cord, so the whole round of it shows.
        c = hit[0] + n * 3.6 * h
        r = H * 0.03
        up = V(0, 0, 1)
        across = unit(np.cross(n, up))
        axes = np.stack([across, up, n], axis=1)
        parts.append(tree.leaf(S, "token", lambda P: sdf.cylinder(P, c - n * 0.45 * h, c + n * 0.45 * h, r, 0.35 * h), Box(c - r - 1, c + r + 1),
                               mats["fitting"], pelvis, protect=1.0))
        # The two blades crossed on its face, raised.
        for k, turn in enumerate((45.0, -45.0)):
            d = np.cos(np.radians(turn)) * up + np.sin(np.radians(turn)) * across
            a, b = c + n * 0.55 * h - d * r * 0.72, c + n * 0.55 * h + d * r * 0.72
            parts.append(tree.leaf(S, "token_blade_%d" % k, lambda P, a=a, b=b: sdf.capsule(P, a, b, 0.5 * h), Box.around([a, b], 1.5), mats["lacquer"],
                                   pelvis, protect=1.0))
        cord_top = V(0, 0, waist - H * 0.02) + out * (np.linalg.norm(hit[0][:2]) + 0.6 * h)
        parts.append(tree.leaf(S, "token_cord", lambda P: sdf.capsule(P, cord_top, c + up * r * 0.9, 0.45 * h), Box.around([cord_top, c], 2.0), mats["red"],
                               pelvis, protect=0.6))
    # The dagger's sheath, hung from the sash behind the token, slanting forward.
    angle = np.radians(-58.0)
    out = V(np.cos(angle), np.sin(angle), 0.0)
    hit, _n = garments.surface_points(under, (V(0, 0, waist - H * 0.04) + out * 40.0)[None, :], (-out)[None, :], reach=40.0)
    mouth = hit[0] + out * 1.6 * h + V(0, 0, H * 0.02)
    tip = mouth + V(H * 0.05, 0, -H * 0.12) + out * 1.0 * h
    parts.append(tree.leaf(S, "sheath", lambda P: sdf.round_cone(P, mouth, tip, 1.9 * h, 0.9 * h), Box.around([mouth, tip], 2.5), mats["leather"], pelvis,
                           protect=0.5))
    parts.append(tree.leaf(S, "sheath_chape", lambda P: sdf.round_cone(P, mouth + (tip - mouth) * 0.85, tip, 1.15 * h, 0.95 * h), Box.around([mouth, tip], 2.5),
                           mats["fitting"], pelvis, protect=0.5))
    return Union(parts, k=0.2) if parts else None


def weapon(S, L, dims, mats, prop, grips, under):
    """A prop of his kit: the long sword sheathed across his back (its scabbard empty once drawn), the dagger, the
    needles or the drawn long sword in a fist. A held one hangs from its hand's prop bone, so his clips carry it."""
    kind = prop["kind"]
    H = dims["height"]
    if kind == "swordBack":
        return scabbard(S, L, dims, mats, under, prop.get("empty", False))
    side = side_of(prop)
    if kind not in HELD:
        return None
    g = grips[side]
    bones = anatomy.rigid("prop_" + side)
    forward = V(1, 0, 0)
    up = V(0, 0, 1)
    parts = []

    def piece(name, a, b, radius, material, rounding=0.0, protect=1.0):
        parts.append(tree.leaf(S, name, lambda P, a=a, b=b: sdf.cylinder(P, a, b, radius, rounding), Box.around([a, b], radius + 1.0), material, bones, protect))

    def blade(name, a, length, stations, material):
        shape = sdf.Loft(a, a + forward * length, up, stations, 0.15)
        parts.append(tree.leaf(S, name, shape, Box.around(shape.bounds_points()), material, bones, protect=1.0))

    if kind == "dagger":
        # A drawn dagger: a short cord-wrapped grip through the fist, a small guard and a straight blade forward.
        piece("dagger_grip", g - forward * H * 0.03, g + forward * H * 0.028, H * 0.0062, mats["grip"])
        piece("dagger_pommel", g - forward * H * 0.036, g - forward * H * 0.029, H * 0.0085, mats["fitting"], H * 0.002)
        piece("dagger_guard", g + forward * H * 0.028, g + forward * H * 0.034, H * 0.012, mats["fitting"], H * 0.002)
        blade("dagger_blade", g + forward * H * 0.033, H * 0.11, [(0.0, 0, 0, H * 0.011, H * 0.0038), (0.65, -H * 0.001, 0, H * 0.0085, H * 0.0034),
                                                                 (1.0, -H * 0.004, 0, H * 0.0012, H * 0.0015)], mats["steel"])
    elif kind == "needles":
        # Thin throwing needles fanned forward from between his fingers.
        for k, turn in enumerate((-14.0, 0.0, 14.0)):
            a = np.radians(turn)
            d = unit(V(np.cos(a), np.sin(a), -0.12))
            root = g + forward * H * 0.012
            tip = root + d * H * 0.11
            parts.append(tree.leaf(S, "needle_%d" % k, lambda P, a=root, b=tip: sdf.round_cone(P, a, b, H * 0.0042, H * 0.0018), Box.around([root, tip], 1.5),
                                   mats["edge"], bones, protect=1.0))
    elif kind == "longSword":
        # The long single-edged sword drawn: a cord-wrapped grip through the fist, a round guard, and a long blade
        # sweeping forward in a slight rising curve, its pale edge down.
        piece("sword_grip", g - forward * H * 0.07, g + forward * H * 0.04, H * 0.0072, mats["grip"])
        piece("sword_pommel", g - forward * H * 0.077, g - forward * H * 0.069, H * 0.0088, mats["fitting"], H * 0.002)
        piece("sword_guard", g + forward * H * 0.04, g + forward * H * 0.05, H * 0.026, mats["fitting"], H * 0.003)
        root = g + forward * H * 0.049
        length = H * 0.55
        rise = H * 0.055
        blade("sword_blade", root, length, [(0.0, 0.0, 0, H * 0.016, H * 0.0042), (0.5, rise * 0.25, 0, H * 0.0155, H * 0.004),
                                            (0.88, rise * 0.77, 0, H * 0.013, H * 0.0036), (1.0, rise * 1.05, 0, H * 0.003, H * 0.0022)], mats["steel"])
        blade("sword_edge", root + forward * H * 0.01, length * 0.97, [(0.0, -H * 0.013, 0, H * 0.0045, H * 0.003), (0.5, rise * 0.25 - H * 0.0125, 0, H * 0.0045, H * 0.003),
                                                                       (0.88, rise * 0.77 - H * 0.0095, 0, H * 0.0038, H * 0.0028),
                                                                       (1.0, rise * 1.05 - H * 0.002, 0, H * 0.0015, H * 0.0018)], mats["edge"])
    return Union(parts, k=0.1)


def scabbard(S, L, dims, mats, under, empty):
    """The long sword's scabbard across his back, as his kit carries it: its mouth behind his left shoulder and its tip
    at his right hip, lying over his cloak-tail; black lacquer with pale fittings. Sheathed, the cord-wrapped grip and
    round guard stand up over his shoulder; drawn (empty), only the scabbard is left."""
    H = dims["height"]
    h = H / 171.0
    s0 = V(*L["spine_03"][0])
    torso, shoulder = dims["torso"], dims["shoulder"]
    mouth = s0 + V(-shoulder * 0.75, shoulder * 0.55, torso * 0.42)
    end = s0 + V(-shoulder * 0.75, -shoulder * 0.8, torso * 0.75 * -1.0)
    radius = H * 0.016
    # Laid over his back: each station out from the worn surface behind it by its radius and the cloak's room.
    shares = np.linspace(0.0, 1.0, 6)
    line = np.array([mouth + (end - mouth) * s for s in shares])
    starts = line * V(0, 1, 1) + V(-60.0, 0, 0)
    hits, _normals = garments.surface_points(under, starts, np.tile(V(1, 0, 0), (len(line), 1)), reach=70.0)
    clearance = radius + (1.0 + 1.4 * shares) * h
    xs = np.minimum(hits[:, 0] - clearance, line[:, 0])
    # The mouth sits clear of his hair and hood: no further forward than the stations below it allow.
    xs[0] = min(xs[0], xs[1])
    points = [V(x, p[1], p[2]) for x, p in zip(xs, line)]
    along = unit(points[0] - points[1])
    bones = anatomy.rigid("spine_03")
    radii = [radius * (1.0 - 0.25 * s) for s in shares]
    parts = [tree.leaf(S, "scabbard", lambda P: sdf.tube(P, points, radii), Box.around(points, radius + 1.0), mats["lacquer"], bones, protect=0.8)]
    first = points[0]
    for name, a, b, r in (("scabbard_mouth", first - along * 0.4 * h, first + along * 2.6 * h, radius * 1.18),
                          ("scabbard_band", points[2] + along * 1.0 * h, points[2] - along * 1.0 * h, radii[2] * 1.15),
                          ("scabbard_chape", points[-1] + unit(points[-2] - points[-1]) * 3.5 * h, points[-1] - unit(points[-2] - points[-1]) * 0.6 * h, radii[-1] * 1.15)):
        parts.append(tree.leaf(S, name, lambda P, a=a, b=b, r=r: sdf.cylinder(P, a, b, r, 0.3 * h), Box.around([a, b], r + 1.0), mats["fitting"], bones, protect=0.8))
    if not empty:
        guard_a, guard_b = first + along * 2.4 * h, first + along * 3.8 * h
        parts.append(tree.leaf(S, "sword_guard_back", lambda P: sdf.cylinder(P, guard_a, guard_b, H * 0.027, 0.4 * h), Box.around([guard_a, guard_b], H * 0.03 + 1.0),
                               mats["fitting"], bones, protect=0.8))
        grip_a, grip_b = guard_b, guard_b + along * H * 0.12
        parts.append(tree.leaf(S, "sword_grip_back", lambda P: sdf.cylinder(P, grip_a, grip_b, H * 0.0085, 0.4 * h), Box.around([grip_a, grip_b], 2.0),
                               mats["grip"], bones, protect=0.8))
        cap_a, cap_b = grip_b - along * 0.3 * h, grip_b + along * 1.2 * h
        parts.append(tree.leaf(S, "sword_pommel_back", lambda P: sdf.cylinder(P, cap_a, cap_b, H * 0.0098, 0.4 * h), Box.around([cap_a, cap_b], 2.0),
                               mats["fitting"], bones, protect=0.8))
    return Union(parts, k=0.15)


# ---------------------------------------------------------------------------------------------- cloth sheets
def cloak(S, L, dims, mats, worn):
    """His red cloak-tail: lying on his shoulder blades under the hood, falling behind him nearly to his calves,
    flaring wide at both sides as it falls, its hem torn into long ragged tongues."""
    H = dims["height"]
    cz = L["spine_03"][1][2]
    material = S.material("cloak_sheet", PALETTE["red"])
    columns, rows, strips = 20, 8, 10
    t0, t1 = np.radians(112.0), np.radians(248.0)
    pz = L["pelvis"][0][2]
    # Clear of his arms, and of his seat at the back, where the loose trousers stand furthest out.
    limbs = [(L["upperarm_" + s][0], L["upperarm_" + s][1], H * 0.045) for s in ("l", "r")] + [(L["lowerarm_" + s][0], L["lowerarm_" + s][1], H * 0.04)
                                                                                               for s in ("l", "r")]
    limbs.append((V(-H * 0.012, 0, pz - H * 0.08), V(-H * 0.012, 0, pz + H * 0.1), H * 0.07))
    samples = np.linspace(0.0, 1.0, 13)
    theta_s = t0 + (t1 - t0) * samples
    inward = -np.stack([np.cos(theta_s), np.sin(theta_s), np.zeros_like(theta_s)], axis=1)
    z = cz - H * 0.035 - H * 0.012 * np.abs(np.sin(theta_s))
    centres = np.stack([np.zeros_like(z), np.zeros_like(z), z], axis=1)
    hits, _normals = garments.surface_points(worn, centres - inward * 45.0, inward, reach=45.0)
    tops = hits - inward * 0.8
    phase = np.random.default_rng(115).uniform(0, 2 * np.pi, 3)
    hem = H * 0.13

    def position(u, v):
        theta = t0 + (t1 - t0) * u
        side = np.abs(np.sin(theta))
        top = np.stack([np.interp(u, samples, tops[:, k]) for k in range(3)], axis=1)
        radial = np.stack([np.cos(theta), np.sin(theta), np.zeros_like(theta)], axis=1)
        flare = (H * 0.05 + H * 0.2 * side) * v ** 1.3
        pleat = (0.4 + H * 0.022 * v) * (0.55 * np.sin(theta * 10.0 + phase[0]) + 0.3 * np.sin(theta * 15.0 + phase[1]) + 0.25 * np.sin(theta * 6.0 + phase[2]))
        p = top + radial * (flare + pleat)[:, None]
        p[:, 2] = top[:, 2] + (hem - top[:, 2]) * v
        return sheet.clear_of(p, limbs, 1.5)
    chains = {prefix: [V(*L["%s_%02d" % (prefix, i)][0]) for i in (1, 2, 3)] + [V(*L[prefix + "_end"][0])] for prefix in ("cape_l", "cape", "cape_r")}
    across = sheet.sweep_shares(chains, t0, t1)

    def bones(P, u, v):
        hold = np.clip(1 - v / 0.1, 0, 1)
        return sheet.down_chains(chains, across, P, u, hold, "spine_03")
    return sheet.Sheet("cloak", position, material, bones, columns, rows, reach=lambda u: sheet.torn(u, strips, 0.62, 0.2, 15.0))


def robe_tails(S, L, dims, mats, worn, side):
    """One of his robe's black tails: a panel from under the sash to above his knee over his hip, from the side of his
    thigh round toward his back (the cloak-tail hangs between the two), its hem torn into tongues; it hangs on its own
    side's chain (ADR-069 §7), its top held at his hips."""
    H = dims["height"]
    material = S.material("robe_tail_" + side, PALETTE["cloth"])
    columns, rows, strips = 8, 4, 4
    t0, t1 = (np.radians(78.0), np.radians(152.0)) if side == "l" else (np.radians(208.0), np.radians(282.0))
    top_z = waist_z(L) - H * 0.026
    hem_z = L["calf_l"][0][2] + H * 0.04
    limbs = [(L["thigh_" + s][0], L["thigh_" + s][1], H * 0.06) for s in ("l", "r")]
    samples = np.linspace(0.0, 1.0, 9)
    theta_s = t0 + (t1 - t0) * samples
    inward = -np.stack([np.cos(theta_s), np.sin(theta_s), np.zeros_like(theta_s)], axis=1)
    centres = np.stack([np.zeros_like(theta_s), np.zeros_like(theta_s), np.full_like(theta_s, top_z)], axis=1)
    hits, _normals = garments.surface_points(worn, centres - inward * 40.0, inward, reach=40.0)
    tops = hits - inward * 0.6

    def position(u, v):
        theta = t0 + (t1 - t0) * u
        radial = np.stack([np.cos(theta), np.sin(theta), np.zeros_like(theta)], axis=1)
        top = np.stack([np.interp(u, samples, tops[:, k]) for k in range(3)], axis=1)
        p = top + radial * (H * 0.025 * v ** 1.3 + (0.3 + H * 0.012 * v) * np.sin(theta * 9.0))[:, None]
        p[:, 2] = top_z + (hem_z - top_z) * v
        return sheet.clear_of(p, limbs, 1.2)

    def bones(P, u, v):
        hold = np.clip(1 - v / 0.15, 0, 1)
        lower = np.clip((v - 0.25) / 0.5, 0, 1) * (1 - hold)
        upper = (1 - hold) - lower
        return {"pelvis": hold.astype(np.float32), "coat_%s_01" % side: upper.astype(np.float32), "coat_%s_02" % side: lower.astype(np.float32)}
    return sheet.Sheet("robe_tail_" + side, position, material, bones, columns, rows, reach=lambda u: sheet.torn(u, strips, 0.7, 0.12, 4.0 + (side == "r")))

def sash_end(S, L, dims, mats, worn, k):
    """One of the red sash's torn ends hanging from its knot at his left hip, on his left tail's chain, so it trails
    as he moves."""
    H = dims["height"]
    material = S.material("sash_end_%d" % k, PALETTE["red"])
    waist = waist_z(L)
    angle = np.radians(70.0 + 14.0 * k)
    out = V(np.cos(angle), np.sin(angle), 0.0)
    hit, _n = garments.surface_points(worn, (V(0, 0, waist - H * 0.01) + out * 40.0)[None, :], (-out)[None, :], reach=40.0)
    root = hit[0] + out * 2.0
    length = H * (0.24 if k == 0 else 0.17)
    width = H * 0.04
    across = unit(np.cross(out, V(0, 0, 1)))
    lean = out * (0.3 + 0.06 * k) + V(-0.22, 0, 0)
    columns, rows = 2, 6

    def position(u, v):
        return root + ((u - 0.5) * width * (1.0 - 0.3 * v))[:, None] * across + v[:, None] * (V(0, 0, -length) + lean * length)

    def bones(P, u, v):
        hold = np.clip(1 - v / 0.15, 0, 1)
        lower = np.clip((v - 0.3) / 0.5, 0, 1) * (1 - hold)
        return {"pelvis": hold.astype(np.float32), "coat_l_01": ((1 - hold) - lower).astype(np.float32), "coat_l_02": lower.astype(np.float32)}
    return sheet.Sheet("sash_end_%d" % k, position, material, bones, columns, rows, reach=lambda u: sheet.torn(u, 1, 0.85, 0.15, 7.0 + k))
