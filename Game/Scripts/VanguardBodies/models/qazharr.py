"""Qazharr, The Harbor Wolf (ADR-069): a harbour local who used to be a pirate, read by his silhouette from the game
camera. Character Bible §13 and his splash art: a big, broad, weathered sailor, heavy through the chest and shoulders,
built by work; long dark curly hair and a full beard, laughing; heavy blue-black nautical tattoo work in coils across his
chest and waves round his forearms. A layered, salvaged navy coat open over the bare chest, a ragged shoulder cape over
it, its sleeves rolled to the elbow, its long skirt to below the knee and torn at the hem; a coil of rope over his left
shoulder; a broad belt with an anchor cast into its brass plate over a red sash knotted at his left hip; a medallion, an
earring, rings, wrist wraps; baggy trousers into heavy buckled boots. His boarding blade is oversized and single-edged, a cleaver's broad
head on a cord-bound haft, and the back of it is an anchor's fluke: harbour salvage reforged. Nothing theatrical: no
tricorn, no flag, no coat of office.

Low poly and flat-coloured (author 2026-10-07): the big forms that make his outline, each a flat colour the toon
material shades. He rests in the A pose, the blade in his right fist pointing ahead of it, edge down (the humanoid's
boarding blade, ADR-064): his shoulder-carry clips fold the arm so it lies back over his right shoulder, edge up, and No
Quarter's clips swing it in the hand. His coat's skirt and the sash's tails hang on the coat's spring chains, his locks
on the hair's (ADR-069 §7). Colours are his kit's, lifted so they read under the toon light and moved toward his art."""
import numpy as np

from ..sculpt import anatomy, garments, hair, paint, sdf, sheet, tree
from ..sculpt.anatomy import V, unit
from ..sculpt.garments import band_z, both, bunch, either, folds, half, without
from ..sculpt.tree import Box, Over, Placed, Shell, Union, Zone

# The hand, wrist to fingertip, as a share of the height: big working hands.
HAND_SHARE = 0.108
# The skull his hair grows over, in the head's frame at a 24 cm template's scale, and how far beyond it a lock has
# passed wholly to its hair chain (cm).
SKULL_CENTRE = (-1.2, 0.0, 14.4)
SKULL_RADII = (9.9, 7.5, 8.0)
HAIR_REACH = 5.0
# His broad belt high on his waist, as a share of the torso above the pelvis.
BELT_SHARE = 0.33

# His kit's colours (sRGB) moved toward his art and lifted where dark: navy cloth and near-black hair go black under the
# toon light below about 0.2.
PALETTE = {
    "skin": (0.66, 0.46, 0.34), "hair": (0.2, 0.15, 0.13), "ink": (0.16, 0.2, 0.33), "coat": (0.22, 0.27, 0.36), "mantle": (0.18, 0.22, 0.3),
    "trim": (0.4, 0.28, 0.2), "cuff": (0.85, 0.81, 0.73), "trousers": (0.27, 0.26, 0.27), "boot": (0.34, 0.24, 0.18),
    "sole": (0.2, 0.15, 0.12), "leather": (0.42, 0.29, 0.2), "sash": (0.62, 0.13, 0.12), "rope": (0.72, 0.56, 0.32), "rope_dark": (0.55, 0.41, 0.24),
    "brass": (0.8, 0.62, 0.3), "bronze": (0.42, 0.3, 0.17), "teeth": (0.93, 0.9, 0.84), "steel": (0.6, 0.61, 0.62),
    "edge": (0.88, 0.88, 0.86), "iron": (0.32, 0.31, 0.32), "wood": (0.4, 0.27, 0.17), "cord": (0.7, 0.6, 0.42),
}


def materials(S):
    """Every material Qazharr is coloured in, flat (the toon material shades it)."""
    return {name: S.material(name, colour) for name, colour in PALETTE.items()}


def build(S, L, dims, spec):
    """Qazharr's sculpt on his layout: (the whole, {"body": the skin alone, under his clothes, "sheets": his cloth})."""
    mats = materials(S)
    H = dims["height"]
    # Heavy through the chest, shoulders and arms, thick in the neck and waist: built by work, not by training.
    figure = anatomy.Figure(S, L, dims, mats["skin"], {"muscle": 1.0, "chest": 1.0, "breadth": 1.2, "hips": 0.82, "limb": 1.25,
                                                        "deltoid": 1.25, "leg": 1.2, "neck": 1.3}).build()
    head_origin = V(*L["head"][0])
    size = (L["head"][1][2] - L["head"][0][2]) * 24.0 / 21.9
    u = size / 24.0
    skull = anatomy.Head(S, mats["skin"], size, look={"jaw": 1.12}).build()
    figure.attach_head(Placed(Over([skull, beard(S, mats, skull, u), mane(S, L, mats, spec, head_origin, u), earring(S, mats, u)]), head_origin, np.eye(3)))
    grip = None
    for side in ("l", "r"):
        placed, held = hand(S, L, H, side, mats)
        figure.parts.append(placed)
        figure.limbs["hand_" + side] = placed
        grip = held if side == "r" else grip
    body = figure.body()
    dressed = clothes(S, L, dims, mats, body, figure.limbs)
    worn = Over([dressed, gear(S, L, dims, mats, dressed, figure.limbs), blade(S, mats, H, grip)])
    skirt, tails = coat_skirt(S, L, dims, mats, dressed)
    return worn, {"body": body, "sheets": [skirt, tails]}


