"""Silt, The Living Mire (ADR-069): a colossus of wet sediment, read by his silhouette from the game camera. Character
Bible §3 and the author's reference (2026-10-07): a towering mass of golden-ochre sediment, clay and riverbed debris,
hunched and forward-leaning on long forelimbs whose clawed digits reach the ground; thick legs on broad clawed feet. His
surface is layered and flaking: plate-like sheets of drying sediment shingled over a darker, saturated interior, peeling
at their edges. He is caught mid-motion, ribbons of wet material flung up off his shoulders and rivulets dripping from
him. He has no face, no head shape anywhere: the top of him is a ridge of sediment between the shoulders.

Low poly and flat-coloured (author 2026-10-07): the big forms that make his outline, each a flat colour the toon
material shades. He rests as his archetype lays him out (ADR-064), forelimbs to the ground; his clips knuckle-walk and
throw. Colours are sampled from the reference (sRGB)."""
import numpy as np

from ..sculpt import anatomy, sdf, sheet, tree
from ..sculpt.anatomy import V, unit
from ..sculpt.tree import Box, Over, Shell, Union, Zone

# Sampled from the reference (sRGB): the dark wet interior, the drying flakes over it, the sun-bleached flakes, the
# claws' riverbed stone and the wet material flung off him.
PALETTE = {"core": (0.4, 0.3, 0.2), "flake": (0.76, 0.6, 0.4), "bleached": (0.88, 0.75, 0.54), "stone": (0.48, 0.4, 0.32),
           "wet": (0.8, 0.66, 0.45)}


def mix(a, b, t):
    return a + (b - a) * t


# How much taller than wide his plates are: sheets of wet sediment sliding down him, not stones.
PLATE_STRETCH = 2.4


def plates(P, cell, seed):
    """How far each point lies inside its plate: a cellular pattern of jittered points cell apart (PLATE_STRETCH times
    as far up and down), each point's plate the nearest one, measured as the gap between its nearest and second-nearest
    (zero on a crack between plates)."""
    P = np.asarray(P, dtype=np.float64) / cell * np.array([1.0, 1.0, 1.0 / PLATE_STRETCH])
    base = np.floor(P)
    first = np.full(len(P), np.inf)
    second = np.full(len(P), np.inf)
    for dx in (-1, 0, 1):
        for dy in (-1, 0, 1):
            for dz in (-1, 0, 1):
                c = base + np.array([dx, dy, dz])
                h = np.sin(c @ np.array([127.1, 311.7, 74.7]) + seed) * 43758.5453
                jitter = np.stack([np.modf(np.abs(h * k))[0] for k in (1.0, 1.31, 1.73)], axis=1)
                d = np.linalg.norm(P - (c + jitter), axis=1)
                second = np.where(d < first, first, np.minimum(second, d))
                first = np.minimum(first, d)
    return (second - first) * cell


def materials(S):
    return {name: S.material(name, colour) for name, colour in PALETTE.items()}


