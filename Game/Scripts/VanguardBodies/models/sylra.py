"""Sylra, The Mistwarden (ADR-069): a hooded harbour pilot, read by her silhouette from the game camera. Character Bible
§16 and the author's reference (2026-10-07): a deep hood holding her face in shadow, long dark hair spilling from it;
layered grey-blue storm cloth cut into long ribbons at every hem, so her edges move with the air (a long cloak behind,
an open skirt to her ankles, drapes from both arms); a dark leather bodice and belt hung with working gear (the
ship's-wheel charm, tuned bells); fingerless gloves and bracers; thigh-high pointed boots. She carries a large lantern
out on its chain, burning cold blue-white: the brightest thing about her, and the point allies steer by.

Low poly and flat-coloured (author 2026-10-07): the big forms that make her outline, each a flat colour the toon
material shades. She rests in the A pose, the lantern hanging from her left hand; her clips hold it out. Every loose
part hangs on spring chains (ADR-069 §7): the cloak, the skirt, the arm drapes and the lantern itself.

Proportions are the kit's (measured from the reference against her height, the hood fitted to a head larger than the
reference's for a high camera); colours are sampled from it (sRGB) and pushed toward the canon's grey-blue."""
import numpy as np

from ..sculpt import anatomy, garments, hair, paint, sdf, sheet, tree
from ..sculpt.anatomy import V, unit
from ..sculpt.garments import around, band_z, both, either, folds, without
from ..sculpt.tree import Box, Over, Placed, Shell, Union, Zone

# The hand, wrist to fingertip, as a share of the height.
HAND_SHARE = 0.1

# Sampled from the reference (sRGB), the cloth lifted toward the canon's grey-blue so it reads as cloth, not black, under
# the toon light from the game camera.
PALETTE = {
    "skin": (0.8, 0.66, 0.6), "hair": (0.12, 0.11, 0.12), "storm": (0.33, 0.38, 0.46), "storm_mid": (0.42, 0.48, 0.57),
    "storm_light": (0.66, 0.7, 0.76), "leather": (0.24, 0.22, 0.24), "boot": (0.2, 0.19, 0.21), "glove": (0.15, 0.13, 0.13),
    "brass": (0.7, 0.55, 0.32), "iron": (0.2, 0.2, 0.22), "lantern": (0.85, 0.94, 1.0),
}


def materials(S):
    """Every material Sylra is coloured in, flat (the toon material shades it); the lantern's light glows."""
    return {name: S.material(name, colour, glow=name == "lantern") for name, colour in PALETTE.items()}


def build(S, L, dims, spec):
    """Sylra's sculpt on her layout: (the whole, {"body": the skin alone, under her clothes, "sheets": her cloth})."""
    mats = materials(S)
    H = dims["height"]
    figure = anatomy.Figure(S, L, dims, mats["skin"], {"muscle": 0.2, "chest": 0.85, "breadth": 0.78, "hips": 1.12, "limb": 0.78,
                                                        "deltoid": 0.7, "leg": 0.82, "neck": 0.78, "bust": 0.85}).build()
    head_origin = V(*L["head"][0])
    size = (L["head"][1][2] - L["head"][0][2]) * 24.0 / 21.9
    figure.attach_head(Placed(anatomy.Head(S, mats["skin"], size, look={"jaw": 0.86}).build(), head_origin, np.eye(3)))
    for side in ("l", "r"):
        figure.limbs["hand_" + side] = hand(S, L, H, side, mats, figure)
    body = figure.body()
    dressed = clothes(S, L, dims, mats, body, figure.limbs)
    hooded = Over([dressed, hood(S, L, dims, mats, dressed, figure.limbs, size)])
    worn = Over([hooded, gear(S, L, dims, mats), lantern(S, L, dims, mats, spec)])
    sheets = [cloak(S, L, dims, mats, hooded), skirt(S, L, dims, mats)] + [drape(S, L, dims, mats, side) for side in ("l", "r")]
    return worn, {"body": body, "sheets": sheets}