# ---------------------------------------------------------------------------------------------- head
def beard(S, mats, skull, u):
    """A full dark beard over his jaw, cheeks and chin, heavy below the chin, and a moustache; his mouth open in a laugh
    through it, a bar of teeth showing. In the head's frame."""
    bones = anatomy.rigid("head")

    def region(P):
        x, z = P[:, 0] / u, P[:, 2] / u
        # Up the cheeks to the sideburns in front of the ears, down to under the nose at the front.
        top = np.interp(x, [0.5, 4.0, 7.6, 12.0], [8.0, 7.2, 5.1, 5.1])
        return np.maximum(z - top, 0.5 - x) * u

    # His laugh: the mouth open wide through the beard, a broad bar of teeth in it.
    mouth = Zone(lambda P: sdf.ellipsoid(P, V(10.4, 0, 3.2) * u, V(2.6, 2.9, 1.35) * u), Box(V(7, -4, 1.0) * u, V(14, 4, 6.0) * u))
    area = without(Zone(region, Box(V(0, -12, -6) * u, V(14, 12, 10) * u)), mouth)
    # Fuller toward the chin, thin round the mouth.
    full = lambda P: (0.5 + 1.5 * np.clip((2.2 - P[:, 2] / u) / 4.0, 0, 1)) * u  # noqa: E731
    shell = Shell(S, "beard", skull, 0.2 * u, 0.5 * u, area, mats["hair"], hem=0.3 * u, bones=bones, displace=full, reach=2.2 * u)
    chin = tree.leaf(S, "beard_chin", lambda P: sdf.ellipsoid(P, V(6.4, 0, -0.2) * u, V(4.0, 5.4, 3.2) * u), Box(V(1, -7, -5) * u, V(12, 7, 5) * u),
                     mats["hair"], bones, protect=0.6)
    teeth = tree.leaf(S, "teeth", lambda P: sdf.ellipsoid(P, V(9.7, 0, 3.45) * u, V(1.0, 2.3, 0.8) * u), Box(V(8, -3.5, 2.0) * u, V(11.5, 3.5, 5.0) * u),
                      mats["teeth"], bones, protect=1.0)
    # Heavy dark brows raised in the laugh: with the beard and the teeth, what reads of his face from above.
    features = []
    for sign in (1.0, -1.0):
        a, b = V(9.7, sign * 1.0, 12.5) * u, V(8.6, sign * 5.0, 13.3) * u
        features.append(tree.leaf(S, "brow_%d" % (sign > 0), lambda P, a=a, b=b: sdf.round_cone(P, a, b, 1.0 * u, 0.75 * u), Box.around([a, b], 1.4 * u),
                                  mats["hair"], bones, protect=1.0))

    return Over([Union([shell, chin], k=1.2 * u), teeth] + features)


def mane(S, L, mats, spec, origin, u):
    """Long dark curls swept back off his face and falling behind his ears to his shoulders, full and lumpy at the
    edges; the locks sway on the hair's chains where his kit gives them (ADR-069 §7), the cap over the scalp stays with
    the head. In the head's frame."""
    locks_bones = anatomy.rigid("head")
    chains = sorted(bone[:-3] for bone in L if bone.startswith("hair_") and bone.endswith("_01"))
    if chains:
        joints = {chain: [L["%s_%s" % (chain, joint)][0] for joint in ("01", "02", "end")] for chain in chains}
        locks_bones = hair.on_chains(joints, "head", origin + V(*SKULL_CENTRE) * u, V(*SKULL_RADII) * u, HAIR_REACH)
    # The cap over the scalp, from a hairline high on the brow to low at the nape.
    cap = hair.messy(S, mats["hair"], locks_bones, SKULL_CENTRE, SKULL_RADII, (17.4, 3.0), seed=spec["seed"], unit_scale=u, count=0,
                     cap_bones=anatomy.rigid("head"))
    c, r = V(*SKULL_CENTRE), V(*SKULL_RADII)
    rng = np.random.default_rng(spec["seed"] + 7)
    locks = []
    # Swept back: each lock from the hairline over the top of the skull to the back of the head, then falling down his
    # back in curls to the shoulders.
    for i, gamma in enumerate(np.linspace(-60.0, 60.0, 9)):
        g = np.radians(gamma + rng.uniform(-4, 4))
        start = np.degrees(np.arcsin(np.clip(3.1 / (r[2] * np.cos(g)), 0, 1)))
        over = []
        wobble = rng.uniform(0, 6.28)
        for t in np.linspace(0.0, 1.0, 8):
            b = np.radians(start + (205.0 - start) * t)
            # Lumpy as curls are: each lock rising and falling and wandering a little across as it goes over.
            gg = g + np.radians(6.0) * np.sin(t * 9.0 + wobble)
            d = V(np.cos(b) * np.cos(gg), np.sin(gg), np.sin(b) * np.cos(gg))
            over.append(c + d * (r + 1.6 + 2.2 * t + 0.8 * np.sin(t * 11.0 + wobble)))
        fall = unit(V(-0.35, np.sin(g) * 0.5, -1.0))
        locks.append(lock(S, "lock_%d" % i, over, fall, rng.uniform(18.0, 23.0), rng.uniform(3.0, 3.6), rng.uniform(0, 6.28), mats["hair"], locks_bones, u))
    # Falling behind the ears and round the back from under the swept locks, fanning out over his shoulders; his left
    # ear (and its ring) left clear.
    for i, azimuth in enumerate((126.0, 146.0, 166.0, 186.0, 206.0, 226.0, 246.0, 262.0)):
        a = np.radians(azimuth + rng.uniform(-5, 5))
        e = np.radians(rng.uniform(6.0, 22.0))
        root = c + V(np.cos(a) * np.cos(e) * r[0], np.sin(a) * np.cos(e) * r[1], np.sin(e) * r[2]) * 1.08
        fall = unit(V(np.cos(a) * 0.35 - 0.2, np.sin(a) * 0.45, -1.0))
        locks.append(lock(S, "fall_%d" % i, [root], fall, rng.uniform(20.0, 26.0), rng.uniform(3.2, 3.8), rng.uniform(0, 6.28), mats["hair"], locks_bones, u))
    return Union([cap] + locks, k=1.0 * u)


