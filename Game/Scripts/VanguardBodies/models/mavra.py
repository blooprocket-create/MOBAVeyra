"""Mavra, The Spillwright (ADR-069): a salvage chemist who fixes hazardous sites for a living, read by her silhouette
from the game camera. Character Bible §17 and her splash art: lean and long-legged; dark red-brown hair pushed back
under a red band, loose strands falling round her face, work goggles pushed up on her forehead; a heavy layered work
coat over a black top and a bare midriff, open at the front, its lapels and high collar faced in its red lining, its
long skirt torn at the hem and painted there in red-and-white hazard stripes, stained and scorched from the work; belts
of canisters and sealed bottles crossing her body, two large reagent canisters at her right hip; shorts, a dark stocking
and thigh straps, gloves and heavy boots. On her back the pressurised dispenser rig: a banded steel cylinder with a glass
window onto the glowing orange reagent, a lozenge hazard mark on the glass, a pump housing on top under a yellow-and-black
placard, and armoured hoses over her right shoulder to the spray lance in her right hand, one of them lit from within.
Industrial, never arcane: valves, straps, gauges and fittings.

Low poly and flat-coloured (author 2026-10-07): the big forms that make her outline, each a flat colour the toon
material shades. She rests in the A pose, the lance carried low in her right fist, pointing ahead; her clips raise it.
Her coat's skirt hangs on the coat's spring chains and her loose hair on the hair's (ADR-069 §7). Colours are her kit's,
lifted toward her art so they read under the toon light; the art is lit for night."""
import numpy as np

from ..sculpt import anatomy, garments, hair, paint, sdf, sheet, tree
from ..sculpt.anatomy import V, unit
from ..sculpt.garments import around, band_z, both, either, folds, without
from ..sculpt.tree import Box, Over, Placed, Shell, Union, Zone

# The hand, wrist to fingertip, as a share of the height.
HAND_SHARE = 0.1
# The skull her hair grows over, in the head's frame at a 24 cm template's scale, and how far beyond it a lock has
# passed wholly to its hair chain (cm).
SKULL_CENTRE = (-1.2, 0.0, 14.4)
SKULL_RADII = (9.9, 7.5, 8.0)
HAIR_REACH = 3.0
# A lean woman's figure, long in the leg.
LOOK = {"muscle": 0.35, "chest": 0.86, "breadth": 0.82, "hips": 1.12, "limb": 0.78, "deltoid": 0.72, "leg": 0.84, "neck": 0.8, "bust": 0.85}

PALETTE = {
    "skin": (0.84, 0.65, 0.53), "hair": (0.42, 0.18, 0.12), "band": (0.74, 0.13, 0.1), "coat": (0.3, 0.27, 0.27),
    "lining": (0.7, 0.12, 0.1), "hazard": (0.92, 0.9, 0.86), "top": (0.2, 0.19, 0.2), "shorts": (0.21, 0.2, 0.22),
    "trousers": (0.28, 0.25, 0.25), "leather": (0.46, 0.29, 0.19), "boot": (0.25, 0.21, 0.19), "sole": (0.16, 0.14, 0.13),
    "glove": (0.29, 0.23, 0.18), "steel": (0.62, 0.62, 0.64), "dark_steel": (0.3, 0.3, 0.32), "brass": (0.74, 0.58, 0.32),
    "glass": (0.55, 0.72, 0.74), "placard": (0.95, 0.8, 0.12), "placard_black": (0.12, 0.12, 0.13), "hose": (0.22, 0.22, 0.24),
    "gauge": (0.92, 0.9, 0.84), "label_white": (0.9, 0.9, 0.86), "scorch": (0.17, 0.14, 0.13), "stain": (0.5, 0.41, 0.24),
}


def materials(S, spec):
    """Every material Mavra is coloured in, flat (the toon material shades it); the reagent (her kit's accent) glows."""
    mats = {name: S.material(name, colour) for name, colour in PALETTE.items()}
    mats["reagent"] = S.material("reagent", tuple(spec["accent"]), glow=True)
    return mats


def build(S, L, dims, spec):
    """Mavra's sculpt on her layout: (the whole, {"body": the skin alone, under her clothes, "sheets": her coat's skirt})."""
    mats = materials(S, spec)
    H = dims["height"]
    figure = anatomy.Figure(S, L, dims, mats["skin"], LOOK).build()
    head_origin = V(*L["head"][0])
    size = (L["head"][1][2] - L["head"][0][2]) * 24.0 / 21.9
    u = size / 24.0
    head = anatomy.Head(S, mats["skin"], size, look={"jaw": 0.86}).build()
    figure.attach_head(Placed(Over([head, headgear(S, L, mats, spec, head_origin, u)]), head_origin, np.eye(3)))
    grips = {}
    for side in ("l", "r"):
        figure.limbs["hand_" + side], grips[side] = hand(S, L, H, side, mats, figure)
    body = figure.body()
    coated = clothes(S, L, dims, mats, body, figure.limbs)
    worn = Over([gear(S, L, dims, mats, coated, figure.limbs), collar(S, L, dims, mats), rig(S, L, dims, mats, grips["r"])])
    return worn, {"body": body, "sheets": coat_skirt(S, L, dims, mats, coated)}


