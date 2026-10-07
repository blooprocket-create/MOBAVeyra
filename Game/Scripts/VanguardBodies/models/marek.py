"""Marek, The Black Accord (ADR-069): a pactcraft warlock, read by his silhouette from the game camera. Character Bible
§10 and his splash art: a lean young man with dark curly hair and violet eyes, delighted and unbothered; a tattered
black-and-violet coat hung with buckled straps and loops of heavy iron chain: a torn mantle and a high collar over his
shoulders, a long cloak torn into tongues behind him and the coat's long skirt open at the front, both lined in violet;
his right arm bare above the elbow under buckled straps, his left sleeved to it; dark trousers in heavy buckled boots.
Violet-black witchfire gathers in his hands and runs along the chain hanging from his right hand and the loops of it
over his coat: the pact made physical, and the chain his kit throws, swaps along and leashes Nix with.

Low poly and flat-coloured (author 2026-10-07): the big forms that make his outline, each a flat colour the toon
material shades. He rests in the A pose, the chain hanging from his right hand; his clips carry it. Every loose part
hangs on spring chains (ADR-069 §7): the cloak, the coat's skirt and his curls. Each cloth is two sheets, its black
face over its violet lining, weighted alike so they move as one.

Proportions are measured from the splash art against his height (its low camera corrected; the head fitted larger for
a high camera); colours are his kit's, lifted toward the art so they read under the toon light: the art is lit for
night."""
import numpy as np

from ..sculpt import anatomy, garments, hair, paint, sdf, sheet, tree
from ..sculpt.anatomy import V, unit
from ..sculpt.garments import around, band_z, both, either, folds, bunch, half
from ..sculpt.tree import Box, Over, Placed, Shell, Union, Zone

# The hand, wrist to fingertip, as a share of the height.
HAND_SHARE = 0.105
# The face's share of the head bone (chin to crown); his curls rise above it to the bone's top and past.
FACE_SHARE = 0.88
# The skull his hair grows over, in the head's frame at a 24 cm template's scale, and how far beyond it a lock has
# passed wholly to its hair chain (cm).
SKULL_CENTRE = (-1.2, 0.0, 14.4)
SKULL_RADII = (9.9, 7.5, 8.0)
HAIR_REACH = 4.5
# Heavy iron chain: the length of a link outside, as a share of the height, in his hand and over his coat.
CHAIN_LINK = 0.04
CHAIN_BODY_LINK = 0.03

# His kit's colours (sRGB), lifted toward the art: black cloth with a violet cast, violet lining, dark leather, silver
# buckles, iron chain and the witchfire's violet.
PALETTE = {
    "skin": (0.84, 0.68, 0.6), "hair": (0.2, 0.15, 0.18), "eyes": (0.42, 0.22, 0.66), "coat": (0.2, 0.16, 0.23),
    "coat_dark": (0.16, 0.13, 0.18), "lining": (0.46, 0.2, 0.62), "trousers": (0.23, 0.2, 0.24), "leather": (0.3, 0.21, 0.19),
    "boot": (0.22, 0.18, 0.19), "sole": (0.12, 0.1, 0.1), "glove": (0.18, 0.14, 0.18), "wrap": (0.3, 0.2, 0.34),
    "silver": (0.55, 0.53, 0.58), "iron": (0.3, 0.28, 0.32), "witchfire": (0.68, 0.32, 1.0),
}


def materials(S):
    """Every material Marek is coloured in, flat (the toon material shades it); the witchfire glows."""
    return {name: S.material(name, colour, glow=name == "witchfire") for name, colour in PALETTE.items()}


def build(S, L, dims, spec):
    """Marek's sculpt on his layout: (the whole, {"body": his skin alone, under his clothes, "sheets": his cloth})."""
    mats = materials(S)
    H = dims["height"]
    figure = anatomy.Figure(S, L, dims, mats["skin"], {"muscle": 0.45, "chest": 0.9, "breadth": 0.82, "limb": 0.82, "deltoid": 0.72,
                                                        "leg": 0.85, "neck": 0.82}).build()
    head_origin = V(*L["head"][0])
    size = (L["head"][1][2] - L["head"][0][2]) * FACE_SHARE * 24.0 / 21.9
    u = size / 24.0
    locks_bones = anatomy.rigid("head")
    chains = sorted(bone[:-3] for bone in L if bone.startswith("hair_") and bone.endswith("_01"))
    if chains:
        joints = {chain: [L["%s_%s" % (chain, joint)][0] for joint in ("01", "02", "end")] for chain in chains}
        locks_bones = hair.on_chains(joints, "head", head_origin + V(*SKULL_CENTRE) * u, V(*SKULL_RADII) * u, HAIR_REACH)
    locks = curls(S, mats["hair"], locks_bones, u, spec["seed"])
    face = Over([anatomy.Head(S, mats["skin"], size, look={"jaw": 0.9}).build(), eyes(S, mats, u)])
    figure.attach_head(Placed(Over([face, locks]), head_origin, np.eye(3)))
    for side in ("l", "r"):
        figure.limbs["hand_" + side] = hand(S, L, H, side, mats, figure)
    body = figure.body()
    dressed = clothes(S, L, dims, mats, body, figure.limbs)
    mantled = Over([dressed, mantle(S, L, dims, mats, dressed, figure.limbs)])
    worn = Over([mantled, chain_loops(S, L, dims, mats, mantled), held_chain(S, L, dims, mats)]
                + [witchfire(S, L, H, side, mats) for side in ("l", "r")])
    skirt = coat_skirt(S, L, dims)
    sheets = list(skirt) + list(cloak(S, L, dims, dressed))
    return worn, {"body": body, "sheets": sheets}


