"""Torr, The Unreturned (ADR-069): a colossal Fluxborn construct, read by his silhouette from the game camera. Character
Bible §9 and his splash art: a mass of rune-carved stone plates held in orbit about a blue-violet crystalline core by
visible Flux light. He is hulking: two great shoulders of stacked slabs, mossed on top, wider than all else; a head of
stacked stone sunk between them, a lit slit for a face; long arms of separate blocks hanging to great clustered fists; a
chest of plates about the core, which burns through a gap at its front; a narrower waist over two short columns of
stacked blocks, floating clear of the ground. A broken ring of stone rides over his head, lit along its inner edge, the
one part of him that holds still, and stones drift about him, each tied to him by a filament of Flux.

The gaps between his plates are the design: every plate stands clear of its neighbours over the Flux that holds them,
which glows through every gap, so he is lit from inside his own gaps. Never a solid statue, a suit of armour or a
seamless rock. Moss and mineral staining mark the plates longest in the ground; runes carved in the larger plates take
the same light as the core.

Low poly and flat-coloured (author 2026-10-07): the big forms that make his outline, each a flat colour the toon
material shades. Every part rides the bone his archetype gives it (ADR-064 construct): the trunk's plates the spine and
pelvis, each arm's plates its arm bones, the legs' blocks the trail, the drifting stones the orbit bones and the ring
the halo bone, so the clips circle, gather and scatter them. Fortified, the drifting stones stand as a wall of great
rune slabs before him; at Overcapacity his core flares and his plates stand further off it (both from his spec, as the
generated body reads them)."""
import numpy as np

from ..sculpt import anatomy, sdf, tree
from ..sculpt.anatomy import V, unit
from ..sculpt.tree import Box, Subtract, Union

# Sampled from the splash art (sRGB): its sunlit sandstone, the darker blocks packed about the core, the lit moss and
# the mineral staining. The kit's colours are drawn toward them.
ART = {"stone": (0.56, 0.48, 0.44), "deep": (0.36, 0.29, 0.27), "moss": (0.55, 0.56, 0.3), "mineral": (0.5, 0.52, 0.42)}

# Rune glyphs carved in a plate's face: strokes in the face's own coordinates (-1 to 1 across it and up it).
RUNES = (
    (((0.0, -0.8), (0.0, 0.15)), ((0.0, 0.15), (-0.55, 0.8)), ((0.0, 0.15), (0.55, 0.8))),
    (((-0.75, 0.0), (0.75, 0.0)), ((0.0, -0.75), (0.0, 0.75)), ((-0.45, -0.45), (0.45, 0.45))),
    (((0.0, -0.8), (0.0, 0.8)), ((0.0, 0.8), (-0.5, 0.25)), ((0.0, 0.8), (0.5, 0.25))),
    (((0.25, 0.85), (-0.35, 0.05)), ((-0.35, 0.05), (0.3, 0.05)), ((0.3, 0.05), (-0.25, -0.85))),
)

# The trunk's Flux, from the crotch to under the head: sections (height, forward offset, half depth, half width) as
# shares of his height. Hips over the legs, a narrow waist, a deep chest, the upper chest broad into the shoulders.
TRUNK = ((0.35, -0.02, 0.07, 0.1), (0.4, -0.01, 0.09, 0.15), (0.5, 0.0, 0.092, 0.13), (0.62, 0.012, 0.118, 0.168),
         (0.72, 0.025, 0.132, 0.19), (0.81, 0.01, 0.112, 0.2), (0.87, -0.005, 0.07, 0.12))


def mix(a, b, t):
    return tuple(float(a[i]) + (float(b[i]) - float(a[i])) * t for i in range(3))


def materials(S, spec):
    """His materials, from his kit's colours drawn toward the art: sandstone in four weathers, moss, and the Flux, which
    glows: deep between his plates, bright in the runes, filaments and joints, blue-violet in the core's crystals and
    nearly white at its heart."""
    stone = mix(spec["primary"], ART["stone"], 0.35)
    flux = spec["accent"]
    colours = {"stone": stone, "weathered": mix(stone, (0.9, 0.86, 0.76), 0.3), "deep": mix(stone, ART["deep"], 0.45),
               "mineral": mix(stone, ART["mineral"], 0.4), "moss": mix(spec["secondary"], ART["moss"], 0.5),
               "flux": mix(flux, (0.08, 0.06, 0.25), 0.5), "rune": mix(flux, (1.0, 1.0, 1.0), 0.3), "crystal": mix(flux, (1.0, 1.0, 1.0), 0.12),
               "core": mix(flux, (1.0, 1.0, 1.0), 0.75)}
    return {name: S.material(name, colour, glow=name in ("flux", "rune", "crystal", "core")) for name, colour in colours.items()}


