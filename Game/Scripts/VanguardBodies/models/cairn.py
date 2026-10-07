"""Cairn, The River's Grasp (ADR-069): a colossus of carved masonry and riverstone, read by his silhouette from the game
camera. Character Bible §18 and his splash art: a gigantic, visibly asymmetrical heap of squared stone blocks, warm tan
and ochre, some cut with square spiral glyphs; moss thick on their tops; heavy rusted chain wound round his shoulder,
chest and arms and hanging in loops. His right arm is the outsized hook arm: its forearm a great stone gauntlet wound
with chain, its fist of squared stone closed on the heavy chain the single rusted iron crescent hangs from, just below
it. His left arm is shorter and slighter, built for support and ground slams, ending in a heavy fist of squared
fingers. Chain trails from his hip behind him on the ground. His head is a small faceless block set low between
enormous shoulders, barely showing; he is the river's own stonework, never lit from within, so no eye glows there. The
water pouring off him is his drip effect, not his mesh.

Low poly and flat-coloured (author 2026-10-07): squared blocks, each a flat colour the toon material shades, the dark
interior showing in the seams between them. He rests as his archetype lays him out (ADR-064), arms hanging; his clips
stomp, throw and slam. Immovable (his E) heaves further layers of stone up over him (reinforced) and plants his feet wide
(the layout's stanceSpread). Colours are sampled from his splash art (sRGB): its lit and shaded stone met halfway, so
the toon light shades them."""
import numpy as np

from ..sculpt import anatomy, garments, sdf, tree
from ..sculpt.anatomy import V, unit
from ..sculpt.tree import Box, Over, Shell, Union, Zone

PALETTE = {
    # Masonry: the warm tan of most blocks, a sunlit paler block, a darker ochre-brown one and the dark interior in the
    # seams between them; his head, in the shadow of his shoulders; the glyphs cut into the dressed blocks.
    "stone": (0.71, 0.56, 0.38), "sun": (0.85, 0.72, 0.52), "ochre": (0.56, 0.41, 0.27), "core": (0.27, 0.21, 0.16),
    "head": (0.5, 0.4, 0.3), "carved": (0.42, 0.31, 0.21),
    # Old iron: the chain's rust, the crescent's deeper rust and its flat, worn inner face.
    "rust": (0.62, 0.34, 0.2), "hook": (0.5, 0.26, 0.16), "edge": (0.7, 0.47, 0.32),
    # Moss and river growth on the tops of his blocks.
    "moss": (0.46, 0.5, 0.17),
}
STONE_TONES = ("stone", "sun", "ochre", "stone")
# The hook arm's stone gauntlet: its radius at the elbow and at the wrist, as shares of his height.
GAUNTLET_SHARE = (0.14, 0.165)
# A heavy chain link's length, as a share of his height.
LINK_SHARE = 0.11
# The crescent: its centre line's radius, its band's half width at the shank and its half thickness there, as shares of
# his height (it narrows to its point); where it sits under the hand (ahead of the hand's end and out to its side); and
# the arc it sweeps from its shank, round below and up to its point, in degrees about its plane.
HOOK_RADIUS, HOOK_WIDTH, HOOK_DEPTH = 0.115, 0.046, 0.032
HOOK_AHEAD, HOOK_OUT = 0.11, 0.11
HOOK_START, HOOK_SWEEP = 105.0, 280.0
# The hook hand's fist, as a share of his height, and where in it the crescent's chain leaves it (down its length and
# ahead, as shares of the fist).
HOOK_FIST, HOOK_GRIP = 0.12, (1.45, 0.35)
CORNERS = np.array([[x, y, z] for x in (-1.0, 1.0) for y in (-1.0, 1.0) for z in (-1.0, 1.0)])
# A square spiral glyph, in steps of its turn (a share of the face's half size): from its centre out two turns.
SPIRAL = [(0, 0), (1, 0), (1, -1), (-1, -1), (-1, 1), (2, 1), (2, -2), (-2, -2), (-2, 2)]
SPIRAL_STEP, GLYPH_FRAME, GLYPH_GROOVE = 0.27, 0.82, 0.07
# Moss: the blocks whose tops it may hold (by name), on about this share of them (seeded), each pad covering between
# these shares of its block's top.
MOSSY = ("shoulder_", "cap_", "blade_", "hump", "collar_", "fist_l", "knee_", "neck", "breast_", "chest", "hookhand")
MOSS_SHARE, MOSS_COVER, MOSS_SEED = 0.8, (0.6, 1.05), 7


def materials(S):
    return {name: S.material(name, colour) for name, colour in PALETTE.items()}


def mix(a, b, t):
    return a + (b - a) * t


def p_of(L, bone, i):
    return V(*L[bone][i])


def tone(rng, mats):
    return mats[STONE_TONES[rng.integers(len(STONE_TONES))]]


