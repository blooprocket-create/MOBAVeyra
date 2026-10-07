"""Kade, Dead Reckoning (ADR-069): a marksman, read by his silhouette from the game camera. Character Bible §2 and the
author's reference (2026-10-06): lean and long-legged; messy windswept brown hair; a dark leather coat with a high
collar, its sleeves to the elbow over a cream shirt's rolled cuffs; belts and a bandolier; bracers; heavy boots; a
crimson mantle bunched on his shoulders and a long crimson cloak torn into tongues. His rifle is long, dark steel and
brass, teal channels glowing its length and a large round optic with a teal lens: the only cool note.

Low poly and flat-coloured (author 2026-10-07): the big forms that make his outline, each a flat colour the toon
material shades; no detail the game camera cannot see. He rests empty-handed in the A pose with the rifle level in his
right hand; his clips carry it low and raise it to the sight only to fire (humanoid holds, ADR-069 §6). His hands and
the rifle are built where they are at the aim and moved to his rest wrists (dims["holds"]).

Proportions are the kit's (measured from the reference at 7.95 pixels a centimetre for his 171); colours are sampled
from the reference (sRGB)."""
import numpy as np

from ..sculpt import anatomy, garments, hair, paint, sdf, sheet, tree
from ..sculpt.anatomy import V, unit
from ..sculpt.garments import around, band_z, both, either, folds, bunch, half, without
from ..sculpt.tree import Box, Over, Placed, Shell, Union, Zone

# The skull's size (a 24 cm template's scale), so the reference's face fits: its chin at the head joint, its crown near
# 166; the hair rises above it to the top of the head bone.
SKULL = 20.4
# The skull his hair grows over, in the head's frame at a 24 cm template's scale: its centre and radii.
SKULL_CENTRE = (-1.2, 0.0, 14.4)
SKULL_RADII = (9.9, 7.5, 8.0)
# How far beyond the skull a lock has passed wholly to its hair chain (cm): the outer half of his locks sways.
HAIR_REACH = 5.0
# The hand, wrist to fingertip, as a share of the height.
HAND_SHARE = 0.108

# Sampled from the reference (sRGB).
PALETTE = {
    "skin": (0.78, 0.54, 0.45), "shirt": (0.86, 0.78, 0.75), "leather": (0.15, 0.12, 0.11), "belt": (0.24, 0.16, 0.13),
    "hair": (0.26, 0.18, 0.17), "teal": (0.1, 0.9, 0.85), "cloak": (0.62, 0.06, 0.1), "brass": (0.62, 0.47, 0.3),
    "steel": (0.2, 0.19, 0.2), "trousers": (0.19, 0.15, 0.14), "boot": (0.3, 0.19, 0.14), "glove": (0.2, 0.15, 0.13),
    "sole": (0.1, 0.08, 0.075),
}


def materials(S):
    """Every material Kade is coloured in, flat (the toon material shades it); teal glows."""
    return {name: S.material(name, colour, glow=name == "teal") for name, colour in PALETTE.items()}