# ---------------------------------------------------------------------------------------------- head
def headgear(S, L, mats, spec, origin, u):
    """In the head's frame: her hair pushed back under the red band and heaped messily behind it, loose strands falling
    round her face to the jaw; the band at her hairline; the work goggles pushed up over it on the front of her head, on
    a strap round it. Her loose locks sway on the hair's chains where her kit gives them (ADR-069 §7)."""
    c, r = V(*SKULL_CENTRE) * u, V(*SKULL_RADII) * u
    head_bones = anatomy.rigid("head")
    locks_bones = head_bones
    chains = sorted(bone[:-3] for bone in L if bone.startswith("hair_") and bone.endswith("_01"))
    if chains:
        joints = {chain: [L["%s_%s" % (chain, joint)][0] for joint in ("01", "02", "end")] for chain in chains}
        locks_bones = hair.on_chains(joints, "head", origin + c, r, HAIR_REACH)
    # Pushed back: the locks flow back from the band, full over the crown, into a messy heap behind it.
    locks = hair.messy(S, mats["hair"], locks_bones, SKULL_CENTRE, SKULL_RADII, (17.0, 8.0), seed=spec["seed"], unit_scale=u, count=28,
                       length=(5.0, 7.5), radius=(1.9, 2.6), wind=(-0.7, 0.0, -0.05), fringe=0.3, volume=0.45, cap_bones=head_bones)
    rng = np.random.default_rng(spec["seed"])
    # The heap: a rounded mass gathered high at the back of the crown, a few short locks spilling from it.
    knot = V(-7.5, 0.0, 20.0) * u
    heap_radii = V(5.6, 6.2, 4.6) * u
    strands = [tree.leaf(S, "hair_heap", lambda P: sdf.ellipsoid(P, knot, heap_radii, sdf.rotation(pitch=-20.0)), Box(knot - heap_radii.max() - 1, knot + heap_radii.max() + 1),
                         mats["hair"], head_bones)]
    for i, (dy, dz) in enumerate(((0.6, 0.1), (-0.6, 0.1), (0.25, -0.4), (-0.25, -0.4))):
        strands.append(hair.clump(S, "hair_heap_%d" % i, knot + V(-2.0, dy * 3.5, dz * 2.5) * u, V(-1.0, dy, dz - 0.3), unit(V(-0.4, dy, 1.0)), rng.uniform(5.0, 6.0) * u,
                                  rng.uniform(2.2, 2.6) * u, 0.4, 0.4, mats["hair"], locks_bones))
    # Loose strands from the temples, in front of the ears to the jaw, and two down the nape: kept dense through the
    # reduction, so their tips keep their chains' weights through the skin's smoothing and sway.
    for sign in (1.0, -1.0):
        side = "l" if sign > 0 else "r"
        for i, (x, z, length) in enumerate(((4.5, 15.5, 11.0), (1.5, 15.0, 12.5))):
            strands.append(loose_lock(S, "hair_strand_%s_%d" % (side, i), V(x, sign * 6.9, z) * u, V(0.12, sign * 0.3, -1.0), V(0.2, sign, 0.0), length * u,
                                      1.5 * u, 0.1, 0.3, mats["hair"], locks_bones))
        strands.append(loose_lock(S, "hair_nape_" + side, V(-9.0, sign * 3.5, 11.0) * u, V(-0.3, sign * 0.2, -1.0), V(-1.0, 0.0, 0.0), 10.0 * u, 1.7 * u, 0.1, 0.2,
                                  mats["hair"], locks_bones))
    hairdo = Union([locks] + strands, k=0.7 * u)
    # The band: a slice of a shell round the skull, from the hairline at the brow down behind the ears.
    band = ring_on_skull(S, "headband", c, r + 2.9 * u, V(8.4, 0, 16.4) * u, V(-11.2, 0, 11.4) * u, 1.5 * u, mats["band"], head_bones)
    # The goggles' strap, higher round the head, and the goggles on it pushed up over the brow: two big lensed cups on
    # a bridge, their glass catching the light.
    strap = ring_on_skull(S, "goggle_strap", c, r + 3.0 * u, V(7.0, 0, 20.6) * u, V(-11.0, 0, 14.4) * u, 1.1 * u, mats["leather"], head_bones)
    goggles = []
    for sign in (1.0, -1.0):
        direction = unit(V(0.6, sign * 0.36, 0.62))
        # On the skull grown by the hair, facing out along its normal there.
        base = c + direction * (r + 2.0 * u)
        normal = unit(direction / (r + 2.0 * u))
        rim_a, rim_b = base - normal * 0.6 * u, base + normal * 3.0 * u
        goggles.append(tree.leaf(S, "goggle_cup_%d" % (sign > 0), lambda P, a=rim_a, b=rim_b: sdf.cylinder(P, a, b, 2.9 * u, 0.5 * u), Box.around([rim_a, rim_b], 3.2 * u),
                                 mats["brass"], head_bones, protect=0.9))
        lens_a, lens_b = rim_b - normal * 0.4 * u, rim_b + normal * 0.5 * u
        goggles.append(tree.leaf(S, "goggle_lens_%d" % (sign > 0), lambda P, a=lens_a, b=lens_b: sdf.cylinder(P, a, b, 2.35 * u, 0.4 * u), Box.around([lens_a, lens_b], 2.6 * u),
                                 mats["glass"], head_bones, protect=0.9))
    bridge_a = c + unit(V(0.6, 0.36, 0.62)) * (r + 2.6 * u)
    bridge_b = c + unit(V(0.6, -0.36, 0.62)) * (r + 2.6 * u)
    goggles.append(tree.leaf(S, "goggle_bridge", lambda P: sdf.capsule(P, bridge_a, bridge_b, 0.8 * u), Box.around([bridge_a, bridge_b], 1.2 * u), mats["leather"], head_bones,
                             protect=0.6))
    return Over([hairdo, band, strap, Union(goggles, k=0.2 * u)])


def loose_lock(S, name, root, direction, normal, length, radius, droop, curl, material, bones):
    """One loose lock along hair.guide's curve, tapering to a point, as hair.clump draws it but kept dense through the
    reduction (protect): a thin lock reduced to a few vertices takes the head's weight through the skin's smoothing."""
    points = hair.guide(root, direction, normal, length, droop, curl)
    radii = [radius * r for r in (0.85, 1.0, 0.95, 0.78, 0.55, 0.3, 0.06)]
    return tree.leaf(S, name, lambda P: sdf.tube(P, points, radii), Box.around(points, radius * 1.2), material, bones, protect=0.7)


def ring_on_skull(S, name, centre, radii, front, back, half, material, bones):
    """A band round the head: the slice of an ellipsoid (centre, radii) within half of the plane through front and back
    (points on the head's middle line) across the head."""
    along = back - front
    normal = unit(V(-along[2], 0.0, along[0]))
    middle = (front + back) * 0.5

    def distance(P):
        return sdf.smax(sdf.ellipsoid(P, centre, radii), np.abs((P - middle) @ normal) - half, 0.3 * half)
    return tree.leaf(S, name, distance, Box(centre - radii - 1.0, centre + radii + 1.0), material, bones, protect=0.7)