def frame(normal, roll=0.0, tangent=None):
    """A plate's axes as columns: its face's normal, across its face (horizontal, or along tangent) and up its face,
    turned roll radians about the normal."""
    n = unit(normal)
    if tangent is None:
        ref = V(0, 0, 1) if abs(n[2]) < 0.9 else V(1, 0, 0)
        a = unit(np.cross(ref, n))
    else:
        t = V(*tangent)
        a = unit(t - n * (t @ n))
    b = np.cross(n, a)
    if roll:
        c, s = np.cos(roll), np.sin(roll)
        a, b = a * c + b * s, b * c - a * s
    return np.stack([n, a, b], axis=1)


def shaped(centre, axes, half, rounding, chops, puck):
    """A plate's distance: a rounded box (half its extents along axes' columns), or a rounded puck (an elliptic disc
    across the first axis), its corners chopped by planes (local normal, offset)."""
    c = np.asarray(centre, dtype=np.float32)
    axes = np.asarray(axes, dtype=np.float32)
    half = np.asarray(half, dtype=np.float32)
    inner = (half - rounding).astype(np.float32)

    def distance(P):
        Q = (P - c) @ axes
        if puck:
            r = min(inner[1], inner[2])
            across = (np.sqrt((Q[:, 1] / inner[1]) ** 2 + (Q[:, 2] / inner[2]) ** 2) - 1.0) * r
            q = np.stack([across, np.abs(Q[:, 0]) - inner[0]], axis=1)
        else:
            q = np.abs(Q) - inner
        d = np.linalg.norm(np.maximum(q, 0.0), axis=1) + np.minimum(q.max(axis=1), 0.0) - rounding
        for m, k in chops:
            d = np.maximum(d, Q @ m - k)
        return d
    return distance


def octahedron(centre, axes, radii):
    """A cut crystal: an octahedron of half-diagonals radii along axes' columns."""
    c = np.asarray(centre, dtype=np.float32)
    axes = np.asarray(axes, dtype=np.float32)
    r = np.asarray(radii, dtype=np.float32)
    norm = float(np.sqrt(np.sum(1.0 / (r * r))))

    def distance(P):
        Q = np.abs((P - c) @ axes)
        return (np.sum(Q / r, axis=1) - 1.0) / norm
    return distance


