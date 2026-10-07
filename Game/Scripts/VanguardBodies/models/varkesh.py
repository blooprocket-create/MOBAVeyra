"""Varkesh, The Forgeheart (ADR-069): a colossus of dark interlocking iron plates over a molten interior, read by his
silhouette from the game camera. Character Bible §14 and his splash art: a roughly 3 m figure, top-heavy, on enormous
layered shoulders, heavy plated forearms and long plated legs. The seams between his plates glow, wider where the plates
of his extremities ride clear of him; a bright orange-white core spiral is set into his chest in a socket of dark plate.
His head is a faceless wedge of plate set low between the shoulders, crested with short spines and lit from inside
through its seams. Both hands are open and empty, fingers spread, and molten fragments torn from his plating hang about
them, ready to be thrown. A heavy tattered drape hangs at his waist from a dark iron band, held by iron rings. The metal
is his body: no face, no figure inside it, no weapon.

Low poly and flat-coloured (author 2026-10-07): the big forms that make his outline, each a flat colour the toon
material shades. He rests as his archetype lays him out (ADR-064); his clips stomp, throw and slam. Every colour comes
from his kit entry (spec), lifted where the toon light would take it to black, so a status body that cools him
(Tempered: blacker plates, near-closed dull seams, the core still lit) draws as its entry reads."""
import numpy as np

from ..sculpt import anatomy, garments, paint, sdf, sheet, tree
from ..sculpt.anatomy import V, unit
from ..sculpt.tree import Box, Over, Shell, Union, Zone

EVERYWHERE = Box(V(-500, -500, -50), V(500, 500, 500))
WHITE = (1.0, 1.0, 1.0)


def mix(a, b, t):
    return tuple(x + (y - x) * t for x, y in zip(a, b))


def lift(colour):
    """An iron the kit gives dark, as the toon light needs it: values under about 0.2 shade to black, so they are raised
    toward the art's lit iron, and a cooled shell (darker in its entry) stays the darker."""
    return tuple(min(1.0, 0.06 + 1.4 * c) for c in colour)


def materials(S, spec):
    """His materials from his entry: the molten interior in his accent (glowing), iron plates from his primary (some
    worn toward his secondary), the core spiral in his core colour (his accent run white-hot when his entry gives none,
    as the generated body draws it), the drape in his detail colour."""
    primary, secondary, accent, detail = (tuple(spec[key]) for key in ("primary", "secondary", "accent", "detail"))
    core = tuple(spec.get("core", mix(accent, WHITE, 0.45)))
    colours = {"molten": (accent, True), "iron": (lift(primary), False), "worn": (lift(mix(primary, secondary, 0.35)), False),
               "socket": (mix(lift(primary), (0.0, 0.0, 0.0), 0.35), False), "ring": (mix(secondary, (0.55, 0.52, 0.48), 0.4), False),
               "core": (core, True), "heart": (mix(core, WHITE, 0.5), True), "drape": (detail, False),
               "drape_dark": (mix(detail, (0.15, 0.04, 0.03), 0.35), False), "drape_faded": (mix(detail, (0.6, 0.3, 0.22), 0.3), False)}
    return {name: S.material(name, colour, glow) for name, (colour, glow) in colours.items()}


