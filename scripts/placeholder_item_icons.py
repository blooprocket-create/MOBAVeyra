#!/usr/bin/env python3
"""Draws placeholder icons for the items the author has not drawn yet (ADR-025 §8.9).

Each icon is ConceptArt/Items/<tier folder>/T_<id>_Icon.png, 256 x 256 px, opaque RGB on the
dark charcoal ground the author's icons use. The main glyph shows the item's identity; up to four
small glyphs along the bottom show its other stats. There is no text, border or other UI (see
ConceptArt/Items/README.md). The author's art replaces a placeholder file for file; then remove its
entry from PLACEHOLDERS.

Usage: py -3 scripts/placeholder_item_icons.py [--only id ...]
Then import them: Game/Scripts/BuildIconArt.ps1 -Kind Items -Ids <ids>
"""

import argparse
import math
import pathlib
import sys

from PIL import Image, ImageDraw, ImageFilter

ROOT = pathlib.Path(__file__).resolve().parent.parent
ART = ROOT / "ConceptArt" / "Items"

# Drawn at 4x and reduced, for smooth edges.
SIZE = 256
SCALE = 4
CANVAS = SIZE * SCALE
GROUND = (27, 28, 32)

# Each glyph's colour follows the stat it stands for.
COLOURS = {
    "blade": (224, 118, 58),
    "blades": (236, 96, 48),
    "shield": (143, 163, 184),
    "shields": (160, 178, 196),
    "crystal": (154, 123, 224),
    "crystals": (170, 136, 240),
    "heart": (208, 72, 79),
    "droplet": (88, 192, 122),
    "droplets": (72, 206, 128),
    "chevrons": (232, 194, 74),
    "star": (242, 213, 138),
    "coil": (76, 195, 217),
    "tower": (150, 170, 190),
    "bell": (120, 150, 230),
    "anchor": (80, 190, 180),
    "bow": (220, 90, 70),
    "vortex": (90, 200, 220),
    "vessel": (70, 190, 200),
    "harbor_crown": (236, 196, 96),
}

# Tier sets how strongly the ground glows.
GLOW = {1: 0.18, 2: 0.28, 3: 0.40, 4: 0.55}

# id: (tier folder, tier, main glyph, stat glyphs)
PLACEHOLDERS = {
    "warforged_grip": ("Tier_1_Components", 1, "blade", []),
    "titansteel_grip": ("Tier_1_Components", 1, "blades", []),
    "marchplate": ("Tier_1_Components", 1, "shield", []),
    "shatterdeep_crystal": ("Tier_1_Components", 1, "crystal", []),
    "picket_plating": ("Tier_2_Assemblies", 2, "shield", ["heart"]),
    "canyonward": ("Tier_2_Assemblies", 2, "crystal", ["heart"]),
    "breaker_aegis": ("Tier_2_Assemblies", 2, "shield", ["crystal", "coil"]),
    "resonant_wardstone": ("Tier_2_Assemblies", 2, "crystals", []),
    "foundation_plate": ("Tier_2_Assemblies", 2, "shields", []),
    "waymark_weave": ("Tier_2_Assemblies", 2, "droplets", []),
    "rescue_rig": ("Tier_2_Assemblies", 2, "heart", ["chevrons"]),
    "killstring_assembly": ("Tier_2_Assemblies", 2, "chevrons", ["star"]),
    "riverhold_bastion": ("Tier_3_Masterworks", 3, "tower", ["shield", "heart"]),
    "blackreef_bell": ("Tier_3_Masterworks", 3, "bell", ["crystal", "heart"]),
    "harborline_harness": ("Tier_3_Masterworks", 3, "anchor", ["blade", "chevrons", "droplet"]),
    "doombringer_bow": ("Tier_3_Masterworks", 3, "bow", ["blade", "chevrons", "star"]),
    "flux_reclaimer": ("Quest_Items", 2, "vortex", ["heart", "droplet"]),
    "wayline_reservoir": ("Quest_Items", 3, "vessel", ["heart", "droplet"]),
    "the_last_harbor": ("Tier_4_Mythicals", 4, "harbor_crown", ["blade", "chevrons", "heart", "droplet"]),
    # The Item Bible's 2026-10-02 Echo line (ADR-050).
    "echo_lens": ("Tier_3_Masterworks", 3, "crystals", ["heart", "coil"]),
    "the_second_self": ("Tier_4_Mythicals", 4, "vortex", ["crystal", "heart", "coil"]),
}


def shade(colour, factor):
    return tuple(max(0, min(255, int(c * factor))) for c in colour)


