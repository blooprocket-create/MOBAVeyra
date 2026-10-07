"""Vera (ADR-069): a young soldier still fighting in the wreckage of her city, read by her silhouette from the game
camera. Character Bible and her splash art: white-silver hair in a short high side-tail bound with a red ribbon; a black
layered coat torn to ribbons at the hem, its red lining showing through every rent, over dark segmented armour (a heavy
plated pauldron on her left shoulder, banded gauntlets, thigh and shin plates, armoured boots). Her weapon is an
enormous two-handed siege arm carried across her body: dark timber and riveted iron, a bladed spike running forward
past the muzzle, a hot red Flux line burning along its length, and a red pennant hung from the fore-end bearing a white
cross-and-sword device. One dark red-and-black figure with a long horizontal weapon. Never translucent.

Low poly and flat-coloured (author 2026-10-07). She rests empty-handed in the A pose; her clips carry the siege arm low
and raise it only to fire (humanoid holds, ADR-069 §6), so it is built on her right hand's prop bone where the holds lay
it, as at the aim, moved to her rest wrist. Her coat and its lining hang on the coat-tail chains, her side-tail on the
hair chains (ADR-069 §7). Colours are her kit's, lifted to read under the toon light."""
import numpy as np

from ..sculpt import anatomy, garments, hair, sdf, sheet, tree
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

PALETTE = {
    "skin": (0.9, 0.82, 0.78), "hair": (0.9, 0.9, 0.93), "ribbon": (0.7, 0.1, 0.1), "coat": (0.2, 0.19, 0.22),
    "lining": (0.62, 0.09, 0.09), "armour": (0.26, 0.25, 0.28), "iron": (0.34, 0.32, 0.31), "leather": (0.24, 0.2, 0.18),
    "timber": (0.32, 0.2, 0.12), "flux": (1.0, 0.25, 0.15), "pennant": (0.66, 0.08, 0.08), "device": (0.92, 0.9, 0.86),
}


def materials(S):
    """Every material Vera is coloured in, flat (the toon material shades it); the siege arm's Flux line glows."""
    return {name: S.material(name, colour, glow=name == "flux") for name, colour in PALETTE.items()}


def build(S, L, dims, spec):
    """Vera's sculpt on her layout: (the whole, {"body": the skin alone, under her clothes, "sheets": her coat})."""
    mats = materials(S)
    H = dims["height"]
    figure = anatomy.Figure(S, L, dims, mats["skin"], {"muscle": 0.4, "chest": 0.85, "breadth": 0.8, "hips": 1.05, "limb": 0.85,
                                                        "deltoid": 0.75, "leg": 0.88, "neck": 0.82, "bust": 0.65}).build()
    head_origin = V(*L["head"][0])
    size = (L["head"][1][2] - L["head"][0][2]) * 24.0 / 21.9
    u = size / 24.0
    locks_bones = anatomy.rigid("head")
    chains = sorted(bone[:-3] for bone in L if bone.startswith("hair_") and bone.endswith("_01"))
    if chains:
        joints = {chain: [L["%s_%s" % (chain, joint)][0] for joint in ("01", "02", "end")] for chain in chains}
        locks_bones = hair.on_chains(joints, "head", head_origin + V(*SKULL_CENTRE) * u, V(*SKULL_RADII) * u, HAIR_REACH)
    locks = hair.messy(S, mats["hair"], locks_bones, SKULL_CENTRE, SKULL_RADII, (16.5, 7.0), seed=spec["seed"], unit_scale=u, count=26,
                       length=(5.0, 9.0), radius=(1.6, 2.4), wind=(0.0, -0.2, 0.0), fringe=0.7, volume=0.4, cap_bones=anatomy.rigid("head"))
    # The high side-tail on her right, bound with a red ribbon, swinging on the hair chains (in the head's frame).
    root = V(-3.0, -6.5, 20.0) * u
    tail = hair.clump(S, "side_tail", root, V(-0.5, -0.6, 0.6), V(0, -1, 0.3), 13.0 * u, 2.6 * u, 0.55, 0.3, mats["hair"], locks_bones)
    ribbon = tree.leaf(S, "ribbon", lambda P: sdf.torus(P, root, 1.8 * u, 0.7 * u, np.stack([V(1, 0, 0), V(0, 0.6, 0.8), V(0, -0.8, 0.6)], axis=1)),
                       Box(root - 4 * u, root + 4 * u), mats["ribbon"], anatomy.rigid("head"))
    head = anatomy.Head(S, mats["skin"], size, look={"jaw": 0.86}).build()
    figure.attach_head(Placed(Over([head, locks, Union([tail, ribbon], k=0.3 * u)]), head_origin, np.eye(3)))
    for side in ("l", "r"):
        figure.limbs["hand_" + side] = hand(S, L, H, side, mats, figure)
    body = figure.body()
    dressed = clothes(S, L, dims, mats, body, figure.limbs)
    worn = Over([dressed, siege_arm(S, L, mats, H)])
    return worn, {"body": body, "sheets": [coat_skirt(S, L, dims, mats, "coat"), coat_skirt(S, L, dims, mats, "lining")]}


