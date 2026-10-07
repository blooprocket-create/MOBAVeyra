"""A human figure sculpted on a humanoid layout (centimetres, facing +X, its left at +Y): torso, limbs, a head in a
frame of its own (so it can bow to a sight) and hands whose fingers curl around what they hold. Every skin part is
skinned to its bone; neighbouring parts blend their weights where their surfaces meet (skin.py).

Sizes scale with the figure's height (H), so a smaller or larger humanoid reuses the anatomy; a look dict shades it
(muscle, chest and hip shape) without naming anyone."""
import math

import numpy as np

from . import sdf, tree
from .tree import Box, Placed, Union


def V(*values):
    return np.asarray(values, dtype=np.float64)


def unit(v):
    v = np.asarray(v, dtype=np.float64)
    return v / max(np.linalg.norm(v), 1e-9)


def rigid(bone):
    return lambda P: {bone: np.ones(len(P), dtype=np.float32)}


def along(bone_a, bone_b, a, b, start=0.35, end=0.65):
    """Weights shifting from bone_a to bone_b along a to b, between start and end of the way."""
    a, b = V(*a), V(*b)
    ab = b - a

    def weights(P):
        t = np.clip(((P - a) @ ab) / max(ab @ ab, 1e-9), 0.0, 1.0)
        s = np.clip((t - start) / (end - start), 0.0, 1.0)
        s = s * s * (3 - 2 * s)
        return {bone_a: (1 - s).astype(np.float32), bone_b: s.astype(np.float32)}
    return weights


def spine_weights(L):
    """The torso's weights up its spine: each spine bone over its own length, blending at the joints."""
    chain = ["pelvis", "spine_01", "spine_02", "spine_03"]
    joints = [L[name][0][2] for name in chain] + [L["spine_03"][1][2]]

    def weights(P):
        z = P[:, 2]
        out = {}
        for index, name in enumerate(chain):
            lo, hi = joints[index], joints[index + 1]
            blend = (hi - lo) * 0.35
            rise = 1.0 if index == 0 else np.clip((z - (lo - blend)) / (2 * blend), 0, 1)
            fall = 1.0 if index == len(chain) - 1 else 1 - np.clip((z - (hi - blend)) / (2 * blend), 0, 1)
            out[name] = (rise * fall * np.ones(len(P))).astype(np.float32)
        return out
    return weights