def build(S, L, dims, spec):
    """Kade's sculpt on his layout: (the whole, {"body": the skin alone, under the garments, "sheets": his cloth})."""
    mats = materials(S)
    H = dims["height"]
    holds = dims["holds"]
    # Lean and long, as the reference stands: narrower through the chest and limbs than a heroic build.
    figure = anatomy.Figure(S, L, dims, mats["skin"], {"muscle": 0.5, "chest": 0.95, "breadth": 0.82, "limb": 0.85, "deltoid": 0.8, "leg": 0.88, "neck": 0.82}).build()
    head_origin = V(*L["head"][0])
    # His hair: messy and windswept in a few big locks, falling over the brow, swept to his right (the reference). Its
    # locks sway on the hair's chains where his kit gives them (ADR-069 §7); the cap over his scalp stays with his head.
    u = SKULL / 24.0
    locks_bones = anatomy.rigid("head")
    chains = sorted(bone[:-3] for bone in L if bone.startswith("hair_") and bone.endswith("_01"))
    if chains:
        joints = {chain: [L["%s_%s" % (chain, joint)][0] for joint in ("01", "02", "end")] for chain in chains}
        locks_bones = hair.on_chains(joints, "head", head_origin + V(*SKULL_CENTRE) * u, V(*SKULL_RADII) * u, HAIR_REACH)
    locks = hair.messy(S, mats["hair"], locks_bones, SKULL_CENTRE, SKULL_RADII, (16.0, 6.5), seed=spec["seed"], unit_scale=u, count=30,
                       length=(6.4, 10.2), radius=(1.5, 2.3), wind=(0.0, -0.3, 0.0), fringe=0.6, volume=0.36, cap_bones=anatomy.rigid("head"))
    figure.attach_head(Placed(Over([anatomy.Head(S, mats["skin"], SKULL, look={"jaw": 0.92}).build(), locks]), head_origin, np.eye(3)))
    for side in ("l", "r"):
        # Built where the hand holds the rifle at the aim, and moved, unturned, to its rest wrist.
        wrist = V(*holds["wrist"][side]) + V(*holds["offset"][side])
        if side == "r":
            # The grip hand: fingers wrapped around the grip under the bore, the index along the guard.
            hand = anatomy.Hand(S, mats["skin"], side, H * HAND_SHARE, "hand_r",
                                curl={"index": (25, 35, 15), "middle": (60, 95, 45), "ring": (65, 95, 45), "little": (70, 95, 40)},
                                thumb=(55, 25, 30, 20))
            x, z = unit(V(0.55, 0, -0.83)), unit(V(0, -1, 0))
        else:
            # The support hand: palm up under the fore-end, fingers curled around it.
            hand = anatomy.Hand(S, mats["skin"], side, H * HAND_SHARE, "hand_l",
                                curl={"index": (35, 60, 30), "middle": (40, 65, 30), "ring": (42, 65, 30), "little": (45, 65, 30)},
                                thumb=(35, 15, 20, 15))
            x, z = unit(V(1, -0.25, 0.1)), unit(V(0, 0.2, -1))
        y = np.cross(z, x)
        # A fingerless glove: dark leather over the palm and the fingers' first bones.
        bare = hand.build()
        glove_region = Zone(lambda P, k=hand.u: P[:, 0] - 11.2 * k, Box((-5, -10, -6), (11.2 * hand.u, 10, 6)))
        glove = Shell(S, "glove_" + side, bare, 0.08, 0.32, glove_region, mats["glove"], hem=0.12, reach=0.5)
        placed = Placed(Over([bare, glove]), wrist, np.stack([x, y, z], axis=1))
        figure.parts.append(placed)
        figure.limbs["hand_" + side] = placed
    body = figure.body()
    clothed = clothes(S, mats, L, dims, body, figure.limbs)
    worn = Over([clothed, scarf(S, L, mats, clothed, figure.limbs), rifle(S, mats, dims, spec)])
    return worn, {"body": body, "sheets": [cloak(S, L, dims, mats, clothed), coat_tails(S, L, dims, mats)]}