# ---------------------------------------------------------------------------------------------- head and hands
def eyes(S, mats, u):
    """His violet eyes, set in the head's sockets (the head's frame, a 24 cm template's centimetres scaled by u)."""
    parts = []
    for sign, side in ((1.0, "l"), (-1.0, "r")):
        c, r = V(9.0, sign * 3.0, 9.9) * u, V(0.6, 1.2, 0.62) * u
        parts.append(tree.leaf(S, "eye_" + side, lambda P, c=c, r=r: sdf.ellipsoid(P, c, r), Box(c - 2 * u, c + 2 * u), mats["eyes"],
                               anatomy.rigid("head"), protect=1.0))
    return Union(parts)


def curl_lock(S, name, root, flow, normal, length, radius, turn, material, bones, steps=8):
    """One curl from root: along flow, lifting a little off the scalp along normal, its last half turning through turn
    (radians) away from the head, so its tip flicks out. Thick to a blunt, rounded tip: curls, not spikes."""
    d, n = unit(flow), unit(normal)
    points = [V(*root)]
    for k in range(steps):
        t = (k + 0.5) / steps
        phi = turn * max(0.0, t - 0.4) / 0.6
        lift = 0.15 * (1.0 - t)
        points.append(points[-1] + unit(d * np.cos(phi) + n * (np.sin(phi) + lift)) * length / steps)
    radii = [radius * r for r in (0.85, 1.0, 1.0, 0.95, 0.88, 0.8, 0.7, 0.6, 0.5)][:len(points)]
    return tree.leaf(S, name, lambda P, pts=points, rs=radii: sdf.tube(P, pts, rs), Box.around(points, radius * 1.2), material, bones)


def curls(S, material, bones, u, seed):
    """His dark curly hair: a tousled mop of locks over the skull, standing up a little at the crown and falling over
    the brow; and curls over the ears and round the back toward the jaw and nape, their blunt tips flicking out. The cap
    stays with the head; the locks sway on the hair's chains where his kit hangs them."""
    mop = hair.messy(S, material, bones, SKULL_CENTRE, SKULL_RADII, (15.5, 4.5), seed=seed, unit_scale=u, count=40,
                     length=(6.5, 9.5), radius=(1.9, 2.7), wind=(0.0, -0.2, 0.0), fringe=0.45, volume=0.36, cap_bones=anatomy.rigid("head"))
    rng = np.random.default_rng(seed + 7)
    c, r = V(*SKULL_CENTRE) * u, V(*SKULL_RADII) * u
    crown = c + V(-0.25 * r[0], 0, 0.95 * r[2])
    hooks = []
    for i in range(12):
        # Round the sides and back, from over one ear to over the other.
        azimuth = np.radians(60.0 + 240.0 * (i + rng.uniform(0.1, 0.9)) / 12.0)
        elevation = rng.uniform(-0.1, 0.45)
        out = V(np.cos(azimuth) * np.cos(elevation), np.sin(azimuth) * np.cos(elevation), np.sin(elevation))
        root = c + out * (r + 0.5 * u)
        normal = unit(out / r)
        away = root - crown
        away = away - normal * (away @ normal)
        flow = unit(unit(away) + V(0, 0, -0.9))
        hooks.append(curl_lock(S, "curl_%02d" % i, root, flow, normal, rng.uniform(7.0, 9.5) * u, rng.uniform(1.9, 2.4) * u, rng.uniform(1.0, 1.6),
                               material, bones))
    # Over each ear, falling to the jaw: the ears are hidden, as the art hides them.
    for side, sign in (("l", 1.0), ("r", -1.0)):
        for k, (degrees, rise) in enumerate(((75.0, 0.3), (100.0, 0.2))):
            a = np.radians(degrees) * sign
            out = V(np.cos(a) * np.cos(rise), np.sin(a) * np.cos(rise), np.sin(rise))
            root = c + out * (r + 0.5 * u)
            hooks.append(curl_lock(S, "ear_lock_%s_%d" % (side, k), root, unit(V(0.15, sign * 0.25, -1.0)), unit(out / r), rng.uniform(9.5, 11.0) * u,
                                   rng.uniform(2.1, 2.5) * u, rng.uniform(0.8, 1.2), material, bones))
    return Union([mop] + hooks, k=0.7 * u)


