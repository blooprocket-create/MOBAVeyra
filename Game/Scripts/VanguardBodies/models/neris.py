"""Neris, The Tidebound (ADR-069): a rescue rider who drowned and came back still working, read by her silhouette from the
game camera. Character Bible §11 and her splash art: she rides a living wave, and that is the silhouette: a standing
swell of dark water carries her above the surface, its crest rising behind her and curling forward at a lip of white
foam. Long pale blue-white hair streams back from under a dark wide-brimmed hat; a heavy layered coat gone ragged at the
hem; a working harness of chains, rope and salvage gear (a heavy shackle at the chest, a coil of line at the hip, a
metal signal whistle on a chain); a grey wrap at her throat, heavy boots. Her hands are bare and open. No lantern, no
ship's-wheel charm: the wave, the open water and the brimmed hat tell her apart.

Low poly and flat-coloured (author 2026-10-07): the big forms that make her outline, each a flat colour the toon
material shades. She stands in the A pose on the swell; her clips brace her on it and roll its crest. The wave is drawn
as the archetype lays it out (humanoid.wave_body): the swell on the root bone, resting on the ground, and the crest on the
cape bones, which rise behind her and curl forward. It grows with her body's scale (the Wave and Tidebreaker bodies)
while she stays her own size, so it is built of sheets, as cloth is: surfaces at the density the game mesh keeps, the
same few hundred triangles at any size, never sharing a surface with her. Its colours are her kit's water and foam (the
Storm body darkens them). Her coat's skirt and its lining hang on the coat-tail chains, her hair on the hair chains
(ADR-069 §7). Colours are her kit's, adjusted toward her art and lifted so they read under the toon light."""
import numpy as np

from ..sculpt import anatomy, garments, hair, paint, sdf, sheet, tree
from ..sculpt.anatomy import V, unit
from ..sculpt.garments import around, band_z, both, either, folds
from ..sculpt.tree import Box, Over, Placed, Shell, Union, Zone

# The hand, wrist to fingertip, as a share of the height.
HAND_SHARE = 0.105
# The skull her hair grows over, in the head's frame at a 24 cm template's scale, and how far beyond it a lock has
# passed wholly to its hair chain (cm).
SKULL_CENTRE = (-1.2, 0.0, 14.4)
SKULL_RADII = (9.9, 7.5, 8.0)
HAIR_REACH = 4.0
# How far below her soles the swell's surface lies (cm).
SOLE_GAP = 0.6
# How far the wave's colours are lifted toward white (a share), so dark water reads as water under the toon light.
WATER_LIFT = 0.14
# How far the crest stands back from its cape bones' joints, as a share of the wave's reach: room for its lip to curl
# over toward her without touching her coat.
CREST_BACK = 0.12
# The swell's sections, from its tail to its bow: where along it (shares of the wave's reach r, from her feet), its
# height (shares of the base b her soles stand at) and its half width (shares of r). Its tail and bow lie on the ground;
# it rises behind her into the crest and is flat under her feet.
SWELL_AT = (-1.8, -1.35, -0.85, -0.3, 0.15, 0.7, 1.2, 1.5)
SWELL_HEIGHT = (0.0, 0.85, 1.1, 1.0, 1.0, 0.7, 0.32, 0.0)
SWELL_WIDTH = (0.5, 1.05, 1.22, 1.0, 0.82, 0.62, 0.42, 0.22)
# Its cross-section: a superellipse of this power, flat-topped and steep at its foot.
SWELL_POWER = 2.4
# Its top lies lit above this share of its height there; foam breaks over its bow from this far along it (a share of r).
SWELL_LIT = 0.64
SWELL_BOW = 1.0
# The crest's crescent: how far round her it reaches either side of her back (degrees), and how much lower its ends
# stand than its middle (a share of its height).
CREST_ARC = 80.0
CREST_FALL = 0.55
# Where up the crest line (in spans, 0 at its foot by the first cape bone) its face gives way to the foam of its lip.
CREST_FOAM = 2.5

# Her kit's colours as seen (sRGB), lifted and pushed toward her art: a charcoal blue-grey coat over a slate lining, a
# dark brown hat, brown leather boots and straps, iron chains, tan rope.
PALETTE = {
    "skin": (0.86, 0.76, 0.7), "hair": (0.86, 0.91, 0.96), "coat": (0.25, 0.28, 0.32), "mantle": (0.31, 0.32, 0.35),
    "lining": (0.3, 0.38, 0.46), "trousers": (0.24, 0.22, 0.22), "boot": (0.36, 0.26, 0.19), "leather": (0.42, 0.29, 0.19),
    "hat": (0.25, 0.21, 0.19), "band": (0.48, 0.36, 0.24), "rope": (0.66, 0.6, 0.48), "iron": (0.44, 0.44, 0.47),
    "steel": (0.64, 0.66, 0.7), "brass": (0.74, 0.6, 0.32), "wrap": (0.47, 0.49, 0.52),
}


def lifted(colour):
    """A kit colour lifted toward white by WATER_LIFT: dark water stays dark but never reads as a hole."""
    c = np.asarray(colour, dtype=np.float64)[:3]
    return tuple(float(x) for x in WATER_LIFT + (1.0 - WATER_LIFT) * c)


def materials(S, spec):
    """Every material Neris is coloured in, flat (the toon material shades it). The wave's are her kit's water and foam
    (a status body may change them): the water, a lit band toward the lip, and the foam; the light in the water under her
    feet glows in her accent."""
    mats = {name: S.material(name, colour) for name, colour in PALETTE.items()}
    water = np.asarray(lifted(spec["water"]))
    foam = np.asarray(spec["foam"], dtype=np.float64)[:3]
    mats["water"] = S.material("water", tuple(water))
    mats["water_lit"] = S.material("water_lit", tuple(np.clip(water * 1.5 * 0.88 + foam * 0.12, 0.0, 1.0)))
    mats["foam"] = S.material("foam", tuple(foam))
    mats["glow"] = S.material("water_glow", tuple(np.asarray(spec["accent"], dtype=np.float64)[:3]), glow=True)
    return mats


