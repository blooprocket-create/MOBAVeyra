"""A painted face for a sculpted head (ADR-069): eyes, brows, lips and stubble painted onto the skin in the head's
own frame (anatomy.Head's template units), as a toon face is painted rather than sculpted. Shapes are drawn in the
front projection (y across, z up) where the face looks forward (x)."""
import numpy as np

from . import paint, sdf


def face(skin, frame, scale, look):
    """skin's colour with a face painted over it. frame: (origin, axes) of the head; scale: cm per template unit.
    look: eyeColour, browColour, lashColour, lipColour (sRGB), eyeTilt (template units the outer corner rises),
    stubble (0 none, 1 heavy), goatee (bool)."""
    origin, axes = np.asarray(frame[0], dtype=np.float64), np.asarray(frame[1], dtype=np.float64)
    eye_c = paint.linear(look.get("eyeColour", (0.3, 0.22, 0.12)))
    brow_c = paint.linear(look.get("browColour", (0.12, 0.08, 0.07)))
    lash_c = paint.linear(look.get("lashColour", (0.06, 0.04, 0.035)))
    lip_c = paint.linear(look.get("lipColour", (0.62, 0.38, 0.33)))
    white = paint.linear((0.86, 0.83, 0.8))
    tilt = look.get("eyeTilt", 0.25)
    stubble = look.get("stubble", 0.0)

    def colour(ctx):
        c = skin(ctx)
        Q = (ctx["P"].astype(np.float64) - origin) @ axes / scale
        x, y, z = Q[:, 0], Q[:, 1], Q[:, 2]
        front = x > 6.0
        out = c.copy()
        for sign in (1.0, -1.0):
            # An almond eye: its outer corner (away from the nose) a little higher.
            ey = (y - sign * 3.0) / 1.3
            lift = tilt * (sign * (y - sign * 3.0)) / 1.3
            ez = (z - 10.1 - lift) / 0.52
            almond = front & (ey * ey + ez * ez * (1.0 + 0.6 * np.abs(ey)) < 1.0)
            out[almond] = white
            iris = front & ((y - sign * 3.0) ** 2 + (z - 10.08) ** 2 < 0.46 ** 2) & almond
            out[iris] = eye_c
            pupil = front & ((y - sign * 3.0) ** 2 + (z - 10.08) ** 2 < 0.2 ** 2) & almond
            out[pupil] = lash_c * 0.6
            # The lash line along the upper lid, heavier at the outer corner; a thin one below.
            upper = front & (np.abs(ey) < 1.12) & (ez > 0.55) & (ez < 1.25 + 0.4 * np.clip(sign * ey, 0, 1))
            out[upper] = lash_c
            lower = front & (np.abs(ey) < 0.95) & (ez < -0.82) & (ez > -1.05)
            out[lower] = out[lower] * 0.55 + lash_c * 0.45
            # The brow: a stroke from beside the nose, rising and then turning down past the eye.
            by = sign * (y - sign * 0.9)
            brow_z = 11.45 + 0.28 * np.clip(by, 0, 3.0) - 0.18 * np.clip(by - 3.0, 0, 1.5)
            brow = front & (by > 0.0) & (by < 4.4) & (np.abs(z - brow_z) < 0.32 - 0.08 * np.clip(by / 4.4, 0, 1))
            out[brow] = brow_c
        lips = front & (np.abs(y) < 1.85 - 0.25 * np.abs(z - 3.35)) & (np.abs(z - 3.35) < 0.75)
        out[lips] = out[lips] * 0.5 + lip_c * 0.5
        if stubble > 0:
            # Stubble: the jaw, chin and upper lip speckled darker; the goatee heavier on the chin.
            speckle = sdf.fbm(Q.astype(np.float32) * 6.0, 1.0, 2, 17)
            jaw = (x > 1.0) & (z < 7.0) & (z > -0.5) & ~lips
            density = stubble * np.clip((7.0 - z) / 2.0, 0, 1) * (0.35 + 0.65 * (speckle > 0.5))
            goatee = (np.abs(y) < 1.1) & (z < 2.6) & (x > 5.5) & look.get("goatee", False)
            density = np.where(goatee, np.maximum(density, 0.85), density)
            dark = jaw[:, None] * density[:, None]
            out = out * (1 - 0.55 * dark) + brow_c[None, :] * 0.55 * dark
        return np.clip(out, 0, 1).astype(np.float32)
    return colour
