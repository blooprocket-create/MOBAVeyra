"""Oriel, The Waiting Light (ADR-069): memory held in glass, read by her silhouette from the game camera. Character Bible
§20 and her splash art: an elegant, unmistakably nonhuman and asymmetrical figure of irregular stained-glass panes in
fractured lead framing, lit from within and warmest over the heart; floating shards; never skin under glass armour, never
a ghost. A faceted bodice tapers to a narrow waist; below it ragged shards splay about a trailing point that floats clear
of the ground. Her limbs are framed glass the whole way through, jointed in gold, her fingers long gold claws. Her head is
a pointed gem of panes with a crest of shards, her core burning in a gold frame at its face; behind it stands a ring of
gold framework with the core at its centre. Broad panes fan from her shoulders like a window opened out: a wide
wing on her right, a smaller fan on her left and a hanging fin of panes along her left forearm.

Low poly and flat-coloured (author 2026-10-07). Every pane of glass is a flat sheet built at the density it keeps (two
triangles), so its jewel colour has an exact edge: laid a little proud of a dark framework solid where she has body (the
lead shows in the gaps between panes), or free in its own leading where she has none (her fans, her shards). The
framework, gold joints, fingers, halo and fan frames are sculpted. Each part rides the bone the construct archetype gives
it (ADR-064): every sculpted piece is rigid to one bone and stands clear of the pieces of other bones, so its panes
follow it exactly; the floating shards circle on the orbit bones, the skirt and point sweep back on the trail bones, the
halo turns on its own. Colours are her kit's: gold, lead lifted from its near-black so it reads, and her jewels."""
import math

import numpy as np

from ..sculpt import anatomy, sdf, sheet, tree
from ..sculpt.anatomy import V, unit
from ..sculpt.tree import Box, Union

# The lead between panes: each pane stands this far in from the edges of the framework or outline it fills (cm), and a
# pane laid on the framework stands this far proud of it.
LEAD = 0.9
LIFT = 0.45
# The waist-to-collar rings of her bodice: one irregular octagon (degrees from the front, radius), deeper than it is
# wide by DEPTH, scaled and placed at each ring, so every facet between two rings is a flat trapezoid.
OCTAGON = ((0, 1.0), (43, 0.95), (88, 1.0), (134, 0.93), (180, 0.9), (226, 0.95), (272, 1.0), (317, 0.96))
DEPTH = 0.62
# Each ring: its height above the core (in shoulder widths, 0 at the core), its scale (shoulder widths), and its centre
# forward and to the left (cm).
RINGS = ((0.17, 0.30, 0.6, 0.0), (0.55, 0.35, 0.9, 0.0), (0.97, 0.47, 1.2, 0.0), (1.5, 0.62, 0.6, -0.6), (1.82, 0.5, -0.6, 0.0),
         (1.87, 0.15, -0.6, 0.0))
# Pane colours by where they lie: a jewel's name and its weight. Her art is mostly pearl-pale glass touched with
# lavender and blue, its amber kept to the panes lit about her core.
BODICE = {"pale": 5, "violet": 1.5, "blue": 1, "cyan": 0.5}
WING = {"pale": 4, "violet": 2, "blue": 1.5, "cyan": 1, "rose": 0.5}
SKIRT = {"pale": 4, "violet": 2, "blue": 1, "rose": 0.5}
LIMB = {"pale": 3, "violet": 1}


def build(S, L, dims, spec):
    """Oriel's sculpt on her layout: (the framework, joints, halo and frames, {"body": the same, "sheets": her glass})."""
    g = Glass(S, spec, np.random.default_rng(spec["seed"]))
    p = lambda bone, i: V(*L[bone][i])  # noqa: E731
    pieces = [bodice(g, L, dims, p), head(g, L, dims, p), halo(g, L, dims, p)]
    for side in ("l", "r"):
        pieces += arm(g, L, dims, p, side)
    # Each fan's panes: (elevation in degrees, length and width in shoulder widths); the wide wing is on her right.
    pieces.append(fan(g, dims, p, "r", ((40, 1.4, 0.44), (14, 1.68, 0.5), (-12, 1.62, 0.5), (-38, 1.34, 0.42))))
    pieces.append(fan(g, dims, p, "l", ((30, 1.05, 0.33), (58, 0.85, 0.28))))
    pieces.append(fin(g, dims, p))
    skirt(g, dims, p)
    point(g, dims, p)
    shards(g, dims, p)
    root = Union(pieces)
    return root, {"body": root, "sheets": g.sheets}