def hand(S, L, H, side, mats, figure):
    """A hand at rest beside his thigh, back out: the right closed on the chain it holds, the left open and loose, the
    witchfire playing round it; a dark fingerless glove over each."""
    w0, w1 = V(*L["hand_" + side][0]), V(*L["hand_" + side][1])
    sign = 1.0 if side == "l" else -1.0
    x = unit(w1 - w0)
    out = V(0, sign, 0)
    z = unit(out - x * (out @ x))
    y = np.cross(z, x)
    if side == "r":
        curl = {"index": (70, 90, 50), "middle": (75, 95, 50), "ring": (78, 95, 50), "little": (80, 95, 45)}
        thumb = (60, 30, 30, 25)
    else:
        curl = {"index": (20, 30, 20), "middle": (25, 35, 22), "ring": (28, 38, 24), "little": (32, 40, 25)}
        thumb = (35, 15, 20, 15)
    bare = anatomy.Hand(S, mats["skin"], side, H * HAND_SHARE, "hand_" + side, curl=curl, thumb=thumb)
    built = bare.build()
    glove_region = Zone(lambda P, k=bare.u: P[:, 0] - 11.2 * k, Box((-5, -10, -6), (11.2 * bare.u, 10, 6)))
    glove = Shell(S, "glove_" + side, built, 0.08, 0.32, glove_region, mats["glove"], hem=0.12, reach=0.5)
    placed = Placed(Over([built, glove]), w0, np.stack([x, y, z], axis=1))
    figure.parts.append(placed)
    return placed


