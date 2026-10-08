"""Gorraveth (ADR-069): a huge scarred reptile in scorched mining armour, read by his silhouette from the game camera.
Character Bible and his splash art: hunched, with powerful digitigrade legs, muscular arms and a heavy spined tail;
red scales over a cream throat and chest; a black mane of spikes from his crown down his neck and back, and a ragged
fringe of it under his jaw; a crocodilian head with a badly scarred snarl, a dark chipped horn and a broken stump, one
clouded eye and a torn frill; mismatched scorched mining armour (bronze-bound pauldrons and a harness strap with a great
ring and chain across his chest, dark gauntlets and greaves, a belt and a torn red tabard). His two oversized hooked
cleavers, stone-grey slabs bolted with bronze, burn molten along their edges, molten slag dripping from them. A living
animal, biological to the last scale.

Low poly and flat-coloured (author 2026-10-07): the big forms that make his outline, each a flat colour the toon
material shades. He stands as his archetype lays him out (hunched in his clips), a cleaver in each hand on its prop
bone, pointing forward with its blade hanging below as the generated body held them."""
import numpy as np

from ..sculpt import anatomy, garments, sdf, tree
from ..sculpt.anatomy import V, unit
from ..sculpt.garments import around, band_z, both
from ..sculpt.tree import Box, Over, Placed, Shell, Union, Zone

HAND_SHARE = 0.12

# Flat colours tuned against his splash art (sRGB), its warm night light lifted so the darks still read in game.
PALETTE = {
    "scale": (0.62, 0.2, 0.16), "belly": (0.9, 0.84, 0.74), "horn": (0.32, 0.24, 0.21), "claw": (0.88, 0.83, 0.72),
    "spine": (0.19, 0.16, 0.17), "iron": (0.3, 0.27, 0.26), "bronze": (0.72, 0.53, 0.3), "stone": (0.58, 0.55, 0.52),
    "leather": (0.42, 0.24, 0.15), "tabard": (0.42, 0.09, 0.11), "molten": (1.0, 0.5, 0.12), "eye": (1.0, 0.45, 0.1),
    "clouded": (0.8, 0.82, 0.8), "teeth": (0.94, 0.9, 0.82),
}


def materials(S):
    return {name: S.material(name, colour, glow=name in ("molten", "eye")) for name, colour in PALETTE.items()}


def polygon(Q, points):
    """Signed distance from 2D points Q (n, 2) to a closed polygon (k, 2), negative inside."""
    pts = np.asarray(points, dtype=np.float64)
    Q = np.asarray(Q, dtype=np.float64)
    d = np.sum((Q - pts[0]) ** 2, axis=1)
    s = np.ones(len(Q))
    j = len(pts) - 1
    for i in range(len(pts)):
        e = pts[j] - pts[i]
        w = Q - pts[i]
        b = w - np.outer(np.clip((w @ e) / max(e @ e, 1e-12), 0.0, 1.0), e)
        d = np.minimum(d, np.sum(b * b, axis=1))
        c1, c2 = Q[:, 1] >= pts[i][1], Q[:, 1] < pts[j][1]
        c3 = e[0] * w[:, 1] > e[1] * w[:, 0]
        s = np.where((c1 & c2 & c3) | (~c1 & ~c2 & ~c3), -s, s)
        j = i
    return s * np.sqrt(d)


def pane(P, origin, ax_u, ax_v, points, half):
    """A flat plate: the polygon points (in the u, v plane through origin) extruded half either side."""
    Q = np.asarray(P, dtype=np.float64) - origin
    d2 = polygon(np.stack([Q @ ax_u, Q @ ax_v], axis=1), points)
    t = np.abs(Q @ unit(np.cross(ax_u, ax_v))) - half
    w = np.stack([d2, t], axis=1)
    return (np.minimum(np.max(w, axis=1), 0.0) + np.linalg.norm(np.maximum(w, 0.0), axis=1)).astype(np.float32)