def lock(S, name, start, fall, length, radius, phase, material, bones, u):
    """A curly lock (in a 24 cm head's units, scaled by u): along the points start, then falling along fall for length,
    swinging side to side as it falls (its curls, at a distance) and tapering to a tip that curls out."""
    side = unit(np.cross(fall, V(0, 0, 1)))
    other = unit(np.cross(fall, side))
    head = [V(*p) for p in start]
    t = np.linspace(0.0, 1.0, 8)[1:]
    swing = 1.4 * np.sin(t * 2.2 * np.pi * 2 + phase) * np.clip(t * 2.5, 0, 1)
    sway = 0.9 * np.cos(t * 2.2 * np.pi * 2 + phase) * np.clip(t * 2.5, 0, 1)
    tail = [head[-1] + fall * length * s + side * a + other * b for s, a, b in zip(t, swing, sway)]
    # The tip curls out and up a little, ending in a curl.
    tail[-1] = tail[-1] + other * -1.8 + V(0, 0, 1.6)
    points = [p * u for p in head + tail]
    share = np.linspace(0.0, 1.0, len(points))
    radii = [radius * u * f for f in np.interp(share, [0.0, 0.25, 0.7, 0.9, 1.0], [0.85, 1.0, 0.85, 0.6, 0.45])]
    end = points[-1]
    curl = radius * u * 0.55

    def shape(P, pts=points, rs=radii):
        return np.minimum(sdf.tube(P, pts, rs), sdf.sphere(P, end, curl))
    return tree.leaf(S, name, shape, Box.around(points, radius * u * 1.3), material, bones)


def earring(S, mats, u):
    """A brass ring through his left ear's lobe. In the head's frame."""
    c = V(-0.8, 7.3, 5.0) * u
    axes = np.stack([V(1, 0, 0), V(0, 0, 1), V(0, -1, 0)], axis=1)
    return tree.leaf(S, "earring", lambda P: sdf.torus(P, c, 1.1 * u, 0.42 * u, axes), Box(c - 2 * u, c + 2 * u), mats["brass"], anatomy.rigid("head"), protect=1.0)


# ---------------------------------------------------------------------------------------------- hands
def hand(S, L, H, side, mats):
    """A hand at rest: the right a fist round the blade's haft, which runs ahead through it, the back of the hand out
    and the thumb forward; the left open and loose, palm in. (its placed node, the haft's line through the fist: a point
    on it and its way, for the right; None for the left)."""
    w0, w1 = V(*L["hand_" + side][0]), V(*L["hand_" + side][1])
    x = unit(w1 - w0)
    if side == "r":
        z = unit(np.cross(x, V(1, 0, 0)))
        curl = {"index": (65, 90, 50), "middle": (70, 95, 50), "ring": (72, 95, 50), "little": (75, 95, 45)}
        thumb = (60, 30, 30, 25)
    else:
        out = V(0, 1, 0)
        z = unit(out - x * (out @ x))
        curl = {"index": (18, 22, 12), "middle": (22, 28, 14), "ring": (25, 30, 15), "little": (28, 32, 16)}
        thumb = (35, 15, 20, 15)
    y = np.cross(z, x)
    built = anatomy.Hand(S, mats["skin"], side, H * HAND_SHARE, "hand_" + side, curl=curl, thumb=thumb)
    axes = np.stack([x, y, z], axis=1)
    node = built.build()
    if side == "l":
        # Brass rings on his left hand's index and ring fingers.
        rings = []
        for finger in ("index", "ring"):
            a, b = built.tips[finger][0], built.tips[finger][1]
            c0, c1 = a + (b - a) * 0.42, a + (b - a) * 0.62
            rings.append(tree.leaf(S, "ring_" + finger, lambda P, c0=c0, c1=c1: sdf.cylinder(P, c0, c1, 1.3 * built.u, 0.15), Box.around([c0, c1], 2.0 * built.u),
                                   mats["brass"], anatomy.rigid("hand_l"), protect=1.0))
        node = Over([node] + rings)
    placed = Placed(node, w0, axes)
    held = None
    if side == "r":
        # The hole the curled fingers close round, in the hand's frame: between the palm and the fingers' middle bones.
        held = (w0 + axes @ (V(8.7, 0.0, -2.6) * built.u), y)
    return placed, held