def hand(S, L, H, side, mats, figure):
    """A hand at rest, hanging beside her thigh, palm in: the left closed on the lantern's chain, the right loose; a
    fingerless glove over each."""
    w0, w1 = V(*L["hand_" + side][0]), V(*L["hand_" + side][1])
    sign = 1.0 if side == "l" else -1.0
    x = unit(w1 - w0)
    out = V(0, sign, 0)
    z = unit(out - x * (out @ x))
    y = np.cross(z, x)
    if side == "l":
        curl = {"index": (70, 90, 50), "middle": (75, 95, 50), "ring": (78, 95, 50), "little": (80, 95, 45)}
        thumb = (60, 30, 30, 25)
    else:
        curl = {"index": (15, 20, 10), "middle": (20, 25, 12), "ring": (22, 28, 14), "little": (25, 30, 15)}
        thumb = (35, 15, 20, 15)
    bare = anatomy.Hand(S, mats["skin"], side, H * HAND_SHARE, "hand_" + side, curl=curl, thumb=thumb)
    built = bare.build()
    glove_region = Zone(lambda P, k=bare.u: P[:, 0] - 11.2 * k, Box((-5, -10, -6), (11.2 * bare.u, 10, 6)))
    glove = Shell(S, "glove_" + side, built, 0.08, 0.32, glove_region, mats["glove"], hem=0.12, reach=0.5)
    placed = Placed(Over([built, glove]), w0, np.stack([x, y, z], axis=1))
    figure.parts.append(placed)
    return placed


def clothes(S, L, dims, mats, body, limbs):
    """Her clothes over her skin, each keeping to its limbs: thigh-high boots, a leather bodice over the torso, dark
    storm-cloth sleeves on the upper arms, leather bracers on the forearms, and a belt."""
    H = dims["height"]
    pz = L["pelvis"][0][2]
    cz = L["spine_03"][1][2]
    torso = cz - pz
    # Boots: pointed, to high on the thigh.
    boot_mats = {"boot": mats["boot"], "sole": mats["boot"]}
    boots = [garments.boot(S, "boot_" + side, L, side, H * 0.43, boot_mats, None, shaft=0.85, toe=0.95) for side in ("l", "r")]
    lower = Over([body] + boots)
    # The bodice: dark leather from the hips to over the bust, cut low between the breasts.
    neckline = Zone(lambda P: np.maximum(np.maximum(1.5 - P[:, 0], np.abs(P[:, 1]) - np.clip((P[:, 2] - (cz - torso * 0.42)) * 0.4, 0, 6.0)),
                                         (cz - torso * 0.42) - P[:, 2]), Box((0, -10, cz - torso * 0.5), (30, 10, cz + 10)))
    bodice_region = without(both(band_z(pz - H * 0.01, cz - torso * 0.12), garments.keep_to(limbs, ["torso"])), neckline)
    bodice = Shell(S, "bodice", lower, 0.3, 0.6, bodice_region, mats["leather"], hem=0.3, displace=folds((0, 0, 1), 7, 0.3, seed=161), reach=0.6)
    sleeves, bracers = [], []
    for side in ("l", "r"):
        sleeves.append(Shell(S, "sleeve_" + side, lower, 0.3, 0.8, garments.keep_to(limbs, ["upperarm_" + side]), mats["storm"], hem=0.4, reach=0.5))
        e, w = V(*L["lowerarm_" + side][0]), V(*L["lowerarm_" + side][1])
        region = both(around([w, w + (e - w) * 0.8], [5.5, 6.5]), garments.keep_to(limbs, ["forearm_" + side]))
        bracers.append(Shell(S, "bracer_" + side, lower, 0.3, 0.9, region, mats["leather"], hem=0.3, reach=0.5))
    dressed = Over([lower, bodice] + sleeves + bracers)
    belt_z = pz + torso * 0.12
    belt = Shell(S, "belt", dressed, 0.2, 0.9, both(band_z(belt_z - H * 0.012, belt_z + H * 0.012), garments.keep_to(limbs, ["torso"])),
                 mats["leather"], hem=0.2)
    return Over([dressed, belt])