def block(S, name, centre, half, axes, material, bones, rounding=None, protect=0.0, rng=None, cuts=1):
    """A squared stone: a box of half extents half in the frame axes (columns), its edges eased; given rng, cuts of its
    corners struck off, so it reads as a worn, broken block rather than a brick."""
    c, h = V(*centre), V(*half)
    r = float(min(h)) * 0.16 if rounding is None else rounding
    A = np.asarray(axes, dtype=np.float64)
    reach = float(np.linalg.norm(h))
    planes = []
    if rng is not None:
        for corner in CORNERS[rng.choice(len(CORNERS), size=cuts, replace=False)]:
            n = unit(corner * V(rng.uniform(0.7, 1.3), rng.uniform(0.7, 1.3), rng.uniform(0.7, 1.3)))
            planes.append((n, float(n @ (corner * h)) * rng.uniform(0.7, 0.85)))

    def distance(P):
        Q = sdf.local(P, c, A)
        d = sdf.box(Q, V(0, 0, 0), h, None, r)
        for n, offset in planes:
            d = np.maximum(d, Q @ n - offset)
        return d
    node = tree.leaf(S, name, distance, Box(c - reach, c + reach), material, bones, protect)
    # Kept so moss can find the block's top face.
    node.squared = (c, A, h, name)
    return node


def glyph_block(S, name, centre, half, axes, mats, bones, colour="sun"):
    """A dressed block from the old works: squared, its front (local +x) face cut with a square spiral glyph inside a
    frame."""
    c, h = V(*centre), V(*half)
    A = np.asarray(axes, dtype=np.float64)
    reach = float(np.linalg.norm(h))
    stone = tree.leaf(S, name, lambda P: sdf.box(P, c, h, A, min(h) * 0.1), Box(c - reach, c + reach), mats[colour], bones, protect=0.5)
    size = min(h[1], h[2])
    path = [V(x, y) * SPIRAL_STEP * size for x, y in SPIRAL]
    groove = GLYPH_GROOVE * size

    def cut(P):
        Q = sdf.local(P, c, A)
        uv = Q[:, 1:3]
        d = np.full(len(P), 1.0e3)
        for a, b in zip(path, path[1:]):
            ab = b - a
            t = np.clip(((uv - a) @ ab) / max(ab @ ab, 1e-9), 0.0, 1.0)
            d = np.minimum(d, np.linalg.norm(uv - a - t[:, None] * ab, axis=1))
        frame = np.abs(np.maximum(np.abs(uv[:, 0]) - h[1] * GLYPH_FRAME, np.abs(uv[:, 1]) - h[2] * GLYPH_FRAME))
        d = np.minimum(d, frame) - groove
        return np.maximum(d, np.abs(Q[:, 0] - h[0]) - size * 0.08)
    carved = tree.leaf(S, name + "_glyph", cut, Box(c - reach, c + reach), mats["carved"], bones, protect=0.5)
    node = tree.Subtract(stone, carved, label=carved.part.label)
    node.squared = (c, A, h, name)
    return node


def column(S, rng, mats, name, a, b, ra, rb, bones, rings=2, jut=2, spin=20.0, tip=6.0, core=0.8):
    """A limb of stacked masonry along a to b: rings of squared blocks, each turned about the limb and tipped a little
    so their corners break its outline, jut smaller blocks set into its sides, and its dark interior between them."""
    a, b = V(*a), V(*b)
    length = float(np.linalg.norm(b - a))
    F = sdf.frame(b - a, (1.0, 0.0, 0.0))
    nodes = []
    if core:
        nodes.append(tree.leaf(S, name + "_core", lambda P: sdf.round_cone(P, a, b, ra * core, rb * core), Box.around([a, b], max(ra, rb)), mats["core"], bones))
    for i in range(rings):
        t = (i + 0.5) / rings
        r = ra + (rb - ra) * t
        axes = F @ sdf.rotation(yaw=rng.uniform(-spin, spin) + 45.0 * (i % 2), pitch=rng.uniform(-tip, tip), roll=rng.uniform(-tip, tip))
        half = (r * rng.uniform(0.84, 0.95), r * rng.uniform(0.84, 0.95), length / rings * 0.5 * rng.uniform(1.0, 1.12))
        nodes.append(block(S, "%s_%d" % (name, i), mix(a, b, t), half, axes, tone(rng, mats), bones, rng=rng, cuts=2))
    for j in range(jut):
        t = rng.uniform(0.2, 0.8)
        r = ra + (rb - ra) * t
        angle = rng.uniform(0, 2 * np.pi)
        out = F[:, 0] * np.cos(angle) + F[:, 1] * np.sin(angle)
        axes = sdf.frame(out, F[:, 2]) @ sdf.rotation(yaw=rng.uniform(-15, 15), pitch=rng.uniform(-8, 8))
        half = (r * rng.uniform(0.38, 0.5), r * rng.uniform(0.38, 0.5), r * rng.uniform(0.3, 0.38))
        nodes.append(block(S, "%s_jut%d" % (name, j), mix(a, b, t) + out * r * 0.8, half, axes, tone(rng, mats), bones, rng=rng, cuts=1))
    return nodes


def build(S, L, dims, spec):
    """Cairn's sculpt on his layout: (the whole, {"body": his stone, under its moss and iron, "sheets": none})."""
    mats = materials(S)
    features = set(spec["features"])
    H = dims["height"]
    rng = np.random.default_rng(spec["seed"])
    stones = trunk(S, L, dims, mats, rng) + head(S, L, dims, mats, rng)
    stones += arms(S, L, dims, mats, rng, features) + legs(S, L, dims, mats, rng)
    if "masonry" in features:
        stones += masonry(S, L, dims, mats)
    if spec.get("reinforced"):
        stones += reinforced(S, L, dims, mats, rng)
    body = Union(stones, k=H * 0.004)
    layers = [body]
    if "moss" in features:
        layers.append(moss(S, dims, mats, body, stones, spec["seed"]))
    iron = []
    if "chainWrap" in features:
        iron.append(chain_wrap(S, L, dims, mats, body))
    if "hookArm" in features:
        iron.append(hook(S, L, dims, mats))
    worn = Over(layers + iron)
    # Cut flat where he meets the ground: nothing of him sinks below it.
    worn = tree.Intersect(worn, Zone(lambda P: -P[:, 2], Box(V(-500, -500, -0.5), V(500, 500, 600))))
    return worn, {"body": body, "sheets": []}