# ---------------------------------------------------------------------------------------------- clothes
def clothes(S, L, dims, mats, body, limbs):
    """His clothes over his skin, each keeping to its limbs: tattoos on the skin; baggy trousers into heavy boots; the
    navy coat open over his bare chest, its sleeves rolled to the elbow over a pale cuff; wrist wraps; belts over a red
    sash."""
    H = dims["height"]
    pz = L["pelvis"][0][2]
    cz = L["spine_03"][1][2]
    torso = cz - pz
    inked = Over([body] + tattoos(S, L, dims, mats, body, limbs))
    legs = both(band_z(H * 0.15, pz + torso * 0.25), garments.keep_to(limbs, ["leg_l", "leg_r", "torso"]))
    trousers = Shell(S, "trousers", body, 1.2, 1.2, legs, mats["trousers"], hem=0.4,
                     displace=garments.combine(folds((0, 0, 1), 7, 0.8, seed=131), bunch((0, 0, H * 0.2), (0, 0, H * 0.3), 3.0, 1.2, seed=133)), reach=2.2)
    boot_mats = {"boot": mats["boot"], "sole": mats["sole"]}
    top = H * 0.205
    boots = [garments.boot(S, "boot_" + side, L, side, top, boot_mats, None, shaft=1.32, toe=1.2) for side in ("l", "r")]
    cuffs = []
    for side in ("l", "r"):
        knee, ankle = V(*L["calf_" + side][0]), V(*L["calf_" + side][1])
        a = ankle + (knee - ankle) * ((top - 3.0 - ankle[2]) / (knee[2] - ankle[2]))
        b = ankle + (knee - ankle) * ((top + 1.0 - ankle[2]) / (knee[2] - ankle[2]))
        cuffs.append(tree.leaf(S, "boot_cuff_" + side, lambda P, a=a, b=b: sdf.cylinder(P, a, b, 10.2, 0.8), Box.around([a, b], 11.0), mats["leather"], None))
        # Its buckle on the outside.
        buckle = (a + b) / 2 + V(0, (1.0 if side == "l" else -1.0) * 10.4, 0)
        cuffs.append(tree.leaf(S, "boot_buckle_" + side, lambda P, c=buckle: sdf.box(P, c, (1.7, 0.7, 1.9), None, 0.3), Box(buckle - 3, buckle + 3), mats["brass"], None,
                               protect=0.6))
    lower = Over([inked, trousers] + boots + cuffs)
    # The coat: open over the chest and belly in a wide V from the collar to the belt, over the shoulders and down the
    # upper arms to just above the elbow.
    opening = lambda z: np.interp(z, [pz, pz + torso * 0.5, cz - torso * 0.2, cz + 4.0], [11.0, 12.5, 13.5, 8.5])  # noqa: E731
    v_cut = Zone(lambda P: np.maximum(2.0 - P[:, 0], np.abs(P[:, 1]) - opening(P[:, 2])), Box((0, -16, pz), (40, 16, cz + 14)))
    sleeves = []
    for side in ("l", "r"):
        s0, e = V(*L["upperarm_" + side][0]), V(*L["upperarm_" + side][1])
        end = s0 + (e - s0) * 0.8
        sleeves.append(both(garments.keep_to(limbs, ["upperarm_" + side]), half(end, unit(e - s0), Box.around([s0, e], 18.0))))
    coat_region = without(either(both(band_z(pz + torso * 0.12, cz + H * 0.01), garments.keep_to(limbs, ["torso"])), *sleeves), v_cut)
    coat = Shell(S, "coat", lower, 0.6, 1.3, coat_region, mats["coat"], hem=0.5, displace=folds((0, 0, 1), 10, 0.6, seed=137), reach=1.0)
    # Its lapels: a band of leather trim along the opening's edges, turned back.
    lapel = without(both(Zone(lambda P: np.maximum(2.0 - P[:, 0], np.abs(P[:, 1]) - opening(P[:, 2]) - 3.2), Box((0, -20, pz + torso * 0.3), (40, 20, cz + 14))),
                         garments.keep_to(limbs, ["torso"])), v_cut)
    lapels = Shell(S, "lapels", lower, 0.9, 1.6, lapel, mats["trim"], hem=0.4, reach=0.5)
    # Layered: a short shoulder cape of darker cloth over the coat, down over the shoulders' caps, open wider than the
    # lapels at the front, its hem cut ragged.
    tongues = 18

    def mantle_hem(P):
        angle = np.arctan2(P[:, 1], P[:, 0])
        strip = (angle + np.pi) / (2.0 * np.pi) * tongues
        tip = 1.0 - np.abs(2.0 * (strip % 1.0) - 1.0)
        hem = cz - torso * 0.2 - torso * 0.1 * np.abs(np.sin(angle)) - (1.5 + 3.0 * paint.hashed(np.floor(strip), 7.0)) * tip ** 0.7
        return np.maximum(hem - P[:, 2], P[:, 2] - (cz + 6.0))
    shoulder = abs(L["upperarm_l"][0][1])
    mantle_cut = Zone(lambda P: np.maximum(2.0 - P[:, 0], np.abs(P[:, 1]) - opening(P[:, 2]) - 3.4), Box((0, -20, pz), (40, 20, cz + 14)))
    mantle_region = without(both(Zone(mantle_hem, Box((-40, -shoulder - 22, cz - torso * 0.45), (40, shoulder + 22, cz + 8))),
                                 garments.keep_to(limbs, ["torso", "upperarm_l", "upperarm_r"])), mantle_cut)
    mantle = Shell(S, "mantle", lower, 1.9, 1.4, mantle_region, mats["mantle"], hem=0.5, displace=folds((0, 0, 1), 12, 0.8, seed=139), reach=1.0)
    # The collar turned up behind his neck, open at the throat.
    collar = Zone(lambda P: np.maximum(np.maximum(cz - 2.0 - P[:, 2], P[:, 2] - (cz + H * 0.05)), np.cos(np.arctan2(P[:, 1], P[:, 0] - 1.0)) - 0.35),
                  Box((-18, -18, cz - 4), (18, 18, cz + 16)))
    ring = tree.leaf(S, "collar_core", lambda P: sdf.round_cone(P, V(0.0, 0, cz - 3.0), V(-1.5, 0, cz + H * 0.05), 10.5, 12.5),
                     Box((-18, -18, cz - 6), (18, 18, cz + 16)), mats["coat"], None)
    inner = tree.leaf(S, "collar_inner", lambda P: sdf.round_cone(P, V(0.0, 0, cz - 4.0), V(-1.5, 0, cz + H * 0.055), 9.3, 11.3),
                      Box((-18, -18, cz - 6), (18, 18, cz + 16)), mats["coat"], None)
    collar_band = tree.Intersect(tree.Subtract(ring, inner), collar, 0.3)
    coated = Over([lower, coat, lapels, mantle, collar_band])
    # The shirt's cuff rolled thick at each elbow under the coat's sleeve, the forearm bare below it; wrist wraps.
    rolled, wraps = [], []
    for side in ("l", "r"):
        s0, e = V(*L["upperarm_" + side][0]), V(*L["upperarm_" + side][1])
        w = V(*L["lowerarm_" + side][1])
        # Flat-ended (a round cone's ends would run the band on past them by its radius).
        a, b = e + unit(s0 - e) * 7.0, e + unit(w - e) * 2.5
        region = both(Zone(lambda P, a=a, b=b: sdf.cylinder(P, a, b, 11.0), Box.around([a, b], 12.0)),
                      garments.keep_to(limbs, ["upperarm_" + side, "forearm_" + side]))
        rolled.append(Shell(S, "cuff_" + side, body, 1.4, 1.8, region, mats["cuff"], hem=0.5, displace=bunch(a, b, 2.0, 0.9, seed=141 if side == "l" else 142), reach=1.0))
        a, b = w + (e - w) * 0.02, w + (e - w) * 0.24
        region = both(Zone(lambda P, a=a, b=b: sdf.cylinder(P, a, b, 9.0), Box.around([a, b], 10.0)), garments.keep_to(limbs, ["forearm_" + side]))
        wraps.append(Shell(S, "wrap_" + side, body, 0.5, 1.3, region, mats["leather"], hem=0.3, displace=bunch(a, b, 3.0, 0.5, seed=143), reach=0.6))
    sleeved = Over([coated] + rolled + wraps)
    # A red sash wound round his waist under a broad belt, a second belt slung lower toward his left hip.
    belt_z = pz + torso * BELT_SHARE
    sash = Shell(S, "sash", sleeved, 0.5, 1.4, both(band_z(belt_z - H * 0.05, belt_z + H * 0.018), garments.keep_to(limbs, ["torso"])), mats["sash"],
                 hem=0.4, displace=folds((0, 0, 1), 9, 0.7, seed=149), reach=0.8)
    sashed = Over([sleeved, sash])
    belts = []
    for name, centre, drop, width in (("belt", belt_z, 0.0, H * 0.03), ("belt_low", pz + torso * 0.19, 0.12, H * 0.02)):
        band = both(garments.tilted_band(V(0, 0, centre), unit(V(0, drop, 1)), width, Box((-40, -40, centre - 12), (40, 40, centre + 12))),
                    garments.keep_to(limbs, ["torso"]))
        belts.append(Shell(S, name, sashed, 0.4, 1.3, band, mats["leather"], hem=0.25))
    return Over([sashed] + belts)


