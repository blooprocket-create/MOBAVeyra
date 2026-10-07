"""Aurelisse, The Open Sky (ADR-069): an elemental of moving air and pale mineral dust, read by her silhouette from the
game camera. Character Bible §24 and her splash art: a tall, unmistakably nonhuman figure of translucent pale-blue air,
slender and waisted, currents of light running through her; a face only suggested, its eyes glimmering; streams of air
pouring back from her head like long white hair. A white shawl lies on her shoulders and a sash of white over teal
binds her waist, a brass disc at her hip. A brass ring stands behind her shoulders, hung with brass tube chimes and
small blue crystal pendants on fine chains, and more hang from her sash; crystals drift about her. Long white and teal
cloth streams away behind her in currents, so her silhouette is horizontal and soft-edged. Below the waist she is a
compact cyclone of spiralling air narrowing to a point that floats clear of the ground. Soft cloth and cool air, never
faceted glass in warm gold.

Low poly and flat-coloured (author 2026-10-07): the big forms that make her outline, each a flat colour the toon
material shades. She floats as her archetype lays her out (ADR-064), arms down at her sides. Her long cloth hangs on
the spring chains her kit gives her construct (ADR-069 §7): a streamer off each shoulder on its clavicle's chain, and the
ends of her sash on the pelvis's; the torn skirt about her cyclone rides the trail bones with it, which her clips sweep
back as she glides; her drifting crystals ride the orbit bones and the ring its halo bone.

Proportions are the kit's; colours are the kit's, its teal drawn toward the art's dusty teal (sRGB)."""
import math

import numpy as np

from ..sculpt import anatomy, garments, sdf, sheet, tree
from ..sculpt.anatomy import V, unit
from ..sculpt.tree import Box, Over, Shell, Union, Zone

# The ribbons' dusty teal sampled from the splash art (sRGB), which the kit's teal is drawn toward.
ART_TEAL = (0.29, 0.39, 0.44)
WHITE = (1.0, 1.0, 1.0)
TRAIL = ("trail_01", "trail_02", "trail_03")
# How deep the cyclone's spiralling flutes stand, as a share of its radius.
FLUTE = 0.24
ORBITS = ["orbit_0" + str(index) for index in range(1, 7)]


def mix(a, b, t):
    return tuple(float(x) * (1.0 - t) + float(y) * t for x, y in zip(a, b))


def materials(S, spec):
    """Every material she is coloured in, flat (the toon material shades it), read from her kit entry: her air, its
    lighter lower currents, the light running through her, her eyes, her hair of white air, her cloth, teal, brass and
    the crystals, which glow."""
    air = tuple(spec["primary"])
    cloth = tuple(spec.get("cloth", (0.95, 0.95, 0.92)))
    brass = tuple(spec["detail"])
    return {
        "air": S.material("air", air),
        "air_low": S.material("air_low", mix(air, cloth, 0.3)),
        "wisp": S.material("wisp", mix(air, cloth, 0.65)),
        "current": S.material("current", mix(air, WHITE, 0.6), glow=True),
        "eye": S.material("eye", mix(air, WHITE, 0.85), glow=True),
        "hair": S.material("hair", mix(cloth, air, 0.1)),
        "hair_shade": S.material("hair_shade", mix(cloth, air, 0.45)),
        "cloth": S.material("cloth", cloth),
        "teal": S.material("teal", mix(spec["secondary"], ART_TEAL, 0.3)),
        "brass": S.material("brass", brass),
        "brass_dark": S.material("brass_dark", mix(brass, (0.3, 0.2, 0.12), 0.4)),
        "crystal": S.material("crystal", tuple(spec["accent"]), glow=True),
    }


def smooth(x):
    x = np.clip(x, 0.0, 1.0)
    return x * x * (3.0 - 2.0 * x)


def normalised(weights):
    total = sum(weights.values())
    return {bone: (np.asarray(w) / np.maximum(total, 1e-6)).astype(np.float32) for bone, w in weights.items()}


def trail_shares(x):
    """Weights down the trail chain at x (0 at its head to 3 at its end): each bone over its own third, blending into
    the next across the joints."""
    x = np.clip(x, 0.5, 2.5)
    return {name: np.clip(1.0 - np.abs(x - (k + 0.5)), 0.0, 1.0) for k, name in enumerate(TRAIL)}


def chain_weights(L):
    """Weights for what rides the trail by where it lies along it: the pelvis above the trail's head, then down the
    trail bones as far along it as each point lies."""
    head, end = V(*L[TRAIL[0]][0]), V(*L[TRAIL[2]][1])
    axis = end - head
    length = float(np.linalg.norm(axis))

    def weights(P):
        along = (P - head) @ axis / length
        hold = 1.0 - smooth((along + 4.0) / 10.0)
        w = {name: share * (1.0 - hold) for name, share in trail_shares(along / length * 3.0).items()}
        w["pelvis"] = hold
        return normalised(w)
    return weights


def build(S, L, dims, spec):
    """Aurelisse's sculpt on her layout: (the whole, {"body": her body of air, "sheets": her streaming cloth})."""
    mats = materials(S, spec)
    rng = np.random.default_rng(spec["seed"])
    body = figure(S, L, dims, mats)
    lit = Over([body, currents(S, L, dims, mats, body)])
    dressed = Over([lit] + garb(S, L, dims, mats, lit))
    worn = Over([dressed, cyclone(S, L, dims, mats), ring(S, L, dims, mats), hangings(S, L, dims, mats),
                 drifting(S, L, dims, mats, rng)])
    sheets = streams(S, L, dims, mats)
    return worn, {"body": body, "sheets": sheets}