def hood(S, L, dims, mats, under, limbs, size):
    """Her deep hood: a peaked cowl over her head, its opening holding her face in shadow, dark hair spilling from it
    over her shoulders to her breast; under it a mantle of the same storm cloth over her shoulders, its hem torn into
    tongues; and a light shawl wound round her neck over the mantle."""
    H = dims["height"]
    u = size / 24.0
    o = V(*L["head"][0])
    cz = L["spine_03"][1][2]
    shoulder = abs(L["upperarm_l"][0][1])
    head_bones = anatomy.rigid("head")
    # The cowl, in the head's frame: a dome over the skull, peaked behind, deep at the front.
    centre = o + V(0.0, 0, 13.0) * u
    radii = V(15.5, 13.5, 14.5) * u
    peak = o + V(-4.0, 0, 31.0) * u

    def cowl_distance(P):
        dome = sdf.ellipsoid(P, centre, radii)
        tip = sdf.round_cone(P, centre + V(-2.0, 0, 5.0) * u, peak, 10.5 * u, 1.0 * u)
        return sdf.smin(dome, tip, 4.0 * u)

    cowl = tree.leaf(S, "cowl", cowl_distance, Box(centre - radii - 2 * u, np.maximum(centre + radii, peak) + 2 * u), mats["storm"], head_bones)
    # Its opening: cut from the front, its brim standing out past the face so the face sits back in its shadow.
    mouth = o + V(19.0, 0, 10.5) * u
    opening = tree.leaf(S, "hood_opening", lambda P: sdf.ellipsoid(P, mouth, V(10.5, 6.2, 9.0) * u), Box(mouth - 12 * u, mouth + 12 * u),
                        mats["storm"], head_bones)
    cowl = tree.Subtract(cowl, opening, k=1.5 * u)
    # Long dark hair spilling from the opening over each shoulder to her breast, falling from the head to her chest.
    locks = []
    rng = np.random.default_rng(spec_seed(L))
    for i, (side, sign) in enumerate((("l", 1.0), ("r", -1.0)) * 5):
        root = o + V(7.0 + rng.uniform(-1.5, 1.5), sign * (6.0 + rng.uniform(0, 1.5)), 8.0 + rng.uniform(-3.0, 4.0)) * u
        direction = V(0.45 + rng.uniform(-0.1, 0.15), sign * (0.12 + rng.uniform(0, 0.15)), -1.0)
        length = H * rng.uniform(0.12, 0.2)
        weights = anatomy.along("head", "spine_03", root, root + unit(direction) * length, 0.15, 0.6)
        locks.append(hair.clump(S, "hair_%s_%d" % (side, i), root, direction, V(sign, 0.2, 0.0), length, rng.uniform(3.2, 4.2) * u,
                                rng.uniform(0.05, 0.15), rng.uniform(0.0, 0.25), mats["hair"], weights))
    # The mantle: storm cloth over the shoulders and upper arms, its hem torn into tongues.
    tongues = 16

    def mantle_region(P):
        angle = np.arctan2(P[:, 1], P[:, 0])
        strip = (angle + np.pi) / (2.0 * np.pi) * tongues
        tip = 1.0 - np.abs(2.0 * (strip % 1.0) - 1.0)
        hem = cz - H * 0.07 - H * 0.03 * np.abs(np.sin(angle)) - (H * 0.012 + H * 0.03 * paint.hashed(np.floor(strip), 5.0)) * tip ** 0.7
        return np.maximum(hem - P[:, 2], P[:, 2] - (cz + H * 0.04))

    region = both(Zone(mantle_region, Box((-40, -shoulder - 20, cz - H * 0.16), (40, shoulder + 20, cz + H * 0.05))),
                  garments.keep_to(limbs, ["torso", "upperarm_l", "upperarm_r"]))
    mantle = Shell(S, "mantle", under, 0.4, 1.2, region, mats["storm"], hem=0.6, displace=folds((0, 0, 1), 11, 1.2, seed=162), reach=1.4)
    # The shawl: light cloth wound round her neck over the mantle, a thick roll lying across the shoulders.
    draped = Over([under, mantle])

    def at(degrees, radius, rise):
        a = np.radians(degrees)
        return V(radius * np.cos(a), radius * np.sin(a), cz + rise)
    # Wound once round the back of her neck, then swagged from her left shoulder across her breast to her right side.
    lines = [("shawl_back", [at(-75, 8.5, 1.5), at(-130, 8.0, 2.0), at(180, 7.5, 2.2), at(130, 8.0, 2.0), at(75, 8.5, 1.5)], 3.8, 1.8),
             ("shawl_swag", [at(80, 9.0, 1.0), at(50, 10.5, -3.0), at(10, 12.0, -8.0), at(-35, 12.5, -12.0), at(-70, shoulder * 0.9, -14.0)], 4.4, 1.8),
             ("shawl_shoulder_l", [at(95, 8.5, 1.0), at(90, shoulder * 0.7, -1.0), at(88, shoulder + 1.0, -3.0)], 4.2, 1.6)]
    shawl = [garments.fold(S, name, draped, garments.laid_on(draped, points), width, height, mats["storm_light"], seed=170 + i)
             for i, (name, points, width, height) in enumerate(lines)]
    return Over([mantle] + shawl + [cowl, Union(locks, k=0.8 * u)])


