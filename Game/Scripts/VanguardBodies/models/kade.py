"""Kade, Dead Reckoning (ADR-069): a marksman caught mid-sight. Character Bible §2 and the author's reference
(2026-10-06): messy windswept brown hair and stubble; a pale cream shirt, sleeves rolled; a dark leather coat with a
high collar, segmented plate, many belts and buckled straps; articulated bracers and fingerless gloves; a line-work
tattoo down one forearm; trousers bound with thigh straps; heavy buckled boots; a long tattered crimson cloak wrapped
at the shoulders. His rifle is long and ornate, dark steel and brass, teal channels glowing its length and a large
round optic with a teal lens: the only cool note.

Proportions are the kit's (legShare, hipShare, shoulderShare, armShare, headShare: measured from the reference at 7.95
pixels a centimetre for his 171); colours are sampled from the reference (sRGB)."""
import numpy as np

from ..sculpt import anatomy, face, garments, hair, paint, sdf, sheet, tree
from ..sculpt.anatomy import V, unit
from ..sculpt.garments import around, band_z, both, either, folds, bunch, half, without
from ..sculpt.tree import Box, Over, Placed, Shell, Union, Zone

# The skull's size (a 24 cm template's scale), so the reference's face fits: its chin at the head joint (148 cm), its
# eyes 8.4 above, its crown near 166; the hair rises above it to the top of the head bone.
SKULL = 20.4
# The head bows to the optic: pitched forward and rolled toward the stock, about the head joint (degrees).
HEAD_PITCH, HEAD_ROLL = 24.0, 16.0
# The hand, wrist to fingertip, as a share of the height.
HAND_SHARE = 0.108

# Sampled from the reference (sRGB).
PALETTE = {
    "skin": (0.78, 0.54, 0.45), "shirt": (0.86, 0.78, 0.75), "leather": (0.15, 0.12, 0.11), "belt": (0.22, 0.15, 0.13),
    "hair": (0.26, 0.18, 0.17), "teal": (0.1, 0.9, 0.85), "cloak": (0.62, 0.06, 0.1), "brass": (0.62, 0.47, 0.3),
    "steel": (0.2, 0.19, 0.2), "trousers": (0.19, 0.15, 0.14), "boot": (0.3, 0.19, 0.14), "glove": (0.2, 0.15, 0.13),
}


def materials(S):
    """Every material Kade is painted in: its colour as the reference shows it, its grain and its wear."""
    m = {}
    m["skin"] = S.material("skin", paint.painted(PALETTE["skin"], grain=0.04, scale=2.0, wear=0.0, cavity=0.75), preview=PALETTE["skin"])
    m["shirt"] = S.material("shirt", paint.painted(PALETTE["shirt"], grain=0.06, scale=1.5, wear=0.05, cavity=0.6), preview=PALETTE["shirt"])
    m["leather"] = S.material("leather", paint.painted(PALETTE["leather"], grain=0.18, scale=1.2, wear=0.25, cavity=0.5), preview=PALETTE["leather"])
    m["belt"] = S.material("belt", paint.painted(PALETTE["belt"], grain=0.15, scale=1.0, wear=0.3, cavity=0.5), preview=PALETTE["belt"])
    m["brass"] = S.material("brass", paint.painted(PALETTE["brass"], grain=0.1, scale=0.6, wear=0.4, cavity=0.45, tint=(0.95, 0.82, 0.55)), preview=PALETTE["brass"])
    m["hair"] = S.material("hair", paint.painted(PALETTE["hair"], grain=0.2, scale=0.8, wear=0.1, cavity=0.55), preview=PALETTE["hair"])
    m["glove"] = S.material("glove", paint.painted(PALETTE["glove"], grain=0.15, scale=0.8, wear=0.2, cavity=0.5), preview=PALETTE["glove"])
    m["trousers"] = S.material("trousers", paint.painted(PALETTE["trousers"], grain=0.1, scale=2.0, wear=0.08, cavity=0.5), preview=PALETTE["trousers"])
    m["boot"] = S.material("boot", paint.painted(PALETTE["boot"], grain=0.2, scale=1.0, wear=0.3, cavity=0.45), preview=PALETTE["boot"])
    m["cloak"] = S.material("cloak", paint.painted(PALETTE["cloak"], grain=0.35, scale=6.0, wear=0.1, cavity=0.5), preview=PALETTE["cloak"])
    m["steel"] = S.material("steel", paint.painted(PALETTE["steel"], grain=0.06, scale=0.8, wear=0.45, cavity=0.45, tint=(0.55, 0.55, 0.58)), preview=PALETTE["steel"])
    m["teal"] = S.material("teal", lambda ctx: np.tile(paint.linear(PALETTE["teal"]), (len(ctx["P"]), 1)).astype(np.float32), glow=paint.glowing(1.0), preview=PALETTE["teal"])
    m["sole"] = S.material("sole", paint.painted((0.1, 0.08, 0.075), grain=0.1, scale=0.5, wear=0.1, cavity=0.5), preview=(0.1, 0.08, 0.075))
    return m