# ---------------------------------------------------------------------------------------------- her body of air
def figure(S, L, dims, mats):
    """A slender waisted body of air, broad at the shoulders, slight at the breast; slender arms ending in long open
    hands; a head with a face only suggested and glimmering eyes, streams of air pouring back from it like hair."""
    H = dims["height"]
    p = lambda bone, i: V(*L[bone][i])  # noqa: E731
    air = mats["air"]
    parts = []

    def add(name, distance, bounds, bones, material=air, protect=0.0):
        parts.append(tree.leaf(S, name, distance, bounds, material, bones, protect))

    core, chest = p("pelvis", 0), p("spine_03", 1)
    span = chest[2] - core[2]
    # Hips, a narrow waist, a slight breast and a broad upper chest, as sections up the spine (height share of the core
    # to the chest, forward offset, depth and width as shares of her height).
    stations = [(-0.17, 0.0, 0.048, 0.062), (0.0, 0.0, 0.056, 0.081), (0.17, 0.002, 0.05, 0.07), (0.3, 0.003, 0.045, 0.059),
                (0.47, 0.005, 0.048, 0.067), (0.64, 0.008, 0.053, 0.076), (0.81, 0.003, 0.05, 0.087), (0.94, -0.003, 0.042, 0.073),
                (1.07, 0.0, 0.028, 0.034)]
    z0, z1 = core[2] + stations[0][0] * span, core[2] + stations[-1][0] * span
    loft = sdf.Loft(V(0, 0, z0), V(0, 0, z1), V(1, 0, 0),
                    [((share * span + core[2] - z0) / (z1 - z0), ahead * H, 0.0, deep * H, wide * H) for share, ahead, deep, wide in stations], cap=H * 0.01)
    spine = anatomy.along("pelvis", "spine_03", core, chest, 0.3, 0.6)
    add("trunk", loft, Box.around(loft.bounds_points()), spine)
    for side, sign in (("l", 1.0), ("r", -1.0)):
        c = core + V(H * 0.034, sign * H * 0.032, span * 0.64)
        add("breast_" + side, lambda P, c=c: sdf.ellipsoid(P, c, V(0.024, 0.028, 0.026) * H), Box(c - H * 0.03, c + H * 0.03), anatomy.rigid("spine_03"))
    # The neck, slender and long.
    n0, n1 = chest - V(0, 0, H * 0.012), p("head", 0) + V(H * 0.004, 0, H * 0.022)
    add("neck", lambda P: sdf.round_cone(P, n0, n1, H * 0.026, H * 0.022), Box.around([n0, n1], H * 0.03), anatomy.along("spine_03", "head", n0, n1, 0.2, 0.8))
    for side, sign in (("l", 1.0), ("r", -1.0)):
        s0, s1 = p("upperarm_" + side, 0), p("upperarm_" + side, 1)
        w0, w1 = p("lowerarm_" + side, 1), p("hand_" + side, 1)
        # The shoulder's slope from the neck, and the round of the shoulder over the arm.
        a, b = chest + V(-H * 0.004, sign * H * 0.03, -H * 0.008), s0 + V(0, -sign * H * 0.012, H * 0.006)
        add("trapezius_" + side, lambda P, a=a, b=b: sdf.round_cone(P, a, b, H * 0.028, H * 0.027), Box.around([a, b], H * 0.03),
            anatomy.along("spine_03", "clavicle_" + side, a, b, 0.2, 0.8))
        d = s0 + V(0, sign * H * 0.002, -H * 0.006)
        add("deltoid_" + side, lambda P, d=d: sdf.ellipsoid(P, d, V(0.031, 0.029, 0.037) * H), Box(d - H * 0.04, d + H * 0.04),
            anatomy.along("clavicle_" + side, "upperarm_" + side, d + V(0, 0, H * 0.03), d - V(0, 0, H * 0.04), 0.2, 0.8))
        add("upperarm_" + side, lambda P, a=s0, b=s1: sdf.round_cone(P, a, b, H * 0.025, H * 0.019), Box.around([s0, s1], H * 0.03), anatomy.rigid("upperarm_" + side))
        add("forearm_" + side, lambda P, a=s1, b=w0: sdf.round_cone(P, a, b, H * 0.019, H * 0.0145), Box.around([s1, w0], H * 0.025), anatomy.rigid("lowerarm_" + side))
        hand(S, parts, side, w0, w1, H, air)
    parts += head(S, L, dims, mats)
    return Union(parts, k=H * 0.016)


def hand(S, parts, side, wrist, end, H, material):
    """A long open hand hanging from the wrist, palm in: a narrow palm, four long fingers a little apart and curled
    toward the palm, and a thumb before them."""
    sign = 1.0 if side == "l" else -1.0
    x = unit(end - wrist)
    out = V(0, sign, 0)
    z = unit(out - x * (out @ x))      # the back of the hand, facing out
    y = np.cross(x, z) * sign           # forward
    bones = anatomy.rigid("hand_" + side)
    palm = wrist + x * H * 0.024
    axes = np.stack([x, y, z], axis=1)
    parts.append(tree.leaf(S, "palm_" + side, lambda P: sdf.ellipsoid(P, palm, V(0.026, 0.02, 0.009) * H, axes), Box(palm - H * 0.035, palm + H * 0.035),
                           material, bones, protect=0.8))
    for k, across in enumerate((-0.75, -0.25, 0.25, 0.75)):
        base = palm + x * H * 0.02 + y * across * H * 0.0135
        length = H * (0.042 - 0.005 * abs(across) * 2 + (0.003 if k == 1 else 0.0))
        tip = base + unit(x + y * across * 0.18 - z * 0.3) * length
        parts.append(tree.leaf(S, "finger_%s_%d" % (side, k), lambda P, a=base, b=tip: sdf.round_cone(P, a, b, H * 0.0052, H * 0.0034),
                               Box.around([base, tip], H * 0.007), material, bones, protect=0.8))
    t0 = wrist + x * H * 0.016 + y * H * 0.016 - z * H * 0.003
    t1 = t0 + unit(x * 0.8 + y * 0.45 - z * 0.4) * H * 0.034
    parts.append(tree.leaf(S, "thumb_" + side, lambda P: sdf.round_cone(P, t0, t1, H * 0.0062, H * 0.0038), Box.around([t0, t1], H * 0.008),
                           material, bones, protect=0.8))


