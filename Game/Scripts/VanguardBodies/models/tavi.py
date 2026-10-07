"""Tavi (ADR-069): a child having a wonderful time, read by her silhouette from the game camera. Character Bible and her
splash art: cream-blonde twin tails streaked pink, big and swinging, clipped with bunny charms; a black hooded jacket
trimmed pink and hung with plush bunny charms, the hood down; pink-and-white shorts; bandaging on one forearm, wrist
cuffs, knee pads and chunky black-and-pink boots with bunny faces on the toes. Her ball is pink, black and white with a
bunny face whose eyes are crosses. The cross is her mark: two drawn on her bare thigh.

Low poly and flat-coloured (author 2026-10-07): the big forms that make her outline, each a flat colour the toon
material shades; brightly coloured, never shadowed or menacing. She rests in the A pose, the ball in her right hand on
its prop bone. Her twin tails swing on the hair chains (ADR-069 §7)."""
import numpy as np

from ..sculpt import anatomy, garments, hair, sdf, tree
from ..sculpt.anatomy import V, unit
from ..sculpt.garments import around, band_z, both, either, folds
from ..sculpt.tree import Box, Over, Placed, Shell, Union, Zone

HAND_SHARE = 0.11
SKULL_CENTRE = (-1.2, 0.0, 14.4)
SKULL_RADII = (9.9, 7.5, 8.0)
# How far beyond the skull a lock has passed wholly to its hair chain (cm): the twin tails swing nearly whole.
HAIR_REACH = 3.0

PALETTE = {
    "skin": (0.92, 0.77, 0.68), "hair": (0.96, 0.9, 0.74), "streak": (0.96, 0.52, 0.72), "jacket": (0.17, 0.15, 0.17),
    "pink": (1.0, 0.42, 0.74), "shorts": (0.97, 0.62, 0.8), "white": (0.97, 0.96, 0.95), "boot": (0.15, 0.13, 0.14),
    "cross": (0.95, 0.3, 0.55), "ball": (0.13, 0.11, 0.12),
}


def materials(S):
    return {name: S.material(name, colour) for name, colour in PALETTE.items()}


def bunny(S, name, centre, size, facing, material, eyes, bones, protect=0.8):
    """A bunny's head facing out along facing: a round face, two tall ears, two crosses for eyes (eyes: their material,
    or None). size is the face's radius."""
    c = V(*centre)
    f = unit(facing)
    side = unit(np.cross(V(0, 0, 1), f)) if abs(f[2]) < 0.9 else V(0, 1, 0)
    up = unit(np.cross(f, side))
    parts = [tree.leaf(S, name + "_face", lambda P: sdf.ellipsoid(P, c, V(size, size, size * 0.9)), Box(c - size * 1.2, c + size * 1.2), material, bones, protect)]
    for k, s in enumerate((1.0, -1.0)):
        a = c + up * size * 0.6 + side * s * size * 0.4
        b = a + up * size * 1.3 + side * s * size * 0.25
        parts.append(tree.leaf(S, "%s_ear_%d" % (name, k), lambda P, a=a, b=b: sdf.round_cone(P, a, b, size * 0.28, size * 0.2), Box.around([a, b], size * 0.4),
                               material, bones, protect))
        if eyes is not None:
            e = c + f * size * 0.88 + side * s * size * 0.38 + up * size * 0.1
            axes = np.stack([f, side, up], axis=1)
            for j, turn in enumerate((45.0, -45.0)):
                r = np.radians(turn)
                ax = np.stack([f, side * np.cos(r) + up * np.sin(r), -side * np.sin(r) + up * np.cos(r)], axis=1)
                parts.append(tree.leaf(S, "%s_eye_%d_%d" % (name, k, j), lambda P, e=e, ax=ax: sdf.box(P, e, (size * 0.1, size * 0.28, size * 0.07), ax, 0.0),
                                       Box(e - size * 0.4, e + size * 0.4), eyes, bones, 1.0))
    return Union(parts, k=size * 0.15)