def blade(d, cx, cy, s, colour):
    # A sword, point up: blade, crossguard, grip, pommel.
    w = s * 0.11
    d.polygon([(cx, cy - s * 0.5), (cx + w, cy - s * 0.32), (cx + w, cy + s * 0.18), (cx - w, cy + s * 0.18), (cx - w, cy - s * 0.32)], fill=colour)
    d.polygon([(cx, cy - s * 0.5), (cx + w, cy - s * 0.32), (cx + w, cy + s * 0.18), (cx, cy + s * 0.18)], fill=shade(colour, 1.25))
    d.rounded_rectangle([cx - s * 0.3, cy + s * 0.18, cx + s * 0.3, cy + s * 0.26], radius=s * 0.03, fill=shade(colour, 0.8))
    d.rectangle([cx - s * 0.05, cy + s * 0.26, cx + s * 0.05, cy + s * 0.42], fill=shade(colour, 0.6))
    d.ellipse([cx - s * 0.08, cy + s * 0.40, cx + s * 0.08, cy + s * 0.52], fill=shade(colour, 0.9))


def blades(d, cx, cy, s, colour):
    # Two crossed swords.
    layer = Image.new("RGBA", (CANVAS, CANVAS), (0, 0, 0, 0))
    ld = ImageDraw.Draw(layer)
    blade(ld, CANVAS / 2, CANVAS / 2, s, colour)
    left = layer.rotate(35, center=(CANVAS / 2, CANVAS / 2), resample=Image.BICUBIC)
    right = layer.rotate(-35, center=(CANVAS / 2, CANVAS / 2), resample=Image.BICUBIC)
    offset = (int(cx - CANVAS / 2), int(cy - CANVAS / 2))
    d._image.paste(left, offset, left)
    d._image.paste(right, offset, right)


def shield(d, cx, cy, s, colour):
    top, bottom, half = cy - s * 0.42, cy + s * 0.48, s * 0.36
    outline = [(cx - half, top), (cx + half, top), (cx + half, cy + s * 0.05), (cx, bottom), (cx - half, cy + s * 0.05)]
    d.polygon(outline, fill=shade(colour, 0.7))
    inset = s * 0.06
    inner = [(cx - half + inset, top + inset), (cx + half - inset, top + inset), (cx + half - inset, cy + s * 0.04), (cx, bottom - inset * 1.6), (cx - half + inset, cy + s * 0.04)]
    d.polygon(inner, fill=colour)
    d.polygon([(cx, top + inset), (cx + half - inset, top + inset), (cx + half - inset, cy + s * 0.04), (cx, bottom - inset * 1.6)], fill=shade(colour, 1.2))


def shields(d, cx, cy, s, colour):
    shield(d, cx - s * 0.16, cy - s * 0.06, s * 0.82, shade(colour, 0.75))
    shield(d, cx + s * 0.12, cy + s * 0.06, s * 0.9, colour)


def crystal(d, cx, cy, s, colour):
    w, h = s * 0.26, s * 0.5
    points = [(cx, cy - h), (cx + w, cy - h * 0.45), (cx + w, cy + h * 0.45), (cx, cy + h), (cx - w, cy + h * 0.45), (cx - w, cy - h * 0.45)]
    d.polygon(points, fill=colour)
    d.polygon([(cx, cy - h), (cx + w, cy - h * 0.45), (cx + w, cy + h * 0.45), (cx, cy + h)], fill=shade(colour, 1.3))
    d.polygon([(cx - w * 0.35, cy - h * 0.3), (cx, cy - h * 0.7), (cx + w * 0.35, cy - h * 0.3), (cx, cy + h * 0.5)], fill=shade(colour, 1.55))


def crystals(d, cx, cy, s, colour):
    crystal(d, cx - s * 0.26, cy + s * 0.1, s * 0.62, shade(colour, 0.75))
    crystal(d, cx + s * 0.26, cy + s * 0.1, s * 0.62, shade(colour, 0.75))
    crystal(d, cx, cy - s * 0.02, s * 0.92, colour)


def heart(d, cx, cy, s, colour):
    r = s * 0.22
    d.ellipse([cx - r * 2, cy - r * 1.6, cx, cy + r * 0.4], fill=colour)
    d.ellipse([cx, cy - r * 1.6, cx + r * 2, cy + r * 0.4], fill=colour)
    d.polygon([(cx - r * 1.93, cy - r * 0.3), (cx + r * 1.93, cy - r * 0.3), (cx, cy + r * 2.1)], fill=colour)
    d.ellipse([cx + r * 0.45, cy - r * 1.15, cx + r * 1.15, cy - r * 0.45], fill=shade(colour, 1.5))


def droplet(d, cx, cy, s, colour):
    r = s * 0.3
    d.ellipse([cx - r, cy - r * 0.2, cx + r, cy + r * 1.8], fill=colour)
    d.polygon([(cx, cy - s * 0.5), (cx + r * 0.97, cy + r * 0.6), (cx - r * 0.97, cy + r * 0.6)], fill=colour)
    d.ellipse([cx - r * 0.55, cy + r * 0.35, cx - r * 0.15, cy + r * 0.95], fill=shade(colour, 1.5))