# ---------------------------------------------------------------------------------------------- trunk and head
def trunk(S, L, dims, mats, rng):
    """The trunk: great squared blocks laid in courses up the forward-leaning spine, narrow at the waist and broadening
    to the chest, the upper back a hump of stone behind the head; the shoulders' heaps rise either side of it, the hook
    arm's the larger."""
    H, shoulder, hip = dims["height"], dims["shoulder"], dims["hip"]
    pelvis, waist, chest, top = p_of(L, "pelvis", 0), p_of(L, "spine_01", 0), p_of(L, "spine_02", 0), p_of(L, "spine_03", 1)
    spine03 = p_of(L, "spine_03", 0)
    R = sdf.rotation
    # The dark interior, kept within the blocks: it shows only in the seams between them.
    nodes = [tree.leaf(S, "trunk_core", lambda P: sdf.round_cone(P, pelvis + V(0, 0, H * 0.08), spine03, hip * 0.55, shoulder * 0.34),
                       Box.around([pelvis, spine03], shoulder), mats["core"], anatomy.spine_weights(L))]

    def stone(name, centre, half, axes, colour, bone, cuts=1):
        nodes.append(block(S, name, centre, half, axes, mats[colour], anatomy.rigid(bone), rng=rng, cuts=cuts))
    stone("hips", pelvis + V(-H * 0.01, 0, H * 0.035), (H * 0.12, hip * 1.12, H * 0.07), R(yaw=4, roll=-3), "ochre", "pelvis")
    # The seat between his thighs, closing the trunk off underneath.
    stone("seat", pelvis + V(0, 0, -H * 0.025), (H * 0.09, hip * 0.6, H * 0.06), R(yaw=-6, roll=5), "stone", "pelvis")
    stone("loin", pelvis + V(H * 0.1, -hip * 0.15, H * 0.05), (H * 0.055, hip * 0.65, H * 0.06), R(yaw=-8, pitch=-10), "stone", "pelvis")
    # Belly and ribs in courses of two blocks side by side, never mirrored.
    belly = mix(waist, chest, 0.5) + V(H * 0.02, 0, 0)
    stone("belly_l", belly + V(0, hip * 0.42, H * 0.01), (H * 0.12, hip * 0.5, H * 0.08), R(yaw=8, roll=6, pitch=4), "sun", "spine_01")
    stone("belly_r", belly + V(H * 0.01, -hip * 0.4, -H * 0.012), (H * 0.125, hip * 0.52, H * 0.075), R(yaw=-10, roll=-3, pitch=8), "stone", "spine_01")
    stone("back_low", mix(waist, chest, 0.6) - V(H * 0.09, -hip * 0.15, 0), (H * 0.08, hip * 0.7, H * 0.1), R(yaw=12, roll=6), "ochre", "spine_01")
    ribs = mix(chest, spine03, 0.55) + V(H * 0.01, 0, 0)
    stone("ribs_l", ribs + V(-H * 0.005, shoulder * 0.27, H * 0.015), (H * 0.14, shoulder * 0.33, H * 0.085), R(yaw=6, roll=8, pitch=10), "stone", "spine_02")
    stone("ribs_r", ribs + V(H * 0.01, -shoulder * 0.3, -H * 0.01), (H * 0.15, shoulder * 0.33, H * 0.095), R(yaw=-5, roll=-6, pitch=6), "ochre", "spine_02")
    stone("chest", mix(spine03, top, 0.35) - V(H * 0.03, 0, 0), (H * 0.15, shoulder * 0.62, H * 0.075), R(yaw=-4, roll=3, pitch=16), "stone", "spine_03")
    stone("breast_l", spine03 + V(H * 0.12, shoulder * 0.28, 0), (H * 0.07, shoulder * 0.27, H * 0.09), R(yaw=18, pitch=-12, roll=5), "sun", "spine_03")
    stone("breast_r", spine03 + V(H * 0.12, -shoulder * 0.3, -H * 0.015), (H * 0.075, shoulder * 0.31, H * 0.1), R(yaw=-14, pitch=-8, roll=-6), "stone", "spine_03")
    # The hump of his upper back, behind and above where the head sits.
    stone("hump", top - V(H * 0.15, 0, -H * 0.005), (H * 0.13, shoulder * 0.5, H * 0.085), R(yaw=5, pitch=24, roll=-4), "sun", "spine_03")
    stone("hump_back", top - V(H * 0.26, 0, H * 0.06), (H * 0.08, shoulder * 0.42, H * 0.09), R(yaw=-8, pitch=-30, roll=6), "ochre", "spine_03")
    # The neck: a block bedded between the hump and the back of the head, so no hollow shows behind it.
    neck0, neck1 = p_of(L, "neck_01", 0), p_of(L, "neck_01", 1)
    stone("neck", mix(neck0, neck1, 0.45) - V(0, 0, H * 0.01), (H * 0.09, H * 0.1, H * 0.06), R(yaw=3, pitch=6), "ochre", "spine_03")
    # The shoulders: heaps of stone rising either side of the head, the hook arm's taller and broader.
    for side, sign in (("l", 1.0), ("r", -1.0)):
        big = 1.15 if side == "r" else 0.95
        bone = "clavicle_" + side
        s0, s1 = p_of(L, bone, 0), p_of(L, bone, 1)
        stone("shoulder_" + side, s1 + V(-H * 0.01, sign * shoulder * 0.02, H * 0.05 * big), (H * 0.12 * big, shoulder * 0.26 * big, H * 0.1 * big),
              R(yaw=sign * 14, roll=sign * 18, pitch=-8), "stone" if side == "r" else "sun", bone, cuts=2)
        stone("cap_" + side, s1 + V(H * 0.03, sign * shoulder * 0.27 * big, -H * 0.02), (H * 0.085 * big, shoulder * 0.15 * big, H * 0.09 * big),
              R(yaw=-sign * 20, roll=sign * 35, pitch=6), "ochre", bone)
        stone("blade_" + side, s1 + V(-H * 0.1, -sign * shoulder * 0.1, H * 0.03 * big), (H * 0.07 * big, shoulder * 0.24 * big, H * 0.08 * big),
              R(yaw=sign * 24, roll=sign * 8, pitch=-28), "sun" if side == "r" else "stone", bone)
        stone("collar_" + side, mix(s0, s1, 0.45) + V(H * 0.0, 0, H * 0.035), (H * 0.09, shoulder * 0.22, H * 0.07), R(yaw=-sign * 8, roll=sign * 20, pitch=10),
              "ochre", bone)
    return nodes


