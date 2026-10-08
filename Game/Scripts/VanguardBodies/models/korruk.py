"""Korruk, The Splinterbeast (ADR-069): a low six-legged predator of the Shatterdeep, read by his silhouette from the game
camera. Character Bible §8 and his splash art: a long, broad, heavy animal built close to the ground on six load-bearing
legs, a heavy wedge-shaped skull of faceted plate carried low and forward with a single deep red ocular burning in it;
his back and flanks armoured in pale bone-coloured mineral plates laid in overlapping rows like a pangolin's, scuffed and
chipped, over dark hide; a dense crest of hollow crimson crystal spines rising from his shoulders and back, longest over
the shoulders and tapering down his tail, each open at its bevelled tip; smaller crimson crystal set into the plates of
his chest and limbs; big claws on every foot. A living animal, never machinery.

Low poly and flat-coloured (author 2026-10-07): the big forms that make his outline, each a flat colour the toon
material shades. He stands as his archetype lays him out (ADR-064): every part rides the bone beneath it (the beast
archetype has no spring parts, so nothing of him hangs loose). His crystal takes its colour from his kit's accent (crimson
by canon; another spine colour is a cosmetic variant of the same animal)."""
import numpy as np

from ..sculpt import anatomy, paint, sdf, tree
from ..sculpt.anatomy import V, unit
from ..sculpt.garments import surface_points
from ..sculpt.paint import hashed
from ..sculpt.tree import Box, Over, Placed, Shell, Subtract, Union, Zone

# Sampled from the splash art (sRGB), its darks lifted so they do not go black under the toon light: the bone-pale plates
# and their scuffed, older ones; the dark hide beneath them; claws of dark horn; teeth; the mouth.
PALETTE = {"plate": (0.9, 0.82, 0.69), "scuffed": (0.77, 0.67, 0.56), "hide": (0.38, 0.31, 0.29), "claw": (0.56, 0.52, 0.48),
           "teeth": (0.93, 0.89, 0.8), "maw": (0.3, 0.16, 0.16)}
# The share of the plates that are scuffed, and of those chipped at their free edge.
SCUFFED_SHARE = 0.3
CHIPPED_SHARE = 0.2
# The seams (cm) where the hide shows: under each row's free edge, and between the plates of a row.
ROW_SEAM = 1.8
COLUMN_SEAM = 1.4
# How far a plate's free edge bulges back at its middle, as a share of its row: a scale's rounded end.
SCALLOP = 0.4
# How far each elbow and stifle bows out from under him, in trunks.
ELBOW_BOW = 0.14
# His skull drawn this much larger than his kit's trunk sizes it (so it reads from the game camera), and pitched this
# many degrees down from level along his head bone's head (carried low).
HEAD_SCALE = 1.12
HEAD_PITCH = 32.0
# A hexagonal crystal's face normals across its axis (each face and its opposite).
HEX = [np.array([np.cos(a), np.sin(a)]) for a in np.radians([0.0, 60.0, 120.0])]


def materials(S, spec):
    """Every material Korruk is coloured in, flat (the toon material shades it). His crystal is his kit's accent deepened:
    a deep face and a lit one, so each crystal reads cut; the open bores of his spines and his ocular glow in it."""
    mats = {name: S.material(name, colour) for name, colour in PALETTE.items()}
    glow = paint.seen(np.asarray(spec["accent"], dtype=np.float64)[:3])
    mats["crystal"] = S.material("crystal", tuple(float(v) for v in glow ** 3.0 * 0.62))
    mats["crystal_lit"] = S.material("crystal_lit", tuple(float(v) for v in glow ** 1.8 * 0.92))
    mats["bore"] = S.material("bore", tuple(float(v) for v in glow + (1.0 - glow) * 0.3), glow=True)
    mats["ocular"] = S.material("ocular", tuple(float(v) for v in glow ** 1.6), glow=True)
    return mats


# ---------------------------------------------------------------------------------------------- helpers
def p0(L, bone):
    return V(*L[bone][0])


def p1(L, bone):
    return V(*L[bone][1])


def smooth(x):
    x = np.clip(x, 0.0, 1.0)
    return x * x * (3.0 - 2.0 * x)


def convex(P, planes):
    """The solid inside every plane (point, outward normal): a faceted form (a bound on the distance, exact on its faces)."""
    d = None
    for point, normal in planes:
        e = (P - point) @ unit(normal)
        d = e if d is None else np.maximum(d, e)
    return d


