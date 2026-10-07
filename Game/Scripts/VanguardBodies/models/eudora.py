"""Eudora (ADR-069): a broad-shouldered engineer of the Iron March, read by her silhouette from the game camera.
Character Bible and her splash art: short iron-grey hair, goggles pushed up on her forehead, a burned cheek; a weathered
red work coat, open over dark workwear, its long skirt torn at the hem; a tool harness and belt hung with pouches. Both
arms are mechanical from the shoulder, banded brass-and-steel sleeves ending in articulated gripping hands, and both legs
are mechanical from the knee down, with heavy plated knees and jointed feet. Her torso, neck and face are flesh. She
carries rolled drawings in her left hand.

Low poly and flat-coloured (author 2026-10-07): the big forms that make her outline, each a flat colour the toon
material shades. Her coat's skirt swings on the coat-tail chains and her hair on the hair chains (ADR-069 §7). Picket,
her machine, is a companion of its own and no part of her."""
import numpy as np

from ..sculpt import anatomy, garments, hair, sdf, sheet, tree
from ..sculpt.anatomy import V, unit
from ..sculpt.garments import around, band_z, both, either, folds
from ..sculpt.tree import Box, Over, Placed, Shell, Union, Zone

HAND_SHARE = 0.11
SKULL_CENTRE = (-1.2, 0.0, 14.4)
SKULL_RADII = (9.9, 7.5, 8.0)
HAIR_REACH = 3.5

PALETTE = {
    "skin": (0.8, 0.64, 0.54), "hair": (0.62, 0.62, 0.64), "scar": (0.72, 0.36, 0.32), "coat": (0.62, 0.15, 0.1),
    "lining": (0.42, 0.09, 0.06), "work": (0.24, 0.22, 0.22), "leather": (0.4, 0.27, 0.16), "steel": (0.38, 0.36, 0.35),
    "brass": (0.74, 0.56, 0.28), "lens": (0.2, 0.22, 0.26), "paper": (0.9, 0.86, 0.74),
}


def materials(S):
    return {name: S.material(name, colour) for name, colour in PALETTE.items()}


def build(S, L, dims, spec):
    """Eudora's sculpt on her layout: (the whole, {"body": the figure, under her clothes and prostheses, "sheets": her
    coat's skirt and its lining})."""
    mats = materials(S)
    H = dims["height"]
    figure = anatomy.Figure(S, L, dims, mats["skin"], {"muscle": 0.7, "chest": 1.0, "breadth": 1.05, "hips": 1.05, "limb": 1.05,
                                                        "deltoid": 1.0, "leg": 1.0, "neck": 0.95, "bust": 0.6}).build()
    head_origin = V(*L["head"][0])
    size = (L["head"][1][2] - L["head"][0][2]) * 24.0 / 21.9
    u = size / 24.0
    locks_bones = anatomy.rigid("head")
    chains = sorted(bone[:-3] for bone in L if bone.startswith("hair_") and bone.endswith("_01"))
    if chains:
        joints = {chain: [L["%s_%s" % (chain, joint)][0] for joint in ("01", "02", "end")] for chain in chains}
        locks_bones = hair.on_chains(joints, "head", head_origin + V(*SKULL_CENTRE) * u, V(*SKULL_RADII) * u, HAIR_REACH)
    locks = hair.messy(S, mats["hair"], locks_bones, SKULL_CENTRE, SKULL_RADII, (16.5, 7.5), seed=spec["seed"], unit_scale=u, count=24,
                       length=(4.0, 7.0), radius=(1.5, 2.2), wind=(-0.3, 0.1, 0.0), fringe=0.6, volume=0.35, cap_bones=anatomy.rigid("head"))
    bones = anatomy.rigid("head")
    extras = [
        # The goggles pushed up on her forehead: a strap round her head, two brass-rimmed lenses at the front.
        tree.leaf(S, "goggle_strap", lambda P: sdf.torus(P, V(0.0, 0, 17.0) * u, 9.6 * u, 0.7 * u, np.stack([V(1, 0, 0), V(0, 1, 0), V(0, 0, 1)], axis=1)),
                  Box(V(-12, -12, 14) * u, V(12, 12, 20) * u), mats["leather"], bones, 0.6),
    ]
    for side in (1.0, -1.0):
        c = V(8.6, side * 3.4, 17.6) * u
        extras.append(tree.leaf(S, "goggle_rim_%d" % (side > 0), lambda P, c=c: sdf.cylinder(P, c - V(1.2, 0, -0.5) * u, c + V(1.0, 0, 0.6) * u, 2.6 * u, 0.4 * u),
                                Box(c - 4 * u, c + 4 * u), mats["brass"], bones, 0.9))
        extras.append(tree.leaf(S, "goggle_lens_%d" % (side > 0), lambda P, c=c: sdf.cylinder(P, c + V(0.6, 0, 0.3) * u, c + V(1.3, 0, 0.7) * u, 2.0 * u, 0.2 * u),
                                Box(c - 4 * u, c + 4 * u), mats["lens"], bones, 0.9))
    # The burned cheek on her right.
    extras.append(tree.leaf(S, "burn", lambda P: sdf.ellipsoid(P, V(6.6, -5.4, 7.6) * u, V(1.6, 1.6, 2.6) * u), Box(V(4, -8, 4) * u, V(9, -3, 11) * u),
                            mats["scar"], bones, 1.0))
    head = anatomy.Head(S, mats["skin"], size, look={"jaw": 1.0}).build()
    figure.attach_head(Placed(Over([head, locks, Union(extras, k=0.2 * u)]), head_origin, np.eye(3)))
    for side in ("l", "r"):
        figure.limbs["hand_" + side] = hand(S, L, H, side, mats, figure)
    body = figure.body()
    dressed = clothes(S, L, dims, mats, body, figure.limbs)
    worn = Over([dressed, drawings(S, L, mats, H)])
    return worn, {"body": body, "sheets": [coat_skirt(S, L, dims, mats, "coat"), coat_skirt(S, L, dims, mats, "lining")]}