def spec_seed(L):
    """A seed for her hair's draw that depends only on her layout (the same in every worker)."""
    return int(abs(L["head"][1][2]) * 1000) % (2 ** 31)


def gear(S, L, dims, mats):
    """The working gear on her belt (never ornament): the ship's-wheel charm at her right hip and a pair of tuned
    bells at her left."""
    H = dims["height"]
    pz = L["pelvis"][0][2]
    belt_z = pz + (L["spine_03"][1][2] - pz) * 0.12
    hip = abs(L["thigh_l"][0][1])
    parts = []
    # The wheel: a brass rim and hub on spokes, hung below the belt on her right hip, facing out.
    angle = np.radians(-40.0)
    out = V(np.cos(angle), np.sin(angle), 0.0)
    c = V(0, 0, belt_z - H * 0.045) + out * (hip * 1.9)
    rim, tube = H * 0.026, H * 0.0045

    def wheel(P, c=c, n=out):
        Q = P - c
        along = Q @ n
        flat = Q - np.outer(along, n)
        ring = np.sqrt((np.linalg.norm(flat, axis=1) - rim) ** 2 + along ** 2) - tube
        hub = np.maximum(np.linalg.norm(flat, axis=1) - rim * 0.3, np.abs(along) - tube * 1.4)
        spokes = np.maximum(np.minimum(np.abs(flat[:, 2]), np.abs(flat @ unit(np.cross(n, V(0, 0, 1))))) - tube * 0.7,
                            np.maximum(np.abs(along) - tube, np.linalg.norm(flat, axis=1) - rim))
        return np.minimum(np.minimum(ring, hub), spokes)
    parts.append(tree.leaf(S, "wheel", wheel, Box(c - rim * 1.5, c + rim * 1.5), mats["brass"], anatomy.rigid("pelvis"), protect=0.6))
    # The bells: two brass cups hanging from the belt at her left hip.
    for i, degrees in enumerate((35.0, 55.0)):
        a = np.radians(degrees)
        top = V(np.cos(a), np.sin(a), 0.0) * (hip * 1.9) + V(0, 0, belt_z - H * 0.02 - i * H * 0.012)
        mouth = top - V(0, 0, H * 0.03)
        parts.append(tree.leaf(S, "bell_%d" % i, lambda P, a=top, b=mouth: sdf.round_cone(P, a, b, H * 0.006, H * 0.014),
                               Box.around([top, mouth], H * 0.02), mats["brass"], anatomy.rigid("pelvis"), protect=0.6))
    return Union(parts, k=0.2)


def lantern(S, L, dims, mats, spec):
    """Her lantern hung on its chain from her left hand's grip: a globe of cold blue-white light between an iron cap and
    base, as large as her head (a cage's bars, at this density, would read as a face). It swings on its own chain where
    her kit gives it one (ADR-069 §7)."""
    H = dims["height"]
    grip = V(*L["prop_l"][0])
    bones = anatomy.rigid("lantern_01" if "lantern" in spec.get("springs", {}) else "prop_l")
    down = V(0, 0, -1)
    centre = grip + down * H * 0.19
    iron, light = mats["iron"], mats["lantern"]
    cap_a, cap_b = centre + V(0, 0, H * 0.05), centre + V(0, 0, H * 0.07)
    parts = [tree.leaf(S, "lantern_chain", lambda P: sdf.capsule(P, grip, cap_b, H * 0.004), Box.around([grip, cap_b], H * 0.01), iron, bones, protect=1.0)]
    parts.append(tree.leaf(S, "lantern_cap", lambda P: sdf.round_cone(P, cap_a, cap_b, H * 0.032, H * 0.012), Box.around([cap_a, cap_b], H * 0.04),
                           iron, bones, protect=1.0))
    base_a, base_b = centre - V(0, 0, H * 0.05), centre - V(0, 0, H * 0.062)
    parts.append(tree.leaf(S, "lantern_base", lambda P: sdf.round_cone(P, base_a, base_b, H * 0.034, H * 0.034), Box.around([base_a, base_b], H * 0.04),
                           iron, bones, protect=1.0))
    parts.append(tree.leaf(S, "lantern_light", lambda P: sdf.ellipsoid(P, centre, V(1.0, 1.0, 1.2) * H * 0.045), Box(centre - H * 0.06, centre + H * 0.06),
                           light, bones, protect=1.0))
    return Union(parts, k=0.15)