def build(S, L, dims, spec):
    """Gorraveth's sculpt on his layout: (the whole, {"body": the scaled skin, under his armour, "sheets": none})."""
    mats = materials(S)
    H = dims["height"]
    figure = anatomy.Figure(S, L, dims, mats["scale"], {"muscle": 1.0, "chest": 1.35, "breadth": 1.35, "hips": 1.15, "limb": 2.0,
                                                         "deltoid": 1.55, "leg": 1.4, "neck": 1.6}).build()
    # His legs are a beast's, on its toes: the figure's are replaced by thigh, shank and a long foot along his bones.
    for side in ("l", "r"):
        figure.parts.remove(figure.limbs["leg_" + side])
        figure.limbs["leg_" + side] = digitigrade_leg(S, L, H, side, mats)
        figure.parts.append(figure.limbs["leg_" + side])
    figure.attach_head(head(S, L, dims, mats))
    for side in ("l", "r"):
        figure.limbs["hand_" + side] = hand(S, L, H, side, mats, figure)
    for part in tail(S, L, H, mats) + claws(S, L, H, mats):
        figure.parts.append(part)
    body = figure.body()
    armoured = armour(S, L, dims, mats, body, figure.limbs)
    worn = Over([armoured, spines(S, L, H, mats)] + [cleaver(S, L, H, mats, side) for side in ("l", "r")])
    # Cut flat where he meets the ground: nothing of him sinks below it.
    worn = tree.Intersect(worn, Zone(lambda P: -P[:, 2], Box(V(-500, -500, -0.5), V(500, 500, 600))))
    return worn, {"body": body, "sheets": []}