# ---------------------------------------------------------------------------------------------- glass
class Glass:
    """Her materials, the panes made so far (sheets) and the pieces' leaves."""

    def __init__(self, S, spec, rng):
        self.S, self.rng, self.sheets = S, rng, []
        gold, jewels = list(spec["primary"]), [list(j) for j in spec["jewels"]]
        # The leading: her kit's near-black lifted toward a dark bronze, so it reads under the toon light.
        lead = [max(c, 0.2) * 0.75 + g * 0.25 for c, g in zip(spec["secondary"], (0.55, 0.38, 0.22))]
        # Her core's warmth glows, and the amber glass nearest it ("lit"); the rest of her glass is shaded.
        colours = {"gold": (gold, False), "lead": (lead, False), "core": (list(spec["accent"]), True), "lit": (jewels[0], True),
                   "amber": (jewels[0], False), "pale": (jewels[1], False), "blue": (jewels[2], False), "violet": (jewels[3], False),
                   "rose": (jewels[4], False), "cyan": (jewels[5], False)}
        self.mats = {name: S.material(name, colour, glow) for name, (colour, glow) in colours.items()}
        # Floating shards catch the light.
        for name in ("amber", "pale", "blue", "violet", "rose", "cyan"):
            self.mats["shard_" + name] = S.material("shard_" + name, colours[name][0], True)

    def pick(self, weights):
        names = list(weights)
        w = np.array([weights[n] for n in names], dtype=np.float64)
        return names[int(self.rng.choice(len(names), p=w / w.sum()))]

    # -- sculpted pieces
    def leaf(self, name, distance, bounds, material, bone, protect=0.0):
        return tree.leaf(self.S, name, distance, bounds, self.mats[material], anatomy.rigid(bone), protect)

    def solid(self, name, faces, material, bone, protect=0.0):
        """A convex solid of flat faces: (its leaf, its faces with their outward normals)."""
        shape = Convex(faces)
        return self.leaf(name, shape.distance, Box.around(shape.points, 0.5), material, bone, protect), shape.faces

    def tube(self, name, points, radii, material, bone, protect=0.6):
        pts = [V(*q) for q in points]
        return self.leaf(name, lambda P: sdf.tube(P, pts, list(radii)), Box.around(pts, max(radii) + 0.5), material, bone, protect)

    def ball(self, name, centre, radius, material, bone, protect=0.6):
        c = V(*centre)
        return self.leaf(name, lambda P: sdf.sphere(P, c, radius), Box(c - radius - 0.5, c + radius + 0.5), material, bone, protect)

    def spike(self, name, base, direction, length, radius, material, bone, sides=4, roll=0.0):
        """A pointed shard: a pyramid of sides flat faces from its base (sunk into what it grows from) to its point."""
        d = unit(direction)
        u, v = frame_across(d, roll)
        ring = [base + (u * math.cos(a) + v * math.sin(a)) * radius for a in np.linspace(0, math.tau, sides, endpoint=False)]
        tip = base + d * length
        return self.solid(name, frustum(ring, [tip] * sides), material, bone)[0]

    # -- panes
    def pane(self, quad, colour, bone):
        """One flat four-cornered pane (two triangles)."""
        Q = np.asarray(quad, dtype=np.float64)

        def position(u, v):
            u, v = u[:, None], v[:, None]
            return (1 - u) * (1 - v) * Q[0] + u * (1 - v) * Q[1] + u * v * Q[2] + (1 - u) * v * Q[3]
        self.sheets.append(sheet.Sheet("pane_%d" % len(self.sheets), position, self.mats[colour], lambda P, u, v, b=anatomy.rigid(bone): b(P), 1, 1))

    def border(self, outer, inner, colour, bone):
        """The leading round a pane: a ring of quads from its outline in to the pane's own edge."""
        n = len(outer)
        O = np.asarray(list(outer) + [outer[0]], dtype=np.float64)
        I = np.asarray(list(inner) + [inner[0]], dtype=np.float64)

        def position(u, v):
            k = np.rint(u * n).astype(int)
            return O[k] * (1 - v[:, None]) + I[k] * v[:, None]
        self.sheets.append(sheet.Sheet("lead_%d" % len(self.sheets), position, self.mats[colour], lambda P, u, v, b=anatomy.rigid(bone): b(P), n, 1))

    def laid(self, face, normal, colour, bone, inset=LEAD, lift=LIFT):
        """A pane laid on a framework's face, inset from its edges (the lead shows round it) and standing proud of it."""
        quad = as_quad(inset_polygon(face, inset))
        if quad is not None:
            self.pane([q + normal * lift for q in quad], colour, bone)

    def framed(self, outline, colour, bone, lead=LEAD, frame="lead"):
        """A free pane filling outline (four corners, flat) in its own leading."""
        inner = inset_polygon(outline, lead)
        if inner is None:
            return
        self.border(outline, inner, frame, bone)
        self.pane(inner, colour, bone)


