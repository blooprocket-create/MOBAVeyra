"""Moro, The Wildspark (ADR-069): a great antlered jungle predator, read by his silhouette from the game camera.
Character Bible §12 and his splash art: a huge wolf-like quadruped, deep through the chest and shoulders, on thick legs
of weathered timber with broad pale-clawed paws. His body is dark bark and fur, plated with weathered grey timber through
the haunches and shoulders; a mane of dark bark shards rakes back along his neck and spine and trails off his rump; thick
cream-white fur makes his ruff, his chest and his cheeks. Branching antlers of living wood rise high off his brow, leaves
growing on their tines. Wildlight grows along his limbs and flanks in blue-violet, never as glyphwork: eye-shaped panels
at shoulder and haunch, leaf-shaped panels on his flanks, and veins following the grain of him down every leg; his eyes
burn the same blue. Moss and small growing things sit on his back. He is wholly an animal: nothing worn, nothing
carried.

Low poly and flat-coloured (author 2026-10-07): the big forms that make his outline, each a flat colour the toon
material shades. He stands as his archetype lays him out (ADR-064): forelegs on the arm bones, hind legs on the leg
bones, the tail on tail_01..03, every part weighted rigidly to the bone it rides. Surging (wildlightSurge, his Dominion and
Wildstorm bodies), his markings swell and pale toward white, veins climb his neck and the leaves on his antlers light.
Colours are his kit's, lifted where dark so they read under the toon light and moved toward his art."""
import math

import numpy as np

from ..sculpt import anatomy, sdf, tree
from ..sculpt.anatomy import V, unit
from ..sculpt.tree import Box, Over, Placed, Shell, Union, Zone

# The trunk his layout was drawn for (cm): every size below is at this trunk and scales with his own.
TRUNK_CM = 45.65
# The span from his hips to his chest, and his head's length, at that trunk (cm): his rump and chest stand out from
# them by the same measure however long his layout makes him, and his head grows half as fast as its bone.
SPAN_CM = 89.1
HEAD_CM = 41.25
# His head a little larger than his layout's, so it reads from the game camera.
HEAD_SCALE = 1.15
# Sections of his trunk along the spine, from the rump to the chest: (share of the way from the hips to the chest,
# centre above the hips' height, half-height, half-width), at TRUNK_CM. Haunches and shoulders broad, the loin tucked
# up and narrow, the chest deep.
TRUNK = [(-0.42, 4.0, 7.0, 7.0), (-0.33, 3.0, 18.0, 18.0), (-0.15, 2.0, 26.0, 29.0), (0.10, 1.0, 25.0, 29.0), (0.32, 4.0, 22.0, 23.0),
         (0.55, 1.0, 30.0, 27.0), (0.80, 3.0, 37.0, 32.0), (1.02, 6.0, 31.0, 28.0), (1.18, 11.0, 20.0, 19.0), (1.27, 14.0, 8.0, 8.0)]


def mix(a, b, t):
    return tuple(float(x + (y - x) * t) for x, y in zip(a, b))


# What his kit's colours are lifted or moved toward (sRGB): dark fur goes black under the toon light below about 0.2,
# so it is lifted toward a warm brown; his timber is weathered paler; his paws and nose are the darkest of him.
FUR_LIFT = ((0.55, 0.48, 0.42), 0.32)
WEATHERED = ((0.62, 0.58, 0.52), 0.35)
DARKEST = (0.1, 0.09, 0.08)
WHITE = (1.0, 1.0, 1.0)
# The greens of what grows on him: moss in two greens, sprouts, and the leaves on his antlers.
GREENS = {"moss": (0.33, 0.48, 0.2), "moss_dark": (0.26, 0.4, 0.16), "sprout": (0.3, 0.52, 0.2), "leaf": (0.32, 0.55, 0.2)}


def palette(spec):
    """His colours (sRGB) from his kit (a status body's own kit colours, where it changes them): the dark fur lifted
    off black, the timber weathered grey, the bark of his mane and the wood of his legs browner, cream fur, antler wood
    and the blue-violet Wildlight, paler while it surges (and then his antlers' leaves light with it)."""
    primary, secondary, accent, detail, horn = (tuple(spec[k]) for k in ("primary", "secondary", "accent", "detail", "horn"))
    surge = "wildlightSurge" in spec["features"]
    light = mix(accent, WHITE, 0.3) if surge else accent
    fur = mix(secondary, *FUR_LIFT)
    bark = mix(fur, horn, 0.45)
    timber = mix(primary, *WEATHERED)
    colours = {"fur": fur, "bark": bark, "timber": timber, "timber_dark": mix(timber, fur, 0.3), "wood": mix(timber, bark, 0.55), "cream": detail,
               "horn": mix(horn, (0.6, 0.5, 0.38), 0.2), "paw": mix(fur, DARKEST, 0.2), "claw": mix(detail, primary, 0.25), "nose": mix(fur, DARKEST, 0.35),
               "wildlight": light, "heart": mix(light, WHITE, 0.6), "eye": mix(accent, WHITE, 0.15)}
    colours.update(GREENS)
    if surge:
        colours["leaf"] = light
    return colours