def plane_through(a, b, c, inside):
    """The plane through a, b and c, its normal turned away from inside."""
    n = unit(np.cross(b - a, c - a))
    return (a, n if (inside - a) @ n < 0 else -n)


def crystal(P, base, R, length, rb, rt, cut=None, sink=0.35):
    """A hexagonal crystal standing from base along R's third column, its apothem rb at its base easing to rt at length,
    sunk sink of its length into what it is set in; cut (point, normal) bevels its tip."""
    q = (P - base) @ R
    h = q[:, 2]
    r = rb + (rt - rb) * np.clip(h / length, 0.0, 1.0)
    d = np.max(np.stack([np.abs(q[:, :2] @ n) for n in HEX]), axis=0) - r
    d = np.maximum(d, np.maximum(-h - length * sink, h - length))
    if cut is not None:
        d = np.maximum(d, (P - cut[0]) @ cut[1])
    return d


class Bones:
    """Korruk's skinning along his trunk: the hips, the belly and the chest, blending where they meet."""

    def __init__(self, L, T):
        self.hips_end = L["pelvis"][1][0]
        self.belly_end = L["spine_01"][1][0]
        self.blend = T * 0.3

    def trunk(self, P):
        x = P[:, 0]
        hips = 1.0 - smooth((x - (self.hips_end - self.blend)) / (2 * self.blend))
        chest = smooth((x - (self.belly_end - self.blend)) / (2 * self.blend))
        belly = np.clip(1.0 - hips - chest, 0.0, 1.0)
        return {"pelvis": hips.astype(np.float32), "spine_01": belly.astype(np.float32), "spine_02": chest.astype(np.float32)}

    def bone_at(self, x):
        """The trunk's bone over x (cm along him)."""
        return "pelvis" if x < self.hips_end else ("spine_01" if x < self.belly_end else "spine_02")


# ---------------------------------------------------------------------------------------------- plates
class Patch:
    """Rows of overlapping plates over a rounded stretch of him, as a pangolin's lie: an axis from a running along w
    toward where the plates' free edges point (toward his tail, down his legs), rows row cm long, columns of span degrees
    round the axis either side of u, staggered row by row, out to reach from the axis. Each plate's free edge bulges back
    in a rounded scale's end over the front of the row behind and stands lift proud of it; some edges are chipped."""

    def __init__(self, a, w, u, length, row, columns, span, lift, reach, salt):
        self.a, self.w = V(*a), unit(w)
        u = V(*u) - self.w * (V(*u) @ self.w)
        self.u = unit(u)
        self.v = np.cross(self.w, self.u)
        self.length, self.row, self.columns, self.lift, self.reach, self.salt = length, row, columns, lift, reach, salt
        self.half = np.radians(span) * 0.5
        self.width = 2.0 * self.half / columns
        self.box = Box.around([self.a, self.a + self.w * self.length], reach)

    def column(self, k, theta):
        """Which column of row k each point lies in, and how far across it (0 to 1)."""
        c = (theta + self.half) / self.width + 0.5 * np.mod(k, 2.0)
        j = np.floor(c)
        return j, c - j

    def edge(self, k, theta):
        """Where row k begins along the axis at each point: under the rounded free edge of row k - 1's plate there."""
        _, fc = self.column(k - 1, theta)
        u = 2.0 * fc - 1.0
        return (k + SCALLOP * (1.0 - u * u)) * self.row

    def fields(self, P):
        """How far each point lies inside its plate (cm, negative outside every plate), how proud its plate stands there,
        and whether its plate is a scuffed one."""
        Q = np.asarray(P, dtype=np.float64) - self.a
        s = Q @ self.w
        x, y = Q @ self.u, Q @ self.v
        r = np.maximum(np.sqrt(x * x + y * y), 1.0)
        theta = np.arctan2(y, x)
        k = np.floor(s / self.row)
        start = self.edge(k, theta)
        behind = s < start
        k = np.where(behind, k - 1, k)
        start = np.where(behind, self.edge(k, theta), start)
        end = self.edge(k + 1, theta)
        fr = np.clip((s - start) / np.maximum(end - start, 1e-6), 0.0, 1.0)
        j, fc = self.column(k, theta)
        arc = self.width * r
        inside = np.minimum(np.minimum(s - start, end - s) - ROW_SEAM * 0.5, np.minimum(fc, 1.0 - fc) * arc - COLUMN_SEAM * 0.5)
        inside = np.minimum(inside, np.minimum((self.half - np.abs(theta)) * r, np.minimum(s, self.length - s)))
        inside = np.minimum(inside, self.reach - r)
        # Some plates chipped out of their free edge.
        key = k * 37.0 + j * 11.0
        chipped = hashed(key, self.salt + 3.0) < CHIPPED_SHARE
        side = np.where(hashed(key, self.salt + 5.0) < 0.5, -1.0, 1.0)
        bite = np.hypot((fc - 0.5 - side * 0.22) * arc, end - s) - min(self.row, 16.0) * 0.24
        inside = np.where(chipped, np.minimum(inside, bite), inside)
        rise = self.lift * fr ** 1.3
        scuffed = hashed(key, self.salt) < SCUFFED_SHARE
        return inside, rise, scuffed