def head(S, L, dims, mats):
    """A crocodilian head in the head's frame (x forward, z up): a heavy skull, a long snout over a slung jaw full of
    teeth in a scarred snarl, slag hanging from it; a chipped horn rising back on its right and a broken stump on its
    left; a burning eye and a clouded one; a torn frill fanning from behind the jaw."""
    o = V(*L["head"][0])
    # Larger than a man's head on his frame, as his art draws it.
    s = (L["head"][1][2] - L["head"][0][2]) * 1.35
    bones = anatomy.rigid("head")
    sk = mats["scale"]
    parts = []

    def leaf(name, distance, lo, hi, material, protect=0.6):
        parts.append(tree.leaf(S, name, distance, Box(lo, hi), material, bones, protect))

    leaf("skull", lambda P: sdf.ellipsoid(P, V(0.0, 0, 0.42) * s, V(0.42, 0.36, 0.34) * s), V(-0.5, -0.45, 0) * s, V(0.5, 0.45, 0.8) * s, sk)
    leaf("snout", lambda P: sdf.round_cone(P, V(0.25, 0, 0.42) * s, V(1.05, 0, 0.32) * s, 0.24 * s, 0.13 * s), V(0, -0.3, 0.1) * s, V(1.25, 0.3, 0.7) * s, sk)
    leaf("jaw", lambda P: sdf.round_cone(P, V(0.15, 0, 0.18) * s, V(0.95, 0, 0.12) * s, 0.2 * s, 0.1 * s), V(-0.1, -0.3, -0.1) * s, V(1.1, 0.3, 0.4) * s, mats["belly"])
    for k, x in enumerate(np.linspace(0.45, 0.95, 5)):
        for side in (1.0, -1.0):
            a = V(x, side * (0.16 - 0.08 * (x - 0.45)), 0.25) * s
            leaf("tooth_%d_%d" % (k, side > 0), lambda P, a=a: sdf.round_cone(P, a, a - V(0, 0, 0.09) * s, 0.035 * s, 0.006 * s),
                 a - 0.15 * s, a + 0.15 * s, mats["teeth"], 0.8)
    # The great dark horn on his right, sweeping back and up from his brow and hooking forward at its chipped tip; the
    # broken stump on his left.
    pts = [V(0.1, -0.25, 0.66), V(-0.14, -0.33, 0.98), V(-0.32, -0.33, 1.3), V(-0.24, -0.28, 1.56), V(0.0, -0.24, 1.66)]
    radii = [0.17, 0.14, 0.1, 0.06, 0.02]
    leaf("horn", lambda P: sdf.tube(P, [p * s for p in pts], [r * s for r in radii]), V(-0.55, -0.55, 0.5) * s, V(0.25, -0.05, 2.0) * s, mats["horn"], 0.8)
    leaf("stump", lambda P: sdf.cylinder(P, V(0.05, 0.25, 0.68) * s, V(0.0, 0.3, 0.86) * s, 0.1 * s, 0.02 * s), V(-0.2, 0.05, 0.5) * s, V(0.3, 0.5, 1.0) * s, mats["horn"], 0.8)
    leaf("eye_burning", lambda P: sdf.sphere(P, V(0.36, -0.22, 0.55) * s, 0.06 * s), V(0.2, -0.35, 0.4) * s, V(0.5, -0.1, 0.7) * s, mats["eye"], 1.0)
    leaf("eye_clouded", lambda P: sdf.sphere(P, V(0.36, 0.22, 0.55) * s, 0.06 * s), V(0.2, 0.1, 0.4) * s, V(0.5, 0.35, 0.7) * s, mats["clouded"], 1.0)
    # The torn frill: spikes fanning back from behind the jaw on both sides, some broken short.
    for k, (angle, length) in enumerate(((-60, 0.55), (-25, 0.7), (15, 0.6), (50, 0.35))):
        for side in (1.0, -1.0):
            a = V(-0.15, side * 0.3, 0.35) * s
            r = np.radians(angle)
            b = a + V(-np.cos(r) * 0.5, side * 0.45, np.sin(r) * 0.6) * s * length
            leaf("frill_%d_%d" % (k, side > 0), lambda P, a=a, b=b: sdf.round_cone(P, a, b, 0.07 * s, 0.01 * s), np.minimum(a, b) - 0.1 * s,
                 np.maximum(a, b) + 0.1 * s, mats["spine"], 0.4)
    # The ragged black fringe of his mane hanging from under his jaw and cheeks.
    for k, (x, y, length) in enumerate(((0.55, 0.0, 0.42), (0.35, 0.18, 0.5), (0.35, -0.18, 0.5), (0.12, 0.26, 0.55), (0.12, -0.26, 0.55),
                                        (-0.1, 0.22, 0.45), (-0.1, -0.22, 0.45))):
        a = V(x, y, 0.12) * s
        b = a + V(-0.12, y * 0.4, -length) * s
        leaf("fringe_%d" % k, lambda P, a=a, b=b: sdf.round_cone(P, a, b, 0.08 * s, 0.012 * s), np.minimum(a, b) - 0.12 * s, np.maximum(a, b) + 0.12 * s,
             mats["spine"], 0.4)
    return Placed(Union(parts, k=0.04 * s), o, np.eye(3))


def digitigrade_leg(S, L, H, side, mats):
    """A beast's leg on its toes along side's bones: a massive thigh, a shank back to the high hock, and a long foot down
    to the ground, each a thick tapered form blended into the next."""
    hp, k = V(*L["thigh_" + side][0]), V(*L["thigh_" + side][1])
    hock = V(*L["calf_" + side][1])
    toe = V(*L["foot_" + side][1])
    toe = V(toe[0], toe[1], H * 0.02)
    parts = [tree.leaf(S, "thigh_" + side, lambda P: sdf.round_cone(P, hp + V(0, 0, H * 0.02), k, H * 0.1, H * 0.065), Box.around([hp, k], H * 0.12),
                       mats["scale"], anatomy.along("pelvis", "thigh_" + side, hp + V(0, 0, H * 0.05), hp - V(0, 0, H * 0.06), 0.2, 0.7)),
             tree.leaf(S, "shank_" + side, lambda P: sdf.round_cone(P, k, hock, H * 0.06, H * 0.035), Box.around([k, hock], H * 0.07),
                       mats["scale"], anatomy.rigid("calf_" + side)),
             tree.leaf(S, "foot_" + side, lambda P: sdf.round_cone(P, hock, toe, H * 0.036, H * 0.028), Box.around([hock, toe], H * 0.045),
                       mats["scale"], anatomy.rigid("foot_" + side))]
    return Union(parts, k=H * 0.025)