def hand(S, L, H, side, mats, figure):
    """A mechanical gripping hand in steel, articulated like her own: the left closed on her drawings."""
    w0, w1 = V(*L["hand_" + side][0]), V(*L["hand_" + side][1])
    sign = 1.0 if side == "l" else -1.0
    x = unit(w1 - w0)
    out = V(0, sign, 0)
    z = unit(out - x * (out @ x))
    y = np.cross(z, x)
    curl = {"index": (55, 75, 40), "middle": (60, 80, 40), "ring": (62, 80, 40), "little": (65, 80, 38)} if side == "l" else \
           {"index": (20, 25, 15), "middle": (25, 30, 15), "ring": (28, 30, 15), "little": (30, 30, 15)}
    built = anatomy.Hand(S, mats["steel"], side, H * HAND_SHARE, "hand_" + side, curl=curl, thumb=(45, 25, 25, 20)).build()
    placed = Placed(built, w0, np.stack([x, y, z], axis=1))
    figure.parts.append(placed)
    return placed


def clothes(S, L, dims, mats, body, limbs):
    """Her prostheses and clothes: steel sleeves banded in brass over both arms from the shoulder, a brass joint at each
    shoulder and elbow; steel shins with heavy plated knees and jointed feet; dark workwear; the red coat open over it,
    its sleeves to the elbow; a tool harness and a belt hung with pouches."""
    H = dims["height"]
    pz = L["pelvis"][0][2]
    cz = L["spine_03"][1][2]
    torso = cz - pz
    work_region = either(both(band_z(H * 0.25, cz + H * 0.01), garments.keep_to(limbs, ["leg_l", "leg_r", "torso"])))
    work = Shell(S, "workwear", body, 0.3, 0.6, work_region, mats["work"], hem=0.3, reach=0.6)
    parts = []
    shells = []
    for side in ("l", "r"):
        s, e = V(*L["upperarm_" + side][0]), V(*L["upperarm_" + side][1])
        w = V(*L["lowerarm_" + side][1])
        # The upper arm slim under her sleeve; the forearm a heavy plated gauntlet, the bulk her art gives it.
        shells.append(Shell(S, "arm_steel_" + side, body, 0.5, 1.0, garments.keep_to(limbs, ["upperarm_" + side]), mats["steel"], hem=0.3, reach=0.6))
        shells.append(Shell(S, "gauntlet_" + side, body, 1.4, 1.8, garments.keep_to(limbs, ["forearm_" + side]), mats["steel"], hem=0.4,
                            displace=folds(unit(w - e), 4, 0.5, seed=37), reach=1.2))
        for k, (a, b) in enumerate(((s, e), (e, w))):
            for share in (0.3, 0.65):
                c = a + (b - a) * share
                axis = unit(b - a)
                parts.append(tree.leaf(S, "band_%s_%d_%d" % (side, k, int(share * 100)), lambda P, c=c, axis=axis: sdf.cylinder(P, c - axis * H * 0.012, c + axis * H * 0.012, H * 0.046, H * 0.004),
                                       Box(c - H * 0.06, c + H * 0.06), mats["brass"], anatomy.rigid("upperarm_" + side if k == 0 else "lowerarm_" + side), protect=0.5))
        for name, c, bone in (("shoulder", s, "upperarm_" + side), ("elbow", e, "lowerarm_" + side)):
            parts.append(tree.leaf(S, "%s_joint_%s" % (name, side), lambda P, c=c: sdf.sphere(P, c, H * 0.04), Box(c - H * 0.05, c + H * 0.05),
                                   mats["brass"], anatomy.rigid(bone), protect=0.5))
        # The mechanical leg from the knee: a steel shin, a plated knee, a jointed foot.
        k, a = V(*L["calf_" + side][0]), V(*L["calf_" + side][1])
        f0, f1 = V(*L["foot_" + side][0]), V(*L["foot_" + side][1])
        shells.append(Shell(S, "shin_" + side, body, 0.6, 1.4, both(band_z(-1.0, k[2] - H * 0.01), garments.keep_to(limbs, ["leg_" + side])), mats["steel"], hem=0.3, reach=0.6))
        parts.append(tree.leaf(S, "knee_plate_" + side, lambda P, c=k + V(H * 0.03, 0, 0): sdf.box(P, c, (H * 0.022, H * 0.04, H * 0.045), None, H * 0.01),
                               Box(k - H * 0.07, k + H * 0.08), mats["brass"], anatomy.rigid("calf_" + side), protect=0.6))
        toe = V(f1[0] + H * 0.02, f1[1], H * 0.02)
        parts.append(tree.leaf(S, "foot_plate_" + side, lambda P, a=V(f0[0] - H * 0.03, f0[1], H * 0.025), b=toe: sdf.box(P, (a + b) / 2, (np.linalg.norm(b - a) / 2 + H * 0.01, H * 0.035, H * 0.025), None, H * 0.008),
                               Box(f0 - H * 0.08, toe + H * 0.08), mats["steel"], anatomy.rigid("foot_" + side), protect=0.6))
    mech = Over([body, work] + shells + [Union(parts, k=0.3)])
    # Short sleeves, to halfway down the upper arm.
    sleeves = []
    for side in ("l", "r"):
        s, e = V(*L["upperarm_" + side][0]), V(*L["upperarm_" + side][1])
        sleeves.append(both(garments.keep_to(limbs, ["upperarm_" + side]), garments.half(s + (e - s) * 0.6, e - s, Box.around([s, e], H * 0.1))))
    coat_region = either(both(band_z(pz + torso * 0.2, cz + H * 0.015), garments.keep_to(limbs, ["torso"])), *sleeves)
    open_front = Zone(lambda P: np.maximum(2.0 - P[:, 0], np.abs(P[:, 1]) - 7.0), Box((0, -12, pz), (40, 12, cz + 15)))
    coat = Shell(S, "coat", mech, 0.5, 1.1, garments.without(coat_region, open_front), mats["coat"], hem=0.4, displace=folds((0, 0, 1), 9, 0.5, seed=251), reach=1.0)
    dressed = Over([mech, coat])
    belt_z = pz + torso * 0.12
    harness = []
    # Belts stacked up her middle, the lowest carrying the pouches.
    for k, z in enumerate((belt_z, belt_z + torso * 0.14, belt_z + torso * 0.27)):
        harness.append(Shell(S, "belt_%d" % k, dressed, 0.3, 1.2, both(band_z(z - H * 0.012, z + H * 0.012), garments.keep_to(limbs, ["torso"])), mats["leather"], hem=0.3))
    for sign in (1.0, -1.0):
        a, b = V(H * 0.03, sign * H * 0.06, cz), V(H * 0.04, sign * H * 0.04, belt_z)
        band = both(garments.band_plane((a + b) / 2, V(0, 1, 0), H * 0.025, Box((-40, -60, belt_z - 5), (40, 60, cz + 8))), garments.keep_to(limbs, ["torso"]))
        harness.append(Shell(S, "harness_%d" % (sign > 0), dressed, 0.3, 0.9, band, mats["leather"], hem=0.3, reach=0.5))
    pouches = []
    centre = V(L["pelvis"][0][0], 0.0, belt_z - H * 0.02)
    for k, degrees in enumerate((40.0, -40.0, -95.0)):
        angle = np.radians(degrees)
        c, normal = garments.surface_point(dressed, centre, V(np.cos(angle), np.sin(angle), 0.0))
        out = unit(normal * V(1, 1, 0))
        axes = np.stack([unit(np.cross(V(0, 0, 1), out)), V(0, 0, 1), out], axis=1)
        pouches.append(garments.pouch(S, "pouch_%d" % k, c, axes, (H * 0.05, H * 0.055, H * 0.03), H * 0.02, mats["leather"], bones=anatomy.rigid("pelvis")))
    return Over([dressed] + harness + [Union(pouches, k=0.3)])