def scarf(S, L, mats, under, limbs):
    """The cloak's top wrapped as the reference shows it: a short mantle of crimson cloth over his shoulders, its hem
    torn into tongues; over it the cloth bunched round his neck in heavy folds, heaped on his left shoulder and swagged
    down across his left breast, lying over his right shoulder."""
    cz = L["spine_03"][1][2]
    shoulder = abs(L["upperarm_l"][0][1])
    tongues, open_at = 14, 0.3

    def mantle_region(P):
        angle = np.arctan2(P[:, 1], P[:, 0])
        strip = (angle + np.pi) / (2.0 * np.pi) * tongues
        tip = 1.0 - np.abs(2.0 * (strip % 1.0) - 1.0)
        hem = (cz - 2.0 - 4.5 * np.abs(np.sin(angle)) ** 1.2 - 4.0 * np.clip(np.sin(angle), 0, 1) - 6.0 * np.clip(-np.cos(angle), 0, 1)
               - (2.0 + 4.5 * paint.hashed(np.floor(strip), 9.0)) * tip ** 0.7)
        return np.max(np.stack([hem - P[:, 2], P[:, 2] - (cz + 8.0), (np.cos(angle - open_at) - 0.78) * 12.0]), axis=0)
    # Over the torso and down the upper arms, so it hangs over the shoulders rather than standing off them.
    region = both(Zone(mantle_region, Box((-35, -shoulder - 12, cz - 30), (35, shoulder + 12, cz + 9))),
                  garments.keep_to(limbs, ["torso", "upperarm_l", "upperarm_r"]))
    mantle = Shell(S, "mantle", under, 0.25, 1.0, region, mats["cloak"], hem=0.5, displace=folds((0, 0, 1), 9, 1.4, seed=96), reach=1.5)

    def at(degrees, radius, rise):
        a = np.radians(degrees)
        return V(radius * np.cos(a), radius * np.sin(a), cz + rise)
    # The folds, all lying on the mantle: (name, its line round him (degrees from the front toward his left, cm out from
    # his spine, cm above the neck's root), how wide either side of it and how high it stands).
    lines = [("scarf_back", [at(-80, 10.0, 2.0), at(-125, 9.5, 2.8), at(180, 9.0, 3.2), at(125, 9.5, 2.8), at(80, 10.0, 2.0)], 4.6, 2.4),
             ("scarf_shoulder_l", [at(100, 9.5, 2.0), at(92, 14.0, 0.5), at(88, shoulder + 1.0, -2.5)], 5.4, 2.2),
             ("scarf_swag", [at(80, 10.0, 2.5), at(55, 11.0, 0.0), at(42, 14.0, -5.0), at(45, 17.0, -10.0), at(55, 19.0, -14.0)], 5.0, 2.6),
             ("scarf_shoulder_r", [at(-55, 10.0, 1.5), at(-68, 13.5, 0.0), at(-75, 17.0, -2.5), at(-82, shoulder, -4.0)], 4.6, 2.0)]
    draped = Over([under, mantle])
    layers = [mantle] + [garments.fold(S, name, draped, garments.laid_on(draped, points), width, height, mats["cloak"], seed=90 + i)
                         for i, (name, points, width, height) in enumerate(lines)]
    return Over(layers)


def cloak(S, L, dims, mats, worn):
    """His long crimson cloak: its top lying on his shoulders under the mantle, falling behind his arms, its hem longer
    down his left side and back, flaring wide and pleated unevenly as it falls, torn into long tongues."""
    cz = L["spine_03"][1][2]
    material = S.material("cloak_sheet", PALETTE["cloak"])
    columns, rows, strips = 20, 8, 10
    t0, t1 = np.radians(55.0), np.radians(305.0)
    hem_u, hem_z = [0.0, 0.22, 0.5, 0.78, 1.0], [34.0, 10.0, 16.0, 22.0, 40.0]
    limbs = [(L["upperarm_" + s][0], L["upperarm_" + s][1], 7.0) for s in ("l", "r")] + [(L["lowerarm_" + s][0], L["lowerarm_" + s][1], 5.5) for s in ("l", "r")]
    # The top edge: where a ray in toward the body at the shoulders' height first meets the clothed body, a little out.
    samples = np.linspace(0.0, 1.0, 13)
    theta_s = t0 + (t1 - t0) * samples
    inward = -np.stack([np.cos(theta_s), np.sin(theta_s), np.zeros_like(theta_s)], axis=1)
    z = cz - 2.5 - 1.5 * np.abs(np.sin(theta_s))
    centres = np.stack([np.zeros_like(z), np.zeros_like(z), z], axis=1)
    hits, _normals = garments.surface_points(worn, centres - inward * 45.0, inward, reach=45.0)
    tops = hits - inward * 1.2
    phase = np.random.default_rng(67).uniform(0, 2 * np.pi, 3)

    def position(u, v):
        theta = t0 + (t1 - t0) * u
        side = np.abs(np.sin(theta))
        top = np.stack([np.interp(u, samples, tops[:, k]) for k in range(3)], axis=1)
        radial = np.stack([np.cos(theta), np.sin(theta), np.zeros_like(theta)], axis=1)
        flare = (8.0 + 24.0 * side) * v ** 1.4
        # Pleats: three waves of their own, so the folds fall unevenly.
        pleat = (0.5 + 3.6 * v) * (0.55 * np.sin(theta * 9.0 + phase[0]) + 0.3 * np.sin(theta * 14.0 + phase[1]) + 0.25 * np.sin(theta * 5.0 + phase[2]))
        p = top + radial * (flare + pleat)[:, None]
        p[:, 2] = top[:, 2] + (np.interp(u, hem_u, hem_z) - top[:, 2]) * v
        return sheet.clear_of(p, limbs, 1.5)
    # Its three chains (ADR-069): down each side behind the arm and down the middle, where each starts round his back.
    chains = {prefix: [V(*L["%s_%02d" % (prefix, i)][0]) for i in (1, 2, 3)] + [V(*L[prefix + "_end"][0])] for prefix in ("cape_l", "cape", "cape_r")}
    across = {prefix: (np.arctan2(points[0][1], points[0][0]) % (2 * np.pi) - t0) / (t1 - t0) for prefix, points in chains.items()}

    def bones(P, u, v):
        # Across the chains by where round his back it hangs; down each by height; the top held by the upper back and
        # the shoulder on its side.
        ul, ur = across["cape_l"], across["cape_r"]
        share = {"cape_l": np.clip((0.5 - u) / (0.5 - ul), 0, 1), "cape_r": np.clip((u - 0.5) / (ur - 0.5), 0, 1)}
        share["cape"] = 1.0 - share["cape_l"] - share["cape_r"]
        w = {}
        for prefix, points in chains.items():
            span = points[0][2] - points[-1][2]
            t = np.clip((points[0][2] - P[:, 2]) / span, 0, 1) * 3.0
            for i in (1, 2, 3):
                w["%s_%02d" % (prefix, i)] = np.clip(1 - np.abs(t - (i - 0.5)), 0, 1) * share[prefix]
        hold = np.clip(1 - v / 0.14, 0, 1)
        left = P[:, 1] > 0
        for k in w:
            w[k] = w[k] * (1 - hold)
        w["spine_03"] = hold * 0.6
        w["clavicle_l"] = hold * 0.4 * left
        w["clavicle_r"] = hold * 0.4 * ~left
        total = sum(w.values())
        return {k: (x / np.maximum(total, 1e-6)).astype(np.float32) for k, x in w.items()}
    # Its hem torn: each strip ends at its own length, pointed.
    return sheet.Sheet("cloak", position, material, bones, columns, rows, reach=lambda u: sheet.torn(u, strips, 0.62, 0.14, 3.0))