def head(S, L, dims, mats):
    """Her head in the head's frame: a long skull, a narrow jaw and pointed chin, a face only suggested (a soft brow and
    the line of a nose), two eyes glimmering in it; streams of air pour back from the crown and the back of it like long
    hair, fanning wider behind her."""
    H = dims["height"]
    o = V(*L["head"][0])
    h = L["head"][1][2] - L["head"][0][2]
    bones = anatomy.rigid("head")
    parts = []

    def add(name, distance, lo, hi, material, protect=0.6):
        parts.append(tree.leaf(S, name, distance, Box(lo, hi), material, bones, protect))

    skull_c, skull_r = o + V(-0.02, 0, 0.55) * h, V(0.43, 0.36, 0.47) * h
    add("skull", lambda P: sdf.ellipsoid(P, skull_c, skull_r), skull_c - 0.5 * h, skull_c + 0.5 * h, mats["air"])
    jaw_c = o + V(0.13, 0, 0.26) * h
    add("jaw", lambda P: sdf.ellipsoid(P, jaw_c, V(0.28, 0.24, 0.26) * h), jaw_c - 0.3 * h, jaw_c + 0.3 * h, mats["air"])
    chin = o + V(0.3, 0, 0.08) * h
    add("chin", lambda P: sdf.sphere(P, chin, 0.1 * h), chin - 0.12 * h, chin + 0.12 * h, mats["air"])
    brow = o + V(0.3, 0, 0.67) * h
    add("brow", lambda P: sdf.ellipsoid(P, brow, V(0.08, 0.17, 0.06) * h), brow - 0.3 * h, brow + 0.3 * h, mats["air"])
    n0, n1 = o + V(0.38, 0, 0.58) * h, o + V(0.46, 0, 0.42) * h
    add("nose", lambda P: sdf.round_cone(P, n0, n1, 0.035 * h, 0.05 * h), n0 - 0.08 * h, n1 + 0.08 * h, mats["air"])
    for sign in (1.0, -1.0):
        e = o + V(0.39, sign * 0.16, 0.52) * h
        # Long level almonds (gentle, never a scowl), set into the face and standing a little proud of it, so they catch
        # the eye from above.
        facing = unit(V(1, sign * 0.35, 0))
        outward = unit(V(-0.35, sign, -0.05))
        upward = unit(np.cross(facing, outward))
        outward = np.cross(upward, facing)
        axes = np.stack([facing, outward, upward], axis=1)
        add("eye_%d" % (sign > 0), lambda P, e=e, axes=axes: sdf.ellipsoid(P, e, V(0.04, 0.11, 0.05) * h, axes), e - 0.15 * h, e + 0.15 * h, mats["eye"], 1.0)
    # Her hair of white air: a cap swept back from a high hairline, and broad soft locks pouring back from it.
    cap_c, cap_r = skull_c + V(-0.04, 0, 0.04) * h, skull_r * 1.1
    line, line_n = o + V(0.3, 0, 0.8) * h, unit(V(0.75, 0, -0.66))
    add("hair_cap", lambda P: np.maximum(sdf.ellipsoid(P, cap_c, cap_r), (P - line) @ line_n), cap_c - 0.55 * h, cap_c + 0.55 * h, mats["hair"], 0.3)
    # The locks lie one over another rather than side by side, so at the height of her head they stay as narrow as it
    # (nothing stands from it like a horn or an ear); the lower ones fall behind her shoulders and only there fan wide.
    # Each: how far aside it ends and how far it falls (shares of her height), its length, width and depth, its wave's
    # phase, its colour and its root's height on the head. Of unequal lengths and blended into one soft mass, so from
    # above they never read as fingers.
    locks = [(0.0, 0.03, 0.44, 0.065, 0.024, 0.4, "hair", 0.22), (0.05, 0.13, 0.38, 0.055, 0.02, 1.7, "hair_shade", 0.18),
             (-0.05, 0.13, 0.36, 0.055, 0.02, 2.6, "hair", 0.18), (0.09, 0.2, 0.3, 0.045, 0.018, 0.9, "hair", 0.04),
             (-0.09, 0.2, 0.32, 0.045, 0.018, 2.2, "hair_shade", 0.04)]
    hair = []
    for index, (aside, fall, length, width, thick, phase, material, rise) in enumerate(locks):
        start = skull_c + V(-0.12, np.sign(aside) * 0.05, rise) * h
        end = start + unit(V(-1.0, 0, 0.12)) * length * H
        stations = []
        for t in (0.0, 0.15, 0.35, 0.55, 0.75, 0.9, 1.0):
            wave = H * 0.02 * math.sin(phase + t * 2.6 * math.pi) * t - H * fall * t * t
            stations.append((t, wave, aside * H * t ** 2.5 + H * 0.008 * math.sin(phase * 1.3 + t * 2.0 * math.pi) * t,
                             thick * H * (1.0 - 0.65 * t), width * H * (0.6 + 0.6 * math.sin(math.pi * min(1.0, t * 1.4))) * (1.0 - 0.55 * t)))
        shape = sdf.Loft(start, end, V(0, 0, 1), stations, cap=H * 0.006)
        box = Box.around(shape.bounds_points())
        hair.append(tree.leaf(S, "hair_%d" % index, shape, box, mats[material], bones, 0.2))
    parts.append(Union(hair, k=H * 0.02))
    return parts