# ---------------------------------------------------------------------------------------------- clothes
def clothes(S, L, dims, mats, body, limbs):
    """His clothes in layers, each over those beneath and keeping to its limbs: dark trousers bunched over heavy boots
    with buckled straps; the black coat over his torso, high at the throat, its left sleeve to below the elbow and its
    right cut away at the shoulder; buckled straps crossed over his chest and round his bare right arm; two belts; dark
    wraps on both forearms."""
    H = dims["height"]
    pz = L["pelvis"][0][2]
    cz = L["spine_03"][1][2]
    torso = cz - pz
    shoulder = abs(L["upperarm_l"][0][1])
    knee = L["calf_l"][0][2]
    # Trousers: from inside the boots to under the belts, loose and bunched over the boot tops.
    boot_top = H * 0.25
    legs = both(band_z(H * 0.05, pz + H * 0.05), garments.keep_to(limbs, ["leg_l", "leg_r", "torso"]))
    trousers = Shell(S, "trousers", body, 0.5, 0.9, legs, mats["trousers"], hem=0.3,
                     displace=bunch((0, 0, boot_top - H * 0.01), (0, 0, knee + H * 0.04), 3.0, H * 0.008, seed=111), reach=H * 0.01)
    boot_mats = {"boot": mats["boot"], "sole": mats["sole"]}
    boots = [garments.boot(S, "boot_" + side, L, side, boot_top, boot_mats, None, shaft=1.12, toe=1.05) for side in ("l", "r")]
    lower = Over([body, trousers] + boots)
    # Two buckled straps round each boot's shaft.
    straps = []
    for side in ("l", "r"):
        for i, share in enumerate((0.45, 0.78)):
            z = H * 0.03 + (boot_top - H * 0.03) * share
            region = both(band_z(z - H * 0.007, z + H * 0.007), garments.keep_to(limbs, ["leg_" + side]))
            straps.append(Shell(S, "boot_strap_%s_%d" % (side, i), lower, 0.0, 0.7, region, mats["leather"], hem=0.2, reach=0.3))
    lower = Over([lower] + straps)
    # The coat: black over the torso from below the belts to the throat, its left sleeve to below the elbow, its right
    # only a cap over the shoulder, so the arm shows bare beneath the mantle.
    s_l, e_l, w_l = V(*L["upperarm_l"][0]), V(*L["upperarm_l"][1]), V(*L["lowerarm_l"][1])
    s_r, e_r = V(*L["upperarm_r"][0]), V(*L["upperarm_r"][1])
    left_sleeve = both(garments.keep_to(limbs, ["upperarm_l", "forearm_l"]), half(e_l + (w_l - e_l) * 0.22, unit(w_l - e_l), Box.around([s_l, w_l], 18.0)))
    right_cap = both(garments.keep_to(limbs, ["upperarm_r"]), half(s_r + (e_r - s_r) * 0.28, unit(e_r - s_r), Box.around([s_r, e_r], 16.0)))
    coat_region = either(both(band_z(pz - H * 0.03, cz + H * 0.012), garments.keep_to(limbs, ["torso"])), left_sleeve, right_cap)
    coat = Shell(S, "coat", lower, 0.6, 1.0, coat_region, mats["coat"], hem=0.4, displace=folds((0, 0, 1), 9, 0.5, seed=113), reach=1.0)
    # The high collar: standing round the neck, open at the throat, flaring as it rises.
    collar = Zone(lambda P: np.maximum(np.maximum(cz - 2.0 - P[:, 2], P[:, 2] - (cz + H * 0.06)), np.cos(np.arctan2(P[:, 1], P[:, 0] - 1.0)) - 0.6),
                  Box((-16, -16, cz - 4), (16, 16, cz + 14)))
    ring = tree.leaf(S, "collar_core", lambda P: sdf.round_cone(P, V(0.5, 0, cz - 3.0), V(-1.0, 0, cz + H * 0.06), H * 0.046, H * 0.06),
                     Box((-18, -18, cz - 6), (18, 18, cz + 16)), mats["coat"], None)
    inner = tree.leaf(S, "collar_inner", lambda P: sdf.round_cone(P, V(0.5, 0, cz - 4.0), V(-1.0, 0, cz + H * 0.065), H * 0.04, H * 0.054),
                      Box((-18, -18, cz - 6), (18, 18, cz + 16)), mats["coat"], None)
    collar_band = tree.Intersect(tree.Subtract(ring, inner), collar, 0.3)
    dressed = Over([lower, coat, collar_band])
    # Straps: two crossed over his chest from each shoulder to the other hip, two round his bare right arm.
    belts = []
    for name, top, low in (("strap_l", V(0, shoulder - 3.0, cz - 1.0), V(0, -H * 0.075, pz + torso * 0.12)),
                           ("strap_r", V(0, -shoulder + 3.0, cz - 1.0), V(0, H * 0.075, pz + torso * 0.12))):
        across = unit(np.cross(low - top, V(1, 0, 0)))
        band = both(garments.band_plane((top + low) / 2, across, H * 0.024, Box((-30, -40, pz), (30, 40, cz + 4))), garments.keep_to(limbs, ["torso"]))
        belts.append(Shell(S, name, dressed, 0.2, 0.7, band, mats["leather"], hem=0.2, reach=0.5))
    for i, share in enumerate((0.56, 0.8)):
        point = s_r + (e_r - s_r) * share
        band = both(garments.band_plane(point, unit(e_r - s_r), H * 0.02, Box.around([s_r, e_r], 12.0)), garments.keep_to(limbs, ["upperarm_r"]))
        belts.append(Shell(S, "arm_strap_%d" % i, dressed, 0.2, 0.8, band, mats["leather"], hem=0.2, reach=0.5))
    # Two wide belts: one at the waist, one slung lower toward his right hip.
    for name, centre, drop, width in (("belt_upper", pz + torso * 0.2, 0.0, H * 0.03), ("belt_lower", pz + torso * 0.08, -0.12, H * 0.022)):
        band = both(garments.tilted_band(V(0, 0, centre), unit(V(0, drop, 1)), width, Box((-40, -40, centre - 12), (40, 40, centre + 12))),
                    garments.keep_to(limbs, ["torso"]))
        belts.append(Shell(S, name, dressed, 0.2, 0.9, band, mats["leather"], hem=0.2))
    # Wraps: dark cloth bound round both forearms from the wrist toward the elbow.
    wraps = []
    for side in ("l", "r"):
        e, w = V(*L["lowerarm_" + side][0]), V(*L["lowerarm_" + side][1])
        region = both(around([w, w + (e - w) * 0.62], [H * 0.035, H * 0.04]), garments.keep_to(limbs, ["forearm_" + side]))
        wraps.append(Shell(S, "wrap_" + side, dressed, 0.2, 0.7, region, mats["wrap"], hem=0.3,
                           displace=bunch(w, w + (e - w) * 0.62, 4.0, 0.5, seed=117 if side == "l" else 118), reach=0.6))
    strapped = Over([dressed] + belts + wraps)
    # Buckles: on the belt and on his arm's straps.
    buckles = []
    belt_z = pz + torso * 0.2
    for name, c, out, half_size in (("buckle_belt", V(0, 0, belt_z), V(1, 0, 0), (H * 0.008, H * 0.016, H * 0.013)),
                                    ("buckle_arm", s_r + (e_r - s_r) * 0.56, V(0, -1, 0), (H * 0.009, H * 0.007, H * 0.009))):
        hit, _normal = garments.surface_point(strapped, c + out * 40.0, -out, reach=45.0)
        c = hit + out * H * 0.004
        buckles.append(tree.leaf(S, name, lambda P, c=c, hs=half_size: sdf.box(P, c, hs, None, H * 0.003), Box(c - H * 0.03, c + H * 0.03), mats["silver"], None,
                                 protect=0.6))
    # The pendant at his breast: a silver sigil, a ring round a four-pointed star.
    hit, _normal = garments.surface_point(strapped, V(40.0, 0, cz - torso * 0.24), V(-1, 0, 0), reach=45.0)
    c = hit + V(H * 0.004, 0, 0)
    axes = np.stack([V(0, 1, 0), V(0, 0, 1), V(1, 0, 0)], axis=1)

    def sigil(P, c=c):
        ring = sdf.torus(P, c, H * 0.011, H * 0.003, axes)
        Q = sdf.local(P, c, axes)
        star = np.minimum(sdf.box(Q, (0, 0, 0), (H * 0.004, H * 0.016, H * 0.004)), sdf.box(Q, (0, 0, 0), (H * 0.013, H * 0.004, H * 0.004)))
        return np.minimum(ring, star)
    buckles.append(tree.leaf(S, "pendant", sigil, Box(c - H * 0.03, c + H * 0.03), mats["silver"], None, protect=0.6))
    return Over([strapped, Union(buckles)])