class Sculptor:
    """What every part of him is built with: the sculpt, his materials, his height, a seeded hand for each plate's own
    shape, and the plates and the Flux beneath them as they are made."""

    def __init__(self, S, spec, H):
        self.S, self.H = S, H
        self.rng = np.random.default_rng(spec["seed"])
        self.mats = materials(S, spec)
        self.plates = []
        self.flux = []
        self.count = 0

    def name(self, what):
        self.count += 1
        return "%s_%d" % (what, self.count)

    def plate(self, centre, normal, half, bones, material="stone", roll=0.0, tangent=None, rune=None, moss=0.0, chops=2,
              protect=0.25, lowest=None, puck=False, rounding=0.4, root=0.0):
        """One stone plate: a rounded block (or puck) facing out along normal, half its extents (deep, across, up), its
        corners chipped; a rune carved in its face, lit by the Flux, and a patch of moss over its top if asked (moss: the
        share of its top the patch spans). Its underside is kept above lowest if given. root sinks its back that much
        further into the Flux beneath it, its face where it was (a flat plate on a curved body then leaves no pocket
        under its edges). Returns its centre."""
        H, rng = self.H, self.rng
        c = V(*centre)
        axes = frame(normal, roll, tangent)
        half = V(*half)
        if root:
            c = c - axes[:, 0] * root * 0.5
            half[0] += root * 0.5
        if lowest is not None:
            c[2] = max(c[2], lowest + float(np.abs(axes[2]) @ half))
        cuts = []
        for _ in range(chops):
            s = rng.choice([-1.0, 1.0], 2)
            m = unit(V(1.0 / half[0], s[0] / half[1], s[1] / half[2]))
            cuts.append((m.astype(np.float32), float(m @ (half * V(1.0, s[0], s[1]))) * float(rng.uniform(0.74, 0.86))))
        distance = shaped(c, axes, half, float(min(half) * rounding), cuts, puck)
        reach = float(np.linalg.norm(half)) + 1.0
        node = tree.leaf(self.S, self.name("plate"), distance, Box(c - reach, c + reach), self.mats[material], bones, protect)
        if rune is not None:
            node = self.carve(node, c, axes, half, RUNES[rune % len(RUNES)], bones)
        self.plates.append(node)
        if moss > 0.0:
            self.moss(c, axes, half, distance, reach, moss, bones, protect)
        return c

    def moss(self, c, axes, half, distance, reach, share, bones, protect):
        """A ragged patch of moss over a plate's face (its top, on a plate facing up): the plate grown a little, kept to
        its face and within a wobbling round patch set a little off its middle, share of the face across."""
        H, rng = self.H, self.rng
        c32, axes32 = c.astype(np.float32), axes.astype(np.float32)
        spot = (rng.uniform(-0.25, 0.25) * half[1], rng.uniform(-0.25, 0.25) * half[2])
        radii = (share * half[1], share * half[2])
        lift = H * 0.006
        seed = int(rng.integers(1000))

        def patch(P):
            Q = (P - c32) @ axes32
            wobble = (sdf.noise(P, H * 0.04, seed) - 0.5) * 0.7
            across = np.sqrt(((Q[:, 1] - spot[0]) / radii[0]) ** 2 + ((Q[:, 2] - spot[1]) / radii[1]) ** 2) - 1.0 - wobble
            return np.maximum(np.maximum(distance(P) - lift, half[0] * 0.3 - Q[:, 0]), across * min(radii))
        self.plates.append(tree.leaf(self.S, self.name("moss"), patch, Box(c - reach - lift, c + reach + lift), self.mats["moss"], bones, protect))

    def carve(self, node, c, axes, half, glyph, bones):
        """A rune cut into the plate's face, its groove lit."""
        radius = self.H * 0.0085
        face = half[0] + radius * 0.15
        ends = []
        for (u0, v0), (u1, v1) in glyph:
            ends.append((c + axes @ V(face, u0 * half[1] * 0.62, v0 * half[2] * 0.62), c + axes @ V(face, u1 * half[1] * 0.62, v1 * half[2] * 0.62)))

        def distance(P):
            return np.min([sdf.capsule(P, a, b, radius) for a, b in ends], axis=0)
        points = [p for pair in ends for p in pair]
        rune = tree.leaf(self.S, self.name("rune"), distance, Box.around(points, radius + 1.0), self.mats["rune"], bones, protect=0.8)
        return Subtract(node, rune, label=rune.part.label)

    def glow(self, what, distance, bounds, bones, material="flux", protect=0.0):
        """Flux: the light that holds his plates, glowing between them."""
        node = tree.leaf(self.S, self.name(what), distance, bounds, self.mats[material], bones, protect)
        self.flux.append(node)
        return node

    def ellipsoid(self, centre, radii, bones, material="flux"):
        c, r = V(*centre), V(*radii)
        return self.glow("flux", lambda P: sdf.ellipsoid(P, c, r), Box(c - r.max(), c + r.max()), bones, material)

    def cone(self, a, b, ra, rb, bones, material="flux", protect=0.0):
        a, b = V(*a), V(*b)
        return self.glow("flux", lambda P: sdf.round_cone(P, a, b, ra, rb), Box.around([a, b], max(ra, rb)), bones, material, protect)

    def ball(self, c, r, bones, material="flux", protect=0.0):
        c = V(*c)
        return self.glow("flux", lambda P: sdf.sphere(P, c, r), Box(c - r, c + r), bones, material, protect)

    def filament(self, a, b, radius, bones):
        """A filament of Flux from a to b, thinning toward b (glowing)."""
        a, b = V(*a), V(*b)
        self.plates.append(tree.leaf(self.S, self.name("filament"), lambda P: sdf.round_cone(P, a, b, radius, radius * 0.5),
                                     Box.around([a, b], radius), self.mats["rune"], bones, protect=0.6))

    def tone(self, weights):
        """A plate's weather, drawn by weights over (stone, weathered, deep, mineral)."""
        return ["stone", "weathered", "deep", "mineral"][int(self.rng.choice(4, p=np.asarray(weights, dtype=float) / np.sum(weights)))]