def head(S, L, dims, mats, rng):
    """His head: a small faceless block of stone set low and forward between his shoulders, in their shadow, a heavier
    stone across its top like a brow; no face and no light beneath it."""
    H = dims["height"]
    centre = p_of(L, "head", 0) + V(H * 0.07, 0, -H * 0.005)
    bones = anatomy.rigid("head")
    R = sdf.rotation
    return [block(S, "head", centre, (H * 0.07, H * 0.08, H * 0.065), R(yaw=4, pitch=8), mats["head"], bones, protect=0.6, rng=rng, cuts=2),
            block(S, "brow", centre + V(H * 0.035, 0, H * 0.052), (H * 0.065, H * 0.095, H * 0.03), R(yaw=-6, pitch=16, roll=4), mats["ochre"], bones, protect=0.6,
                  rng=rng)]


# ---------------------------------------------------------------------------------------------- limbs
def arms(S, L, dims, mats, rng, features):
    """His arms: the right the hook arm, its forearm a great stone gauntlet ending in the hook hand; the left slighter,
    for support and ground slams, ending in a heavy fist of squared fingers."""
    H = dims["height"]
    nodes = []
    for side in ("l", "r"):
        hooked = side == "r" and "hookArm" in features
        s, e, w, end = p_of(L, "upperarm_" + side, 0), p_of(L, "upperarm_" + side, 1), p_of(L, "lowerarm_" + side, 1), p_of(L, "hand_" + side, 1)
        if hooked:
            # The upper arm runs on past the elbow into the gauntlet's mouth, so no hollow shows between them.
            nodes += column(S, rng, mats, "upperarm_" + side, s, e + (e - s) * 0.12, H * 0.12, H * 0.11, anatomy.rigid("upperarm_" + side), rings=2, jut=2)
            nodes += column(S, rng, mats, "forearm_" + side, e, w, H * GAUNTLET_SHARE[0], H * GAUNTLET_SHARE[1], anatomy.rigid("lowerarm_" + side), rings=3, jut=3, core=0.55)
            nodes += hook_hand(S, L, H, mats, rng)
        else:
            nodes += column(S, rng, mats, "upperarm_" + side, s, e, H * 0.08, H * 0.075, anatomy.rigid("upperarm_" + side), rings=2, jut=1)
            nodes += column(S, rng, mats, "forearm_" + side, e, w, H * 0.08, H * 0.09, anatomy.rigid("lowerarm_" + side), rings=2, jut=2)
            nodes += fist(S, rng, mats, side, w, end, H * 0.105)
    return nodes


def hand_frame(wrist, end):
    """A hand's frame: x down its length, y across it, z out of its back (forward, as it hangs)."""
    down = unit(end - wrist)
    ahead = unit(V(1, 0, 0) - down * down[0])
    return np.stack([down, np.cross(ahead, down), ahead], axis=1)


