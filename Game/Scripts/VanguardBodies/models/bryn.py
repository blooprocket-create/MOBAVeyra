"""Bryn (ADR-069): a veteran harbour gunner, read by her silhouette from the game camera. Character Bible and her
splash art: compact and weathered, short salt-and-pepper hair; a worn navy naval coat to the knee, trimmed in gold, open over a
dark shirt, belted, its long skirt flaring; a red sash and a coil of rope at her waist among the buckles, ammunition
pouches and a bandolier; gloves and heavy boots. Mournwake, her giant brass-and-iron cannon, is carried on both sides
of her: the banded barrel runs forward under her right arm to a wide muzzle with Flux light standing in the bore, and the
breech and recoil assembly rides up over her left shoulder, blue-lit Flux chambers along its flanks. One heavy piece,
never a rotating barrel cluster.

Low poly and flat-coloured (author 2026-10-07): the big forms that make her outline, each a flat colour the toon
material shades. She stands as her braced stance lays her out (ADR-064): the cannon rides her upper body, her hands on
it. Her coat's skirt and her hair hang on spring chains (ADR-069 §7). Colours are her kit's, lifted so they read under
the toon light; her splash art is lit for night."""
import numpy as np

from ..sculpt import anatomy, garments, hair, sdf, sheet, tree
from ..sculpt.anatomy import V, unit
from ..sculpt.garments import around, band_z, both, either, folds, without
from ..sculpt.tree import Box, Over, Placed, Shell, Union, Zone

# The hand, wrist to fingertip, as a share of the height.
HAND_SHARE = 0.105
# The skull her hair grows over, in the head's frame at a 24 cm template's scale, and how far beyond it a lock has
# passed wholly to its hair chain (cm).
SKULL_CENTRE = (-1.2, 0.0, 14.4)
SKULL_RADII = (9.9, 7.5, 8.0)
HAIR_REACH = 3.5

PALETTE = {
    "skin": (0.76, 0.6, 0.5), "hair": (0.74, 0.74, 0.72), "coat": (0.2, 0.25, 0.42), "shirt": (0.22, 0.21, 0.22),
    "leather": (0.38, 0.27, 0.19), "boot": (0.3, 0.21, 0.15), "trousers": (0.27, 0.23, 0.21), "glove": (0.28, 0.2, 0.15),
    "sash": (0.66, 0.12, 0.12), "rope": (0.7, 0.6, 0.42), "iron": (0.28, 0.27, 0.29), "brass": (0.78, 0.6, 0.3),
    "flux": (0.4, 0.72, 1.0), "trim": (0.8, 0.64, 0.3),
}


def materials(S):
    """Every material Bryn is coloured in, flat (the toon material shades it); the cannon's Flux glows."""
    return {name: S.material(name, colour, glow=name == "flux") for name, colour in PALETTE.items()}


def build(S, L, dims, spec):
    """Bryn's sculpt on her layout: (the whole, {"body": the skin alone, under her clothes, "sheets": her coat's skirt})."""
    mats = materials(S)
    H = dims["height"]
    figure = anatomy.Figure(S, L, dims, mats["skin"], {"muscle": 0.6, "chest": 0.95, "breadth": 0.95, "hips": 1.05, "limb": 0.95,
                                                        "deltoid": 0.9, "leg": 0.95, "neck": 0.92, "bust": 0.7}).build()
    head_origin = V(*L["head"][0])
    size = (L["head"][1][2] - L["head"][0][2]) * 24.0 / 21.9
    u = size / 24.0
    locks_bones = anatomy.rigid("head")
    chains = sorted(bone[:-3] for bone in L if bone.startswith("hair_") and bone.endswith("_01"))
    if chains:
        joints = {chain: [L["%s_%s" % (chain, joint)][0] for joint in ("01", "02", "end")] for chain in chains}
        locks_bones = hair.on_chains(joints, "head", head_origin + V(*SKULL_CENTRE) * u, V(*SKULL_RADII) * u, HAIR_REACH)
    # Short, swept back and to her left, salt-and-pepper.
    locks = hair.messy(S, mats["hair"], locks_bones, SKULL_CENTRE, SKULL_RADII, (17.0, 7.5), seed=spec["seed"], unit_scale=u, count=24,
                       length=(4.0, 7.0), radius=(1.5, 2.2), wind=(-0.35, 0.2, 0.0), fringe=0.45, volume=0.3, cap_bones=anatomy.rigid("head"))
    figure.attach_head(Placed(Over([anatomy.Head(S, mats["skin"], size, look={"jaw": 1.0}).build(), locks]), head_origin, np.eye(3)))
    breech, muzzle = V(*dims["gun"][0]), V(*dims["gun"][1])
    for side in ("l", "r"):
        figure.limbs["hand_" + side] = hand(S, L, H, side, mats, figure, breech, muzzle)
    body = figure.body()
    dressed = clothes(S, L, dims, mats, body, figure.limbs)
    worn = Over([dressed, cannon(S, mats, H, breech, muzzle)])
    return worn, {"body": body, "sheets": [coat_skirt(S, L, dims, mats)]}