# ---------------------------------------------------------------------------------------------- hands
def hand(S, L, H, side, mats, figure):
    """A gloved hand hanging at her side, palm in: the left loosely curled, the right closed in a fist round the spray
    lance's grip, which runs forward through it. Returns (its node, (the grip's centre, the way through the fist))."""
    w0, w1 = V(*L["hand_" + side][0]), V(*L["hand_" + side][1])
    sign = 1.0 if side == "l" else -1.0
    x = unit(w1 - w0)
    out = V(0, sign, 0)
    z = unit(out - x * (out @ x))
    y = np.cross(z, x)
    if side == "r":
        curl = {"index": (75, 95, 55), "middle": (78, 98, 55), "ring": (80, 98, 55), "little": (82, 98, 50)}
        thumb = (60, 30, 35, 25)
    else:
        curl = {"index": (30, 40, 25), "middle": (35, 45, 28), "ring": (38, 48, 30), "little": (40, 50, 30)}
        thumb = (35, 15, 20, 15)
    bare = anatomy.Hand(S, mats["skin"], side, H * HAND_SHARE, "hand_" + side, curl=curl, thumb=thumb)
    built = bare.build()
    glove = Shell(S, "glove_" + side, built, 0.1, 0.4, Zone(lambda P: -np.ones(len(P)), Box((-30, -30, -30), (30, 30, 30))), mats["glove"], hem=0.1, reach=0.5)
    layers = [built, glove]
    if side == "r":
        # The hand that sprays: its glove scorched across the back and knuckles.
        k = bare.u
        burn = Zone(lambda P: sdf.ellipsoid(P, V(7.5, 0.5, 1.5) * k, V(4.5, 3.4, 3.0) * k), Box(V(2, -4, -2) * k, V(13, 5, 5) * k))
        layers.append(Shell(S, "glove_scorch", Over(layers), 0.0, 0.15, burn, mats["scorch"], hem=0.3))
    axes = np.stack([x, y, z], axis=1)
    placed = Placed(Over(layers), w0, axes)
    figure.parts.append(placed)
    # The fist's tunnel: below the palm, across it (the hand's y: forward, for a hanging right hand).
    return placed, (w0 + axes @ (V(8.3, 0.0, -2.1) * bare.u), unit(y))


# ---------------------------------------------------------------------------------------------- clothes
def angle_from_front(P):
    return np.abs(np.arctan2(P[:, 1], P[:, 0]))


def ring_zone(a, b, radius):
    """Everything within a short flat-ended cylinder from a to b: a strap's or cuff's band round a limb (a capsule's
    round ends would reach a radius past each end)."""
    return Zone(lambda P: sdf.cylinder(P, a, b, radius), Box.around([a, b], radius + 1.0))


def clothes(S, L, dims, mats, body, limbs):
    """Her clothes over her skin, each keeping to its limbs: a dark stocking on her right leg to mid-thigh and trousers on
    her left, black shorts, heavy boots, a black top over her breasts and her midriff bare; over them the heavy coat,
    open down the front, its lapels and turned cuffs showing its red lining."""
    H = dims["height"]
    pz = L["pelvis"][0][2]
    cz = L["spine_03"][1][2]
    torso = cz - pz
    belt_z = pz + torso * 0.18
    # Legs: the left covered to the waist, the right in a stocking to mid-thigh, bare above it to the shorts.
    left = both(band_z(H * 0.04, belt_z + 1.0), garments.keep_to(limbs, ["leg_l", "torso"]))
    right = both(band_z(H * 0.04, stocking_top(L, H)), garments.keep_to(limbs, ["leg_r"]))
    trousers = Shell(S, "trousers", body, 0.3, 0.6, either(left, right), mats["trousers"], hem=0.3, reach=0.8)
    shorts = Shell(S, "shorts", Over([body, trousers]), 0.4, 0.6, both(band_z(pz - H * 0.075, belt_z + 1.0), garments.keep_to(limbs, ["leg_l", "leg_r", "torso"])),
                   mats["shorts"], hem=0.4, displace=folds((0, 0, 1), 8, 0.3, seed=171), reach=0.5)
    boot_mats = {"boot": mats["boot"], "sole": mats["sole"]}
    boots = [garments.boot(S, "boot_" + side, L, side, H * 0.25, boot_mats, None, shaft=1.15, toe=1.1) for side in ("l", "r")]
    lower = Over([body, trousers, shorts] + boots)
    # The top: black, over her breasts, high under the arms; bare from under it to the belts.
    top = Shell(S, "top", lower, 0.3, 0.5, both(band_z(cz - torso * 0.46, cz - torso * 0.1), garments.keep_to(limbs, ["torso"])), mats["top"], hem=0.4, reach=0.5)
    under = Over([lower, top])
    # The coat: from just under the belts to the shoulders and down both arms to the wrist, open down the front by an
    # angle that widens over the breasts and narrows toward the collar.
    opening = lambda z: np.radians(np.interp(z, [belt_z, cz - torso * 0.3, cz - torso * 0.1, cz + 5.0], [36.0, 44.0, 38.0, 30.0]))  # noqa: E731
    front_open = Zone(lambda P: (angle_from_front(P) - opening(P[:, 2])) * np.maximum(np.hypot(P[:, 0], P[:, 1]), 4.0) * np.where(P[:, 0] > 0, 1.0, 50.0),
                      Box((-5, -40, belt_z - H * 0.05), (40, 40, cz + 20)))
    coat_region = without(either(both(band_z(belt_z - H * 0.035, cz + H * 0.02), garments.keep_to(limbs, ["torso"])),
                                 garments.keep_to(limbs, ["upperarm_l", "upperarm_r", "forearm_l", "forearm_r"])), front_open)
    coat = Shell(S, "coat", under, 0.8, 1.2, coat_region, mats["coat"], hem=0.5, displace=folds((0, 0, 1), 9, 0.6, seed=172), reach=1.2)
    dressed = Over([under, coat])
    # The lapels: the lining turned back along the coat's front edges, narrow at the waist and broad over the chest.
    facing = lambda z: np.radians(np.interp(z, [belt_z, cz - torso * 0.35, cz - torso * 0.12, cz + 5.0], [7.0, 8.0, 20.0, 22.0]))  # noqa: E731

    def lapel(P):
        angle, edge = angle_from_front(P), opening(P[:, 2])
        rho = np.maximum(np.hypot(P[:, 0], P[:, 1]), 4.0)
        return np.maximum((edge - angle) * rho, (angle - edge - facing(P[:, 2])) * rho)
    lapels = Shell(S, "lapels", dressed, 0.0, 0.5, both(Zone(lapel, Box((0, -40, belt_z - H * 0.04), (40, 40, cz + 12))), band_z(belt_z - H * 0.035, cz + H * 0.02),
                                                        garments.keep_to(limbs, ["torso"])), mats["lining"], hem=0.3)
    cuffs = []
    for side in ("l", "r"):
        e, w = V(*L["lowerarm_" + side][0]), V(*L["lowerarm_" + side][1])
        region = both(ring_zone(w - (e - w) * 0.02, w + (e - w) * 0.24, 6.8), garments.keep_to(limbs, ["forearm_" + side]))
        cuffs.append(Shell(S, "cuff_" + side, dressed, 0.1, 1.0, region, mats["lining"], hem=0.3, reach=0.5))
    faced = Over([dressed, lapels] + cuffs)
    # The coat's second layer: a short cape over the shoulders and the tops of the arms, open in front beside the
    # lapels, its hem cut ragged: the heavy layered shoulders that make her outline.
    tongues = 18

    def mantle_hem(P):
        angle = np.arctan2(P[:, 1], P[:, 0])
        strip = (angle + np.pi) / (2.0 * np.pi) * tongues
        tip = 1.0 - np.abs(2.0 * (strip % 1.0) - 1.0)
        hem = cz - H * 0.07 - H * 0.025 * np.abs(np.sin(angle)) - (H * 0.006 + H * 0.018 * paint.hashed(np.floor(strip), 7.0)) * tip ** 0.7
        return np.maximum(hem - P[:, 2], P[:, 2] - (cz + H * 0.04))
    shoulder = abs(L["upperarm_l"][0][1])
    mantle_open = Zone(lambda P: (angle_from_front(P) - opening(P[:, 2]) - facing(P[:, 2]) - np.radians(2.0)) * np.maximum(np.hypot(P[:, 0], P[:, 1]), 4.0)
                       * np.where(P[:, 0] > 0, 1.0, 50.0), Box((-5, -40, cz - 30), (40, 40, cz + 20)))
    region = without(both(Zone(mantle_hem, Box((-40, -shoulder - 20, cz - H * 0.16), (40, shoulder + 20, cz + H * 0.05))),
                          garments.keep_to(limbs, ["torso", "upperarm_l", "upperarm_r"])), mantle_open)
    mantle = Shell(S, "mantle", faced, 0.3, 1.4, region, mats["coat"], hem=0.5, displace=folds((0, 0, 1), 12, 0.9, seed=174), reach=1.0)
    return Over([faced, mantle])