def build(S, L, dims, spec):
    """Tavi's sculpt on her layout: (the whole, {"body": the skin alone, under her clothes, "sheets": none})."""
    mats = materials(S)
    H = dims["height"]
    figure = anatomy.Figure(S, L, dims, mats["skin"], {"muscle": 0.1, "chest": 0.75, "breadth": 0.78, "hips": 0.95, "limb": 0.8,
                                                        "deltoid": 0.65, "leg": 0.86, "neck": 0.85, "pecs": 0.3}).build()
    head_origin = V(*L["head"][0])
    size = (L["head"][1][2] - L["head"][0][2]) * 24.0 / 21.9
    u = size / 24.0
    locks_bones = anatomy.rigid("head")
    chains = sorted(bone[:-3] for bone in L if bone.startswith("hair_") and bone.endswith("_01"))
    if chains:
        joints = {chain: [L["%s_%s" % (chain, joint)][0] for joint in ("01", "02", "end")] for chain in chains}
        locks_bones = hair.on_chains(joints, "head", head_origin + V(*SKULL_CENTRE) * u, V(*SKULL_RADII) * u, HAIR_REACH)
    locks = hair.messy(S, mats["hair"], locks_bones, SKULL_CENTRE, SKULL_RADII, (16.0, 7.0), seed=spec["seed"], unit_scale=u, count=22,
                       length=(5.0, 9.0), radius=(1.7, 2.5), wind=(0.0, 0.0, 0.0), fringe=0.85, volume=0.35, cap_bones=anatomy.rigid("head"))
    # Twin tails high on each side of her head, big and flaring, each with a pink streak, tied with a pink bow and a bunny charm.
    tails = []
    for side, sign in (("l", 1.0), ("r", -1.0)):
        root = V(-2.0, sign * 7.2, 19.0) * u
        # Each tail flares out from its bow and falls past her shoulder, in three big locks.
        for k, (lift, spread, back, streak) in enumerate(((0.15, 0.75, -0.35, False), (-0.05, 0.95, -0.15, True), (0.25, 0.55, -0.55, False))):
            tails.append(hair.clump(S, "tail_%s_%d" % (side, k), root, V(back, sign * spread, lift), V(0, sign, 0.4), 27.0 * u, 6.6 * u, 0.85, 0.25,
                                    mats["streak"] if streak else mats["hair"], locks_bones))
        tails.append(tree.leaf(S, "bow_%s" % side, lambda P, c=root: sdf.torus(P, c, 2.2 * u, 0.9 * u, np.stack([V(1, 0, 0), V(0, 0, 1), V(0, 1, 0)], axis=1)),
                               Box(root - 4 * u, root + 4 * u), mats["pink"], anatomy.rigid("head")))
    tails.append(bunny(S, "hair_charm", V(1.0, 8.5, 17.5) * u, 2.0 * u, V(0.6, 1, 0.2), mats["white"], mats["boot"], anatomy.rigid("head")))
    head = anatomy.Head(S, mats["skin"], size, look={"jaw": 0.82}).build()
    figure.attach_head(Placed(Over([head, locks, Union(tails, k=0.5 * u)]), head_origin, np.eye(3)))
    for side in ("l", "r"):
        figure.limbs["hand_" + side] = hand(S, L, H, side, mats, figure)
    body = figure.body()
    dressed = clothes(S, L, dims, mats, body, figure.limbs)
    worn = Over([dressed, ball(S, L, mats, H)])
    return worn, {"body": body, "sheets": []}


def hand(S, L, H, side, mats, figure):
    """A bare hand at rest beside her thigh, palm in; the right closed on her ball."""
    w0, w1 = V(*L["hand_" + side][0]), V(*L["hand_" + side][1])
    sign = 1.0 if side == "l" else -1.0
    x = unit(w1 - w0)
    out = V(0, sign, 0)
    z = unit(out - x * (out @ x))
    y = np.cross(z, x)
    curl = {"index": (30, 40, 20), "middle": (35, 45, 22), "ring": (38, 45, 22), "little": (40, 45, 20)}
    built = anatomy.Hand(S, mats["skin"], side, H * HAND_SHARE, "hand_" + side, curl=curl, thumb=(40, 20, 20, 15)).build()
    placed = Placed(built, w0, np.stack([x, y, z], axis=1))
    figure.parts.append(placed)
    return placed


