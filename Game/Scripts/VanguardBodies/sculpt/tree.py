"""A sculpt as a tree of distance nodes, each with world bounds, so a brick of space evaluates only the parts near it.
Every leaf is a Part: a named piece with its own label (to paint and skin by), its material and how it is skinned.

Evaluate a tree with evaluate(root, P, box): each node is evaluated once per call however many layers above read it
(a coat's shell reads the shirt and body beneath it, which the union above reads again)."""
import itertools
import math

import numpy as np

from . import sdf

# Where a culled node's distance is unknown, it is at least this far: beyond any band the mesher or baker resolves.
FAR = 1.0e3
# "form" builds the game mesh (detail parts left out); "detail" bakes textures (every part).
MODE = ["form"]
# How far beyond a box a node must lie to be skipped: the caller's band.
REACH = [1.0]
# The step a shell widens the reach by (cm).
REACH_STEP = 4.0
_CACHE = [{}]
# The worker processes open while a model builds (workers.Pool), which spread hands its work to. (Threads do not pay:
# a field is many small numpy calls, and Python holds them to one at a time.)
POOL = [None]


class Box:
    """An axis-aligned box (lo, hi) in centimetres."""
    __slots__ = ("lo", "hi")

    def __init__(self, lo, hi):
        self.lo = np.asarray(lo, dtype=np.float32)
        self.hi = np.asarray(hi, dtype=np.float32)

    @staticmethod
    def around(points, pad=0.0):
        P = np.asarray(points, dtype=np.float32).reshape(-1, 3)
        return Box(P.min(axis=0) - pad, P.max(axis=0) + pad)

    def grown(self, pad):
        return Box(self.lo - pad, self.hi + pad)

    def gap(self, other):
        outside = np.maximum(np.maximum(other.lo - self.hi, self.lo - other.hi), 0.0)
        return float(np.linalg.norm(outside))

    def union(self, other):
        return Box(np.minimum(self.lo, other.lo), np.maximum(self.hi, other.hi))

    def intersection(self, other):
        return Box(np.maximum(self.lo, other.lo), np.minimum(self.hi, other.hi))

    def corners(self):
        return np.array([[x, y, z] for x, y, z in itertools.product(*zip(self.lo, self.hi))], dtype=np.float32)


class Material:
    """How a surface is painted, at bake time: colour(ctx) -> (n, 3) linear colour; glow(ctx) -> (n,) in [0, 1]; and
    opacity(ctx) -> (n,) in [0, 1] for cloth with holes. ctx carries P, N, ao, cavity and the part labels."""

    def __init__(self, name, colour, glow=None, opacity=None, preview=(0.8, 0.8, 0.8)):
        self.name, self.colour, self.glow, self.opacity, self.preview = name, colour, glow, opacity, preview


class Part:
    """A leaf of the sculpt: distance(P) -> (n,), its bounds, its material, and its skinning: bones(P) -> {bone: (n,)}
    weights, or None to take the weights of the body beneath it. A detail part is baked into textures but left out
    of the game mesh."""

    def __init__(self, sculpt, name, distance, bounds, material, bones=None, detail=False, protect=0.0):
        self.label = len(sculpt.parts)
        self.name, self.distance, self.bounds, self.material = name, distance, bounds, material
        self.bones, self.detail, self.protect = bones, detail, protect
        sculpt.parts.append(self)


class Sculpt:
    def __init__(self):
        self.parts = []
        self.materials = {}

    def material(self, name, colour=None, glow=None, opacity=None, preview=(0.8, 0.8, 0.8)):
        self.materials[name] = Material(name, colour, glow, opacity, preview)
        return self.materials[name]


# ---------------------------------------------------------------------------------------------- evaluation
def evaluate(root, P, box):
    _CACHE[0] = {}
    try:
        return root.field(P, box)
    finally:
        _CACHE[0] = {}


def spread(fn, root, args_list):
    """fn(root, *args) for each of args_list, in order: in the worker processes while a pool is open that holds root
    (each worker its own, built as this one was; fn must be a module's own function), else here."""
    name = POOL[0].name_of(root) if POOL[0] is not None else None
    if name is not None:
        return POOL[0].map(fn, name, args_list)
    return [fn(root, *args) for args in args_list]


def in_cells(root, P, fn, cell):
    """fn(root, Q, box) over the points P grouped into cells cell cm wide, each group evaluating only the parts near
    it (box: around its points): a tuple of arrays over P, in P's order. A point's result depends only on its cell, so
    it is the same however many workers share the work."""
    keys = np.floor(P / cell).astype(np.int64)
    order = np.lexsort((keys[:, 2], keys[:, 1], keys[:, 0]))
    sk = keys[order]
    change = np.any(np.diff(sk, axis=0) != 0, axis=1)
    bounds = np.concatenate([[0], np.nonzero(change)[0] + 1, [len(P)]])
    groups = [order[a:b] for a, b in zip(bounds[:-1], bounds[1:])]
    outs = spread(fn, root, [(P[idx], Box.around(P[idx], 0.5)) for idx in groups])
    results = [np.empty((len(P),) + np.shape(o)[1:], dtype=np.asarray(o).dtype) for o in outs[0]]
    for idx, out in zip(groups, outs):
        for r, o in zip(results, out):
            r[idx] = o
    return results


