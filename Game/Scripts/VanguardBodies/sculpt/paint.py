"""How a sculpted surface is painted at bake time: a stylised material model of a base colour, a grain of value
noise, crevices darkened by occlusion and convex edges worn lighter. Colours are given as seen (sRGB) and painted in
linear light."""
import numpy as np

from . import sdf


def linear(srgb):
    c = np.asarray(srgb, dtype=np.float32)
    return np.where(c <= 0.04045, c / 12.92, ((c + 0.055) / 1.055) ** 2.4)


def painted(base, grain=0.08, scale=3.0, wear=0.12, cavity=0.55, seed=0, tint=None):
    """base (sRGB) varied by grain at feature size scale (cm), darkened toward cavity in occluded crevices, lightened
    by wear on convex edges (toward tint, sRGB, or a lighter base)."""
    base_l = linear(base)
    worn = linear(tint) if tint is not None else np.clip(base_l * 1.8 + 0.02, 0, 1)

    def colour(ctx):
        P, ao, convex = ctx["P"], ctx["ao"], ctx["convex"]
        n = sdf.fbm(P.astype(np.float32), scale, 3, seed) - 0.5
        c = base_l[None, :] * (1.0 + grain * 2.0 * n)[:, None]
        c = c * (cavity + (1.0 - cavity) * ao)[:, None]
        edge = wear * np.clip(convex, 0.0, 1.0)[:, None]
        c = c * (1.0 - edge) + worn[None, :] * edge
        return np.clip(c, 0.0, 1.0).astype(np.float32)
    return colour


def glowing(strength=1.0):
    return lambda ctx: np.full(len(ctx["P"]), strength, dtype=np.float32)


def inked(base, coverage, ink, strength=0.85):
    """base's colour with ink laid over it where coverage(P) (0 to 1) says: a tattoo, a painted mark."""
    ink_l = linear(ink)

    def colour(ctx):
        c = base(ctx)
        a = (np.clip(coverage(ctx["P"]), 0.0, 1.0) * strength)[:, None]
        return (c * (1.0 - a) + ink_l[None, :] * a).astype(np.float32)
    return colour