def hand(S, L, H, side, mats, figure):
    """A gloved hand where the holds lay it: built as it is at the aim (the right closed on the grip, the left palm up
    under the fore-end), moved unturned to her rest wrist."""
    w0, w1 = V(*L["hand_" + side][0]), V(*L["hand_" + side][1])
    x = unit(w1 - w0)
    z = unit(V(0, -1, 0) - x * (V(0, -1, 0) @ x)) if side == "r" else unit(V(0, 0, -1) - x * (V(0, 0, -1) @ x))
    y = np.cross(z, x)
    curl = {"index": (60, 85, 45), "middle": (68, 92, 48), "ring": (70, 92, 48), "little": (72, 92, 45)} if side == "r" else \
           {"index": (35, 55, 30), "middle": (40, 60, 30), "ring": (42, 60, 30), "little": (45, 60, 30)}
    built = anatomy.Hand(S, mats["skin"], side, H * HAND_SHARE, "hand_" + side, curl=curl, thumb=(50, 25, 25, 20)).build()
    glove = Shell(S, "glove_" + side, built, 0.08, 0.35, Zone(lambda P: -np.ones(len(P)), Box((-30, -30, -30), (30, 30, 30))), mats["leather"], hem=0.1, reach=0.5)
    placed = Placed(Over([built, glove]), w0, np.stack([x, y, z], axis=1))
    figure.parts.append(placed)
    return placed


