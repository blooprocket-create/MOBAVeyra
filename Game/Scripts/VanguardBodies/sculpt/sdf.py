"""Signed distance fields for sculpting characters in code: numpy-vectorised primitives over point arrays (N, 3), smooth
blends that carry a material per point, and value noise. A Field is (distance, material): negative inside."""
import math

import numpy as np


class Field:
    """Distances (N,) and the material index (N,) of the part nearest each point."""
    __slots__ = ("d", "m")

    def __init__(self, d, m):
        self.d = d
        self.m = m

    @staticmethod
    def of(d, material):
        return Field(d, np.full(d.shape, material, dtype=np.int16))


# ---------------------------------------------------------------------------------------------- frames
def frame(forward, up=(0.0, 0.0, 1.0)):
    """A rotation (3x3, its columns the local x, y, z axes) whose local z runs along forward, local x as near up as
    allowed."""
    z = np.asarray(forward, dtype=np.float64)
    z = z / max(np.linalg.norm(z), 1e-9)
    hint = np.asarray(up, dtype=np.float64)
    if abs(np.dot(hint, z)) > 0.95:
        hint = np.array([1.0, 0.0, 0.0]) if abs(z[0]) < 0.9 else np.array([0.0, 1.0, 0.0])
    x = hint - z * np.dot(hint, z)
    x /= np.linalg.norm(x)
    y = np.cross(z, x)
    return np.stack([x, y, z], axis=1)


def rotation(yaw=0.0, pitch=0.0, roll=0.0):
    """Degrees about z (yaw), then y (pitch), then x (roll), as a 3x3 whose columns are the local axes."""
    cy, sy = math.cos(math.radians(yaw)), math.sin(math.radians(yaw))
    cp, sp = math.cos(math.radians(pitch)), math.sin(math.radians(pitch))
    cr, sr = math.cos(math.radians(roll)), math.sin(math.radians(roll))
    rz = np.array([[cy, -sy, 0], [sy, cy, 0], [0, 0, 1]])
    ry = np.array([[cp, 0, sp], [0, 1, 0], [-sp, 0, cp]])
    rx = np.array([[1, 0, 0], [0, cr, -sr], [0, sr, cr]])
    return rz @ ry @ rx


def local(P, centre, axes=None):
    """P in a frame at centre whose axes are the columns of axes."""
    Q = P - np.asarray(centre, dtype=np.float32)
    return Q @ np.asarray(axes, dtype=np.float32) if axes is not None else Q


# ---------------------------------------------------------------------------------------------- primitives
def sphere(P, c, r):
    return np.linalg.norm(P - np.asarray(c, dtype=np.float32), axis=1) - r


def ellipsoid(P, c, radii, axes=None):
    """A near-exact distance to an ellipsoid (bounded, exact on the axes)."""
    Q = local(P, c, axes)
    r = np.asarray(radii, dtype=np.float32)
    k0 = np.linalg.norm(Q / r, axis=1)
    k1 = np.linalg.norm(Q / (r * r), axis=1)
    return k0 * (k0 - 1.0) / np.maximum(k1, 1e-6)


def round_cone(P, a, b, ra, rb):
    """A cone from a (radius ra) to b (radius rb) with round ends: limbs, digits, strands."""
    a = np.asarray(a, dtype=np.float32)
    b = np.asarray(b, dtype=np.float32)
    ba = b - a
    l2 = float(np.dot(ba, ba))
    rr = ra - rb
    a2 = l2 - rr * rr
    il2 = 1.0 / max(l2, 1e-9)
    pa = P - a
    y = pa @ ba
    z = y - l2
    x2v = pa * l2 - y[:, None] * ba
    x2 = np.einsum("ij,ij->i", x2v, x2v)
    y2 = y * y * l2
    z2 = z * z * l2
    k = math.copysign(1.0, rr) * rr * rr * x2
    out = (np.sqrt(x2 * a2 * il2) + y * rr) * il2 - ra
    first = np.sign(z) * a2 * z2 > k
    second = np.sign(y) * a2 * y2 < k
    out = np.where(first, np.sqrt(x2 + z2) * il2 - rb, out)
    out = np.where(second & ~first, np.sqrt(x2 + y2) * il2 - ra, out)
    return out


def capsule(P, a, b, r):
    return round_cone(P, a, b, r, r)


def box(P, c, half, axes=None, rounding=0.0):
    Q = np.abs(local(P, c, axes)) - (np.asarray(half, dtype=np.float32) - rounding)
    outside = np.linalg.norm(np.maximum(Q, 0.0), axis=1)
    inside = np.minimum(np.max(Q, axis=1), 0.0)
    return outside + inside - rounding