def materials(S, spec):
    glowing = ("wildlight", "heart", "eye") + (("leaf",) if "wildlightSurge" in spec["features"] else ())
    return {name: S.material(name, colour, glow=name in glowing) for name, colour in palette(spec).items()}


# ---------------------------------------------------------------------------------------------- shapes
def shard(P, a, b, ra, rb, flat, thin):
    """A pointed shard of bark or fur from a (radius ra) to b (rb): a round cone made thin times thinner across flat
    (a direction, made square to its length), so it lies like a slat, not a quill."""
    a = np.asarray(a, dtype=np.float64)
    axis = unit(np.asarray(b, dtype=np.float64) - a)
    f = np.asarray(flat, dtype=np.float64)
    f = unit(f - axis * (f @ axis))
    Q = np.asarray(P, dtype=np.float64) - a
    Q = Q + np.outer(Q @ f, f) * (thin - 1.0)
    return (sdf.round_cone(Q, np.zeros(3), np.asarray(b, dtype=np.float64) - a, ra, rb) / thin).astype(np.float32)


def vesica(Q, centre, half_length, half_width, angle):
    """Signed distance in 2D to a pointed oval (an eye or a leaf) about centre, its length turned angle radians off the
    first axis: two discs' overlap, so it comes to a point at each end."""
    c, s = math.cos(angle), math.sin(angle)
    q = np.asarray(Q, dtype=np.float64) - np.asarray(centre, dtype=np.float64)
    u, v = q[:, 0] * c + q[:, 1] * s, -q[:, 0] * s + q[:, 1] * c
    radius = (half_length ** 2 + half_width ** 2) / (2.0 * half_width)
    off = radius - half_width
    return np.maximum(np.hypot(u, v + off), np.hypot(u, v - off)) - radius


def polyline(Q, points):
    """Distance in 2D to a path through points."""
    Q = np.asarray(Q, dtype=np.float64)
    d = np.full(len(Q), np.inf)
    for a, b in zip(points, points[1:]):
        a, b = np.asarray(a, dtype=np.float64), np.asarray(b, dtype=np.float64)
        e = b - a
        t = np.clip(((Q - a) @ e) / max(e @ e, 1e-12), 0.0, 1.0)
        d = np.minimum(d, np.linalg.norm(Q - a - np.outer(t, e), axis=1))
    return d


def polygon(Q, points):
    """Signed distance in 2D to a closed polygon, negative inside."""
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


def chain(stops):
    """Weights handing along x from bone to bone: stops is [(bone, x)] from back to front, each bone whole at its x and
    easing into the next between them."""
    def weights(P):
        x = P[:, 0]
        out = {bone: np.zeros(len(P), dtype=np.float32) for bone, _ in stops}
        out[stops[0][0]] += (x <= stops[0][1]).astype(np.float32)
        out[stops[-1][0]] += (x > stops[-1][1]).astype(np.float32)
        for (a, xa), (b, xb) in zip(stops, stops[1:]):
            inside = (x > xa) & (x <= xb)
            s = np.clip((x - xa) / (xb - xa), 0.0, 1.0)
            s = s * s * (3 - 2 * s)
            out[a] += np.where(inside, 1 - s, 0).astype(np.float32)
            out[b] += np.where(inside, s, 0).astype(np.float32)
        return out
    return weights


class Kit:
    """What every part of him is made with: the sculpt, his materials, his scale and his layout."""

    def __init__(self, S, L, dims, spec):
        self.S, self.L, self.dims, self.spec = S, L, dims, spec
        self.mats = materials(S, spec)
        self.features = set(spec["features"])
        self.surge = "wildlightSurge" in self.features
        # cm at his layout's trunk, scaled to this one; his legs thicken with the kit's legScale as the generated
        # body's do.
        self.k = dims["trunk"] / TRUNK_CM
        self.g = self.k * spec.get("legScale", 1.0)
        self.hips = V(*L["pelvis"][0])
        self.chest = V(*L["spine_02"][1])
        head = float(np.linalg.norm(V(*L["head"][1]) - V(*L["head"][0])))
        self.head = HEAD_SCALE * math.sqrt(self.k * head / HEAD_CM)

    def p(self, bone, i):
        return V(*self.L[bone][i])

    def x_at(self, share):
        """Where along him a share of the way from his hips to his chest lies; behind the hips and ahead of the chest,
        shares are of his trunk's own span, so his rump and chest keep their shape however long his body is."""
        if share < 0.0:
            return self.hips[0] + share * SPAN_CM * self.k
        if share > 1.0:
            return self.chest[0] + (share - 1.0) * SPAN_CM * self.k
        return self.hips[0] + (self.chest[0] - self.hips[0]) * share

    def section(self, x):
        """The trunk's (centre z, half-height, half-width) at x."""
        xs = [self.x_at(s) for s, *_ in TRUNK]
        k = self.k
        return (self.hips[2] + np.interp(x, xs, [c for _, c, _, _ in TRUNK]) * k, np.interp(x, xs, [h for _, _, h, _ in TRUNK]) * k,
                np.interp(x, xs, [w for _, _, _, w in TRUNK]) * k)

    def on_trunk(self, x, degrees, sign, sink=0.0):
        """A point on the trunk at x, degrees round from the top of the back toward side sign, sink cm under its
        surface, and the way out there."""
        cz, hh, hw = self.section(x)
        a = math.radians(degrees)
        point = V(x, sign * math.sin(a) * hw, cz + math.cos(a) * hh)
        normal = unit(V(0, sign * math.sin(a) / max(hw, 1e-6), math.cos(a) / max(hh, 1e-6)))
        return point - normal * sink, normal

    def cone(self, name, a, b, ra, rb, mat, bones, protect=0.0):
        a, b = V(*a), V(*b)
        return tree.leaf(self.S, name, lambda P, a=a, b=b: sdf.round_cone(P, a, b, ra, rb), Box.around([a, b], max(ra, rb)), self.mats[mat], bones, protect)

    def blob(self, name, c, r, mat, bones, axes=None, protect=0.0):
        c, r = V(*c), V(*r)
        return tree.leaf(self.S, name, lambda P, c=c, r=r: sdf.ellipsoid(P, c, r, axes), Box(c - r.max(), c + r.max()), self.mats[mat], bones, protect)

    def shard(self, name, a, b, ra, rb, flat, mat, bones, thin=1.6, protect=0.0):
        a, b = V(*a), V(*b)
        return tree.leaf(self.S, name, lambda P, a=a, b=b, f=V(*flat): shard(P, a, b, ra, rb, f, thin), Box.around([a, b], max(ra, rb)),
                         self.mats[mat], bones, protect)