def clothes(S, L, dims, mats, body, limbs):
    """Dark segmented armour under the black coat: a breastplate and tassets over the torso, banded gauntlets, thigh and
    shin plates and armoured boots; the coat over the torso and arms, high at the collar; a belt; and the heavy layered
    pauldron over her left shoulder."""
    H = dims["height"]
    pz = L["pelvis"][0][2]
    cz = L["spine_03"][1][2]
    torso = cz - pz
    legs = both(band_z(H * 0.05, pz + H * 0.03), garments.keep_to(limbs, ["leg_l", "leg_r", "torso"]))
    under = Shell(S, "leggings", body, 0.3, 0.6, legs, mats["coat"], hem=0.3)
    boot_mats = {"boot": mats["armour"], "sole": mats["leather"]}
    boots = [garments.boot(S, "boot_" + side, L, side, H * 0.3, boot_mats, None, shaft=1.08, toe=1.0) for side in ("l", "r")]
    lower = Over([body, under] + boots)
    plates = []
    for side in ("l", "r"):
        hp, k = V(*L["thigh_" + side][0]), V(*L["thigh_" + side][1])
        region = both(around([hp + (k - hp) * 0.25, hp + (k - hp) * 0.85], [8.0, 7.5]), garments.keep_to(limbs, ["leg_" + side]),
                      Zone(lambda P, x0=hp[0]: x0 - 1.0 - P[:, 0], Box((-30, -40, 0), (40, 40, 200))))
        plates.append(Shell(S, "thigh_plate_" + side, lower, 0.4, 1.0, region, mats["armour"], hem=0.4, reach=0.5))
        e, w = V(*L["lowerarm_" + side][0]), V(*L["lowerarm_" + side][1])
        region = both(around([w, w + (e - w) * 0.75], [6.0, 6.5]), garments.keep_to(limbs, ["forearm_" + side]))
        plates.append(Shell(S, "gauntlet_" + side, lower, 0.4, 1.0, region, mats["iron"], hem=0.3, displace=garments.folds(e - w, 3, 0.4, seed=7), reach=0.6))
    armoured = Over([lower] + plates)
    breast = Shell(S, "breastplate", armoured, 0.3, 0.9, both(band_z(pz + torso * 0.05, cz - torso * 0.08), garments.keep_to(limbs, ["torso"])),
                   mats["armour"], hem=0.3, reach=0.5)
    # The coat over the torso and arms, its high collar standing at her neck.
    coat_region = either(both(band_z(pz + torso * 0.25, cz + H * 0.015), garments.keep_to(limbs, ["torso"])),
                         garments.keep_to(limbs, ["upperarm_l", "upperarm_r"]))
    open_front = Zone(lambda P: np.maximum(2.0 - P[:, 0], np.abs(P[:, 1]) - 5.5), Box((0, -10, pz), (40, 10, cz + 15)))
    coat = Shell(S, "coat", Over([armoured, breast]), 0.6, 1.2, garments.without(coat_region, open_front), mats["coat"], hem=0.4,
                 displace=folds((0, 0, 1), 10, 0.6, seed=171), reach=1.0)
    dressed = Over([armoured, breast, coat])
    belt_z = pz + torso * 0.12
    belt = Shell(S, "belt", dressed, 0.2, 1.0, both(band_z(belt_z - H * 0.012, belt_z + H * 0.012), garments.keep_to(limbs, ["torso"])),
                 mats["leather"], hem=0.2)
    # The pauldron: three plates layered over her left shoulder, overhanging it.
    s = V(*L["upperarm_l"][0])
    pieces = []
    for layer in range(3):
        c = s + V(-H * 0.006 * layer, H * 0.008 * layer, H * 0.03 - H * 0.016 * layer)
        r = H * (0.078 - layer * 0.009)
        pieces.append(tree.leaf(S, "pauldron_%d" % layer, lambda P, c=c, r=r: sdf.ellipsoid(P, c, V(r * 1.15, r, r * 0.45)), Box(c - r * 1.3, c + r * 1.3),
                                mats["armour"] if layer != 1 else mats["iron"], anatomy.along("clavicle_l", "upperarm_l", s - V(0, H * 0.05, 0), s + V(0, H * 0.05, 0), 0.3, 0.7),
                                protect=0.5))
    return Over([dressed, belt, Union(pieces, k=0.3)])


def flat_cone(P, a, b, ra, rb, flatten, across):
    """A cone from a to b squashed across (a unit vector square to it) by flatten: a blade's spike. Its distance is the
    squashed cone's divided by flatten, so it never reads farther than the true surface."""
    Q = P - a
    along_unit = unit(b - a)
    side = np.outer(Q @ across, across)
    Q2 = Q + side * (flatten - 1.0)
    return sdf.round_cone(Q2 + a, a, b, ra, rb) / flatten