def stocking_top(L, H):
    return L["pelvis"][0][2] - H * 0.15


def collar(S, L, dims, mats):
    """Her coat's high collar: standing round the back and sides of her neck and flaring as it rises to a point beside
    each side of her jaw, open at the throat; dark outside, its red lining inside."""
    H = dims["height"]
    cz = L["spine_03"][1][2]
    bones = anatomy.along("spine_03", "neck_01", (0, 0, cz - 4.0), (0, 0, cz + 10.0), 0.3, 0.9)
    base, tip = V(-0.8, 0, cz - 3.5), V(-1.6, 0, cz + H * 0.075)
    box = Box((-20, -20, cz - 6), (20, 20, cz + H * 0.09))

    def ring(name, outer, inner, material):
        shell = lambda P: np.maximum(sdf.round_cone(P, base, tip, outer[0], outer[1]), -sdf.round_cone(P, base - V(0, 0, 2), tip + V(0, 0, 2), inner[0], inner[1]))  # noqa: E731
        return tree.leaf(S, name, shell, box, material, bones)

    def shape(P):
        angle = angle_from_front(P)
        # Its top edge: low behind, rising to a point behind each side of the jaw (clear of the face from the game
        # camera); open in front.
        rise = cz + H * (0.04 + 0.024 * np.exp(-((angle - np.radians(82.0)) / 0.35) ** 2) + 0.012 * (angle / np.pi))
        return np.maximum(np.maximum(P[:, 2] - rise, (cz - 3.0) - P[:, 2]), (np.radians(50.0) - angle) * 12.0)
    outer = ring("collar", (9.6, 12.2), (8.4, 11.0), mats["coat"])
    inner = ring("collar_lining", (8.5, 11.1), (7.6, 10.2), mats["lining"])
    return tree.Intersect(Over([outer, inner]), Zone(shape, box), 0.4)


