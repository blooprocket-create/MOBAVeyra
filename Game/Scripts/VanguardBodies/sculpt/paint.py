"""Colour helpers for flat-coloured models: colours are given as seen (sRGB) and stored in linear light."""
import numpy as np


def linear(srgb):
    c = np.asarray(srgb, dtype=np.float32)
    return np.where(c <= 0.04045, c / 12.92, ((c + 0.055) / 1.055) ** 2.4)


def seen(kit):
    """A kit colour as seen (sRGB), which a material takes: the kit's colours are in linear light, as the generated
    bodies write them straight to their vertex colours, so a model that takes one shows it as they did."""
    c = np.clip(np.asarray(kit, dtype=np.float64), 0.0, 1.0)
    return np.where(c <= 0.0031308, c * 12.92, 1.055 * c ** (1.0 / 2.4) - 0.055)


def hashed(x, salt):
    """A repeatable value in [0, 1) for each whole x: a strip's own length, a lock's own twist."""
    return np.abs(np.modf(np.sin(np.asarray(x, dtype=np.float64) * 12.9898 + salt * 78.233) * 43758.5453)[0])