def hand(S, L, H, side, mats, figure):
    """A big hand in a dark gauntlet, closed on a cleaver's haft, which points forward out of it."""
    w0, w1 = V(*L["hand_" + side][0]), V(*L["hand_" + side][1])
    sign = 1.0 if side == "l" else -1.0
    x = unit(w1 - w0)
    out = V(0, sign, 0)
    z = unit(out - x * (out @ x))
    y = np.cross(z, x)
    curl = {"index": (70, 90, 50), "middle": (75, 95, 50), "ring": (78, 95, 50), "little": (80, 95, 45)}
    built = anatomy.Hand(S, mats["iron"], side, H * HAND_SHARE, "hand_" + side, curl=curl, thumb=(60, 30, 30, 25)).build()
    placed = Placed(built, w0, np.stack([x, y, z], axis=1))
    figure.parts.append(placed)
    return placed


def tail(S, L, H, mats):
    """His heavy tail along its bones, thick at the root and tapering to a point, its spines along the top."""
    joints = [V(*L["tail_01"][0]), V(*L["tail_02"][0]), V(*L["tail_03"][0]), V(*L["tail_03"][1])]
    radii = [H * 0.075, H * 0.055, H * 0.035, H * 0.008]
    parts = []
    for k in range(3):
        a, b, ra, rb = joints[k], joints[k + 1], radii[k], radii[k + 1]
        parts.append(tree.leaf(S, "tail_%d" % k, lambda P, a=a, b=b, ra=ra, rb=rb: sdf.round_cone(P, a, b, ra, rb), Box.around([a, b], max(ra, rb)),
                               mats["scale"], anatomy.rigid("tail_%02d" % (k + 1))))
        for j in range(2):
            c = a + (b - a) * (0.3 + 0.4 * j) + V(0, 0, (ra + rb) * 0.5 * 0.9)
            tip = c + V(-H * 0.01, 0, (ra + rb) * 0.45)
            parts.append(tree.leaf(S, "tail_spine_%d_%d" % (k, j), lambda P, c=c, tip=tip, r=(ra + rb) * 0.18: sdf.round_cone(P, c, tip, r, r * 0.1),
                                   Box.around([c, tip], (ra + rb) * 0.3), mats["spine"], anatomy.rigid("tail_%02d" % (k + 1)), protect=0.4))
    return parts


def claws(S, L, H, mats):
    """Three pale claws at each foot's toe."""
    parts = []
    for side in ("l", "r"):
        f0, f1 = V(*L["foot_" + side][0]), V(*L["foot_" + side][1])
        for k, spread in enumerate((-1.0, 0.0, 1.0)):
            a = f1 + V(-H * 0.01, spread * H * 0.022, H * 0.01)
            b = a + V(H * 0.045, spread * H * 0.012, -H * 0.012)
            b[2] = max(b[2], H * 0.004)
            parts.append(tree.leaf(S, "claw_%s_%d" % (side, k), lambda P, a=a, b=b: sdf.round_cone(P, a, b, H * 0.012, H * 0.002), Box.around([a, b], H * 0.015),
                                   mats["claw"], anatomy.rigid("foot_" + side), protect=0.6))
    return parts