def build(S, L, dims, spec):
    """Kade's sculpt on his layout: (the whole, {"body": the skin alone, under the garments})."""
    mats = materials(S)
    H = dims["height"]
    # Lean and long, as the reference stands: narrower through the chest and limbs than a heroic build.
    figure = anatomy.Figure(S, L, dims, mats["skin"], {"muscle": 0.5, "chest": 0.95, "breadth": 0.82, "limb": 0.85, "deltoid": 0.8, "leg": 0.88, "neck": 0.82}).build()
    head_origin = V(*L["head"][0])
    # Mid-sight his head bows to the optic; any other stance carries it level.
    aiming = spec.get("stance") == "aim"
    head_axes = sdf.rotation(pitch=HEAD_PITCH) @ sdf.rotation(roll=HEAD_ROLL) if aiming else np.eye(3)
    # His face, painted in the head's frame: intent hazel eyes, dark brows, stubble and a goatee (the reference).
    mats["skin"].colour = face.face(mats["skin"].colour, (head_origin, head_axes), SKULL / 24.0,
                                    {"eyeColour": (0.34, 0.27, 0.15), "browColour": (0.16, 0.1, 0.08), "eyeTilt": 0.3, "stubble": 0.8, "goatee": True})
    # A line-work tattoo down his right forearm, between the rolled cuff and the bracer (the reference).
    mats["skin"].colour = paint.inked(mats["skin"].colour, _vine(V(*L["lowerarm_r"][0]), V(*L["lowerarm_r"][1])), (0.13, 0.1, 0.1))
    # His hair: messy and windswept, falling over the brow, swept to his right (the reference).
    u = SKULL / 24.0
    locks, guides = hair.messy(S, mats["hair"], anatomy.rigid("head"), (-1.2, 0, 14.4), (9.9, 7.5, 8.0), (16.0, 6.5), seed=spec["seed"],
                               unit_scale=u, count=96, length=(6.5, 11.0), radius=(1.1, 1.9), wind=(0.0, -0.3, 0.0), fringe=0.6, volume=0.32)
    # Hair cards along every lock, over the solid core: strands, fraying at their tips (author 2026-10-07: cards).
    card_paint = hair.card_material(S, "hair_card", PALETTE["hair"], (0.55, 0.38, 0.28))
    head_frame = (head_origin, head_axes)
    cards = [sheet.Sheet("hair_card_%02d" % i, sheet.ribbon(g["points"], g["normal"], g["radius"] * 2.6, head_frame, lift=0.12 * u), card_paint,
                         lambda P, cu, cv: {"head": np.ones(len(P), dtype=np.float32)}, 2, 7) for i, g in enumerate(guides)]
    figure.attach_head(Placed(Over([anatomy.Head(S, mats["skin"], SKULL, look={"jaw": 0.92}).build(), locks]), head_origin, head_axes))
    for side in ("l", "r"):
        wrist = V(*L["hand_" + side][0])
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
        # A fingerless glove: dark leather over the palm and the fingers' first bones, the rest of each finger bare.
        bare = hand.build()
        glove_region = Zone(lambda P, k=hand.u: P[:, 0] - 11.2 * k, Box((-5, -10, -6), (11.2 * hand.u, 10, 6)))
        glove = Shell(S, "glove_" + side, bare, 0.08, 0.32, glove_region, mats["glove"], hem=0.12, reach=0.5, detail=True)
        placed = Placed(Over([bare, glove]), wrist, np.stack([x, y, z], axis=1))
        figure.parts.append(placed)
        figure.limbs["hand_" + side] = placed
    body = figure.body()
    clothed = clothes(S, mats, L, dims, body, figure.limbs)
    worn = Over([clothed, scarf(S, L, mats, clothed, figure.limbs)] + ([rifle(S, mats, L, dims, spec)] if aiming else []))
    return worn, {"body": body, "sheets": cards + [cloak(S, L, dims, worn), coat_tails(S, L, dims)]}


def _hashed(x, salt):
    """A repeatable value in [0, 1) for each whole x: a strip's own length, a lock's own twist."""
    return np.abs(np.modf(np.sin(np.asarray(x, dtype=np.float64) * 12.9898 + salt * 78.233) * 43758.5453)[0])