def build(S, L, dims, spec):
    """Neris's sculpt on her layout: (the rider, {"body": the skin alone, under her clothes, "sheets": her coat's skirt and
    its lining, and the wave})."""
    mats = materials(S, spec)
    H = dims["height"]
    base = dims["base"]
    figure = anatomy.Figure(S, L, dims, mats["skin"], {"muscle": 0.3, "chest": 0.85, "breadth": 0.8, "hips": 1.08, "limb": 0.82,
                                                        "deltoid": 0.72, "leg": 0.86, "neck": 0.8, "bust": 0.7}).build()
    # She stands on the wave, its swell under her soles: her legs are the figure's own, their feet set on the swell
    # rather than on the ground.
    for side in ("l", "r"):
        figure.parts.remove(figure.limbs["leg_" + side])
        figure.limbs["leg_" + side] = leg(figure, L, side, base)
        figure.parts.append(figure.limbs["leg_" + side])
    head_origin = V(*L["head"][0])
    size = (L["head"][1][2] - L["head"][0][2]) * 24.0 / 21.9
    u = size / 24.0
    head = anatomy.Head(S, mats["skin"], size, look={"jaw": 0.86}).build()
    figure.attach_head(Placed(Over([head, hair_of(S, L, spec, mats, head_origin, u), hat(S, mats, u)]), head_origin, np.eye(3)))
    for side in ("l", "r"):
        figure.limbs["hand_" + side] = hand(S, L, H, side, mats, figure)
    body = figure.body()
    dressed = clothes(S, L, dims, mats, body, figure.limbs)
    worn = Over([dressed, gear(S, L, dims, mats, dressed)])
    # The wave is sheets (as cloth is): surfaces built at the density the game mesh keeps, so it costs the same however
    # large a body grows it, and the rider's sculpt keeps the rest of the budget.
    sheets = [coat_skirt(S, L, dims, "coat"), coat_skirt(S, L, dims, "lining")] + swell(dims, mats) + crest(L, dims, mats)
    return worn, {"body": body, "sheets": sheets}


def leg(figure, L, side, base):
    """One leg as the figure builds it (anatomy.Figure.leg), its foot set on the swell base cm above the ground."""
    H = figure.H
    g = figure.look.get("leg", 1.0)
    h = lambda cm: H * cm * g / 171.0  # noqa: E731
    hp, k = V(*L["thigh_" + side][0]), V(*L["thigh_" + side][1])
    a = V(*L["calf_" + side][1])
    group = []
    figure.loft("thigh", hp + V(0, 0, h(2.0)), k, (1, 0, 0), [(0.0, 0, 0, h(8.6), h(8.0)), (0.25, h(0.6), 0, h(8.4), h(7.9)),
                                                           (0.6, h(1.0), 0, h(7.4), h(6.9)), (0.9, h(0.4), 0, h(5.5), h(5.3)),
                                                           (1.0, h(0.3), 0, h(5.0), h(4.9))],
                anatomy.along("pelvis", "thigh_" + side, hp + V(0, 0, 8), hp - V(0, 0, 10), 0.2, 0.7), group, cap=h(3.0))
    figure.add("knee", lambda P, c=k: sdf.sphere(P, c, h(4.7)), Box(k - h(6), k + h(6)), anatomy.rigid("calf_" + side), group=group)
    figure.ellipsoid("patella", k + V(h(3.6), 0, h(0.6)), (h(1.8), h(2.6), h(3.0)), None, anatomy.rigid("calf_" + side), group)
    figure.loft("calf", k, a, (1, 0, 0), [(0.0, h(0.3), 0, h(4.9), h(4.8)), (0.2, h(-1.5), 0, h(5.6), h(5.1)), (0.45, h(-1.0), 0, h(4.7), h(4.3)),
                                       (0.8, h(-0.2), 0, h(3.3), h(3.1)), (1.0, 0, 0, h(3.0), h(2.9))], anatomy.rigid("calf_" + side), group, cap=h(2.0))
    f0, f1 = V(*L["foot_" + side][0]), V(*L["foot_" + side][1])
    foot_bones = anatomy.rigid("foot_" + side)
    figure.ellipsoid("ankle", a + V(0, 0, h(0.6)), (h(3.0), h(2.7), h(3.2)), None, foot_bones, group)
    heel = V(f0[0] - h(2.0), f0[1], base + h(2.8))
    toe = V(f1[0] + h(1.7), f1[1], base + h(2.4))
    figure.cone("foot", heel, toe, h(2.9), h(2.5), foot_bones, group)
    return Union(group, k=h(2.4))


def boot(S, name, L, side, top, mats, base, shaft, toe):
    """A heavy boot (garments.boot) on a foot that stands base cm above the ground: built on the leg moved down to it,
    and placed back up."""
    drop = V(0, 0, base)
    moved = {bone: (tuple(V(*L[bone][0]) - drop), tuple(V(*L[bone][1]) - drop)) for bone in ("calf_" + side, "foot_" + side)}
    return Placed(garments.boot(S, name, moved, side, top - base, mats, None, shaft=shaft, toe=toe), drop, np.eye(3))