# ---------------------------------------------------------------------------------------------- gear
def gear(S, L, dims, mats, coated, limbs):
    """The working gear over her coat: the rig's harness straps over both shoulders and down her front, a sealed bottle
    on each over her chest; the waist belt and a lower belt dropping to her right hip, two large reagent canisters hung
    from it there and a pouch and a bottle at her left; straps round her thighs and round each boot."""
    H = dims["height"]
    pz = L["pelvis"][0][2]
    cz = L["spine_03"][1][2]
    torso = cz - pz
    belt_z = pz + torso * 0.18
    leather = mats["leather"]
    # The work on her coat: a scorch over her right shoulder and down the sleeve of the arm that sprays, and chemical runs
    # down her left sleeve and the coat's right front: each splashed wide where it struck and running down to a point
    # (round patches would read as spots).
    marks = []
    s_r, e_r = V(*L["upperarm_r"][0]), V(*L["upperarm_r"][1])
    e_l, w_l = V(*L["lowerarm_l"][0]), V(*L["lowerarm_l"][1])
    for name, point, way, run, radii, material, keep in (
            ("scorch", s_r + (e_r - s_r) * 0.25, V(0.2, -1.0, 0.3), (e_r - s_r) * 0.55, (4.2, 1.8), mats["scorch"], ["upperarm_r", "torso"]),
            ("stain_sleeve", e_l + (w_l - e_l) * 0.3, V(0.7, 1.0, 0.0), (w_l - e_l) * 0.5, (3.0, 1.0), mats["stain"], ["forearm_l"]),
            ("stain_coat", V(0, 0, belt_z + torso * 0.4), V(np.cos(np.radians(-64.0)), np.sin(np.radians(-64.0)), 0.0), V(0, 0, -torso * 0.32), (3.4, 1.0), mats["stain"],
             ["torso"])):
        start = point + unit(way) * 40.0
        (hit,), _ = garments.surface_points(coated, start[None, :], -unit(way)[None, :], reach=40.0)
        end = hit + run
        zone = Zone(lambda P, a=hit, b=end, r=radii: sdf.round_cone(P, a, b, r[0], r[1]), Box.around([hit, end], max(radii) + 1.0))
        marks.append(Shell(S, name, coated, 0.0, 0.25, both(zone, garments.keep_to(limbs, keep)), material, hem=0.5))
    coated = Over([coated] + marks)
    straps, strap_lines = [], {}
    for sign in (1.0, -1.0):
        line = harness_line(coated, sign, cz, torso, belt_z)
        strap_lines[sign] = line
        straps.append(Shell(S, "harness_" + ("l" if sign > 0 else "r"), coated, 0.1, 0.8, both(around(line, [2.2] * len(line)), garments.keep_to(limbs, ["torso"])),
                            leather, hem=0.2, reach=0.3))
    belt = Shell(S, "belt", coated, 0.2, 1.0, both(band_z(belt_z - H * 0.013, belt_z + H * 0.013), garments.keep_to(limbs, ["torso"])), leather, hem=0.2)
    hip_z = belt_z - H * 0.035
    hip_band = garments.tilted_band(V(0, 0, hip_z), unit(V(0, -0.2, 1.0)), H * 0.022, Box((-40, -40, hip_z - 12), (40, 40, hip_z + 8)))
    hip_belt = Shell(S, "hip_belt", coated, 0.2, 1.0, both(hip_band, garments.keep_to(limbs, ["torso", "leg_l", "leg_r"])), leather, hem=0.2)
    thigh_straps = []
    for side in ("l", "r"):
        hp, k = V(*L["thigh_" + side][0]), V(*L["thigh_" + side][1])
        axis = unit(k - hp)
        shares = (0.42, 0.62) if side == "l" else ((hp[2] - stocking_top(L, H)) / (hp[2] - k[2]),)
        for i, share in enumerate(shares):
            p = hp + (k - hp) * share
            region = both(ring_zone(p - axis * 1.3, p + axis * 1.3, 11.0), garments.keep_to(limbs, ["leg_" + side]))
            thigh_straps.append(Shell(S, "thigh_strap_%s_%d" % (side, i), coated, 0.2, 0.8, region, leather, hem=0.2))
    boot_straps = []
    for side in ("l", "r"):
        knee, ankle = V(*L["calf_" + side][0]), V(*L["calf_" + side][1])
        p = ankle + (knee - ankle) * 0.62
        region = both(ring_zone(p - V(0, 0, 1.4), p + V(0, 0, 1.4), 10.0), garments.keep_to(limbs, ["leg_" + side]))
        boot_straps.append(Shell(S, "boot_strap_" + side, coated, 0.2, 0.7, region, leather, hem=0.2))
    strapped = Over([coated] + straps + [belt, hip_belt] + thigh_straps + boot_straps)
    parts = []
    # A sealed bottle on each harness strap high on the chest, labelled and valved.
    for i, sign in enumerate((1.0, -1.0)):
        front = strap_lines[sign][strap_lines[sign][:, 0] > 2.0]
        front = front[np.argsort(front[:, 2])]
        z = cz - torso * 0.14
        y = np.interp(z, front[:, 2], front[:, 1])
        (hit,), _ = garments.surface_points(strapped, V(40.0, y, z)[None, :], V(-1.0, 0, 0)[None, :], reach=40.0)
        parts += bottle(S, "vial_%d" % i, hit + V(1.5, 0, 0), 1.5, 3.4, mats["steel"], mats["placard"] if sign > 0 else mats["label_white"], mats["brass"],
                        anatomy.rigid("spine_03"))
    # Two large reagent canisters hung from the hip belt at her right hip, toward the front, their windows lit.
    for i, degrees in enumerate((-13.0, -31.0)):
        a = np.radians(degrees)
        out = V(np.cos(a), np.sin(a), 0.0)
        z = hip_z - H * 0.065 + 0.2 * np.sin(a) * 12.0
        (hit,), _ = garments.surface_points(strapped, (V(0, 0, z) + out * 40.0)[None, :], -out[None, :], reach=40.0)
        parts += canister(S, "canister_%d" % i, V(hit[0], hit[1], z) + out * 4.0, 3.8, 7.2, mats, anatomy.rigid("pelvis"))
    # A pouch and a bottle on the hip belt at her left.
    for name, degrees, rise in (("pouch", 40.0, 0.01), ("hip_bottle", 18.0, -0.005)):
        a = np.radians(degrees)
        out = V(np.cos(a), np.sin(a), 0.0)
        z = hip_z + H * rise + 0.2 * np.sin(a) * 12.0
        (hit,), _ = garments.surface_points(strapped, (V(0, 0, z) + out * 40.0)[None, :], -out[None, :], reach=40.0)
        if name == "pouch":
            axes = np.stack([unit(np.cross(V(0, 0, 1), out)), V(0, 0, 1), out], axis=1)
            parts.append(garments.pouch(S, name, hit - V(0, 0, H * 0.03), axes, (H * 0.05, H * 0.055, H * 0.028), H * 0.018, leather, bones=anatomy.rigid("pelvis")))
        else:
            parts += bottle(S, name, hit + out * 1.8 - V(0, 0, H * 0.025), 1.8, 4.0, mats["steel"], mats["lining"], mats["brass"], anatomy.rigid("pelvis"))
    return Over([strapped, Union(parts, k=0.2)])


