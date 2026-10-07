"""Raska (ADR-069): a lean, hard-worked courier and brawler, read by her silhouette from the game camera. Character
Bible and her splash art: long windblown dark auburn hair streaked with red; a black crop top under an open red riding
jacket, a heavy armour plate over her left shoulder; dark trousers layered with buckle straps, plated thigh and knee
guards, heavy buckled boots; a segmented mechanical bracer sheathing her left forearm, an orange Flux core burning at its
wrist; fingerless gloves and a pendant at her throat.

On foot she is a humanoid; riding (her Ride body, the rider archetype) she sits on Hound, the other half of her
silhouette: a massive armoured machine in rust-red and gunmetal on two great knobbled tyres, orange Flux light through its
headlight grille, wheel hubs and engine channels. Hound is built here only when the body is the rider's, on the mount's
and wheels' bones, so its wheels spin and a crash throws her clear of it.

Low poly and flat-coloured (author 2026-10-07): the big forms that make her outline, each a flat colour the toon material
shades. Her hair hangs on the hair chains where her body has them (ADR-069 §7)."""
import math

import numpy as np

from ..sculpt import anatomy, garments, hair, sdf, tree
from ..sculpt.anatomy import V, unit
from ..sculpt.garments import around, band_z, both, either, folds
from ..sculpt.tree import Box, Over, Placed, Shell, Union, Zone

HAND_SHARE = 0.105
SKULL_CENTRE = (-1.2, 0.0, 14.4)
SKULL_RADII = (9.9, 7.5, 8.0)
HAIR_REACH = 4.0
# Knobs around each of Hound's tyres, so its spin shows.
TYRE_KNOBS = 14

PALETTE = {
    "skin": (0.8, 0.6, 0.48), "hair": (0.33, 0.1, 0.07), "streak": (0.58, 0.1, 0.08), "jacket": (0.6, 0.1, 0.09),
    "oxblood": (0.36, 0.08, 0.07), "top": (0.17, 0.16, 0.17), "trousers": (0.22, 0.2, 0.21), "leather": (0.26, 0.18, 0.13),
    "metal": (0.45, 0.43, 0.4), "gunmetal": (0.3, 0.3, 0.32), "flux": (1.0, 0.45, 0.08),
}


def materials(S, spec):
    """Her materials, flat (the toon material shades them); her Flux glows in her kit's accent."""
    mats = {name: S.material(name, colour) for name, colour in PALETTE.items() if name != "flux"}
    mats["flux"] = S.material("flux", tuple(spec.get("accent", PALETTE["flux"])), glow=True)
    return mats


def build(S, L, dims, spec):
    """Raska's sculpt on her layout, standing or seated on Hound: (the whole, {"body": the figure under her clothes})."""
    mats = materials(S, spec)
    H = dims["height"]
    figure = anatomy.Figure(S, L, dims, mats["skin"], {"muscle": 0.5, "chest": 0.85, "breadth": 0.85, "hips": 1.0, "limb": 0.85,
                                                        "deltoid": 0.8, "leg": 0.9, "neck": 0.85, "bust": 0.8, "pecs": 0.3}).build()
    head_origin = V(*L["head"][0])
    size = (L["head"][1][2] - L["head"][0][2]) * 24.0 / 21.9
    u = size / 24.0
    figure.attach_head(Placed(head(S, L, spec, mats, size, u, head_origin), head_origin, np.eye(3)))
    for side in ("l", "r"):
        figure.limbs["hand_" + side] = hand(S, L, H, side, mats, figure)
    body = figure.body()
    worn = Over([clothes(S, L, dims, mats, body, figure.limbs), bracer(S, L, mats, H)])
    if spec["archetype"] == "rider":
        worn = Over([worn, hound(S, L, dims, spec, mats)])
    ground = Zone(lambda P: -P[:, 2], Box((-400, -400, 0), (400, 400, 400)))
    return tree.Intersect(worn, ground), {"body": body}