def tattoos(S, L, dims, mats, body, limbs):
    """His blue-black tattoo work, laid on the skin: a broad coil over each side of his chest, and waves round his
    forearms (a crested band at the wrist, a running wave above it). Big shapes, so the flat colour holds them."""
    cz = L["spine_03"][1][2]
    torso = cz - L["pelvis"][0][2]
    shoulder = abs(L["upperarm_l"][0][1])
    pitch, width, reach = 5.2, 2.5, 11.0
    coils = []
    for sign in (1.0, -1.0):
        c = V(0, sign * shoulder * 0.36, cz - torso * 0.22)

        def coil(P, c=c, sign=sign):
            dy, dz = (P[:, 1] - c[1]) * sign, P[:, 2] - c[2]
            r = np.hypot(dy, dz)
            turn = np.arctan2(dz, dy) / (2 * np.pi)
            t = r / pitch - turn
            band = np.abs((t - np.floor(t)) - 0.5) * pitch - width * 0.5
            # Its centre filled, a dot the coil winds out from.
            return np.maximum(np.minimum(band, r - width * 0.7), np.maximum(r - reach, 2.0 - P[:, 0]))
        coils.append(Zone(coil, Box((0, c[1] - 13, c[2] - 13), (40, c[1] + 13, c[2] + 13))))
    chest = both(either(*coils), garments.keep_to(limbs, ["torso"]))
    inks = [Shell(S, "ink_chest", body, 0.0, 0.45, chest, mats["ink"], hem=0.2)]
    for side in ("l", "r"):
        e, w = V(*L["lowerarm_" + side][0]), V(*L["lowerarm_" + side][1])
        axis = unit(w - e)
        length = np.linalg.norm(w - e)
        ref = unit(np.cross(axis, V(1, 0, 0)))
        other = np.cross(axis, ref)

        def waves(P, e=e, axis=axis, ref=ref, other=other, length=length):
            s = (P - e) @ axis / length
            phi = np.arctan2(P @ other - e @ other, P @ ref - e @ ref)
            crest = 0.5 - 0.08 * np.sin(phi * 4.0)
            wrist = np.maximum(crest - s, s - 0.86)
            run = np.abs(s - (0.24 + 0.06 * np.sin(phi * 3.0 + 1.0))) - 0.06
            return np.minimum(wrist, run) * length
        region = both(Zone(waves, Box.around([e, w], 12.0)), garments.keep_to(limbs, ["forearm_" + side]))
        inks.append(Shell(S, "ink_arm_" + side, body, 0.0, 0.45, region, mats["ink"], hem=0.2))
    for ink in inks:
        ink.part.protect = 0.6
    return inks