def harness_line(base, sign, cz, torso, belt_z):
    """The line one of the rig's harness straps lies along on base: up her back from the rig's foot, over the shoulder
    between her neck and the joint, and down her front over the edge of her top to the waist belt."""
    starts, ways = [], []
    for z in (belt_z + 3.0, cz - 18.0):
        starts.append(V(-40.0, sign * 9.5, z))
        ways.append(V(1.0, 0, 0))
    centre = V(0, sign * 11.0, cz - 8.0)
    for phi in np.radians(np.linspace(-75.0, 75.0, 7)):
        out = V(np.sin(phi), 0, np.cos(phi))
        starts.append(centre + out * 40.0)
        ways.append(-out)
    for y, z in ((8.6, cz - 16.0), (7.0, cz - torso * 0.35), (6.0, belt_z + 2.0)):
        starts.append(V(40.0, sign * y, z))
        ways.append(V(-1.0, 0, 0))
    hits, _normals = garments.surface_points(base, np.array(starts), np.array(ways), reach=40.0)
    return hits


def bottle(S, name, c, radius, half, steel, label, cap, bones):
    """A sealed reagent bottle standing upright: a steel body, a coloured label band round its middle, a valve cap."""
    a, b = c - V(0, 0, half), c + V(0, 0, half)
    mid_a, mid_b = c - V(0, 0, half * 0.35), c + V(0, 0, half * 0.35)
    top_a, top_b = b - V(0, 0, 0.3), b + V(0, 0, radius * 0.9)
    return [tree.leaf(S, name, lambda P: sdf.cylinder(P, a, b, radius, radius * 0.3), Box.around([a, b], radius * 1.2), steel, bones, protect=0.6),
            tree.leaf(S, name + "_label", lambda P: sdf.cylinder(P, mid_a, mid_b, radius * 1.1, 0.1), Box.around([mid_a, mid_b], radius * 1.3), label, bones, protect=0.6),
            tree.leaf(S, name + "_cap", lambda P: sdf.cylinder(P, top_a, top_b, radius * 0.6, 0.2), Box.around([top_a, top_b], radius), cap, bones, protect=0.6)]


def canister(S, name, c, radius, half, mats, bones):
    """A large reagent canister hanging upright: a steel can banded dark at both ends, a window round its middle onto the
    glowing reagent, a brass valve on top."""
    up = V(0, 0, 1)
    parts = []

    def piece(label, a, b, r, material, rounding=0.0):
        parts.append(tree.leaf(S, name + "_" + label, lambda P, a=a, b=b: sdf.cylinder(P, a, b, r, rounding), Box.around([a, b], r * 1.2), material, bones, protect=0.7))
    piece("can", c - up * half, c + up * half, radius, mats["steel"], radius * 0.2)
    for end in (-1.0, 1.0):
        piece("band_%d" % (end > 0), c + up * end * half * 0.62, c + up * end * half * 0.9, radius * 1.08, mats["dark_steel"])
    piece("window", c - up * half * 0.42, c + up * half * 0.42, radius * 1.05, mats["reagent"])
    piece("valve", c + up * (half - 0.3), c + up * (half + radius * 0.6), radius * 0.45, mats["brass"], 0.2)
    return parts


# ---------------------------------------------------------------------------------------------- the rig
def rig(S, L, dims, mats, grip):
    """The pressurised dispenser rig on her back (her chest's bone carries it): a banded steel cylinder from her waist to
    her shoulders, a tall glass window down its back onto the glowing orange reagent and a gauge-glass ring round its top,
    so it reads from any side, a lozenge hazard mark on the glass; the pump housing on top, its top a yellow-and-black
    placard, a pressure gauge on its flank; and two armoured hoses from the housing over her right shoulder and down the
    back of her arm to the spray lance in her right fist, one dark, one lit from within."""
    H = dims["height"]
    pz = L["pelvis"][0][2]
    cz = L["spine_03"][1][2]
    torso = cz - pz
    bones = anatomy.rigid("spine_03")
    R = H * 0.058
    cx = -(H * 0.075 + R)
    # From her waist to above her shoulders, so its top and housing show over them from the game camera.
    z0, z1 = pz + torso * 0.24, cz + H * 0.012
    span = z1 - z0
    at = lambda share: V(cx, 0, z0 + span * share)  # noqa: E731
    parts = []

    def piece(name, a, b, r, material, rounding=0.0, protect=0.8):
        parts.append(tree.leaf(S, name, lambda P, a=a, b=b: sdf.cylinder(P, a, b, r, rounding), Box.around([a, b], r * 1.2), material, bones, protect=protect))
    piece("tank", at(0.0), at(1.0), R, mats["steel"], R * 0.18)
    for i, (s0, s1) in enumerate(((0.05, 0.12), (0.5, 0.56), (0.9, 0.96))):
        piece("tank_band_%d" % i, at(s0), at(s1), R * 1.07, mats["dark_steel"], 0.3)
    # The window: down its back half between the bands, standing a little proud of the steel.
    w0, w1 = z0 + span * 0.14, z0 + span * 0.86

    def window(P):
        Q = P - V(cx, 0, 0)
        facing = np.arctan2(Q[:, 1], -Q[:, 0])
        d = sdf.cylinder(P, at(0.1), at(0.9), R * 1.03, 0.0)
        return np.maximum(d, np.maximum(np.maximum(w0 - P[:, 2], P[:, 2] - w1), (np.abs(facing) - np.radians(68.0)) * R))
    parts.append(tree.leaf(S, "tank_window", window, Box(V(cx - R * 1.2, -R * 1.2, w0 - 1), V(cx + R * 1.2, R * 1.2, w1 + 1)), mats["reagent"], bones, protect=0.8))
    piece("tank_gauge", at(0.7), at(0.86), R * 1.035, mats["reagent"])
    # The lozenge hazard mark on the glass, facing back: a dark diamond's outline and its heart, lying on the curve.
    mz = z0 + span * 0.32

    def lozenge(P):
        Q = P - V(cx, 0, 0)
        q = np.abs(Q[:, 1]) + np.abs(P[:, 2] - mz)
        outline = np.maximum(q - R * 0.62, R * 0.44 - q)
        heart = q - R * 0.2
        skin = np.maximum(np.abs(np.hypot(Q[:, 0], Q[:, 1]) - R * 1.05) - 0.4, Q[:, 0])
        return np.maximum(np.minimum(outline, heart), skin)
    parts.append(tree.leaf(S, "lozenge", lozenge, Box(V(cx - R * 1.2, -R, mz - R), V(cx, R, mz + R)), mats["placard_black"], bones, protect=1.0))
    # The pump housing on top, a yellow-and-black placard over it and a gauge on its flank.
    housing = V(cx + R * 0.1, 0, z1 + H * 0.02)
    half = V(R * 0.8, R * 0.95, H * 0.022)
    parts.append(tree.leaf(S, "housing", lambda P: sdf.box(P, housing, half, None, 0.6), Box(housing - half - 1, housing + half + 1), mats["dark_steel"], bones, protect=0.8))
    # The placard: a yellow plate, its black diagonal stripes raised on it as ridges, so their edges are the mesh's own (a
    # flat plate is merged into a few faces by the reduction, and painted stripes on it are lost).
    plate_c = housing + V(0, 0, half[2] + 0.25)
    plate_half = V(half[0] * 0.92, half[1] * 0.92, 0.5)
    plate_box = Box(plate_c - plate_half - 2, plate_c + plate_half + 2)
    parts.append(tree.leaf(S, "placard", lambda P: sdf.box(P, plate_c, plate_half, None, 0.2), plate_box, mats["placard"], bones, protect=1.0))
    ridge_c, ridge_half = plate_c + V(0, 0, 0.8), plate_half - V(1.0, 1.0, 0.0)

    def stripes(P):
        diagonal = (P[:, 0] - plate_c[0] + P[:, 1] - plate_c[1]) / np.sqrt(2.0)
        band = np.abs(np.mod(diagonal, 6.0) - 3.0) - 1.4
        return np.maximum(sdf.box(P, ridge_c, ridge_half, None, 0.15), band)
    parts.append(tree.leaf(S, "placard_stripes", stripes, plate_box, mats["placard_black"], bones, protect=1.0))
    dial = housing + V(0, half[1], 0)
    piece("gauge_rim", dial - V(0, 0.4, 0), dial + V(0, 1.2, 0), H * 0.016, mats["brass"], 0.2, protect=1.0)
    piece("gauge_face", dial + V(0, 1.0, 0), dial + V(0, 1.5, 0), H * 0.012, mats["gauge"], 0.1, protect=1.0)
    # The hoses' outlet on the housing's right flank.
    outlet = housing + V(R * 0.2, -half[1], 0)
    piece("outlet", outlet + V(0, 0.5, 0), outlet - V(0, 2.0, 0), H * 0.016, mats["brass"], 0.2)
    rigged = Union(parts, k=0.25)
    return Over([rigged, hoses(S, L, dims, mats, outlet - V(0, 1.8, 0), grip), lance(S, L, dims, mats, grip)])