def scarf(S, L, mats, under, limbs):
    """The cloak's top wrapped as the reference shows it: a short mantle of crimson cloth over his shoulders, its hem
    torn into tongues; over it the cloth bunched round his neck in heavy folds, close behind the standing collar,
    heaped on his left shoulder and swagged down across his left breast, lying over his right shoulder to the brass
    brooch that pins it there, and a leather strap hanging from the brooch to his belt."""
    cz = L["spine_03"][1][2]
    pz = L["pelvis"][0][2]
    shoulder = abs(L["upperarm_l"][0][1])
    # The mantle: from the neck over the tops of his shoulders and down his back, longer over his left arm, to a hem
    # torn into tongues of their own lengths; open at the chest a little toward his left, its folds falling from the neck.
    tongues, open_at = 22, 0.3

    def mantle_region(P):
        angle = np.arctan2(P[:, 1], P[:, 0])
        strip = (angle + np.pi) / (2.0 * np.pi) * tongues
        tip = 1.0 - np.abs(2.0 * (strip % 1.0) - 1.0)
        hem = (cz - 2.0 - 4.5 * np.abs(np.sin(angle)) ** 1.2 - 4.0 * np.clip(np.sin(angle), 0, 1) - 6.0 * np.clip(-np.cos(angle), 0, 1)
               - (1.5 + 4.5 * _hashed(np.floor(strip), 9.0)) * tip ** 0.7)
        return np.max(np.stack([hem - P[:, 2], P[:, 2] - (cz + 8.0), (np.cos(angle - open_at) - 0.78) * 12.0]), axis=0)
    # Over the torso and down the upper arms, so it hangs over the shoulders rather than standing off them.
    region = both(Zone(mantle_region, Box((-35, -shoulder - 12, cz - 30), (35, shoulder + 12, cz + 9))),
                  garments.keep_to(limbs, ["torso", "upperarm_l", "upperarm_r"]))
    mantle = Shell(S, "mantle", under, 0.25, 1.0, region, mats["cloak"], hem=0.35,
                   displace=garments.combine(folds((0, 0, 1), 13, 1.1, seed=96), folds((0, 0, 1), 29, 0.35, seed=97)), reach=1.5)

    def at(degrees, radius, rise):
        a = np.radians(degrees)
        return V(radius * np.cos(a), radius * np.sin(a), cz + rise)
    # The folds, all lying on the mantle: (name, its line round him (degrees from the front toward his left,
    # cm out from his spine, cm above the neck's root), how wide either side of it and how high it stands).
    lines = [("scarf_back", [at(-80, 10.0, 2.0), at(-125, 9.5, 2.8), at(180, 9.0, 3.2), at(125, 9.5, 2.8), at(80, 10.0, 2.0)], 4.2, 2.0),
             ("scarf_shoulder_l", [at(100, 9.5, 2.0), at(92, 14.0, 0.5), at(88, shoulder + 1.0, -2.5)], 5.0, 1.8),
             ("scarf_swag_high", [at(80, 10.0, 2.5), at(55, 11.0, 0.0), at(42, 14.0, -5.0), at(45, 17.0, -10.0), at(55, 19.0, -14.0)], 4.4, 2.2),
             ("scarf_swag_low", [at(20, 10.0, -2.0), at(22, 12.5, -7.0), at(35, 15.5, -12.0), at(50, 18.0, -17.0)], 4.0, 1.9),
             ("scarf_shoulder_r", [at(-55, 10.0, 1.5), at(-68, 13.5, 0.0), at(-75, 17.0, -2.5), at(-82, shoulder, -4.0)], 4.2, 1.7)]
    # (Each on the mantle rather than on the folds before it: a fold on a fold would evaluate the whole body again for
    # each it lay on, and where two cross they merge as cloth bunched together does.)
    draped = Over([under, mantle])
    layers = [mantle] + [garments.fold(S, name, draped, garments.laid_on(draped, points), width, height, mats["cloak"], seed=90 + i)
                         for i, (name, points, width, height) in enumerate(lines)]
    cloth = Over([under] + layers)
    # The brooch, pinning the cloth at the front of his right shoulder: a brass disc, a raised rim and a boss.
    toward = unit(V(np.cos(np.radians(-50.0)), np.sin(np.radians(-50.0)), 0.3))
    (brooch_at,), (n,) = garments.surface_points(cloth, [at(-50, 15.0, -3.0) + toward * 20.0], [-toward], reach=30.0)
    axes = garments.facing_frame(n)
    brooch = tree.leaf(S, "brooch", lambda P, c=brooch_at, n=n, R=axes: np.minimum(np.minimum(sdf.ellipsoid(P, c, (2.7, 2.7, 0.8), R),
                                                                                           sdf.torus(P, c + n * 0.55, 2.55, 0.42, R)),
                                                                                sdf.sphere(P, c + n * 0.75, 1.0)),
                       Box(brooch_at - 4, brooch_at + 4), mats["brass"], None)
    # The strap hanging from it down his chest to the upper belt.
    drop = Box((-5, brooch_at[1] - 4, pz + 20), (30, brooch_at[1] + 4, brooch_at[2]))
    hanging = both(garments.band_plane(brooch_at, (0, 1, 0), 3.4, drop), half(V(0, 0, 0), V(-1, 0, 0), drop), band_z(pz + 22.0, brooch_at[2] - 1.5),
                   garments.keep_to(limbs, ["torso"]))
    strap = Shell(S, "brooch_strap", under, 0.1, 0.55, hanging, mats["belt"], hem=0.2)
    return Over(layers + [strap, brooch])

def _laces(S, mats, under, cz):
    """The shirt's open neck laced across with a leather cord (the reference): crossing between eyelets either side of
    the opening, standing a little off the chest between them (baked detail)."""
    heights = [cz - 13.0, cz - 10.5, cz - 8.0, cz - 5.5]
    # The opening's half width at each height, as the shirt's neckline cuts it, and the eyelets just outside it.
    half_width = [min(max((z - (cz - 15.0)) * 0.3, 0.0), 5.0) + 0.7 for z in heights]
    starts = [V(40.0, side * w, z) for z, w in zip(heights, half_width) for side in (1.0, -1.0)]
    eyelets, _normals = garments.surface_points(under, starts, [V(-1.0, 0.0, 0.0)] * len(starts), reach=45.0)
    left, right = eyelets[0::2], eyelets[1::2]
    cords = []
    for i in range(len(heights) - 1):
        for a, b in ((left[i], right[i + 1]), (right[i], left[i + 1])):
            cords.append([a, (a + b) * 0.5 + V(0.5, 0.0, 0.0), b])

    def distance(P):
        return np.min(np.stack([sdf.tube(P, [tuple(p) for p in cord], [0.17, 0.17, 0.17]) for cord in cords]), axis=0)
    return tree.leaf(S, "shirt_laces", distance, Box.around(eyelets, 1.5), mats["belt"], None, True)