def clothes(S, L, dims, mats, body, limbs):
    """Her clothes: pink shorts with a pink cross stitched on them, knee pads, chunky black-and-pink boots with bunny
    faces on the toes; the black jacket trimmed pink, its hood down behind her neck, plush bunny charms on it; bandaging
    on her right forearm and black cuffs at her wrists; two pink crosses on her bare left thigh."""
    H = dims["height"]
    pz = L["pelvis"][0][2]
    cz = L["spine_03"][1][2]
    torso = cz - pz
    shorts = Shell(S, "shorts", body, 0.4, 0.9, both(band_z(pz - H * 0.075, pz + torso * 0.12), garments.keep_to(limbs, ["leg_l", "leg_r", "torso"])),
                   mats["shorts"], hem=0.4, displace=folds((0, 0, 1), 7, 0.4, seed=61), reach=0.6)
    boot_mats = {"boot": mats["boot"], "sole": mats["pink"]}
    boots = [garments.boot(S, "boot_" + side, L, side, H * 0.2, boot_mats, None, shaft=1.35, toe=1.2) for side in ("l", "r")]
    lower = Over([body, shorts] + boots)
    pads = []
    for side in ("l", "r"):
        k = V(*L["calf_" + side][0])
        pads.append(tree.leaf(S, "knee_pad_" + side, lambda P, c=k + V(H * 0.025, 0, 0): sdf.ellipsoid(P, c, V(H * 0.022, H * 0.03, H * 0.035)),
                              Box(k - H * 0.05, k + H * 0.06), mats["boot"], anatomy.rigid("calf_" + side), protect=0.5))
        # Boot cuffs in pink and a bunny face on each toe.
        a = V(*L["calf_" + side][1])
        cuff = Zone(lambda P, z0=H * 0.2: np.abs(P[:, 2] - (z0 - H * 0.012)) - H * 0.014, Box((-50, -50, 0), (50, 50, H * 0.25)))
        pads.append(Shell(S, "boot_cuff_" + side, Over([lower]), 0.2, 0.8, both(cuff, garments.keep_to(limbs, ["leg_" + side])), mats["pink"], hem=0.2))
        toe = V(*L["foot_" + side][1])
        pads.append(bunny(S, "boot_bunny_" + side, toe + V(H * 0.03, 0, H * 0.035), H * 0.018, V(1, 0, 0.3), mats["white"], mats["boot"],
                          anatomy.rigid("foot_" + side), protect=0.7))
    kneed = Over([lower] + pads)
    jacket_region = either(both(band_z(pz + torso * 0.05, cz + H * 0.01), garments.keep_to(limbs, ["torso"])),
                           garments.keep_to(limbs, ["upperarm_l", "upperarm_r"]))
    jacket = Shell(S, "jacket", kneed, 0.8, 1.4, jacket_region, mats["jacket"], hem=0.5,
                   displace=folds((0, 0, 1), 8, 0.7, seed=62), reach=1.2)
    dressed = Over([kneed, jacket])
    trim = Shell(S, "jacket_trim", dressed, 0.0, 0.5, both(band_z(pz + torso * 0.05, pz + torso * 0.12), garments.keep_to(limbs, ["torso"])), mats["pink"], hem=0.2)
    extras = []
    # The hood, down: a thick roll behind her neck.
    back = V(-H * 0.035, 0, cz + H * 0.005)
    extras.append(tree.leaf(S, "hood", lambda P: sdf.ellipsoid(P, back, V(H * 0.035, H * 0.06, H * 0.03)), Box(back - H * 0.07, back + H * 0.07),
                            mats["jacket"], anatomy.rigid("spine_03"), protect=0.3))
    # Plush bunny charms on the jacket's left breast and shoulder.
    for k, (c, f) in enumerate(((V(H * 0.07, H * 0.06, cz - torso * 0.2), V(1, 0.3, 0)), (V(H * 0.01, H * 0.1, cz + H * 0.005), V(0.3, 1, 0.3)))):
        extras.append(bunny(S, "charm_%d" % k, c, H * 0.016, f, mats["white"], mats["boot"], anatomy.rigid("spine_03"), protect=0.6))
    # Bandaging on her right forearm; black cuffs at her wrists.
    for side in ("l", "r"):
        e, w = V(*L["lowerarm_" + side][0]), V(*L["lowerarm_" + side][1])
        if side == "r":
            region = both(around([e + (w - e) * 0.2, e + (w - e) * 0.75], [5.0, 4.5]), garments.keep_to(limbs, ["forearm_r"]))
            extras.append(Shell(S, "bandage", body, 0.2, 0.5, region, mats["white"], hem=0.2, displace=folds(w - e, 4, 0.25, seed=63), reach=0.4))
        region = both(around([w + (e - w) * 0.05, w + (e - w) * 0.2], [4.0, 4.5]), garments.keep_to(limbs, ["forearm_" + side]))
        extras.append(Shell(S, "cuff_" + side, body, 0.3, 0.9, region, mats["boot"], hem=0.2, reach=0.4))
    # Two crosses drawn on her bare left thigh.
    hp, k = V(*L["thigh_l"][0]), V(*L["thigh_l"][1])
    for j, share in enumerate((0.55, 0.72)):
        c = hp + (k - hp) * share + V(H * 0.045, H * 0.005, 0)
        for t, turn in enumerate((45.0, -45.0)):
            r = np.radians(turn)
            ax = np.stack([V(1, 0, 0), V(0, np.cos(r), np.sin(r)), V(0, -np.sin(r), np.cos(r))], axis=1)
            extras.append(tree.leaf(S, "thigh_cross_%d_%d" % (j, t), lambda P, c=c, ax=ax: sdf.box(P, c, (H * 0.004, H * 0.02, H * 0.005), ax, 0.0),
                                    Box(c - H * 0.03, c + H * 0.03), mats["cross"], anatomy.rigid("thigh_l"), protect=0.6))
    return Over([dressed, trim, Union(extras, k=0.2)])


def ball(S, L, mats, H):
    """Her ball in her right hand on its prop bone: black, banded pink, a white bunny face with crossed eyes."""
    bones = anatomy.rigid("prop_r")
    grip = V(*L["hand_r"][1])
    c = grip + V(H * 0.07, 0, H * 0.01)
    r = H * 0.075
    parts = [tree.leaf(S, "ball", lambda P: sdf.sphere(P, c, r), Box(c - r * 1.1, c + r * 1.1), mats["ball"], bones, protect=0.8),
             tree.leaf(S, "ball_band", lambda P: sdf.cylinder(P, c - V(0, 0, r * 0.22), c + V(0, 0, r * 0.22), r * 1.02, r * 0.05),
                       Box(c - r * 1.1, c + r * 1.1), mats["pink"], bones, protect=0.8)]
    face = bunny(S, "ball_bunny", c + V(r * 0.72, 0, r * 0.15), r * 0.42, V(1, 0, 0), mats["white"], mats["ball"], bones, protect=1.0)
    return Union(parts + [face], k=r * 0.05)