def spines(S, L, H, mats):
    """His black mane of spikes: a crest running from the back of his skull down his neck and his back, the spikes
    longest over his shoulders, with shorter ones fanning out over each shoulder."""
    parts = []
    pelvis, chest = V(*L["pelvis"][0]), V(*L["spine_03"][1])
    nape = V(*L["head"][0])
    # The crest: down the back of the neck from the skull, then down the back.
    path = [nape + V(-H * 0.05, 0, H * 0.05), chest + V(-H * 0.07, 0, H * 0.02)] + [pelvis + (chest - pelvis) * f + V(-H * 0.085, 0, 0) for f in (0.75, 0.5, 0.25)]
    sizes = [0.7, 1.0, 0.9, 0.7, 0.5]
    for k, (c, size) in enumerate(zip(path, sizes)):
        bone = "head" if k == 0 else ("spine_03" if k < 3 else ("spine_02" if k == 3 else "spine_01"))
        for j, ahead in enumerate((0.0, 0.4)):
            if k == len(path) - 1 and j:
                continue
            base = c + (path[min(k + 1, len(path) - 1)] - c) * ahead
            tip = base + V(-H * 0.07, 0, H * 0.06 * (1.0 - 0.2 * j)) * size
            parts.append(tree.leaf(S, "crest_%d_%d" % (k, j), lambda P, a=base, b=tip, r=H * 0.026 * size: sdf.round_cone(P, a, b, r, r * 0.12),
                                   Box.around([base, tip], H * 0.04), mats["spine"], anatomy.rigid(bone), protect=0.4))
    # Fanning out over each shoulder.
    for side, sign in (("l", 1.0), ("r", -1.0)):
        s = V(*L["upperarm_" + side][0])
        for j, share in enumerate((0.35, 0.7)):
            base = chest + (s - chest) * share + V(-H * 0.04, 0, H * 0.03)
            tip = base + V(-H * 0.05, sign * H * 0.03, H * 0.055)
            parts.append(tree.leaf(S, "mane_%s_%d" % (side, j), lambda P, a=base, b=tip: sdf.round_cone(P, a, b, H * 0.022, H * 0.003),
                                   Box.around([base, tip], H * 0.035), mats["spine"], anatomy.rigid("clavicle_" + side), protect=0.4))
    return Union(parts, k=0.3)