def head(S, L, spec, mats, size, u, head_origin):
    """Her head with its long windblown hair, swept back and down her back, red streaks through it."""
    locks_bones = anatomy.rigid("head")
    chains = sorted(bone[:-3] for bone in L if bone.startswith("hair_") and bone.endswith("_01"))
    if chains:
        joints = {chain: [L["%s_%s" % (chain, joint)][0] for joint in ("01", "02", "end")] for chain in chains}
        locks_bones = hair.on_chains(joints, "head", head_origin + V(*SKULL_CENTRE) * u, V(*SKULL_RADII) * u, HAIR_REACH)
    locks = hair.messy(S, mats["hair"], locks_bones, SKULL_CENTRE, SKULL_RADII, (16.5, 7.0), seed=spec["seed"], unit_scale=u, count=30,
                       length=(7.0, 11.0), radius=(1.9, 2.7), wind=(-0.35, 0.1, 0.0), fringe=0.5, volume=0.55, cap_bones=anatomy.rigid("head"))
    # Her long hair, full down her back and over her shoulders to the shoulder blades, two locks streaked red.
    falls = []
    for k, (x, y, z, length) in enumerate(((-8.5, 0.0, 15.0, 30.0), (-7.5, -5.5, 14.0, 28.0), (-7.5, 5.5, 14.0, 28.0), (-3.0, -8.0, 13.0, 26.0),
                                           (-3.0, 8.0, 13.0, 26.0), (-9.0, -3.0, 18.0, 29.0), (-9.0, 3.0, 18.0, 29.0))):
        root = V(x, y, z) * u
        material = mats["streak"] if k in (1, 6) else mats["hair"]
        falls.append(hair.clump(S, "fall_%d" % k, root, V(-0.35, y * 0.06, -1.0), V(-1, y * 0.1, 0.0), length * u, 4.6 * u, 0.3, 0.2,
                                material, locks_bones))
    face = anatomy.Head(S, mats["skin"], size, look={"jaw": 0.85}).build()
    return Over([face, locks, Union(falls, k=0.6 * u)])


def hand(S, L, H, side, mats, figure):
    """A gloved hand closed to a fist (on her bars when she rides), fingerless plated gloves reading dark."""
    w0, w1 = V(*L["hand_" + side][0]), V(*L["hand_" + side][1])
    sign = 1.0 if side == "l" else -1.0
    x = unit(w1 - w0)
    out = V(0, sign, 0)
    z = unit(out - x * (out @ x))
    y = np.cross(z, x)
    curl = {"index": (65, 90, 50), "middle": (70, 95, 50), "ring": (72, 95, 50), "little": (75, 95, 48)}
    built = anatomy.Hand(S, mats["skin"], side, H * HAND_SHARE, "hand_" + side, curl=curl, thumb=(50, 30, 30, 25)).build()
    glove = Shell(S, "glove_" + side, built, 0.08, 0.35, Zone(lambda P: P[:, 0] - H * HAND_SHARE * 0.55, Box((-30, -30, -30), (30, 30, 30))),
                  mats["leather"], hem=0.1, reach=0.5)
    placed = Placed(Over([built, glove]), w0, np.stack([x, y, z], axis=1))
    figure.parts.append(placed)
    return placed