def _far(n, gap):
    return sdf.Field(np.full(n, max(gap, REACH[0] * 2.0), dtype=np.float32), np.full(n, -1, dtype=np.int16))


class Node:
    bounds = None

    def field(self, P, box):
        """This node's field over P, evaluated once per evaluate(). Culling is its parent's call: only a parent knows
        how far its blend reaches (a child culled here but blended there would seam the bricks)."""
        # Keyed by the reach and mode too: a shell widens the reach to see its base's true distances farther out, and
        # reads its base as meshed.
        key = (id(self), REACH[0], MODE[0])
        cache = _CACHE[0]
        if key not in cache:
            field = self.eval(P, box)
            # Never nearer than the node's own bounds: approximate distances (an ellipsoid's) can read short far from
            # the surface, and a parent culls by the bounds, so both must agree or bricks seam.
            outside = np.linalg.norm(np.maximum(np.maximum(self.bounds.lo - P, P - self.bounds.hi), 0.0), axis=1)
            field.d = np.where(outside > 0.0, np.maximum(field.d, outside.astype(np.float32)), field.d)
            cache[key] = field
        return cache[key]

    def reach(self):
        """How far beyond its bounds this node's evaluation still matters (a blend's radius)."""
        return 0.0

    def eval(self, P, box):
        raise NotImplementedError


class Leaf(Node):
    def __init__(self, part):
        self.part = part
        self.bounds = part.bounds

    def eval(self, P, box):
        if self.part.detail and MODE[0] == "form":
            return _far(len(P), 0.0)
        return sdf.Field.of(self.part.distance(P).astype(np.float32), self.part.label)


class Union(Node):
    """Children fused, blending over k (0: a hard union); each point is labelled by the nearer surface."""

    def __init__(self, children, k=0.0):
        self.children = [child for child in children if child is not None]
        assert self.children, "an empty union"
        self.k = k
        bounds = self.children[0].bounds
        for child in self.children[1:]:
            bounds = bounds.union(child.bounds)
        self.bounds = bounds

    def reach(self):
        return self.k

    def eval(self, P, box):
        out = None
        for child in self.children:
            if child.bounds.gap(box) > REACH[0] + self.k * 1.5:
                continue
            field = child.field(P, box)
            out = field if out is None else sdf.union(out, field, self.k)
        return out if out is not None else _far(len(P), self.bounds.gap(box))


class Subtract(Node):
    """a with b carved from it, the cut blended over k; the cut faces take label (a's when None)."""

    def __init__(self, a, b, k=0.0, label=None):
        self.a, self.b, self.k, self.label = a, b, k, label
        self.bounds = a.bounds

    def eval(self, P, box):
        field = self.a.field(P, box)
        if self.b.bounds.gap(box) > REACH[0] + self.k * 1.5:
            return field
        cut = self.b.field(P, box)
        out = sdf.subtract(field, cut, self.k)
        if self.label is not None:
            out.m = np.where(-cut.d >= field.d, np.int16(self.label), out.m)
        return out


class Intersect(Node):
    """a kept only within b (b adds no label)."""

    def __init__(self, a, b, k=0.0):
        self.a, self.b, self.k = a, b, k
        self.bounds = a.bounds.intersection(b.bounds).grown(k)

    def eval(self, P, box):
        return sdf.intersect(self.a.field(P, box), self.b.field(P, box), self.k)


class Over(Node):
    """Layers laid one over another: a hard union in which a later layer's label wins wherever its surface is the
    outer one."""

    def __init__(self, layers):
        self.layers = [layer for layer in layers if layer is not None]
        bounds = self.layers[0].bounds
        for layer in self.layers[1:]:
            bounds = bounds.union(layer.bounds)
        self.bounds = bounds

    def eval(self, P, box):
        out = self.layers[0].field(P, box)
        for layer in self.layers[1:]:
            if layer.bounds.gap(box) > REACH[0]:
                continue
            out = sdf.over(out, layer.field(P, box))
        return out