def _vine(elbow, wrist, start=0.16, end=0.55, girth=3.6):
    """A line-work tattoo down a forearm: two thorned stems winding about each other from the cuff to the bracer. Its
    ink's coverage at P."""
    axis = wrist - elbow
    length = float(np.linalg.norm(axis))
    axis = axis / length
    ref = np.cross(axis, V(0, 0, 1))
    ref = unit(ref if np.linalg.norm(ref) > 0.1 else np.cross(axis, V(1, 0, 0)))
    other = np.cross(axis, ref)

    def coverage(P):
        rel = P.astype(np.float64) - elbow
        s = rel @ axis / length
        angle = np.arctan2(rel @ other, rel @ ref)
        along = s * length
        cover = np.zeros(len(P))
        # (where it starts round the arm, how far it sways, its line's half width, its thorns' spacing; cm)
        for phase, sway, width, thorns in ((0.0, 0.9, 0.16, 1.7), (np.pi, 0.65, 0.11, 0.0)):
            stem = phase + sway * np.sin(along / 6.0 * 2.0 * np.pi + phase)
            across = ((angle - stem + np.pi) % (2.0 * np.pi) - np.pi) * girth
            cover = np.maximum(cover, np.clip(1.0 - (np.abs(across) - width) / 0.06, 0.0, 1.0))
            if thorns:
                k = np.round(along / thorns)
                out = across * np.where(k % 2 == 0, 1.0, -1.0) - width
                spike = (out > 0) & (out < 0.9) & (np.abs(along - k * thorns) < 0.3 * (1.0 - out / 0.9))
                cover = np.maximum(cover, spike.astype(np.float64))
        window = np.clip((s - start) / 0.03, 0.0, 1.0) * np.clip((end - s) / 0.03, 0.0, 1.0)
        return (cover * window).astype(np.float32)
    return coverage

def _clear_of(points, limbs, margin):
    """points pushed out of each limb (a segment and its radius) to its surface plus margin: cloth over a raised arm."""
    P = points.copy()
    for a, b, radius in limbs:
        a, b = V(*a), V(*b)
        ab = b - a
        t = np.clip(((P - a) @ ab) / (ab @ ab), 0, 1)
        nearest = a + t[:, None] * ab
        away = P - nearest
        dist = np.linalg.norm(away, axis=1)
        inside = dist < radius + margin
        P[inside] = nearest[inside] + away[inside] / np.maximum(dist[inside], 1e-6)[:, None] * (radius + margin)
    return P


def _cloth_paint(S, name, colour, dark, strips, cut, point, holes, seed):
    """Tattered cloth's paint: colour mottled toward dark, its hem torn into strips (each ending at its own share of
    the length, pointed), holes through its lower part where noise runs high."""
    base, shade = paint.linear(colour), paint.linear(dark)

    def colour_of(ctx):
        mottle = np.clip((sdf.fbm(ctx["P"].astype(np.float32), 9.0, 3, seed) - 0.45) * 1.5, 0, 1)
        c = base[None, :] * (1 - mottle[:, None]) + shade[None, :] * mottle[:, None]
        return np.clip(c * (0.55 + 0.45 * ctx["ao"])[:, None], 0, 1).astype(np.float32)

    def opacity_of(ctx):
        u, v = ctx["uv"][:, 0], ctx["uv"][:, 1]
        strip = np.floor(u * strips)
        across = np.abs((u * strips) % 1.0 - 0.5) * 2.0
        ends = cut + (1.0 - cut) * _hashed(strip, 3.0) - point * across
        torn = sdf.fbm(ctx["P"].astype(np.float32), 4.0, 2, seed + 7)
        hole = (torn > holes) & (v > 0.35)
        return ((v < ends) & ~hole).astype(np.float32)
    return S.material(name, colour_of, opacity=opacity_of, preview=colour)