class Trunk:
    """The trunk's Flux: an upright loft of elliptical sections, and points on its surface by height and azimuth."""

    def __init__(self, H):
        self.lo, self.hi = TRUNK[0][0] * H, TRUNK[-1][0] * H
        stations = [((z * H - self.lo) / (self.hi - self.lo), cx * H, 0.0, rx * H, ry * H) for z, cx, rx, ry in TRUNK]
        self.loft = sdf.Loft(V(0, 0, self.lo), V(0, 0, self.hi), V(1, 0, 0), stations, cap=H * 0.03)
        self.samples = np.arange(self.loft.SAMPLES)

    def point(self, z, azimuth):
        t = np.clip((z - self.lo) / (self.hi - self.lo), 0.0, 1.0) * (self.loft.SAMPLES - 1)
        cu, cv, ru, rv = (float(np.interp(t, self.samples, self.loft.curve[:, i])) for i in range(4))
        return V(cu + ru * np.cos(azimuth), cv + rv * np.sin(azimuth), z)

    def frame(self, z, azimuth):
        """The outward normal and the horizontal tangent at (z, azimuth)."""
        dz = (self.point(z + 0.5, azimuth) - self.point(z - 0.5, azimuth))
        da = (self.point(z, azimuth + 1e-3) - self.point(z, azimuth - 1e-3)) / 2e-3
        return unit(np.cross(da, dz)), unit(da)

    def arc(self, z0, z1, azimuth, steps=12):
        zs = np.linspace(z0, z1, steps + 1)
        points = [self.point(z, azimuth) for z in zs]
        return float(sum(np.linalg.norm(b - a) for a, b in zip(points, points[1:])))

    def perimeter(self, z, steps=48):
        points = [self.point(z, a) for a in np.linspace(0.0, 2 * np.pi, steps + 1)]
        return float(sum(np.linalg.norm(b - a) for a, b in zip(points, points[1:])))


def build(S, L, dims, spec):
    """Torr's sculpt on his layout: (the whole, {"body": the Flux that holds his plates, "sheets": none})."""
    H = dims["height"]
    K = Sculptor(S, spec, H)
    flare = spec.get("coreFlare", 1.0)
    # At Overcapacity his plates stand further off the Flux beneath them, its light spilling through wider gaps.
    spread = (flare - 1.0) * H * 0.02
    trunk(K, L, dims, flare, spread)
    head(K, L, dims)
    for side in ("l", "r"):
        shoulder(K, L, dims, side, spread)
        arm(K, L, dims, side, flare, spread)
        leg(K, L, dims, side, flare, spread)
    orbits(K, L, dims, spec, flare)
    halo(K, L, dims)
    flux = Union(K.flux, k=H * 0.025)
    root = Union([flux] + K.plates)
    return root, {"body": flux, "sheets": []}