# ---------------------------------------------------------------------------------------------- plating
class Plating:
    """Where his plates lie over his interior. Each point belongs to the segment of his interior whose surface is
    nearest (the trunk, a shoulder, an arm, a leg); along that segment its plates run in rows, round it in sectors
    staggered from row to row and slanted, so they interlock as iron does, not as cells of stone. A point's seam is how
    far it lies from the nearest edge of its plate (cm): a row's edge, a sector's edge, or the joint where two segments
    meet. Cached for the last points asked (each shell, and each shell's rise, asks for the same points)."""

    def __init__(self, segments):
        self.segments = []
        for index, segment in enumerate(segments):
            a, b = V(*segment["a"]), V(*segment["b"])
            axes = sdf.frame(b - a, segment.get("hint", (1.0, 0.0, 0.0)))
            self.segments.append(dict(segment, a=a, b=b, length=float(np.linalg.norm(b - a)), w=axes[:, 2], x=axes[:, 0], y=axes[:, 1],
                                      salt=11.0 + index * 3.7, downward=b[2] < a[2]))
        self.last = (None, None)

    def at(self, P):
        """(seam cm, plate id, share toward the plate's lower edge, how wide its seams run, whether it is bare)."""
        if self.last[0] is P:
            return self.last[1]
        Q = np.asarray(P, dtype=np.float64)
        D = np.stack([segment["distance"](Q) for segment in self.segments], axis=1)
        order = np.argsort(D, axis=1)
        nearest = order[:, 0]
        first = np.take_along_axis(D, order[:, :1], axis=1)[:, 0]
        second = np.take_along_axis(D, order[:, 1:2], axis=1)[:, 0]
        # Where two segments meet (their surfaces about as near), a seam: narrow, as collinear limbs stay near alike
        # over a long stretch.
        seam = (second - first) * 2.0
        plate = np.zeros(len(Q))
        lower = np.zeros(len(Q))
        wide = np.ones(len(Q))
        bare = np.zeros(len(Q), dtype=bool)
        for index, segment in enumerate(self.segments):
            sel = nearest == index
            if not sel.any():
                continue
            if segment.get("bare"):
                bare[sel] = True
                continue
            R = Q[sel] - segment["a"]
            t = (R @ segment["w"]) / segment["length"]
            x, y = R @ segment["x"], R @ segment["y"]
            theta = np.arctan2(y, x)
            radius = np.hypot(x, y)
            rows, sectors = segment["rows"], segment["sectors"]
            # Row edges wander a little round the segment, so no plate is a perfect band.
            tw = t + (0.12 / rows) * np.sin(2.0 * theta + segment["salt"])
            row = np.clip(np.floor(tw * rows), 0, rows - 1)
            if rows > 1:
                edges = np.arange(1, rows) / rows
                row_seam = np.min(np.abs(tw[:, None] - edges[None, :]), axis=1) * segment["length"]
            else:
                row_seam = np.full(len(t), np.inf)
            within = np.clip(tw * rows - row, 0.0, 1.0)
            # Sectors staggered half a plate from row to row, each row's seams slanted the other way.
            s = (theta / (2.0 * np.pi) * sectors + 0.5 * row + 0.35 * paint.hashed(row + index * 10, segment["salt"])
                 + 0.3 * (within - 0.5) * np.where(row % 2 == 0, 1.0, -1.0))
            sector = np.mod(np.floor(s), sectors)
            sector_seam = np.abs(s - np.round(s)) * (2.0 * np.pi / sectors) * radius
            seam[sel] = np.minimum(seam[sel], np.minimum(row_seam, sector_seam))
            plate[sel] = index * 1000 + row * 31 + sector
            lower[sel] = within if segment["downward"] else 1.0 - within
            wide[sel] = segment.get("wide", 1.0)
        result = (seam, plate, lower, wide, bare)
        self.last = (P, result)
        return result


def lame(P, centre, radius, thickness, facing, along, half_along, half_across):
    """A curved plate: a patch of a spherical shell about centre (from radius out by thickness), facing out along facing,
    its edges straight in angle (half_along either way along `along`, half_across either way across it, in radians)."""
    Q = np.asarray(P, dtype=np.float64) - centre
    r = np.linalg.norm(Q, axis=1)
    shell = np.abs(r - (radius + thickness * 0.5)) - thickness * 0.5
    f = unit(facing)
    a = unit(along - f * (along @ f))
    c = np.cross(f, a)
    x = Q @ f
    patch = np.maximum(np.abs(np.arctan2(Q @ a, x)) - half_along, np.abs(np.arctan2(Q @ c, x)) - half_across) * np.maximum(r, 1.0)
    return np.maximum(shell, patch)