def cloak(S, L, dims, worn):
    """His long tattered crimson cloak: its top lying on his shoulders (found on the clothed body) under the scarf,
    falling behind his arms, its hem longer down his left side and back, flaring wide and pleated unevenly as it
    falls, torn into strips and holed."""
    cz = L["spine_03"][1][2]
    paint_cloak = _cloth_paint(S, "cloak_sheet", PALETTE["cloak"], (0.24, 0.02, 0.04), strips=26, cut=0.72, point=0.14, holes=0.7, seed=61)
    t0, t1 = np.radians(55.0), np.radians(305.0)
    hem_u, hem_z = [0.0, 0.22, 0.5, 0.78, 1.0], [34.0, 10.0, 16.0, 22.0, 40.0]
    limbs = [(L["upperarm_" + s][0], L["upperarm_" + s][1], 7.0) for s in ("l", "r")] + [(L["lowerarm_" + s][0], L["lowerarm_" + s][1], 5.5) for s in ("l", "r")]
    # The top edge: where a ray in toward the body at the shoulders' height first meets the clothed body, a little out.
    samples = np.linspace(0.0, 1.0, 25)
    tops = []
    for s in samples:
        theta = t0 + (t1 - t0) * s
        side = abs(np.sin(theta))
        z = cz - 2.5 - 1.5 * side
        inward = -V(np.cos(theta), np.sin(theta), 0.0)
        hit, normal = garments.surface_point(worn, V(0, 0, z) - inward * 45.0, inward, reach=45.0)
        tops.append(hit + V(np.cos(theta), np.sin(theta), 0.0) * 1.2)
    tops = np.array(tops)
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
        return _clear_of(p, limbs, 1.5)
    chain = [V(*L["cape_01"][0]), V(*L["cape_02"][0]), V(*L["cape_03"][0]), V(*L["cape_03"][1])]

    def bones(P, u, v):
        # Down the cape chain by height; the top held by the upper back and the shoulder on its side.
        z = P[:, 2]
        span = chain[0][2] - chain[-1][2]
        t = np.clip((chain[0][2] - z) / span, 0, 1) * 3.0
        w = {"cape_01": np.clip(1 - np.abs(t - 0.5), 0, 1), "cape_02": np.clip(1 - np.abs(t - 1.5), 0, 1), "cape_03": np.clip(1 - np.abs(t - 2.5), 0, 1)}
        hold = np.clip(1 - v / 0.14, 0, 1)
        left = P[:, 1] > 0
        for k in w:
            w[k] = w[k] * (1 - hold)
        w["spine_03"] = hold * 0.6
        w["clavicle_l"] = hold * 0.4 * left
        w["clavicle_r"] = hold * 0.4 * ~left
        total = sum(w.values())
        return {k: (x / np.maximum(total, 1e-6)).astype(np.float32) for k, x in w.items()}
    return sheet.Sheet("cloak", position, paint_cloak, bones, 36, 24)


def coat_tails(S, L, dims):
    """The coat's long tails: leather panels from the belts to the knees, open at the front, their hems frayed into
    strips; each half follows its own thigh a little."""
    pz = L["pelvis"][0][2]
    paint_tails = _cloth_paint(S, "coat_tails", PALETTE["leather"], (0.08, 0.06, 0.055), strips=18, cut=0.8, point=0.1, holes=0.8, seed=71)
    t0, t1 = np.radians(24.0), np.radians(336.0)
    top_z, hem_z = pz + 14.0, 46.0
    limbs = [(L["thigh_" + s][0], L["thigh_" + s][1], 8.2) for s in ("l", "r")]

    def position(u, v):
        theta = t0 + (t1 - t0) * u
        radial = np.stack([np.cos(theta), np.sin(theta), np.zeros_like(theta)], axis=1)
        top = np.stack([12.6 * np.cos(theta), 17.5 * np.sin(theta), np.full_like(theta, top_z)], axis=1)
        p = top + radial * (5.0 * v ** 1.3 + (0.3 + 1.2 * v) * np.sin(theta * 7.0))[:, None]
        p[:, 2] = top_z + (hem_z - top_z) * v
        return _clear_of(p, limbs, 1.2)

    def bones(P, u, v):
        left = P[:, 1] > 0
        follow = np.clip(v * 0.6, 0, 0.6)
        return {"pelvis": (1 - follow).astype(np.float32), "thigh_l": (follow * left).astype(np.float32), "thigh_r": (follow * ~left).astype(np.float32)}
    return sheet.Sheet("coat_tails", position, paint_tails, bones, 22, 10)