def fist(S, rng, mats, side, wrist, end, size):
    """A fist of squared stone: a heavy block over the wrist, a knuckle block below it and four squared fingers hanging
    from it, two blocks each, curled in a little, and a squared thumb."""
    bones = anatomy.rigid("hand_" + side)
    F = hand_frame(wrist, end)
    down, across, ahead = F[:, 0], F[:, 1], F[:, 2]
    palm = wrist + down * size * 0.55
    nodes = [block(S, "fist_" + side, palm, (size * 0.75, size * 0.9, size * 0.8), F @ sdf.rotation(yaw=8, roll=6), mats["stone"], bones, protect=0.3, rng=rng),
             block(S, "knuckles_" + side, palm + down * size * 0.75, (size * 0.3, size * 0.88, size * 0.7), F @ sdf.rotation(yaw=-6), mats["sun"], bones, protect=0.3)]
    for i, offset in enumerate((-0.66, -0.22, 0.22, 0.66)):
        root = palm + down * size * 1.05 + across * offset * size * 0.8 + ahead * size * 0.15
        knuckle = root + down * size * 0.35 + ahead * size * 0.08
        for j, (c, half) in enumerate(((mix(root, knuckle, 0.5), (size * 0.24, size * 0.18, size * 0.2)),
                                       (knuckle + down * size * 0.25 - ahead * size * 0.08, (size * 0.2, size * 0.17, size * 0.18)))):
            nodes.append(block(S, "finger_%s%d%d" % (side, i, j), c, half, F @ sdf.rotation(pitch=-10.0 * (j + 1)), mats[("stone", "ochre")[(i + j) % 2]], bones,
                               protect=0.4))
    # The thumb, on the side toward his body.
    inward = -1.0 if side == "l" else 1.0
    thumb = palm + across * size * 0.95 * inward + down * size * 0.3 + ahead * size * 0.45
    nodes.append(block(S, "thumb_" + side, thumb, (size * 0.32, size * 0.2, size * 0.2), F @ sdf.rotation(yaw=25), mats["ochre"], bones, protect=0.4))
    return nodes


def hook_hand(S, L, H, mats, rng):
    """The hook arm's hand: a fist of squared stone closed on the heavy chain the crescent hangs from; and the iron cuff
    round the gauntlet's wrist."""
    wrist, end = p_of(L, "hand_r", 0), p_of(L, "hand_r", 1)
    nodes = fist(S, rng, mats, "r", wrist, end, H * HOOK_FIST)
    # The iron cuff round the gauntlet's wrist, old iron bedded in the stone.
    e, w = p_of(L, "lowerarm_r", 0), p_of(L, "lowerarm_r", 1)
    a, b = mix(e, w, 0.8), mix(e, w, 0.97)
    radius = H * GAUNTLET_SHARE[1] * 1.04
    nodes.append(tree.leaf(S, "cuff", lambda P: sdf.cylinder(P, a, b, radius, H * 0.008), Box.around([a, b], radius * 1.05), mats["rust"], anatomy.rigid("lowerarm_r"),
                           protect=0.4))
    return nodes


def legs(S, L, dims, mats, rng):
    """Massive legs of stacked blocks on broad flat feet."""
    H, hip = dims["height"], dims["hip"]
    nodes = []
    for side, sign in (("l", 1.0), ("r", -1.0)):
        hp, k, a = p_of(L, "thigh_" + side, 0), p_of(L, "thigh_" + side, 1), p_of(L, "calf_" + side, 1)
        f0, f1 = p_of(L, "foot_" + side, 0), p_of(L, "foot_" + side, 1)
        nodes += column(S, rng, mats, "thigh_" + side, hp + V(0, 0, H * 0.03), k, hip * 0.64, hip * 0.58,
                        anatomy.along("pelvis", "thigh_" + side, hp + V(0, 0, H * 0.05), hp - V(0, 0, H * 0.05), 0.2, 0.7), rings=2, jut=1, core=0.62)
        nodes += column(S, rng, mats, "calf_" + side, k, a + V(0, 0, H * 0.03), hip * 0.56, hip * 0.62, anatomy.rigid("calf_" + side), rings=2, jut=1, core=0.65)
        # A squared knee stone over the joint.
        nodes.append(block(S, "knee_" + side, k + V(hip * 0.45, 0, H * 0.005), (H * 0.045, hip * 0.5, H * 0.055), sdf.rotation(yaw=sign * 10, pitch=-8),
                           mats["sun" if side == "l" else "stone"], anatomy.rigid("calf_" + side), rng=rng))
        # The foot: a broad flat slab of stone on the ground, the toes a heavy block ahead of it.
        pad = V((f0[0] + f1[0]) * 0.5, f0[1], H * 0.04)
        nodes.append(block(S, "foot_" + side, pad, (H * 0.13, hip * 0.55, H * 0.045), sdf.rotation(yaw=sign * -6), mats["ochre"], anatomy.rigid("foot_" + side)))
        nodes.append(block(S, "toes_" + side, pad + V(H * 0.11, sign * H * 0.005, -H * 0.005), (H * 0.05, hip * 0.5, H * 0.035), sdf.rotation(yaw=sign * 8),
                           mats["stone"], anatomy.rigid("foot_" + side)))
    return nodes


# ---------------------------------------------------------------------------------------------- masonry
def masonry(S, L, dims, mats):
    """Collapsed masonry worked into him, dressed blocks from the old works cut with square spiral glyphs: a tall one
    standing up off the hook arm's shoulder beside his head, one on the outside of his left shoulder and one worked into
    his left thigh."""
    H, shoulder, hip = dims["height"], dims["shoulder"], dims["hip"]
    s0, s1 = p_of(L, "clavicle_r", 0), p_of(L, "clavicle_r", 1)
    nodes = [glyph_block(S, "masonry_shoulder", mix(s0, s1, 0.45) + V(H * 0.0, 0, H * 0.15), (H * 0.075, H * 0.09, H * 0.11), sdf.rotation(yaw=-18, roll=-10, pitch=-4),
                         mats, anatomy.rigid("clavicle_r"))]
    s0, s1 = p_of(L, "clavicle_l", 0), p_of(L, "clavicle_l", 1)
    nodes.append(glyph_block(S, "masonry_shoulder_l", s1 + V(H * 0.035, shoulder * 0.2, H * 0.02), (H * 0.06, H * 0.075, H * 0.08), sdf.rotation(yaw=55, roll=20, pitch=4),
                             mats, anatomy.rigid("clavicle_l"), colour="stone"))
    hp, k = p_of(L, "thigh_l", 0), p_of(L, "thigh_l", 1)
    nodes.append(glyph_block(S, "masonry_thigh", mix(hp, k, 0.4) + V(hip * 0.55, hip * 0.2, 0), (H * 0.05, H * 0.065, H * 0.06), sdf.rotation(yaw=20, pitch=-8, roll=6),
                             mats, anatomy.along("pelvis", "thigh_l", hp + V(0, 0, H * 0.05), hp - V(0, 0, H * 0.05), 0.2, 0.7), colour="stone"))
    return nodes