def droplets(d, cx, cy, s, colour):
    droplet(d, cx - s * 0.2, cy - s * 0.08, s * 0.72, shade(colour, 0.75))
    droplet(d, cx + s * 0.16, cy + s * 0.04, s * 0.8, colour)


def chevrons(d, cx, cy, s, colour):
    for i, factor in enumerate((0.7, 0.9, 1.15)):
        y = cy - s * 0.32 + i * s * 0.28
        w, t = s * 0.42, s * 0.14
        d.polygon([(cx - w, y + s * 0.2), (cx, y - s * 0.06), (cx + w, y + s * 0.2), (cx + w, y + s * 0.2 + t), (cx, y - s * 0.06 + t), (cx - w, y + s * 0.2 + t)], fill=shade(colour, factor))


def star(d, cx, cy, s, colour):
    points = []
    for i in range(8):
        radius = s * (0.5 if i % 2 == 0 else 0.14)
        angle = math.pi / 4 * i - math.pi / 2
        points.append((cx + radius * math.cos(angle), cy + radius * math.sin(angle)))
    d.polygon(points, fill=colour)
    d.ellipse([cx - s * 0.08, cy - s * 0.08, cx + s * 0.08, cy + s * 0.08], fill=shade(colour, 1.2))


def coil(d, cx, cy, s, colour):
    for i, radius in enumerate((0.46, 0.32, 0.18)):
        r = s * radius
        d.arc([cx - r, cy - r, cx + r, cy + r], start=-60 + i * 40, end=240 + i * 40, fill=shade(colour, 0.8 + i * 0.2), width=int(s * 0.07))


def tower(d, cx, cy, s, colour):
    w = s * 0.3
    d.rectangle([cx - w, cy - s * 0.3, cx + w, cy + s * 0.5], fill=colour)
    d.rectangle([cx, cy - s * 0.3, cx + w, cy + s * 0.5], fill=shade(colour, 1.2))
    for i in range(3):
        x = cx - w + i * w * 0.8
        d.rectangle([x, cy - s * 0.46, x + w * 0.4, cy - s * 0.3], fill=colour)
    d.rounded_rectangle([cx - w * 0.3, cy + s * 0.18, cx + w * 0.3, cy + s * 0.5], radius=w * 0.3, fill=GROUND)
    d.rectangle([cx - w * 1.4, cy + s * 0.44, cx + w * 1.4, cy + s * 0.52], fill=shade(colour, 0.6))


def bell(d, cx, cy, s, colour):
    d.pieslice([cx - s * 0.3, cy - s * 0.46, cx + s * 0.3, cy + s * 0.14], start=180, end=360, fill=colour)
    d.polygon([(cx - s * 0.3, cy - s * 0.16), (cx + s * 0.3, cy - s * 0.16), (cx + s * 0.44, cy + s * 0.3), (cx - s * 0.44, cy + s * 0.3)], fill=colour)
    d.polygon([(cx, cy - s * 0.46), (cx + s * 0.3, cy - s * 0.16), (cx + s * 0.44, cy + s * 0.3), (cx, cy + s * 0.3)], fill=shade(colour, 1.2))
    d.rounded_rectangle([cx - s * 0.5, cy + s * 0.28, cx + s * 0.5, cy + s * 0.36], radius=s * 0.03, fill=shade(colour, 0.8))
    d.ellipse([cx - s * 0.08, cy + s * 0.36, cx + s * 0.08, cy + s * 0.5], fill=shade(colour, 1.3))
    d.ellipse([cx - s * 0.07, cy - s * 0.56, cx + s * 0.07, cy - s * 0.44], outline=colour, width=int(s * 0.04))


def anchor(d, cx, cy, s, colour):
    t = int(s * 0.08)
    d.ellipse([cx - s * 0.1, cy - s * 0.52, cx + s * 0.1, cy - s * 0.32], outline=colour, width=t)
    d.line([(cx, cy - s * 0.32), (cx, cy + s * 0.42)], fill=colour, width=t)
    d.line([(cx - s * 0.24, cy - s * 0.2), (cx + s * 0.24, cy - s * 0.2)], fill=colour, width=t)
    d.arc([cx - s * 0.42, cy - s * 0.1, cx + s * 0.42, cy + s * 0.46], start=15, end=165, fill=shade(colour, 1.2), width=t)
    for side in (-1, 1):
        x = cx + side * s * 0.4
        d.polygon([(x, cy + s * 0.08), (x - side * s * 0.02, cy + s * 0.28), (x + side * s * 0.12, cy + s * 0.2)], fill=shade(colour, 1.2))