def clothes(S, mats, L, dims, body, limbs):
    """His clothes in layers, each over those beneath: trousers and boots, the shirt, the coat and its collar, belts.
    Each keeps to its limbs (limbs: the figure's), so a sleeve never wraps the hip its arm hangs by."""
    H = dims["height"]
    pz = L["pelvis"][0][2]
    cz = L["spine_03"][1][2]
    wide = Box((-80, -80, -5), (80, 80, 200))
    # Trousers: from inside the boots to under the belts, loose about the shins and bunched above the boots.
    legs = both(band_z(8.0, pz + 15.0), garments.keep_to(limbs, ["leg_l", "leg_r", "torso"]))
    trouser_folds = garments.combine(
        bunch((0, 0, 30.0), (0, 0, 44.0), 3.5, 0.9, seed=11),
        folds((0, 0, 1), 9, 0.3, seed=12))
    trousers = Shell(S, "trousers", body, 0.35, 0.7, legs, mats["trousers"], hem=0.3, displace=trouser_folds, reach=1.5)
    # Boots: heavy, to just below the knee, a thick sole under the foot and a capped toe.
    boot_mats = {"boot": mats["boot"], "sole": mats["sole"], "strap": mats["belt"], "buckle": mats["brass"]}
    boots = [garments.boot(S, "boot_" + side, L, side, 34.0, boot_mats, None,
                           strap_heights=(9.0, 15.0, 21.0, 27.0, 32.0)) for side in ("l", "r")]
    lower = Over([body, trousers] + boots)
    # The shirt: torso above the belts and the arms to the elbow, its cuffs rolled thick there.
    neckline = Zone(lambda P: np.maximum(np.maximum(2.0 - P[:, 0], np.abs(P[:, 1]) - np.clip((P[:, 2] - (cz - 15.0)) * 0.3, 0, 5.0)), (cz - 15.0) - P[:, 2]),
                    Box((0, -8, cz - 18), (30, 8, cz + 10)))
    shirt_region = without(either(both(band_z(pz + 12.0, cz - 0.5), garments.keep_to(limbs, ["torso"])),
                                  garments.keep_to(limbs, ["upperarm_l", "upperarm_r"])), neckline)
    shirt = Shell(S, "shirt", body, 0.25, 0.55, shirt_region, mats["shirt"], hem=0.3, displace=folds((0, 0, 1), 7, 0.3, seed=21), reach=1.0)
    cuffs = []
    for side in ("l", "r"):
        e, w = V(*L["lowerarm_" + side][0]), V(*L["lowerarm_" + side][1])
        s0 = V(*L["upperarm_" + side][0])
        a = e + unit(s0 - e) * 3.0
        b = e + unit(w - e) * 4.5
        roll = bunch(a, b, 2.5, 0.8, seed=31 if side == "l" else 32)
        cuff_region = both(around([a, b], [8.0, 7.0]), garments.keep_to(limbs, ["upperarm_" + side, "forearm_" + side]))
        cuffs.append(Shell(S, "cuff_" + side, body, 0.6, 1.8, cuff_region, mats["shirt"], hem=0.5, displace=roll, reach=2.0))
    dressed = Over([lower, shirt] + cuffs + [_laces(S, mats, Over([body, shirt]), cz)])
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
    # Quilted in diamonds, as the reference's coat is (baked relief).
    coat = Shell(S, "coat", Over([body, shirt]), 0.75, 1.0, coat_region, mats["leather"], hem=0.35,
                 displace=folds((0, 0, 1), 11, 0.35, seed=41), reach=1.0, fine=garments.quilt(3.4, 0.22))
    # The high collar: a leather band standing round the neck, open at the throat, flaring as it rises.
    collar = Zone(lambda P: np.maximum(np.maximum(cz - 2.0 - P[:, 2], P[:, 2] - (cz + 9.0)), np.cos(np.arctan2(P[:, 1], P[:, 0] - 1.0)) - 0.62),
                  Box((-14, -14, cz - 4), (14, 14, cz + 11)))
    neck_ring = tree.leaf(S, "collar_core", lambda P: sdf.round_cone(P, V(0.5, 0, cz - 3.0), V(-0.5, 0, cz + 9.0), 7.4, 9.4) - 0.0,
                          Box((-14, -14, cz - 6), (14, 14, cz + 12)), mats["leather"], None)
    collar_band = tree.Intersect(tree.Subtract(neck_ring, tree.leaf(S, "collar_inner", lambda P: sdf.round_cone(P, V(0.5, 0, cz - 4.0), V(-0.5, 0, cz + 10.0), 6.4, 8.4),
                                                                   Box((-14, -14, cz - 6), (14, 14, cz + 12)), mats["leather"], None)), collar, 0.3)
    # Two wide belts over the coat, the lower dropping toward his left hip.
    belts = []
    for name, centre, drop in (("belt_upper", pz + 23.2, 0.0), ("belt_lower", pz + 16.6, -0.06)):
        # Round the torso only, clear of the arms.
        band = both(garments.tilted_band(V(0, 0, centre), unit(V(0, drop, 1)), 4.4, Box((-40, -40, centre - 8), (40, 40, centre + 8))),
                    garments.keep_to(limbs, ["torso"]))
        belts.append(Shell(S, name, Over([body, shirt, coat]), 0.1, 0.9, band, mats["belt"], hem=0.2))
    outfit = Over([dressed, coat, collar_band] + belts)
    return Over([outfit] + gear(S, mats, L, outfit, pz, cz, limbs))