def plating(patches):
    """The plates' two regions (the bone-pale and the scuffed) as zones, and how proud the plates stand: a point takes the
    patch it lies deepest inside."""
    box = patches[0].box
    for patch in patches[1:]:
        box = box.union(patch.box)

    def best(P):
        near = Box.around(P, 0.0)
        inside = np.full(len(P), -1.0e3)
        rise = np.zeros(len(P))
        scuffed = np.zeros(len(P), dtype=bool)
        for patch in patches:
            if patch.box.gap(near) > 0.0:
                continue
            i, r, s = patch.fields(P)
            better = i > inside
            inside, rise, scuffed = np.where(better, i, inside), np.where(better, r, rise), np.where(better, s, scuffed)
        return inside, rise, scuffed

    def region(want):
        def distance(P):
            inside, _, scuffed = best(P)
            return np.where(scuffed == want, -inside, np.abs(inside) + ROW_SEAM)
        return Zone(distance, box)
    return region(False), region(True), (lambda P: best(P)[1])


def patches(L, T, loft, legs):
    """Where his plates lie: over his back and flanks (the belly stays hide), his neck, the top of his tail, and the outer
    face of each upper leg and foreleg, every plate's free edge toward his tail or down his leg."""
    out = []
    front = loft.a + loft.w * loft.length
    # The trunk: from the chest back to the rump, over the top and down the flanks.
    out.append(Patch(front + loft.w * T * 0.1, -loft.w, loft.u, loft.length + T * 0.1, T * 0.52, 7, 240, 4.0, T * 1.4, 11.0))
    # The neck: from behind the head back to the shoulders.
    n0, n1 = p0(L, "neck_01"), p1(L, "neck_01")
    out.append(Patch(n1, n0 - n1, V(0, 0, 1), np.linalg.norm(n1 - n0) + T * 0.1, T * 0.42, 6, 250, 2.8, T * 1.0, 23.0))
    # The tail's top, toward its tip.
    t0, t3 = p0(L, "tail_01"), p1(L, "tail_03")
    start = t0 + unit(t3 - t0) * T * 0.3
    out.append(Patch(start, t3 - t0, V(0, 0, 1), np.linalg.norm(t3 - start), T * 0.4, 3, 190, 2.4, T * 0.55, 31.0))
    # Each leg's shoulder or haunch, down to its knee, and down the outside of the forelegs.
    for n, (upper, lower, foot, parent, size) in enumerate(legs):
        for side, sign in (("l", 1.0), ("r", -1.0)):
            u0, u1 = p0(L, upper + "_" + side), p1(L, upper + "_" + side)
            out.append(Patch(u0, u1 - u0, V(0, sign, 0.3), np.linalg.norm(u1 - u0) + T * 0.1, T * 0.42 * size, 3, 210, 2.8, T * 0.6 * size,
                             41.0 + n * 7 + sign))
            if upper == "upperarm":
                l1 = p1(L, lower + "_" + side)
                out.append(Patch(u1, l1 - u1, V(0.6, sign, 0), np.linalg.norm(l1 - u1) * 0.7, T * 0.33, 2, 170, 2.2, T * 0.45, 61.0 + sign))
    return out