# ---------------------------------------------------------------------------------------------- moss
def moss(S, dims, mats, body, stones, seed):
    """Moss and river growth thick on the tops of his blocks: on the upward face of most of the blocks that face the sky
    (his shoulders, back, head, chest, knees and fists), a pad over part or all of it, draping a little over its edges
    where it reaches them, its edges ragged."""
    H = dims["height"]
    rng = np.random.default_rng(seed + MOSS_SEED)
    tops = []
    for node in stones:
        squared = getattr(node, "squared", None)
        if squared is None:
            continue
        c, A, h, name = squared
        k = int(np.argmax(np.abs(A[2, :])))
        if not name.startswith(MOSSY) or abs(A[2, k]) < 0.7 or rng.uniform() > MOSS_SHARE:
            continue
        across = [i for i in range(3) if i != k]
        tops.append((c, A, h, k, float(np.sign(A[2, k])), across, rng.uniform(*MOSS_COVER, size=2), rng.uniform(-0.3, 0.3, size=2)))
    drape = H * 0.02

    def region(P):
        out = np.full(len(P), 1.0e3, dtype=np.float64)
        rag = (sdf.fbm(P.astype(np.float32), H * 0.04, 2, 61) - 0.5) * H * 0.07
        for c, A, h, k, s, across, cover, offset in tops:
            Q = (P - c) @ A
            lateral = np.maximum(*[np.abs(Q[:, i] - offset[j] * h[i]) - cover[j] * h[i] for j, i in enumerate(across)])
            # Within a slab about the block's top face only: nothing that stands on it or hangs below it.
            slab = np.abs(s * Q[:, k] - h[k]) - drape
            out = np.minimum(out, np.maximum(slab, lateral + rag))
        return out
    points = np.array([c for c, *_rest in tops])
    reach = max(float(np.linalg.norm(t[2])) for t in tops) + H * 0.05
    zone = Zone(region, Box(points.min(axis=0) - reach, points.max(axis=0) + reach))
    return Shell(S, "moss", body, 0.0, H * 0.012, zone, mats["moss"], hem=H * 0.004)

# ---------------------------------------------------------------------------------------------- iron
def chain(S, name, points, size, material, bones, protect=0.45, samples=64):
    """Heavy iron chain along a curve through points: oval links size long with open eyes, forged square in section so
    their faces stay flat, each turned a quarter about the chain from the last, so every other link shows its eye."""
    line = garments.curve(points, samples)
    run = np.concatenate([[0.0], np.cumsum(np.linalg.norm(np.diff(line, axis=0), axis=1))])
    wire = size * 0.12
    bend = size * 0.37 - wire
    straight = size * 0.5 - bend - wire
    pitch = size - 3.6 * wire
    round_ = wire * 0.3
    count = max(1, int(round(run[-1] / pitch)))
    at = (np.arange(count) + 0.5) * run[-1] / count
    centres = np.stack([np.interp(at, run, line[:, k]) for k in range(3)], axis=1)
    ahead = np.stack([np.interp(np.minimum(at + 0.5, run[-1]), run, line[:, k]) - np.interp(np.maximum(at - 0.5, 0.0), run, line[:, k]) for k in range(3)], axis=1)
    frames = []
    for i in range(count):
        a = unit(ahead[i])
        n = unit(np.cross(a, V(0, 0, 1))) if abs(a[2]) < 0.95 else unit(np.cross(a, V(1, 0, 0)))
        if i % 2:
            n = unit(np.cross(a, n))
        frames.append((centres[i], a, unit(np.cross(n, a)), n))

    def links(P):
        d = np.full(len(P), 1.0e4, dtype=np.float64)
        for c, a, b, n in frames:
            Q = P - c
            x, y, z = np.abs(Q @ a) - straight, Q @ b, Q @ n
            ring = np.abs(np.sqrt(np.maximum(x, 0.0) ** 2 + y * y) - bend) - wire + round_
            w = np.stack([ring, np.abs(z) - wire * 0.85 + round_], axis=1)
            d = np.minimum(d, np.minimum(np.max(w, axis=1), 0.0) + np.linalg.norm(np.maximum(w, 0.0), axis=1) - round_)
        return d
    return tree.leaf(S, name, links, Box.around(centres, size), material, bones, protect=protect)