def armour(S, L, dims, mats, body, limbs):
    """Mismatched scorched mining armour over his scales, his cream throat and chest bare: bronze-bound pauldrons on
    both shoulders, a broad bronze harness strap from his right shoulder across his chest past a great ring at his
    breast, a chain looped down from the ring, dark gauntlets to the elbow and greaves on his shanks, bronze knee plates,
    a belt and a torn red tabard hanging before him."""
    H = dims["height"]
    pz = L["pelvis"][0][2]
    cz = L["spine_03"][1][2]
    torso = cz - pz
    belly = Shell(S, "belly", body, 0.0, 0.3, both(Zone(lambda P: H * 0.015 - P[:, 0] + np.abs(P[:, 1]) * 0.3, Box((-60, -60, 0), (80, 60, 300))),
                                                  band_z(pz - H * 0.02, cz + H * 0.12), garments.keep_to(limbs, ["torso"])), mats["belly"], hem=0.4)
    skin = Over([body, belly])
    parts = []
    for side, sign in (("l", 1.0), ("r", -1.0)):
        s = V(*L["upperarm_" + side][0])
        # Mismatched: a heavier stack on his right than his left.
        layers, scale = (3, 1.0) if side == "r" else (2, 0.9)
        for layer in range(layers):
            c = s + V(-H * 0.006 * layer, sign * H * 0.012 * layer, H * 0.045 - H * 0.024 * layer)
            r = H * (0.09 - layer * 0.012) * scale
            parts.append(tree.leaf(S, "pauldron_%s_%d" % (side, layer), lambda P, c=c, r=r: sdf.ellipsoid(P, c, V(r * 1.15, r, r * 0.5)), Box(c - r * 1.3, c + r * 1.3),
                                   mats["bronze"] if layer == 0 else mats["iron"],
                                   anatomy.along("clavicle_" + side, "upperarm_" + side, s - V(0, sign * H * 0.06, 0), s + V(0, sign * H * 0.06, 0), 0.3, 0.7), protect=0.5))
    shells = []
    for side in ("l", "r"):
        e, w = V(*L["lowerarm_" + side][0]), V(*L["lowerarm_" + side][1])
        region = both(around([w, e], [9.0, 10.0]), garments.keep_to(limbs, ["forearm_" + side]))
        shells.append(Shell(S, "gauntlet_" + side, skin, 0.5, 1.4, region, mats["iron"], hem=0.4, reach=0.5))
        k = V(*L["calf_" + side][0])
        parts.append(tree.leaf(S, "knee_plate_" + side, lambda P, c=k + V(H * 0.04, 0, 0): sdf.ellipsoid(P, c, V(H * 0.03, H * 0.05, H * 0.055)),
                               Box(k - H * 0.08, k + H * 0.09), mats["bronze"], anatomy.rigid("calf_" + side), protect=0.4))
        hock = V(*L["calf_" + side][1])
        region = both(around([k, hock], [11.0, 9.0]), garments.keep_to(limbs, ["leg_" + side]))
        shells.append(Shell(S, "greave_" + side, skin, 0.4, 1.2, region, mats["iron"], hem=0.4, reach=0.5))
    # The harness strap from his right shoulder down across his chest, past the ring at his breast, to his left hip.
    shoulder = abs(L["upperarm_l"][0][1])
    a, b = V(0, -shoulder, cz - 1.0), V(0, shoulder * 0.7, pz + H * 0.04)
    across = unit(np.cross(b - a, V(1, 0, 0)))
    strap = both(garments.band_plane((a + b) / 2, across, H * 0.05, Box((-60, -80, pz), (60, 80, cz + 10))), garments.keep_to(limbs, ["torso"]))
    shells.append(Shell(S, "harness", skin, 0.4, 1.2, strap, mats["bronze"], hem=0.3, reach=0.5))
    ring = garments.surface_point(body, V(0, 0, pz + torso * 0.6), V(1, 0, 0))[0] + V(H * 0.012, 0, 0)
    facing = np.stack([V(0, 1, 0), V(0, 0, 1), V(1, 0, 0)], axis=1)
    parts.append(tree.leaf(S, "harness_ring", lambda P: sdf.torus(P, ring, H * 0.035, H * 0.011, facing), Box(ring - H * 0.06, ring + H * 0.06),
                           mats["bronze"], anatomy.rigid("spine_03"), protect=0.6))
    belt_z = pz + torso * 0.08
    shells.append(Shell(S, "belt", skin, 0.4, 1.3, both(band_z(belt_z - H * 0.022, belt_z + H * 0.022), garments.keep_to(limbs, ["torso"])), mats["leather"], hem=0.3))
    # The chain looped down from the ring across his belly to his left hip.
    hip = abs(L["thigh_l"][0][1])
    end = garments.surface_point(body, V(0, hip * 1.2, belt_z + H * 0.03), V(1, 0.4, 0))[0]
    chain_bones = anatomy.along("spine_03", "pelvis", ring, end, 0.2, 0.8)
    for k in range(7):
        f = k / 6.0
        c = ring + (end - ring) * f - V(0, 0, H * 0.05 * np.sin(np.pi * f)) + V(H * 0.02 * np.sin(np.pi * f), 0, 0)
        axes = np.stack([V(1, 0, 0), V(0, 1, 0), V(0, 0, 1)], axis=1) if k % 2 else np.stack([V(0, 1, 0), V(0, 0, 1), V(1, 0, 0)], axis=1)
        parts.append(tree.leaf(S, "chain_%d" % k, lambda P, c=c, axes=axes: sdf.torus(P, c, H * 0.016, H * 0.005, axes), Box(c - H * 0.03, c + H * 0.03),
                               mats["iron"], chain_bones, protect=0.4))
    # The torn red tabard hanging before him from the belt, toward his knees, its hem ragged.
    top = garments.surface_point(body, V(0, 0, belt_z - H * 0.01), V(1, 0, 0))[0] + V(H * 0.012, 0, 0)
    rag = [(-0.075, 0.0), (0.075, 0.0), (0.07, 0.12), (0.05, 0.19), (0.03, 0.14), (0.01, 0.22), (-0.02, 0.15), (-0.045, 0.2), (-0.07, 0.11)]
    rag = [(u * H, v * H) for u, v in rag]
    hang = unit(V(0.15, 0, -1))
    parts.append(tree.leaf(S, "tabard", lambda P: pane(P, top, V(0, 1, 0), hang, rag, H * 0.006), Box(top - V(H * 0.06, H * 0.09, H * 0.24), top + V(H * 0.08, H * 0.09, H * 0.02)),
                           mats["tabard"], anatomy.rigid("pelvis"), protect=0.3))
    return Over([skin] + shells + [Union(parts, k=0.3)])