def hair_of(S, L, spec, mats, head_origin, u):
    """Her long pale hair, in the head's frame: a messy cap under the hat and long locks streaming back and out from
    under its brim past her shoulders, as the wind takes it. It sways on the hair chains where her kit gives them."""
    locks_bones = anatomy.rigid("head")
    chains = sorted(bone[:-3] for bone in L if bone.startswith("hair_") and bone.endswith("_01"))
    if chains:
        joints = {chain: [L["%s_%s" % (chain, joint)][0] for joint in ("01", "02", "end")] for chain in chains}
        locks_bones = hair.on_chains(joints, "head", head_origin + V(*SKULL_CENTRE) * u, V(*SKULL_RADII) * u, HAIR_REACH)
    cap = hair.messy(S, mats["hair"], locks_bones, SKULL_CENTRE, SKULL_RADII, (15.0, 6.0), seed=spec["seed"], unit_scale=u, count=14,
                     length=(5.0, 9.0), radius=(1.8, 2.5), wind=(-0.8, 0.0, 0.0), fringe=0.6, volume=0.15, cap_bones=anatomy.rigid("head"))
    # Long locks from under the hat's brim at the back and sides of her head, streaming back behind her in a broad
    # wavy mass and fanning out, falling a little as they go.
    rng = np.random.default_rng(spec["seed"])
    locks = []
    for i, around_deg in enumerate((-115.0, -132.0, -149.0, -166.0, 180.0, 166.0, 149.0, 132.0, 115.0, -158.0, 158.0)):
        a = np.radians(around_deg)
        out = V(np.cos(a), np.sin(a), 0.0)
        low = i >= 9
        root = V(*SKULL_CENTRE) * u + V(out[0] * 8.6, out[1] * 6.8, (-5.0 if low else -1.0) + rng.uniform(-1.0, 1.0)) * u
        direction = unit(V(-1.0, out[1] * 0.6, (-0.45 if low else -0.18) + rng.uniform(-0.1, 0.1)))
        length = rng.uniform(32.0, 42.0) * u * (0.8 if low else 1.0)
        locks.append(wavy_lock(S, "hair_long_%d" % i, root, direction, length, rng.uniform(3.4, 4.2) * u, rng.uniform(0.15, 0.3), 2.4 * u,
                               rng.uniform(0, 2 * np.pi), mats["hair"], locks_bones))
    return Union([cap] + locks, k=1.2 * u)


def wavy_lock(S, name, root, direction, length, radius, droop, amplitude, phase, material, bones):
    """A long lock of hair from root out along direction: falling droop of its length by its tip, rippling up and down
    by amplitude as the wind takes it, full along most of its length and ending blunt, so many read as one mass."""
    t = np.linspace(0.0, 1.0, 9)
    d = unit(direction)
    points = [V(*root) + d * length * k + V(0, 0, -droop * length * k * k + amplitude * np.sin(k * 2.4 * np.pi + phase) * k) for k in t]
    radii = [radius * r for r in (0.75, 0.95, 1.05, 1.05, 1.0, 0.92, 0.8, 0.62, 0.38)]
    return tree.leaf(S, name, lambda P: sdf.tube(P, points, radii), Box.around(points, radius * 1.3), material, bones)


def hat(S, mats, u):
    """Her dark wide-brimmed hat, in the head's frame: a broad brim turned up at the sides over a rounded crown, a band
    round its base; set low over the brow and tipped back a little, so her face shows from above."""
    bones = anatomy.rigid("head")
    axes = sdf.rotation(pitch=-8.0)
    centre = V(-1.2, 0.0, 15.6) * u
    up = axes[:, 2]
    R, half, curl = 16.5 * u, 0.6 * u, 0.32

    def brim(P):
        Q = sdf.local(P, centre, axes)
        lift = curl * Q[:, 1] ** 2 / R
        d = np.stack([np.linalg.norm(Q[:, :2], axis=1) - R, np.abs(Q[:, 2] - lift) - half], axis=1)
        return np.minimum(np.max(d, axis=1), 0.0) + np.linalg.norm(np.maximum(d, 0.0), axis=1) - 0.25 * u

    crown_shape = sdf.Loft(centre - up * 0.5 * u, centre + up * 12.5 * u, axes[:, 0], [(0.0, 0, 0, 11.4 * u, 9.6 * u), (0.5, -0.2 * u, 0, 10.8 * u, 9.0 * u),
                                                                                    (1.0, -0.8 * u, 0, 8.6 * u, 6.6 * u)], cap=3.0 * u)
    parts = [tree.leaf(S, "hat_brim", brim, Box(centre - V(R, R, R * 0.5) - 2 * u, centre + V(R, R, R * 0.5) + 2 * u), mats["hat"], bones, protect=0.6),
             tree.leaf(S, "hat_crown", crown_shape, Box.around(crown_shape.bounds_points()), mats["hat"], bones, protect=0.6)]
    band_a, band_b = centre + up * 2.2 * u, centre + up * 4.4 * u
    band_shape = sdf.Loft(band_a, band_b, axes[:, 0], [(0.0, -0.05 * u, 0, 11.6 * u, 9.75 * u), (1.0, -0.1 * u, 0, 11.4 * u, 9.55 * u)], cap=0.3 * u)
    parts.append(tree.leaf(S, "hat_band", band_shape, Box.around(band_shape.bounds_points()), mats["band"], bones, protect=0.6))
    return Union(parts, k=0.6 * u)


def hand(S, L, H, side, mats, figure):
    """A bare hand at rest beside her thigh, open, its fingers a little spread: the water answers it."""
    w0, w1 = V(*L["hand_" + side][0]), V(*L["hand_" + side][1])
    sign = 1.0 if side == "l" else -1.0
    x = unit(w1 - w0)
    out = V(0, sign, 0)
    z = unit(out - x * (out @ x))
    y = np.cross(z, x)
    curl = {"index": (8, 12, 6), "middle": (10, 14, 8), "ring": (12, 16, 9), "little": (14, 18, 10)}
    spread = {"index": -9.0, "middle": -2.0, "ring": 5.0, "little": 12.0}
    built = anatomy.Hand(S, mats["skin"], side, H * HAND_SHARE, "hand_" + side, curl=curl, spread=spread, thumb=(30, 12, 12, 10)).build()
    placed = Placed(built, w0, np.stack([x, y, z], axis=1))
    figure.parts.append(placed)
    return placed