def chain_wrap(S, L, dims, mats, body):
    """Heavy rusted chain: slung over the hook arm's shoulder outside its glyph block, across his chest to his left hip
    and back round his back; wound about the hook arm's gauntlet; wrapped round his left forearm, hanging below it in
    a loop; and trailing from his right hip behind him along the ground, as though he has just dragged it up out of the
    river."""
    H = dims["height"]
    size = H * LINK_SHARE
    lift = size * 0.15
    pz, cz = p_of(L, "pelvis", 0)[2], p_of(L, "spine_03", 1)[2]
    torso = cz - pz
    spine_x = p_of(L, "spine_02", 0)[0]
    shoulder, hip = dims["shoulder"], dims["hip"]
    parts = []
    # Points on his stone found by rays coming in to it (from above over the shoulder, else in toward his spine, kept
    # clear of where his arms hang), stood off it a little.
    rays = [(V(spine_x + H * 0.04, -shoulder * 0.9, cz + H * 0.4), V(0, 0, -1))]
    for degrees, z in ((-28.0, cz - torso * 0.14), (0.0, cz - torso * 0.33), (22.0, cz - torso * 0.52), (40.0, cz - torso * 0.68),
                       (125.0, cz - torso * 0.62), (180.0, cz - torso * 0.38), (-150.0, cz - torso * 0.12)):
        o = V(np.cos(np.radians(degrees)), np.sin(np.radians(degrees)), 0.0)
        rays.append((V(spine_x, 0.0, z) + o * 150.0, -o))
    rays.append((V(spine_x - H * 0.1, -shoulder * 0.9, cz + H * 0.4), V(0, 0, -1)))
    hits, normals = garments.surface_points(body, np.array([s for s, _d in rays]), np.array([d for _s, d in rays]), reach=150.0)
    line = [hits[i] + normals[i] * lift for i in range(len(hits))]
    parts.append(chain(S, "chain_sling", line + [line[0]], size, mats["rust"], anatomy.spine_weights(L)))
    # Wound about the hook arm's gauntlet: a loose spiral, half sunk in its stone.
    e, w = p_of(L, "lowerarm_r", 0), p_of(L, "lowerarm_r", 1)
    axis = unit(w - e)
    side = unit(np.cross(axis, V(1, 0, 0)))
    up = np.cross(axis, side)
    wound = [mix(e, w, 0.15 + 0.5 * t) + (side * np.cos(t * 2.0 * np.pi) + up * np.sin(t * 2.0 * np.pi)) * H * mix(*GAUNTLET_SHARE, 0.15 + 0.5 * t) * 1.03
             for t in np.linspace(0.0, 1.0, 9)]
    parts.append(chain(S, "chain_wound", wound, size, mats["rust"], anatomy.rigid("lowerarm_r")))
    # Round the left forearm below the elbow, and a loop of it hanging under the arm.
    e, w = p_of(L, "lowerarm_l", 0), p_of(L, "lowerarm_l", 1)
    axis = unit(w - e)
    side = unit(np.cross(axis, V(1, 0, 0)))
    up = np.cross(axis, side)
    ring_c = mix(e, w, 0.25)
    radius = H * 0.088
    ring = [ring_c + (side * np.cos(a) + up * np.sin(a)) * radius for a in np.linspace(0.0, 2 * np.pi, 9)]
    bones = anatomy.rigid("lowerarm_l")
    parts.append(chain(S, "chain_bound", ring, size * 0.85, mats["rust"], bones))
    hang_a, hang_b = ring_c + up * radius, ring_c - side * radius * 0.3 - up * radius * 0.4
    loop = [hang_a, mix(hang_a, hang_b, 0.5) + V(H * 0.03, 0, -H * 0.17), hang_b]
    parts.append(chain(S, "chain_loop", loop, size * 0.85, mats["rust"], bones))
    # Trailing from his right hip, down to the ground behind him and along it.
    p0 = p_of(L, "pelvis", 0)
    root, _normal = garments.surface_point(body, p0 + V(-150.0, -hip * 0.5, H * 0.05), V(1, 0, 0), reach=150.0)
    trail = [root, root + V(-hip * 0.4, -hip * 0.15, -root[2] * 0.45), V(root[0] - hip * 0.8, -hip * 0.65, size * 0.3), V(root[0] - hip * 1.8, -hip * 0.7, size * 0.3)]
    parts.append(chain(S, "chain_trail", trail, size, mats["rust"], anatomy.rigid("pelvis")))
    return Union(parts)