class Figure:
    """The sculpted body: skin (a Node), its head frame, and its hands' frames. Built once per model."""

    def __init__(self, S, L, dims, skin, look):
        self.S, self.L, self.dims, self.skin_material, self.look = S, L, dims, skin, look
        self.H = dims["height"]
        self.parts = []
        self.head_parts = []

    # ------------------------------------------------------------------------------------------ helpers
    def p(self, name, i):
        return V(*self.L[name][i])

    def add(self, name, distance, bounds, bones, detail=False, protect=0.0, group=None):
        node = tree.leaf(self.S, name, distance, bounds, self.skin_material, bones, detail, protect)
        (self.parts if group is None else group).append(node)
        return node

    def ellipsoid(self, name, c, r, axes, bones, group=None, detail=False, protect=0.0):
        c = V(*c)
        rr = V(*r)
        return self.add(name, lambda P: sdf.ellipsoid(P, c, rr, axes), Box(c - rr.max(), c + rr.max()), bones, detail, protect, group)

    def cone(self, name, a, b, ra, rb, bones, group=None, detail=False, protect=0.0):
        a, b = V(*a), V(*b)
        return self.add(name, lambda P: sdf.round_cone(P, a, b, ra, rb), Box.around([a, b], max(ra, rb)), bones, detail, protect, group)

    # ------------------------------------------------------------------------------------------ build
    def build(self):
        L, H, look = self.L, self.H, self.look
        muscle = look.get("muscle", 0.6)
        self.pelvis_z = L["pelvis"][0][2]
        self.chest_z = L["spine_03"][1][2]
        self.torso = self.chest_z - self.pelvis_z
        self.shoulder_z = L["upperarm_l"][0][2]
        self.hip = L["thigh_l"][0][1]
        self.shoulder = L["upperarm_l"][0][1]
        self.torso_parts()
        for side in ("l", "r"):
            self.arm(side, muscle)
            self.leg(side, muscle)
        return self

    def loft(self, name, a, b, up, stations, bones, group, cap=0.0, protect=0.0):
        shape = sdf.Loft(a, b, up, stations, cap)
        return self.add(name, shape, Box.around(shape.bounds_points()), bones, False, protect, group)

    def torso_parts(self):
        """One loft up the spine, crotch to neck root, its sections a heroic V: hips, a narrow waist, a deep chest
        and broad shoulders. Pectorals, scapulae and glutes lie on it. Sizes are a 171 cm figure's, scaled."""
        H, S, hip, look = self.H, self.shoulder, self.hip, self.look
        pz, cz, T = self.pelvis_z, self.chest_z, self.torso
        h = lambda cm: H * cm / 171.0  # noqa: E731
        weights = spine_weights(self.L)
        wide, hips = look.get("chest", 1.0), look.get("hips", 1.0)
        bottom, top = pz - h(9.5), cz + h(5.0)
        span = top - bottom
        z = lambda value: (value - bottom) / span  # noqa: E731
        stations = [(z(bottom), h(-1.0), 0, h(8.0), h(9.5) * hips),
                    (z(pz - h(3.4)), h(-0.8), 0, h(9.6), (hip + h(3.4)) * hips),
                    (z(pz), h(-0.5), 0, h(10.4), (hip + h(5.2)) * hips),
                    (z(pz + h(8.5)), 0.0, 0, h(10.4), (hip + h(4.6)) * hips),
                    (z(pz + T * 0.30), h(0.4), 0, h(9.6), h(13.4)),
                    (z(pz + T * 0.52), h(0.8), 0, h(10.6), h(14.8) * wide),
                    (z(pz + T * 0.72), h(1.2), 0, h(11.6), h(17.0) * wide),
                    (z(pz + T * 0.87), h(0.6), 0, h(11.0), S * 0.77 * wide),
                    (z(cz), h(-0.2), 0, h(8.6), S * 0.58),
                    (z(top), h(0.6), 0, h(5.9), h(6.3))]
        group = []
        self.loft("torso", (0, 0, bottom), (0, 0, top), (1, 0, 0), stations, weights, group, cap=h(3.0))
        for sign in (1, -1):
            side = "l" if sign > 0 else "r"
            pec = sdf.rotation(roll=sign * -14.0)
            self.ellipsoid("pectoral", (h(8.6), sign * S * 0.36, cz - T * 0.21), (h(3.2), S * 0.4 * wide, h(5.2)), pec, weights, group)
            self.ellipsoid("scapula", (h(-7.6), sign * S * 0.38, cz - T * 0.2), (h(2.4), S * 0.3, h(6.4)), None, weights, group)
            self.ellipsoid("glute", (h(-5.6), sign * hip * 0.62, pz - h(1.0)), (h(5.4), h(6.8), h(7.4)), None, weights, group)
            self.cone("trapezius", (h(-1.4), sign * h(3.0), cz + h(3.5)), (h(-1.2), sign * S * 0.8, self.shoulder_z + h(2.6)),
                      h(4.4), h(3.0), along("spine_03", "clavicle_" + side, (0, 0, cz), (0, sign * S, self.shoulder_z)), group)
            self.cone("clavicle", (h(5.2), sign * h(2.0), cz - h(1.4)), (h(2.2), sign * S * 0.86, self.shoulder_z + h(1.8)), h(1.3), h(1.2),
                      rigid("clavicle_" + side), group)
        self.cone("neck", (0.0, 0, cz - h(2.0)), (h(1.6), 0, self.L["head"][0][2] + h(3.0)), h(5.9), h(5.3),
                  along("spine_03", "neck_01", (0, 0, cz - 4), (0, 0, self.L["head"][0][2]), 0.2, 0.6), group)
        self.torso_node = Union(group, k=h(2.6))
        self.parts.append(self.torso_node)

    def arm(self, side, muscle):
        H = self.H
        h = lambda cm: H * cm / 171.0  # noqa: E731
        s0, e = self.p("upperarm_" + side, 0), self.p("upperarm_" + side, 1)
        w = self.p("lowerarm_" + side, 1)
        upper, fore = unit(e - s0), unit(w - e)
        # The front of the upper arm faces the forearm's fold.
        front = fore - upper * (fore @ upper)
        front = unit(front) if np.linalg.norm(front) > 0.2 else V(1, 0, 0)
        bulk = 0.85 + muscle * 0.25
        group = []
        self.loft("upperarm", s0, e, front, [(0.0, 0, 0, h(5.0) * bulk, h(5.2) * bulk), (0.35, h(0.4), 0, h(4.5) * bulk, h(4.4) * bulk),
                                            (0.58, h(0.7), 0, h(4.4) * bulk, h(3.9) * bulk), (0.86, h(0.1), 0, h(3.5), h(3.4)),
                                            (1.0, 0, 0, h(3.3), h(3.3))], rigid("upperarm_" + side), group, cap=h(2.0))
        self.ellipsoid("deltoid", s0 + upper * h(3.2) + V(0, 0, h(0.6)), (h(5.6) * bulk, h(5.0) * bulk, h(7.6)), sdf.frame(upper, front),
                       along("clavicle_" + side, "upperarm_" + side, s0 - upper * 6, s0 + upper * 8, 0.3, 0.6), group)
        self.add("elbow", lambda P, c=e: sdf.sphere(P, c, h(3.3)), Box(e - h(4), e + h(4)), rigid("lowerarm_" + side), group=group)
        self.loft("forearm", e, w, front, [(0.0, 0, 0, h(3.4), h(3.7)), (0.22, h(0.3), h(0.2), h(3.9) * bulk, h(4.0) * bulk),
                                         (0.6, h(0.1), 0, h(2.9), h(3.1)), (1.0, 0, 0, h(2.2), h(2.6))], rigid("lowerarm_" + side), group, cap=h(1.5))
        self.arm_node = Union(group, k=h(2.2))
        self.parts.append(self.arm_node)
        setattr(self, "fore_" + side, (e, w, fore, front))

    def leg(self, side, muscle):
        H = self.H
        h = lambda cm: H * cm / 171.0  # noqa: E731
        hp, k = self.p("thigh_" + side, 0), self.p("thigh_" + side, 1)
        a = self.p("calf_" + side, 1)
        group = []
        self.loft("thigh", hp + V(0, 0, h(2.0)), k, (1, 0, 0), [(0.0, 0, 0, h(8.6), h(8.0)), (0.25, h(0.6), 0, h(8.4), h(7.9)),
                                                             (0.6, h(1.0), 0, h(7.4), h(6.9)), (0.9, h(0.4), 0, h(5.5), h(5.3)),
                                                             (1.0, h(0.3), 0, h(5.0), h(4.9))],
                  along("pelvis", "thigh_" + side, hp + V(0, 0, 8), hp - V(0, 0, 10), 0.2, 0.7), group, cap=h(3.0))
        self.add("knee", lambda P, c=k: sdf.sphere(P, c, h(4.7)), Box(k - h(6), k + h(6)), rigid("calf_" + side), group=group)
        self.ellipsoid("patella", k + V(h(3.6), 0, h(0.6)), (h(1.8), h(2.6), h(3.0)), None, rigid("calf_" + side), group)
        self.loft("calf", k, a, (1, 0, 0), [(0.0, h(0.3), 0, h(4.9), h(4.8)), (0.2, h(-1.5), 0, h(5.6), h(5.1)), (0.45, h(-1.0), 0, h(4.7), h(4.3)),
                                         (0.8, h(-0.2), 0, h(3.3), h(3.1)), (1.0, 0, 0, h(3.0), h(2.9))], rigid("calf_" + side), group, cap=h(2.0))
        f0, f1 = self.p("foot_" + side, 0), self.p("foot_" + side, 1)
        bones_foot = rigid("foot_" + side)
        self.ellipsoid("ankle", a + V(0, 0, h(0.6)), (h(3.0), h(2.7), h(3.2)), None, bones_foot, group)
        heel = V(f0[0] - h(2.0), f0[1], h(2.8))
        toe = V(f1[0] + h(1.7), f1[1], h(2.4))
        self.cone("foot", heel, toe, h(2.9), h(2.5), bones_foot, group)
        self.leg_node = Union(group, k=h(2.4))
        self.parts.append(self.leg_node)
    def body(self):
        """The skin of everything built so far (torso, limbs, and any head and hands added)."""
        return Union(self.parts, k=self.H * 0.016)