def bow(d, cx, cy, s, colour):
    t = int(s * 0.08)
    d.arc([cx - s * 0.5, cy - s * 0.5, cx + s * 0.3, cy + s * 0.5], start=-80, end=80, fill=colour, width=t)
    top = (cx - s * 0.1 + s * 0.4 * math.cos(math.radians(-80)), cy + s * 0.5 * math.sin(math.radians(-80)))
    bottom = (top[0], cy - (top[1] - cy))
    d.line([top, bottom], fill=shade(colour, 0.7), width=int(s * 0.025))
    d.line([(cx - s * 0.46, cy), (cx + s * 0.44, cy)], fill=shade(colour, 1.3), width=int(s * 0.04))
    d.polygon([(cx + s * 0.52, cy), (cx + s * 0.36, cy - s * 0.08), (cx + s * 0.36, cy + s * 0.08)], fill=shade(colour, 1.3))


def vortex(d, cx, cy, s, colour):
    for i in range(4):
        r = s * (0.5 - i * 0.1)
        d.arc([cx - r, cy - r, cx + r, cy + r], start=i * 70, end=i * 70 + 250, fill=shade(colour, 0.7 + i * 0.15), width=int(s * 0.07))
    d.ellipse([cx - s * 0.08, cy - s * 0.08, cx + s * 0.08, cy + s * 0.08], fill=shade(colour, 1.4))


def vessel(d, cx, cy, s, colour):
    d.rectangle([cx - s * 0.1, cy - s * 0.5, cx + s * 0.1, cy - s * 0.3], fill=shade(colour, 0.7))
    d.ellipse([cx - s * 0.4, cy - s * 0.34, cx + s * 0.4, cy + s * 0.5], fill=shade(colour, 0.7))
    d.chord([cx - s * 0.34, cy - s * 0.28, cx + s * 0.34, cy + s * 0.44], start=-10, end=190, fill=colour)
    d.ellipse([cx - s * 0.2, cy - s * 0.2, cx - s * 0.06, cy - s * 0.06], fill=shade(colour, 1.5))


def harbor_crown(d, cx, cy, s, colour):
    anchor(d, cx, cy + s * 0.12, s * 0.82, COLOURS["anchor"])
    y, w = cy - s * 0.36, s * 0.34
    d.polygon([(cx - w, y + s * 0.1), (cx - w, y - s * 0.08), (cx - w * 0.5, y + s * 0.01), (cx, y - s * 0.14), (cx + w * 0.5, y + s * 0.01), (cx + w, y - s * 0.08), (cx + w, y + s * 0.1)], fill=colour)


GLYPHS = {name: globals()[name] for name in COLOURS}


def draw_icon(tier, main, stats):
    image = Image.new("RGB", (CANVAS, CANVAS), GROUND)

    # A soft glow of the main colour behind the glyph; higher tiers glow more.
    glow = Image.new("RGB", (CANVAS, CANVAS), (0, 0, 0))
    ImageDraw.Draw(glow).ellipse([CANVAS * 0.18, CANVAS * 0.12, CANVAS * 0.82, CANVAS * 0.76], fill=COLOURS[main])
    glow = glow.filter(ImageFilter.GaussianBlur(CANVAS * 0.09))
    image = Image.blend(image, Image.composite(glow, image, glow.convert("L")), GLOW[tier])

    d = ImageDraw.Draw(image, "RGBA")
    main_centre_y = CANVAS * (0.44 if stats else 0.5)
    main_size = CANVAS * (0.6 if stats else 0.72)
    GLYPHS[main](d, CANVAS / 2, main_centre_y, main_size, COLOURS[main])

    if stats:
        chip = CANVAS * 0.15
        gap = CANVAS * 0.035
        span = len(stats) * chip + (len(stats) - 1) * gap
        x = CANVAS / 2 - span / 2 + chip / 2
        y = CANVAS * 0.86
        for stat in stats:
            d.ellipse([x - chip * 0.62, y - chip * 0.62, x + chip * 0.62, y + chip * 0.62], fill=(18, 19, 22, 255))
            GLYPHS[stat](d, x, y, chip * 0.9, COLOURS[stat])
            x += chip + gap

    return image.resize((SIZE, SIZE), Image.LANCZOS)


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--only", nargs="*", default=[], help="draw only these item IDs")
    args = parser.parse_args()
    unknown = [item for item in args.only if item not in PLACEHOLDERS]
    if unknown:
        print(f"Not a placeholder: {', '.join(unknown)}", file=sys.stderr)
        return 1
    for item, (folder, tier, glyph, stats) in PLACEHOLDERS.items():
        if args.only and item not in args.only:
            continue
        path = ART / folder / f"T_{item}_Icon.png"
        path.parent.mkdir(parents=True, exist_ok=True)
        draw_icon(tier, glyph, stats).save(path, optimize=True)
        print(path.relative_to(ROOT))
    return 0


if __name__ == "__main__":
    sys.exit(main())