def build(S, L, dims, spec):
    """Moro's sculpt on his layout: (the whole, {"body": his skin under his markings, "sheets": none; a beast has no
    spring parts})."""
    kit = Kit(S, L, dims, spec)
    head = head_frame(kit)
    core = Union([trunk(kit), neck(kit)] + legs(kit) + [tail(kit), head_core(kit, head)], k=4.0 * kit.k)
    coat = Over([core] + timber(kit, core))
    lit = Over([coat, wildlight(kit, coat)])
    lit = Over([lit, hearts(kit, lit)])
    worn = Over([lit, ruff(kit), mane(kit), head_features(kit, head), antlers(kit, head), moss(kit), claws(kit), streamers(kit)])
    # Cut flat where he meets the ground: nothing of him sinks below it.
    worn = tree.Intersect(worn, Zone(lambda P: -P[:, 2], Box(V(-500, -500, -0.5), V(500, 500, 600))))
    return worn, {"body": core, "sheets": []}


# ---------------------------------------------------------------------------------------------- body
def trunk(kit):
    """One loft from the rump to the chest along his spine: broad haunches, a narrow tucked-up loin, a deep chest."""
    xs = [kit.x_at(s) for s, *_ in TRUNK]
    a, b = V(xs[0], 0, kit.hips[2]), V(xs[-1], 0, kit.hips[2])
    span = xs[-1] - xs[0]
    k = kit.k
    stations = [((x - xs[0]) / span, c * k, 0.0, h * k, w * k) for x, (_, c, h, w) in zip(xs, TRUNK)]
    shape = sdf.Loft(a, b, (0, 0, 1), stations, cap=3.0 * k)
    weights = chain([("pelvis", kit.x_at(0.08)), ("spine_01", kit.x_at(0.4)), ("spine_02", kit.x_at(0.72))])
    return tree.leaf(kit.S, "trunk", shape, Box.around(shape.bounds_points()), kit.mats["fur"], weights)


def neck(kit):
    """A thick neck rising from the chest to the head."""
    k = kit.k
    n0, n1 = kit.p("neck_01", 0), kit.p("neck_01", 1)
    a, b = n0 + V(4, 0, 6) * k, n1 + V(4, 0, 2) * k
    return kit.cone("neck", a, b, 21 * k, 16 * k, "fur", anatomy.along("spine_02", "neck_01", n0, n1, 0.05, 0.5))