def trunk(K, L, dims, flare, spread):
    """The trunk: the Flux that holds him, broad hips narrowing to a waist under a deep chest that broadens into the
    shoulders, tiled all round with plates standing clear of each other, deeper-coloured about the core; the core itself,
    a cluster of blue-violet crystal, burning through a gap at the front of his chest."""
    H = K.H
    waist = H * 0.56
    body = Trunk(H)
    K.glow("flux", body.loft, Box.around(body.loft.bounds_points()), anatomy.along("pelvis", "spine_03", V(0, 0, H * 0.48), V(0, 0, H * 0.64)))
    chest_bones, hip_bones = anatomy.rigid("spine_03"), anatomy.rigid("pelvis")
    # The core, deep in the chest and burning through the gap at its front: a heart of white-blue light ringed by
    # blue-violet crystals standing out from it like a burst, out to the plates' faces.
    core_height = H * 0.71
    normal, _ = body.frame(core_height, 0.0)
    heart = body.point(core_height, 0.0) - normal * H * 0.01
    K.ball(heart, H * 0.048 * flare, chest_bones, "core", protect=0.8)
    for turn in range(7):
        angle = turn / 7 * 2 * np.pi + 0.3
        out = unit(V(0.55, np.cos(angle), np.sin(angle) * 1.1))
        length = H * (0.072 if turn % 2 else 0.09) * flare
        crystal = heart + out * length * 0.55
        axes = frame(out, tangent=V(0, -np.sin(angle), np.cos(angle)))
        K.glow("crystal", octahedron(crystal, axes, V(length * 0.75, H * 0.024 * flare, H * 0.024 * flare)), Box(crystal - length, crystal + length),
               chest_bones, "crystal", protect=0.8)
    # Rows of plates up the trunk, brick-wise, each as wide and tall as its share of the surface less the gap; some span
    # two places, some stand prouder or turn, so the gaps between them run uneven. The row through the core is laid so
    # one place lies square before it, left open.
    gap = H * 0.009
    zs = np.linspace(H * 0.355, H * 0.83, 120)
    side_arc = np.array([0.0] + [0.5 * (body.arc(a, b, 0.0, 1) + body.arc(a, b, np.pi / 2, 1)) for a, b in zip(zs, zs[1:])]).cumsum()
    rows = int(round(side_arc[-1] / (H * 0.088)))
    edges = np.interp(np.linspace(0.0, side_arc[-1], rows + 1), side_arc, zs)
    core_row = int(np.argmin([abs((a + b) / 2 - core_height) for a, b in zip(edges, edges[1:])]))
    runes = 0
    for row in range(rows):
        z0, z1 = edges[row], edges[row + 1]
        z = (z0 + z1) / 2
        count = max(4, int(round(body.perimeter(z) / (H * 0.105))))
        slot = 2 * np.pi / count
        offset = 0.0 if row == core_row else K.rng.uniform(0.3, 0.7)
        index = 0
        while index < count:
            span = 2 if (index not in (0, count - 1) and K.rng.random() < 0.3) else 1
            azimuth = (index + offset + (span - 1) * 0.5 + K.rng.uniform(-0.05, 0.05)) * slot
            index += span
            if z < H * 0.43 and abs(np.sin(azimuth)) > 0.85:
                continue  # where the legs hang
            normal, across = body.frame(z, azimuth)
            point = body.point(z, azimuth)
            if row == core_row and span == 1 and abs(np.arctan2(np.sin(azimuth), np.cos(azimuth))) < slot * 0.3:
                continue  # the gap the core burns through
            wide = np.linalg.norm(body.point(z, azimuth + slot * span / 2) - body.point(z, azimuth - slot * span / 2))
            tall = body.arc(z0, z1, azimuth, 4)
            half = V(H * K.rng.uniform(0.017, 0.024), (wide / 2 - gap / 2) * K.rng.uniform(0.9, 1.0), (tall / 2 - gap / 2) * K.rng.uniform(0.9, 1.0))
            lift = half[0] * 0.45 + spread + K.rng.uniform(0.0, H * 0.016)
            turned = unit(normal + np.cross(normal, across) * K.rng.uniform(-0.12, 0.12) + across * K.rng.uniform(-0.12, 0.12))
            chest = z > waist
            near_core = np.linalg.norm(point - heart) < H * 0.2
            rune = None
            if chest and span == 2 and abs(np.cos(azimuth)) > 0.5 and runes < 2:
                rune, runes = runes + 2, runes + 1
            K.plate(point + normal * lift, turned, half, chest_bones if chest else hip_bones,
                    K.tone((2, 1, 4, 1) if near_core else (3, 2, 2, 1)), tangent=across, roll=K.rng.uniform(-0.12, 0.12), rune=rune, chops=3,
                    root=H * 0.03)
    # A collar of plates over the shoulders of the trunk beside and behind the head, tipped up toward the sky (the game
    # camera looks down on them).
    for azimuth in (60, 95, 130, -60, -95, -130):
        edge = body.point(H * 0.845, np.radians(azimuth))
        radial = unit(V(edge[0], edge[1], 0.0))
        spot = edge * V(0.92, 0.92, 1.0)
        K.plate(spot + radial * spread, radial * 0.8 + V(0, 0, 1), V(H * 0.018, H * 0.042, H * 0.036), chest_bones, K.tone((3, 2, 2, 1)),
                tangent=V(-radial[1], radial[0], 0), roll=K.rng.uniform(-0.15, 0.15), chops=3)
    # Under the hips, between the legs.
    K.plate(V(-H * 0.015, 0, H * 0.34), V(0.25, 0, -1), V(H * 0.02, H * 0.06, H * 0.065), hip_bones, "deep", tangent=V(0, 1, 0), chops=2)