def siege_arm(S, L, mats, H):
    """The Merrin siege arm on her right hand's prop bone, pointing the way she faces as the holds lay it: a dark timber
    stock, a riveted iron barrel in bands, a bladed spike running forward past the muzzle, a red Flux line burning along
    its top, and a red pennant hung from the fore-end bearing a white cross-and-sword device."""
    bones = anatomy.rigid("prop_r")
    grip = V(*L["prop_r"][0])
    f, up, side = V(1, 0, 0), V(0, 0, 1), V(0, 1, 0)
    parts = []

    def add(name, distance, box_points, pad, material, protect=0.8):
        parts.append(tree.leaf(S, name, distance, Box.around(box_points, pad), material, bones, protect=protect))

    stock_c = grip - f * H * 0.115 - up * H * 0.012
    add("stock", lambda P: sdf.box(P, stock_c, (H * 0.165, H * 0.026, H * 0.045), None, H * 0.008), [stock_c], H * 0.17, mats["timber"])
    muzzle = grip + f * H * 0.5
    add("barrel", lambda P: sdf.cylinder(P, grip, muzzle, H * 0.04), [grip, muzzle], H * 0.05, mats["iron"])
    for i, share in enumerate((0.05, 0.18, 0.32, 0.46)):
        a, b = grip + f * H * share, grip + f * H * (share + 0.024)
        add("band_%d" % i, lambda P, a=a, b=b: sdf.cylinder(P, a, b, H * 0.05, H * 0.004), [a, b], H * 0.06, mats["iron"])
    tip = grip + f * H * 0.74
    add("spike", lambda P: flat_cone(P, muzzle - f * H * 0.02, tip, H * 0.035, H * 0.002, 3.0, side), [muzzle, tip], H * 0.04, mats["iron"], 1.0)
    a, b = grip - f * H * 0.2 + up * H * 0.043, grip + f * H * 0.48 + up * H * 0.043
    add("flux_line", lambda P: sdf.capsule(P, a, b, H * 0.008), [a, b], H * 0.012, mats["flux"], 1.0)
    # The pennant, hung from the fore-end, its device on both faces.
    top = grip + f * H * 0.38 - up * H * 0.04
    pc = top - up * H * 0.09
    add("pennant", lambda P: sdf.box(P, pc, (H * 0.06, H * 0.004, H * 0.09), None, 0.0), [pc], H * 0.1, mats["pennant"])
    for k, sgn in enumerate((1.0, -1.0)):
        dc = pc + side * sgn * H * 0.005
        add("device_v_%d" % k, lambda P, dc=dc: sdf.box(P, dc, (H * 0.008, H * 0.002, H * 0.05), None, 0.0), [dc], H * 0.06, mats["device"], 1.0)
        add("device_h_%d" % k, lambda P, dc=dc: sdf.box(P, dc + up * H * 0.02, (H * 0.03, H * 0.002, H * 0.008), None, 0.0), [dc], H * 0.06, mats["device"], 1.0)
    return Union(parts, k=0.15)


def coat_skirt(S, L, dims, mats, layer):
    """Her coat's long skirt, or its red lining just inside it: from the belt nearly to the ground, open at the front,
    torn to ribbons at the hem (the coat more than its lining, so the red shows through every rent). Each half hangs on
    its own coat-tail chain (ADR-069 §7)."""
    H = dims["height"]
    pz = L["pelvis"][0][2]
    belt_z = pz + (L["spine_03"][1][2] - pz) * 0.12
    lining = layer == "lining"
    material = S.material("skirt_" + layer, PALETTE["lining" if lining else "coat"])
    columns, rows, strips = 18, 6, 9
    t0, t1 = np.radians(30.0), np.radians(330.0)
    hip = abs(L["thigh_l"][0][1])
    hem_z = H * (0.08 if lining else 0.045)
    inset = -0.8 if lining else 0.0
    limbs = [(L["thigh_" + s][0], L["thigh_" + s][1], H * 0.05) for s in ("l", "r")] + [(L["calf_" + s][0], L["calf_" + s][1], H * 0.045) for s in ("l", "r")]

    def position(u, v):
        theta = t0 + (t1 - t0) * u
        radial = np.stack([np.cos(theta), np.sin(theta), np.zeros_like(theta)], axis=1)
        top = np.stack([hip * 2.0 * np.cos(theta), hip * 2.2 * np.sin(theta), np.full_like(theta, belt_z)], axis=1)
        p = top + radial * (H * 0.09 * v ** 1.2 + inset + (0.3 + 1.2 * v) * np.sin(theta * 8.0))[:, None]
        p[:, 2] = belt_z + (hem_z - belt_z) * v
        return sheet.clear_of(p, limbs, 1.5 + (inset if lining else 0.0) * 0.5)

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
    cut, point = (0.75, 0.1) if lining else (0.45, 0.25)
    return sheet.Sheet("skirt_" + layer, position, material, bones, columns, rows,
                       reach=lambda u: sheet.torn(u, strips, cut, point, 13.0 if lining else 17.0))