def currents(S, L, dims, mats, body):
    """Currents of light running through her: two bright strands spiralling down her trunk from the shoulders to the
    waist, and one down each arm, lying in her surface."""
    H = dims["height"]
    core, chest = V(*L["pelvis"][0]), V(*L["spine_03"][1])
    width = H * 0.0035
    pitch = H * 0.24

    def strands(P):
        theta = np.arctan2(P[:, 1], P[:, 0] * 1.35)
        radius = np.linalg.norm(P[:, :2], axis=1)
        phase = 2.0 * theta + P[:, 2] / pitch * 2.0 * np.pi
        off = np.abs(np.mod(phase + np.pi, 2.0 * np.pi) - np.pi) / 2.0 * radius
        trunk = np.maximum(np.maximum(off - width, radius - H * 0.1), np.maximum(core[2] + H * 0.04 - P[:, 2], P[:, 2] - (chest[2] - H * 0.035)))
        out = trunk
        for side in ("l", "r"):
            a, b = V(*L["upperarm_" + side][0]), V(*L["lowerarm_" + side][1])
            ab = b - a
            t = np.clip((P - a) @ ab / (ab @ ab), 0.0, 1.0)
            rel = P - a - np.outer(t, ab)
            first = unit(np.cross(ab, V(1, 0, 0)))
            second = unit(np.cross(ab, first))
            angle = np.arctan2(rel @ second, rel @ first)
            arm_phase = angle - t * 2.4 * np.pi
            reach = np.linalg.norm(rel, axis=1)
            arm_off = np.abs(np.mod(arm_phase + np.pi, 2.0 * np.pi) - np.pi) * reach
            arm = np.maximum(np.maximum(arm_off - width, reach - H * 0.035), np.maximum(0.08 - t, t - 0.92))
            out = np.minimum(out, arm)
        return out
    region = Zone(strands, Box(V(-H * 0.3, -H * 0.35, core[2]), V(H * 0.3, H * 0.35, chest[2] + H * 0.05)))
    return Shell(S, "currents", body, 0.0, H * 0.0015, region, mats["current"], hem=H * 0.001)


# ---------------------------------------------------------------------------------------------- what she wears
def garb(S, L, dims, mats, body):
    """A white shawl on her shoulders, wound in a soft roll about her neck and swagged across her breast, a brass disc
    pinning it at her left; a sash of white wound over teal about her waist, a brass disc at her right hip; brass bangles
    at her wrists."""
    H = dims["height"]
    core, chest = V(*L["pelvis"][0]), V(*L["spine_03"][1])
    span = chest[2] - core[2]
    shoulder = abs(L["upperarm_l"][0][1])
    spine03 = anatomy.rigid("spine_03")
    torso_box = Box(V(-H * 0.2, -shoulder - H * 0.06, core[2] - H * 0.05), V(H * 0.2, shoulder + H * 0.06, chest[2] + H * 0.06))
    # The shawl over the tops of her shoulders, to the round of each arm.
    cape = Zone(lambda P: np.maximum(np.maximum((chest[2] - H * 0.045 + np.abs(P[:, 1]) * 0.08) - P[:, 2], np.abs(P[:, 1]) - (shoulder + H * 0.02)),
                                     P[:, 2] - (chest[2] + H * 0.035)), torso_box)
    shawl = Shell(S, "shawl", body, H * 0.002, H * 0.008, cape, mats["cloth"], hem=H * 0.004,
                  displace=garments.folds((0, 1, 0), 9, H * 0.004, seed=241), reach=H * 0.004)
    worn = [shawl]
    # The roll about her neck, and the swag across her breast from shoulder to shoulder.
    def ring_points(count, rx, ry, z, dip):
        return [V(rx * math.cos(a), ry * math.sin(a), z - dip * max(0.0, math.cos(a)) ** 2) for a in np.linspace(0, 2 * math.pi, count, endpoint=False)]
    roll = ring_points(12, H * 0.064, H * 0.09, chest[2] - H * 0.018, H * 0.028)
    roll = roll + roll[:1]
    swag = [V(H * 0.01, H * 0.1, chest[2] - H * 0.012), V(H * 0.052, H * 0.07, chest[2] - H * 0.05), V(H * 0.062, 0, chest[2] - H * 0.075),
            V(H * 0.052, -H * 0.07, chest[2] - H * 0.05), V(H * 0.01, -H * 0.1, chest[2] - H * 0.012)]
    pieces = [tree.leaf(S, "shawl_roll", lambda P: sdf.tube(P, roll, [H * 0.019] * len(roll)), Box.around(roll, H * 0.03), mats["cloth"], spine03),
              tree.leaf(S, "shawl_swag", lambda P: sdf.tube(P, swag, [H * 0.016, H * 0.019, H * 0.02, H * 0.019, H * 0.016]), Box.around(swag, H * 0.03),
                        mats["cloth"], spine03)]
    # The brooch pinning it, at the front of her left shoulder.
    pin = V(H * 0.055, H * 0.075, chest[2] - H * 0.02)
    facing = unit(V(1, 0.35, 0.4))
    pieces.append(tree.leaf(S, "brooch", lambda P: sdf.cylinder(P, pin - facing * H * 0.004, pin + facing * H * 0.004, H * 0.017, H * 0.003),
                            Box(pin - H * 0.025, pin + H * 0.025), mats["brass"], spine03, protect=0.6))
    worn.append(Union(pieces, k=H * 0.006))
    # The sash: white wound over teal, low about her hips.
    pelvis = anatomy.rigid("pelvis")
    white_lo, white_hi = core[2] + span * 0.09, core[2] + span * 0.23
    teal_lo = core[2] - span * 0.01
    # Kept to her trunk: her forearms hang beside it at the same height.
    trunk_only = Zone(lambda P: np.maximum(np.abs(P[:, 0]) - H * 0.2, np.abs(P[:, 1]) - H * 0.11), torso_box)
    worn.append(Shell(S, "sash_teal", body, H * 0.003, H * 0.007, garments.both(garments.band_z(teal_lo, white_lo + H * 0.004), trunk_only),
                      mats["teal"], hem=H * 0.003, displace=garments.folds((0, 0, 1), 7, H * 0.003, seed=242), reach=H * 0.003))
    worn.append(Shell(S, "sash_white", body, H * 0.006, H * 0.008, garments.both(garments.band_z(white_lo, white_hi), trunk_only),
                      mats["cloth"], hem=H * 0.004, displace=garments.folds((0, 0, 1), 8, H * 0.004, seed=243), reach=H * 0.004))
    # The brass disc at her right hip, on the sash, a blue crystal set in its face.
    disc, normal = hip_disc(L, dims)
    a, b = disc - normal * H * 0.006, disc + normal * H * 0.005
    face = disc + normal * H * 0.006
    worn.append(Union([tree.leaf(S, "hip_disc", lambda P: sdf.cylinder(P, a, b, H * 0.036, H * 0.004), Box(disc - H * 0.045, disc + H * 0.045), mats["brass"], pelvis, protect=0.6),
                       tree.leaf(S, "hip_disc_face", lambda P: sdf.cylinder(P, face - normal * H * 0.003, face + normal * H * 0.002, H * 0.014, H * 0.002),
                                 Box(face - H * 0.025, face + H * 0.025), mats["crystal"], pelvis, protect=0.6)], k=0.0))
    # Bangles: two brass rings at each wrist.
    bangles = []
    for side in ("l", "r"):
        e, w = V(*L["lowerarm_" + side][0]), V(*L["lowerarm_" + side][1])
        for k, (lo, hi) in enumerate(((0.74, 0.8), (0.84, 0.9))):
            a, b = e + (w - e) * lo, e + (w - e) * hi
            bangles.append(tree.leaf(S, "bangle_%s_%d" % (side, k), lambda P, a=a, b=b: sdf.cylinder(P, a, b, H * 0.0185, H * 0.003), Box.around([a, b], H * 0.025),
                                     mats["brass"], anatomy.rigid("lowerarm_" + side), protect=0.5))
    worn.append(Union(bangles, k=0.0))
    return worn