def hand(S, L, H, side, mats, figure, breech, muzzle):
    """A gloved hand closed round the cannon where her stance grips it: its palm toward the barrel's axis."""
    w0, w1 = V(*L["hand_" + side][0]), V(*L["hand_" + side][1])
    x = unit(w1 - w0)
    axis = unit(muzzle - breech)
    centre = w1
    nearest = breech + axis * ((centre - breech) @ axis)
    away = centre - nearest
    z = unit(away - x * (away @ x)) if np.linalg.norm(away - x * (away @ x)) > 1e-3 else unit(np.cross(x, axis))
    y = np.cross(z, x)
    built = anatomy.Hand(S, mats["skin"], side, H * HAND_SHARE, "hand_" + side,
                         curl={"index": (65, 90, 50), "middle": (70, 95, 50), "ring": (72, 95, 50), "little": (75, 95, 45)},
                         thumb=(60, 30, 30, 25)).build()
    glove = Shell(S, "glove_" + side, built, 0.08, 0.35, Zone(lambda P: -np.ones(len(P)), Box((-30, -30, -30), (30, 30, 30))), mats["glove"], hem=0.1, reach=0.5)
    placed = Placed(Over([built, glove]), w0, np.stack([x, y, z], axis=1))
    figure.parts.append(placed)
    return placed