# ---------------------------------------------------------------------------------------------- build
def build(S, L, dims, spec):
    """Varkesh's sculpt on his layout: (the whole, {"body": his molten interior, under his plates, "sheets": his drape})."""
    mats = materials(S, spec)
    H = dims["height"]
    p = lambda bone, i: V(*L[bone][i])  # noqa: E731
    gap = float(spec.get("seamGap", 0.2))

    core_parts, segments = [], []
    shoulder_centre, radii = {}, V(0.11, 0.1, 0.09) * H

    def interior(name, distance, bounds, bones, segment=None):
        """A piece of his molten interior; with segment, the plating's record of it."""
        core_parts.append(tree.leaf(S, name, distance, bounds, mats["molten"], bones))
        if segment is not None:
            segments.append(dict(segment, distance=distance))

    # The trunk: one form up the forward-leaning spine, broadest and deepest at the chest, narrowing to the waist; its
    # top drawn back so the head stands ahead of it.
    pelvis, chest = p("pelvis", 0), p("spine_03", 1)
    axis = unit(chest - pelvis)
    trunk_a, trunk_b = pelvis - axis * H * 0.07, chest - axis * H * 0.02
    trunk = sdf.Loft(trunk_a, trunk_b, (1.0, 0.0, 0.0), [
        (0.0, -0.005 * H, 0.0, 0.095 * H, 0.115 * H, 2.4), (0.12, 0.0, 0.0, 0.105 * H, 0.135 * H, 2.4),
        (0.3, 0.005 * H, 0.0, 0.1 * H, 0.118 * H, 2.4), (0.55, 0.015 * H, 0.0, 0.135 * H, 0.17 * H, 2.6),
        (0.78, 0.02 * H, 0.0, 0.165 * H, 0.215 * H, 2.6), (0.9, 0.005 * H, 0.0, 0.15 * H, 0.2 * H, 2.6),
        (1.0, -0.04 * H, 0.0, 0.09 * H, 0.13 * H, 2.4)], cap=0.03 * H)
    interior("trunk", trunk, Box.around(trunk.bounds_points()), anatomy.spine_weights(L),
             dict(a=trunk_a, b=trunk_b, rows=4, sectors=8))
    # The neck: molten, a glowing collar under the head, never standing out ahead of it.
    neck_a, neck_b = p("neck_01", 0) - V(H * 0.02, 0, H * 0.02), p("head", 0) + V(-H * 0.03, 0, H * 0.035)
    interior("neck", lambda P: sdf.round_cone(P, neck_a, neck_b, H * 0.042, H * 0.036), Box.around([neck_a, neck_b], H * 0.06),
             anatomy.along("spine_03", "head", neck_a, neck_b, 0.3, 0.9), dict(a=neck_a, b=neck_b, bare=True))
    for side, sign in (("l", 1.0), ("r", -1.0)):
        s, e, w = p("upperarm_" + side, 0), p("upperarm_" + side, 1), p("lowerarm_" + side, 1)
        c = s + V(-0.005 * H, -sign * 0.02 * H, -0.015 * H)
        shoulder_centre[side] = c
        inner, outer = s - V(0, sign * 0.06 * H, 0), s + V(0, sign * 0.06 * H, 0)
        interior("shoulder_" + side, lambda P, c=c, radii=radii: sdf.ellipsoid(P, c, radii), Box(c - radii.max(), c + radii.max()),
                 anatomy.along("clavicle_" + side, "upperarm_" + side, inner, outer, 0.35, 0.85),
                 dict(a=inner, b=outer, rows=1, sectors=5, hint=(0.0, 0.0, 1.0)))
        interior("upperarm_" + side, lambda P, a=s, b=e: sdf.round_cone(P, a, b, H * 0.07, H * 0.062), Box.around([s, e], H * 0.07),
                 anatomy.rigid("upperarm_" + side), dict(a=s, b=e, rows=2, sectors=5))
        # The forearm stops short of the wrist, so the open hand below it shows whole.
        cuff = w - unit(w - e) * 0.035 * H
        interior("forearm_" + side, lambda P, a=e, b=cuff: sdf.round_cone(P, a, b, H * 0.07, H * 0.054), Box.around([e, w], H * 0.075),
                 anatomy.rigid("lowerarm_" + side), dict(a=e, b=cuff, rows=2, sectors=5, wide=1.5))
        hp, k, hock = p("thigh_" + side, 0), p("thigh_" + side, 1), p("calf_" + side, 1)
        top = hp - V(0, 0, 0.01 * H)
        interior("thigh_" + side, lambda P, a=top, b=k: sdf.round_cone(P, a, b, H * 0.085, H * 0.068), Box.around([top, k], H * 0.09),
                 anatomy.along("pelvis", "thigh_" + side, hp + V(0, 0, H * 0.05), hp - V(0, 0, H * 0.06), 0.2, 0.7),
                 dict(a=top, b=k, rows=2, sectors=6))
        ankle = hock + V(0, 0, 0.01 * H)
        interior("calf_" + side, lambda P, a=k, b=ankle: sdf.round_cone(P, a, b, H * 0.068, H * 0.06), Box.around([k, ankle], H * 0.07),
                 anatomy.rigid("calf_" + side), dict(a=k, b=ankle, rows=2, sectors=5, wide=1.5))
    core = Union(core_parts, k=H * 0.03)

    # His plates: two tones of iron over the interior, cut apart along their seams (the interior glowing in them), each
    # plate standing a little proud of its neighbours and its lower edge proudest, as lames overlap.
    plating = Plating(segments)
    half_seam = gap * 0.07 * H * 0.5
    amp, shingle = 0.006 * H, 0.006 * H

    def plates(tone_in):
        def region(P):
            seam, plate, _lower, wide, bare = plating.at(P)
            mine = (paint.hashed(plate, 7.0) < 0.55) == tone_in
            return np.where(bare | ~mine, 100.0, half_seam * wide - seam)
        return region

    def rise(P):
        _seam, plate, lower, wide, _bare = plating.at(P)
        return amp * wide * (0.2 + 0.8 * paint.hashed(plate, 13.0)) + shingle * wide * lower

    thickness = 0.008 * H
    reach = (amp + shingle) * 1.5
    plated = Over([core, Shell(S, "plates", core, 0.0, thickness, Zone(plates(True), EVERYWHERE), mats["iron"], hem=0.4, displace=rise, reach=reach),
                   Shell(S, "plates_worn", core, 0.0, thickness, Zone(plates(False), EVERYWHERE), mats["worn"], hem=0.4, displace=rise, reach=reach)])

    belt_z = p("pelvis", 0)[2] + (chest[2] - p("pelvis", 0)[2]) * 0.1
    centre_at_belt = pelvis + axis * ((belt_z - pelvis[2]) / max(axis[2], 1e-6))
    belt = Shell(S, "belt", core, 0.0, 0.026 * H, Zone(lambda P: np.maximum(np.abs(P[:, 2] - belt_z) - 0.022 * H,
                                                                          (np.hypot((P[:, 0] - centre_at_belt[0]) / (0.25 * H), P[:, 1] / (0.21 * H)) - 1.0) * 30.0),
                                                       EVERYWHERE), mats["socket"], hem=0.5, bones=anatomy.rigid("pelvis"))
    layers = [plated, belt, pauldrons(S, L, H, mats, shoulder_centre, radii), knees(S, L, H, mats), head(S, L, dims, mats, gap)]
    for side in ("l", "r"):
        layers += [hand(S, L, H, mats, side), foot(S, L, H, mats, side), shards(S, L, H, mats, side)]
    layers += [spiral(S, L, H, mats, core, trunk_a, trunk_b), rings(S, L, H, mats, core, belt_z)]
    worn = Over(layers)
    # Cut flat where he meets the ground: nothing of him sinks below it.
    worn = tree.Intersect(worn, Zone(lambda P: -P[:, 2], Box(V(-500, -500, -0.5), V(500, 500, 600))))
    return worn, {"body": core, "sheets": drape(S, L, H, mats, core, belt_z)}