def torus(P, c, major, minor, axes=None):
    """A ring about local z."""
    Q = local(P, c, axes)
    q = np.stack([np.linalg.norm(Q[:, :2], axis=1) - major, Q[:, 2]], axis=1)
    return np.linalg.norm(q, axis=1) - minor


def slab(P, point, normal, half):
    """The space within half of a plane through point, across normal."""
    n = np.asarray(normal, dtype=np.float32)
    n = n / np.linalg.norm(n)
    return np.abs((P - np.asarray(point, dtype=np.float32)) @ n) - half


def half_space(P, point, normal):
    """Negative on the side normal points away from (inside), positive beyond."""
    n = np.asarray(normal, dtype=np.float32)
    n = n / np.linalg.norm(n)
    return (P - np.asarray(point, dtype=np.float32)) @ n


def tube(P, points, radii):
    """A tube swept through points, its radius easing between radii at each: strands, straps, tails (round cones
    chained, so its joins are smooth)."""
    d = None
    for i in range(len(points) - 1):
        piece = round_cone(P, points[i], points[i + 1], radii[i], radii[i + 1])
        d = piece if d is None else np.minimum(d, piece)
    return d


def bezier(p0, p1, p2, count):
    """count + 1 points along a quadratic Bezier curve."""
    p0, p1, p2 = (np.asarray(p, dtype=np.float64) for p in (p0, p1, p2))
    return [tuple((1 - t) ** 2 * p0 + 2 * (1 - t) * t * p1 + t * t * p2) for t in np.linspace(0.0, 1.0, count + 1)]


# ---------------------------------------------------------------------------------------------- blends
def smin(a, b, k):
    """A smooth minimum, cubic so its curvature runs on without a seam (C2). Its span is 1.5 k, so its deepest fillet
    is k / 4, as a quadratic blend of k makes."""
    if k <= 0:
        return np.minimum(a, b)
    span = k * 1.5
    h = np.maximum(span - np.abs(a - b), 0.0) / span
    return np.minimum(a, b) - h * h * h * span * (1.0 / 6.0)


def smax(a, b, k):
    return -smin(-a, -b, k)


def union(a, b, k=0.0):
    """Two fields fused, blending over k; each point keeps the material of the nearer surface."""
    return Field(smin(a.d, b.d, k), np.where(a.d <= b.d, a.m, b.m))


def subtract(a, b, k=0.0):
    """a with b carved away (its cut faces a's material)."""
    return Field(smax(a.d, -b.d, k), a.m)


def intersect(a, b, k=0.0):
    return Field(smax(a.d, b.d, k), a.m)


def over(a, b):
    """b laid over a: a's distances where b does not reach, b's material wherever b's surface is the outer one."""
    d = np.minimum(a.d, b.d)
    return Field(d, np.where(b.d <= a.d, b.m, a.m))



def solid_layer(d, out, rise=0.0):
    """What lies within out of the surface of d (out + rise on its outer side, rise an array of folds), from out deep
    inside it: a garment solid down into what it covers."""
    return np.maximum(d - out - rise, -d - out)


# ---------------------------------------------------------------------------------------------- noise
def _hash(ix, iy, iz, seed):
    h = (ix * 374761393 + iy * 668265263 + iz * 1274126177 + seed * 1442695041) & 0xFFFFFFFF
    h = ((h ^ (h >> 13)) * 1274126177) & 0xFFFFFFFF
    return ((h ^ (h >> 16)) & 0xFFFF).astype(np.float32) / 65535.0


def noise(P, scale, seed=0):
    """Smooth value noise in [0, 1] at a feature size of scale."""
    Q = P / scale
    i = np.floor(Q).astype(np.int64)
    f = Q - i
    u = f * f * (3.0 - 2.0 * f)
    ix, iy, iz = i[:, 0], i[:, 1], i[:, 2]
    out = 0.0
    for dx in (0, 1):
        for dy in (0, 1):
            for dz in (0, 1):
                w = (u[:, 0] if dx else 1 - u[:, 0]) * (u[:, 1] if dy else 1 - u[:, 1]) * (u[:, 2] if dz else 1 - u[:, 2])
                out = out + w * _hash(ix + dx, iy + dy, iz + dz, seed)
    return out


def fbm(P, scale, octaves=3, seed=0):
    total, amplitude, weight = 0.0, 1.0, 0.0
    for octave in range(octaves):
        total = total + noise(P, scale / (2 ** octave), seed + octave) * amplitude
        weight += amplitude
        amplitude *= 0.5
    return total / weight