def legs(kit):
    """Four thick legs of dark weathered wood on broad dark paws: forelegs straight down from the shoulder, hind legs
    from the haunch, the stifle forward and the hock back."""
    g = kit.g
    parts = []
    for side in ("l", "r"):
        # The top of each leg sits in under the shoulder or the haunch, so the leg grows out of the body rather than
        # hanging beside it.
        tuck = lambda p, share=0.3: V(p[0], p[1] * (1 - share), p[2])  # noqa: E731
        sh, el, wr = kit.p("upperarm_" + side, 0), kit.p("upperarm_" + side, 1), kit.p("lowerarm_" + side, 1)
        parts.append(kit.cone("upperarm_" + side, tuck(sh + V(2, 0, 8) * kit.k), el, 15 * g, 10.5 * g, "wood",
                              anatomy.along("spine_02", "upperarm_" + side, sh + V(0, 0, 16) * kit.k, el, 0.15, 0.45)))
        parts.append(kit.cone("forearm_" + side, el, wr, 11 * g, 8.5 * g, "wood", anatomy.rigid("lowerarm_" + side)))
        parts.append(kit.blob("forepaw_" + side, V(wr[0] + 9 * g, wr[1], 7 * g), (15 * g, 11.5 * g, 7.5 * g), "paw", anatomy.rigid("hand_" + side)))
        hp, st, hk, pt = kit.p("thigh_" + side, 0), kit.p("thigh_" + side, 1), kit.p("calf_" + side, 1), kit.p("foot_" + side, 1)
        parts.append(kit.cone("thigh_" + side, tuck(hp + V(-6, 0, 8) * kit.k), st, 17 * g, 10.5 * g, "wood",
                              anatomy.along("pelvis", "thigh_" + side, hp + V(0, 0, 18) * kit.k, st, 0.15, 0.45)))
        parts.append(kit.cone("calf_" + side, st, hk, 10.5 * g, 7 * g, "wood", anatomy.rigid("calf_" + side)))
        parts.append(kit.cone("hindfoot_" + side, hk, pt + V(2, 0, 4) * g, 7 * g, 7 * g, "wood", anatomy.rigid("foot_" + side)))
        parts.append(kit.blob("hindpaw_" + side, V(pt[0] + 4 * g, pt[1], 6.5 * g), (13.5 * g, 10.5 * g, 6.5 * g), "paw", anatomy.rigid("foot_" + side)))
    return parts


def claws(kit):
    """Four big pale claws at the front of every paw, curving down to the ground."""
    g = kit.g
    parts = []
    for side in ("l", "r"):
        for bone, front in (("hand_" + side, kit.p("lowerarm_" + side, 1)[0] + 21 * g), ("foot_" + side, kit.p("foot_" + side, 1)[0] + 14 * g)):
            y = kit.p(bone, 1)[1]
            for index, spread in enumerate((-1.5, -0.5, 0.5, 1.5)):
                root = V(front, y + spread * 4.6 * g, 7 * g)
                tip = root + V(11 * g, spread * 1.4 * g, -6.2 * g)
                parts.append(kit.cone("claw_%s_%d" % (bone, index), root, tip, 3.2 * g, 0.6 * g, "claw", anatomy.rigid(bone), protect=0.5))
    return Union(parts, k=0.5)


def tail(kit):
    """A short thick tail, hidden under the bark trailing off it."""
    k = kit.k
    t0, t3 = kit.p("tail_01", 0), kit.p("tail_03", 1)
    a, b = t0 + (t3 - t0) * 0.55 + V(0, 0, 4) * k, t3 + V(-8, 0, -2) * k
    return kit.cone("tail", a, b, 10 * k, 6 * k, "fur", tail_weights(kit))


def tail_weights(kit):
    """Down the tail by how far back a point lies: each tail bone whole a little behind its root."""
    return chain([(bone, kit.p(bone, 0)[0] - 6 * kit.k) for bone in ("tail_03", "tail_02", "tail_01")])


def streamers(kit):
    """Long shards of bark trailing back off his rump and tail, the end of his mane."""
    k = kit.k
    t0, t3 = kit.p("tail_01", 0), kit.p("tail_03", 1)
    root = t0 + (t3 - t0) * 0.5
    parts = []
    for index, (start, end, ra) in enumerate((((0, 0, 12), (-50, 0, 2), 6.0), ((-2, 9, 6), (-44, 20, -10), 5.5), ((-2, -9, 6), (-44, -20, -10), 5.5),
                                               ((-6, 6, -2), (-38, 13, -22), 5.0), ((-6, -6, -2), (-38, -13, -22), 5.0), ((-10, 0, -6), (-34, 0, -30), 4.5))):
        a, b = root + V(*start) * k, root + V(*end) * k
        parts.append(kit.shard("streamer_%d" % index, a, b, ra * k, 0.7 * k, V(0, 1, 0) if index == 0 else V(0, 0, 1), "bark", tail_weights(kit), thin=1.5))
    return Union(parts, k=2.0 * k)


# ---------------------------------------------------------------------------------------------- head
def head_frame(kit):
    """The head's frame: its origin at the head bone's root, x along the bone, z up."""
    h0, h1 = kit.p("head", 0), kit.p("head", 1)
    x = unit(h1 - h0)
    z = unit(V(0, 0, 1) - x * x[2])
    y = np.cross(z, x)
    return h0, np.stack([x, y, z], axis=1)


def in_head(head, parts, k):
    origin, axes = head
    return Placed(Union(parts, k=k), origin, axes)


def head_core(kit, head):
    """A wolf's head, long in the muzzle: a dark skull, a pale muzzle and jaw, full pale cheeks. (Head frame.)"""
    s = kit.head
    bones = anatomy.rigid("head")
    parts = [kit.blob("skull", V(6, 0, 4) * s, V(17, 13.5, 13) * s, "bark", bones, protect=0.4),
             kit.cone("muzzle", V(20, 0, 0.5) * s, V(42, 0, -3) * s, 9.5 * s, 6 * s, "cream", bones, protect=0.4),
             kit.cone("jaw", V(13, 0, -8) * s, V(38, 0, -8.5) * s, 7.5 * s, 4 * s, "cream", bones, protect=0.4)]
    for sign in (1.0, -1.0):
        parts.append(kit.blob("cheek_%d" % (sign > 0), V(12, sign * 9.5, -4) * s, V(11, 10, 9.5) * s, "cream", bones, protect=0.4))
    return in_head(head, parts, 2.0 * s)