# ---------------------------------------------------------------------------------------------- build
def build(S, L, dims, spec):
    """Korruk's sculpt on his layout: (the whole, {"body": his hide, under the plates and crystal, "sheets": none})."""
    mats = materials(S, spec)
    T = dims["trunk"]
    bones = Bones(L, T)
    legs = [("upperarm", "lowerarm", "hand", "spine_02", 1.0), ("thigh", "calf", "foot", "pelvis", 1.0)]
    if dims.get("six"):
        legs.insert(1, ("midleg_upper", "midleg_lower", "midleg_foot", "spine_01", 0.85))
    trunk, loft = trunk_core(S, L, T, mats, bones)
    core = [trunk, neck_core(S, L, T, mats)] + tail_core(S, L, T, mats)
    for upper, lower, foot, parent, size in legs:
        for side in ("l", "r"):
            core += leg_core(S, L, T, mats, upper, lower, foot, parent, side, size)
    body = Union(core, k=T * 0.18)
    pale, scuffed, rise = plating(patches(L, T, loft, legs))
    plates = [Shell(S, "plates", body, 0.0, 1.6, pale, mats["plate"], hem=0.5, displace=rise, reach=4.0),
              Shell(S, "scuffed", body, 0.0, 1.6, scuffed, mats["scuffed"], hem=0.5, displace=rise, reach=4.0)]
    worn = Over([body] + plates + [head(S, L, T, mats), crest(S, L, T, mats, body, bones, loft, spec["seed"]), set_crystal(S, L, T, mats, body, legs, loft),
                                   claws(S, L, T, mats, legs)])
    # Cut flat where he meets the ground: nothing of him sinks below it.
    worn = tree.Intersect(worn, Zone(lambda P: -P[:, 2], Box(V(-500, -500, -0.5), V(500, 500, 600))))
    return worn, {"body": body, "sheets": []}


# ---------------------------------------------------------------------------------------------- the hide
def trunk_core(S, L, T, mats, bones):
    """His trunk: one long broad body from the rump to the chest, wider than it is deep and deepest through the
    shoulders, its belly hung low between his legs. Returns its part and its loft (to lay plates and crystal on)."""
    hips, chest = p0(L, "pelvis"), p1(L, "spine_02")
    a = hips + V(-T * 0.55, 0, -T * 0.1)
    # Its chest heaves forward round the root of the neck, so the head comes out of the shoulders.
    b = chest + V(T * 0.62, 0, -T * 0.08)
    stations = [(0.0, -T * 0.1, 0, T * 0.45, T * 0.5, 2.2), (0.1, -T * 0.16, 0, T * 0.8, T * 0.88, 2.3), (0.25, -T * 0.2, 0, T * 0.92, T * 1.0, 2.3),
                (0.45, -T * 0.22, 0, T * 0.92, T * 1.0, 2.3), (0.65, -T * 0.22, 0, T * 0.98, T * 1.1, 2.3), (0.82, -T * 0.2, 0, T * 1.02, T * 1.14, 2.3),
                (0.94, -T * 0.22, 0, T * 0.8, T * 0.94, 2.3), (1.0, -T * 0.25, 0, T * 0.5, T * 0.6, 2.2)]
    loft = sdf.Loft(a, b, V(0, 0, 1), stations, cap=T * 0.2)
    part = tree.leaf(S, "trunk", loft, Box.around(loft.bounds_points()), mats["hide"], bones.trunk)
    return part, loft


def on_trunk(loft, t, phi):
    """A point on the trunk's surface (t along it from the rump, phi degrees round from the top of the back toward his
    left) and the way out there."""
    i = int(round(np.clip(t, 0.0, 1.0) * (loft.SAMPLES - 1)))
    cu, cv, hu, hv = loft.curve[i, :4]
    f = np.radians(phi)
    centre = loft.a + loft.w * loft.length * t
    # The loft's v runs to his right (its frame: forward, up, and their cross).
    point = centre + loft.u * (cu + hu * np.cos(f)) - loft.v * (cv + hv * np.sin(f))
    normal = unit(loft.u * (np.cos(f) / hu) - loft.v * (np.sin(f) / hv))
    return point, normal


def neck_core(S, L, T, mats):
    """A short thick neck carrying the head low and forward from the chest, ending inside the back of the skull."""
    n0, n1 = p0(L, "neck_01"), p1(L, "neck_01")
    a, b = n0 + V(-T * 0.15, 0, -T * 0.05), n1 - unit(n1 - n0) * T * 0.3
    return tree.leaf(S, "neck", lambda P: sdf.round_cone(P, a, b, T * 0.8, T * 0.55), Box.around([a, b], T * 0.82), mats["hide"],
                     anatomy.along("spine_02", "neck_01", n0, n1, 0.05, 0.45))


def tail_core(S, L, T, mats):
    """A heavy tail tapering from the rump along its three bones."""
    joints = [p0(L, "tail_01"), p0(L, "tail_02"), p0(L, "tail_03"), p1(L, "tail_03")]
    radii = [T * 0.48, T * 0.34, T * 0.22, T * 0.08]
    parts = []
    for k in range(3):
        a, b, ra, rb = joints[k], joints[k + 1], radii[k], radii[k + 1]
        parts.append(tree.leaf(S, "tail_%d" % (k + 1), lambda P, a=a, b=b, ra=ra, rb=rb: sdf.round_cone(P, a, b, ra, rb), Box.around([a, b], ra),
                               mats["hide"], anatomy.rigid("tail_%02d" % (k + 1))))
    return parts