class Shell(Node):
    """A garment: cloth whose outer face stands offset + thickness out from base's surface, kept within region (a
    zone), its cut edges rounded by hem. It is solid down into its base rather than a skin with air beneath it: a
    layered outfit is then one outer surface, where a gap thinner than the mesher's voxel would tunnel the mesh
    with handles and leave the layer beneath as hidden surface spending the triangle budget. Where its region cuts
    it, its edge stands as thick as offset + thickness. It is a part of its own (label, material, skinning by
    inheritance)."""

    def __init__(self, sculpt, name, base, offset, thickness, region, material, hem=0.15, bones=None, detail=False,
                 displace=None, reach=0.0, fine=None):
        self.base, self.offset, self.thickness, self.region, self.hem = base, offset, thickness, region, hem
        # displace(P): folds raising the outer face, meshed; fine(P): relief too small to mesh (stitching, quilting),
        # baked into the textures only. reach: the most either raises it, which its bounds must cover.
        self.displace, self.fine = displace, fine
        self.extra = reach
        self.part = Part(sculpt, name, None, None, material, bones, detail)
        self.bounds = base.bounds.grown(offset + thickness + reach).intersection(region.bounds.grown(hem))
        self.part.bounds = self.bounds

    def reach(self):
        return self.extra

    def eval(self, P, box):
        if self.part.detail and MODE[0] == "form":
            return _far(len(P), 0.0)
        # Its base out to the shell's own extent: a base culled nearer would answer a placeholder there, and a shell
        # standing off a placeholder would be a wall at every brick's edge.
        saved, mode = REACH[0], MODE[0]
        # Rounded up to a whole step, so the layers of a stack share one evaluation of what lies beneath them (each
        # distinct reach evaluates the stack afresh); a wider reach is only ever more exact.
        REACH[0] = saved + REACH_STEP * math.ceil((self.offset + self.thickness + self.extra) / REACH_STEP)
        # It rests on its base as meshed: relief baked into a layer beneath (quilting, a strap's stitching) does not
        # show through the cloth over it.
        MODE[0] = "form"
        try:
            under = self.base.field(P, box).d
        finally:
            REACH[0], MODE[0] = saved, mode
        # Folds raise its outer face only: it stays solid down into its base however high a fold stands.
        rise = self.displace(P) if self.displace is not None else 0.0
        if self.fine is not None and MODE[0] == "detail":
            rise = rise + self.fine(P)
        d = sdf.solid_layer(under, self.offset + self.thickness, rise)
        d = sdf.smax(d, self.region.field(P, box).d, self.hem)
        return sdf.Field.of(d.astype(np.float32), self.part.label)


class Nearer(Node):
    """A region: where a point lies nearer the surfaces of inside's nodes than of outside's, so a garment keeps to its
    limbs whatever the pose (a sleeve to its arm, never the hip that arm hangs by). Negative inside, about a distance
    across its border; no label of its own. A node beyond reach of the box counts by its bounds' distance."""

    def __init__(self, inside, outside, margin):
        self.inside, self.outside = list(inside), list(outside)
        bounds = self.inside[0].bounds
        for node in self.inside[1:]:
            bounds = bounds.union(node.bounds)
        self.margin = margin
        self.bounds = bounds.grown(margin)

    def _nearest(self, nodes, P, box):
        d = np.full(len(P), FAR, dtype=np.float32)
        for node in nodes:
            gap = node.bounds.gap(box)
            d = np.minimum(d, np.float32(gap) if gap > REACH[0] + self.margin else node.field(P, box).d)
        return d

    def eval(self, P, box):
        inside, outside = self._nearest(self.inside, P, box), self._nearest(self.outside, P, box)
        return sdf.Field((0.5 * (inside - outside)).astype(np.float32), np.full(len(P), -1, dtype=np.int16))


class Zone(Node):
    """A region for a garment or a cut: distance(P) negative inside. No label of its own."""

    def __init__(self, distance, bounds):
        self.distance, self.bounds = distance, bounds

    def eval(self, P, box):
        return sdf.Field(self.distance(P).astype(np.float32), np.full(len(P), -1, dtype=np.int16))


class Placed(Node):
    """child authored in a frame of its own, placed in the world: origin, and the frame's axes as world vectors (the
    columns of axes). Distances are kept (a rigid move)."""

    def __init__(self, child, origin, axes):
        self.child = child
        self.origin = np.asarray(origin, dtype=np.float32)
        self.axes = np.asarray(axes, dtype=np.float32)
        corners = child.bounds.corners() @ self.axes.T + self.origin
        self.bounds = Box.around(corners)

    def eval(self, P, box):
        Q = (P - self.origin) @ self.axes
        local_box = Box.around((box.corners() - self.origin) @ self.axes)
        saved = _CACHE[0]
        _CACHE[0] = {}
        try:
            return self.child.field(Q, local_box)
        finally:
            _CACHE[0] = saved

    def to_local(self, P):
        return (P - self.origin) @ self.axes


def leaf(sculpt, name, distance, bounds, material, bones=None, detail=False, protect=0.0):
    return Leaf(Part(sculpt, name, distance, bounds, material, bones, detail, protect))


def zone(distance, bounds):
    return Zone(distance, bounds)