# ---------------------------------------------------------------------------------------------- head
class Head:
    """A head authored in its own frame: origin at the head joint, x forward, y left, z up; chin to crown spanning
    `size`. Shapes are fractions of size, so a face scales with the figure."""

    def __init__(self, S, skin, size, bones="head", look=None):
        self.S, self.skin, self.size, self.look = S, skin, size, look or {}
        self.bones = rigid(bones) if isinstance(bones, str) else bones
        self.nodes = []

    def e(self, name, c, r, axes=None, detail=False, protect=1.0):
        u = self.size / 24.0
        c, r = V(*c) * u, V(*r) * u
        node = tree.leaf(self.S, name, lambda P: sdf.ellipsoid(P, c, r, axes), Box(c - r.max(), c + r.max()), self.skin, self.bones, detail, protect)
        return node

    def c(self, name, a, b, ra, rb, detail=False, protect=1.0):
        u = self.size / 24.0
        a, b, ra, rb = V(*a) * u, V(*b) * u, ra * u, rb * u
        return tree.leaf(self.S, name, lambda P: sdf.round_cone(P, a, b, ra, rb), Box.around([a, b], max(ra, rb)), self.skin, self.bones, detail, protect)

    def build(self):
        """The head as one loft up its axis, chin to crown, its sections giving the jaw's taper, the cheeks, the brow
        and the skull; then the brow ridge, the cheekbones and the jaw's angles, the eye sockets, the nose, the lips and
        the ears. Sizes are a 24 cm head's centimetres, scaled. Returns a Node in the head frame."""
        look = self.look
        jaw = look.get("jaw", 1.0)
        u = self.size / 24.0
        bottom, top = 1.6, 26.0
        z = lambda value: (value - bottom) / (top - bottom)  # noqa: E731
        stations = [(z(1.6), 6.6, 0, 1.6, 1.8 * jaw, 2.2), (z(3.2), 6.0, 0, 3.0, 2.9 * jaw, 2.3), (z(5.4), 4.0, 0, 5.4, 4.4 * jaw, 2.5),
                    (z(8.0), 1.8, 0, 7.8, 5.6 * jaw, 2.7), (z(10.6), 0.7, 0, 9.1, 6.4, 2.8), (z(13.4), -0.1, 0, 9.7, 6.95, 2.8),
                    (z(16.0), -0.6, 0, 9.95, 7.3, 2.7), (z(19.5), -1.0, 0, 9.8, 7.45, 2.5), (z(22.6), -1.5, 0, 8.9, 7.0, 2.3),
                    (z(24.8), -1.9, 0, 6.6, 5.3, 2.1), (z(26.0), -2.1, 0, 3.2, 2.6, 2.0)]
        shape = sdf.Loft(V(0, 0, bottom) * u, V(0, 0, top) * u, (1, 0, 0), [(s[0], s[1] * u, s[2] * u, s[3] * u, s[4] * u, s[5]) for s in stations], 1.2 * u)
        skull = tree.leaf(self.S, "skull", shape, Box.around(shape.bounds_points()), self.skin, self.bones, False, 1.0)
        forms = [skull,
                 self.e("cheekbone_l", (5.0, 5.4, 12.3), (2.4, 1.2, 0.95)), self.e("cheekbone_r", (5.0, -5.4, 12.3), (2.4, 1.2, 0.95)),
                 self.c("brow_l", (8.85, 0.7, 15.75), (8.0, 4.4, 15.9), 0.75, 0.6),
                 self.c("brow_r", (8.85, -0.7, 15.75), (8.0, -4.4, 15.9), 0.75, 0.6),
                 self.e("chin_point", (8.0, 0, 3.4), (1.3, 1.9 * jaw, 1.4))]
        face = Union(forms, k=1.6 * u)
        sockets = Union([self.e("socket_l", (9.6, 3.2, 13.5), (1.6, 1.9, 1.15)), self.e("socket_r", (9.6, -3.2, 13.5), (1.6, 1.9, 1.15))])
        face = tree.Subtract(face, sockets, k=1.0 * u)
        features = [self.c("nose_bridge", (9.35, 0, 15.0), (11.05, 0, 10.75), 0.55, 0.85),
                    self.e("nose_tip", (10.9, 0, 10.4), (0.95, 0.9, 0.85)),
                    self.e("nostril_wing_l", (10.0, 1.1, 9.95), (0.7, 0.62, 0.6)), self.e("nostril_wing_r", (10.0, -1.1, 9.95), (0.7, 0.62, 0.6)),
                    self.e("lip_upper", (9.35, 0, 8.25), (0.75, 2.1, 0.5)),
                    self.e("lip_lower", (9.1, 0, 7.35), (0.75, 1.8, 0.5))]
        face = Union([face] + features, k=0.5 * u)
        cuts = Union([self.e("mouth_line", (9.95, 0, 7.8), (0.75, 2.05, 0.08)),
                      self.e("nostril_l", (10.2, 0.68, 9.45), (0.42, 0.3, 0.26)), self.e("nostril_r", (10.2, -0.68, 9.45), (0.42, 0.3, 0.26))])
        face = tree.Subtract(face, cuts, k=0.16 * u)
        ears = []
        for sign, side in ((1, "l"), (-1, "r")):
            axes = sdf.rotation(yaw=sign * 15.0, roll=sign * -6.0)
            ear = self.e("ear_" + side, (-1.0, sign * 6.95, 12.6), (2.2, 0.62, 3.1), axes)
            bowl = self.e("ear_bowl_" + side, (-0.7, sign * 7.55, 12.4), (1.45, 0.42, 2.15), axes)
            ears.append(tree.Subtract(ear, bowl, k=0.25 * u))
        return Union([face] + ears, k=0.8 * u)