def coat_tails(S, L, dims, mats):
    """The coat's long tails: leather panels from the belts to the knees, open at the front, their hems torn into
    tongues; each half follows its own thigh a little."""
    pz = L["pelvis"][0][2]
    material = S.material("coat_tails", PALETTE["leather"])
    columns, rows, strips = 12, 4, 6
    t0, t1 = np.radians(24.0), np.radians(336.0)
    top_z, hem_z = pz + 14.0, 46.0
    limbs = [(L["thigh_" + s][0], L["thigh_" + s][1], 8.2) for s in ("l", "r")]

    def position(u, v):
        theta = t0 + (t1 - t0) * u
        radial = np.stack([np.cos(theta), np.sin(theta), np.zeros_like(theta)], axis=1)
        top = np.stack([12.6 * np.cos(theta), 17.5 * np.sin(theta), np.full_like(theta, top_z)], axis=1)
        p = top + radial * (5.0 * v ** 1.3 + (0.3 + 1.2 * v) * np.sin(theta * 7.0))[:, None]
        p[:, 2] = top_z + (hem_z - top_z) * v
        return sheet.clear_of(p, limbs, 1.2)

    def bones(P, u, v):
        # Each half down its own chain (ADR-069), the top held at the belts.
        left = P[:, 1] > 0
        hold = np.clip(1 - v / 0.15, 0, 1)
        lower = np.clip((v - 0.25) / 0.5, 0, 1) * (1 - hold)
        upper = (1 - hold) - lower
        w = {"pelvis": hold}
        for side, mask in (("l", left), ("r", ~left)):
            w["coat_%s_01" % side] = upper * mask
            w["coat_%s_02" % side] = lower * mask
        return {k: np.asarray(x, dtype=np.float32) for k, x in w.items()}
    return sheet.Sheet("coat_tails", position, material, bones, columns, rows, reach=lambda u: sheet.torn(u, strips, 0.75, 0.1, 5.0))