# ---------------------------------------------------------------------------------------------- gear
def gear(S, L, dims, mats, dressed, limbs):
    """The everyday harbour hardware on him: a coil of rope over his left shoulder, the anchor cast into his belt's
    brass plate, a medallion on a cord and the sash's knot at his left hip."""
    H = dims["height"]
    pz = L["pelvis"][0][2]
    cz = L["spine_03"][1][2]
    torso = cz - pz
    belt_z = pz + torso * BELT_SHARE
    parts = []
    # The rope coil: three loops slung over the left shoulder, lying on the coat down his chest and his back, his arm
    # through them (their bottoms pass under the armpit). Laid on the torso's skin (not on his hair, which falls near
    # them), the coat's thickness out.
    shoulder = abs(L["upperarm_l"][0][1])
    rope_r, coat = H * 0.0105, 3.9
    skin = limbs["torso"]
    # Each loop hangs to its own depth; alternate loops a shade darker, so the coil reads as loops, not a pad.
    loops = ((shoulder * 0.5, 29.0, 23.0), (shoulder * 0.6, 35.0, 29.0), (shoulder * 0.7, 31.0, 26.0), (shoulder * 0.8, 25.0, 20.0))
    for k, (y, front_low, back_low) in enumerate(loops):
        zs = np.linspace(cz - front_low, cz - 3.0, 5)
        front, n_front = garments.surface_points(skin, np.stack([np.full(5, 40.0), np.full(5, y), zs], axis=1), np.tile(V(-1.0, 0, 0), (5, 1)), reach=45.0)
        xs = np.linspace(7.0, -7.0, 4)
        top, n_top = garments.surface_points(skin, np.stack([xs, np.full(4, y), np.full(4, cz + 40.0)], axis=1), np.tile(V(0, 0, -1.0), (4, 1)), reach=45.0)
        zs = np.linspace(cz - 3.0, cz - back_low, 5)
        back, n_back = garments.surface_points(skin, np.stack([np.full(5, -40.0), np.full(5, y), zs], axis=1), np.tile(V(1.0, 0, 0), (5, 1)), reach=45.0)
        laid = np.vstack([front + n_front * (coat + rope_r), top + n_top * (coat + rope_r), back + n_back * (coat + rope_r)])
        # Closed under the arm.
        points = np.vstack([laid, V(-2.0, y + 2.0, cz - max(front_low, back_low) - 6.0)[None, :], laid[:1]])
        parts.append(tree.leaf(S, "rope_%d" % k, lambda P, pts=points: sdf.tube(P, list(pts), [rope_r] * len(pts)), Box.around(points, rope_r * 1.5),
                               mats["rope_dark" if k % 2 else "rope"], rope_weights(cz), protect=0.3))
    # The knot binding the coil on top of the shoulder.
    knot_at, knot_n = garments.surface_point(skin, V(-1.0, shoulder * 0.67, cz + 40.0), V(0, 0, -1), reach=45.0)
    knot = knot_at + knot_n * (coat + rope_r * 1.6)
    parts.append(tree.leaf(S, "rope_knot", lambda P: sdf.ellipsoid(P, knot, V(3.6, 5.0, 2.8)), Box(knot - 6, knot + 6), mats["rope"], rope_weights(cz), protect=0.3))
    # The belt's brass plate at the front, the anchor cast proud of it.
    front, _n = garments.surface_point(dressed, V(40.0, 0, belt_z), V(-1, 0, 0), reach=40.0)
    plate_c = front + V(0.6, 0, 0)
    half_y, half_z = H * 0.03, H * 0.024
    parts.append(tree.leaf(S, "belt_plate", lambda P: sdf.box(P, plate_c, (0.9, half_y, half_z), None, 0.4), Box(plate_c - 7, plate_c + 7), mats["brass"], None, protect=0.8))
    face = plate_c + V(0.9, 0, 0)
    anchor = []
    top_a, foot = face + V(0, 0, half_z * 0.62), face + V(0, 0, -half_z * 0.55)
    anchor.append(lambda P: sdf.capsule(P, top_a, foot, 0.42))
    stock_l, stock_r = face + V(0, half_y * 0.4, half_z * 0.42), face + V(0, -half_y * 0.4, half_z * 0.42)
    anchor.append(lambda P: sdf.capsule(P, stock_l, stock_r, 0.36))
    ring_c = face + V(0, 0, half_z * 0.78)
    anchor.append(lambda P: sdf.torus(P, ring_c, 0.75, 0.3, np.stack([V(0, 1, 0), V(0, 0, 1), V(1, 0, 0)], axis=1)))
    for sign in (1.0, -1.0):
        mid = face + V(0, sign * half_y * 0.42, -half_z * 0.48)
        tip = face + V(0, sign * half_y * 0.66, -half_z * 0.05)
        anchor.append(lambda P, a=foot, b=mid: sdf.capsule(P, a, b, 0.42))
        anchor.append(lambda P, a=mid, b=tip: sdf.round_cone(P, a, b, 0.42, 0.62))
    parts.append(tree.leaf(S, "belt_anchor", lambda P: np.min(np.stack([f(P) for f in anchor]), axis=0), Box(face - 6, face + 6), mats["bronze"], None, protect=1.0))
    # The medallion on its cord at the top of his breastbone.
    med_at, med_n = garments.surface_point(dressed, V(40.0, 0, cz - torso * 0.13), V(-1, 0, 0), reach=40.0)
    med_c = med_at + med_n * 0.5
    parts.append(tree.leaf(S, "medallion", lambda P: sdf.cylinder(P, med_c - med_n * 0.4, med_c + med_n * 0.5, H * 0.014, 0.25), Box(med_c - 5, med_c + 5),
                           mats["brass"], anatomy.rigid("spine_03"), protect=0.8))
    line = [V(-1.0, 7.5 * s, cz + 2.0) for s in (1.0, -1.0)]
    for k, start in enumerate(line):
        samples = [start + (med_c + V(0, 0, H * 0.014) - start) * t for t in np.linspace(0, 1, 6)]
        hits, normals = garments.surface_points(dressed, np.array(samples) + V(25.0, 0, 0), np.tile(V(-1.0, 0, 0), (6, 1)), reach=40.0)
        pts = list(hits + normals * 0.4)
        parts.append(tree.leaf(S, "cord_%d" % k, lambda P, pts=pts: sdf.tube(P, pts, [0.42] * len(pts)), Box.around(pts, 2.0), mats["leather"],
                               anatomy.rigid("spine_03"), protect=0.4))
    # The sash knotted at his left hip.
    angle = np.radians(58.0)
    out = V(np.cos(angle), np.sin(angle), 0.0)
    hip_at, hip_n = garments.surface_point(dressed, V(0, 0, belt_z - H * 0.03) + out * 45.0, -out, reach=45.0)
    knot_c = hip_at + hip_n * 1.5
    parts.append(tree.leaf(S, "sash_knot", lambda P: sdf.ellipsoid(P, knot_c, V(3.4, 3.4, 3.0)), Box(knot_c - 5, knot_c + 5), mats["sash"], None, protect=0.4))
    return Union(parts, k=0.2)


