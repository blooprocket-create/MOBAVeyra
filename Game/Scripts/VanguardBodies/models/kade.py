"""Kade, Dead Reckoning (ADR-069): a marksman caught mid-sight. Character Bible §2 and the author's reference
(2026-10-06): messy windswept brown hair and stubble; a pale cream shirt, sleeves rolled; a dark leather coat with a
high collar, segmented plate, many belts and buckled straps; articulated bracers and fingerless gloves; a line-work
tattoo down one forearm; trousers bound with thigh straps; heavy buckled boots; a long tattered crimson cloak wrapped
at the shoulders. His rifle is long and ornate, dark steel and brass, teal channels glowing its length and a large
round optic with a teal lens: the only cool note.

Proportions are the kit's (legShare, hipShare, shoulderShare, armShare, headShare: measured from the reference at 7.95
pixels a centimetre for his 171); colours are sampled from the reference (sRGB)."""
import numpy as np

from ..sculpt import anatomy, paint, sdf
from ..sculpt.anatomy import V, unit
from ..sculpt.tree import Placed

# The skull, chin to crown (cm); the hair rises above it to the top of the head bone.
SKULL = 20.6
# The head bows to the optic: pitched forward and rolled toward the stock, about the head joint (degrees).
HEAD_PITCH, HEAD_ROLL = 24.0, 16.0
# The hand, wrist to fingertip, as a share of the height.
HAND_SHARE = 0.108

# Sampled from the reference (sRGB).
PALETTE = {
    "skin": (0.78, 0.54, 0.45), "shirt": (0.86, 0.78, 0.75), "leather": (0.15, 0.12, 0.11), "belt": (0.22, 0.15, 0.13),
    "hair": (0.26, 0.18, 0.17), "teal": (0.1, 0.9, 0.85), "cloak": (0.62, 0.06, 0.1), "brass": (0.62, 0.47, 0.3),
    "steel": (0.2, 0.19, 0.2), "trousers": (0.24, 0.19, 0.18), "boot": (0.28, 0.22, 0.2), "glove": (0.2, 0.15, 0.13),
}


def materials(S):
    """Every material Kade is painted in: its colour as the reference shows it, its grain and its wear."""
    m = {}
    m["skin"] = S.material("skin", paint.painted(PALETTE["skin"], grain=0.04, scale=2.0, wear=0.0, cavity=0.75), preview=PALETTE["skin"])
    m["shirt"] = S.material("shirt", paint.painted(PALETTE["shirt"], grain=0.06, scale=1.5, wear=0.05, cavity=0.6), preview=PALETTE["shirt"])
    m["leather"] = S.material("leather", paint.painted(PALETTE["leather"], grain=0.18, scale=1.2, wear=0.25, cavity=0.5), preview=PALETTE["leather"])
    m["belt"] = S.material("belt", paint.painted(PALETTE["belt"], grain=0.15, scale=1.0, wear=0.3, cavity=0.5), preview=PALETTE["belt"])
    m["brass"] = S.material("brass", paint.painted(PALETTE["brass"], grain=0.1, scale=0.6, wear=0.4, cavity=0.45, tint=(0.95, 0.82, 0.55)), preview=PALETTE["brass"])
    m["hair"] = S.material("hair", paint.painted(PALETTE["hair"], grain=0.2, scale=0.8, wear=0.1, cavity=0.55), preview=PALETTE["hair"])
    m["glove"] = S.material("glove", paint.painted(PALETTE["glove"], grain=0.15, scale=0.8, wear=0.2, cavity=0.5), preview=PALETTE["glove"])
    return m


def build(S, L, dims, spec):
    """Kade's sculpt on his layout: (the whole, {"body": the skin alone, under the garments})."""
    mats = materials(S)
    H = dims["height"]
    figure = anatomy.Figure(S, L, dims, mats["skin"], {"muscle": 0.65, "chest": 1.0}).build()
    head_origin = V(*L["head"][0])
    head_axes = sdf.rotation(pitch=HEAD_PITCH) @ sdf.rotation(roll=HEAD_ROLL)
    figure.parts.append(Placed(anatomy.Head(S, mats["skin"], SKULL).build(), head_origin, head_axes))
    for side in ("l", "r"):
        wrist = V(*L["hand_" + side][0])
        if side == "r":
            # The grip hand: fingers wrapped around the grip under the bore, the index along the guard.
            hand = anatomy.Hand(S, mats["skin"], side, H * HAND_SHARE, "hand_r",
                                curl={"index": (25, 35, 15), "middle": (60, 95, 45), "ring": (65, 95, 45), "little": (70, 95, 40)},
                                thumb=(55, 25, 30, 20))
            x, z = unit(V(0.55, 0, -0.83)), unit(V(0, -1, 0))
        else:
            # The support hand: palm up under the fore-end, fingers curled around it.
            hand = anatomy.Hand(S, mats["skin"], side, H * HAND_SHARE, "hand_l",
                                curl={"index": (35, 60, 30), "middle": (40, 65, 30), "ring": (42, 65, 30), "little": (45, 65, 30)},
                                thumb=(35, 15, 20, 15))
            x, z = unit(V(1, -0.25, 0.1)), unit(V(0, 0.2, -1))
        y = np.cross(z, x)
        figure.parts.append(Placed(hand.build(), wrist, np.stack([x, y, z], axis=1)))
    body = figure.body()
    return body, {"body": body}