def clothes(S, L, dims, mats, body, limbs):
    """Her clothes over her skin, each keeping to its limbs: trousers and heavy boots; a dark shirt; the long navy coat
    over it, open at the front, high-collared, its sleeves to the wrist with turned-back cuffs; belts, a red sash and a
    coil of rope at her waist, pouches on the belt, a bandolier across her chest."""
    H = dims["height"]
    pz = L["pelvis"][0][2]
    cz = L["spine_03"][1][2]
    torso = cz - pz
    legs = both(band_z(H * 0.04, pz + H * 0.04), garments.keep_to(limbs, ["leg_l", "leg_r", "torso"]))
    trousers = Shell(S, "trousers", body, 0.35, 0.7, legs, mats["trousers"], hem=0.3, reach=1.0)
    boot_mats = {"boot": mats["boot"], "sole": mats["leather"]}
    boots = [garments.boot(S, "boot_" + side, L, side, H * 0.29, boot_mats, None, shaft=1.15, toe=1.1) for side in ("l", "r")]
    lower = Over([body, trousers] + boots)
    shirt = Shell(S, "shirt", lower, 0.25, 0.5, both(band_z(pz, cz + H * 0.01), garments.keep_to(limbs, ["torso"])), mats["shirt"], hem=0.3)
    # The coat: open in a narrow V at the front, over the shoulders and down both arms to the wrist.
    v_cut = Zone(lambda P: np.maximum(np.maximum(3.0 - P[:, 0], np.abs(P[:, 1]) - np.clip((P[:, 2] - (pz + torso * 0.1)) * 0.22, 1.5, 9.0)),
                                      (pz - H * 0.02) - P[:, 2]), Box((0, -14, pz - H * 0.05), (40, 14, cz + 12)))
    coat_region = without(either(both(band_z(pz - H * 0.02, cz + H * 0.01), garments.keep_to(limbs, ["torso"])),
                                 garments.keep_to(limbs, ["upperarm_l", "upperarm_r", "forearm_l", "forearm_r"])), v_cut)
    coat = Shell(S, "coat", Over([lower, shirt]), 0.7, 1.1, coat_region, mats["coat"], hem=0.4, displace=folds((0, 0, 1), 9, 0.5, seed=191), reach=1.0)
    cuffs = []
    for side in ("l", "r"):
        e, w = V(*L["lowerarm_" + side][0]), V(*L["lowerarm_" + side][1])
        region = both(around([w, w + (e - w) * 0.3], [6.5, 7.0]), garments.keep_to(limbs, ["forearm_" + side]))
        cuffs.append(Shell(S, "cuff_" + side, Over([lower, coat]), 0.1, 1.0, region, mats["trim"], hem=0.3, reach=0.5))
    # The high collar, standing open at the throat.
    collar = Zone(lambda P: np.maximum(np.maximum(cz - 2.0 - P[:, 2], P[:, 2] - (cz + H * 0.055)), np.cos(np.arctan2(P[:, 1], P[:, 0] - 1.0)) - 0.55),
                  Box((-16, -16, cz - 4), (16, 16, cz + 14)))
    ring = tree.leaf(S, "collar_core", lambda P: sdf.round_cone(P, V(0.5, 0, cz - 3.0), V(-0.8, 0, cz + H * 0.055), 8.0, 10.0),
                     Box((-16, -16, cz - 6), (16, 16, cz + 14)), mats["coat"], None)
    inner = tree.leaf(S, "collar_inner", lambda P: sdf.round_cone(P, V(0.5, 0, cz - 4.0), V(-0.8, 0, cz + H * 0.06), 7.0, 9.0),
                      Box((-16, -16, cz - 6), (16, 16, cz + 14)), mats["coat"], None)
    collar_band = tree.Intersect(tree.Subtract(ring, inner), collar, 0.3)
    dressed = Over([lower, shirt, coat, collar_band] + cuffs)
    # The belt, the red sash under it, a bandolier from her right shoulder to her left hip.
    belt_z = pz + torso * 0.14
    sash = Shell(S, "sash", dressed, 0.3, 1.2, both(band_z(belt_z - H * 0.02, belt_z + H * 0.022), garments.keep_to(limbs, ["torso"])), mats["sash"],
                 hem=0.4, displace=folds((0, 0, 1), 8, 0.6, seed=193), reach=0.8)
    sashed = Over([dressed, sash])
    belt = Shell(S, "belt", sashed, 0.2, 0.9, both(band_z(belt_z - H * 0.008, belt_z + H * 0.008), garments.keep_to(limbs, ["torso"])), mats["leather"], hem=0.2)
    shoulder = abs(L["upperarm_l"][0][1])
    a, b = V(0, -shoulder + 3.0, cz - 1.0), V(0, 14.0, pz + H * 0.05)
    across = unit(np.cross(b - a, V(1, 0, 0)))
    bandolier = Shell(S, "bandolier", sashed, 0.2, 0.9, both(garments.band_plane((a + b) / 2, across, 5.0, Box((-30, -40, pz), (30, 40, cz + 4))),
                                                           garments.keep_to(limbs, ["torso"])), mats["leather"], hem=0.2, reach=0.5)
    gear = []
    hip = abs(L["thigh_l"][0][1])
    for i, degrees in enumerate((25.0, -20.0, -60.0)):
        angle = np.radians(degrees)
        out = V(np.cos(angle), np.sin(angle), 0.0)
        c = V(0, 0, belt_z - H * 0.03) + out * (hip * 1.8)
        axes = np.stack([unit(np.cross(V(0, 0, 1), out)), V(0, 0, 1), out], axis=1)
        gear.append(garments.pouch(S, "pouch_%d" % i, c, axes, (H * 0.04, H * 0.045, H * 0.022), H * 0.015, mats["leather"], bones=anatomy.rigid("pelvis")))
    # The rope coil hung at her left hip.
    angle = np.radians(70.0)
    out = V(np.cos(angle), np.sin(angle), 0.0)
    c = V(0, 0, belt_z - H * 0.06) + out * (hip * 2.0)
    axes = np.stack([unit(np.cross(V(0, 0, 1), out)), V(0, 0, 1), out], axis=1)
    gear.append(tree.leaf(S, "rope_coil", lambda P: sdf.torus(P, c, H * 0.035, H * 0.012, axes), Box(c - H * 0.06, c + H * 0.06), mats["rope"], anatomy.rigid("pelvis")))
    return Over([sashed, belt, bandolier, Union(gear, k=0.3)])