def head(K, L, dims):
    """The head: three slabs of stone stacked over a ball of Flux, sunk between the shoulders, each smaller than the
    one beneath; between the lowest two, at the front, a single lit slit for a face."""
    H = K.H
    bones = anatomy.rigid("head")
    centre = V(H * 0.03, 0, H * 0.912)
    K.ball(centre, H * 0.055, bones)
    gap = H * 0.012
    z = centre[2] - H * 0.04
    for level, (thick, deep, wide, tilt, tone) in enumerate(((0.026, 0.085, 0.095, -8, "deep"), (0.025, 0.08, 0.088, 6, "stone"), (0.02, 0.056, 0.062, 4, "weathered"))):
        half = V(H * thick, H * deep, H * wide)
        if level:
            z += half[0] + gap
        normal = V(np.sin(np.radians(tilt)), 0, np.cos(np.radians(tilt)))
        K.plate(V(centre[0] + H * 0.01 * (1 - level), 0, z), normal, half, bones, tone, tangent=V(1, 0, 0), puck=True, rounding=0.55,
                chops=2, protect=0.5)
        z += half[0]
    # The slit: a bar of light in the gap between the lowest slabs, at the front.
    slit = V(centre[0] + H * 0.07, 0, centre[2] - H * 0.04 + H * 0.026 + gap * 0.5)
    K.glow("slit", lambda P, c=slit: sdf.box(P, c, (H * 0.016, H * 0.055, H * 0.008), None, H * 0.004), Box(slit - H * 0.07, slit + H * 0.07), bones, "core", protect=0.9)


def shoulder(K, L, dims, side, spread):
    """A great shoulder: two fat rounded slabs of stone stacked over the Flux in a broad mound, the upper smaller and
    tipped out, a lit seam between them, moss over the top, a rune-carved plate guarding its front."""
    H = K.H
    sign = 1.0 if side == "l" else -1.0
    bones = anatomy.rigid("clavicle_" + side)
    sc = V(-H * 0.01, sign * H * 0.33, H * 0.83)
    K.ellipsoid(sc + V(0, 0, H * 0.02), V(H * 0.065, H * 0.085, H * 0.05), bones)
    gap = H * 0.011 + spread
    z = sc[2] - H * 0.045
    for level, (thick, deep, wide, tip, out, moss) in enumerate(((0.052, 0.115, 0.14, 20, 0.012, 0.0), (0.046, 0.088, 0.1, 10, 0.03, 0.85))):
        half = V(H * thick, H * deep, H * wide)
        if level:
            z += half[0] + gap
        normal = V(0, sign * np.sin(np.radians(tip)), np.cos(np.radians(tip)))
        K.plate(V(sc[0] - H * 0.01 * level, sc[1] + sign * H * out, z), normal, half, bones, "weathered" if level else "stone",
                tangent=V(1, 0, 0), roll=K.rng.uniform(-0.1, 0.1), moss=moss, chops=3, puck=True, rounding=0.9)
        z += half[0]
    K.plate(V(H * 0.13, sign * H * 0.31, H * 0.79), V(1, sign * 0.35, 0.15), V(H * 0.022, H * 0.056, H * 0.05), bones, "stone",
            rune=1 if side == "l" else 0, protect=0.4, rounding=0.5)