def hip_disc(L, dims):
    """Where the brass disc at her right hip sits on her sash, and the way it faces."""
    H = dims["height"]
    core, chest = V(*L["pelvis"][0]), V(*L["spine_03"][1])
    z = core[2] + (chest[2] - core[2]) * 0.14
    angle = math.radians(-58)
    depth, width = H * 0.054, H * 0.077
    point = V(depth * math.cos(angle), width * math.sin(angle), z)
    normal = unit(V(math.cos(angle) / depth, math.sin(angle) / width, 0.15 / H))
    return point + normal * H * 0.016, normal


# ---------------------------------------------------------------------------------------------- the cyclone
def cyclone_shape(L, dims):
    """Her lower body: a column of air from under the hips down the trail to a point at the hover height, its three
    flutes spiralling round it twice as it narrows. (distance, top, tip, radius at a share of its length, frame)."""
    H = dims["height"]
    top = V(*L[TRAIL[0]][0]) + V(0, 0, H * 0.03)
    tip = V(*L[TRAIL[2]][1])
    axis = tip - top
    length = float(np.linalg.norm(axis))
    w = axis / length
    e1 = unit(V(1, 0, 0) - w * w[0])
    e2 = np.cross(w, e1)
    shares = np.array([0.0, 0.2, 0.45, 0.7, 0.88, 1.0])
    radii = np.array([0.074, 0.068, 0.052, 0.032, 0.015, 0.004]) * H

    def radius(t):
        return np.interp(t, shares, radii)

    def squash(t):
        # Flattened front to back under the hips, round below.
        return 0.85 + 0.15 * np.clip(t / 0.4, 0.0, 1.0)

    def distance(P):
        Q = P - top
        s = Q @ w
        t = np.clip(s / length, 0.0, 1.0)
        x, y = (Q @ e1) / squash(t), Q @ e2
        rho = np.sqrt(x * x + y * y)
        theta = np.arctan2(y, x)
        flute = radius(t) * (1.0 + FLUTE * np.sin(3.0 * theta - 4.0 * np.pi * t))
        return np.maximum((rho - flute) * 0.7, np.maximum(-s, s - length))
    return distance, top, tip, radius, squash, (w, e1, e2), length


def cyclone(S, L, dims, mats):
    """The cyclone below her waist, its flutes' crests lit by currents spiralling down them; it rides the trail."""
    H = dims["height"]
    distance, top, tip, radius, squash, (w, e1, e2), length = cyclone_shape(L, dims)
    bones = chain_weights(L)
    lo, hi = np.minimum(top, tip) - V(H * 0.09, H * 0.09, H * 0.02), np.maximum(top, tip) + V(H * 0.09, H * 0.09, H * 0.02)
    parts = [tree.leaf(S, "cyclone", distance, Box(lo, hi), mats["air_low"], bones)]
    for strand in range(3):
        points = []
        for t in np.linspace(0.06, 0.9, 11):
            theta = (np.pi / 2.0 + 4.0 * np.pi * t + 2.0 * np.pi * strand) / 3.0
            r = radius(t) * (1.0 + FLUTE) + H * 0.002
            points.append(top + w * length * t + e1 * math.cos(theta) * r * squash(t) + e2 * math.sin(theta) * r)
        parts.append(tree.leaf(S, "cyclone_current_%d" % strand, lambda P, points=points: sdf.tube(P, points, [H * 0.0045] * len(points)),
                               Box.around(points, H * 0.01), mats["current"], bones, protect=0.4))
    # Wisps of wind wound about it, standing clear of it, tapering as they spiral down.
    wisps = []
    for wisp, (start, end, turns, phase) in enumerate(((0.24, 0.66, 1.1, 0.4), (0.42, 0.88, 1.1, 3.3))):
        points, radii = [], []
        for t in np.linspace(start, end, 12):
            f = (t - start) / (end - start)
            theta = phase + f * turns * 2.0 * np.pi
            r = radius(t) * (1.0 + FLUTE) + H * (0.03 - 0.012 * f)
            points.append(top + w * length * t + e1 * math.cos(theta) * r * squash(t) + e2 * math.sin(theta) * r)
            radii.append(H * (0.007 - 0.004 * f) * math.sin(math.pi * min(1.0, 0.15 + f * 0.85)) + H * 0.0035)
        wisps.append(tree.leaf(S, "wisp_%d" % wisp, lambda P, points=points, radii=radii: sdf.tube(P, points, radii), Box.around(points, H * 0.012),
                               mats["wisp"], bones, protect=0.3))
    return Union(parts + wisps, k=H * 0.004)