# ---------------------------------------------------------------------------------------------- parts
def pauldrons(S, L, H, mats, shoulder_centre, shoulder_radii):
    """His enormous shoulders: on each, rows of great flat slabs fanned from the top of the shoulder out and down over
    the arm, each row overlapping the one below it and its lower edge jutting out, each row a front and a back slab
    angled to wrap the shoulder. The top row rides the clavicle into the arm, the others the arm."""
    parts = []
    fore = V(1.0, 0, 0)
    for side, sign in (("l", 1.0), ("r", -1.0)):
        s = V(*L["upperarm_" + side][0])
        c = shoulder_centre[side]
        for k, (angle, depth, length) in enumerate(((8.0, 0.1, 0.06), (44.0, 0.098, 0.058), (80.0, 0.09, 0.055), (114.0, 0.078, 0.05))):
            phi = np.radians(angle)
            normal = V(0, sign * np.sin(phi), np.cos(phi))
            # Where the shoulder's surface lies that way, and the slab over it (each lower row a little nearer him, so the
            # row above laps over it).
            reach = 1.0 / np.sqrt(np.sum((normal / shoulder_radii) ** 2)) + (0.024 - 0.004 * k) * H
            tangent = V(0, sign * np.cos(phi), -np.sin(phi))
            tilt = np.radians(10.0)
            tangent, normal = tangent * np.cos(tilt) + normal * np.sin(tilt), normal * np.cos(tilt) - tangent * np.sin(tilt)
            bones = (anatomy.along("clavicle_" + side, "upperarm_" + side, s - V(0, sign * 0.1 * H, 0), s + V(0, sign * 0.05 * H, 0), 0.3, 0.9)
                     if k == 0 else anatomy.rigid("upperarm_" + side))
            for half_index, wrap in enumerate((1.0, -1.0)):
                # Front and back slabs, each turned about the fan's line to follow the shoulder round.
                turn = np.radians(22.0) * wrap
                n = normal * np.cos(turn) + fore * np.sin(turn)
                f = fore * np.cos(turn) - normal * np.sin(turn)
                centre = c + n * reach + f * wrap * depth * H * 0.48
                axes = np.stack([f, tangent, n], axis=1)
                half = V(depth * 0.52, length, 0.011) * H
                parts.append(tree.leaf(S, "pauldron_%s_%d_%d" % (side, k, half_index),
                                       lambda P, centre=centre, axes=axes, half=half: sdf.box(P, centre, half, axes, 0.004 * H),
                                       Box(centre - 0.12 * H, centre + 0.12 * H), mats["iron"] if (k + half_index) % 2 == 0 else mats["worn"], bones, protect=0.3))
    return Union(parts)


def knees(S, L, H, mats):
    """A broad curved plate over each knee, riding the shin."""
    parts = []
    for side in ("l", "r"):
        k = V(*L["calf_" + side][0])
        c = k - V(0.01 * H, 0, 0)
        parts.append(tree.leaf(S, "knee_" + side, lambda P, c=c: lame(P, c, 0.078 * H, 0.014 * H, V(1.0, 0, 0.1), V(0, 0, 1.0), 0.7, 0.75),
                               Box(c - 0.11 * H, c + 0.11 * H), mats["worn"], anatomy.rigid("calf_" + side), protect=0.3))
    return Union(parts)


def polyhedron(P, planes):
    """The solid inside every plane (each a point and its outward normal): a cut block of plate, its edges sharp."""
    return np.max(np.stack([sdf.half_space(P, point, unit(normal)) for point, normal in planes]), axis=0)