def clothes(S, L, dims, mats, body, limbs):
    """Her clothes over her skin, each keeping to its limbs: dark trousers and heavy boots strapped at the shin; the heavy
    coat over the torso and down the arms, its sleeves turned back at the forearm and bound with rope; a short ragged
    shoulder cape over it, the coat's top layer; a belt; and the grey wrap round her throat."""
    H = dims["height"]
    base = dims["base"]
    pz = L["pelvis"][0][2]
    cz = L["spine_03"][1][2]
    torso = cz - pz
    legs = both(band_z(base + H * 0.06, pz + H * 0.04), garments.keep_to(limbs, ["leg_l", "leg_r", "torso"]))
    trousers = Shell(S, "trousers", body, 0.35, 0.7, legs, mats["trousers"], hem=0.3, reach=1.0)
    boot_top = base + (pz - base) * 0.5
    boot_mats = {"boot": mats["boot"], "sole": mats["leather"]}
    boots = [boot(S, "boot_" + side, L, side, boot_top, boot_mats, base, 1.12, 1.05) for side in ("l", "r")]
    lower = Over([body, trousers] + boots)
    straps = []
    for side in ("l", "r"):
        for k, share in enumerate((0.45, 0.8)):
            z = base + (boot_top - base) * share
            region = both(band_z(z - H * 0.008, z + H * 0.008), garments.keep_to(limbs, ["leg_" + side]))
            straps.append(Shell(S, "boot_strap_%s_%d" % (side, k), lower, 0.4, 0.9, region, mats["leather"], hem=0.2))
    lower = Over([lower] + straps)
    # The coat: closed over the torso, down both arms to the forearm, where it turns back in a wide cuff.
    sleeves = []
    for side in ("l", "r"):
        e, w = V(*L["lowerarm_" + side][0]), V(*L["lowerarm_" + side][1])
        sleeves.append(both(garments.keep_to(limbs, ["upperarm_" + side, "forearm_" + side]), garments.half(e + (w - e) * 0.72, w - e, Box.around([e, w], H * 0.2))))
    coat_region = either(both(band_z(pz - H * 0.02, cz + H * 0.012), garments.keep_to(limbs, ["torso"])), *sleeves)
    coat = Shell(S, "coat", lower, 0.7, 1.1, coat_region, mats["coat"], hem=0.4, displace=folds((0, 0, 1), 9, 0.5, seed=211), reach=1.0)
    dressed = Over([lower, coat])
    cuffs = []
    for side in ("l", "r"):
        e, w = V(*L["lowerarm_" + side][0]), V(*L["lowerarm_" + side][1])
        a, b = e + (w - e) * 0.5, e + (w - e) * 0.74
        region = both(around([a, b], [H * 0.06, H * 0.07]), garments.keep_to(limbs, ["forearm_" + side]))
        cuffs.append(Shell(S, "cuff_" + side, dressed, 0.4, 1.8, region, mats["coat"], hem=0.5, displace=folds(w - e, 6, 0.6, seed=213), reach=0.8))
        ra, rb = e + (w - e) * 0.3, e + (w - e) * 0.42
        cuffs.append(Shell(S, "cuff_rope_" + side, dressed, 0.2, 1.0, both(around([ra, rb], [H * 0.05, H * 0.05]), garments.keep_to(limbs, ["forearm_" + side])),
                           mats["rope"], hem=0.3))
    dressed = Over([dressed] + cuffs)
    # The harness's leather straps, crossed over her chest from each shoulder to the other hip.
    shoulder = abs(L["upperarm_l"][0][1])
    hip = abs(L["thigh_l"][0][1])
    straps = []
    for sign in (1.0, -1.0):
        a, b = V(0, sign * (shoulder - 2.5), cz - 1.0), V(0, -sign * hip * 1.2, pz + H * 0.04)
        across = unit(np.cross(b - a, V(1, 0, 0)))
        band = both(garments.band_plane((a + b) / 2, across, H * 0.03, Box((-40, -60, pz), (40, 60, cz + 6))), garments.keep_to(limbs, ["torso"]))
        straps.append(Shell(S, "harness_%d" % (sign > 0), dressed, 0.2, 0.9, band, mats["leather"], hem=0.2, reach=0.5))
    belt_z = pz + torso * 0.12
    belt = Shell(S, "belt", dressed, 0.3, 1.1, both(band_z(belt_z - H * 0.014, belt_z + H * 0.014), garments.keep_to(limbs, ["torso"])), mats["leather"], hem=0.2)
    strapped = Over([dressed, belt] + straps)
    # The shoulder cape: the coat's heavy top layer over her shoulders and upper arms, shorter at the front, its hem
    # torn into ragged tongues.
    tongues = 15

    def cape_region(P):
        angle = np.arctan2(P[:, 1], P[:, 0])
        strip = (angle + np.pi) / (2.0 * np.pi) * tongues
        tip = 1.0 - np.abs(2.0 * (strip % 1.0) - 1.0)
        hem = (cz - H * 0.045 - H * 0.05 * np.abs(np.sin(angle)) - H * 0.04 * np.clip(-np.cos(angle), 0, 1)
               - (H * 0.012 + H * 0.03 * paint.hashed(np.floor(strip), 7.0)) * tip ** 0.7)
        return np.maximum(hem - P[:, 2], P[:, 2] - (cz + H * 0.05))
    region = both(Zone(cape_region, Box((-40, -shoulder - 20, cz - H * 0.2), (40, shoulder + 20, cz + H * 0.06))),
                  garments.keep_to(limbs, ["torso", "upperarm_l", "upperarm_r"]))
    cape = Shell(S, "shoulder_cape", strapped, 0.4, 1.2, region, mats["mantle"], hem=0.6, displace=folds((0, 0, 1), 11, 1.0, seed=217), reach=1.2)
    caped = Over([strapped, cape])
    # The grey wrap round her throat: wound round the back of her neck and swagged down across her breast.

    def at(degrees, radius, rise):
        r = np.radians(degrees)
        return V(radius * np.cos(r), radius * np.sin(r), cz + rise)
    lines = [("wrap_back", [at(-75, 8.0, 2.0), at(-130, 7.5, 2.6), at(180, 7.0, 2.8), at(130, 7.5, 2.6), at(75, 8.0, 2.0)], 4.0, 2.2),
             ("wrap_front", [at(70, 8.5, 1.5), at(35, 9.5, -1.0), at(0, 10.0, -3.0), at(-35, 9.5, -1.5), at(-70, 8.5, 1.0)], 4.2, 2.4),
             ("wrap_tail", [at(25, 10.0, -2.5), at(18, 11.5, -7.0), at(14, 12.5, -12.0)], 3.4, 1.8)]
    wraps = [garments.fold(S, name, caped, garments.laid_on(caped, points), width, height, mats["wrap"], seed=220 + i)
             for i, (name, points, width, height) in enumerate(lines)]
    return Over([caped] + wraps)