def mantle(S, L, dims, mats, under, limbs):
    """His mantle: black cloth over his shoulders, open at the throat under the collar, its hem torn into tongues that
    fall longest at his back; his hood lying in heavy folds round the back of his neck."""
    H = dims["height"]
    cz = L["spine_03"][1][2]
    shoulder = abs(L["upperarm_l"][0][1])
    tongues = 15

    def mantle_region(P):
        angle = np.arctan2(P[:, 1], P[:, 0])
        strip = (angle + np.pi) / (2.0 * np.pi) * tongues
        tip = 1.0 - np.abs(2.0 * (strip % 1.0) - 1.0)
        hem = (cz - H * 0.05 - H * 0.03 * np.abs(np.sin(angle)) - H * 0.05 * np.clip(-np.cos(angle), 0, 1)
               - (H * 0.012 + H * 0.035 * paint.hashed(np.floor(strip), 13.0)) * tip ** 0.7)
        return np.max(np.stack([hem - P[:, 2], P[:, 2] - (cz + H * 0.05), (np.cos(angle) - 0.8) * 12.0]), axis=0)
    region = both(Zone(mantle_region, Box((-40, -shoulder - 16, cz - H * 0.2), (40, shoulder + 16, cz + H * 0.06))),
                  garments.keep_to(limbs, ["torso", "upperarm_l", "upperarm_r"]))
    cape = Shell(S, "mantle", under, 0.4, 1.1, region, mats["coat_dark"], hem=0.5, displace=folds((0, 0, 1), 10, 1.3, seed=119), reach=1.5)
    draped = Over([under, cape])

    def at(degrees, radius, rise):
        a = np.radians(degrees)
        return V(radius * np.cos(a), radius * np.sin(a), cz + rise)
    # The hood, lowered: heavy folds round the back of his neck from shoulder to shoulder.
    lines = [("hood_back", [at(-70, 10.0, 2.5), at(-125, 10.5, 4.0), at(180, 10.5, 5.0), at(125, 10.5, 4.0), at(70, 10.0, 2.5)], 5.0, 3.2),
             ("hood_fold", [at(-110, 11.0, -1.0), at(-150, 12.0, -1.5), at(150, 12.0, -1.5), at(110, 11.0, -1.0)], 4.0, 2.4)]
    folds_ = [garments.fold(S, name, draped, garments.laid_on(draped, points), width, height, mats["coat_dark"], seed=120 + i)
              for i, (name, points, width, height) in enumerate(lines)]
    return Over([cape] + folds_)