# ---------------------------------------------------------------------------------------------- lofts
class Loft:
    """A body swept from a to b through elliptical cross-sections: stations (t, centre_u, centre_v, half_u, half_v)
    with t from 0 at a to 1 at b; u is up (as given, made square to the axis) and v completes the frame. Between
    stations the section eases along a Catmull-Rom spline, so torsos and limbs read as one continuous form. Its ends
    are flat, rounded by `cap`."""

    SAMPLES = 256

    def __init__(self, a, b, up, stations, cap=0.0):
        self.a = np.asarray(a, dtype=np.float64)
        b = np.asarray(b, dtype=np.float64)
        axis = b - self.a
        self.length = float(np.linalg.norm(axis))
        self.w = axis / self.length
        u = np.asarray(up, dtype=np.float64)
        u = u - self.w * (u @ self.w)
        self.u = u / np.linalg.norm(u)
        self.v = np.cross(self.w, self.u)
        self.cap = cap
        # A station's optional sixth value is its section's exponent: 2 an ellipse, more a squarer superellipse.
        stations = sorted(tuple(s) + (2.0,) * (6 - len(s)) for s in stations)
        t = np.array([s[0] for s in stations])
        values = np.array([s[1:] for s in stations])
        self.ts = np.linspace(0.0, 1.0, self.SAMPLES)
        self.curve = np.stack([_catmull(t, values[:, i], self.ts) for i in range(5)], axis=1)
        # How steeply the section's radius changes along the axis, per unit of length (to correct the distance).
        radius = self.curve[:, 2:4].max(axis=1)
        self.slope = np.gradient(radius, self.ts) / self.length

    def bounds_points(self):
        r = self.curve[:, 2:4].max() + np.abs(self.curve[:, 0:2]).max()
        return [self.a - r, self.a + r, self.a + self.w * self.length - r, self.a + self.w * self.length + r]

    def __call__(self, P):
        Q = P.astype(np.float64) - self.a
        s = Q @ self.w
        x = Q @ self.u
        y = Q @ self.v
        t = np.clip(s / self.length, 0.0, 1.0)
        idx = t * (self.SAMPLES - 1)
        cu = np.interp(idx, np.arange(self.SAMPLES), self.curve[:, 0])
        cv = np.interp(idx, np.arange(self.SAMPLES), self.curve[:, 1])
        ru = np.interp(idx, np.arange(self.SAMPLES), self.curve[:, 2])
        rv = np.interp(idx, np.arange(self.SAMPLES), self.curve[:, 3])
        n = np.interp(idx, np.arange(self.SAMPLES), self.curve[:, 4])
        slope = np.interp(idx, np.arange(self.SAMPLES), self.slope)
        ax, ay = np.abs(x - cu) / ru, np.abs(y - cv) / rv
        # A superellipse's level k0, and the distance its gradient puts to the section's edge.
        k0 = np.maximum((ax ** n + ay ** n) ** (1.0 / n), 1e-9)
        scale = k0 ** (1.0 - n)
        gx, gy = scale * ax ** (n - 1.0) / ru, scale * ay ** (n - 1.0) / rv
        e = (k0 - 1.0) / np.maximum(np.sqrt(gx * gx + gy * gy), 1e-9)
        # Far outside, the first-order estimate overshoots: blend toward the scaled radial distance.
        e = np.where(k0 > 1.5, np.minimum(e, (k0 - 1.0) * np.minimum(ru, rv)), e) / np.sqrt(1.0 + slope * slope)
        axial = np.maximum(-s, s - self.length) + self.cap
        e = e + self.cap
        outside = np.sqrt(np.maximum(e, 0.0) ** 2 + np.maximum(axial, 0.0) ** 2)
        return (outside + np.minimum(np.maximum(e, axial), 0.0) - self.cap).astype(np.float32)


def _catmull(t, values, samples):
    """A Catmull-Rom curve through (t, values), sampled at samples (ends held)."""
    t = np.asarray(t, dtype=np.float64)
    v = np.asarray(values, dtype=np.float64)
    if len(t) == 1:
        return np.full(len(samples), v[0])
    out = np.empty(len(samples))
    for n, x in enumerate(samples):
        i = int(np.clip(np.searchsorted(t, x, side="right") - 1, 0, len(t) - 2))
        t0, t1 = t[i], t[i + 1]
        p0, p1, p2, p3 = v[max(i - 1, 0)], v[i], v[i + 1], v[min(i + 2, len(v) - 1)]
        span = max(t1 - t0, 1e-9)
        m1 = (p2 - p0) / max(t[min(i + 1, len(t) - 1)] - t[max(i - 1, 0)], 1e-9) * span
        m2 = (p3 - p1) / max(t[min(i + 2, len(t) - 1)] - t[i], 1e-9) * span
        h = np.clip((x - t0) / span, 0.0, 1.0)
        h2, h3 = h * h, h * h * h
        out[n] = (2 * h3 - 3 * h2 + 1) * p1 + (h3 - 2 * h2 + h) * m1 + (-2 * h3 + 3 * h2) * p2 + (h3 - h2) * m2
    return out