# ---------------------------------------------------------------------------------------------- brass and crystal
def bipyramid(P, centre, axis, top, bottom, radius, sides=6):
    """A cut crystal: a six-sided girdle of radius (its inradius) at centre, coming to a point top above it and bottom
    below it along axis."""
    axes = sdf.frame(axis)
    Q = sdf.local(P, centre, axes)
    d = None
    for k in range(sides):
        a = 2.0 * math.pi * k / sides
        for height, sign in ((top, 1.0), (bottom, -1.0)):
            n = np.array([height * math.cos(a), height * math.sin(a), sign * radius])
            norm = float(np.linalg.norm(n))
            facet = Q @ (n / norm).astype(np.float32) - radius * height / norm
            d = facet if d is None else np.maximum(d, facet)
    return d


def chime(S, name, top, drop, length, radius, mats, bones, H, swing=None):
    """A brass tube chime hung from top on a fine chain drop long (swung out by swing): a capped tube length long."""
    cap = top - V(0, 0, drop) + (swing if swing is not None else 0.0)
    tube_top = cap - V(0, 0, H * 0.006)
    end = tube_top - V(0, 0, length)
    return [tree.leaf(S, name + "_chain", lambda P: sdf.capsule(P, top, cap, H * 0.0026), Box.around([top, cap], H * 0.004), mats["brass_dark"], bones, protect=0.7),
            tree.leaf(S, name + "_cap", lambda P: sdf.cylinder(P, tube_top, cap + V(0, 0, H * 0.001), radius * 1.3, H * 0.002),
                      Box.around([cap, tube_top], radius * 1.5), mats["brass_dark"], bones, protect=0.7),
            tree.leaf(S, name, lambda P: sdf.cylinder(P, end, tube_top, radius, H * 0.0015), Box.around([end, tube_top], radius * 1.2), mats["brass"], bones, protect=0.7)]


def pendant(S, name, top, drop, size, mats, bones, H, swing=None):
    """A small blue crystal pendant hung from top on a fine chain drop long (swung out by swing)."""
    end = top - V(0, 0, drop) + (swing if swing is not None else 0.0)
    centre = end - V(0, 0, size * 0.35)
    return [tree.leaf(S, name + "_chain", lambda P: sdf.capsule(P, top, end, H * 0.0026), Box.around([top, end], H * 0.004), mats["brass_dark"], bones, protect=0.7),
            tree.leaf(S, name, lambda P: bipyramid(P, centre, V(0, 0, 1), size * 0.35, size * 0.75, size * 0.22), Box(centre - size, centre + size),
                      mats["crystal"], bones, protect=0.9)]


def ring_frame(L, dims):
    """The brass ring behind her shoulders: its centre, radius, and its plane's across and up (tilted back)."""
    H = dims["height"]
    chest = V(*L["spine_03"][1])
    centre = V(-H * 0.12, 0, chest[2] + H * 0.07)
    tilt = math.radians(12)
    up = V(-math.sin(tilt), 0, math.cos(tilt))
    return centre, H * 0.175, V(0, 1, 0), up


def ring(S, L, dims, mats):
    """A brass ring standing behind her shoulders, tilted back, beads on its upper arc; it is hung with chimes and
    crystal pendants on fine chains along its lower arc, inside the streamers flowing past its sides. Her hair passes
    through it."""
    H = dims["height"]
    centre, radius, across, up = ring_frame(L, dims)
    normal = np.cross(across, up)
    axes = np.stack([across, up, normal], axis=1)
    bones = anatomy.rigid("halo")
    box = Box(centre - radius - H * 0.02, centre + radius + H * 0.02)
    parts = [tree.leaf(S, "ring", lambda P: sdf.torus(P, centre, radius, H * 0.0085, axes), box, mats["brass"], bones, protect=0.4)]
    at = lambda degrees, share=1.0: centre + (across * math.cos(math.radians(degrees)) + up * math.sin(math.radians(degrees))) * radius * share  # noqa: E731
    for k, degrees in enumerate((30, 60, 90, 120, 150)):
        b = at(degrees)
        parts.append(tree.leaf(S, "ring_bead_%d" % k, lambda P, b=b: sdf.sphere(P, b, H * 0.0125), Box(b - H * 0.015, b + H * 0.015), mats["brass"], bones, protect=0.4))
    for k, (degrees, kind, drop, size) in enumerate(((205, "chime", 0.02, 0.07), (225, "chime", 0.035, 0.085), (250, "pendant", 0.015, 0.06),
                                                    (290, "pendant", 0.03, 0.06), (315, "chime", 0.02, 0.085), (335, "chime", 0.035, 0.07))):
        top = at(degrees) - V(0, 0, H * 0.008)
        if kind == "chime":
            parts += chime(S, "ring_chime_%d" % k, top, drop * H, size * H, H * 0.0075, mats, bones, H)
        else:
            parts += pendant(S, "ring_pendant_%d" % k, top, drop * H, size * H, mats, bones, H)
    return Union(parts, k=H * 0.003)