# ---------------------------------------------------------------------------------------------- gear
def chain_along(S, name, points, link, bones, mats):
    """Heavy chain along points: iron links of length link, each turned a quarter about the chain from the last."""
    pts = [V(*p) for p in points]
    centres, frames = [], []
    for a, b in zip(pts, pts[1:]):
        count = max(1, int(round(np.linalg.norm(b - a) / (link * 0.78))))
        axis = unit(b - a)
        side = unit(np.cross(axis, V(0, 0, 1)) if abs(axis[2]) < 0.9 else np.cross(axis, V(1, 0, 0)))
        for i in range(count):
            turn = side if (len(centres) % 2 == 0) else unit(np.cross(axis, side))
            centres.append(a + (b - a) * ((i + 0.5) / count))
            # A link's ring lies in the plane of its axis and turn: its own z (the ring's normal) is square to both.
            frames.append(np.stack([axis, turn, unit(np.cross(axis, turn))], axis=1))
    major, minor = link * 0.36, link * 0.16

    def distance(P):
        d = None
        for c, f in zip(centres, frames):
            Q = sdf.local(P, c, f)
            # An oval link: a ring stretched along its axis.
            q = np.stack([np.maximum(np.abs(Q[:, 0]) - major * 0.45, 0.0), Q[:, 1]], axis=1)
            ring = np.sqrt((np.linalg.norm(q, axis=1) - major * 0.62) ** 2 + Q[:, 2] ** 2) - minor
            d = ring if d is None else np.minimum(d, ring)
        return d
    return tree.leaf(S, name, distance, Box.around(centres, link), mats["iron"], bones, protect=0.5)