def head_features(kit, head):
    """What reads of his face from above: a heavy dark brow, eyes burning blue with a streak of Wildlight running back
    from each, a dark nose, the cheek fur flaring back, and bark shards raking back off the crown into his mane."""
    s = kit.head
    bones = anatomy.rigid("head")
    parts = [kit.blob("nose", V(47.5, 0, -1) * s, V(3.6, 4.6, 3.4) * s, "nose", bones, protect=0.8)]
    for sign in (1.0, -1.0):
        side = "l" if sign > 0 else "r"
        parts.append(kit.cone("brow_" + side, V(12, sign * 7, 12) * s, V(27, sign * 8.6, 9.2) * s, 4.4 * s, 2.8 * s, "bark", bones, protect=0.6))
        parts.append(kit.blob("eye_" + side, V(27, sign * 9.6, 6.4) * s, V(4.6, 2.6, 2.6) * s, "eye", bones, protect=1.0))
        parts.append(kit.cone("eye_streak_" + side, V(23, sign * 10.2, 7.6) * s, V(6, sign * 12.6, 13.5) * s, 1.6 * s, 0.8 * s, "wildlight", bones,
                              protect=0.8))
        for index, (a, b, r) in enumerate((((8, 12, -4), (-12, 23, -12), 5.5), ((4, 13, 4), (-16, 22, 4), 5.0), ((10, 9, -11), (-6, 17, -24), 5.0),
                                           ((0, 12, 10), (-16, 18, 16), 4.5))):
            parts.append(kit.shard("cheek_tuft_%s_%d" % (side, index), V(a[0], sign * a[1], a[2]) * s, V(b[0], sign * b[1], b[2]) * s, r * s, 0.6 * s,
                                   V(0, 0, 1), "cream", bones, protect=0.3))
        parts.append(kit.shard("crown_%s" % side, V(2, sign * 6, 13) * s, V(-17, sign * 11, 25) * s, 4.5 * s, 0.6 * s, V(0, sign, 0.4), "bark", bones,
                               protect=0.3))
    parts.append(kit.shard("crown", V(-3, 0, 14) * s, V(-24, 0, 23) * s, 5.0 * s, 0.6 * s, V(0, 1, 0), "bark", bones, protect=0.3))
    return in_head(head, parts, 0.8 * s)


# Each antler's beam from its root on the brow, in four lengths (head frame, cm at the head's scale; y outward): up and
# out first, then swept back.
BEAM = ((-1, 16, 10), (-8, 12, 16), (-13, 6, 15), (-14, -2, 12))
BEAM_RADII = (5.5, 4.6, 3.6, 2.6, 0.9)
# Its tines: (the beam length they grow from, how far along it, their way out (y outward), their length), and which
# of them carry a leaf.
TINES = ((0, 0.35, (1, 0.15, 0.6), 16), (1, 0.0, (0.35, 0.1, 1), 22), (1, 0.6, (0.1, 0.5, 1), 20), (2, 0.0, (0.2, 0.1, 1), 24),
         (2, 0.5, (-0.2, 0.6, 0.8), 18), (3, 0.0, (0.1, 0.2, 1), 18), (3, 0.0, (-0.6, 0.4, 0.5), 14), (3, 0.6, (-0.1, 0.1, 1), 12))
LEAFED = (1, 3, 5)


def antlers(kit, head):
    """Great branching antlers of living wood off his brow, a crown of points: each beam sweeps up and out, then back,
    thick at its root; a brow tine forward, tines up and out along it, and its end forked; leaves grow on some tines and
    a sprig on each beam, lit while his Wildlight surges. (Head frame.)"""
    s = kit.head
    bones = anatomy.rigid("head")
    parts = []
    for sign in (1.0, -1.0):
        side = "l" if sign > 0 else "r"
        beam = [V(4, sign * 7, 12) * s]
        for step in BEAM:
            beam.append(beam[-1] + V(step[0], sign * step[1], step[2]) * s)
        for index in range(len(BEAM)):
            parts.append(kit.cone("beam_%s_%d" % (side, index), beam[index], beam[index + 1], BEAM_RADII[index] * s, BEAM_RADII[index + 1] * s, "horn", bones,
                                  protect=0.3))
        for index, (length, along, (x, y, z), reach) in enumerate(TINES):
            root = beam[length] + (beam[length + 1] - beam[length]) * along
            tip = root + unit(V(x, sign * y, z)) * reach * s
            parts.append(kit.cone("tine_%s_%d" % (side, index), root, tip, 2.4 * s, 0.6 * s, "horn", bones, protect=0.3))
            if index in LEAFED:
                parts.append(kit.blob("leaf_%s_%d" % (side, index), tip + V(0, 0, 1.5) * s, V(4.5, 3.2, 2.0) * s, "leaf", bones, protect=0.5))
        sprig = beam[1] + (beam[2] - beam[1]) * 0.35 + V(1, sign * 3, 0) * s
        parts.append(kit.blob("sprig_" + side, sprig, V(4.0, 3.0, 2.0) * s, "leaf", bones, protect=0.5))
    return in_head(head, parts, 1.2 * s)