class Convex:
    """A convex solid bounded by flat faces: its distance is the farthest of its faces' planes (exact on and inside it,
    never more than the true distance outside)."""

    def __init__(self, faces):
        faces = [[V(*q) for q in face] for face in faces]
        self.points = np.concatenate([np.asarray(face) for face in faces])
        centre = self.points.mean(axis=0)
        self.faces, normals, offsets = [], [], []
        for face in faces:
            c, n = plane_of(face)
            if n @ (c - centre) < 0:
                n = -n
            self.faces.append((face, n))
            normals.append(n)
            offsets.append(n @ c)
        self.N = np.asarray(normals, dtype=np.float32)
        self.D = np.asarray(offsets, dtype=np.float32)

    def distance(self, P):
        return np.max(np.asarray(P, dtype=np.float32) @ self.N.T - self.D, axis=1)


# ---------------------------------------------------------------------------------------------- geometry helpers
def plane_of(points):
    """A flat polygon's centre and unit normal (by its winding)."""
    P = np.asarray(points, dtype=np.float64)
    c = P.mean(axis=0)
    n = np.zeros(3)
    for a, b in zip(P, np.roll(P, -1, axis=0)):
        n += np.cross(a - c, b - c)
    return c, unit(n)


def frame_across(d, roll=0.0):
    """Two unit vectors square to d and to each other, the first as near forward (+X) as d allows, turned roll radians."""
    hint = V(1, 0, 0) if abs(d[0]) < 0.9 else V(0, 0, 1)
    u = unit(hint - d * (hint @ d))
    v = np.cross(d, u)
    return u * math.cos(roll) + v * math.sin(roll), v * math.cos(roll) - u * math.sin(roll)


def ring(centre, u, v, shape, scale):
    return [V(*centre) + (u * x + v * y) * scale for x, y in shape]


def frustum(ring0, ring1):
    """The faces of the solid between two similar rings (or a ring and a point): flat sides and its caps."""
    n = len(ring0)
    apex = all(np.allclose(ring1[0], q) for q in ring1)
    faces = []
    for k in range(n):
        a, b = ring0[k], ring0[(k + 1) % n]
        faces.append([a, b, ring1[0]] if apex else [a, b, ring1[(k + 1) % n], ring1[k]])
    faces.append(list(ring0))
    if not apex:
        faces.append(list(ring1))
    return faces


def inset_polygon(points, w):
    """A flat convex polygon with each edge moved w in toward its middle, or None if it would vanish."""
    P = np.asarray(points, dtype=np.float64)
    c, n = plane_of(P)
    e1 = unit(P[1] - P[0])
    e2 = np.cross(n, e1)
    Q = np.stack([(P - c) @ e1, (P - c) @ e2], axis=1)
    k = len(Q)
    area = 0.5 * np.sum(Q[:, 0] * np.roll(Q[:, 1], -1) - np.roll(Q[:, 0], -1) * Q[:, 1])
    turn = 1.0 if area > 0 else -1.0
    lines = []
    for i in range(k):
        a, b = Q[i], Q[(i + 1) % k]
        d = (b - a) / max(np.linalg.norm(b - a), 1e-9)
        lines.append((a + turn * np.array([-d[1], d[0]]) * w, d))
    out = []
    for i in range(k):
        (p1, d1), (p2, d2) = lines[i - 1], lines[i]
        det = d1[0] * -d2[1] - (-d2[0]) * d1[1]
        if abs(det) < 1e-9:
            out.append(p2)
            continue
        r = p2 - p1
        t = (r[0] * -d2[1] - (-d2[0]) * r[1]) / det
        out.append(p1 + d1 * t)
    out = np.asarray(out)
    inner = 0.5 * np.sum(out[:, 0] * np.roll(out[:, 1], -1) - np.roll(out[:, 0], -1) * out[:, 1])
    if inner * area <= 0 or abs(inner) < abs(area) * 0.12:
        return None
    # Every new corner inside the old outline (a sharp corner's inset can shoot past it).
    for i in range(k):
        a, b = Q[i], Q[(i + 1) % k]
        edge = b - a
        if np.any(turn * (edge[0] * (out[:, 1] - a[1]) - edge[1] * (out[:, 0] - a[0])) < 0):
            return None
    return [c + q[0] * e1 + q[1] * e2 for q in out]