def cleaver(S, L, H, mats, side):
    """An oversized hooked slag cleaver on side's prop bone, as the generated body holds it: a bronze-bound haft forward
    from the fist, a huge broad stone-grey blade bolted to it below with bronze bolts, its back hooked forward and up at
    the tip, its cutting edge burning molten and molten slag dripping from it."""
    bones = anatomy.rigid("prop_" + side)
    grip = V(*L["prop_" + side][0])
    forward, down, across = V(1, 0, 0), V(0, 0, -1), V(0, 1, 0)
    root = grip + forward * H * 0.1
    at = lambda ahead, below: root + forward * H * ahead + down * H * below  # noqa: E731
    parts = [tree.leaf(S, "haft_" + side, lambda P: sdf.cylinder(P, grip - forward * H * 0.05, root, H * 0.018, H * 0.004), Box.around([grip, root], H * 0.07),
                       mats["leather"], bones, protect=0.8),
             tree.leaf(S, "ferrule_" + side, lambda P: sdf.cylinder(P, root - forward * H * 0.02, root + forward * H * 0.01, H * 0.026, H * 0.003),
                       Box.around([root], H * 0.04), mats["bronze"], bones, protect=0.8)]
    outline = [(0.0, -0.035), (0.22, -0.05), (0.34, -0.11), (0.31, -0.02), (0.33, 0.07), (0.26, 0.14), (0.08, 0.13), (0.0, 0.07)]
    pts = [(a * H, b * H) for a, b in outline]
    parts.append(tree.leaf(S, "blade_" + side, lambda P: pane(P, root, forward, down, pts, H * 0.008), Box(root - V(H * 0.02, H * 0.02, H * 0.16), root + V(H * 0.36, H * 0.02, H * 0.12)),
                           mats["stone"], bones, protect=1.0))
    for k, (ahead, below) in enumerate(((0.035, -0.01), (0.035, 0.05), (0.15, -0.025))):
        c = at(ahead, below)
        parts.append(tree.leaf(S, "bolt_%s_%d" % (side, k), lambda P, c=c: sdf.cylinder(P, c - across * H * 0.013, c + across * H * 0.013, H * 0.014, H * 0.003),
                               Box(c - H * 0.02, c + H * 0.02), mats["bronze"], bones, protect=0.8))
    edge = [at(0.33, 0.075), at(0.26, 0.145), at(0.08, 0.135), at(0.005, 0.075)]
    for k, (a, b) in enumerate(zip(edge, edge[1:])):
        parts.append(tree.leaf(S, "edge_%s_%d" % (side, k), lambda P, a=a, b=b: sdf.capsule(P, a, b, H * 0.009), Box.around([a, b], H * 0.013),
                               mats["molten"], bones, protect=1.0))
    # Molten slag dripping from the edge.
    for k, (ahead, length) in enumerate(((0.12, 0.06), (0.2, 0.09), (0.28, 0.05))):
        drip = at(ahead, 0.142 - 0.012 * k)
        tip = drip + down * H * length
        parts.append(tree.leaf(S, "drip_%s_%d" % (side, k), lambda P, a=drip, b=tip: sdf.round_cone(P, a, b, H * 0.009, H * 0.004),
                               Box.around([drip, tip], H * 0.014), mats["molten"], bones, protect=0.6))
    return Union(parts, k=0.1)