def paw_of(L, T, foot, side, size):
    """Where a paw's pad sits: under the end of its foot, its sole on the ground."""
    f0, f1 = p0(L, foot + "_" + side), p1(L, foot + "_" + side)
    pad = f0 + (f1 - f0) * 0.6
    return V(pad[0], pad[1], T * 0.19 * size)


def leg_core(S, L, T, mats, upper, lower, foot, parent, side, size):
    """One leg's hide: a heavy upper leg blending from the trunk, a lower leg, and a broad paw flat on the ground. Its
    elbow or stifle bows out a little from under him, a predator's braced stance rather than a pillar."""
    u0, u1 = p0(L, upper + "_" + side), p1(L, upper + "_" + side)
    u1 = u1 + V(0, (1.0 if side == "l" else -1.0) * T * ELBOW_BOW * size, 0)
    l1 = p1(L, lower + "_" + side)
    f0 = p0(L, foot + "_" + side)
    up = V(0, 0, 1)
    pad = paw_of(L, T, foot, side, size)
    radii = V(T * 0.38, T * 0.33, T * 0.21) * size
    return [tree.leaf(S, upper + "_" + side, lambda P: sdf.round_cone(P, u0, u1, T * 0.45 * size, T * 0.34 * size), Box.around([u0, u1], T * 0.46 * size),
                      mats["hide"], anatomy.along(parent, upper + "_" + side, u0 + up * T * 0.3, u0 - up * T * 0.3, 0.2, 0.8)),
            tree.leaf(S, lower + "_" + side, lambda P: sdf.round_cone(P, u1, l1, T * 0.32 * size, T * 0.27 * size), Box.around([u1, l1], T * 0.33 * size),
                      mats["hide"], anatomy.rigid(lower + "_" + side)),
            tree.leaf(S, foot + "_" + side, lambda P: np.minimum(sdf.ellipsoid(P, pad, radii), sdf.round_cone(P, f0, pad, T * 0.27 * size, T * 0.22 * size)),
                      Box.around([pad, f0], float(max(radii)) + T * 0.28), mats["hide"], anatomy.rigid(foot + "_" + side))]


# ---------------------------------------------------------------------------------------------- crystal
def crystal_parts(S, name, base, direction, length, rb, mats, bones, hollow=False, lit=V(1, 0, 0.2), protect=0.7):
    """One crimson crystal standing from base along direction: hexagonal, tapering, half its faces lit and half deep so it
    reads cut. A hollow one (a spine he fires) is bevelled across its tip like a quill and open there, its bore glowing:
    the hollowness stays visible from above."""
    axis = unit(direction)
    R = sdf.frame(axis, lit)
    rt = rb * (0.72 if hollow else 0.12)
    cut = None
    if hollow:
        # Bevelled to face up and a little ahead, the cut through the axis short of the tip's point.
        n = unit(V(0, 0, 1) + unit(V(lit[0], lit[1], 0)) * 0.4)
        cut = (base + axis * (length - rt * 1.5), n)
        protect = 1.0
    half = R[:, 0]
    box = Box.around([base - axis * length * 0.35, base + axis * length], rb * 1.2)
    faces = [tree.leaf(S, name + "_lit", lambda P: np.maximum(crystal(P, base, R, length, rb, rt, cut), -((P - base) @ half) - 0.3), box,
                       mats["crystal_lit"], bones, protect),
             tree.leaf(S, name + "_deep", lambda P: np.maximum(crystal(P, base, R, length, rb, rt, cut), (P - base) @ half - 0.3), box,
                       mats["crystal"], bones, protect)]
    node = Union(faces)
    if hollow:
        mouth = cut[0]
        # A deep bore with thin walls, so the reduction keeps it open.
        a, b = mouth - axis * rt * 2.6, base + axis * (length + rt * 2.0)
        r = rt * 0.66
        bore = tree.leaf(S, name + "_bore", lambda P: sdf.cylinder(P, a, b, r), Box.around([a, b], r), mats["bore"], bones, 1.0)
        node = Subtract(node, bore, label=bore.part.label)
    return node


# The crest's profile along the trunk (t from the rump): its spines' length there, in trunks, and how far they lean back
# (degrees): longest over the shoulders, shortening down the back.
CREST_T = [0.15, 0.4, 0.6, 0.74, 0.82, 0.93]
CREST_LENGTH = [0.46, 0.84, 1.18, 1.4, 1.42, 1.1]
CREST_LEAN = [68, 64, 60, 56, 54, 50]