def build(S, L, dims, spec):
    """Silt's sculpt on his layout: (the whole, {"body": his interior, under the flakes, "sheets": what he flings}).
    Plates are cut in a cellular pattern (plates), so the cracks between them, where his wet interior shows, run as the
    material slides."""
    mats = materials(S)
    H = dims["height"]
    p = lambda bone, i: V(*L[bone][i])  # noqa: E731
    parts = []

    def cone(name, a, b, ra, rb, bones, material=None):
        node = tree.leaf(S, name, lambda P, a=a, b=b: sdf.round_cone(P, a, b, ra, rb), Box.around([a, b], max(ra, rb)), material or mats["core"], bones)
        parts.append(node)
        return node

    def blob(name, c, r, bones, axes=None, material=None):
        node = tree.leaf(S, name, lambda P, c=c, r=V(*r): sdf.ellipsoid(P, c, r, axes), Box(c - max(r), c + max(r)), material or mats["core"], bones)
        parts.append(node)
        return node

    pelvis, chest = p("pelvis", 0), p("spine_03", 1)
    shoulders = {side: p("upperarm_" + side, 0) for side in ("l", "r")}
    spine = anatomy.along("pelvis", "spine_03", pelvis, chest, 0.2, 0.8)
    # The trunk: a great forward-leaning mass, broadest across the shoulders, its belly hanging low.
    cone("trunk", pelvis + V(0, 0, H * 0.02), chest, H * 0.11, H * 0.17, spine)
    blob("belly", mix(pelvis, chest, 0.35) + V(H * 0.04, 0, -H * 0.02), (H * 0.1, H * 0.11, H * 0.1), anatomy.rigid("spine_01"))
    for side, sign in (("l", 1.0), ("r", -1.0)):
        s = shoulders[side]
        blob("shoulder_" + side, s + V(-H * 0.02, -sign * H * 0.01, H * 0.03), (H * 0.11, H * 0.1, H * 0.11),
             anatomy.along("spine_03", "upperarm_" + side, chest, s, 0.3, 0.8))
    # The top of him: a ridge of sediment heaped between the shoulders and peaking forward, where a head would be and is not.
    ridge_base, ridge_top = chest + V(-H * 0.02, 0, -H * 0.02), chest + V(H * 0.06, 0, H * 0.12)
    cone("ridge", ridge_base, ridge_top, H * 0.12, H * 0.025, anatomy.along("spine_03", "neck_01", ridge_base, ridge_top, 0.4, 0.9))
    # Arms: thick, the forearms heavier than the upper arms, reaching the ground.
    for side in ("l", "r"):
        s, e, w, h1 = p("upperarm_" + side, 0), p("upperarm_" + side, 1), p("lowerarm_" + side, 1), p("hand_" + side, 1)
        cone("upperarm_" + side, s, e, H * 0.075, H * 0.068, anatomy.rigid("upperarm_" + side))
        cone("forearm_" + side, e, w, H * 0.085, H * 0.07, anatomy.rigid("lowerarm_" + side))
        hand(S, mats, parts, side, w, h1, H)
    # Legs: thick and short, on broad clawed feet.
    for side in ("l", "r"):
        hp, k, a, toe = p("thigh_" + side, 0), p("thigh_" + side, 1), p("calf_" + side, 1), p("foot_" + side, 1)
        cone("thigh_" + side, hp, k, H * 0.1, H * 0.085, anatomy.along("pelvis", "thigh_" + side, hp + V(0, 0, H * 0.05), hp - V(0, 0, H * 0.06), 0.2, 0.7))
        cone("calf_" + side, k, a, H * 0.085, H * 0.075, anatomy.rigid("calf_" + side))
        foot(S, mats, parts, side, a, toe, H)
    core = Union(parts, k=H * 0.03)
    # Flakes: plate-like sheets of drying sediment over the interior, the dark wet sediment showing in the cracks between
    # them; some plates sun-bleached, standing a little prouder.
    everywhere = Box(V(-500, -500, -50), V(500, 500, 500))
    flakes = Shell(S, "flakes", core, 0.0, H * 0.02, Zone(lambda P: H * 0.012 - plates(P, H * 0.11, 31.0), everywhere), mats["flake"], hem=H * 0.004, reach=H * 0.002)
    shingled = Over([core, flakes])
    bleached = Shell(S, "bleached", shingled, 0.0, H * 0.012,
                     Zone(lambda P: np.maximum(H * 0.02 - plates(P, H * 0.11, 31.0), 0.08 - (sdf.fbm(P.astype(np.float32), H * 0.15, 2, 47) - 0.5) * H * 0.2), everywhere),
                     mats["bleached"], hem=H * 0.004)
    surface = Over([shingled, bleached])
    worn = Over([surface, drips(S, L, mats, H)])
    # Cut flat where he meets the ground: nothing of him sinks below it.
    worn = tree.Intersect(worn, Zone(lambda P: -P[:, 2], Box(V(-500, -500, -0.5), V(500, 500, 600))))
    return worn, {"body": core, "sheets": [flung(S, L, dims, mats, side) for side in ("l", "r")]}


def hand(S, mats, parts, side, wrist, end, H):
    """A great hand on the ground: a heavy palm and four long tapering claws of riverbed stone, splayed forward."""
    bones = anatomy.rigid("hand_" + side)
    down = unit(end - wrist)
    sign = 1.0 if side == "l" else -1.0
    palm = wrist + down * H * 0.04
    parts.append(tree.leaf(S, "palm_" + side, lambda P, c=palm: sdf.ellipsoid(P, c, V(H * 0.07, H * 0.075, H * 0.06)), Box(palm - H * 0.09, palm + H * 0.09),
                           mats["core"], bones))
    for i, spread in enumerate((-0.65, -0.22, 0.22, 0.65)):
        # Long tapering digits from the front of the palm, curving down to the ground ahead of it.
        root = palm + V(H * 0.035, sign * spread * H * 0.065, -H * 0.01)
        knuckle = root + V(H * 0.07, sign * spread * H * 0.03, -H * 0.01)
        tip = knuckle + V(H * 0.07, sign * spread * H * 0.02, 0.0)
        tip[2] = H * 0.006
        knuckle[2] = max(knuckle[2], H * 0.04)
        parts.append(tree.leaf(S, "digit_%s_%d" % (side, i), lambda P, a=root, b=knuckle: sdf.round_cone(P, a, b, H * 0.03, H * 0.022), Box.around([root, knuckle], H * 0.035),
                               mats["core"], bones))
        parts.append(tree.leaf(S, "claw_%s_%d" % (side, i), lambda P, a=knuckle, b=tip: sdf.round_cone(P, a, b, H * 0.022, H * 0.004), Box.around([knuckle, tip], H * 0.03),
                               mats["stone"], bones))