def gear(S, L, dims, mats, dressed):
    """The rescuer's working gear over the coat: a chain from her right shoulder across her breast to her left hip, the
    heavy iron shackle hung at her chest where the straps cross, the metal signal whistle hung from its bow on a short
    chain, the belt's brass buckle, the coil of line strapped at her left hip, and a pouch at her right hip. All of it on
    her chest's or her hips' bones, clear of her thighs as they brace."""
    H = dims["height"]
    pz = L["pelvis"][0][2]
    cz = L["spine_03"][1][2]
    torso = cz - pz
    belt_z = pz + torso * 0.12
    shoulder = abs(L["upperarm_l"][0][1])
    hip = abs(L["thigh_l"][0][1])
    chest_bones, hip_bones = anatomy.rigid("spine_03"), anatomy.rigid("pelvis")
    parts = []
    # The chest chain: laid on the coat from the right shoulder, across the breast, to the left of the belt.
    starts = np.array([[30.0, -shoulder * 0.75, cz - H * 0.02], [30.0, -shoulder * 0.35, cz - torso * 0.22], [30.0, 0.0, cz - torso * 0.42],
                       [30.0, hip * 0.7, pz + torso * 0.3], [30.0, hip * 1.2, belt_z + H * 0.01]])
    hits, normals = garments.surface_points(dressed, starts, np.tile([-1.0, 0.0, 0.0], (len(starts), 1)), reach=40.0)
    path = hits + normals * H * 0.012
    parts.append(chain_along(S, "chest_chain", path, H * 0.03, anatomy.along("spine_03", "pelvis", path[0], path[-1], 0.4, 0.9), mats))
    # The shackle: a heavy iron bow hung where the straps cross, its pin across the top.
    cross = np.array([[30.0, hip * 0.25, cz - torso * 0.36]])
    hit, normal = garments.surface_points(dressed, cross, np.array([[-1.0, 0.0, 0.0]]), reach=40.0)
    pin = hit[0] + normal[0] * H * 0.02
    bow_r, bar = H * 0.026, H * 0.0075
    bow_c = pin - V(0, 0, H * 0.04)

    def shackle(P):
        # The bow: a half ring below its centre, in the plane facing forward, its two legs running straight up to the pin.
        Q = P - bow_c
        bend = np.sqrt(Q[:, 1] ** 2 + np.minimum(Q[:, 2], 0.0) ** 2) - bow_r
        return np.sqrt(bend ** 2 + Q[:, 0] ** 2 + np.maximum(Q[:, 2] - H * 0.04, 0.0) ** 2) - bar
    parts.append(tree.leaf(S, "shackle", shackle, Box(bow_c - V(H * 0.02, bow_r + bar * 2, bow_r + bar * 2), pin + V(H * 0.02, bow_r + bar * 2, bar * 2)),
                           mats["iron"], chest_bones, protect=0.8))
    parts.append(tree.leaf(S, "shackle_pin", lambda P: sdf.cylinder(P, pin + V(0, -bow_r - bar * 2.2, 0), pin + V(0, bow_r + bar * 2.2, 0), bar * 1.1, bar * 0.3),
                           Box(pin - V(bar * 2, bow_r + bar * 3, bar * 2), pin + V(bar * 2, bow_r + bar * 3, bar * 2)), mats["iron"], chest_bones, protect=0.8))
    # The belt's buckle.
    hit, normal = garments.surface_points(dressed, np.array([[30.0, 0.0, belt_z]]), np.array([[-1.0, 0.0, 0.0]]), reach=40.0)
    buckle = hit[0] + V(H * 0.004, 0, 0)
    parts.append(tree.leaf(S, "buckle", lambda P: sdf.box(P, buckle, (H * 0.006, H * 0.022, H * 0.018), None, H * 0.003), Box(buckle - H * 0.03, buckle + H * 0.03),
                           mats["brass"], hip_bones, protect=0.6))
    # The signal whistle: a steel tube hung on a short chain from the shackle's bow.
    hang = bow_c - V(0, 0, bow_r + bar)
    top = hang - V(H * 0.004, 0, H * 0.03)
    parts.append(chain_along(S, "whistle_chain", [hang, top], H * 0.018, chest_bones, mats))
    tube_a, tube_b = top - V(0, 0, H * 0.006), top - V(H * 0.004, 0, H * 0.062)
    parts.append(tree.leaf(S, "whistle", lambda P: sdf.cylinder(P, tube_a, tube_b, H * 0.01, H * 0.003), Box.around([tube_a, tube_b], H * 0.02),
                           mats["steel"], chest_bones, protect=0.8))
    # The coil of line at her left hip: turns of rope hung flat against it, outside the coat's skirt.
    angle = np.radians(125.0)
    out = V(np.cos(angle), np.sin(angle), 0.0)
    c = V(0, 0, belt_z - H * 0.08) + out * (hip * 2.6)
    axes = np.stack([unit(np.cross(V(0, 0, 1), out)), V(0, 0, 1), out], axis=1)
    for k in range(3):
        ck = c + out * (k * H * 0.011) + V(H * 0.004 * k, 0, -k * H * 0.007)
        parts.append(tree.leaf(S, "rope_coil_%d" % k, lambda P, ck=ck: sdf.torus(P, ck, H * 0.045, H * 0.011, axes), Box(ck - H * 0.06, ck + H * 0.06),
                               mats["rope"], hip_bones, protect=0.4))
    # Its strap, from the belt down to the coil it holds.
    hit, _normal = garments.surface_points(dressed, (V(0, 0, belt_z) + out * 40.0)[None, :], -out[None, :], reach=40.0)
    strap_a, strap_b = hit[0] - out * H * 0.004, c + out * H * 0.006 + V(0, 0, H * 0.04)
    parts.append(tree.leaf(S, "coil_strap", lambda P: sdf.capsule(P, strap_a, strap_b, H * 0.008), Box.around([strap_a, strap_b], H * 0.015),
                           mats["leather"], hip_bones, protect=0.4))
    # A pouch on the belt at her right hip, over the coat's skirt.
    angle = np.radians(-70.0)
    out = V(np.cos(angle), np.sin(angle), 0.0)
    pc = V(0, 0, belt_z - H * 0.03) + out * (hip * 2.45)
    axes = np.stack([unit(np.cross(V(0, 0, 1), out)), V(0, 0, 1), out], axis=1)
    parts.append(garments.pouch(S, "pouch", pc, axes, (H * 0.05, H * 0.055, H * 0.028), H * 0.018, mats["leather"], bones=hip_bones))
    return Union(parts, k=0.2)


# ---------------------------------------------------------------------------------------------- the wave
def monotone(xs, ys):
    """A smooth curve through the points (xs, ys) that never overshoots them (a monotone cubic): f(x) -> its values."""
    xs, ys = np.asarray(xs, dtype=np.float64), np.asarray(ys, dtype=np.float64)
    h = np.diff(xs)
    d = np.diff(ys) / h
    m = np.zeros(len(xs))
    m[0], m[-1] = d[0], d[-1]
    for k in range(1, len(xs) - 1):
        if d[k - 1] * d[k] > 0:
            w1, w2 = 2 * h[k] + h[k - 1], h[k] + 2 * h[k - 1]
            m[k] = (w1 + w2) / (w1 / d[k - 1] + w2 / d[k])

    def f(x):
        x = np.clip(np.asarray(x, dtype=np.float64), xs[0], xs[-1])
        k = np.clip(np.searchsorted(xs, x, side="right") - 1, 0, len(xs) - 2)
        t = (x - xs[k]) / h[k]
        t2, t3 = t * t, t * t * t
        return ((2 * t3 - 3 * t2 + 1) * ys[k] + (t3 - 2 * t2 + t) * h[k] * m[k] + (-2 * t3 + 3 * t2) * ys[k + 1]
                + (t3 - t2) * h[k] * m[k + 1])
    return f


def rigid_sheet(bone):
    """A sheet's skinning wholly to one bone."""
    return lambda P, u, v: {bone: np.ones(len(P), dtype=np.float32)}