def crest_spines(seed):
    """Each crest spine as (t along the trunk, degrees round from the ridge, length in trunks, lean back, splay out),
    jittered so the crest reads as a grown cluster, not a comb: a centre row, a row either side splayed out, and a fan
    over the shoulders splayed wide."""
    rng = np.random.default_rng(seed)
    out = []
    rows = [(0.0, np.arange(0.17, 0.93, 0.068), 1.0, 0.0, 0.0), (30.0, np.arange(0.21, 0.9, 0.085), 0.74, 34.0, 5.0),
            (56.0, np.arange(0.62, 0.89, 0.09), 0.5, 60.0, 0.0)]
    for phi, ts, share, splay, extra_lean in rows:
        for sign in ((1.0, -1.0) if phi else (1.0,)):
            for t in ts:
                t = t + rng.uniform(-0.015, 0.015)
                length = np.interp(t, CREST_T, CREST_LENGTH) * share * rng.uniform(0.85, 1.12)
                lean = np.interp(t, CREST_T, CREST_LEAN) + extra_lean + rng.uniform(-6, 6)
                angle = phi + (rng.uniform(-7, 7) if phi == 0 else rng.uniform(-4, 4))
                out.append((t, angle * sign, length, lean, (splay + rng.uniform(-8, 8)) * sign + (rng.uniform(-10, 10) if phi == 0 else 0.0)))
    return out


def crest(S, L, T, mats, body, bones, loft, seed):
    """The dorsal crest: hollow crimson spines dense over his shoulders and back, a centre row and a row splayed out either
    side and a fan over the shoulders, swept back, longest over the shoulders and shortening down the back and along the
    tail."""
    starts, directions, specs = [], [], []
    for t, phi, length, lean, splay in crest_spines(seed):
        point, normal = on_trunk(loft, t, phi)
        starts.append(point + normal * T * 0.8)
        directions.append(-normal)
        # A thicker spine for a longer one.
        specs.append((length, lean, splay, 0.035 + length * 0.11, point[0]))
    points, normals = surface_points(body, starts, directions, reach=T * 1.6)
    parts = []
    for n, ((length, lean, splay, rb, x), point, normal) in enumerate(zip(specs, points, normals)):
        lean, splay = np.radians(lean), np.radians(splay)
        direction = V(-np.sin(lean), np.sin(splay) * np.cos(lean), np.cos(lean) * np.cos(splay))
        base = point - normal * T * 0.04
        parts.append(crystal_parts(S, "spine_%d" % n, base, direction, length * T, rb * T, mats, anatomy.rigid(bones.bone_at(x)), hollow=True))
    # Along the tail: one to each bone and a pair splayed either side of the first two, shortening toward the tip.
    for k in range(3):
        a, b = p0(L, "tail_%02d" % (k + 1)), p1(L, "tail_%02d" % (k + 1))
        radius = T * (0.48 - k * 0.13)
        bone = anatomy.rigid("tail_%02d" % (k + 1))
        base = a + (b - a) * 0.45 + V(0, 0, radius * 0.75)
        lean = np.radians(56 + k * 4)
        length = T * (0.62 - k * 0.16)
        parts.append(crystal_parts(S, "tail_spine_%d" % k, base, V(-np.sin(lean), 0, np.cos(lean)), length, T * 0.035 + length * 0.11, mats, bone,
                                   hollow=True))
        if k < 2:
            for sign in (1.0, -1.0):
                side = a + (b - a) * 0.75 + V(0, sign * radius * 0.55, radius * 0.45)
                splay = np.radians(42)
                direction = V(-np.sin(lean), sign * np.sin(splay) * np.cos(lean), np.cos(lean) * np.cos(splay))
                parts.append(crystal_parts(S, "tail_spine_%d_%d" % (k, sign > 0), side, direction, length * 0.7, T * 0.035 + length * 0.08, mats, bone,
                                           hollow=True))
    # At the root of the neck, leaning back over the shoulders.
    n0, n1 = p0(L, "neck_01"), p1(L, "neck_01")
    base = n0 + (n1 - n0) * 0.3 + V(0, 0, T * 0.62)
    parts.append(crystal_parts(S, "neck_spine", base, V(-np.sin(np.radians(48)), 0, np.cos(np.radians(48))), T * 1.05, T * 0.16, mats,
                               anatomy.rigid("neck_01"), hollow=True))
    return Union(parts)