def clothes(S, L, dims, mats, body, limbs):
    """Dark trousers with buckle straps, plated thigh and knee guards and heavy buckled boots; a black crop top, her
    midriff bare; the open red riding jacket over it, its collar turned up, the left sleeve to the elbow over her bracer;
    a belt; the heavy layered plate over her left shoulder; a pendant at her throat."""
    H = dims["height"]
    pz = L["pelvis"][0][2]
    cz = L["spine_03"][1][2]
    torso = cz - pz
    legs = both(band_z(-1.0, pz + torso * 0.22), garments.keep_to(limbs, ["leg_l", "leg_r", "torso"]))
    trousers = Shell(S, "trousers", body, 0.4, 0.7, legs, mats["trousers"], hem=0.3, displace=folds((0, 0, 1), 8, 0.4, seed=101), reach=0.6)
    boots = [garments.boot(S, "boot_" + side, L, side, H * 0.27, {"boot": mats["leather"], "sole": mats["gunmetal"]}, None, shaft=1.12, toe=1.05)
             for side in ("l", "r")]
    lower = Over([body, trousers] + boots)
    straps, plates = [], []
    for side in ("l", "r"):
        hp, k = V(*L["thigh_" + side][0]), V(*L["thigh_" + side][1])
        for share in (0.35, 0.65):
            c = hp + (k - hp) * share
            straps.append(Shell(S, "strap_%s_%d" % (side, int(share * 100)), lower, 0.3, 0.8,
                                both(garments.band_plane(c, k - hp, H * 0.014, Box.around([c], H * 0.08)), garments.keep_to(limbs, ["leg_" + side])),
                                mats["leather"], hem=0.2))
        front = Zone(lambda P, x0=hp[0]: x0 - P[:, 0], Box((-60, -60, -10), (80, 60, 220)))
        plates.append(Shell(S, "thigh_plate_" + side, lower, 0.6, 0.9, both(around([hp + (k - hp) * 0.3, hp + (k - hp) * 0.8], [8.0, 7.0]),
                                                                           garments.keep_to(limbs, ["leg_" + side]), front),
                            mats["gunmetal"], hem=0.4, reach=0.6))
        knee_dir = unit(k - hp)
        forward = unit(V(1, 0, 0) - knee_dir * (V(1, 0, 0) @ knee_dir))
        c = k + forward * H * 0.03
        plates.append(tree.leaf(S, "knee_" + side, lambda P, c=c, f=forward: sdf.ellipsoid(P, c, V(H * 0.022, H * 0.034, H * 0.04), sdf.frame(f, V(0, 0, 1))[:, [2, 1, 0]]),
                                Box(c - H * 0.06, c + H * 0.06), mats["metal"], anatomy.rigid("calf_" + side), protect=0.5))
    legged = Over([lower] + straps + plates)
    top = Shell(S, "crop_top", legged, 0.3, 0.6, both(band_z(pz + torso * 0.55, cz + H * 0.005), garments.keep_to(limbs, ["torso"])), mats["top"], hem=0.3)
    sleeves = [garments.keep_to(limbs, ["upperarm_l"]), garments.keep_to(limbs, ["upperarm_r", "forearm_r"])]
    jacket_region = either(both(band_z(pz + torso * 0.3, cz + H * 0.02), garments.keep_to(limbs, ["torso"])), *sleeves)
    open_front = Zone(lambda P: np.maximum(2.0 - P[:, 0], np.abs(P[:, 1]) - 8.0), Box((0, -14, pz), (60, 14, cz + 20)))
    jacket = Shell(S, "jacket", Over([legged, top]), 0.6, 1.1, garments.without(jacket_region, open_front), mats["jacket"], hem=0.4,
                   displace=folds((0, 0, 1), 8, 0.5, seed=103), reach=1.0)
    dressed = Over([legged, top, jacket])
    belt_z = pz + torso * 0.16
    belt = Shell(S, "belt", dressed, 0.3, 1.1, both(band_z(belt_z - H * 0.013, belt_z + H * 0.013), garments.keep_to(limbs, ["torso"])),
                 mats["leather"], hem=0.2)
    extras = []
    # The collar turned up behind her neck.
    n0 = V(*L["neck_01"][0])
    extras.append(tree.leaf(S, "collar", lambda P: sdf.torus(P, n0 + V(-H * 0.012, 0, H * 0.012), H * 0.042, H * 0.012, sdf.frame(V(0.25, 0, 1), V(1, 0, 0))),
                            Box(n0 - H * 0.08, n0 + H * 0.08), mats["oxblood"], anatomy.rigid("spine_03"), protect=0.3))
    # The pendant at her throat.
    p = n0 + V(H * 0.035, 0, -H * 0.03)
    extras.append(tree.leaf(S, "pendant", lambda P: sdf.cylinder(P, p - V(0.6, 0, 0), p + V(0.6, 0, 0), H * 0.009, 0.3), Box(p - 4, p + 4),
                            mats["metal"], anatomy.rigid("spine_03"), protect=0.5))
    # The heavy plate over her left shoulder: three plates layered, overhanging it.
    s = V(*L["upperarm_l"][0])
    for layer in range(3):
        c = s + V(-H * 0.006 * layer, H * 0.008 * layer, H * 0.034 - H * 0.017 * layer)
        r = H * (0.082 - layer * 0.01)
        extras.append(tree.leaf(S, "plate_%d" % layer, lambda P, c=c, r=r: sdf.ellipsoid(P, c, V(r * 1.15, r, r * 0.45)), Box(c - r * 1.3, c + r * 1.3),
                                mats["gunmetal"] if layer != 1 else mats["oxblood"],
                                anatomy.along("clavicle_l", "upperarm_l", s - V(0, H * 0.05, 0), s + V(0, H * 0.05, 0), 0.3, 0.7), protect=0.5))
    return Over([dressed, belt, Union(extras, k=0.3)])