def clothes(S, mats, L, dims, body, limbs):
    """His clothes in layers, each over those beneath and each keeping to its limbs: trousers and boots, the shirt and
    its rolled cuffs, the coat and its collar, belts, a bandolier and bracers."""
    pz = L["pelvis"][0][2]
    cz = L["spine_03"][1][2]
    # Trousers: from inside the boots to under the belts, bunched above the boots.
    legs = both(band_z(8.0, pz + 15.0), garments.keep_to(limbs, ["leg_l", "leg_r", "torso"]))
    trousers = Shell(S, "trousers", body, 0.35, 0.7, legs, mats["trousers"], hem=0.3, displace=bunch((0, 0, 30.0), (0, 0, 44.0), 3.0, 1.0, seed=11), reach=1.5)
    # Boots: heavy, to just below the knee, a thick sole under the foot and a capped toe.
    boot_mats = {"boot": mats["boot"], "sole": mats["sole"]}
    boots = [garments.boot(S, "boot_" + side, L, side, 34.0, boot_mats, None) for side in ("l", "r")]
    lower = Over([body, trousers] + boots)
    # The shirt: torso above the belts and the upper arms, its cuffs rolled thick at the elbow.
    neckline = Zone(lambda P: np.maximum(np.maximum(2.0 - P[:, 0], np.abs(P[:, 1]) - np.clip((P[:, 2] - (cz - 15.0)) * 0.3, 0, 5.0)), (cz - 15.0) - P[:, 2]),
                    Box((0, -8, cz - 18), (30, 8, cz + 10)))
    shirt_region = without(either(both(band_z(pz + 12.0, cz - 0.5), garments.keep_to(limbs, ["torso"])),
                                  garments.keep_to(limbs, ["upperarm_l", "upperarm_r"])), neckline)
    shirt = Shell(S, "shirt", body, 0.25, 0.55, shirt_region, mats["shirt"], hem=0.3, reach=1.0)
    cuffs = []
    for side in ("l", "r"):
        e, w = V(*L["lowerarm_" + side][0]), V(*L["lowerarm_" + side][1])
        s0 = V(*L["upperarm_" + side][0])
        a = e + unit(s0 - e) * 3.0
        b = e + unit(w - e) * 4.5
        cuff_region = both(around([a, b], [8.0, 7.0]), garments.keep_to(limbs, ["upperarm_" + side, "forearm_" + side]))
        cuffs.append(Shell(S, "cuff_" + side, body, 0.6, 1.9, cuff_region, mats["shirt"], hem=0.5, displace=bunch(a, b, 2.0, 0.8, seed=31 if side == "l" else 32), reach=2.0))
    dressed = Over([lower, shirt] + cuffs)
    # The coat: dark leather over the shirt from the belts to the shoulders, open in a V at the front, its sleeves to
    # just above the elbow where the shirt's rolled cuffs show beneath.
    v_cut = Zone(lambda P: np.maximum(np.maximum(3.0 - P[:, 0], np.abs(P[:, 1]) - np.clip((P[:, 2] - (cz - 26.0)) * 0.36, 0, 9.0)),
                                      (cz - 26.0) - P[:, 2]), Box((0, -12, cz - 30), (30, 12, cz + 10)))
    sleeves = []
    for side in ("l", "r"):
        s0, e = V(*L["upperarm_" + side][0]), V(*L["upperarm_" + side][1])
        cuff = s0 + (e - s0) * 0.86
        sleeves.append(both(garments.keep_to(limbs, ["upperarm_" + side]), half(cuff, unit(e - s0), Box.around([s0, e], 16.0))))
    coat_region = without(either(both(band_z(pz + 11.0, cz - 1.0), garments.keep_to(limbs, ["torso"])), *sleeves), v_cut)
    coat = Shell(S, "coat", Over([body, shirt]), 0.75, 1.0, coat_region, mats["leather"], hem=0.35, displace=folds((0, 0, 1), 9, 0.5, seed=41), reach=1.0)
    # The high collar: a leather band standing round the neck, open at the throat, flaring as it rises.
    collar = Zone(lambda P: np.maximum(np.maximum(cz - 2.0 - P[:, 2], P[:, 2] - (cz + 9.0)), np.cos(np.arctan2(P[:, 1], P[:, 0] - 1.0)) - 0.62),
                  Box((-14, -14, cz - 4), (14, 14, cz + 11)))
    neck_ring = tree.leaf(S, "collar_core", lambda P: sdf.round_cone(P, V(0.5, 0, cz - 3.0), V(-0.5, 0, cz + 9.0), 7.4, 9.4),
                          Box((-14, -14, cz - 6), (14, 14, cz + 12)), mats["leather"], None)
    collar_band = tree.Intersect(tree.Subtract(neck_ring, tree.leaf(S, "collar_inner", lambda P: sdf.round_cone(P, V(0.5, 0, cz - 4.0), V(-0.5, 0, cz + 10.0), 6.4, 8.4),
                                                                   Box((-14, -14, cz - 6), (14, 14, cz + 12)), mats["leather"], None)), collar, 0.3)
    # Two wide belts over the coat, the lower dropping toward his left hip, round the torso only.
    belts = []
    for name, centre, drop in (("belt_upper", pz + 23.2, 0.0), ("belt_lower", pz + 16.6, -0.06)):
        band = both(garments.tilted_band(V(0, 0, centre), unit(V(0, drop, 1)), 4.4, Box((-40, -40, centre - 8), (40, 40, centre + 8))),
                    garments.keep_to(limbs, ["torso"]))
        belts.append(Shell(S, name, Over([body, shirt, coat]), 0.1, 0.9, band, mats["belt"], hem=0.2))
    outfit = Over([dressed, coat, collar_band] + belts)
    # The bandolier: a strap across the chest and back from his right shoulder to his left hip.
    shoulder = abs(L["upperarm_l"][0][1])
    a, b = V(0, -shoulder + 3.0, cz - 1.0), V(0, 13.0, pz + 12.0)
    across = unit(np.cross(b - a, V(1, 0, 0)))
    band = both(garments.band_plane((a + b) / 2, across, 4.6, Box((-30, -40, pz), (30, 40, cz + 4))), garments.keep_to(limbs, ["torso"]))
    bandolier = Shell(S, "bandolier", outfit, 0.2, 0.75, band, mats["belt"], hem=0.2, reach=0.5)
    # Bracers: hard leather over the forearm from the wrist halfway to the elbow.
    bracers = []
    for side in ("l", "r"):
        e, w = V(*L["lowerarm_" + side][0]), V(*L["lowerarm_" + side][1])
        region = both(around([w, w + (e - w) * 0.5], [6.5, 7.0]), garments.keep_to(limbs, ["forearm_" + side]))
        bracers.append(Shell(S, "bracer_" + side, outfit, 0.25, 1.1, region, mats["leather"], hem=0.3, reach=0.5))
    return Over([outfit, bandolier] + bracers)