def set_crystal(S, L, T, mats, body, legs, loft):
    """Smaller crimson crystal set into his plates: clustered on his chest either side of the neck, and standing out of
    the plates of each shoulder and haunch, and the foreleg's forearm."""
    parts = []
    # The chest: three either side on the front of the chest, out at the shoulders and clear of the head (so none reads as
    # an eye beside the ocular), raking out and up (found by rays coming in onto the chest).
    chest = [(0.95, 62, 0.55), (0.9, 84, 0.45), (0.97, 100, 0.4)]
    starts, directions = [], []
    for sign in (1.0, -1.0):
        for t, phi, _ in chest:
            point, normal = on_trunk(loft, t, phi * sign)
            starts.append(point + normal * T * 0.8)
            directions.append(-normal)
    points, normals = surface_points(body, starts, directions, reach=T * 1.6)
    for n, (point, normal) in enumerate(zip(points, normals)):
        length = chest[n % len(chest)][2]
        direction = unit(normal + V(0.35, 0, 0.55))
        parts.append(crystal_parts(S, "chest_%d" % n, point - normal * T * 0.04, direction, T * length, T * 0.1, mats, anatomy.rigid("spine_02"),
                                   lit=V(0, 0, 1)))
    for side, sign in (("l", 1.0), ("r", -1.0)):
        for upper, lower, foot, parent, size in legs:
            u0, u1 = p0(L, upper + "_" + side), p1(L, upper + "_" + side)
            radius = T * 0.42 * size
            for k, (at, length, rise, back) in enumerate(((0.3, 0.62, 0.55, -0.3), (0.58, 0.44, 0.35, 0.2))):
                base = u0 + (u1 - u0) * at + V(0, sign * radius * 0.85, 0)
                parts.append(crystal_parts(S, "%s_%s_%d" % (upper, side, k), base, V(back, sign * 0.8, rise), T * length * size, T * 0.105 * size, mats,
                                           anatomy.rigid(upper + "_" + side), lit=V(0, 0, 1)))
            if upper == "upperarm":
                l0, l1 = p0(L, lower + "_" + side), p1(L, lower + "_" + side)
                base = l0 + (l1 - l0) * 0.3 + V(T * 0.12, sign * T * 0.27, 0)
                parts.append(crystal_parts(S, "%s_%s" % (lower, side), base, V(0.2, sign * 0.9, 0.35), T * 0.38, T * 0.08, mats,
                                           anatomy.rigid(lower + "_" + side), lit=V(0, 0, 1)))
    return Union(parts)