# ---------------------------------------------------------------------------------------------- hands
FINGERS = {
    # name: (base across the knuckles as a share of half the palm's width, lengths of its three bones in cm at an
    # 18.5 cm hand, radius scale)
    "index": (0.78, (4.1, 2.5, 2.0), 1.0),
    "middle": (0.26, (4.5, 2.85, 2.1), 1.02),
    "ring": (-0.26, (4.2, 2.7, 2.0), 0.97),
    "little": (-0.76, (3.3, 2.0, 1.8), 0.85),
}


class Hand:
    """A hand authored in its own frame: origin at the wrist, x toward the knuckles, z out of the back of the hand,
    the thumb on +y for a right hand (-y for a left). curl: {finger: (base, middle, tip) degrees toward the palm},
    spread: {finger: degrees}, thumb: (reach across the palm, base, middle, tip) degrees."""

    def __init__(self, S, skin, side, length, bones, curl, spread=None, thumb=(40, 20, 25, 20)):
        self.S, self.skin, self.side, self.length = S, skin, side, length
        self.bones = rigid(bones)
        self.curl, self.spread, self.thumb = curl, spread or {}, thumb
        self.u = length / 18.5
        self.s = 1.0 if side == "r" else -1.0
        self.tips = {}

    def cone(self, name, a, b, ra, rb, detail=False):
        a, b = V(*a), V(*b)
        return tree.leaf(self.S, name, lambda P: sdf.round_cone(P, a, b, ra, rb), Box.around([a, b], max(ra, rb)), self.skin, self.bones, detail, 1.0)

    def chain(self, name, base, direction, bend_axis, lengths, radii, angles):
        """Bones from base along direction, each turned by its angle about bend_axis (toward the palm)."""
        nodes, point, d = [], base, unit(direction)
        axis = unit(bend_axis)
        joints = [point]
        for index, (length, angle) in enumerate(zip(lengths, angles)):
            d = _rotate(d, axis, math.radians(angle))
            nxt = point + d * length
            nodes.append(self.cone("%s_%d" % (name, index), point, nxt, radii[index], radii[index + 1]))
            point = nxt
            joints.append(point)
        self.tips[name] = joints
        return nodes

    def build(self):
        u, s = self.u, self.s
        nodes = []
        palm = tree.leaf(self.S, "palm", lambda P: sdf.box(P, V(5.0, 0, 0) * u, V(4.6, 3.9, 1.25) * u, None, 1.1 * u),
                         Box(V(0, -4, -1.5) * u, V(10, 4, 1.5) * u), self.skin, self.bones, False, 1.0)
        thenar_c, thenar_r = V(3.4, s * 2.6, -0.55) * u, V(2.6, 1.6, 1.3) * u
        thenar = tree.leaf(self.S, "thenar", lambda P: sdf.ellipsoid(P, thenar_c, thenar_r), Box(thenar_c - 3 * u, thenar_c + 3 * u),
                           self.skin, self.bones, False, 1.0)
        hypo_c, hypo_r = V(3.8, -s * 2.8, -0.4) * u, V(3.0, 1.2, 1.1) * u
        hypothenar = tree.leaf(self.S, "hypothenar", lambda P: sdf.ellipsoid(P, hypo_c, hypo_r), Box(hypo_c - 3 * u, hypo_c + 3 * u),
                               self.skin, self.bones, False, 1.0)
        wrist_c, wrist_r = V(0.4, 0, 0) * u, V(1.6, 2.9, 1.9) * u
        wrist = tree.leaf(self.S, "wrist", lambda P: sdf.ellipsoid(P, wrist_c, wrist_r), Box(wrist_c - 3 * u, wrist_c + 3 * u),
                          self.skin, self.bones, False, 1.0)
        nodes += [palm, thenar, hypothenar, wrist]
        for finger, (across, lengths, scale) in FINGERS.items():
            base = V(9.4 - abs(across) * 0.9, s * across * 3.9, 0.15) * u
            spread = math.radians(self.spread.get(finger, across * -6.0) * s)
            direction = V(math.cos(spread), math.sin(spread), 0.0)
            bend = np.cross(direction, V(0, 0, 1)) * -1.0
            radii = [r * u * scale for r in (0.98, 0.86, 0.76, 0.66)]
            nodes += self.chain(finger, base, direction, bend, [l * u for l in lengths], radii, self.curl.get(finger, (10, 15, 10)))
        reach, t0, t1, t2 = self.thumb
        base = V(2.2, s * 2.4, -0.6) * u
        direction = unit(V(math.cos(math.radians(reach)), s * math.sin(math.radians(reach)), -0.45))
        bend = unit(np.cross(direction, V(0, 0, -1)) * s)
        nodes += self.chain("thumb", base, direction, bend, [4.1 * u, 3.1 * u, 2.6 * u], [1.3 * u, 1.12 * u, 0.98 * u, 0.82 * u], (t0, t1, t2))
        return Union(nodes, k=0.55 * u)


def _rotate(v, axis, angle):
    """v turned by angle about axis (Rodrigues)."""
    return v * math.cos(angle) + np.cross(axis, v) * math.sin(angle) + axis * (axis @ v) * (1 - math.cos(angle))