def gear(S, mats, L, outfit, pz, cz, limbs):
    """What hangs on him: buckles on his belts, a bandolier from his right shoulder to his left hip, pouches on his
    belt and left thigh, straps round his thighs, and bracers on his forearms."""
    pieces = []
    buckle_mat, strap_mat = mats["brass"], mats["belt"]
    # Belt buckles, at the front of each.
    for name, z, y in (("buckle_upper", pz + 23.2, 1.5), ("buckle_lower", pz + 16.6, 6.0)):
        at, normal = garments.surface_point(outfit, V(40, y, z), V(-1, 0, 0), reach=45)
        pieces.append(garments.buckle(S, name, at, garments.facing_frame(normal), 5.2, 4.2, 0.75, 0.9, buckle_mat))
    # The bandolier: a strap across the chest and back from his right shoulder to his left hip.
    shoulder = abs(L["upperarm_l"][0][1])
    a, b = V(0, -shoulder + 3.0, cz - 1.0), V(0, 13.0, pz + 12.0)
    across = unit(np.cross(b - a, V(1, 0, 0)))
    band = both(garments.band_plane((a + b) / 2, across, 4.6, Box((-30, -40, pz), (30, 40, cz + 4))), garments.keep_to(limbs, ["torso"]))
    bandolier = Shell(S, "bandolier", outfit, 0.2, 0.75, band, strap_mat, hem=0.2, reach=0.5)
    pieces.append(bandolier)
    on_top = Over([outfit, bandolier])
    studs, normals = [], []
    for s in np.linspace(0.12, 0.88, 7):
        p = a + (b - a) * s
        at, normal = garments.surface_point(on_top, V(40, p[1], p[2]), V(-1, 0, 0), reach=45)
        studs.append(at)
        normals.append(normal)
    pieces.append(garments.studs(S, "bandolier_studs", studs[::2], normals[::2], 0.55, buckle_mat))
    pieces.append(garments.buckle(S, "bandolier_buckle", studs[3], garments.facing_frame(normals[3], b - a), 3.6, 3.0, 0.55, 0.7, buckle_mat))
    # Pouches on the lower belt at his front left, and a large one on his left thigh.
    for name, theta, size in (("pouch_belt_a", 38.0, (7.0, 8.5, 3.6)), ("pouch_belt_b", 62.0, (6.0, 7.5, 3.2))):
        direction = V(np.cos(np.radians(theta)), np.sin(np.radians(theta)), 0)
        at, normal = garments.surface_point(outfit, V(0, 0, pz + 12.5) + direction * 40, -direction, reach=40)
        pieces.append(garments.pouch(S, name, at, garments.facing_frame(normal), size, 3.0, strap_mat, mats["leather"]))
    thigh_a, thigh_b = V(*L["thigh_l"][0]), V(*L["thigh_l"][1])
    on_thigh = thigh_a + (thigh_b - thigh_a) * 0.45
    out = unit(V(0.45, 1.0, 0))
    at, normal = garments.surface_point(outfit, on_thigh + out * 30, -out, reach=30)
    pieces.append(garments.pouch(S, "pouch_thigh", at, garments.facing_frame(normal), (9.0, 12.0, 4.6), 3.6, strap_mat, mats["leather"]))
    # Thigh straps: four round his right thigh, two round his left holding the pouch, each buckled on the outside.
    for side, shares in (("r", (0.2, 0.36, 0.52, 0.68)), ("l", (0.3, 0.62))):
        ta, tb = V(*L["thigh_" + side][0]), V(*L["thigh_" + side][1])
        sign = 1.0 if side == "l" else -1.0
        for i, share in enumerate(shares):
            centre = ta + (tb - ta) * share
            region = both(band_z(centre[2] - 1.1, centre[2] + 1.1), around([ta, tb], [12.0, 10.0]), garments.keep_to(limbs, ["leg_" + side]))
            strap = Shell(S, "thigh_strap_%s%d" % (side, i), outfit, 0.15, 0.6, region, strap_mat, hem=0.15, reach=0.4, detail=True)
            pieces.append(strap)
            out = unit(V(0.3, sign, 0))
            at, normal = garments.surface_point(Over([outfit, strap]), centre + out * 30, -out, reach=30)
            pieces.append(garments.buckle(S, "thigh_buckle_%s%d" % (side, i), at, garments.facing_frame(normal), 2.6, 2.2, 0.45, 0.6, buckle_mat))
    # Bracers: hard leather over the forearm from the wrist halfway to the elbow, strapped three times.
    for side in ("l", "r"):
        e, w = V(*L["lowerarm_" + side][0]), V(*L["lowerarm_" + side][1])
        region = both(around([w, w + (e - w) * 0.5], [6.5, 7.0]), garments.keep_to(limbs, ["forearm_" + side]))
        bracer = Shell(S, "bracer_" + side, outfit, 0.25, 1.1, region, mats["leather"], hem=0.3, reach=0.5)
        pieces.append(bracer)
        for i, share in enumerate((0.1, 0.25, 0.4)):
            c = w + (e - w) * share
            axis = unit(e - w)
            ring = both(garments.band_plane(c, axis, 1.2, Box(c - 10, c + 10)), around([w, e], [7.5, 7.5]), garments.keep_to(limbs, ["forearm_" + side]))
            pieces.append(Shell(S, "bracer_strap_%s%d" % (side, i), Over([outfit, bracer]), 0.0, 0.45, ring, strap_mat, hem=0.1, reach=0.3, detail=True))
    return pieces