def drawings(S, L, mats, H):
    """Her rolled drawings in her left hand: three rolls of paper, forward and down past her hip."""
    bones = anatomy.rigid("prop_l")
    grip = V(*L["prop_l"][0])
    parts = []
    for k, (dy, dz, length) in enumerate(((0.0, 0.0, 0.17), (0.012, 0.02, 0.15), (-0.012, 0.018, 0.14))):
        c = grip + V(0, dy * H, dz * H)
        a, b = c - V(H * length * 0.35, 0, -H * 0.02), c + V(H * length * 0.65, 0, -H * 0.05)
        parts.append(tree.leaf(S, "roll_%d" % k, lambda P, a=a, b=b: sdf.cylinder(P, a, b, H * 0.016, H * 0.004), Box.around([a, b], H * 0.025),
                               mats["paper"], bones, protect=0.8))
    return Union(parts, k=0.1)


def coat_skirt(S, L, dims, mats, layer):
    """Her red coat's long skirt, or its darker lining just inside it: from the belt to her shins, open at the front, its
    hem worn ragged. Each half hangs on its own coat-tail chain (ADR-069 §7)."""
    H = dims["height"]
    pz = L["pelvis"][0][2]
    belt_z = pz + (L["spine_03"][1][2] - pz) * 0.12
    lining = layer == "lining"
    material = S.material("skirt_" + layer, PALETTE["lining" if lining else "coat"])
    columns, rows, strips = 16, 5, 8
    t0, t1 = np.radians(48.0), np.radians(312.0)
    hip = abs(L["thigh_l"][0][1])
    hem_z = H * (0.1 if lining else 0.075)
    inset = -0.8 if lining else 0.0
    limbs = [(L["thigh_" + s][0], L["thigh_" + s][1], H * 0.055) for s in ("l", "r")] + [(L["calf_" + s][0], L["calf_" + s][1], H * 0.05) for s in ("l", "r")]

    def position(u, v):
        theta = t0 + (t1 - t0) * u
        radial = np.stack([np.cos(theta), np.sin(theta), np.zeros_like(theta)], axis=1)
        top = np.stack([hip * 1.6 * np.cos(theta), hip * 1.85 * np.sin(theta), np.full_like(theta, belt_z)], axis=1)
        p = top + radial * (H * 0.045 * v ** 1.2 + inset + (0.3 + 0.8 * v) * np.sin(theta * 7.0))[:, None]
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
    cut, point = (0.85, 0.06) if lining else (0.7, 0.12)
    return sheet.Sheet("skirt_" + layer, position, material, bones, columns, rows,
                       reach=lambda u: sheet.torn(u, strips, cut, point, 29.0 if lining else 31.0))