def swell(dims, mats):
    """The swell she stands on (humanoid.wave_body), on the root bone: a long hump of water resting on the ground, its
    tail and bow on the ground, rising behind her where the crest climbs from it and flat under her feet, a hand's
    breadth below her soles; narrow at the bow, broad behind. Its top lies lit above a line rolling round it, its flanks
    the deep water's colour, and white foam breaks over its bow, sweeping back along both flanks; light shows in the water
    at her feet. Its sizes are the archetype's (waveReach r, base b), so a larger body's swell is deeper and broader.
    Sheets, so it costs the same at every size: its flanks, its top and its bow, sharing their edges."""
    b, r = dims["base"], dims["waveReach"]
    xs = [a * r for a in SWELL_AT]
    height = monotone(xs, [a * (b - SOLE_GAP) for a in SWELL_HEIGHT])
    width = monotone(xs, [a * r for a in SWELL_WIDTH])
    p = SWELL_POWER
    back, front, bow = xs[0], xs[-1], SWELL_BOW * r

    def surface(x, phi):
        """The swell's surface at x along it and phi round its section (-pi/2 its right foot, pi/2 its left)."""
        s, c = np.sin(phi), np.cos(phi)
        return np.stack([x, width(x) * np.sign(s) * np.abs(s) ** (2.0 / p), height(x) * np.abs(c) ** (2.0 / p)], axis=1)

    def lit_edge(u):
        """How far round its section the lit top reaches (radians), a column u along it: where its height falls to the
        lit share, the line rolling as it runs."""
        x = back + u * (bow - back)
        share = SWELL_LIT + 0.035 * np.sin(x / (0.25 * r))
        return np.arccos(share ** (p / 2.0))

    def bow_edge(phi):
        """Where the bow's foam begins along it, round its section: scalloped, and sweeping back down both flanks."""
        return bow + 0.05 * r * np.cos(7.0 * phi) - 0.24 * r * (phi / (np.pi / 2.0)) ** 2

    def band(lo, hi):
        """The part of the swell behind the bow's foam between lo(u) and hi(u) round its section."""
        def position(u, v):
            phi = lo(u) + (hi(u) - lo(u)) * v
            return surface(back + u * (bow_edge(phi) - back), phi)
        return position
    half = np.pi / 2.0
    columns, flank_rows, top_rows = 18, 4, 6
    root = rigid_sheet("root")
    sheets = [sheet.Sheet("swell_right", band(lambda u: np.full_like(u, -half), lambda u: -lit_edge(u)), mats["water"], root, columns, flank_rows),
              sheet.Sheet("swell_top", band(lambda u: -lit_edge(u), lit_edge), mats["water_lit"], root, columns, top_rows),
              sheet.Sheet("swell_left", band(lit_edge, lambda u: np.full_like(u, half)), mats["water"], root, columns, flank_rows)]
    # The bow: round its whole section, its edge on the others' front edges, vertex for vertex.
    edge = float(lit_edge(np.array([1.0]))[0])
    rows = 2 * flank_rows + top_rows
    knots = np.array([0.0, flank_rows, flank_rows + top_rows, rows]) / rows

    def bow_position(u, v):
        phi = np.interp(v, knots, [-half, -edge, edge, half])
        start = bow_edge(phi)
        return surface(start + u * (front - start), phi)
    sheets.append(sheet.Sheet("swell_bow", bow_position, mats["foam"], root, 4, rows))
    # The light in the water at her feet: a patch laid on the swell's top just over it.

    def glow_position(u, v):
        a, c = 2.0 * u - 1.0, 2.0 * v - 1.0
        x = 0.08 * r + 0.42 * r * a * np.sqrt(1.0 - c * c / 2.0)
        y = 0.36 * r * c * np.sqrt(1.0 - a * a / 2.0)
        t = np.clip(y / width(x), -1.0, 1.0)
        phi = np.sign(t) * np.arcsin(np.abs(t) ** (p / 2.0))
        point = surface(x, phi)
        # Clear of the swell's own facets beneath it, which grow with it.
        point[:, 2] += 0.25 + 0.015 * r
        return point
    sheets.append(sheet.Sheet("water_light", glow_position, mats["glow"], root, 5, 5))
    return sheets


def crest_line(L, dims, samples=64):
    """The crest's spine in its own half plane (x back from her, y = 0): from deep in the swell up the cape bones' joints
    (the archetype lays them up the crest), set back a little from them so its lip has room to curl, and on past the last,
    where the lip rolls forward and hooks down. (points (n, 3), each one's span: 0 at the first cape bone's head, a span
    a joint)."""
    r, u = dims["waveReach"], dims["waveUnit"]
    foot = V(*L["cape_01"][0])
    joints = [foot - V(0, 0, 0.35 * u), foot] + [V(*L[bone][0]) - V(CREST_BACK * r, 0, 0) for bone in ("cape_02", "cape_03")] + \
             [V(*L["cape_03"][1]) - V(CREST_BACK * r, 0, 0)]
    roll = joints[-1] + V(0.16 * u, 0.0, 0.01 * u)
    hook = roll + V(0.05 * u, 0.0, -0.2 * u)
    controls = joints + [roll, hook]
    points = garments.curve(controls, samples)
    return points, np.linspace(-1.0, len(controls) - 2.0, samples)