def head(S, L, dims, mats, gap):
    """A faceless wedge of plate set low between the shoulders: a tall helm whose front closes to a prow (two faces
    meeting at an upright edge), its plates parted by seams that glow from the molten interior inside (a slit across its
    upper front, a seam down each side; lit from inside like the rest of him, never a face), crested along its top with
    four short spines swept back."""
    H = dims["height"]
    # Larger than his layout's head, standing a little above and ahead of it, so it reads from the game camera between
    # those shoulders as his art draws it.
    o = V(*L["head"][0]) + V(0.02 * H, 0, 0)
    # (Its spines' tips stay within his body's height bound: no body stands more than 1.4 of its capsule tall.)
    s = max(dims["head"], H * 0.06) * 1.4
    bones = anatomy.rigid("head")
    parts = []
    at = lambda x, y, z: o + V(x, y, z) * s  # noqa: E731
    prow = np.radians(38.0)
    planes = [(at(-0.45, 0, 0), V(-1, 0, 0.1)), (at(0, 0, 1.05), V(0.12, 0, 1)), (at(0, 0, 0.12), V(0, 0, -1)),
              (at(0, 0.44, 0), V(0, 1, 0.2)), (at(0, -0.44, 0), V(0, -1, 0.2)),
              (at(0.48, 0, 0), V(np.cos(prow), np.sin(prow), 0.22)), (at(0.48, 0, 0), V(np.cos(prow), -np.sin(prow), 0.22)),
              (at(0.35, 0, 0.15), V(0.6, 0, -1)), (at(-0.45, 0, 0.92), V(-0.7, 0, 1))]

    def block(P):
        return polyhedron(P, planes)
    bounds = Box(at(-0.6, -0.5, 0.0), at(0.65, 0.5, 1.2))
    # The seams, as wide as his seams run: one slanting across the upper face and round the sides (a crack between
    # plates, never a visor), the helm above it and the lower plate below; and one down each side, between the prow
    # plate and the back.
    half = max(0.03 * s, gap * 0.3 * s)
    level, level_point = unit(V(0.15, 0.3, 1.0)), at(0, 0, 0.58)
    split, split_point = unit(V(1.0, 0, 0.25)), at(0.1, 0, 0)

    def upper(P):
        return np.maximum(block(P), -sdf.half_space(P, level_point + level * half, level))
    # The molten interior just inside the plates, showing only through the seams.
    parts.append(tree.leaf(S, "head_glow", lambda P: block(P) + 0.03 * s, bounds, mats["molten"], bones, protect=0.6))
    parts.append(tree.leaf(S, "prow", lambda P: np.maximum(upper(P), -sdf.half_space(P, split_point + split * half, split)), bounds, mats["worn"], bones, protect=0.6))
    parts.append(tree.leaf(S, "helm", lambda P: np.maximum(upper(P), sdf.half_space(P, split_point - split * half, split)), bounds, mats["iron"], bones, protect=0.6))
    parts.append(tree.leaf(S, "head_lower", lambda P: np.maximum(block(P), sdf.half_space(P, level_point - level * half, level)), bounds,
                           mats["iron"], bones, protect=0.6))
    for k, x in enumerate((0.3, 0.06, -0.18, -0.38)):
        # Each rooted just under the top, which falls a little toward the front.
        base = at(x, 0, 1.05 - 0.12 * x - 0.05)
        tip = base + V(-0.3, 0, 0.27 - k * 0.03) * s
        parts.append(tree.leaf(S, "spine_%d" % k, lambda P, a=base, b=tip: sdf.round_cone(P, a, b, 0.09 * s, 0.012 * s), Box.around([base, tip], 0.1 * s),
                               mats["worn"], bones, protect=0.6))
    return Union(parts)


def hand_frame(L, side):
    """A hand's frame: its wrist, x down the hand, out away from the body, forward (where its open palm faces)."""
    w, end = V(*L["hand_" + side][0]), V(*L["hand_" + side][1])
    sign = 1.0 if side == "l" else -1.0
    x = unit(end - w)
    out = unit(V(0, sign, 0) - x * (x[1] * sign))
    fore = unit(np.cross(out, x))
    return w, x, out, (fore if fore[0] >= 0 else -fore)