def as_quad(points):
    """A flat polygon as four corners: a triangle's sharpest corner cut off; a quad as it is."""
    if points is None:
        return None
    if len(points) == 4:
        return points
    if len(points) > 4:
        # A larger polygon: the quad of four of its corners spread round it.
        n = len(points)
        return [points[0], points[n // 4 + (1 if n % 4 >= 2 else 0)], points[n // 2], points[(3 * n) // 4 + (1 if n % 4 == 3 else 0)]]
    a, b, c = (V(*q) for q in points)
    corners = [a, b, c]
    angles = []
    for i in range(3):
        x, y, z = corners[i], corners[i - 1], corners[(i + 1) % 3]
        angles.append(math.acos(np.clip(unit(y - x) @ unit(z - x), -1, 1)))
    i = int(np.argmin(angles))
    tip, before, after = corners[i], corners[i - 1], corners[(i + 1) % 3]
    return [before, tip + (before - tip) * 0.2, tip + (after - tip) * 0.2, after]


def split(face, t0, t1):
    """A trapezoid [a, b, c, d] (a-b its bottom, d-c its top) cut from t0 along its bottom to t1 along its top."""
    a, b, c, d = face
    p, q = a + (b - a) * t0, d + (c - d) * t1
    return [[a, p, q, d], [p, b, c, q]]


def split_across(face, s0, s1):
    """A trapezoid [a, b, c, d] cut from s0 up its first side (a-d) to s1 up its other (b-c)."""
    a, b, c, d = face
    p, q = a + (d - a) * s0, b + (c - b) * s1
    return [[a, b, q, p], [p, q, c, d]]


def mosaic(g, face):
    """A facet as irregular panes: whole, cut up and down or aslant, or cut across."""
    roll = g.rng.random()
    if roll < 0.3:
        return [face]
    if roll < 0.65:
        t0 = g.rng.uniform(0.2, 0.8)
        return split(face, t0, float(np.clip(t0 + g.rng.uniform(-0.45, 0.45), 0.2, 0.8)))
    s0 = g.rng.uniform(0.3, 0.7)
    return split_across(face, s0, float(np.clip(s0 + g.rng.uniform(-0.3, 0.3), 0.25, 0.75)))


# ---------------------------------------------------------------------------------------------- the bodice
def bodice(g, L, dims, p):
    """The bodice: a framework of flat facets between irregular rings from the waist to the neck, a pane laid on each
    (some cut in two), the heart's panes burning with her core; shoulder caps over the arms bristling with shards. All
    of it rides the chest bone."""
    s = dims["shoulder"]
    core = p("pelvis", 0)
    shape = [(math.cos(math.radians(a)) * r * DEPTH, math.sin(math.radians(a)) * r) for a, r in OCTAGON]
    X, Y = V(1, 0, 0), V(0, 1, 0)
    rings = [ring(core + V(cx, cy, h * s), X, Y, shape, scale * s) for h, scale, cx, cy in RINGS]
    nodes = []
    for band in range(len(rings) - 1):
        node, faces = g.solid("bodice_%d" % band, frustum(rings[band], rings[band + 1]), "lead", "spine_03")
        nodes.append(node)
        for k, (face, normal) in enumerate(faces[:len(shape)]):
            front = k in (0, len(shape) - 1)
            # Over the heart her core's warmth burns through the glass; about it the panes are lit amber.
            heart = front and band == 2
            for part in mosaic(g, face):
                g.laid(part, normal, "core" if heart else ("lit" if front and band in (1, 3) else g.pick(BODICE)), "spine_03")
    # Shoulders: a wedge sloping from the collar, and from it a pointed blade of glass reaching out and down over the
    # arm, clear above its gold joint; shards bristle up from it, more on her right.
    for side, sign in (("l", 1.0), ("r", -1.0)):
        shoulder = p("upperarm_" + side, 0)
        section = [(-4.5, 0.0), (5.0, 0.0), (6.2, 4.5), (0.5, 7.5), (-4.8, 4.0)]
        inner = [V(0.3 + x, sign * (s * 0.3), shoulder[2] + 5.2 + z) for x, z in section]
        outer = [V(0.3 + x * 0.8, sign * (s * 0.72), shoulder[2] + 6.0 + z * 0.8) for x, z in section]
        node, faces = g.solid("yoke_" + side, frustum(inner, outer), "gold", "spine_03")
        nodes.append(node)
        for face, normal in faces[:len(section)]:
            if normal[2] > 0.3 or normal[0] > 0.4:
                for part in mosaic(g, face):
                    g.laid(part, normal, g.pick(LIMB), "spine_03")
        root = V(0.6, sign * s * 0.62, shoulder[2] + 11.7)
        blade = unit(V(0, sign * 0.91, -0.41))
        node, faces = g.solid("pauldron_" + side, flat_faces(root, blade, V(0, 0, 1), s * 0.68, s * 0.2, 0.36), "gold", "spine_03")
        nodes.append(node)
        for face, normal in faces[:4]:
            if normal[2] > 0.2:
                g.laid(face, normal, g.pick(LIMB), "spine_03", inset=0.7)
        spikes = ((0.55, V(-0.2, sign * 0.45, 0.87), 0.5, 0.085), (0.85, V(0.15, sign * 0.75, 0.65), 0.38, 0.07), (0.3, V(-0.55, sign * 0.4, 0.73), 0.32, 0.065))
        if side == "l":
            spikes = spikes[:2]
        for k, (share, direction, length, radius) in enumerate(spikes):
            base = root + blade * s * 0.5 * share + V(0, 0, 0.6)
            nodes.append(g.spike("cap_spike_%s_%d" % (side, k), base, direction, length * s, radius * s, "gold", "spine_03", sides=3, roll=0.4 * k))
    return Union(nodes)


def flat_faces(base, direction, normal, length, radius, thin=0.35):
    """The faces of a flattened pyramid from base along direction: wide across (square to normal), thin along normal."""
    d = unit(direction)
    wide = unit(np.cross(normal, d))
    across = unit(np.cross(d, wide))
    ring4 = [base + wide * radius, base + across * radius * thin, base - wide * radius, base - across * radius * thin]
    return frustum(ring4, [base + d * length] * 4)


# ---------------------------------------------------------------------------------------------- the head and halo
def head_points(L, dims, p):
    h0, h1 = p("head", 0), p("head", 1)
    hd = dims["head"]
    middle = h0[2] + hd * 0.45
    return hd, middle, h0, h1


def head(g, L, dims, p):
    """A pointed gem of flat panes on the head bone, wider than deep and as large as the game camera needs; at its face
    her core burns in a gold frame; a crest of glass shards fans from behind it; a gold joint where it sits on the neck."""
    hd, middle, h0, h1 = head_points(L, dims, p)
    rx, ry = hd * 0.46, hd * 0.52
    ring6 = [V(math.cos(math.radians(30 + 60 * k)) * rx, math.sin(math.radians(30 + 60 * k)) * ry, middle) for k in range(6)]
    crown = V(-hd * 0.18, 0, h1[2] + hd * 0.22)
    chin = V(hd * 0.12, 0, h0[2] + hd * 0.07)
    faces = [[ring6[k], ring6[(k + 1) % 6], crown] for k in range(6)] + [[ring6[(k + 1) % 6], ring6[k], chin] for k in range(6)]
    gem, faces = g.solid("gem", faces, "lead", "head", protect=0.8)
    nodes = [gem]
    for k, (face, normal) in enumerate(faces):
        g.laid(face, normal, g.pick({"pale": 3, "violet": 1, "blue": 1}), "head", inset=0.6, lift=0.35)
    # The core at its face, in a gold frame.
    front = rx * math.cos(math.radians(30))
    nodes.append(g.ball("core", (front - 1.2, 0, middle), hd * 0.25, "core", "head", protect=1.0))
    frame_centre = V(front + 0.4, 0, middle)
    axes = sdf.frame((1.0, 0.0, 0.0))
    nodes.append(g.leaf("face_frame", lambda P: sdf.torus(P, frame_centre, hd * 0.33, hd * 0.05, axes),
                        Box(frame_centre - hd * 0.4, frame_centre + hd * 0.4), "gold", "head", protect=1.0))
    # The crest: framed glass shards fanned up and out from behind the gem like a crown of rays, tipped back, the longest
    # at the top and longer on her right; five panes facing forward, so from before they read as a crown, never as horns.
    back = V(-math.sin(math.radians(22)), 0, math.cos(math.radians(22)))
    facing = unit(np.cross(back, V(0, 1, 0)))
    for k, (angle, length, colour) in enumerate(((0, 0.95, "pale"), (30, 0.7, "violet"), (-32, 0.78, "pale"), (62, 0.5, "blue"), (-64, 0.58, "violet"))):
        a = math.radians(angle)
        direction = unit(back * math.cos(a) + V(0, 1, 0) * math.sin(a))
        across = unit(np.cross(facing, direction))
        base = V(-hd * 0.3, 0, middle + hd * 0.1) + direction * hd * 0.2 - facing * (0.5 + 0.4 * k)
        tip, waist = base + direction * hd * length, base + direction * hd * length * 0.38
        width = hd * (0.3 if k == 0 else 0.24)
        g.framed([base, waist + across * width * 0.5, tip, waist - across * width * 0.5], colour, "head", lead=0.55, frame="gold")
    # Her neck: a gold joint under the gem's point, standing clear of the collar.
    nodes.append(g.ball("neck_joint", chin + V(-hd * 0.04, 0, -hd * 0.03), hd * 0.1, "gold", "head"))
    return Union(nodes)


def halo(g, L, dims, p):
    """The ring of gold framework standing behind her head, tilted back a little, her core at its centre seen from before:
    a heavy outer ring and a lighter inner one held by struts, rays standing out from its upper arc. It stands far
    enough back that its lower arc passes clear behind her shoulders, touching nothing of her: it rides the halo bone."""
    hd, middle, h0, h1 = head_points(L, dims, p)
    s = dims["shoulder"]
    centre = V(p("halo", 0)[0] - s * 0.32, 0, middle)
    tilt = math.radians(10)
    up = V(-math.sin(tilt), 0, math.cos(tilt))
    radius = hd * 1.12
    at = lambda degrees, share: centre + V(0, 1, 0) * math.cos(math.radians(degrees)) * radius * share + up * math.sin(math.radians(degrees)) * radius * share  # noqa: E731
    span = np.arange(-90, 271, 10.0)
    nodes = [g.tube("halo_outer", [at(a, 1.0) for a in span], [s * 0.055] * len(span), "gold", "halo", protect=0.35),
             g.tube("halo_inner", [at(a, 0.8) for a in span], [s * 0.022] * len(span), "gold", "halo", protect=0.35)]
    for k, a in enumerate(range(-90, 270, 45)):
        nodes.append(g.tube("halo_strut_%d" % k, [at(a, 0.8), at(a, 1.0)], [s * 0.02, s * 0.02], "gold", "halo"))
    for k, a in enumerate(range(30, 151, 20)):
        nodes.append(g.tube("halo_ray_%d" % k, [at(a, 1.04), at(a, 1.34 if k % 2 else 1.2)], [s * 0.03, s * 0.008], "gold", "halo"))
    return Union(nodes)


# ---------------------------------------------------------------------------------------------- limbs
def arm(g, L, dims, p, side):
    """An arm of framed glass the whole way through: a square-sectioned framework for the upper arm and the forearm,
    turned to show a ridge forward, a pane on every face; gold joints at the shoulder, elbow and wrist, a shard off the
    elbow; the hand a glass palm with long gold claws. Each part is its own piece on its own bone."""
    s = dims["shoulder"]
    sign = 1.0 if side == "l" else -1.0
    shoulder, elbow, wrist = p("upperarm_" + side, 0), p("upperarm_" + side, 1), p("lowerarm_" + side, 1)
    square = [(1, 0), (0, 1), (-1, 0), (0, -1)]
    pieces = []
    for part, a, b, (start, end), (r0, r1), joint in (("upperarm", shoulder, elbow, (4.0, 4.2), (0.115, 0.09), 0.1),
                                                       ("lowerarm", elbow, wrist, (2.0, 3.8), (0.095, 0.068), 0.082)):
        bone = part + "_" + side
        d = unit(b - a)
        u, v = frame_across(d)
        ring0 = ring(a + d * start, u, v, square, s * r0)
        ring1 = ring(b - d * end, u, v, square, s * r1)
        node, faces = g.solid(part + "_" + side, frustum(ring0, ring1), "lead", bone)
        for face, normal in faces[:4]:
            g.laid(face, normal, g.pick(LIMB), bone, inset=0.6, lift=0.35)
        nodes = [node, g.ball(part + "_joint_" + side, a, s * joint, "gold", bone)]
        if part == "lowerarm":
            nodes.append(g.spike("elbow_spike_" + side, a + d * 3.0 - u * 1.5, V(-0.8, sign * 0.35, 0.35), s * 0.3, s * 0.05, "lead", bone, sides=3))
        pieces.append(Union(nodes))
    pieces.append(hand(g, dims, p, side))
    return pieces


def hand(g, dims, p, side):
    """A framed glass palm turned forward, lit from within, with four long gold claws spread from it and a thumb."""
    s = dims["shoulder"]
    bone = "hand_" + side
    sign = 1.0 if side == "l" else -1.0
    w0, w1 = p(bone, 0), p(bone, 1)
    d = unit(w1 - w0)
    inward = unit(np.cross(d, V(1, 0, 0))) * sign
    normal = V(1, 0, 0)
    outline = [(0.8, 1.6), (3.0, 2.6), (5.2, 2.2), (5.2, -2.2), (3.0, -2.6), (0.8, -1.6)]
    back = [w0 + d * a + inward * b - normal * 0.8 for a, b in outline]
    front = [q + normal * 1.6 for q in back]
    palm, faces = g.solid("palm_" + side, frustum(back, front), "lead", bone)
    nodes = [palm, g.ball("wrist_" + side, w0, s * 0.062, "gold", bone)]
    for face, n in faces[-2:]:
        g.laid(face, n, "lit", bone, inset=0.5, lift=0.3)
    for k, across in enumerate((-1.8, -0.6, 0.6, 1.8)):
        spread = across * 0.17
        base = w0 + d * 5.0 + inward * across
        first = unit(d * math.cos(spread) + inward * math.sin(spread))
        knuckle = base + first * 4.8
        tip = knuckle + unit(first + normal * 0.45) * 4.2
        nodes.append(g.tube("finger_%s_%d" % (side, k), [base, knuckle, tip], [0.85, 0.7, 0.3], "gold", bone, protect=1.0))
    base = w0 + d * 1.8 + inward * 2.4 + normal * 0.6
    nodes.append(g.tube("thumb_" + side, [base, base + unit(d * 0.6 + inward * 0.5 + normal * 0.6) * 5.0], [0.85, 0.35], "gold", bone, protect=1.0))
    return Union(nodes)


# ---------------------------------------------------------------------------------------------- fans and fin
def kite(g, bone, base, direction, facing, length, width, colours, nodes, layer):
    """A long diamond pane from base out along direction, widest a little before its middle, cut in four by leading
    from an off-centre point to its edges, in a heavy gold outline."""
    d = unit(direction)
    across = unit(np.cross(facing, d))
    lift = unit(np.cross(d, across)) * layer
    base = base + lift
    tip = base + d * length
    widest = 0.42 + g.rng.uniform(-0.05, 0.05)
    middle = base + d * length * widest
    right, left = middle + across * width * 0.5, middle - across * width * 0.5
    outline = [base, right, tip, left]
    nodes.append(g.tube("kite_%s_%d" % (bone, len(nodes)), outline + [base], [0.95] * 5, "gold", bone, protect=0.35))
    centre = base + d * length * (widest + g.rng.uniform(-0.04, 0.08)) + across * width * g.rng.uniform(-0.1, 0.1)
    cuts = [outline[k] + (outline[(k + 1) % 4] - outline[k]) * g.rng.uniform(0.4, 0.6) for k in range(4)]
    for k in range(4):
        g.framed([outline[k], cuts[k], centre, cuts[k - 1]], g.pick(colours), bone, lead=0.75)


def fan(g, dims, p, side, kites):
    """Broad panes fanned from a gold hinge behind one shoulder like a window opened out, faces turned forward and up:
    above the shoulder they rise back, below it they fall back, clear of the arm. It rides the clavicle."""
    s = dims["shoulder"]
    sign = 1.0 if side == "l" else -1.0
    bone = "clavicle_" + side
    shoulder = p("upperarm_" + side, 0)
    hub = V(-8.0, sign * (abs(shoulder[1]) - 3.6), shoulder[2] + 2.5)
    out, rise, fall = V(0, sign, 0), unit(V(-0.55, 0, 0.83)), unit(V(-0.4, 0, -0.92))
    facing = unit(V(0.75, 0, 0.66))
    nodes = [g.ball("hinge_" + side, hub, 2.4, "gold", bone)]
    for k, (elevation, length, width) in enumerate(kites):
        e = math.radians(elevation)
        direction = unit(out * math.cos(e) + (rise if e >= 0 else fall) * math.sin(abs(e)))
        base = hub + direction * 4.0
        nodes.append(g.tube("stem_%s_%d" % (side, k), [hub, base], [1.0, 1.0], "gold", bone))
        kite(g, bone, base, direction, facing, length * s, width * s, WING, nodes, layer=(k - len(kites) / 2) * 0.8)
    return Union(nodes)


def fin(g, dims, p):
    """A fin of panes hanging out from along her left forearm, flared out from it so it faces forward: the panes the art
    hangs from her raised arm."""
    s = dims["shoulder"]
    elbow, wrist = p("lowerarm_l", 0), p("lowerarm_l", 1)
    d = unit(wrist - elbow)
    out = unit(np.cross(V(1, 0, 0), d))
    if out[1] < 0:
        out = -out
    nodes = []
    for k, (share, angle, length, width) in enumerate(((0.25, 26, 1.0, 0.32), (0.5, 36, 1.2, 0.36), (0.75, 46, 0.95, 0.3))):
        a = math.radians(angle)
        direction = unit(d * math.cos(a) + out * math.sin(a))
        base = elbow + (wrist - elbow) * share + out * 2.0 - V(1.0 + 1.4 * k, 0, 0)
        kite(g, "lowerarm_l", base, direction, V(1, 0, 0), length * s, width * s, WING, nodes, layer=0.0)
    return Union(nodes)


# ---------------------------------------------------------------------------------------------- below the waist
def skirt(g, dims, p):
    """Ragged shards hanging apart from each other below the waist, splayed out and swept back, each cut in two; they
    ride the trail, so they sweep back as she glides."""
    s = dims["shoulder"]
    core = p("pelvis", 0)
    h, scale, cx, cy = RINGS[0]
    shape = [(math.cos(math.radians(a)) * r * DEPTH, math.sin(math.radians(a)) * r) for a, r in OCTAGON]
    waist = ring(core + V(cx, cy, h * s + 2.5), V(1, 0, 0), V(0, 1, 0), shape, scale * s * 1.12)
    for k in range(len(waist)):
        a, b = waist[k], waist[(k + 1) % len(waist)]
        top = (a + b) / 2
        out = unit(V(top[0], top[1], 0))
        drop = s * (1.35 if k % 2 == 0 else 0.95) * g.rng.uniform(0.88, 1.12)
        splay = math.radians(g.rng.uniform(30, 40))
        tip = top + out * drop * math.tan(splay) + V(-drop * 0.12, 0, -drop)
        e1 = unit(b - a)
        e2 = unit((tip - top) - e1 * ((tip - top) @ e1))
        length = (tip - top) @ e2
        half = np.linalg.norm(b - a) * 0.5
        at = lambda x, y: top + e1 * x + e2 * y  # noqa: E731
        lean = g.rng.uniform(-0.25, 0.25) * half
        upper = [at(-half, 0), at(half, 0), at(half * 1.5 + lean, length * 0.34), at(-half * 1.5 + lean, length * 0.34)]
        lower = [upper[3], upper[2], at(0.5 + lean * 2, length), at(-0.5 + lean * 2, length)]
        for part in (upper, lower):
            g.framed(part, g.pick(SKIRT), "trail_01", lead=0.7)


def point(g, dims, p):
    """The trailing point beneath her: on each trail bone three narrow panes crossed about it, each segment turned a
    little from the one above, narrowing to the point that floats clear of the ground."""
    s = dims["shoulder"]
    radii = (0.3, 0.21, 0.13, 0.03)
    for k, name in enumerate(("trail_01", "trail_02", "trail_03")):
        a, b = p(name, 0), p(name, 1)
        if k == 0:
            a = a + V(0, 0, s * 0.2)
        d = unit(b - a)
        reach = b + d * (2.5 if k < 2 else 0.0)
        for j in range(3):
            angle = math.radians(j * 60 + k * 35)
            u, v = frame_across(d, angle)
            quad = [a + u * s * radii[k], a - u * s * radii[k], reach - u * s * radii[k + 1], reach + u * s * radii[k + 1]]
            g.framed(quad, g.pick(SKIRT), name, lead=0.6)


def shards(g, dims, p):
    """Shards drifting about her, two on each orbit bone (so they circle her, gather and scatter as it turns): each two
    crossed diamonds of glass, catching the light."""
    s = dims["shoulder"]
    for k in range(6):
        name = "orbit_0%d" % (k + 1)
        o0, o1 = p(name, 0), p(name, 1)
        out = unit(o1 - o0)
        for j, (rise, size) in enumerate(((-0.25, 0.45), (0.9, 0.55))):
            centre = o1 + V(0, 0, rise * s) + out * s * g.rng.uniform(-0.05, 0.15)
            along = unit(V(g.rng.uniform(-0.45, 0.45), g.rng.uniform(-0.45, 0.45), 1.0))
            u, v = frame_across(along, g.rng.uniform(0, math.pi))
            length, width = size * s, size * s * 0.36
            colour = "shard_" + g.pick({"amber": 3, "blue": 2, "violet": 2, "pale": 2, "cyan": 1})
            for across in (u, v):
                g.pane([centre - along * length * 0.5, centre + across * width * 0.5, centre + along * length * 0.5, centre - across * width * 0.5],
                       colour, name)