def rifle(S, mats, dims, spec):
    """His rifle (Character Bible §2): long and ornate, dark steel and brass, teal channels glowing along its body and
    shroud, a large round optic with a teal lens. Built along the aim's bore line, its grip in his right hand and its
    fore-end in his left, then moved with his right hand to its rest; it hangs from the hand's prop bone, so his clips
    carry it, raise it to fire and let it fall."""
    H = dims["height"]
    aim = spec["aimBore"]
    y0, z0 = -H * aim["rightShare"], H * aim["boreShare"]
    grip_x, fore_x = H * aim["gripShare"], H * aim["foreShare"]
    hold = anatomy.rigid("prop_r")
    steel, brass, teal = mats["steel"], mats["brass"], mats["teal"]
    parts = []

    def loft(name, x0, x1, stations, material, cap=0.4):
        shape = sdf.Loft(V(x0, y0, z0), V(x1, y0, z0), (0, 0, 1), stations, cap)
        parts.append(tree.leaf(S, name, shape, Box.around(shape.bounds_points()), material, hold, protect=1.0))

    def ring(name, x, radius, width, material):
        parts.append(tree.leaf(S, name, lambda P, c=V(x, y0, z0): sdf.round_cone(P, c - V(width / 2, 0, 0), c + V(width / 2, 0, 0), radius, radius),
                               Box(V(x - width, y0 - radius, z0 - radius), V(x + width, y0 + radius, z0 + radius)), material, hold, protect=1.0))

    # The stock: a tall butt in the shoulder, narrowing into the wrist before the grip.
    butt = grip_x - 17.0
    loft("stock", butt, grip_x - 1.0, [(0.0, -4.6, 0, 6.2, 2.1, 3.2), (0.35, -3.6, 0, 5.0, 1.9, 3.2), (0.75, -1.8, 0, 3.0, 1.6, 3.0), (1.0, -1.2, 0, 2.6, 1.6, 3.0)], steel)
    loft("butt_plate", butt - 0.8, butt + 0.6, [(0.0, -4.6, 0, 6.6, 2.3, 3.6), (1.0, -4.6, 0, 6.6, 2.3, 3.6)], brass)
    # The receiver, its grip angled down and back under his right hand.
    loft("receiver", grip_x - 2.0, grip_x + 14.0, [(0.0, 0.0, 0, 3.1, 2.3, 4.0), (1.0, 0.2, 0, 2.8, 2.1, 4.0)], steel)
    grip_a, grip_b = V(grip_x + 1.5, y0, z0 - 2.4), V(grip_x - 1.6, y0, z0 - H * aim["gripDropShare"] - 4.5)
    parts.append(tree.leaf(S, "grip", lambda P: sdf.round_cone(P, grip_a, grip_b, 1.6, 1.4), Box.around([grip_a, grip_b], 2.0), steel, hold, protect=1.0))
    # The shroud over the barrel, ringed in brass, and the fore-end under it where his left hand holds.
    loft("shroud", grip_x + 13.0, grip_x + 58.0, [(0.0, 0.0, 0, 2.3, 2.1, 2.6), (0.5, 0.0, 0, 2.1, 1.95, 2.4), (1.0, 0.0, 0, 1.8, 1.8, 2.2)], steel)
    loft("fore_end", fore_x - 7.0, fore_x + 8.0, [(0.0, -1.6, 0, 1.8, 1.9, 3.0), (1.0, -1.4, 0, 1.6, 1.8, 3.0)], steel)
    for i, x in enumerate(np.arange(grip_x + 20.0, grip_x + 58.0, 12.0)):
        ring("shroud_ring_%d" % i, x, 2.5, 1.4, brass)
    loft("barrel", grip_x + 57.0, grip_x + 86.0, [(0.0, 0, 0, 1.25, 1.25, 2.0), (1.0, 0, 0, 1.15, 1.15, 2.0)], steel)
    loft("muzzle", grip_x + 85.0, grip_x + 91.0, [(0.0, 0, 0, 1.8, 1.8, 2.2), (1.0, 0, 0, 1.7, 1.7, 2.2)], brass)
    # The teal channels: glowing strips set into both flanks, from the receiver along the shroud.
    for sign in (1.0, -1.0):
        a, b = V(grip_x, y0 + sign * 2.15, z0 + 0.6), V(grip_x + 56.0, y0 + sign * 1.75, z0 + 0.6)
        parts.append(tree.leaf(S, "channel_%d" % (sign > 0), lambda P, a=a, b=b: sdf.capsule(P, a, b, 0.7), Box.around([a, b], 1.2), teal, hold, protect=1.0))
    # The large round optic on top: its tube on brass mounts, its objective bell wide, a teal lens at its front.
    oz = z0 + 6.6
    scope = sdf.Loft(V(grip_x + 1.0, y0, oz), V(grip_x + 27.0, y0, oz), (0, 0, 1),
                     [(0.0, 0, 0, 2.6, 2.6, 2.0), (0.14, 0, 0, 2.2, 2.2, 2.0), (0.3, 0, 0, 1.9, 1.9, 2.0), (0.62, 0, 0, 1.9, 1.9, 2.0), (0.8, 0, 0, 3.6, 3.6, 2.0),
                      (1.0, 0, 0, 4.1, 4.1, 2.0)], 0.4)
    parts.append(tree.leaf(S, "scope", scope, Box.around(scope.bounds_points()), steel, hold, protect=1.0))
    for i, x in enumerate((grip_x + 7.0, grip_x + 18.0)):
        mount = lambda P, c=V(x, y0, z0 + 3.5): sdf.box(P, c, (1.1, 1.5, 3.2), None, 0.4)  # noqa: E731
        parts.append(tree.leaf(S, "scope_mount_%d" % i, mount, Box(V(x - 2, y0 - 2, z0), V(x + 2, y0 + 2, oz + 2)), brass, hold, protect=1.0))
    lens = lambda P, c=V(grip_x + 27.2, y0, oz): sdf.ellipsoid(P, c, (0.6, 3.7, 3.7))  # noqa: E731
    parts.append(tree.leaf(S, "lens", lens, Box(V(grip_x + 26, y0 - 4, oz - 4), V(grip_x + 28.5, y0 + 4, oz + 4)), teal, hold, protect=1.0))
    # Moved, unturned, with his right hand from the aim to its rest.
    held = Union(parts, k=0.25)
    return Placed(held, V(*dims["holds"]["offset"]["r"]), np.eye(3))