def hand(S, L, H, mats, side):
    """An open, empty hand, its palm facing forward and its five fingers spread and a little curled, as though about to
    draw something off himself: a broad iron palm, a plate over its back, iron fingers and a glowing seam at each
    knuckle."""
    w, x, out, fore = hand_frame(L, side)
    bones = anatomy.rigid("hand_" + side)
    axes = np.stack([x, out, fore], axis=1)
    # Large, as his art draws them: the hands are what he works with, and they must read from the game camera.
    palm = w + x * 0.04 * H
    parts = [tree.leaf(S, "palm_" + side, lambda P: sdf.box(P, palm, V(0.05, 0.052, 0.026) * H, axes, 0.012 * H), Box(palm - 0.08 * H, palm + 0.08 * H),
                       mats["iron"], bones, protect=0.8),
             tree.leaf(S, "knuckle_glow_" + side, lambda P: sdf.capsule(P, palm + x * 0.048 * H - out * 0.042 * H, palm + x * 0.048 * H + out * 0.042 * H, 0.016 * H),
                       Box(palm - 0.09 * H, palm + 0.09 * H), mats["molten"], bones, protect=0.8),
             tree.leaf(S, "backplate_" + side, lambda P: sdf.box(P, palm - fore * 0.024 * H - x * 0.006 * H, V(0.042, 0.048, 0.009) * H, axes, 0.004 * H),
                       Box(palm - 0.08 * H, palm + 0.08 * H), mats["worn"], bones, protect=0.8)]
    root = palm + x * 0.056 * H
    for k, spread in enumerate((-1.5, -0.5, 0.5, 1.5)):
        base = root + out * spread * 0.027 * H
        d1 = unit(x + out * spread * 0.45 + fore * 0.2)
        d2 = unit(x + out * spread * 0.5 + fore * 0.7)
        length = (0.042 - abs(spread) * 0.005) * H
        mid = base + d1 * length
        tip = mid + d2 * length * 0.8
        # Blunt iron fingers, not claws.
        parts.append(tree.leaf(S, "finger_%s_%d" % (side, k), lambda P, a=base, b=mid, c=tip: np.minimum(sdf.round_cone(P, a, b, 0.016 * H, 0.014 * H),
                                                                                                    sdf.round_cone(P, b, c, 0.014 * H, 0.011 * H)),
                               Box.around([base, mid, tip], 0.017 * H), mats["worn"], bones, protect=0.8))
    # The thumb, out to the side of the open palm.
    base = palm + out * 0.046 * H - x * 0.012 * H
    mid = base + unit(out * 0.8 + x * 0.6 + fore * 0.3) * 0.042 * H
    tip = mid + unit(out * 0.3 + x * 0.8 + fore * 0.6) * 0.032 * H
    parts.append(tree.leaf(S, "thumb_" + side, lambda P: np.minimum(sdf.round_cone(P, base, mid, 0.017 * H, 0.014 * H), sdf.round_cone(P, mid, tip, 0.015 * H, 0.012 * H)),
                           Box.around([base, mid, tip], 0.019 * H), mats["worn"], bones, protect=0.8))
    return Union(parts, k=0.004 * H)


def foot(S, L, H, mats, side):
    """A heavy plated foot flat on the ground: a block from heel to ball and a toe cap, a glowing seam between them."""
    f0 = V(*L["foot_" + side][0])
    y = f0[1]
    bones = anatomy.rigid("foot_" + side)
    heel = V(f0[0] + 0.035 * H, y, 0.035 * H)
    toe = V(f0[0] + 0.14 * H, y, 0.022 * H)
    seam = V(f0[0] + 0.1 * H, y, 0.03 * H)
    parts = [tree.leaf(S, "foot_" + side, lambda P: sdf.box(P, heel, V(0.07, 0.052, 0.036) * H, None, 0.012 * H), Box(heel - 0.08 * H, heel + 0.08 * H), mats["iron"], bones, protect=0.4),
             tree.leaf(S, "toe_" + side, lambda P: sdf.box(P, toe, V(0.035, 0.05, 0.024) * H, None, 0.01 * H), Box(toe - 0.06 * H, toe + 0.06 * H), mats["worn"], bones, protect=0.4),
             tree.leaf(S, "toe_seam_" + side, lambda P: sdf.box(P, seam, V(0.012, 0.045, 0.026) * H, None, 0.006 * H), Box(seam - 0.06 * H, seam + 0.06 * H), mats["molten"], bones, protect=0.4)]
    return Union(parts)