def arm(K, L, dims, side, flare, spread):
    """An arm of separate blocks over a limb of Flux, lit at elbow and wrist, hanging to a great fist of clustered stone
    that reaches far below the wrist."""
    H = K.H
    sign = 1.0 if side == "l" else -1.0
    s, e = V(*L["upperarm_" + side][0]), V(*L["upperarm_" + side][1])
    w, end = V(*L["lowerarm_" + side][1]), V(*L["hand_" + side][1])
    upper, lower, hand = anatomy.rigid("upperarm_" + side), anatomy.rigid("lowerarm_" + side), anatomy.rigid("hand_" + side)
    K.cone(s, e, H * 0.032, H * 0.03, upper)
    K.cone(e, w, H * 0.032, H * 0.036, lower)
    K.ball(e, H * 0.035 * flare, lower, "rune", protect=0.4)
    K.ball(w, H * 0.032 * flare, hand, "rune", protect=0.4)
    for a, b, bones, length, width in ((s, e, upper, 0.05, 0.045), (e, w, lower, 0.062, 0.05)):
        axis = unit(b - a)
        mid = a + (b - a) * (0.62 if bones is upper else 0.5)
        for index, out in enumerate((V(1, 0.3 * sign, 0), V(0, sign, 0), V(-1, 0.3 * sign, 0))):
            n = unit(out - axis * (out @ axis))
            half = V(H * 0.02, H * width * K.rng.uniform(0.92, 1.05), H * length * K.rng.uniform(0.92, 1.08))
            K.plate(mid + n * (H * 0.038 + half[0] * 0.3 + spread), n, half, bones, K.tone((3, 2, 1, 1)), tangent=np.cross(axis, n),
                    roll=K.rng.uniform(-0.12, 0.12), rune=(3 if (bones is lower and index == 1 and side == "r") else None), root=H * 0.02)
    # The fist: a cluster of rounded blocks about a heart of Flux, mossed over its back on his left.
    down = unit(end - w)
    fist = w + down * H * 0.14 + V(0, -sign * H * 0.035, 0)
    K.ball(fist, H * 0.065, hand)
    for index, (out, size) in enumerate(((V(1, 0, -0.3), 1.0), (V(0, sign, -0.15), 0.95), (V(0.25, 0, -1), 1.05), (V(-0.85, 0.2 * sign, -0.1), 0.9),
                                         (V(0.3, 0.25 * sign, 1.0), 0.95))):
        n = unit(out)
        half = V(H * 0.04, H * 0.058, H * 0.053) * size * K.rng.uniform(0.92, 1.08)
        K.plate(fist + n * (H * 0.057 + spread), n, half, hand, K.tone((3, 2, 1, 1)), roll=K.rng.uniform(-0.4, 0.4), chops=3, protect=0.4,
                moss=(0.8 if (index == 4 and side == "l") else 0.0), rune=(2 if (index == 1 and side == "l") else None), rounding=0.5)


def leg(K, L, dims, side, flare, spread):
    """A leg under the core: a column of upright blocks over a limb of Flux, lit at the knee and ankle, on a broad foot
    of stone that floats at the archetype's hover height (it rides the trail, so it sweeps back as he glides)."""
    H = K.H
    sign = 1.0 if side == "l" else -1.0
    core_z, hover = L["trail_01"][0][2], dims["hover"]
    back = L["trail_03"][1][0]

    def at(z):
        return V(back * (core_z - z) / max(core_z - hover, 1e-6), sign * H * 0.172, z)
    top, knee, ankle = at(H * 0.42), at(H * 0.27), at(H * 0.13)
    thigh, shin, foot = anatomy.rigid("trail_01"), anatomy.rigid("trail_02"), anatomy.rigid("trail_03")
    K.cone(top, knee, H * 0.045, H * 0.04, thigh)
    K.cone(knee, ankle, H * 0.04, H * 0.035, shin)
    K.ball(knee, H * 0.032 * flare, shin, "rune", protect=0.4)
    K.ball(ankle, H * 0.028 * flare, foot, "rune", protect=0.4)
    for a, b, bones, length, width in ((top, knee, thigh, 0.072, 0.06), (knee, ankle, shin, 0.06, 0.052)):
        mid = (a + b) / 2
        for index, out in enumerate((V(1, 0.3 * sign, 0), V(0.15, sign, 0), V(-1, 0.25 * sign, 0))):
            n = unit(out)
            half = V(H * 0.022, H * width * K.rng.uniform(0.9, 1.05), H * length * K.rng.uniform(0.92, 1.05))
            K.plate(mid + n * (H * 0.05 + spread), n, half, bones, K.tone((3, 2, 1, 2)), roll=K.rng.uniform(-0.1, 0.1),
                    rune=(0 if (bones is thigh and index == 0 and side == "l") else None), root=H * 0.02)
    centre = ankle + V(H * 0.03, 0, -H * 0.06)
    K.plate(centre, V(0, 0, 1), V(H * 0.05, H * 0.085, H * 0.065), foot, "deep", tangent=V(1, 0, 0), chops=3, protect=0.3,
            moss=(0.6 if side == "l" else 0.0), lowest=hover, rounding=0.5)