def bracer(S, L, mats, H):
    """The segmented mechanical bracer sheathing her left forearm: three plated rings, a channel of Flux along its top and
    the core burning at the wrist, the brightest point on her."""
    e0, e1 = V(*L["lowerarm_l"][0]), V(*L["lowerarm_l"][1])
    bones = anatomy.rigid("lowerarm_l")
    a, b = e0 + (e1 - e0) * 0.08, e1 - (e1 - e0) * 0.02
    parts = [tree.leaf(S, "bracer", lambda P: sdf.round_cone(P, a, b, H * 0.03, H * 0.034), Box.around([a, b], H * 0.045), mats["gunmetal"], bones, protect=0.5)]
    for k in range(3):
        a, b = e0 + (e1 - e0) * (0.3 + k * 0.27), e0 + (e1 - e0) * (0.36 + k * 0.27)
        r = H * (0.037 + k * 0.002)
        parts.append(tree.leaf(S, "bracer_ring_%d" % k, lambda P, a=a, b=b, r=r: sdf.cylinder(P, a, b, r, H * 0.003), Box.around([a, b], r * 1.2),
                               mats["metal"], bones, protect=0.5))
    axis = unit(e1 - e0)
    up = unit(V(0, 0, 1) - axis * axis[2])
    a, b = e0 + (e1 - e0) * 0.15 + up * H * 0.034, e1 + up * H * 0.036
    parts.append(tree.leaf(S, "flux_line", lambda P: sdf.capsule(P, a, b, H * 0.007), Box.around([a, b], H * 0.02), mats["flux"], bones, protect=0.8))
    parts.append(tree.leaf(S, "flux_core", lambda P: sdf.sphere(P, e1, H * 0.034), Box.around([e1], H * 0.05), mats["flux"], bones, protect=0.8))
    return Union(parts, k=0.2)