def shards(S, L, H, mats, side):
    """Molten fragments torn from his plating, hanging about the open hand ready to be thrown (on its prop bone): jagged
    glowing slabs, the larger still dark plate on their outer face. None hangs between the hand and his leg."""
    w, x, out, fore = hand_frame(L, side)
    rng = np.random.default_rng(307 if side == "l" else 311)
    bones = anatomy.rigid("prop_" + side)
    centre = w + x * 0.09 * H
    parts = []
    # Big enough to read from the game camera, and each to keep its form through the reduction (a loose piece left a
    # speck is dropped).
    for k, (angle, height, size) in enumerate(((-50.0, -0.03, 1.0), (20.0, 0.05, 1.1), (85.0, -0.05, 0.95), (160.0, 0.02, 0.9))):
        a = np.radians(angle)
        spot = centre + (fore * np.cos(a) + out * np.sin(a)) * 0.15 * H + x * height * H
        # A random tumble for each, the larger face toward the outside.
        face = unit((fore * np.cos(a) + out * np.sin(a)) + rng.uniform(-0.6, 0.6, 3))
        axes = sdf.frame(face, unit(rng.uniform(-1, 1, 3)))
        half = V(0.05, 0.034, 0.013) * H * size
        cut1, cut2 = unit(rng.uniform(-1, 1, 3) + axes[:, 0]), unit(rng.uniform(-1, 1, 3) - axes[:, 1])

        def chunk(P, c=spot, axes=axes, half=half, n1=cut1, n2=cut2):
            box = sdf.box(P, c, half, axes, 0.003 * H)
            return np.maximum(box, np.maximum(sdf.half_space(P, c + n1 * half[0] * 0.55, n1), sdf.half_space(P, c + n2 * half[1] * 0.6, n2)))
        parts.append(tree.leaf(S, "shard_%s_%d" % (side, k), chunk, Box(spot - 0.08 * H, spot + 0.08 * H), mats["molten"], bones, protect=1.0))
        # Its plate face set off to one side and overhanging, so the piece is stepped (a plain chunk reduces to a speck).
        plate = spot + axes[:, 2] * half[2] * 1.1 + axes[:, 0] * half[0] * 0.3 + axes[:, 1] * half[1] * 0.2

        def outer(P, c=plate, axes=axes, half=half * V(0.85, 0.9, 0.55), n1=cut2, s0=plate, h0=half):
            box = sdf.box(P, c, half, axes, 0.002 * H)
            return np.maximum(box, sdf.half_space(P, s0 + n1 * h0[1] * 0.5, n1))
        parts.append(tree.leaf(S, "shard_plate_%s_%d" % (side, k), outer, Box(spot - 0.08 * H, spot + 0.08 * H), mats["iron"], bones, protect=1.0))
    return Union(parts)


def spiral(S, L, H, mats, core, trunk_a, trunk_b):
    """The core spiral set into his chest, a little to his left as the art sets it: a socket of dark plate in a rim of
    worn iron, a bright coil winding out across it from a white-hot heart. It stays lit when his shell cools."""
    bones = anatomy.rigid("spine_02")
    centre = V(*trunk_a) + (V(*trunk_b) - V(*trunk_a)) * 0.76 + V(0, 0.035 * H, 0)
    hit, normal = garments.surface_point(core, centre + V(0.6 * H, 0, 0), V(-1.0, 0, 0), reach=0.8 * H)
    # Turned a little up, as the top of the chest is, so the game camera above sees it whole.
    n = unit(normal * 0.4 + V(1.0, 0, 0.45) * 0.6)
    axes = sdf.frame(n, (0.0, 0.0, 1.0))
    up, across = axes[:, 0], axes[:, 1]
    outer = 0.092 * H
    face = hit + n * 0.026 * H
    box = Box(hit - 0.12 * H, hit + 0.12 * H)
    # The socket: a dark disc of plate standing a little proud of the chest, its rim of worn iron.
    parts = [tree.leaf(S, "socket", lambda P: sdf.cylinder(P, hit - n * 0.02 * H, face, outer - 0.008 * H, 0.004 * H), box, mats["socket"], bones, protect=0.8),
             tree.leaf(S, "socket_rim", lambda P: np.maximum(sdf.cylinder(P, hit - n * 0.02 * H, face + n * 0.008 * H, outer, 0.005 * H),
                                                             -sdf.cylinder(P, hit, face + n * 0.03 * H, outer - 0.012 * H, 0.0)), box, mats["worn"], bones, protect=0.8),
             tree.leaf(S, "heart", lambda P: sdf.ellipsoid(P, face, V(0.014, 0.019, 0.019) * H, np.stack([n, across, up], axis=1)), box, mats["heart"], bones, protect=1.0)]
    # The coil winding out from the heart: wide apart turns, so the reduction keeps them apart.
    turns = np.linspace(0.0, 1.0, 15)
    coil = [face + (up * np.cos(a) + across * np.sin(a)) * (0.018 + 0.06 * t) * H for t, a in zip(turns, turns * 2.0 * np.pi * 1.25)]
    parts.append(tree.leaf(S, "coil", lambda P: sdf.tube(P, coil, [0.012 * H - 0.003 * H * t for t in turns]), Box.around(coil, 0.02 * H), mats["core"], bones, protect=1.0))
    return Union(parts)


def rings(S, L, H, mats, core, belt_z):
    """Iron rings on the band at the front of his waist: a great one with a second hanging through it, a third at his
    left hip."""
    bones = anatomy.rigid("pelvis")
    parts = []
    for k, (y, drop, facing_side) in enumerate(((-0.05 * H, 0.03 * H, False), (-0.05 * H, 0.085 * H, True), (0.11 * H, 0.03 * H, False))):
        hit, normal = garments.surface_point(core, V(0.6 * H, y, belt_z), V(-1.0, 0, 0), reach=0.8 * H)
        c = V(hit[0] + 0.035 * H, y, belt_z - drop)
        if facing_side:
            axes = np.stack([V(1, 0, 0), V(0, 0, 1), V(0, 1, 0)], axis=1)
        else:
            axes = np.stack([V(0, 1, 0), V(0, 0, 1), V(1, 0, 0)], axis=1)
        parts.append(tree.leaf(S, "ring_%d" % k, lambda P, c=c, axes=axes: sdf.torus(P, c, 0.032 * H, 0.0065 * H, axes), Box(c - 0.045 * H, c + 0.045 * H),
                               mats["ring"], bones, protect=0.8))
    return Union(parts)