def crest(L, dims, mats):
    """The crest on the cape bones: a wall of water swept round behind her in a crescent (CREST_ARC either side of her
    back), tallest at her back and lower toward its ends, which close; thick where it rises from the swell, thin up its
    face, its lip rolling forward over her in white foam and hooking down. Its back is the deep water's colour and its
    face, turned to her, lit. Each point follows the cape bone of the span of the crest line it lies on, so the crest
    rolls as she rides. Sheets round its section (its back, its foamed lip and its face), sharing their edges."""
    u_ = dims["waveUnit"]
    points, spans = crest_line(L, dims)
    lo, hi = spans[0], spans[-1]
    tangents = np.gradient(points, axis=0)
    # Each point's normal in its half plane, toward the crest's face (its concave side, turned to her).
    normals = np.stack([tangents[:, 2], np.zeros(len(points)), -tangents[:, 0]], axis=1)
    normals /= np.maximum(np.linalg.norm(normals, axis=1, keepdims=True), 1e-9)
    radius = lambda s: np.interp(s, [-1.0, 0.0, 1.0, 2.0, 2.6, 3.3, 4.0, 5.0], [0.3, 0.27, 0.16, 0.09, 0.085, 0.14, 0.11, 0.0]) * u_  # noqa: E731
    arc = np.radians(CREST_ARC)
    floor = points[0][2]

    def at(s, side, off):
        """The crest's surface at span s, on its back (side -1) or its face (side 1), off round from her back."""
        spine = np.stack([np.interp(s, spans, points[:, k]) for k in range(3)], axis=1)
        normal = np.stack([np.interp(s, spans, normals[:, k]) for k in range(3)], axis=1)
        taper = np.sqrt(np.clip(1.0 - (off / arc) ** 6, 0.0, 1.0))
        q = spine + normal * (side * radius(s) * taper)[:, None]
        z = floor + (q[:, 2] - floor) * (1.0 - CREST_FALL * (off / arc) ** 2)
        out = -q[:, 0]
        return np.stack([-out * np.cos(off), out * np.sin(off), z], axis=1)

    def loop(lam):
        """Round the crest's section: from its foot up its back (0 to 1) to the tip of its lip, and down its face (1 to 2):
        (span, side)."""
        up = lam <= 1.0
        return np.where(up, lo + lam * (hi - lo), hi - (lam - 1.0) * (hi - lo)), np.where(up, -1.0, 1.0)

    def foam_edge(u):
        """Where round the section the lip's foam begins, a column u across the crescent: scalloped as the lip breaks."""
        off = arc * (2.0 * u - 1.0)
        return (CREST_FOAM + 0.22 * np.cos(11.0 * off) - lo) / (hi - lo)

    def part(start, end):
        def lam(u, v):
            return start(u) + (end(u) - start(u)) * v

        def position(u, v):
            s, side = loop(lam(u, v))
            return at(s, side, arc * (2.0 * u - 1.0))

        def bones(P, u, v):
            s, _side = loop(lam(u, v))
            x = np.clip(s, 0.5, 2.5)
            return {"cape_%02d" % (k + 1): np.clip(1.0 - np.abs(x - (k + 0.5)), 0.0, 1.0).astype(np.float32) for k in range(3)}
        return position, bones
    columns, rows = 18, 6
    sheets = []
    for name, start, end, material in (("crest_back", lambda u: np.zeros_like(u), foam_edge, mats["water"]),
                                        ("crest_lip", foam_edge, lambda u: 2.0 - foam_edge(u), mats["foam"]),
                                        ("crest_face", lambda u: 2.0 - foam_edge(u), lambda u: np.full_like(u, 2.0), mats["water_lit"])):
        position, bones = part(start, end)
        sheets.append(sheet.Sheet(name, position, material, bones, columns, rows))
    return sheets


# ---------------------------------------------------------------------------------------------- cloth sheets
def coat_skirt(S, L, dims, layer):
    """Her coat's long skirt, or its slate lining just inside it: from the belt to the shins, open at the front, flaring
    as it falls and longest behind, its hem torn ragged (the coat more than its lining, so the lining shows through the
    rents). Each half hangs on its own coat-tail chain (ADR-069 §7)."""
    H = dims["height"]
    base = dims["base"]
    pz = L["pelvis"][0][2]
    belt_z = pz + (L["spine_03"][1][2] - pz) * 0.12
    lining = layer == "lining"
    material = S.material("skirt_" + layer, PALETTE["lining" if lining else "coat"])
    columns, rows, strips = (16, 5, 8) if lining else (22, 6, 11)
    t0, t1 = np.radians(32.0), np.radians(328.0)
    hip = abs(L["thigh_l"][0][1])
    inset = -0.8 if lining else 0.0
    limbs = [(L["thigh_" + s][0], L["thigh_" + s][1], H * 0.055) for s in ("l", "r")] + [(L["calf_" + s][0], L["calf_" + s][1], H * 0.05) for s in ("l", "r")]
    phase = np.random.default_rng(223).uniform(0, 2 * np.pi, 2)

    def position(u, v):
        theta = t0 + (t1 - t0) * u
        back = np.clip(-np.cos(theta), 0.0, 1.0)
        radial = np.stack([np.cos(theta), np.sin(theta), np.zeros_like(theta)], axis=1)
        top = np.stack([hip * 1.9 * np.cos(theta), hip * 2.15 * np.sin(theta), np.full_like(theta, belt_z)], axis=1)
        flare = H * (0.07 + 0.05 * back) * v ** 1.2
        pleat = (0.3 + H * 0.012 * v) * (0.6 * np.sin(theta * 8.0 + phase[0]) + 0.4 * np.sin(theta * 13.0 + phase[1]))
        p = top + radial * (flare + inset + pleat)[:, None]
        hem = base + H * (0.13 - 0.06 * back) + (H * 0.03 if lining else 0.0)
        p[:, 2] = belt_z + (hem - belt_z) * v
        return sheet.clear_of(p, limbs, 1.5 + (inset * 0.5 if lining else 0.0))

    def bones(P, u, v):
        left = P[:, 1] > 0
        hold = np.clip(1 - v / 0.15, 0, 1)
        lower = np.clip((v - 0.25) / 0.5, 0, 1) * (1 - hold)
        upper = (1 - hold) - lower
        w = {"pelvis": hold}
        for side, mask in (("l", left), ("r", ~left)):
            w["coat_%s_01" % side] = upper * mask
            w["coat_%s_02" % side] = lower * mask
        return {k: np.asarray(x, dtype=np.float32) for k, x in w.items()}
    cut, point = (0.8, 0.08) if lining else (0.42, 0.22)
    return sheet.Sheet("skirt_" + layer, position, material, bones, columns, rows,
                       reach=lambda u: sheet.torn(u, strips, cut, point, 37.0 if lining else 41.0))