def hound(S, L, dims, spec, mats):
    """Hound, as her Ride body's rider archetype lays it out: two great knobbled tyres with burning hubs; the engine
    between them, its channels lit, crash bars standing out from its flanks; the heavy painted tank and the seat; a long
    plate over the rear wheel; the raked fork; the armoured front around the lit headlight grille; exhausts swept back,
    their mouths glowing. Rust-red and gunmetal; every light the kit's accent."""
    mount = spec["mount"]
    r, w, base = float(dims["wheel"]), float(dims["tyre"]), float(dims["wheelbase"])
    seat_z = float(dims["seatZ"])
    seat = V(*dims["seat"])
    grip_l, grip_r = V(*dims["grips"]["l"]), V(*dims["grips"]["r"])
    paint = S.material("hound_paint", tuple(mount["primary"]))
    frame = S.material("hound_frame", tuple(min(1.0, c * 1.5) for c in mount["secondary"]))
    metal = S.material("hound_metal", tuple(mount["metal"]))
    plate = S.material("hound_plate", tuple((a + b) * 0.5 for a, b in zip(mount["primary"], mount["secondary"])))
    rubber = S.material("hound_rubber", (0.12, 0.12, 0.13))
    glow = mats["flux"]
    parts = []

    def add(name, distance, points, pad, material, bone="mount", protect=0.3):
        parts.append(tree.leaf(S, name, distance, Box.around(points, pad), material, anatomy.rigid(bone), protect))

    def slab(a, b, width, thickness):
        """A plate from a to b, width across, thickness thick (a fender)."""
        x = unit(b - a)
        z = unit(np.cross(x, V(0, 1, 0)))
        axes = np.stack([x, V(0, 1, 0), z], axis=1)
        half = (np.linalg.norm(b - a) * 0.5, width * 0.5, thickness * 0.5)
        return lambda P: sdf.box(P, (a + b) * 0.5, half, axes, min(half[2] * 0.9, 1.0))

    for bone in ("wheel_back", "wheel_front"):
        axle = V(*L[bone][0])

        def tyre(P, axle=axle):
            d = sdf.cylinder(P, axle - V(0, w * 0.5, 0), axle + V(0, w * 0.5, 0), r * 0.93, r * 0.12)
            for index in range(TYRE_KNOBS):
                angle = index / TYRE_KNOBS * math.tau
                out = V(math.cos(angle), 0.0, math.sin(angle))
                d = np.minimum(d, sdf.box(P, axle + out * r * 0.93, (r * 0.11, w * 0.52, r * 0.07), sdf.rotation(pitch=math.degrees(math.pi / 2 - angle)), r * 0.02))
            return d
        add(bone + "_tyre", tyre, [axle], r * 1.1 + w, rubber, bone)
        add(bone + "_hub", lambda P, axle=axle: sdf.cylinder(P, axle - V(0, w * 0.56, 0), axle + V(0, w * 0.56, 0), r * 0.5, 0.5), [axle], r * 0.6 + w,
            glow, bone, 0.5)
        for sign in (1.0, -1.0):
            face = axle + V(0, sign * w * 0.58, 0)
            for spoke in range(3):
                add("%s_spoke_%d_%d" % (bone, sign > 0, spoke), lambda P, face=face, spoke=spoke: sdf.box(P, face, (r * 0.47, 0.6, r * 0.06), sdf.rotation(pitch=spoke * 60.0)),
                    [face], r * 0.6, frame, bone)
            add("%s_cap_%d" % (bone, sign > 0), lambda P, face=face: sdf.sphere(P, face, r * 0.16), [face], r * 0.2, metal, bone)
    engine = V(base * 0.02, 0, r * 1.3)
    add("engine", lambda P: sdf.box(P, engine, (base * 0.25, w * 0.75, r * 0.625), None, 1.5), [engine], base * 0.3 + r, frame)
    for sign in (1.0, -1.0):
        for k, (dz, length, height) in enumerate(((-0.18, 0.18, 0.045), (0.18, 0.13, 0.035))):
            c = engine + V(0, sign * w * 0.76, r * dz)
            add("engine_glow_%d_%d" % (sign > 0, k), lambda P, c=c, length=length, height=height: sdf.box(P, c, (base * length, 0.6, r * height)),
                [c], base * 0.2, glow, protect=0.5)
        top, low, back = V(base * 0.3, sign * w * 1.25, r * 1.95), V(base * 0.3, sign * w * 1.25, r * 0.85), V(base * 0.02, sign * w * 1.1, r * 0.75)
        rise = V(base * 0.18, sign * w * 0.6, r * 2.3)
        add("crash_bar_%d" % (sign > 0), lambda P, top=top, low=low, back=back, rise=rise: sdf.tube(P, [rise, top, low, back], [r * 0.09] * 4),
            [rise, top, low, back], r * 0.15, metal)
        a, b = V(base * 0.12, sign * w * 0.9, r * 1.05), V(-base * 0.55, sign * w * 1.2, r * 1.6)
        add("exhaust_%d" % (sign > 0), lambda P, a=a, b=b: sdf.round_cone(P, a, b, r * 0.15, r * 0.2), [a, b], r * 0.25, frame)
        add("exhaust_glow_%d" % (sign > 0), lambda P, b=b: sdf.sphere(P, b, r * 0.15), [b], r * 0.2, glow, protect=0.5)
        a, b = V(-base / 2, sign * w * 0.62, r), V(-base * 0.12, sign * w * 0.62, r * 1.1)
        add("swingarm_%d" % (sign > 0), lambda P, a=a, b=b: sdf.round_cone(P, a, b, r * 0.1, r * 0.12), [a, b], r * 0.15, frame)
    for lean_x in (-1.0, 1.0):
        start = engine + V(lean_x * base * 0.07, 0, r * 0.45)
        end = start + V(lean_x * base * 0.12, 0, r * 0.75)
        add("cylinder_%d" % (lean_x > 0), lambda P, start=start, end=end: sdf.round_cone(P, start, end, r * 0.3, r * 0.26), [start, end], r * 0.35, metal)
        a, b = start + V(lean_x * base * 0.1, 0, r * 0.62), start + V(lean_x * base * 0.13, 0, r * 0.8)
        add("cylinder_head_%d" % (lean_x > 0), lambda P, a=a, b=b: sdf.cylinder(P, a, b, r * 0.34, 1.0), [a, b], r * 0.4, frame)
    skid = V(base * 0.05, 0, r * 0.62)
    add("skid_plate", lambda P: sdf.box(P, skid, (base * 0.23, w * 0.7, 1.5)), [skid], base * 0.25 + w, metal)
    bars = V(grip_l[0], 0.0, grip_l[2])
    head_stock = bars + V(-r * 0.15, 0, -r * 0.5)
    spine = V(-base * 0.3, 0, seat_z - r * 0.15)
    add("spine", lambda P: sdf.capsule(P, spine, head_stock, r * 0.16), [spine, head_stock], r * 0.2, frame)
    tank = V(base * 0.16, 0, seat_z + r * 0.4)
    add("tank", lambda P: sdf.ellipsoid(P, tank, V(r * 1.36, r * 0.98, r * 0.6)), [tank], r * 1.5, paint, protect=0.4)
    for sign in (1.0, -1.0):
        c = tank + V(0, sign * r * 0.92, -r * 0.1)
        add("tank_plate_%d" % (sign > 0), lambda P, c=c: sdf.box(P, c, (r * 0.72, 1.3, r * 0.35), None, 0.5), [c], r * 0.8, plate)
        c = tank + V(0, sign * r * 0.99, -r * 0.1)
        add("tank_glow_%d" % (sign > 0), lambda P, c=c: sdf.box(P, c, (r * 0.6, 0.6, r * 0.045)), [c], r * 0.7, glow, protect=0.5)
    s = V(seat[0] - base * 0.06, 0, seat_z - 3.0)
    add("seat", lambda P: sdf.box(P, s, (base * 0.18, w * 0.67, 3.5), None, 2.0), [s], base * 0.2 + w, rubber)
    tail_end = V(-base / 2 - r * 1.05, 0, r * 1.7)
    rear = V(-base * 0.26, 0, seat_z - r * 0.05)
    add("rear_fender", slab(rear, tail_end, w * 1.5, 6.0), [rear, tail_end], w + 6.0, paint)
    light = tail_end + V(-1.5, 0, 0)
    add("tail_light", lambda P: sdf.box(P, light, (1.5, w * 0.45, r * 0.09)), [light], w, glow, protect=0.5)
    a, b = V(base / 2 + r * 0.8, 0, r * 1.75), V(base / 2 - r * 0.55, 0, r * 2.12)
    add("front_fender", slab(a, b, w * 1.3, 4.0), [a, b], w + 4.0, paint)
    for sign in (1.0, -1.0):
        top = head_stock + V(0, sign * w * 0.55, 0)
        axle = V(base / 2, sign * w * 0.55, r)
        mid = top + (axle - top) * 0.55
        add("fork_%d" % (sign > 0), lambda P, top=top, mid=mid: sdf.capsule(P, top, mid, r * 0.12), [top, mid], r * 0.15, metal)
        low = top + (axle - top) * 0.5
        add("fork_leg_%d" % (sign > 0), lambda P, low=low, axle=axle: sdf.round_cone(P, low, axle, r * 0.17, r * 0.15), [low, axle], r * 0.2, frame)
    grille = head_stock + V(r * 0.75, 0, -r * 0.2)
    add("grille", lambda P: sdf.box(P, grille, (r * 0.28, w * 0.95, r * 0.5), None, 1.0), [grille], w + r, frame)
    for sign in (1.0, -1.0):
        c = grille + V(-r * 0.1, sign * w * 0.95, 0)
        add("front_plate_%d" % (sign > 0), lambda P, c=c, sign=sign: sdf.box(P, c, (r * 0.35, 1.5, r * 0.58), sdf.rotation(yaw=-sign * math.degrees(0.35)), 0.6),
            [c], r * 0.8, paint)
    for index in (-1, 0, 1):
        c = grille + V(r * 0.28, 0, index * r * 0.26)
        add("grille_glow_%d" % (index + 1), lambda P, c=c: sdf.box(P, c, (0.8, w * 0.75, r * 0.06)), [c], w, glow, protect=0.5)
    add("riser", lambda P: sdf.capsule(P, head_stock, bars, r * 0.09), [head_stock, bars], r * 0.12, metal)
    add("bars", lambda P: sdf.capsule(P, grip_r, grip_l, r * 0.075), [grip_r, grip_l], r * 0.1, metal)
    return Union(parts, k=0.4)