def hoses(S, L, dims, mats, outlet, grip):
    """The two armoured hoses from the rig over her right shoulder and down the back of her arm into the lance's inlet,
    side by side, the lit one outside the dark: each span skinned to the bones it lies along, so it rides her shoulder and
    arm; steel clamps bind the pair at each span."""
    H = dims["height"]
    cz = L["spine_03"][1][2]
    s0, e = V(*L["upperarm_r"][0]), V(*L["upperarm_r"][1])
    w = V(*L["lowerarm_r"][1])
    c, a = grip
    inlet = c - a * 8.6
    # Where the dark hose runs, the bone carrying each point, and where the lit hose lies beside it there.
    path = [(outlet, "spine_03", V(0, 0, 2.6)),
            (V(-4.5, -14.0, cz + 4.7), "clavicle_r", V(-2.6, 0, 0.3)),
            (s0 + V(-5.5, -2.5, 3.0), "clavicle_r", V(-2.0, -1.6, 0)),
            (s0 + (e - s0) * 0.5 + V(-5.0, -0.6, 0), "upperarm_r", V(-0.9, -2.4, 0)),
            (e + V(-5.0, -0.6, 0), "lowerarm_r", V(-0.9, -2.4, 0)),
            (e + (w - e) * 0.62 + V(-4.8, -0.8, 0), "lowerarm_r", V(-0.9, -2.4, 0)),
            (inlet + V(-0.5, 0.8, 0), "prop_r", V(0, -1.9, 0.3))]
    parts = []
    for i in range(len(path) - 1):
        pa, bone_a, lit_a = path[i]
        pb, bone_b, lit_b = path[i + 1]
        weights = anatomy.rigid(bone_a) if bone_a == bone_b else anatomy.along(bone_a, bone_b, pa, pb, 0.2, 0.8)
        for name, (qa, qb), radius, material in (("hose", (pa, pb), H * 0.011, mats["hose"]), ("hose_lit", (pa + lit_a, pb + lit_b), H * 0.0075, mats["reagent"])):
            parts.append(tree.leaf(S, "%s_%d" % (name, i), lambda P, qa=qa, qb=qb, r=radius: sdf.capsule(P, qa, qb, r), Box.around([qa, qb], radius * 1.3), material,
                                   weights, protect=0.6))
        tangent = unit(pb - pa)
        middle = (pa + pb) * 0.5 + (lit_a + lit_b) * 0.25
        clamp_a, clamp_b = middle - tangent * 1.0, middle + tangent * 1.0
        parts.append(tree.leaf(S, "hose_clamp_%d" % i, lambda P, qa=clamp_a, qb=clamp_b: sdf.cylinder(P, qa, qb, H * 0.022, 0.3), Box.around([clamp_a, clamp_b], H * 0.025),
                               mats["steel"], weights, protect=0.6))
    return Union(parts, k=0.3)