def rope_weights(cz):
    """The coil rides his upper body, its top on the left shoulder a little with the clavicle."""
    def weights(P):
        top = np.clip((P[:, 2] - (cz - 10.0)) / 14.0, 0, 1)
        top = top * top * (3 - 2 * top) * 0.6
        return {"spine_03": (1 - top).astype(np.float32), "clavicle_l": top.astype(np.float32)}
    return weights


# ---------------------------------------------------------------------------------------------- the blade
def polygon(x, y, verts):
    """Signed distance in a plane from (x, y) to a closed polygon through verts (in order): negative inside."""
    v = np.asarray(verts, dtype=np.float64)
    d = np.full(x.shape, np.inf)
    s = np.ones(x.shape)
    for i in range(len(v)):
        j = i - 1
        ex, ey = v[j, 0] - v[i, 0], v[j, 1] - v[i, 1]
        wx, wy = x - v[i, 0], y - v[i, 1]
        t = np.clip((wx * ex + wy * ey) / (ex * ex + ey * ey), 0.0, 1.0)
        d = np.minimum(d, (wx - ex * t) ** 2 + (wy - ey * t) ** 2)
        c1, c2, c3 = y >= v[i, 1], y < v[j, 1], ex * wy > ey * wx
        s = np.where((c1 & c2 & c3) | (~c1 & ~c2 & ~c3), -s, s)
    return s * np.sqrt(d)


def polyline(x, y, verts):
    """Distance in a plane from (x, y) to the open polyline through verts."""
    v = np.asarray(verts, dtype=np.float64)
    d = np.full(x.shape, np.inf)
    for i in range(len(v) - 1):
        ex, ey = v[i + 1, 0] - v[i, 0], v[i + 1, 1] - v[i, 1]
        wx, wy = x - v[i, 0], y - v[i, 1]
        t = np.clip((wx * ex + wy * ey) / (ex * ex + ey * ey), 0.0, 1.0)
        d = np.minimum(d, (wx - ex * t) ** 2 + (wy - ey * t) ** 2)
    return np.sqrt(d)


def extrude(d2, across, half, rounding):
    """A plane shape's distance d2 made a slab half thick either side of its plane (across: the distance off it), its
    edges rounded by rounding."""
    wx, wy = d2 + rounding, np.abs(across) - half + rounding
    return np.minimum(np.maximum(wx, wy), 0.0) + np.hypot(np.maximum(wx, 0.0), np.maximum(wy, 0.0)) - rounding