# ---------------------------------------------------------------------------------------------- chain and witchfire
def chain(S, name, points, size, mats, bones, fire=True, protect=0.5, samples=80):
    """Heavy iron chain along a curve through points: oval links size long outside, each turned a quarter about the
    chain from the last, so a link's eye shows on every other; witchfire burning along its core, showing through the
    links' eyes, where fire."""
    line = garments.curve(points, samples)
    steps = np.linalg.norm(np.diff(line, axis=0), axis=1)
    run = np.concatenate([[0.0], np.cumsum(steps)])
    wire = size * 0.12
    width = size * 0.7
    bend = width * 0.5 - wire
    straight = size * 0.5 - bend - wire
    pitch = size - 4.0 * wire
    count = max(1, int(round(run[-1] / pitch)))
    at = (np.arange(count) + 0.5) * run[-1] / count
    centres = np.stack([np.interp(at, run, line[:, k]) for k in range(3)], axis=1)
    ahead = np.stack([np.interp(np.minimum(at + 0.5, run[-1]), run, line[:, k]) - np.interp(np.maximum(at - 0.5, 0.0), run, line[:, k])
                      for k in range(3)], axis=1)
    frames = []
    for i in range(count):
        a = unit(ahead[i])
        n = unit(np.cross(a, V(0, 0, 1))) if abs(a[2]) < 0.95 else unit(np.cross(a, V(1, 0, 0)))
        if i % 2:
            n = unit(np.cross(a, n))
        frames.append((centres[i], a, unit(np.cross(n, a)), n))

    def links(P):
        d = np.full(len(P), 1.0e4, dtype=np.float64)
        for c, a, b, n in frames:
            Q = P - c
            x, y, z = np.abs(Q @ a) - straight, Q @ b, Q @ n
            ring = np.sqrt(np.maximum(x, 0.0) ** 2 + y * y)
            d = np.minimum(d, np.sqrt((ring - bend) ** 2 + z * z) - wire)
        return d
    parts = [tree.leaf(S, name, links, Box.around(centres, size), mats["iron"], bones, protect=protect)]
    if fire:
        core = line[::max(1, samples // 16)]
        # Thicker than a link's iron, so it shows past every link seen edge-on, as well as through the eyes.
        parts.append(tree.leaf(S, name + "_fire", lambda P, pts=core: sdf.tube(P, pts, [wire * 1.5] * len(pts)), Box.around(core, size),
                               mats["witchfire"], bones, protect=protect))
    return Union(parts)


def held_chain(S, L, dims, mats):
    """The chain in his right hand: wound twice round his wrist, then out of his fist and hanging in a loose curve to
    his shin, swinging a little forward and out, witchfire running its length. It hangs from the hand's prop bone, so
    his clips carry it."""
    H = dims["height"]
    k = H / 171.0
    w0, w1 = V(*L["hand_r"][0]), V(*L["hand_r"][1])
    e = V(*L["lowerarm_r"][0])
    axis = unit(w1 - w0)
    fist = w0 + axis * H * 0.05
    hang = [fist, fist + V(0.5, -1.0, -8.0) * k, fist + V(2.5, -3.5, -19.0) * k, fist + V(6.0, -5.0, -30.0) * k, fist + V(11.0, -4.5, -39.0) * k,
            fist + V(16.0, -2.0, -43.0) * k]
    parts = [chain(S, "chain_held", hang, H * CHAIN_LINK, mats, anatomy.rigid("prop_r"), protect=0.7)]
    # Wound round the wrist and up the forearm.
    side = unit(np.cross(unit(w0 - e), V(1, 0, 0)))
    up = np.cross(unit(w0 - e), side)
    start = w0 + unit(e - w0) * H * 0.01
    wound = [start + unit(e - w0) * H * 0.07 * t + (side * np.cos(t * 4.0 * np.pi) + up * np.sin(t * 4.0 * np.pi)) * H * 0.032 for t in np.linspace(0.0, 1.0, 13)]
    parts.append(chain(S, "chain_wound", wound, H * CHAIN_BODY_LINK, mats, anatomy.rigid("lowerarm_r"), protect=0.5))
    return Union(parts)


def chain_loops(S, L, dims, mats, under):
    """Loops of heavy chain over his coat: across his chest from his left shoulder to his right hip, witchfire running
    along it; and slung round his hips over the belts, sagging at the front."""
    H = dims["height"]
    pz = L["pelvis"][0][2]
    cz = L["spine_03"][1][2]
    torso = cz - pz
    shoulder = abs(L["upperarm_l"][0][1])
    size = H * CHAIN_BODY_LINK
    lift = size * 0.4
    # Across the chest: points on the coat's surface seen from the front, stood off it by half a link.
    across = [(shoulder * 0.7, cz + H * 0.005), (shoulder * 0.35, cz - torso * 0.18), (0.0, cz - torso * 0.38), (-shoulder * 0.4, cz - torso * 0.62),
              (-shoulder * 0.62, pz + torso * 0.24)]
    starts = np.array([[40.0, y, z] for y, z in across])
    hits, normals = garments.surface_points(under, starts, np.tile([-1.0, 0.0, 0.0], (len(starts), 1)), reach=45.0)
    sash = [hits[i] + normals[i] * lift for i in range(len(hits))]
    parts = [chain(S, "chain_chest", sash, size, mats, anatomy.along("spine_03", "pelvis", sash[0], sash[-1], 0.35, 0.8), protect=0.4)]
    # Round the hips: from his left side across the front, sagging below the belts, to his right side.
    belt_z = pz + torso * 0.14
    ring = []
    for degrees, drop in ((95.0, 0.0), (55.0, -0.03), (20.0, -0.05), (-15.0, -0.055), (-50.0, -0.035), (-90.0, -0.01), (-125.0, 0.0)):
        a = np.radians(degrees)
        ring.append((V(np.cos(a), np.sin(a), 0.0), belt_z + H * drop))
    starts = np.array([o * 40.0 + V(0, 0, z) for o, z in ring])
    hits, normals = garments.surface_points(under, starts, -np.array([o for o, _z in ring]), reach=45.0)
    slung = [hits[i] + normals[i] * lift for i in range(len(hits))]
    parts.append(chain(S, "chain_hips", slung, size, mats, anatomy.rigid("pelvis"), fire=False, protect=0.3))
    return Union(parts)


def witchfire(S, L, H, side, mats):
    """Violet-black witchfire gathered in one hand: tongues of flame rooted round the hand's outer side, fused at their
    roots, rising past the wrist and leaning out from the arm toward their tips."""
    w0, w1 = V(*L["hand_" + side][0]), V(*L["hand_" + side][1])
    axis = unit(w1 - w0)
    sign = 1.0 if side == "l" else -1.0
    out = unit(V(0, sign, 0) - axis * (V(0, sign, 0) @ axis))
    fwd = unit(V(1, 0, 0) - axis * axis[0])
    rng = np.random.default_rng(131 if side == "l" else 137)
    bones = anatomy.rigid("hand_" + side)
    centre = w0 + axis * H * 0.05
    parts = []
    count = 6
    for i in range(count):
        # Round the hand's front, outer side and back (none between the hand and the thigh).
        a = np.radians(-110.0 + 220.0 * (i + rng.uniform(0.2, 0.8)) / count)
        ring = fwd * np.sin(a) + out * np.cos(a)
        root = centre + ring * H * 0.026 + axis * H * rng.uniform(-0.005, 0.035)
        rise = H * rng.uniform(0.075, 0.12)
        lean = unit(ring * 0.6 + out * 0.5)
        mid = root + V(0, 0, rise * 0.5) + lean * H * 0.012
        tip = root + V(0, 0, rise) + lean * H * 0.03 + fwd * H * rng.uniform(-0.012, 0.012)
        points = [root, mid, tip]
        radii = [H * 0.013, H * 0.009, H * 0.0015]
        parts.append(tree.leaf(S, "witchfire_%s_%d" % (side, i), lambda P, pts=points, rs=radii: sdf.tube(P, pts, rs), Box.around(points, H * 0.016),
                               mats["witchfire"], bones, protect=0.5))
    return Union(parts, k=H * 0.012)


# ---------------------------------------------------------------------------------------------- cloth sheets
def skirt_shape(L, dims):
    """The coat skirt's sweep and hang: (t0, t1, belt height, hem heights round it, radii at the belt, its flare)."""
    H = dims["height"]
    pz = L["pelvis"][0][2]
    belt_z = pz + (L["spine_03"][1][2] - pz) * 0.1
    hip = abs(L["thigh_l"][0][1])
    return {"t0": np.radians(32.0), "t1": np.radians(328.0), "belt": belt_z, "rx": hip * 1.95, "ry": hip * 2.15, "flare": H * 0.1,
            "hem_u": [0.0, 0.25, 0.5, 0.75, 1.0], "hem_z": [H * 0.2, H * 0.12, H * 0.09, H * 0.12, H * 0.2]}


def skirt_radius(shape, theta, z):
    """How far out from his middle the coat skirt hangs at angle theta (radians) and height z."""
    rx, ry = shape["rx"], shape["ry"]
    top = rx * ry / np.sqrt((ry * np.cos(theta)) ** 2 + (rx * np.sin(theta)) ** 2)
    v = np.clip((shape["belt"] - z) / max(shape["belt"] - min(shape["hem_z"]), 1e-6), 0.0, 1.0)
    return top + shape["flare"] * v ** 1.3 + 1.6


def coat_skirt(S, L, dims):
    """The coat's long skirt: from the belt to his shins, open at the front, flaring as it falls, its hem torn into
    tongues: black outside over its violet lining. Each half hangs on its own chain (ADR-069 §7); toward the front it
    follows the thigh beside it."""
    H = dims["height"]
    shape = skirt_shape(L, dims)
    t0, t1, belt_z = shape["t0"], shape["t1"], shape["belt"]
    columns, rows, strips = 16, 5, 8
    limbs = [(L["thigh_" + s][0], L["thigh_" + s][1], H * 0.055) for s in ("l", "r")] + [(L["calf_" + s][0], L["calf_" + s][1], H * 0.05) for s in ("l", "r")]
    phase = np.random.default_rng(141).uniform(0, 2 * np.pi, 2)

    def placed(inset):
        def position(u, v):
            theta = t0 + (t1 - t0) * u
            radial = np.stack([np.cos(theta), np.sin(theta), np.zeros_like(theta)], axis=1)
            top = np.stack([shape["rx"] * np.cos(theta), shape["ry"] * np.sin(theta), np.full_like(theta, belt_z)], axis=1)
            hem = np.interp(u, shape["hem_u"], shape["hem_z"])
            pleat = (0.3 + H * 0.007 * v) * (0.6 * np.sin(theta * 7.0 + phase[0]) + 0.4 * np.sin(theta * 12.0 + phase[1]))
            p = top + radial * (shape["flare"] * v ** 1.3 + pleat - inset)[:, None]
            p[:, 2] = belt_z + (hem - belt_z) * v
            return sheet.clear_of(p, limbs, 1.5 - inset)
        return position
    outer = placed(0.0)

    def bones(P, u, v):
        # Weighted by where the outer face hangs, so the lining moves with it.
        theta = t0 + (t1 - t0) * u
        left = np.sin(theta) > 0
        hold = np.clip(1 - v / 0.12, 0, 1)
        front = np.clip(np.cos(theta) / 0.85, 0, 1) * 0.75
        lower = np.clip((v - 0.25) / 0.5, 0, 1) * (1 - hold)
        upper = (1 - hold) - lower
        w = {"pelvis": hold}
        for side, mask in (("l", left), ("r", ~left)):
            w["coat_%s_01" % side] = upper * mask * (1 - front)
            w["coat_%s_02" % side] = lower * mask * (1 - front)
            w["thigh_" + side] = (1 - hold) * front * mask
        total = sum(w.values())
        return {k: (np.asarray(x) / np.maximum(total, 1e-6)).astype(np.float32) for k, x in w.items()}
    reach = lambda u: sheet.torn(u, strips, 0.7, 0.14, 7.0)  # noqa: E731
    return (sheet.Sheet("coat_skirt", outer, S.material("coat_skirt", PALETTE["coat"]), bones, columns, rows, reach=reach),
            sheet.Sheet("coat_lining", placed(0.7), S.material("coat_lining", PALETTE["lining"]), bones, columns, rows, reach=reach))


def cloak(S, L, dims, worn):
    """His long cloak: lying on his shoulders under the mantle, falling behind his arms and outside his coat's skirt
    nearly to the ground, flaring wide at both sides, its hem torn into long ragged tongues: black outside over its
    violet lining."""
    H = dims["height"]
    cz = L["spine_03"][1][2]
    columns, rows, strips = 20, 8, 10
    t0, t1 = np.radians(100.0), np.radians(260.0)
    limbs = [(L["upperarm_" + s][0], L["upperarm_" + s][1], H * 0.045) for s in ("l", "r")] + [(L["lowerarm_" + s][0], L["lowerarm_" + s][1], H * 0.04)
                                                                                               for s in ("l", "r")]
    samples = np.linspace(0.0, 1.0, 13)
    theta_s = t0 + (t1 - t0) * samples
    inward = -np.stack([np.cos(theta_s), np.sin(theta_s), np.zeros_like(theta_s)], axis=1)
    z = cz - H * 0.015 - H * 0.01 * np.abs(np.sin(theta_s))
    centres = np.stack([np.zeros_like(z), np.zeros_like(z), z], axis=1)
    hits, _normals = garments.surface_points(worn, centres - inward * 45.0, inward, reach=45.0)
    tops = hits - inward * 1.2
    phase = np.random.default_rng(151).uniform(0, 2 * np.pi, 3)
    hem_u, hem_z = [0.0, 0.25, 0.5, 0.75, 1.0], [H * 0.16, H * 0.07, H * 0.04, H * 0.07, H * 0.16]
    skirt = skirt_shape(L, dims)

    def placed(inset):
        def position(u, v):
            theta = t0 + (t1 - t0) * u
            side = np.abs(np.sin(theta))
            top = np.stack([np.interp(u, samples, tops[:, k]) for k in range(3)], axis=1)
            radial = np.stack([np.cos(theta), np.sin(theta), np.zeros_like(theta)], axis=1)
            flare = (H * 0.06 + H * 0.2 * side) * v ** 1.3
            pleat = (0.5 + H * 0.022 * v) * (0.55 * np.sin(theta * 10.0 + phase[0]) + 0.3 * np.sin(theta * 15.0 + phase[1]) + 0.25 * np.sin(theta * 6.0 + phase[2]))
            p = top + radial * (flare + pleat - inset)[:, None]
            p[:, 2] = top[:, 2] + (np.interp(u, hem_u, hem_z) - top[:, 2]) * v
            p = sheet.clear_of(p, limbs, 1.5 - inset)
            # Outside the coat's skirt wherever it hangs beside it.
            out = np.linalg.norm(p[:, :2], axis=1)
            need = skirt_radius(skirt, np.arctan2(p[:, 1], p[:, 0]), p[:, 2]) + 2.0 - inset
            below = p[:, 2] < skirt["belt"] + H * 0.02
            grow = np.where(below & (out < need), need / np.maximum(out, 1e-6), 1.0)
            p[:, :2] *= grow[:, None]
            return p
        return position
    outer = placed(0.0)
    chains = {prefix: [V(*L["%s_%02d" % (prefix, i)][0]) for i in (1, 2, 3)] + [V(*L[prefix + "_end"][0])] for prefix in ("cape_l", "cape", "cape_r")}
    across = sheet.sweep_shares(chains, t0, t1)

    def bones(P, u, v):
        # Weighted by where the outer face hangs, so the lining moves with it.
        hold = np.clip(1 - v / 0.1, 0, 1)
        return sheet.down_chains(chains, across, outer(u, v), u, hold, "spine_03")
    reach = lambda u: sheet.torn(u, strips, 0.55, 0.16, 4.0)  # noqa: E731
    return (sheet.Sheet("cloak", outer, S.material("cloak_sheet", PALETTE["coat"]), bones, columns, rows, reach=reach),
            sheet.Sheet("cloak_lining", placed(0.8), S.material("cloak_lining", PALETTE["lining"]), bones, columns, rows, reach=reach))