# ---------------------------------------------------------------------------------------------- fur and bark
def ruff(kit):
    """A thick cream-white ruff: a collar of long tufts round his neck raking back toward his shoulders, a second ring
    standing out round its base, the throat pale up to the jaw, and a deep chest of fur, shaggy underneath."""
    k = kit.k
    n0, n1 = kit.p("neck_01", 0), kit.p("neck_01", 1)
    spine = anatomy.rigid("spine_02")
    neckw = anatomy.along("spine_02", "neck_01", n0, n1, 0.1, 0.6)
    parts = [kit.blob("chest_fur", n0 + V(13, 0, -12) * k, V(16, 24, 25) * k, "cream", spine),
             kit.cone("throat", n0 + V(14, 0, 2) * k, n1 + V(4, 0, -9) * k, 16 * k, 11 * k, "cream", neckw)]
    axis = unit(n1 - n0)
    up = unit(V(0, 0, 1) - axis * axis[2])
    rng = np.random.default_rng(kit.spec["seed"])
    for ring, (share, low, high, count, lean, (shortest, longest), radius) in enumerate(((0.42, 55, 305, 11, 0.55, (26, 32), 7.0),
                                                                                          (0.2, 70, 290, 9, 0.8, (20, 26), 6.5))):
        centre = n0 + (n1 - n0) * share + V(2, 0, 0) * k
        for index, degrees in enumerate(np.linspace(low, high, count)):
            a = math.radians(degrees)
            out = up * math.cos(a) + V(0, 1, 0) * math.sin(a)
            root = centre + out * 16 * k
            tip = root + unit(-axis + out * lean + V(0, 0, -0.2 * ring)) * rng.uniform(shortest, longest) * k
            parts.append(kit.shard("ruff_%d_%d" % (ring, index), root, tip, radius * k, 0.8 * k, out, "cream", neckw if out[2] > -0.3 else spine, thin=1.6))
    for index, (x, y) in enumerate(((-2, 0), (8, 9), (8, -9), (18, 4), (18, -5))):
        root = n0 + V(x, y, -32) * k
        tip = root + V(-6, y * 0.3, -14) * k
        parts.append(kit.shard("chest_tuft_%d" % index, root, tip, 6 * k, 0.8 * k, V(0, 1, 0), "cream", spine, thin=1.4))
    return Union(parts, k=2.5 * k)


def mane(kit):
    """His mane of dark bark shards: raking back along the top of his neck and spine, longest over the shoulders, a
    row down each side of his back, overlapping like bark."""
    k = kit.k
    parts = []
    n0, n1 = kit.p("neck_01", 0), kit.p("neck_01", 1)
    axis = unit(n1 - n0)
    up = unit(V(0, 0, 1) - axis * axis[2])
    neck_top = [n0 + V(4, 0, 6) * k + (n1 - n0 + V(0, 0, -4) * k) * t + up * (21 - 5 * t) * k for t in (0.85, 0.55, 0.25)]
    for index, (root, length) in enumerate(zip(neck_top, (24, 30, 35))):
        tip = root + unit(V(-0.85, 0, 0.55)) * length * k
        parts.append(kit.shard("mane_neck_%d" % index, root - up * 3 * k, tip, 6 * k, 0.7 * k, V(0, 1, 0), "bark",
                               anatomy.along("spine_02", "neck_01", n0, n1, 0.05, 0.5)))
    spine = chain([("pelvis", kit.x_at(0.08)), ("spine_01", kit.x_at(0.4)), ("spine_02", kit.x_at(0.72))])
    centre_row = [(0.72, 36, 0.55), (0.58, 33, 0.52), (0.44, 30, 0.5), (0.3, 26, 0.48), (0.16, 23, 0.45), (0.02, 22, 0.42), (-0.12, 23, 0.4),
                  (-0.25, 25, 0.38), (-0.36, 28, 0.35)]
    rng = np.random.default_rng(kit.spec["seed"] + 1)
    for index, (share, length, rise) in enumerate(centre_row):
        root, _ = kit.on_trunk(kit.x_at(share), 0, 1, sink=3 * k)
        # Each a little askew, as bark grows.
        tip = root + unit(V(-1, rng.uniform(-0.12, 0.12), rise + rng.uniform(-0.08, 0.08))) * length * rng.uniform(0.9, 1.1) * k
        parts.append(kit.shard("mane_%d" % index, root, tip, 7.5 * k, 0.7 * k, V(0, 1, 0), "bark", spine, thin=1.8))
    for sign in (1.0, -1.0):
        for index, (share, length) in enumerate(((0.66, 28), (0.4, 23), (0.14, 19), (-0.1, 19), (-0.3, 22))):
            root, normal = kit.on_trunk(kit.x_at(share), 44, sign, sink=3 * k)
            tip = root + unit(V(-1, 0, 0) + normal * 0.55) * length * k
            parts.append(kit.shard("mane_side_%d_%d" % (sign > 0, index), root, tip, 5.5 * k, 0.7 * k, normal, "bark", spine, thin=1.8))
    return Union(parts, k=2.0 * k)