def hangings(S, L, dims, mats):
    """Chimes and crystal pendants hung on fine chains from her sash, about her front and sides (her skirt hangs behind),
    and two from the disc at her hip."""
    H = dims["height"]
    core, chest = V(*L["pelvis"][0]), V(*L["spine_03"][1])
    z = core[2] + (chest[2] - core[2]) * 0.06
    bones = anatomy.rigid("pelvis")
    depth, width = H * 0.066, H * 0.092
    parts = []
    for k, (degrees, kind, drop, size) in enumerate(((-84, "pendant", 0.02, 0.06), (-28, "chime", 0.012, 0.075), (-4, "pendant", 0.035, 0.055),
                                                     (22, "chime", 0.02, 0.085), (50, "pendant", 0.012, 0.06), (80, "chime", 0.03, 0.07))):
        a = math.radians(degrees)
        out = V(math.cos(a), math.sin(a), 0)
        top = V(depth * math.cos(a), width * math.sin(a), z)
        if kind == "chime":
            parts += chime(S, "sash_chime_%d" % k, top, drop * H, size * H, H * 0.0075, mats, bones, H, swing=out * H * 0.012)
        else:
            parts += pendant(S, "sash_pendant_%d" % k, top, drop * H, size * H, mats, bones, H, swing=out * H * 0.012)
    disc, normal = hip_disc(L, dims)
    for k, offset in enumerate((-1.0, 1.0)):
        side = unit(np.cross(V(0, 0, 1), normal))
        top = disc + side * offset * H * 0.02 - V(0, 0, H * 0.03) + normal * H * 0.004
        parts += chime(S, "disc_chime_%d" % k, top, H * (0.01 + 0.02 * k), H * (0.09 - 0.015 * k), H * 0.008, mats, bones, H)
    return Union(parts, k=H * 0.002)


def drifting(S, L, dims, mats, rng):
    """Crystals drifting about her, one at each orbit's end, each turned a little its own way."""
    H = dims["height"]
    parts = []
    for name in ORBITS:
        c = V(*L[name][1])
        axis = unit(V(rng.uniform(-0.3, 0.3), rng.uniform(-0.3, 0.3), 1.0))
        size = H * rng.uniform(0.08, 0.095)
        parts.append(tree.leaf(S, "crystal_" + name, lambda P, c=c, axis=axis, size=size: bipyramid(P, c, axis, size * 0.45, size * 0.6, size * 0.2),
                               Box(c - size, c + size), mats["crystal"], anatomy.rigid(name), protect=0.9))
    return Union(parts, k=0.0)


# ---------------------------------------------------------------------------------------------- streaming cloth
def ribbon(S, name, colour, root, heading, length, width, phase, sway, lift, droop, twist, bones, rows, taper=0.45, fork=0.1):
    """A short ribbon of cloth streaming away from root along heading, lying flat to the sky at its root and turning as
    it goes, waving from side to side and up and down more the further it runs, narrowing to a forked end."""
    material = S.material(name, colour)
    heading = unit(heading)
    side = unit(np.cross(V(0, 0, 1), heading))
    up = np.cross(heading, side)

    def position(u, v):
        f = v[:, None]
        centre = (root + heading * length * f + side * (np.sin(phase + v * 2.4 * np.pi) * sway * v)[:, None]
                  + up * (np.sin(phase * 0.7 + v * 2.0 * np.pi) * lift * v - droop * v ** 1.6)[:, None])
        turn = (twist * v)[:, None]
        across = side * np.cos(turn) + up * np.sin(turn)
        return centre + across * ((u - 0.5) * width * (1.0 - taper * v))[:, None]
    return sheet.Sheet(name, position, material, bones, 2, rows, reach=lambda u: 1.0 - fork * (1.0 - np.abs(2.0 * u - 1.0)))


def chain_ribbon(S, name, colour, attach, joints, holder, chain, width, twist, phase, sway, lift, rows, past=0.2, offset=None, root_share=0.7,
                 taper=0.4, fork=0.1):
    """A long ribbon of cloth laid along a spring chain (ADR-069 §7): from attach (where it leaves her) to the chain's
    first joint, down its joints, and on past its last by past of the chain's length; flat across its run at its root
    and turning by twist as it goes, waving a little to the side and up more the further it runs (offset lays it a
    little beside the chain), narrowing to a forked end. Weighted as silt.py's flung ribbons are: its root held by
    holder, then each span of chain (its bones, one per span) where the cloth lies along it, the run past its end on the
    last."""
    material = S.material(name, colour)
    joints = [np.asarray(j, dtype=np.float64) for j in joints]
    path = np.array([attach] + joints + [joints[-1] + (joints[-1] - joints[-2]) / np.linalg.norm(joints[-1] - joints[-2])
                                          * past * sum(np.linalg.norm(b - a) for a, b in zip(joints, joints[1:]))])
    lengths = np.linalg.norm(np.diff(path, axis=0), axis=1)
    run = np.concatenate([[0.0], np.cumsum(lengths)])
    total, first, last = run[-1], run[1], run[-2]
    shift = np.zeros(3) if offset is None else np.asarray(offset, dtype=np.float64)

    def point(s):
        k = np.clip(np.searchsorted(run, s, side="right") - 1, 0, len(path) - 2)
        f = ((s - run[k]) / lengths[k])[:, None]
        return path[k] + (path[k + 1] - path[k]) * f

    def position(u, v):
        s = v * total
        centre = point(s)
        # Its run's way at each point, eased across the joints.
        ahead = point(np.minimum(s + total * 0.04, total)) - point(np.maximum(s - total * 0.04, 0.0))
        ahead /= np.linalg.norm(ahead, axis=1, keepdims=True)
        side = np.cross(np.array([0.0, 0.0, 1.0]), ahead)
        side /= np.maximum(np.linalg.norm(side, axis=1, keepdims=True), 1e-6)
        up = np.cross(ahead, side)
        wave = side * (np.sin(phase + v * 2.4 * np.pi) * sway * v)[:, None] + up * (np.sin(phase * 0.7 + v * 2.0 * np.pi) * lift * v)[:, None]
        turn = (twist * v)[:, None]
        across = side * np.cos(turn) + up * np.sin(turn)
        broad = width * (root_share + (1.0 - root_share) * smooth(v / 0.3)) * (1.0 - taper * v)
        return centre + shift + wave + across * ((u - 0.5) * broad)[:, None]

    spans = len(joints) - 1

    def bones(P, u, v):
        s = v * total
        hold = np.clip(1.0 - (s - first) / (0.12 * (last - first)), 0.0, 1.0)
        x = np.clip((s - first) / (last - first) * spans, 0.5, spans - 0.5)
        w = {chain[k]: np.clip(1.0 - np.abs(x - (k + 0.5)), 0.0, 1.0) * (1.0 - hold) for k in range(spans)}
        w[holder] = hold
        return normalised(w)
    return sheet.Sheet(name, position, material, bones, 2, rows, reach=lambda u: 1.0 - fork * (1.0 - np.abs(2.0 * u - 1.0)))