def blade(S, mats, H, grip):
    """His boarding blade, held in his right fist: a cord-bound wooden haft running ahead through the fist to an iron
    socket, then an oversized single-edged blade, its back straight and its pale edge falling away into a broad
    cleaver's head with a squared, raked tip; on its back near the head an anchor's fluke rises and sweeps back toward
    the haft to a pointed palm. Built at rest, edge down; it rides the right hand's prop bone, which his clips carry."""
    centre, along = grip
    along = unit(along)
    up = V(0, 0, 1)
    side = unit(np.cross(up, along))
    bones = anatomy.rigid("prop_r")
    parts = []

    def frame(P):
        Q = P - centre
        return Q @ along / H, Q @ up / H, Q @ side

    def piece(name, a, b, radius, material, rounding=0.0):
        pa, pb = centre + along * H * a, centre + along * H * b
        parts.append(tree.leaf(S, name, lambda P: sdf.cylinder(P, pa, pb, H * radius, rounding), Box.around([pa, pb], H * radius * 1.2), material, bones, protect=0.8))

    piece("haft", -0.05, 0.13, 0.0105, mats["wood"])
    piece("pommel", -0.07, -0.045, 0.016, mats["iron"], rounding=1.0)
    for i, start in enumerate((-0.04, 0.062, 0.088)):
        piece("binding_%d" % i, start, start + 0.016, 0.0125, mats["cord"], rounding=0.3)
    piece("socket", 0.112, 0.138, 0.0155, mats["iron"], rounding=0.5)
    # The blade's side, in shares of the height along the haft and up from its axis: the straight back, the raked tip,
    # the edge sweeping from the broad head back to the heel.
    edge_line = [(0.604, -0.168), (0.52, -0.178), (0.44, -0.16), (0.36, -0.122), (0.28, -0.088), (0.2, -0.062), (0.15, -0.047), (0.13, -0.03)]
    outline = [(0.13, 0.032), (0.598, 0.032), (0.628, -0.04)] + edge_line
    half = H * 0.0058
    box = Box.around([centre + along * H * 0.1 - up * H * 0.2 - side * 4, centre + along * H * 0.66 + up * H * 0.16 + side * 4])

    def steel(P):
        a, b, c = frame(P)
        return extrude(polygon(a, b, outline) * H, c, half, 0.3)

    def edge(P):
        a, b, c = frame(P)
        d2 = np.maximum(polygon(a, b, outline), polyline(a, b, edge_line) - 0.022) * H
        return extrude(d2, c, half + 0.2, 0.3)
    parts.append(tree.leaf(S, "blade", steel, box, mats["steel"], bones, protect=0.8))
    edge_part = tree.leaf(S, "blade_edge", edge, box, mats["edge"], bones, protect=0.8)
    # The fluke on its back: an iron arm rising from the back and sweeping toward the haft to a pointed palm.
    fluke_outline = [(0.468, 0.03), (0.54, 0.03), (0.552, 0.072), (0.532, 0.104), (0.49, 0.122), (0.45, 0.146), (0.352, 0.13),
                     (0.432, 0.09), (0.47, 0.088), (0.497, 0.072), (0.5, 0.048)]

    def fluke(P):
        a, b, c = frame(P)
        return extrude(polygon(a, b, fluke_outline) * H, c, H * 0.0075, 0.4)
    fluke_box = Box.around([centre + along * H * 0.33 - side * 5, centre + along * H * 0.58 + up * H * 0.17 + side * 5])
    fluke_part = tree.leaf(S, "fluke", fluke, fluke_box, mats["iron"], bones, protect=0.8)
    return Over([Union(parts, k=0.25), edge_part, fluke_part])


# ---------------------------------------------------------------------------------------------- cloth sheets
def coat_skirt(S, L, dims, mats, worn):
    """His coat's long skirt, from under the belt to below the knee, open at the front, flaring and pleated as it
    falls, its hem torn ragged; and the red sash's two tails hanging over it from the knot at his left hip. Each half
    hangs on its own coat chain (ADR-069 §7); the tails lie on the skirt's surface and move with it."""
    H = dims["height"]
    pz = L["pelvis"][0][2]
    torso = L["spine_03"][1][2] - pz
    top_z, hem_z = pz + torso * 0.21, H * 0.22
    t0, t1 = np.radians(32.0), np.radians(328.0)
    limbs = [(L["thigh_" + s][0], L["thigh_" + s][1], H * 0.074) for s in ("l", "r")] + [(L["calf_" + s][0], L["calf_" + s][1], H * 0.055) for s in ("l", "r")]
    samples = np.linspace(0.0, 1.0, 17)
    theta_s = t0 + (t1 - t0) * samples
    inward = -np.stack([np.cos(theta_s), np.sin(theta_s), np.zeros_like(theta_s)], axis=1)
    centres = np.tile(V(0, 0, top_z), (len(samples), 1))
    hits, _normals = garments.surface_points(worn, centres - inward * 50.0, inward, reach=50.0)
    tops = hits - inward * 0.8
    phase = np.random.default_rng(113).uniform(0, 2 * np.pi, 2)

    def shape(u, v, extra=0.0):
        theta = t0 + (t1 - t0) * u
        radial = np.stack([np.cos(theta), np.sin(theta), np.zeros_like(theta)], axis=1)
        top = np.stack([np.interp(u, samples, tops[:, k]) for k in range(3)], axis=1)
        flare = (H * 0.075 + H * 0.035 * np.abs(np.sin(theta))) * v ** 1.2
        pleat = (0.3 + 1.8 * v) * (0.6 * np.sin(theta * 8.0 + phase[0]) + 0.4 * np.sin(theta * 13.0 + phase[1]))
        p = top + radial * (flare + pleat + extra)[:, None]
        p[:, 2] = top_z + (hem_z - top_z) * v
        return sheet.clear_of(p, limbs, 1.5 + extra)

    def bones(P, u, v):
        left = P[:, 1] > 0
        hold = np.clip(1 - v / 0.15, 0, 1)
        lower = np.clip((v - 0.25) / 0.5, 0, 1) * (1 - hold)
        upper = (1 - hold) - lower
        w = {"pelvis": hold}
        for s, mask in (("l", left), ("r", ~left)):
            w["coat_%s_01" % s] = upper * mask
            w["coat_%s_02" % s] = lower * mask
        return {k: np.asarray(x, dtype=np.float32) for k, x in w.items()}

    skirt = sheet.Sheet("coat_skirt", lambda u, v: shape(u, v), S.material("coat_skirt", PALETTE["coat"]), bones, 18, 5,
                        reach=lambda u: sheet.torn(u, 9, 0.72, 0.12, 13.0))
    # The sash's tails: from the knot down over the skirt's left front, past its hem, two pointed tongues.
    ua = (np.radians(48.0) - t0) / (t1 - t0)
    ub = (np.radians(70.0) - t0) / (t1 - t0)
    at = lambda u: ua + (ub - ua) * u  # noqa: E731
    tails = sheet.Sheet("sash_tails", lambda u, v: shape(at(u), v * 1.2, 1.2), S.material("sash_tails", PALETTE["sash"]),
                        lambda P, u, v: bones(P, at(u), v * 1.2), 4, 6, reach=lambda u: sheet.torn(u, 2, 0.8, 0.2, 5.0))
    return skirt, tails