# The timber's plates over each shoulder and haunch, seen from the side about its joint (x forward, z up, cm at
# TRUNK_CM): (centre x, centre z, half-length, half-width, the way its grain runs in degrees off forward), laid down and
# back over one another like bark, alternately lighter and darker.
SHOULDER_PLATES = ((2, 30, 18, 8, 75), (-12, 18, 18, 7, 65), (12, 22, 20, 8, 85), (-4, 4, 18, 7, 70), (14, 0, 18, 7, 90), (-18, 0, 15, 6, 60),
                   (4, -12, 15, 6, 80), (20, -10, 14, 6, 95))
HAUNCH_PLATES = ((-8, 32, 20, 8, 100), (10, 24, 18, 7, 115), (-24, 22, 18, 7, 85), (2, 10, 18, 7, 105), (-14, 6, 18, 7, 95), (18, 8, 15, 6, 120),
                 (-4, -8, 16, 6, 100), (-26, -4, 14, 6, 80), (10, -8, 14, 6, 110))


def timber(kit, core):
    """Weathered grey timber through his shoulders and haunches: long pointed plates laid over his fur and the tops of
    his legs on both flanks, down and back along the grain, overlapping like bark, alternately lighter and darker; the
    ridge of his back stays fur. (Layers over the skin, in regions seen from the side.)"""
    k = kit.k
    sh, hp = kit.p("upperarm_l", 0), kit.p("thigh_l", 0)
    plates = [(V(sh[0] + x * k, sh[2] + z * k), a * k, b * k, math.radians(angle)) for x, z, a, b, angle in SHOULDER_PLATES] + \
             [(V(hp[0] + x * k, hp[2] + z * k), a * k, b * k, math.radians(angle)) for x, z, a, b, angle in HAUNCH_PLATES]
    xs = [c[0] for c, *_ in plates]
    zs = [c[1] for c, *_ in plates]
    reach = 22 * k
    bounds = Box(V(min(xs) - reach, -80, min(zs) - reach), V(max(xs) + reach, 80, max(zs) + reach))
    layers = []
    # The darker plates first, the lighter laid over them, each standing a little prouder than what it covers.
    for tone, name in enumerate(("timber_dark", "timber")):
        mine = plates[1 - tone::2]

        def region(P, mine=mine):
            Q = P[:, [0, 2]]
            d = np.min([vesica(Q, c, a, b, angle) for c, a, b, angle in mine], axis=0)
            return np.maximum(d, 14 * k - np.abs(P[:, 1]))
        base = Over([core] + layers) if layers else core
        layers.append(Shell(kit.S, name, base, 0.0, (1.2 + 0.8 * tone) * k, Zone(region, bounds), kit.mats[name], hem=0.4 * k))
    return layers

def moss(kit):
    """Moss and small growing things on his back, as though the forest has begun using him for ground: low cushions of
    differing greens along the ridge, and a few sprouts standing out of them."""
    k = kit.k
    spine = chain([("pelvis", kit.x_at(0.08)), ("spine_01", kit.x_at(0.4)), ("spine_02", kit.x_at(0.72))])
    parts = []
    for index, (share, across, size) in enumerate(((0.52, 9, 1.0), (0.23, -10, 1.1), (-0.05, 8, 0.95), (-0.22, -7, 0.9))):
        point, normal = kit.on_trunk(kit.x_at(share), math.degrees(math.atan2(across, 20)), 1.0, sink=2 * k)
        parts.append(kit.blob("moss_%d" % index, point, V(12, 9, 4.5) * k * size, "moss" if index % 2 else "moss_dark", spine))
        if index % 2 == 0:
            stalk = point + normal * 3 * k
            tip = stalk + V(2, across * 0.15, 12) * k
            parts.append(kit.cone("sprout_%d" % index, stalk, tip, 1.2 * k, 0.6 * k, "moss", spine, protect=0.5))
            for sign in (1.0, -1.0):
                parts.append(kit.blob("sprout_leaf_%d_%d" % (index, sign > 0), tip + V(1, sign * 3.5, -1) * k, V(4.5, 3, 1.4) * k, "sprout", spine,
                                      protect=0.5))
    return Union(parts, k=1.5 * k)


# ---------------------------------------------------------------------------------------------- Wildlight
def side_zone(shape, bounds, outward):
    """A region seen from the side: shape(Q) over (x, z), on whichever flank lies outward of |y| = outward."""
    return Zone(lambda P: np.maximum(shape(P[:, [0, 2]]), outward - np.abs(P[:, 1])), bounds)