def hook(S, L, dims, mats):
    """The rusted iron crescent hung on heavy chain from the hook hand's fist: a broad band of iron sweeping down and
    round from its shank to a sharpened point, open forward and out, turned toward the camera and tipped back so it
    reads from above and ahead, its flat inner face worn paler; the chain runs from the fist to the eye at its shank."""
    H = dims["height"]
    bones = anatomy.rigid("prop_r")
    anchor = p_of(L, "prop_r", 0)
    across = unit(V(0.42, -0.91, 0.0))
    facing = unit(np.cross(V(0, 0, 1), across))
    up = unit(V(0, 0, 1) - facing * 0.35)
    up = unit(up - across * (up @ across))
    normal = np.cross(across, up)
    radius, width, depth = H * HOOK_RADIUS, H * HOOK_WIDTH, H * HOOK_DEPTH
    centre = anchor + V(H * HOOK_AHEAD, 0, 0) + across * H * HOOK_OUT
    centre[2] = radius + width + H * 0.012
    start, sweep = np.radians(HOOK_START), np.radians(HOOK_SWEEP)

    def shape(P):
        Q = P - centre
        x, y, z = Q @ across, Q @ up, Q @ normal
        theta = np.mod(np.arctan2(y, x) - start, 2 * np.pi)
        t = np.clip(theta / sweep, 0.0, 1.0)
        rho = np.sqrt(x * x + y * y)
        # The band narrows from its shank to the point, its centre line drawing in as it goes (a hook, not a ring).
        half = width * (1.0 - t) ** 0.7 + H * 0.002
        line = radius * (1.0 - 0.12 * t)
        band = np.abs(rho - line) - half
        # Past either end: the distance to that end's rounded tip.
        ends = []
        for tt in (0.0, 1.0):
            angle = start + sweep * tt
            r = radius * (1.0 - 0.12 * tt)
            ends.append(np.sqrt((x - np.cos(angle) * r) ** 2 + (y - np.sin(angle) * r) ** 2) - (width * (1.0 - tt) ** 0.7 + H * 0.002))
        d2 = np.where(theta <= sweep, band, np.minimum(ends[0], ends[1]))
        w = np.stack([d2, np.abs(z) - depth * (1.0 - 0.5 * t)], axis=1)
        d = np.minimum(np.max(w, axis=1), 0.0) + np.linalg.norm(np.maximum(w, 0.0), axis=1)
        return d - H * 0.002, rho, line, half

    def face(P):
        d, rho, line, half = shape(P)
        return np.maximum(d - H * 0.0025, rho - (line - half * 0.4))
    reach = radius + width + H * 0.02
    box = Box(centre - reach, centre + reach)
    crescent = tree.leaf(S, "crescent", lambda P: shape(P)[0], box, mats["hook"], bones, protect=1.0)
    worn = tree.leaf(S, "crescent_face", face, box, mats["edge"], bones, protect=1.0)
    eye = centre + up * (radius + width * 0.6)
    ring = tree.leaf(S, "crescent_eye", lambda P: sdf.torus(P, eye, width * 0.75, width * 0.32, sdf.frame(normal)), Box(eye - width * 1.3, eye + width * 1.3),
                     mats["hook"], bones, protect=0.6)
    wrist, end = p_of(L, "hand_r", 0), p_of(L, "hand_r", 1)
    F = hand_frame(wrist, end)
    grip = wrist + (F[:, 0] * HOOK_GRIP[0] + F[:, 2] * HOOK_GRIP[1]) * H * HOOK_FIST
    links = chain(S, "crescent_chain", [grip, mix(grip, eye, 0.5) - up * H * 0.01, eye + up * width * 0.4], H * LINK_SHARE * 0.9, mats["rust"], bones)
    return Over([Union([crescent, ring, links]), worn])


# ---------------------------------------------------------------------------------------------- Immovable
def reinforced(S, L, dims, mats, rng):
    """Immovable (his E): further layers of stone heaved up over him: standing slabs rising off his shoulders round his
    low head like a rampart, a slab braced over his left forearm, greaves of stone about his shins and a girdle of
    blocks round his hips."""
    H, hip = dims["height"], dims["hip"]
    nodes = []
    R = sdf.rotation
    for side, sign in (("l", 1.0), ("r", -1.0)):
        s0, s1 = p_of(L, "clavicle_" + side, 0), p_of(L, "clavicle_" + side, 1)
        for i, (along, rise, lean) in enumerate(((0.3, 0.17, 18.0), (0.75, 0.16, 30.0))):
            centre = mix(s0, s1, along) + V(-H * 0.06, 0, H * rise)
            nodes.append(block(S, "rampart_%s%d" % (side, i), centre, (H * 0.05, H * 0.08, H * 0.12), R(yaw=sign * 25 * (i + 1), roll=sign * lean, pitch=-10),
                               mats["ochre" if i else "sun"], anatomy.rigid("clavicle_" + side), rng=rng))
        k, a = p_of(L, "thigh_" + side, 1), p_of(L, "calf_" + side, 1)
        nodes += column(S, rng, mats, "greave_" + side, k + V(0, 0, H * 0.02), a + V(0, 0, H * 0.07), hip * 0.74, hip * 0.78, anatomy.rigid("calf_" + side),
                        rings=1, jut=2, core=0.0)
    e, w = p_of(L, "lowerarm_l", 0), p_of(L, "lowerarm_l", 1)
    F = sdf.frame(w - e, (1.0, 0.0, 0.0))
    nodes.append(block(S, "bracer_l", mix(e, w, 0.55) + F[:, 0] * H * 0.06, (H * 0.04, H * 0.1, H * 0.13), F @ R(yaw=-15), mats["ochre"], anatomy.rigid("lowerarm_l"), rng=rng))
    pelvis = p_of(L, "pelvis", 0)
    for i in range(6):
        angle = i / 6 * 2 * np.pi + 0.3
        out = V(np.cos(angle), np.sin(angle), 0.0)
        centre = pelvis + V(0, 0, H * 0.04) + out * V(H * 0.15, hip * 1.15, 0)
        nodes.append(block(S, "girdle_%d" % i, centre, (H * 0.05, H * 0.08, H * 0.07), sdf.frame(out, (0, 0, 1)) @ R(yaw=rng.uniform(-15, 15), roll=rng.uniform(-10, 10)),
                           mats[STONE_TONES[i % len(STONE_TONES)]], anatomy.rigid("pelvis"), rng=rng))
    return nodes