def lance(S, L, dims, mats, grip):
    """The spray lance in her right fist, carried low and pointing ahead: its grip through the fist, the brass inlet
    behind it where the hoses join, a valve lever over it, a steel lance tube and a flared nozzle lit orange at its mouth.
    It hangs from the hand's prop bone, so her clips carry it and raise it to spray."""
    H = dims["height"]
    c, a = grip
    up = V(0, 0, 1)
    bones = anatomy.rigid("prop_r")
    parts = []

    def piece(name, p, q, r, material, rounding=0.0):
        parts.append(tree.leaf(S, name, lambda P, p=p, q=q: sdf.cylinder(P, p, q, r, rounding), Box.around([p, q], r * 1.2), material, bones, protect=1.0))
    piece("lance_grip", c - a * 4.0, c + a * 4.0, 1.5, mats["dark_steel"], 0.3)
    piece("lance_inlet", c - a * 8.6, c - a * 3.8, 2.6, mats["brass"], 0.4)
    piece("lance_body", c + a * 3.8, c + a * 9.0, 2.2, mats["dark_steel"], 0.4)
    piece("lance_collar", c + a * 8.5, c + a * 10.0, 2.7, mats["brass"], 0.2)
    lever = c + a * 5.5 + up * 2.6
    parts.append(tree.leaf(S, "lance_lever", lambda P: sdf.box(P, lever, (0.9, 0.7, 2.2), sdf.frame(a, up), 0.3), Box(lever - 3, lever + 3), mats["placard_black"], bones,
                           protect=1.0))
    tip = c + a * (H * 0.17)
    piece("lance_tube", c + a * 9.5, tip, 1.05, mats["steel"])
    parts.append(tree.leaf(S, "lance_nozzle", lambda P: sdf.round_cone(P, tip - a * 1.0, tip + a * 4.0, 1.3, 2.2), Box.around([tip - a, tip + a * 4], 2.6), mats["steel"], bones,
                           protect=1.0))
    piece("lance_mouth", tip + a * 4.2, tip + a * 5.0, 1.7, mats["reagent"], 0.2)
    return Union(parts, k=0.2)


# ---------------------------------------------------------------------------------------------- the coat's skirt
def coat_skirt(S, L, dims, mats, worn):
    """Her coat's long skirt: from under the belt round her hips and back to the calves, open at the front and shorter
    there, flaring as it falls; dark outside and lined red within; its ragged hem torn into tongues painted in
    alternate red and white hazard stripes, slanted. Each half hangs on its own coat chain (ADR-069 §7)."""
    H = dims["height"]
    pz = L["pelvis"][0][2]
    cz = L["spine_03"][1][2]
    top_z = pz + (cz - pz) * 0.18 - H * 0.004
    t0, t1 = np.radians(37.0), np.radians(323.0)
    samples = np.linspace(0.0, 1.0, 17)
    theta_s = t0 + (t1 - t0) * samples
    outward = np.stack([np.cos(theta_s), np.sin(theta_s), np.zeros_like(theta_s)], axis=1)
    centres = np.stack([np.zeros_like(theta_s), np.zeros_like(theta_s), np.full_like(theta_s, top_z)], axis=1)
    # Out from inside her (a ray coming in would meet the hanging arms first): where each leaves her coat.
    hits, _normals = garments.surface_points(worn, centres, outward, reach=40.0)
    tops = hits + outward * 0.3
    hem_back, hem_front = H * 0.1, H * 0.21
    limbs = [(L["thigh_" + s][0], L["thigh_" + s][1], H * 0.05) for s in ("l", "r")] + [(L["calf_" + s][0], L["calf_" + s][1], H * 0.045) for s in ("l", "r")]
    phase = np.random.default_rng(173).uniform(0, 2 * np.pi, 2)

    def place(gu, gv, inset=0.0):
        theta = t0 + (t1 - t0) * gu
        top = np.stack([np.interp(gu, samples, tops[:, k]) for k in range(3)], axis=1)
        radial = np.stack([np.cos(theta), np.sin(theta), np.zeros_like(theta)], axis=1)
        back = 0.5 - 0.5 * np.cos(theta)
        flare = (H * 0.06 + H * 0.06 * back) * gv ** 1.25
        pleat = (0.3 + H * 0.01 * gv) * (0.6 * np.sin(theta * 7.0 + phase[0]) + 0.4 * np.sin(theta * 11.0 + phase[1]))
        p = top + radial * (flare + pleat - inset)[:, None]
        hem = hem_back + (hem_front - hem_back) * ((1.0 + np.cos(theta)) * 0.5) ** 1.5
        p[:, 2] = top[:, 2] + (hem - top[:, 2]) * gv
        return sheet.clear_of(p, limbs, 2.4 - inset)

    def weights(P, gv):
        hold = np.clip(1 - gv / 0.12, 0, 1)
        lower = np.clip((gv - 0.3) / 0.45, 0, 1) * (1 - hold)
        upper = (1 - hold) - lower
        left = np.clip(0.5 + P[:, 1] / 12.0, 0, 1)
        w = {"pelvis": hold}
        for side, share in (("l", left), ("r", 1 - left)):
            w["coat_%s_01" % side] = upper * share
            w["coat_%s_02" % side] = lower * share
        return {k: np.asarray(x, dtype=np.float32) for k, x in w.items()}

    band, stripes, slant = 0.74, 14, 0.5
    sheets = [sheet.Sheet("coat_skirt", lambda u, v: place(u, v * band), S.material("coat_skirt", PALETTE["coat"]), lambda P, u, v: weights(P, v * band), stripes, 3),
              sheet.Sheet("coat_lining", lambda u, v: place(u, v * band, 0.9), S.material("coat_lining", PALETTE["lining"]), lambda P, u, v: weights(P, v * band), stripes, 3)]
    for k in range(stripes):
        def spread(uu, vv, k=k):
            """Where on the skirt a point of this stripe lies: its band slanting across as it falls, the end stripes held
            to the skirt's front edges."""
            lo = 0.0 if k == 0 else (k + slant * vv) / stripes
            hi = 1.0 if k == stripes - 1 else (k + 1 + slant * vv) / stripes
            return lo + (hi - lo) * uu, band + (1.0 - band) * vv

        def reach(uu, k=k):
            """Each stripe a tongue torn to its own length, pointed."""
            return np.clip(0.55 + 0.45 * paint.hashed(np.full(np.shape(uu), k), 17.0) - 0.18 * np.abs(2.0 * np.asarray(uu) - 1.0), 0.1, 1.0)
        material = S.material("hazard_%d" % k, PALETTE["lining"] if k % 2 == 0 else PALETTE["hazard"])
        sheets.append(sheet.Sheet("coat_hem_%d" % k, lambda uu, vv, spread=spread: place(*spread(uu, vv)), material,
                                  lambda P, uu, vv, spread=spread: weights(P, spread(uu, vv)[1]), 2, 2, reach=reach))
    return sheets