def wildlight(kit, coat):
    """Wildlight growing along limb and grain, never laid out like glyphwork: an eye-shaped panel at each shoulder and
    haunch, leaf-shaped panels on his flanks, a vein along each side of his back sending branches down between the
    plates to the leaves, and veins down the outside of every leg. Surging, the panels swell and the veins thicken and
    climb his neck. (A layer over his coat, in regions seen from the side and from above.)"""
    k = kit.k
    grow = 1.35 if kit.surge else 1.0
    vein = (1.5 if kit.surge else 1.0) * k
    sh, hp = kit.p("upperarm_l", 0), kit.p("thigh_l", 0)
    el, wr, toe = kit.p("upperarm_l", 1), kit.p("lowerarm_l", 1), kit.p("hand_l", 1)
    st, hk, ft = kit.p("thigh_l", 1), kit.p("calf_l", 1), kit.p("foot_l", 1)
    xz = lambda p, dx=0.0, dz=0.0: (p[0] + dx * k, p[2] + dz * k)  # noqa: E731
    eyes = [(xz(sh, -4, -6), 13 * grow * k, 6 * grow * k, 0.12), (xz(hp, 1, -6), 13 * grow * k, 6 * grow * k, -0.12)]
    # Mid-flank, by how far along him (his layout may lengthen his body).
    mid = lambda share, dz: (kit.x_at(share), sh[2] + dz * k)  # noqa: E731
    leaves = [(mid(0.47, 12), 16 * grow * k, 4.6 * grow * k, 0.12), (xz(sh, -22, -2), 11 * grow * k, 3.8 * grow * k, -0.35),
              (xz(hp, 20, 4), 12 * grow * k, 4.2 * grow * k, 0.4)]
    # Branches from the back's vein down the flank to each leaf's stem, and the legs' veins from each eye to the paw.
    branches = [[mid(0.49, 34), mid(0.43, 24), mid(0.34, 14)], [xz(sh, -14, 36), xz(sh, -18, 16), xz(sh, -30, 2)],
                [xz(hp, 12, 34), xz(hp, 18, 20), xz(hp, 30, 8)]]
    leg_veins = [[xz(sh, -4, -14), xz(el, 1, 6), xz(el, 3, -10), xz(wr, 1, 8), xz(toe, -4, 6)],
                 [xz(hp, 1, -14), xz(st, -3, 2), xz(st, -10, -10), xz(hk, 1, 4), xz(ft, -4, 6)]]

    def flank(Q):
        d = np.full(len(Q), np.inf)
        for centre, a, b, angle in leaves:
            d = np.minimum(d, vesica(Q, centre, a, b, angle))
        for path in branches:
            d = np.minimum(d, polyline(Q, path) - vein)
        return d

    def limbs(Q):
        d = np.full(len(Q), np.inf)
        for centre, a, b, angle in eyes:
            d = np.minimum(d, vesica(Q, centre, a, b, angle))
        for path in leg_veins:
            d = np.minimum(d, polyline(Q, path) - vein)
        return d
    # The back's vein in each side of the ridge, seen from above.
    back = [(kit.x_at(s), kit.section(kit.x_at(s))[2] * 0.62) for s in (0.86, 0.6, 0.3, 0.0, -0.22, -0.38)]
    top = kit.section(kit.x_at(0.3))[0]

    def ridge(P):
        Q = np.stack([P[:, 0], np.abs(P[:, 1])], axis=1)
        return np.maximum(polyline(Q, back) - vein, top - P[:, 2])
    span = Box(V(kit.x_at(-0.6), -80, 0), V(kit.x_at(1.4), 80, 200))
    regions = [side_zone(flank, span, 16 * k), side_zone(limbs, span, kit.p("upperarm_l", 0)[1] + 2 * k), Zone(ridge, span)]
    if kit.surge:
        # Surging, the veins run on up his neck toward the antlers.
        n0, n1 = kit.p("neck_01", 0), kit.p("neck_01", 1)
        axis = unit(n1 - n0)
        up = unit(V(0, 0, 1) - axis * axis[2])
        climb = [xz(n0 + V(4, 0, 6) * k + (n1 - n0) * t + up * 10 * k) for t in (0.0, 0.5, 1.0)]
        regions.append(side_zone(lambda Q: polyline(Q, climb) - vein, span, 10 * k))
    fields = regions

    def region(P):
        out = fields[0].distance(P)
        for zone in fields[1:]:
            out = np.minimum(out, zone.distance(P))
        return out
    shell = Shell(kit.S, "wildlight", coat, 0.0, 0.9 * k, Zone(region, span), kit.mats["wildlight"], hem=0.3 * k)
    shell.part.protect = 0.7
    kit.eye_panels = eyes
    return shell


def hearts(kit, lit):
    """The pale heart of each eye-shaped panel."""
    k = kit.k
    centres = [c for c, *_ in kit.eye_panels]
    r = (4.4 if kit.surge else 3.4) * k

    def shape(Q):
        return np.min([np.hypot(Q[:, 0] - c[0], Q[:, 1] - c[1]) - r for c in centres], axis=0)
    span = Box(V(kit.x_at(-0.6), -80, 0), V(kit.x_at(1.4), 80, 200))
    shell = Shell(kit.S, "heart", lit, 0.0, 1.5 * k, side_zone(shape, span, kit.p("upperarm_l", 0)[1] + 2 * k), kit.mats["heart"], hem=0.3 * k)
    shell.part.protect = 0.8
    return shell