# ---------------------------------------------------------------------------------------------- the head
def head(S, L, T, mats):
    """A heavy wedge-shaped skull of faceted bone-pale plate, broad behind and narrowing to a blunt snout carried low and
    pointing down ahead; a dark jaw slung under it, fangs showing; a pale spike swept back from each cheek; and the single
    deep red ocular burning in a crystal socket in the brow, where a camera above sees it. Built in the head's frame (x
    along the skull, z up from it), in centimetres at his kit's trunk."""
    o = p0(L, "head")
    pitch = np.radians(HEAD_PITCH)
    X = V(np.cos(pitch), 0, -np.sin(pitch))
    Y = V(0, 1, 0)
    Z = np.cross(X, Y)
    s = T / 29.75 * HEAD_SCALE
    bones = anatomy.rigid("head")
    parts = []

    def leaf(name, distance, lo, hi, material, protect=0.6):
        node = tree.leaf(S, name, distance, Box(V(*lo) * s, V(*hi) * s), material, bones, protect)
        parts.append(node)
        return node

    c = V(13, 0, 0) * s
    BT, ST = V(-6, 0, 17) * s, V(33, 0, 6) * s
    planes = [(V(-8, 0, 0) * s, V(-1, 0, 0)), (V(35, 0, 0) * s, V(1, 0, 0.2))]
    for sign in (1.0, -1.0):
        BS, SS, BB = V(-6, 19 * sign, 4) * s, V(34, 7 * sign, 0) * s, V(-6, 11 * sign, -10) * s
        planes += [plane_through(BT, ST, BS, c), plane_through(BS, SS, BB, c)]
    planes.append(plane_through(V(-6, 11, -10) * s, V(-6, -11, -10) * s, V(34, 5.5, -7) * s, c))
    skull = tree.leaf(S, "skull", lambda P: convex(P, planes), Box(V(-9, -21, -12) * s, V(37, 21, 19) * s), mats["plate"], bones, 0.7)
    # The ocular's socket, cut into the brow on the ridge, lined with crystal.
    ridge = unit(ST - BT)
    up = unit(np.cross(ridge, V(0, -1, 0)))
    brow = BT + ridge * (17 * s) - up * s
    frame = np.stack([ridge, V(0, 1, 0), up], axis=1)
    socket = tree.leaf(S, "socket", lambda P: sdf.ellipsoid(P, brow + up * 1.5 * s, V(6.2, 5.2, 4.0) * s, frame), Box(brow - 8 * s, brow + 8 * s),
                       mats["crystal"], bones, 1.0)
    parts.append(Subtract(skull, socket, label=socket.part.label))
    leaf("ocular", lambda P: sdf.ellipsoid(P, brow - up * 0.6 * s, V(4.6, 3.8, 2.6) * s, frame), (brow - 6 * s) / s, (brow + 6 * s) / s, mats["ocular"], 1.0)
    # The jaw slung beneath, its mouth a dark seam under the skull's edge.
    jaw_planes = [(V(-6, 0, 0) * s, V(-1, 0, 0)), (V(29, 0, 0) * s, V(1, 0, -0.2)), (V(0, 0, -7) * s, V(0, 0, 1))]
    jc = V(12, 0, -11) * s
    for sign in (1.0, -1.0):
        jaw_planes.append(plane_through(V(-4, 11 * sign, -8) * s, V(28, 5 * sign, -8) * s, V(-4, 9 * sign, -17) * s, jc))
    jaw_planes.append(plane_through(V(-4, 9, -17) * s, V(-4, -9, -17) * s, V(28, 4, -12) * s, jc))
    leaf("jaw", lambda P: convex(P, jaw_planes), (-8, -13, -19), (31, 13, -5), mats["hide"], 0.5)
    leaf("maw", lambda P: sdf.box(P, V(13, 0, -7.6) * s, V(13, 6.5, 1.2) * s), (-1, -8, -10), (27, 8, -5), mats["maw"], 0.5)
    for sign in (1.0, -1.0):
        for k, (x, y, size) in enumerate(((25, 5.6, 0.9), (17, 7.8, 1.1), (9, 9.6, 0.8))):
            a = V(x, y * sign, -5.5) * s
            b = a + V(1, 0.3 * sign, -7 * size) * s
            leaf("fang_%d_%d" % (k, sign > 0), lambda P, a=a, b=b, r=1.7 * size * s: sdf.round_cone(P, a, b, r, 0.3 * s), (np.minimum(a, b) - 3 * s) / s,
                 (np.maximum(a, b) + 3 * s) / s, mats["teeth"], 0.8)
        # A pale spike swept back from each cheek, broadening the wedge behind.
        a, b = V(-3, 15 * sign, 4) * s, V(-19, 25 * sign, 11) * s
        leaf("cheek_spike_%d" % (sign > 0), lambda P, a=a, b=b: sdf.round_cone(P, a, b, 3.8 * s, 0.6 * s), (np.minimum(a, b) - 5 * s) / s,
             (np.maximum(a, b) + 5 * s) / s, mats["plate"], 0.7)
    # No crimson on the face but the ocular: one red light there, never a row of eyes.
    return Placed(Union(parts), o, np.stack([X, Y, Z], axis=1))


# ---------------------------------------------------------------------------------------------- claws
def claws(S, L, T, mats, legs):
    """Big hooked claws of dark horn on every paw, splayed ahead of it and curving down to the ground: four on each
    forefoot, three on the others."""
    parts = []
    for upper, lower, foot, parent, size in legs:
        count = 4 if upper == "upperarm" else 3
        for side in ("l", "r"):
            pad = paw_of(L, T, foot, side, size)
            bones = anatomy.rigid(foot + "_" + side)
            for k in range(count):
                spread = (k / (count - 1) - 0.5) * 2.0
                root = pad + V(T * 0.3, spread * T * 0.25, T * 0.06) * size
                knuckle = root + V(T * 0.18, spread * T * 0.07, -T * 0.02) * size
                tip = knuckle + V(T * 0.14, spread * T * 0.04, 0) * size
                tip[2] = T * 0.015
                r = T * 0.085 * size
                parts.append(tree.leaf(S, "claw_%s_%s_%d" % (foot, side, k),
                                       lambda P, a=root, b=knuckle, c=tip, r=r: np.minimum(sdf.round_cone(P, a, b, r, r * 0.75), sdf.round_cone(P, b, c, r * 0.75, r * 0.12)),
                                       Box.around([root, knuckle, tip], r * 1.1), mats["claw"], bones, 0.6))
    return Union(parts)