# ---------------------------------------------------------------------------------------------- cloth sheets
def cloak(S, L, dims, mats, worn):
    """Her long storm cloak: lying on her shoulders under the mantle, falling behind her arms nearly to the ground,
    flaring wide at both sides as it falls, its hem torn into long ragged tongues."""
    H = dims["height"]
    cz = L["spine_03"][1][2]
    material = S.material("cloak_sheet", PALETTE["storm"])
    columns, rows, strips = 22, 9, 11
    # Behind her arms: from her shoulder blades round her back.
    t0, t1 = np.radians(105.0), np.radians(255.0)
    limbs = [(L["upperarm_" + s][0], L["upperarm_" + s][1], H * 0.045) for s in ("l", "r")] + [(L["lowerarm_" + s][0], L["lowerarm_" + s][1], H * 0.035)
                                                                                               for s in ("l", "r")]
    samples = np.linspace(0.0, 1.0, 13)
    theta_s = t0 + (t1 - t0) * samples
    inward = -np.stack([np.cos(theta_s), np.sin(theta_s), np.zeros_like(theta_s)], axis=1)
    z = cz - H * 0.02 - H * 0.01 * np.abs(np.sin(theta_s))
    centres = np.stack([np.zeros_like(z), np.zeros_like(z), z], axis=1)
    hits, _normals = garments.surface_points(worn, centres - inward * 45.0, inward, reach=45.0)
    tops = hits - inward * 1.0
    phase = np.random.default_rng(116).uniform(0, 2 * np.pi, 3)
    hem = H * 0.02

    def position(u, v):
        theta = t0 + (t1 - t0) * u
        side = np.abs(np.sin(theta))
        top = np.stack([np.interp(u, samples, tops[:, k]) for k in range(3)], axis=1)
        radial = np.stack([np.cos(theta), np.sin(theta), np.zeros_like(theta)], axis=1)
        flare = (H * 0.05 + H * 0.24 * side) * v ** 1.3
        pleat = (0.5 + H * 0.025 * v) * (0.55 * np.sin(theta * 10.0 + phase[0]) + 0.3 * np.sin(theta * 15.0 + phase[1]) + 0.25 * np.sin(theta * 6.0 + phase[2]))
        p = top + radial * (flare + pleat)[:, None]
        p[:, 2] = top[:, 2] + (hem - top[:, 2]) * v
        return sheet.clear_of(p, limbs, 1.5)
    chains = {prefix: [V(*L["%s_%02d" % (prefix, i)][0]) for i in (1, 2, 3)] + [V(*L[prefix + "_end"][0])] for prefix in ("cape_l", "cape", "cape_r")}
    across = sheet.sweep_shares(chains, t0, t1)

    def bones(P, u, v):
        hold = np.clip(1 - v / 0.1, 0, 1)
        return sheet.down_chains(chains, across, P, u, hold, "spine_03")
    return sheet.Sheet("cloak", position, material, bones, columns, rows, reach=lambda u: sheet.torn(u, strips, 0.5, 0.18, 4.0))