def streams(S, L, dims, mats):
    """Her streaming cloth, every long piece on the spring chains her kit gives her (ADR-069 §7), so it keeps moving
    after she stops: a broad white streamer and a narrower teal one beneath it off the back of each shoulder, flowing
    back and out past the ring on her shoulder's streamer chain; the ends of her sash, teal on her left and white on her
    right, falling back from behind her waist on its sash chains. Short ribbons stream from her wrists on her forearms,
    and a torn white skirt hangs about the top of her cyclone, riding the trail as the cyclone does."""
    H = dims["height"]
    core, chest = V(*L["pelvis"][0]), V(*L["spine_03"][1])
    span = chest[2] - core[2]
    cloth, teal = mats["cloth"].colour, mats["teal"].colour
    sheets = []
    for side, sign in (("l", 1.0), ("r", -1.0)):
        streamer = ["streamer_%s_%02d" % (side, i) for i in (1, 2, 3)]
        if streamer[0] in L:
            joints = [L[bone][0] for bone in streamer] + [L["streamer_%s_end" % side][0]]
            # Leaving from the shawl at the back of the shoulder, the teal a hand's breadth under the white, both turning alike.
            attach = V(*joints[0]) + V(H * 0.025, -sign * H * 0.01, H * 0.004)
            sheets.append(chain_ribbon(S, "streamer_white_" + side, cloth, attach, joints, "clavicle_" + side, streamer, H * 0.09, sign * 0.7,
                                       0.6 * sign, H * 0.03, H * 0.03, 12, past=0.25))
            sheets.append(chain_ribbon(S, "streamer_teal_" + side, teal, attach, joints, "clavicle_" + side, streamer, H * 0.055,
                                       sign * 0.7, 0.6 * sign, H * 0.03, H * 0.03, 12, past=0.1, offset=V(0, 0, -H * 0.035)))
        sash = ["sash_%s_01" % side, "sash_%s_02" % side]
        if sash[0] in L:
            joints = [L[bone][0] for bone in sash] + [L["sash_%s_end" % side][0]]
            # Leaving from the knot at the back of the sash, falling back, out and down.
            attach = V(-H * 0.06, sign * H * 0.035, core[2] + span * 0.1)
            sheets.append(chain_ribbon(S, "sash_end_" + side, teal if side == "l" else cloth, attach, joints, "pelvis", sash, H * 0.075, sign * 0.5,
                                       2.4 * sign, H * 0.025, H * 0.02, 12, past=0.25, root_share=0.8))
        # From the wrist, short, back and out.
        e, w = V(*L["lowerarm_" + side][0]), V(*L["lowerarm_" + side][1])
        root = e + (w - e) * 0.82 - V(H * 0.012, 0, 0)
        forearm = anatomy.rigid("lowerarm_" + side)
        sheets.append(ribbon(S, "wrist_" + side, teal if side == "l" else cloth, root, V(-1.0, sign * 0.55, -0.35), H * 0.24, H * 0.034, 0.8, H * 0.03,
                             H * 0.02, H * 0.0, sign * 0.8, lambda P, u, v, forearm=forearm: forearm(P), 6))
    sheets.append(skirt(S, L, dims, mats))
    return sheets


def skirt(S, L, dims, mats):
    """Torn white cloth hanging from her sash about the top of her cyclone, round her sides and back (open at the
    front), flaring and streaming back as it falls, its hem torn into long tongues. It rides the trail as the cyclone
    beside it does."""
    H = dims["height"]
    core, chest = V(*L["pelvis"][0]), V(*L["spine_03"][1])
    top_z = core[2] + (chest[2] - core[2]) * 0.06
    material = S.material("skirt_white", mats["cloth"].colour)
    columns, rows, strips = 14, 6, 7
    t0, t1 = math.radians(95.0), math.radians(265.0)
    rx, ry = H * 0.062, H * 0.086
    drop = H * 0.24
    phase = np.random.default_rng(244).uniform(0, 2 * np.pi, 2)

    def position(u, v):
        theta = t0 + (t1 - t0) * u
        radial = np.stack([np.cos(theta), np.sin(theta), np.zeros_like(theta)], axis=1)
        top = np.stack([rx * np.cos(theta), ry * np.sin(theta), np.full_like(theta, top_z)], axis=1)
        flare = H * 0.06 * v ** 1.1
        pleat = H * (0.002 + 0.008 * v) * (0.6 * np.sin(theta * 8.0 + phase[0]) + 0.4 * np.sin(theta * 13.0 + phase[1]))
        p = top + radial * (flare + pleat)[:, None]
        p[:, 0] -= H * 0.12 * v ** 1.5
        p[:, 2] = top_z - drop * v
        return p
    chain = chain_weights(L)

    def bones(P, u, v):
        hold = 1.0 - smooth(v / 0.12)
        w = {bone: x * (1.0 - hold) for bone, x in chain(P).items()}
        w["pelvis"] = w.get("pelvis", 0.0) + hold
        return normalised(w)
    return sheet.Sheet("skirt", position, material, bones, columns, rows, reach=lambda u: sheet.torn(u, strips, 0.55, 0.2, 24.0))
