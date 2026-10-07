"""Colour helpers for flat-coloured models: colours are given as seen (sRGB) and stored in linear light."""
import numpy as np


def linear(srgb):
    c = np.asarray(srgb, dtype=np.float32)
    return np.where(c <= 0.04045, c / 12.92, ((c + 0.055) / 1.055) ** 2.4)


def hashed(x, salt):
    """A repeatable value in [0, 1) for each whole x: a strip's own length, a lock's own twist."""
    return np.abs(np.modf(np.sin(np.asarray(x, dtype=np.float64) * 12.9898 + salt * 78.233) * 43758.5453)[0])