def foot(S, mats, parts, side, ankle, toe, H):
    """A broad foot: a heavy pad and three clawed toes."""
    bones = anatomy.rigid("foot_" + side)
    pad = ankle + (toe - ankle) * 0.4
    pad[2] = H * 0.045
    parts.append(tree.leaf(S, "pad_" + side, lambda P, c=pad: sdf.ellipsoid(P, c, V(H * 0.1, H * 0.08, H * 0.045)), Box(pad - H * 0.11, pad + H * 0.11),
                           mats["core"], bones))
    for i, spread in enumerate((-0.6, 0.0, 0.6)):
        root = pad + V(H * 0.06, spread * H * 0.06, 0.0)
        tip = root + V(H * 0.07, spread * H * 0.03, -H * 0.025)
        tip[2] = max(tip[2], H * 0.004)
        parts.append(tree.leaf(S, "toe_%s_%d" % (side, i), lambda P, a=root, b=tip: sdf.round_cone(P, a, b, H * 0.03, H * 0.005), Box.around([root, tip], H * 0.035),
                               mats["stone"], bones))


def drips(S, L, mats, H):
    """Rivulets of wet sediment hanging from him: from under his forearms, his belly and his shoulders, tapering to drops."""
    rng = np.random.default_rng(301)
    nodes = []
    anchors = []
    for side in ("l", "r"):
        e, w = V(*L["lowerarm_" + side][0]), V(*L["lowerarm_" + side][1])
        anchors += [("lowerarm_" + side, mix(e, w, share) + V(0, (1 if side == "l" else -1) * H * 0.05, 0)) for share in (0.15, 0.4, 0.62, 0.85)]
        s, el = V(*L["upperarm_" + side][0]), V(*L["upperarm_" + side][1])
        anchors += [("upperarm_" + side, mix(s, el, share) + V(0, (1 if side == "l" else -1) * H * 0.05, 0)) for share in (0.45, 0.8)]
        hp, k = V(*L["thigh_" + side][0]), V(*L["thigh_" + side][1])
        anchors += [("thigh_" + side, mix(hp, k, share) + V(H * 0.07, 0, 0)) for share in (0.3, 0.6)]
    pelvis, chest = V(*L["pelvis"][0]), V(*L["spine_03"][1])
    anchors += [("spine_01", mix(pelvis, chest, share) + V(H * 0.14, sign * H * 0.07, -H * 0.04)) for share in (0.25, 0.5) for sign in (1.0, -1.0)]
    for i, (bone, top) in enumerate(anchors):
        length = H * rng.uniform(0.08, 0.18)
        bottom = top - V(0, 0, length) + V(rng.uniform(-1, 1), rng.uniform(-1, 1), 0) * H * 0.01
        nodes.append(tree.leaf(S, "drip_%d" % i, lambda P, a=top, b=bottom, r=H * rng.uniform(0.014, 0.022): sdf.round_cone(P, a, b, r, r * 0.2),
                               Box.around([top, bottom], H * 0.03), mats["wet"], anatomy.rigid(bone), protect=0.5))
    return Union(nodes, k=H * 0.01)


def flung(S, L, dims, mats, side):
    """A broad ribbon of wet material flung up and back off one shoulder, arcing over and trailing behind him, torn into
    tongues at its end: he is always throwing himself through the air."""
    H = dims["height"]
    sign = 1.0 if side == "l" else -1.0
    material = S.material("flung_" + side, PALETTE["wet"])
    s = V(*L["upperarm_" + side][0])
    root = s + V(-H * 0.09, -sign * H * 0.02, H * 0.03)
    columns, rows, strips = 6, 8, 3

    def position(u, v):
        v = v * sheet.torn(u, strips, 0.5, 0.25, 21.0 + sign)
        across = (u - 0.5) * H * 0.1
        # Off the back of the shoulder, swept back and out and falling away behind him, low enough never to stand
        # above him like horns.
        reach = H * 0.4 * v
        p = np.stack([np.full_like(u, root[0]), np.full_like(u, root[1]), np.full_like(u, root[2])], axis=1)
        p[:, 0] += -reach * 0.85 - across * 0.15
        p[:, 1] += sign * (across + reach * 0.5)
        p[:, 2] += H * 0.06 * np.sin(np.pi * np.clip(v * 0.9, 0, 1)) - H * 0.18 * v ** 1.5
        return p

    def bones(P, u, v):
        hold = np.clip(1 - v / 0.3, 0, 1)
        return {"spine_03": (hold * 0.5 + (1 - hold)).astype(np.float32), "clavicle_" + side: (hold * 0.5).astype(np.float32)}
    return sheet.Sheet("flung_" + side, position, material, bones, columns, rows)