def rifle(S, mats, L, dims, spec):
    """His rifle (Character Bible §2): long and ornate, dark steel and brass, teal channels glowing along its body and
    shroud, a large round optic with a teal lens. It lies along the aim's bore line, its stock in his shoulder under
    his cheek, its grip in his right hand and its fore-end in his left, its barrel reaching well ahead of him."""
    H = dims["height"]
    aim = spec["aimBore"]
    y0, z0 = -H * aim["rightShare"], H * aim["boreShare"]
    grip_x, fore_x = H * aim["gripShare"], H * aim["foreShare"]
    hold = anatomy.rigid("hand_r")
    steel, brass, teal = mats["steel"], mats["brass"], mats["teal"]
    parts = []

    def loft(name, x0, x1, stations, material, cap=0.4, up=(0, 0, 1)):
        shape = sdf.Loft(V(x0, y0, z0), V(x1, y0, z0), up, stations, cap)
        parts.append(tree.leaf(S, name, shape, Box.around(shape.bounds_points()), material, hold))

    def ring(name, x, radius, width, material):
        parts.append(tree.leaf(S, name, lambda P, c=V(x, y0, z0): sdf.round_cone(P, c - V(width / 2, 0, 0), c + V(width / 2, 0, 0), radius, radius),
                               Box(V(x - width, y0 - radius, z0 - radius), V(x + width, y0 + radius, z0 + radius)), material, hold))

    # The stock: a tall butt in the shoulder, its comb under the cheek, narrowing into the wrist before the grip.
    butt = grip_x - 17.0
    loft("stock", butt, grip_x - 1.0, [(0.0, -4.6, 0, 6.2, 2.1, 3.2), (0.35, -3.6, 0, 5.0, 1.9, 3.2), (0.75, -1.8, 0, 3.0, 1.6, 3.0), (1.0, -1.2, 0, 2.6, 1.6, 3.0)], steel)
    loft("butt_plate", butt - 0.8, butt + 0.4, [(0.0, -4.6, 0, 6.6, 2.3, 3.6), (1.0, -4.6, 0, 6.6, 2.3, 3.6)], brass)
    # The receiver: a long chamfered box with brass trim, its grip angled down and back under his right hand.
    loft("receiver", grip_x - 2.0, grip_x + 14.0, [(0.0, 0.0, 0, 3.1, 2.3, 4.0), (1.0, 0.2, 0, 2.8, 2.1, 4.0)], steel)
    grip_a, grip_b = V(grip_x + 1.5, y0, z0 - 2.4), V(grip_x - 1.6, y0, z0 - H * aim["gripDropShare"] - 4.5)
    parts.append(tree.leaf(S, "grip", lambda P: sdf.round_cone(P, grip_a, grip_b, 1.5, 1.3), Box.around([grip_a, grip_b], 2.0), steel, hold))
    parts.append(tree.leaf(S, "guard", lambda P: sdf.torus(P, V(grip_x + 3.6, y0, z0 - 3.4), 1.7, 0.28, sdf.rotation(roll=90.0)),
                           Box(V(grip_x + 1, y0 - 3, z0 - 6), V(grip_x + 6, y0 + 3, z0 - 1)), brass, hold))
    # The shroud over the barrel, ringed in brass, and the fore-end under it where his left hand holds.
    loft("shroud", grip_x + 13.0, grip_x + 58.0, [(0.0, 0.0, 0, 2.3, 2.1, 2.6), (0.5, 0.0, 0, 2.1, 1.95, 2.4), (1.0, 0.0, 0, 1.8, 1.8, 2.2)], steel)
    loft("fore_end", fore_x - 7.0, fore_x + 8.0, [(0.0, -1.6, 0, 1.8, 1.9, 3.0), (1.0, -1.4, 0, 1.6, 1.8, 3.0)], steel)
    for i, x in enumerate(np.arange(grip_x + 16.0, grip_x + 58.0, 8.0)):
        ring("shroud_ring_%d" % i, x, 2.45, 0.8, brass)
    loft("barrel", grip_x + 57.0, grip_x + 86.0, [(0.0, 0, 0, 1.15, 1.15, 2.0), (1.0, 0, 0, 1.05, 1.05, 2.0)], steel)
    loft("muzzle", grip_x + 85.0, grip_x + 91.0, [(0.0, 0, 0, 1.7, 1.7, 2.2), (1.0, 0, 0, 1.6, 1.6, 2.2)], steel)
    # The teal channels: glowing strips set into both flanks, from the receiver along the shroud.
    for sign in (1.0, -1.0):
        a, b = V(grip_x, y0 + sign * 2.25, z0 + 0.6), V(grip_x + 56.0, y0 + sign * 1.85, z0 + 0.6)
        parts.append(tree.leaf(S, "channel_%d" % (sign > 0), lambda P, a=a, b=b: sdf.capsule(P, a, b, 0.42), Box.around([a, b], 1.0), teal, hold))
    # The large round optic on top: its tube on brass rings, its objective bell wide, a teal lens at each end.
    oz = z0 + 6.6
    scope = sdf.Loft(V(grip_x + 1.0, y0, oz), V(grip_x + 27.0, y0, oz), (0, 0, 1),
                     [(0.0, 0, 0, 2.6, 2.6, 2.0), (0.14, 0, 0, 2.2, 2.2, 2.0), (0.3, 0, 0, 1.9, 1.9, 2.0), (0.62, 0, 0, 1.9, 1.9, 2.0), (0.8, 0, 0, 3.6, 3.6, 2.0),
                      (1.0, 0, 0, 4.1, 4.1, 2.0)], 0.4)
    parts.append(tree.leaf(S, "scope", scope, Box.around(scope.bounds_points()), steel, hold))
    for i, x in enumerate((grip_x + 7.0, grip_x + 18.0)):
        mount = lambda P, c=V(x, y0, z0 + 3.5): sdf.box(P, c, (0.9, 1.5, 3.2), None, 0.4)  # noqa: E731
        parts.append(tree.leaf(S, "scope_mount_%d" % i, mount, Box(V(x - 2, y0 - 2, z0), V(x + 2, y0 + 2, oz + 2)), brass, hold))
    for name, x, r in (("lens_front", grip_x + 27.1, 3.7), ("lens_rear", grip_x + 0.9, 2.2)):
        parts.append(tree.leaf(S, name, lambda P, c=V(x, y0, oz), rr=r: sdf.ellipsoid(P, c, (0.35, rr, rr)), Box(V(x - 1, y0 - r, oz - r), V(x + 1, y0 + r, oz + r)), teal, hold))
    return Union(parts, k=0.25)