# How far down its chain a drape is held by the pelvis at the band, as a share of the chain's length.
DRAPE_HOLD = 0.15


def chain_bones(L, chain):
    """A drape panel's weights on its spring chain (ADR-069 §7): the strip at the band held by his pelvis, the cloth below
    weighted down the chain's two spans by how far below the chain's top it hangs (past its end, wholly the lower span),
    so it swings and trails as he moves."""
    top, end = V(*L[chain + "_01"][0]), V(*L[chain + "_end"][0])
    length = max(top[2] - end[2], 1e-6)

    def weights(P, u, v):
        below = np.clip((top[2] - P[:, 2]) / length, 0.0, 1.0)
        hold = np.clip(1.0 - below / DRAPE_HOLD, 0.0, 1.0)
        x = np.clip(below * 2.0, 0.5, 1.5)
        w = {"pelvis": hold}
        for k in range(2):
            w["%s_%02d" % (chain, k + 1)] = np.clip(1.0 - np.abs(x - (k + 0.5)), 0.0, 1.0) * (1.0 - hold)
        return {name: np.asarray(value, dtype=np.float32) for name, value in w.items()}
    return weights


def thigh_bones(L):
    """A drape panel's weights without a chain to hang on: its pelvis at the band and, lower down, each side the thigh it
    hangs over."""
    hip = abs(L["thigh_l"][0][1])

    def weights(P, u, v):
        hold = np.clip(1.0 - v / 0.12, 0.0, 1.0)
        follow = 0.7 * np.clip((v - 0.1) / 0.6, 0.0, 1.0) * (1.0 - hold)
        left = np.clip(0.5 + P[:, 1] / (1.2 * hip), 0.0, 1.0)
        w = {"pelvis": 1.0 - follow, "thigh_l": follow * left, "thigh_r": follow * (1.0 - left)}
        return {name: np.asarray(value, dtype=np.float32) for name, value in w.items()}
    return weights


def drape(S, L, H, mats, core, belt_z):
    """The heavy tattered drape at his waist, hung from the band before and behind him to below the knee, its hem torn
    into tongues, and a faded strip down its front. Cloth is dynamic: where his kit hangs the drape on its spring chains,
    the front panel and strip swing on the front chain and the back panel on the back one; without them, it rides his
    pelvis and thighs."""
    hip = abs(L["thigh_l"][0][1])
    limbs = [(L["thigh_" + s][0], L["thigh_" + s][1], 0.1 * H) for s in ("l", "r")] + [(L["calf_" + s][0], L["calf_" + s][1], 0.085 * H) for s in ("l", "r")]

    def surface(facing, ys):
        starts = np.array([[facing * 0.6 * H, y, belt_z - 0.02 * H] for y in ys])
        hits, _normals = garments.surface_points(core, starts, np.tile([-facing, 0.0, 0.0], (len(ys), 1)), reach=0.8 * H)
        return hits[:, 0]

    def panel(name, facing, width, hem, material, columns, rows, strips, cut, salt, inset=0.0):
        ys = np.linspace(-width, width, 9)
        xs = surface(facing, ys)
        phase = np.random.default_rng(int(salt)).uniform(0, 2 * np.pi, 2)
        chain = "drape_f" if facing > 0 else "drape_b"

        def position(u, v):
            y0 = (u - 0.5) * 2.0 * width
            y = y0 * (1.0 + 0.25 * v)
            x = np.interp(y0, ys, xs) + facing * (0.014 * H + inset + 0.05 * H * v ** 1.3)
            pleat = 0.012 * H * v * (0.6 * np.sin(u * 9.0 + phase[0]) + 0.4 * np.sin(u * 17.0 + phase[1]))
            z = belt_z - 0.015 * H - (belt_z - 0.015 * H - hem) * v
            P = np.stack([x + facing * pleat, y, z], axis=1)
            return sheet.clear_of(P, limbs, 1.5)
        bones = chain_bones(L, chain) if chain + "_01" in L else thigh_bones(L)
        return sheet.Sheet(name, position, material, bones, columns, rows, reach=lambda u: sheet.torn(u, strips, cut, 0.2, salt))

    return [panel("drape_front", 1.0, hip * 1.15, 0.12 * H, mats["drape"], 12, 6, 6, 0.6, 51.0),
            panel("drape_strip", 1.0, hip * 0.22, 0.2 * H, mats["drape_faded"], 2, 5, 1, 0.85, 53.0, inset=0.006 * H),
            panel("drape_back", -1.0, hip * 1.1, 0.16 * H, mats["drape_dark"], 12, 5, 6, 0.6, 57.0)]