def orbits(K, L, dims, spec, flare):
    """The drifting stones, where the orbit bones hold them (between the arms, never against them): a stone and a
    smaller one by it, the larger tied to the core by a filament of Flux; each rigid on its bone, so the clips circle,
    gather and scatter it. Fortified, each is instead a great rune slab, the six a wall before him."""
    H = K.H
    for index in range(1, 7):
        name = "orbit_%02d" % index
        o0, o1 = V(*L[name][0]), V(*L[name][1])
        bones = anatomy.rigid(name)
        if spec.get("fortified"):
            n = unit(V(1.0, 0.2 * np.sign(o1[1] - o0[1]), 0.05))
            half = V(H * 0.03, H * 0.125, H * 0.15)
            K.plate(o1, n, half, bones, K.tone((3, 2, 1, 1)), rune=index, chops=3, protect=0.4)
            K.filament(o0 + (o1 - o0) * 0.3, o1 - n * half[0], H * 0.007 * flare, bones)
            continue
        out = unit(o1 - o0)
        half = V(H * 0.04, H * 0.055, H * 0.05) * K.rng.uniform(0.85, 1.15)
        mossed = index in (3, 6)
        # The pair straddles where the bone holds them, so neither hangs square on the body's line.
        across = unit(np.cross(out, V(0, 0, 1))) * (1 if index % 2 else -1)
        stone = o1 + across * H * 0.045
        K.plate(stone, unit(out + V(0, 0, 1.2)) if mossed else out, half, bones, K.tone((3, 2, 1, 2)), roll=K.rng.uniform(-1.0, 1.0), chops=3,
                protect=0.6, rounding=0.5, rune=(index if index in (2, 5) else None), moss=(0.8 if mossed else 0.0))
        small = o1 - across * H * 0.07 + V(0, 0, H * 0.05 * (1 if index % 3 else -1))
        K.plate(small, out + V(0, 0, 0.4), V(H * 0.024, H * 0.032, H * 0.03), bones, K.tone((2, 2, 1, 1)), roll=K.rng.uniform(-1.0, 1.0), chops=2,
                protect=0.7, rounding=0.5)
        K.filament(o0 + unit(stone - o0) * H * 0.1, stone - unit(stone - o0) * half[0], H * 0.006 * flare, bones)


def halo(K, L, dims):
    """The broken ring of stone over his head and shoulders, on the halo bone: blocks set round it with a gap where
    two are missing, lit along its inner edge by a ring of Flux that runs on across the gap."""
    H = K.H
    bones = anatomy.rigid("halo")
    centre = V(*L["halo"][0]) + V(0, 0, H * 0.06)
    radius = dims["shoulder"] * 0.46
    slots = 11
    half = V(H * 0.03, H * 0.036, H * 0.024)
    for index in range(slots):
        if index in (2, 3):
            continue
        angle = (index + 0.5) / slots * 2.0 * np.pi
        radial = V(np.cos(angle), np.sin(angle), 0)
        K.plate(centre + radial * radius + V(0, 0, K.rng.uniform(-0.15, 0.15) * half[2]), radial, half, bones, K.tone((3, 3, 1, 1)),
                tangent=V(-np.sin(angle), np.cos(angle), 0), roll=K.rng.uniform(-0.12, 0.12), chops=2, protect=0.4)
    inner = radius - half[0] * 0.85
    K.plates.append(tree.leaf(K.S, K.name("ring"), lambda P: sdf.torus(P, centre, inner, H * 0.008), Box(centre - inner - H * 0.02, centre + inner + H * 0.02),
                              K.mats["rune"], bones, protect=0.6))