def skirt(S, L, dims, mats):
    """Her long skirt of storm cloth in layers: from the belt nearly to the ground, open at the front over her right
    leg's thigh straps, flaring as it falls, its hem torn into long tongues; each part follows the leg beside it."""
    H = dims["height"]
    pz = L["pelvis"][0][2]
    belt_z = pz + (L["spine_03"][1][2] - pz) * 0.12
    material = S.material("skirt_sheet", PALETTE["storm_mid"])
    columns, rows, strips = 24, 8, 12
    # Round her from just right of the front, its one slit over her right thigh.
    t0, t1 = np.radians(-8.0), np.radians(328.0)
    hip = abs(L["thigh_l"][0][1])
    rx, ry = hip * 1.6, hip * 1.95
    hem = H * 0.03
    limbs = [(L["thigh_" + s][0], L["thigh_" + s][1], H * 0.045) for s in ("l", "r")] + [(L["calf_" + s][0], L["calf_" + s][1], H * 0.035) for s in ("l", "r")]
    phase = np.random.default_rng(161).uniform(0, 2 * np.pi, 2)

    def position(u, v):
        theta = t0 + (t1 - t0) * u
        radial = np.stack([np.cos(theta), np.sin(theta), np.zeros_like(theta)], axis=1)
        top = np.stack([rx * np.cos(theta), ry * np.sin(theta), np.full_like(theta, belt_z)], axis=1)
        flare = H * 0.12 * v ** 1.2
        pleat = (0.3 + H * 0.015 * v) * (0.6 * np.sin(theta * 9.0 + phase[0]) + 0.4 * np.sin(theta * 14.0 + phase[1]))
        p = top + radial * (flare + pleat)[:, None]
        p[:, 2] = belt_z + (hem - belt_z) * v
        return sheet.clear_of(p, limbs, 1.5)
    chains = {name: [V(*L["%s_%02d" % (name, i)][0]) for i in (1, 2, 3)] + [V(*L[name + "_end"][0])] for name in ("skirt_fl", "skirt_bl", "skirt_br", "skirt_fr")}
    across = sheet.sweep_shares(chains, t0, t1)

    def bones(P, u, v):
        hold = np.clip(1 - v / 0.1, 0, 1)
        return sheet.down_chains(chains, across, P, u, hold, "pelvis")
    return sheet.Sheet("skirt", position, material, bones, columns, rows, reach=lambda u: sheet.torn(u, strips, 0.6, 0.15, 6.0))


def drape(S, L, dims, mats, side):
    """The light storm cloth hanging from one arm, from high on the upper arm to the wrist, falling outward past her
    hand to her knee, torn into long strips: what moves most about her as she walks. Seen from before or behind, it
    spreads from her arm like a wing (an arm hangs too near upright for cloth falling straight down to show)."""
    H = dims["height"]
    sign = 1.0 if side == "l" else -1.0
    material = S.material("drape_sheet_" + side, PALETTE["storm_light"])
    columns, rows, strips = 10, 6, 5
    s0, e = V(*L["upperarm_" + side][0]), V(*L["upperarm_" + side][1])
    w = V(*L["lowerarm_" + side][1])
    start = s0 + (e - s0) * 0.15 + V(-0.4, sign * 1.2, 0)
    out = V(0, sign, 0)
    fall = H * 0.3

    def edge(u):
        u = np.asarray(u)[:, None]
        first = start + (e - start) * np.clip(u * 2.0, 0, 1)
        second = e + (w - e) * np.clip(u * 2.0 - 1.0, 0, 1)
        return np.where(u < 0.5, first, second) + out * 1.6

    def position(u, v):
        top = edge(u)
        p = top + out * (H * 0.17 * v ** 0.9)[:, None] - V(1, 0, 0) * (2.0 * v)[:, None]
        p[:, 2] = top[:, 2] - fall * v * (0.8 + 0.2 * u)
        return p
    chain = [V(*L["sleeve_%s_01" % side][0]), V(*L["sleeve_%s_02" % side][0]), V(*L["sleeve_%s_end" % side][0])]

    def bones(P, u, v):
        hold = np.clip(1 - v / 0.15, 0, 1)
        # Its top along the upper arm, then the forearm, from the elbow (halfway along it).
        along = np.clip((u - 0.4) / 0.2, 0, 1)
        t = np.clip((chain[0][2] - P[:, 2]) / (chain[0][2] - chain[-1][2]), 0, 1)
        w = {"sleeve_%s_01" % side: (1 - hold) * (1 - t), "sleeve_%s_02" % side: (1 - hold) * t,
             "upperarm_" + side: hold * (1 - along), "lowerarm_" + side: hold * along}
        total = sum(w.values())
        return {k: (np.asarray(x) / np.maximum(total, 1e-6)).astype(np.float32) for k, x in w.items()}
    return sheet.Sheet("drape_" + side, position, material, bones, columns, rows, reach=lambda u: sheet.torn(u, strips, 0.45, 0.22, 9.0 + sign))