def cannon(S, mats, H, breech, muzzle):
    """Mournwake: an iron barrel in brass bands from the breech riding up over her left shoulder to a wide brass muzzle
    under her right arm, Flux light standing in its bore and in a chamber along each flank, collared in brass; the
    breech block, its recoil assembly and housing at the back, and a stabilizing brace along each upper flank from the
    breech (canon names the braces among what makes it recognizable). Giant, as her art draws it: it runs on past where her stance holds it, behind her
    shoulder and out ahead of her. It rides her upper body (her chest's bone), which carries it with her hands on it."""
    bones = anatomy.rigid("spine_03")
    along = unit(muzzle - breech)
    side = unit(np.cross(along, V(0, 0, 1)))
    up = unit(np.cross(side, along))
    r = H * 0.062
    # Its whole length: the stance's breech and muzzle, run on behind and ahead.
    back, front = breech - along * H * 0.12, muzzle + along * H * 0.16
    point = lambda share: back + (front - back) * share  # noqa: E731
    parts = []

    def piece(name, a, b, radius, material, rounding=0.0):
        parts.append(tree.leaf(S, name, lambda P, a=a, b=b: sdf.cylinder(P, a, b, radius, rounding), Box.around([a, b], radius * 1.1), material, bones, protect=0.8))

    piece("breech_block", point(0.0), point(0.3), r * 1.3, mats["iron"], rounding=r * 0.25)
    piece("breech_cap", point(-0.015), point(0.025), r * 1.38, mats["brass"], rounding=r * 0.1)
    piece("recoil", point(0.05) + up * r * 1.35, point(0.32) + up * r * 1.35, r * 0.42, mats["brass"], rounding=r * 0.1)
    piece("barrel", point(0.28), point(0.93), r, mats["iron"])
    for i, share in enumerate((0.36, 0.5, 0.64, 0.78)):
        piece("band_%d" % i, point(share), point(share + 0.03), r * 1.18, mats["brass"], rounding=r * 0.05)
    piece("muzzle", point(0.9), point(1.0), r * 1.55, mats["brass"], rounding=r * 0.15)
    piece("bore_light", point(0.94), point(1.005), r * 1.2, mats["flux"])
    for k, offset in enumerate((side, -side)):
        chamber = offset * r * 1.02 + up * r * 0.2
        piece("chamber_%d" % k, point(0.42) + chamber, point(0.6) + chamber, r * 0.4, mats["flux"], rounding=r * 0.15)
        for end, share in enumerate((0.4, 0.6)):
            piece("collar_%d_%d" % (k, end), point(share) + chamber, point(share + 0.02) + chamber, r * 0.5, mats["brass"])
        # The stabilizing brace on this upper flank, from the breech along the barrel.
        brace = (offset * 0.7 + up * 0.9) * r * 1.3
        piece("brace_%d" % k, point(0.04) + brace, point(0.5) + brace, r * 0.3, mats["iron"], rounding=r * 0.1)
    # The recoil housing over the breech.
    housing = point(0.14) + up * r * 1.5
    parts.append(tree.leaf(S, "recoil_housing", lambda P: sdf.box(P, housing, (r * 0.5, r * 1.2, r * 1.4), np.stack([up, side, along], axis=1), r * 0.15),
                           Box(housing - r * 1.6, housing + r * 1.6), mats["brass"], bones, protect=0.8))
    return Union(parts, k=0.3)

def coat_skirt(S, L, dims, mats):
    """Her coat's long skirt: from the belt to below the knee, open at the front, flaring as it falls; each half hangs
    on its own chain (ADR-069 §7) and follows its leg."""
    H = dims["height"]
    pz = L["pelvis"][0][2]
    belt_z = pz + (L["spine_03"][1][2] - pz) * 0.12
    material = S.material("coat_skirt", PALETTE["coat"])
    columns, rows = 14, 5
    t0, t1 = np.radians(28.0), np.radians(332.0)
    hip = abs(L["thigh_l"][0][1])
    hem_z = H * 0.2
    limbs = [(L["thigh_" + s][0], L["thigh_" + s][1], H * 0.05) for s in ("l", "r")] + [(L["calf_" + s][0], L["calf_" + s][1], H * 0.045) for s in ("l", "r")]

    def position(u, v):
        theta = t0 + (t1 - t0) * u
        radial = np.stack([np.cos(theta), np.sin(theta), np.zeros_like(theta)], axis=1)
        top = np.stack([hip * 1.9 * np.cos(theta), hip * 2.1 * np.sin(theta), np.full_like(theta, belt_z)], axis=1)
        p = top + radial * (H * 0.07 * v ** 1.3 + (0.3 + 1.0 * v) * np.sin(theta * 7.0))[:, None]
        p[:, 2] = belt_z + (hem_z - belt_z) * v
        return sheet.clear_of(p, limbs, 1.5)

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
    return sheet.Sheet("coat_skirt", position, material, bones, columns, rows)
