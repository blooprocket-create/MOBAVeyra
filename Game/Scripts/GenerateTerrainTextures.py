"""Deterministic, tileable terrain textures for the Crucible's Landscape (ADR-040 §3; World Production Bible §8).

Plain Python (numpy and Pillow): each layer of Game/ArtSource/Environment/Terrain/TerrainTextures.json becomes a base
colour, a normal map and an ORM map (ambient occlusion, roughness, metallic) that tile seamlessly, from its seed and
recipe alone. No third-party imagery: every pixel comes from the noise below. The manifest records each file's hash.

Usage: python Game/Scripts/GenerateTerrainTextures.py [--only Causeway,Moss] [--size 512] [--preview]
"""
import argparse
import hashlib
import json
from pathlib import Path

import numpy as np
from PIL import Image

GAME = Path(__file__).resolve().parents[1]
SOURCE = GAME / "ArtSource" / "Environment" / "Terrain"
PROFILE = SOURCE / "TerrainTextures.json"
# The macro variation's and the water's own random streams, apart from every layer's, so adding a layer never changes them.
MACRO_STREAM = 1000
WATER_STREAM = 1001
# Still water is one value everywhere, so a handful of texels hold it. Resolution, not tuning.
STILL_WATER_TEXELS = 8


# Periodic noise. Every pattern is built on lattices whose periods divide the image, so it tiles exactly.

def periodic_gradient_noise(size, period, rng):
    """Gradient noise in [-1, 1] with `period` lattice cells across the image, wrapping at its edges."""
    angles = rng.uniform(0.0, 2.0 * np.pi, (period, period))
    gx, gy = np.cos(angles), np.sin(angles)
    coords = (np.arange(size) + 0.5) * period / size
    x, y = np.meshgrid(coords, coords, indexing="xy")
    x0, y0 = np.floor(x).astype(int), np.floor(y).astype(int)
    fx, fy = x - x0, y - y0

    def corner(dx, dy):
        ix, iy = (x0 + dx) % period, (y0 + dy) % period
        return gx[iy, ix] * (fx - dx) + gy[iy, ix] * (fy - dy)

    def fade(t):
        return t * t * t * (t * (t * 6 - 15) + 10)

    u, v = fade(fx), fade(fy)
    top = corner(0, 0) + u * (corner(1, 0) - corner(0, 0))
    bottom = corner(0, 1) + u * (corner(1, 1) - corner(0, 1))
    return np.clip((top + v * (bottom - top)) * 1.414, -1.0, 1.0)


def fbm(size, base_period, octaves, rng, gain=0.5):
    """Fractal sum of periodic gradient noise, normalised to [0, 1]."""
    total = np.zeros((size, size))
    amplitude, norm = 1.0, 0.0
    for octave in range(octaves):
        total += amplitude * periodic_gradient_noise(size, base_period * 2 ** octave, rng)
        norm += amplitude
        amplitude *= gain
    return total / norm * 0.5 + 0.5


def cellular(size, cells, rng, jitter=0.9, stretch=(1.0, 1.0), offsets=False):
    """Periodic cellular noise on a `cells` grid: nearest and second-nearest distances, and the nearest cell's id; with
    `offsets`, each pixel's offset from its nearest cell's point too, in cells, so a plane over it is one flat facet."""
    points = rng.uniform(0.5 - jitter / 2, 0.5 + jitter / 2, (cells, cells, 2))
    coords = (np.arange(size) + 0.5) * cells / size
    x, y = np.meshgrid(coords, coords, indexing="xy")
    cx, cy = np.floor(x).astype(int), np.floor(y).astype(int)
    f1 = np.full((size, size), np.inf)
    f2 = np.full((size, size), np.inf)
    ids = np.zeros((size, size), dtype=np.int64)
    ox = np.zeros((size, size))
    oy = np.zeros((size, size))
    for dy in (-1, 0, 1):
        for dx in (-1, 0, 1):
            nx, ny = cx + dx, cy + dy
            wx, wy = nx % cells, ny % cells
            px = nx + points[wy, wx, 0]
            py = ny + points[wy, wx, 1]
            d = np.hypot((x - px) * stretch[0], (y - py) * stretch[1])
            closer = d < f1
            f2 = np.where(closer, f1, np.minimum(f2, d))
            ids = np.where(closer, wy * cells + wx, ids)
            ox = np.where(closer, x - px, ox)
            oy = np.where(closer, y - py, oy)
            f1 = np.where(closer, d, f1)
    return (f1, f2, ids, ox, oy) if offsets else (f1, f2, ids)


def blur_periodic(image, radius):
    """A box blur that wraps at the edges, so blurred maps still tile."""
    out = image.copy()
    for axis in (0, 1):
        acc = np.zeros_like(out)
        for shift in range(-radius, radius + 1):
            acc += np.roll(out, shift, axis=axis)
        out = acc / (2 * radius + 1)
    return out


def smoothstep(lo, hi, x):
    t = np.clip((x - lo) / (hi - lo), 0.0, 1.0)
    return t * t * (3 - 2 * t)


def mix(a, b, t):
    t = t[..., None] if np.ndim(t) == 2 else t
    return np.asarray(a) * (1 - t) + np.asarray(b) * t


def per_cell(ids, cells, rng, spread):
    """A random value per cell, centred on 0, looked up for every pixel."""
    values = rng.uniform(-spread, spread, cells * cells)
    return values[ids]


# Recipes: each returns (albedo [H,W,3] linear-ish sRGB 0..1, height [H,W] 0..1, roughness [H,W] 0..1).

def flagstones(spec, size, rng):
    """An old causeway: big uneven flags, a share of them laid as smaller setts instead, each settled a little higher or
    lower; worn edges, moss creeping from the joints, blotches of lichen, a few cracks; here and there a flag gone,
    its hole filled with earth and grass. Large slab-to-slab variation, so a long lane never reads as one even field."""
    cells = spec["cells"]
    f1, f2, ids = cellular(size, cells, rng, jitter=0.7, stretch=(1.0, 0.85))
    s1, s2, sub_ids = cellular(size, cells * 2, rng, jitter=0.8, stretch=(1.0, 0.9))
    split = per_cell(ids, cells, rng, 1.0) > 1.0 - 2.0 * spec["settShare"]
    missing = (per_cell(ids, cells, rng, 1.0) > 1.0 - 2.0 * spec["missingShare"]) & ~split
    edge = np.where(split, np.minimum(f2 - f1, s2 - s1), f2 - f1)
    joint_width = spec["grout"] * (0.7 + 0.6 * fbm(size, 4, 3, rng))
    grout = 1.0 - smoothstep(joint_width * 0.45, joint_width, edge)
    bevel = smoothstep(0.0, joint_width * 3.0, edge)
    detail = fbm(size, 8, 6, rng)
    grain = fbm(size, 48, 3, rng)
    pits = smoothstep(0.7, 0.85, fbm(size, 24, 3, rng))
    _, crack_f2, _ = cellular(size, cells * 3, rng, jitter=1.0)
    crack_f1 = cellular(size, cells * 3, np.random.default_rng(rng.integers(1 << 30)), jitter=1.0)[0]
    cracked = (per_cell(ids, cells, rng, 1.0) > 0.45) & ~split
    cracks = (1.0 - smoothstep(0.0, 0.01, np.abs(crack_f2 - crack_f1))) * cracked * bevel
    # Each flag or sett sits a little higher or lower and tilts a little: the old causeway has settled unevenly.
    level = np.where(split, per_cell(sub_ids, cells * 2, rng, 0.1), per_cell(ids, cells, rng, 0.12))
    coords = (np.arange(size) + 0.5) / size
    x, y = np.meshgrid(coords, coords, indexing="xy")
    tilt = per_cell(ids, cells, rng, 0.6) * np.sin(2 * np.pi * x * cells) * 0.04
    height = 0.5 * bevel + 0.2 * detail + 0.08 * grain - 0.1 * pits - 0.2 * cracks + level + tilt
    height = height * (1 - grout) + grout * (0.02 + 0.08 * detail)

    variation = spec["stoneVariation"]
    brightness = 1.0 + np.where(split, per_cell(sub_ids, cells * 2, rng, variation), per_cell(ids, cells, rng, variation))
    warmth = np.where(split, per_cell(sub_ids, cells * 2, rng, spec["warmCool"]), per_cell(ids, cells, rng, spec["warmCool"]))
    stone = np.asarray(spec["stone"])[None, None, :] * brightness[..., None]
    stone = stone + np.stack([warmth, warmth * 0.3, -warmth], axis=-1)
    stone = stone * (0.84 + 0.22 * detail[..., None] + 0.08 * grain[..., None]) * (1 - 0.2 * pits[..., None])
    # Wear lightens the worn tops; the joints' moss spills onto the edges.
    stone = stone * (0.92 + 0.12 * bevel[..., None])
    spill = (1 - smoothstep(joint_width, joint_width * 4.0, edge)) * smoothstep(0.45, 0.7, fbm(size, 12, 4, rng))
    blotches = fbm(size, 6, 5, rng)
    lichen_mask = smoothstep(1 - spec["lichenCover"] - 0.06, 1 - spec["lichenCover"] + 0.06, blotches) * bevel
    albedo = mix(stone, spec["lichen"], lichen_mask * 0.45)
    groutcol = np.asarray(spec["groutColor"])[None, None, :] * (0.65 + 0.7 * detail[..., None])
    albedo = mix(albedo, groutcol, np.clip(grout + spill * 0.8, 0, 1))
    albedo = albedo * (1 - 0.4 * cracks[..., None])
    # A flag gone: earth in its hole, grass coming up through it, sunk below its neighbours.
    tuft = smoothstep(0.4, 0.65, fbm(size, 32, 3, rng))
    hole = np.asarray(spec["holeEarth"])[None, None, :] * (0.8 + 0.4 * detail[..., None])
    hole = mix(hole, spec["holeGrass"], tuft * 0.8)
    albedo = mix(albedo, hole, missing.astype(float))
    height = np.where(missing, 0.03 + 0.1 * tuft + 0.04 * detail, height)
    rough_lo, rough_hi = spec["roughness"]
    roughness = rough_lo + (rough_hi - rough_lo) * np.clip(0.4 * detail + grout + spill + 0.3 * lichen_mask, 0, 1)
    roughness = np.where(missing, rough_hi, roughness)
    return albedo, height, roughness


def moss(spec, size, rng):
    """Jungle floor: patches of dark soil under cushions of moss in several greens, leaf litter, small stones."""
    for key in ("cushionContrast", "cushionRelief"):
        if not isinstance(spec[key], (int, float)) or isinstance(spec[key], bool) or not 0 <= spec[key] <= 1:
            raise ValueError(f"Moss {key} must be a number in [0, 1]")
    cover = fbm(size, 3, 6, rng)
    fine = fbm(size, 64, 3, rng)
    tone = fbm(size, 6, 4, rng)
    cushions_f1, _, _ = cellular(size, 56, rng)
    cushions = 1.0 - smoothstep(0.0, 0.8, cushions_f1)
    moss_mask = smoothstep(1 - spec["mossCover"] - 0.08, 1 - spec["mossCover"] + 0.08, cover + 0.12 * (fine - 0.5))
    soil = np.asarray(spec["soil"])[None, None, :] * (0.7 + 0.6 * fine[..., None])
    moss_col = mix(spec["moss"], spec["mossLight"], smoothstep(0.35, 0.8, tone) * 0.85)
    dry = smoothstep(1 - spec["dryCover"] - 0.1, 1 - spec["dryCover"] + 0.1, fbm(size, 2, 5, rng))
    moss_col = mix(moss_col, spec["dry"], dry * 0.7)
    # Keep the broad colour field dominant at gameplay distance; small cushions should not read as paving.
    moss_col = moss_col * (0.925 + spec["cushionContrast"] * (cushions[..., None] - 0.5) + 0.15 * fine[..., None])
    albedo = mix(soil, moss_col, moss_mask)
    litter_f1, _, litter_ids = cellular(size, 80, rng, stretch=(1.0, 2.4))
    # Leaf litter on a share of the cells: per_cell spreads -1..1, so the share keeps the cells above 1 - 2 * share.
    litter = (1.0 - smoothstep(0.1, 0.2, litter_f1)) * (per_cell(litter_ids, 80, rng, 1.0) > 1.0 - 2.0 * spec["litterShare"])
    litter = litter * (1 - moss_mask * 0.75)
    leaf = np.asarray(spec["litter"])[None, None, :] * (1.0 + per_cell(litter_ids, 80, rng, 0.45)[..., None])
    albedo = mix(albedo, leaf, litter * spec["litterStrength"])
    stones_f1, _, stone_ids = cellular(size, 24, rng)
    stones = np.sqrt(np.clip(1 - (stones_f1 / 0.22) ** 2, 0, 1)) * (per_cell(stone_ids, 24, rng, 1.0) > 0.6) * (1 - moss_mask)
    albedo = mix(albedo, np.asarray([0.36, 0.34, 0.30])[None, None, :] * (0.7 + 0.4 * stones[..., None]), smoothstep(0.0, 0.2, stones))
    height = 0.3 * moss_mask * (0.7 + spec["cushionRelief"] * (cushions - 0.5)) + 0.2 * fine + 0.12 * litter + 0.2 * cover + 0.35 * stones
    rough_lo, rough_hi = spec["roughness"]
    roughness = rough_hi - (rough_hi - rough_lo) * np.clip(0.6 * litter + 0.4 * stones + 0.2 * (1 - moss_mask), 0, 1)
    return albedo, height, roughness


def pebbles(spec, size, rng):
    """River shore: damp sand, gathered drifts of rounded pebbles of varied stone and size."""
    grain = fbm(size, 128, 2, rng)
    ripples = fbm(size, 6, 3, rng)
    drifts = smoothstep(0.35, 0.65, fbm(size, 3, 4, rng))
    sand = np.asarray(spec["sand"])[None, None, :] * (0.85 + 0.25 * grain[..., None] + 0.12 * ripples[..., None])
    albedo, height = sand, 0.12 * grain + 0.18 * ripples
    covered = np.zeros((size, size))
    for cells, radius, share in ((spec["cells"] // 2, 0.36, 0.6), (spec["cells"], 0.34, 0.8), (spec["cells"] * 2, 0.3, 1.0)):
        f1, _, ids = cellular(size, cells, rng, jitter=0.95)
        keep = (per_cell(ids, cells, rng, 0.5) + 0.5) < drifts * spec["pebbleCover"] * 2 * share
        r = radius + per_cell(ids, cells, rng, 0.08)
        squash = 1.0 + per_cell(ids, cells, rng, 0.3)
        dome = np.sqrt(np.clip(1.0 - (f1 / r) ** 2 * squash, 0.0, 1.0)) * keep * (1 - covered)
        tone = 1.0 + per_cell(ids, cells, rng, spec["pebbleVariation"] * 2)
        hue = per_cell(ids, cells, rng, 0.06)
        pebble = np.asarray(spec["pebble"])[None, None, :] * tone[..., None] + np.stack([hue, hue * 0.4, -hue * 0.6], axis=-1)
        mask = smoothstep(0.0, 0.15, dome)
        albedo = mix(albedo, pebble * (0.75 + 0.35 * dome[..., None]), mask)
        height = np.maximum(height, 0.25 + 0.6 * dome * (cells / spec["cells"]) ** -0.5)
        covered = np.maximum(covered, mask)
    rough_lo, rough_hi = spec["roughness"]
    roughness = rough_hi - (rough_hi - rough_lo) * covered * 0.8
    return albedo, height, roughness


def covering(noise, share, softness):
    """A soft mask over the `share` of the image where `noise` runs highest: its coverage is the share, whatever the
    noise's spread."""
    edge = np.quantile(noise, 1.0 - share)
    return smoothstep(edge - softness, edge + softness, noise)


def soil(spec, size, rng):
    """Trampled earth: packed soil in broad lighter and darker patches, worn pale and smooth where feet have pressed it,
    damp hollows, fine grit, a scatter of stones half buried, and moss creeping back in from the edges of its patches."""
    broad = fbm(size, 3, 5, rng)
    fine = fbm(size, 48, 3, rng)
    worn = covering(fbm(size, 4, 4, rng), spec["wornCover"], 0.06)
    earth = mix(spec["soil"], spec["soilLight"], np.clip(0.5 * smoothstep(0.3, 0.7, broad) + 0.6 * worn, 0, 1))
    earth = earth * (0.9 + 0.2 * fine[..., None])
    damp = covering(fbm(size, 5, 4, rng), spec["dampCover"], 0.05)
    earth = earth * (1 - spec["dampDarkening"] * damp[..., None])
    f1, _, ids = cellular(size, spec["stoneCells"], rng, jitter=0.95)
    keep = (per_cell(ids, spec["stoneCells"], rng, 0.5) + 0.5) < spec["stoneShare"]
    # Each stone its own size, its outline broken, so it reads as a stone half buried and not a placed disc.
    radius = (0.22 + per_cell(ids, spec["stoneCells"], rng, 0.08)) * (0.75 + 0.5 * fbm(size, 64, 2, rng))
    dome = np.sqrt(np.clip(1.0 - (f1 / radius) ** 2, 0.0, 1.0)) * keep
    stone_tone = 1.0 + per_cell(ids, spec["stoneCells"], rng, 0.18)
    stones = smoothstep(0.0, 0.25, dome)
    earth = mix(earth, np.asarray(spec["stone"])[None, None, :] * stone_tone[..., None] * (0.75 + 0.35 * dome[..., None]), stones)
    moss_in = covering(fbm(size, 4, 5, rng) + 0.15 * fine, spec["mossCover"], 0.05) * (1 - worn)
    moss_col = np.asarray(spec["moss"])[None, None, :] * (0.85 + 0.3 * fine[..., None])
    albedo = mix(earth, moss_col, moss_in)
    height = 0.25 * broad + 0.12 * fine - 0.08 * worn - 0.06 * damp + 0.45 * dome + 0.12 * moss_in
    rough_lo, rough_hi = spec["roughness"]
    roughness = rough_hi - (rough_hi - rough_lo) * np.clip(0.5 * worn + 0.5 * damp + 0.3 * stones, 0, 1)
    return albedo, height, roughness


def courses(size, rng, count, spread, per_course, warp=0.0, lean=0.0):
    """Stone laid, or bedded, in `count` courses of varied thickness (spread in [0, 1)), each split into blocks
    (per_course: fewest, most) at its own offsets, its joints leaning by up to `lean`, its bedding wavering by `warp`
    tile heights. Returns each pixel's block id, how many ids there are, where it lies across its block (u, v in [0, 1))
    and its block's width and height in tile widths. Whole courses and blocks wrap at the edges, so it tiles."""
    coords = (np.arange(size) + 0.5) / size
    x, y = np.meshgrid(coords, coords, indexing="xy")
    if warp:
        y = (y + warp * (fbm(size, 2, 3, rng) - 0.5)) % 1.0
    thickness = rng.uniform(1.0 - spread, 1.0 + spread, count)
    edges = np.concatenate([[0.0], np.cumsum(thickness / thickness.sum())])
    row = np.clip(np.searchsorted(edges, y, side="right") - 1, 0, count - 1)
    height = (edges[1:] - edges[:-1])[row]
    v = np.clip((y - edges[row]) / height, 0.0, 1.0 - 1e-9)
    low, high = per_course
    counts = rng.integers(low, high + 1, count)
    shifts = rng.uniform(0.0, 1.0, count)
    leans = rng.uniform(-lean, lean, count)
    n = counts[row]
    along = x * n + shifts[row] + leans[row] * (v - 0.5)
    col = np.floor(along).astype(int) % n
    u = along - np.floor(along)
    return row * (high + 1) + col, count * (high + 1), u, v, 1.0 / n, height


def bedded(spec, size, rng):
    """Weathered cliff rock: beds of uneven thickness, wavering, split into blocks by leaning joints; each block broken
    into two chiselled facets, each tilted its own way so it catches the light as one plane; dark grime in the joints,
    edges chipped and worn pale, soft lichen. Large value breakup first: the facets read from the gameplay camera and
    the detail stays subordinate. Returns the facets' slope too, for the normal map."""
    ids, cells, u, v, width, height = courses(size, rng, spec["beds"], spec["bedSpread"], spec["blocksPerBed"],
                                              warp=spec["bedWarp"], lean=spec["jointLean"])
    chip = (fbm(size, 20, 4, rng) - 0.5) * spec["chip"]
    border = np.minimum(np.minimum(u, 1 - u) * width, np.minimum(v, 1 - v) * height) + chip
    joint = spec["jointWidth"]
    joints = 1.0 - smoothstep(joint * 0.4, joint, border)
    rim = smoothstep(joint, joint * 4.0, border)
    # Each block split by its own line into two facets; each facet a plane of its own slope.
    split = per_cell(ids, cells, rng, 1.0) * (u - 0.5) + per_cell(ids, cells, rng, 1.0) * (v - 0.5) + per_cell(ids, cells, rng, 0.25) > 0
    slope = spec["facetSlope"]
    sx = np.where(split, per_cell(ids, cells, rng, slope), per_cell(ids, cells, rng, slope))
    sy = np.where(split, per_cell(ids, cells, rng, slope), per_cell(ids, cells, rng, slope)) - slope * 0.3
    detail = fbm(size, 12, 5, rng)
    level = per_cell(ids, cells, rng, 0.2)
    surface = 0.55 * rim + level + 0.18 * detail - 0.35 * joints
    wear = rim * (1.0 - smoothstep(joint * 1.5, joint * 5.0, border))
    tone = 1.0 + per_cell(ids, cells, rng, spec["blockVariation"]) + 0.12 * np.where(split, 1.0, -1.0) * spec["facetContrast"]
    warmth = per_cell(ids, cells, rng, spec["warmCool"])
    rock = mix(spec["rock"], spec["rockLight"], np.clip(0.5 * smoothstep(0.3, 0.8, detail) + 0.7 * wear, 0, 1))
    rock = rock * tone[..., None] + np.stack([warmth, warmth * 0.4, -warmth], axis=-1)
    lichen = smoothstep(1 - spec["lichenCover"] - 0.05, 1 - spec["lichenCover"] + 0.05, fbm(size, 5, 5, rng)) * rim
    albedo = mix(rock, spec["lichen"], lichen * 0.5)
    albedo = mix(albedo, spec["crevice"], joints * spec["creviceStrength"])
    rough_lo, rough_hi = spec["roughness"]
    roughness = rough_lo + (rough_hi - rough_lo) * np.clip(0.4 * detail + 0.6 * joints + 0.3 * lichen, 0, 1)
    facets = (sx * rim, sy * rim)
    return albedo, surface, roughness, facets


def ashlar(spec, size, rng):
    """Cut stone laid in courses of varied height: long blocks set in mortar, their faces dressed flat but weathered,
    their edges chipped, each block its own tone and a slight set of its own, grime toward each block's foot, moss in
    the deepest mortar. Joints never line up from course to course."""
    ids, cells, u, v, width, height = courses(size, rng, spec["courses"], spec["courseSpread"], spec["blocksPerCourse"])
    chip = (fbm(size, 24, 4, rng) - 0.5) * spec["chip"]
    border = np.minimum(np.minimum(u, 1 - u) * width, np.minimum(v, 1 - v) * height) + chip
    mortar = 1.0 - smoothstep(spec["mortar"] * 0.6, spec["mortar"], border)
    face = smoothstep(spec["mortar"], spec["mortar"] * 3.0, border)
    detail = fbm(size, 16, 5, rng)
    pits = smoothstep(0.72, 0.86, fbm(size, 32, 3, rng))
    surface = 0.5 * face + 0.15 * detail - 0.1 * pits + per_cell(ids, cells, rng, 0.08)
    surface = surface * (1 - mortar) + mortar * (0.05 + 0.05 * detail)
    tone = 1.0 + per_cell(ids, cells, rng, spec["blockVariation"])
    warmth = per_cell(ids, cells, rng, spec["warmCool"])
    stone = np.asarray(spec["stone"])[None, None, :] * tone[..., None] + np.stack([warmth, warmth * 0.5, -warmth], axis=-1)
    stains = fbm(size, 3, 5, rng)
    stone = stone * (0.84 + 0.2 * detail[..., None]) * (1 - spec["stains"] * smoothstep(0.5, 0.8, stains)[..., None]) * (1 - 0.2 * pits[..., None])
    stone = stone * (1 - spec["grime"] * (1 - smoothstep(0.0, 0.6, v))[..., None])
    moss_in = mortar * smoothstep(0.5, 0.7, fbm(size, 8, 4, rng))
    albedo = mix(stone, np.asarray(spec["mortarColor"])[None, None, :] * (0.8 + 0.4 * detail[..., None]), mortar)
    albedo = mix(albedo, spec["moss"], moss_in * 0.8)
    rough_lo, rough_hi = spec["roughness"]
    roughness = rough_lo + (rough_hi - rough_lo) * np.clip(0.4 * detail + mortar + 0.3 * pits, 0, 1)
    set_slope = spec["setSlope"]
    facets = (per_cell(ids, cells, rng, set_slope) * face, per_cell(ids, cells, rng, set_slope) * face)
    return albedo, surface, roughness, facets


RECIPES = {"flagstones": flagstones, "moss": moss, "pebbles": pebbles, "soil": soil, "bedded": bedded, "ashlar": ashlar}


def normal_map(height, strength, facets=None):
    """A tangent-space normal map from the height field, wrapping at the edges (OpenGL-style +Y flipped for Unreal),
    with any facets' slope (rise per unit across, along x and y) added as it stands."""
    dx = (np.roll(height, -1, axis=1) - np.roll(height, 1, axis=1)) * 0.5
    dy = (np.roll(height, -1, axis=0) - np.roll(height, 1, axis=0)) * 0.5
    size = height.shape[0]
    scale = strength * size / 256.0
    nx, ny, nz = -dx * scale, dy * scale, np.ones_like(height)
    if facets is not None:
        nx, ny = nx - facets[0], ny + facets[1]
    length = np.sqrt(nx * nx + ny * ny + nz * nz)
    return np.stack([nx / length, ny / length, nz / length], axis=-1) * 0.5 + 0.5


def ambient_occlusion(height):
    """Cavity occlusion: how far each texel sits below its neighbourhood's average."""
    size = height.shape[0]
    around = blur_periodic(height, max(2, size // 128))
    return np.clip(1.0 - 2.2 * np.maximum(around - height, 0.0), 0.35, 1.0)


def to_image(array):
    return Image.fromarray((np.clip(array, 0, 1) * 255 + 0.5).astype(np.uint8))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--only", default="")
    parser.add_argument("--size", type=int, default=0, help="override the profile's size, for quick previews")
    parser.add_argument("--preview", action="store_true", help="also write a 2x2-tiled contact sheet to Saved/TerrainTextures")
    args = parser.parse_args()
    profile = json.loads(PROFILE.read_text(encoding="utf-8"))
    size = args.size or profile["size"]
    only = {name for name in args.only.split(",") if name}
    manifest = {"profileSha256": hashlib.sha256(PROFILE.read_bytes()).hexdigest(), "size": size, "files": []}
    preview_dir = GAME / "Saved" / "TerrainTextures"
    # A size override is a quick look: its files go beside the previews, never over the sources.
    out_dir = preview_dir if args.size else SOURCE
    out_dir.mkdir(parents=True, exist_ok=True)
    for index, spec in enumerate(profile["layers"]):
        if only and spec["id"] not in only:
            continue
        # Each layer's own stream: changing one layer's recipe leaves the others' pixels as they were.
        rng = np.random.default_rng([profile["seed"], index])
        albedo, height, roughness, *facets = RECIPES[spec["recipe"]](spec, size, rng)
        height = (height - height.min()) / max(height.max() - height.min(), 1e-6)
        normal = normal_map(height, spec["normalStrength"], facets[0] if facets else None)
        # Ambient occlusion, roughness and metallic, with the height in alpha for the material's height blend.
        orm = np.stack([ambient_occlusion(height), roughness, np.zeros_like(height), height], axis=-1)
        albedo = albedo * (0.75 + 0.25 * orm[..., 0:1])
        outputs = {
            f"T_Crucible_{spec['id']}_BaseColor.png": albedo,
            f"T_Crucible_{spec['id']}_Normal.png": normal,
            f"T_Crucible_{spec['id']}_ORMH.png": orm,
        }
        for name, array in outputs.items():
            path = out_dir / name
            to_image(array).save(path, optimize=False)
            manifest["files"].append({"name": name, "layer": spec["id"], "tileMetres": spec["tileMetres"],
                                      "sha256": hashlib.sha256(path.read_bytes()).hexdigest()})
        if args.preview:
            preview_dir.mkdir(parents=True, exist_ok=True)
            light = np.array([-0.5, 0.6, 0.62])
            shade = np.clip(((normal * 2 - 1) * light).sum(-1) / np.linalg.norm(light), 0, 1)
            lit = albedo * (0.35 + 0.8 * shade[..., None]) * orm[..., 0:1]
            lit = lit[..., :3]
            to_image(np.tile(lit, (2, 2, 1))).save(preview_dir / f"{spec['id']}_preview.png")
        print(f"{spec['id']}: {size}px, tiles every {spec['tileMetres']} m")
    if not only:
        # Large-scale variation the material lays over every layer, so no layer's tiling shows across a lane:
        # brightness in red, a warm-cool shift in green, wetness and moss in blue.
        rng = np.random.default_rng([profile["seed"], MACRO_STREAM])
        macro = np.stack([fbm(size, 2, 5, rng), fbm(size, 3, 4, rng), fbm(size, 2, 4, rng)], axis=-1)
        macro = (macro - macro.min(axis=(0, 1))) / np.maximum(macro.max(axis=(0, 1)) - macro.min(axis=(0, 1)), 1e-6)
        path = out_dir / "T_Crucible_Macro.png"
        to_image(macro).save(path, optimize=False)
        manifest["files"].append({"name": path.name, "layer": "Macro", "tileMetres": profile["macroTileMetres"],
                                  "sha256": hashlib.sha256(path.read_bytes()).hexdigest()})
        # The river's ripples: a normal map whose crests run across the flow (the texture's U axis), and a foam
        # pattern in its alpha for the shore.
        water = profile["water"]
        rng = np.random.default_rng([profile["seed"], WATER_STREAM])
        ripples = 0.6 * fbm(size, 6, 4, rng) + 0.4 * fbm(size, 12, 3, rng)
        _, cells_f2, _ = cellular(size, 40, rng, stretch=(1.0, 0.6))
        cells_f1 = cellular(size, 40, np.random.default_rng(rng.integers(1 << 30)), stretch=(1.0, 0.6))[0]
        foam = smoothstep(0.55, 0.85, 1.0 - np.abs(cells_f2 - cells_f1) * 6.0) * smoothstep(0.35, 0.7, fbm(size, 8, 3, rng))
        water_map = np.concatenate([normal_map(ripples, water["rippleStrength"]), foam[..., None]], axis=-1)
        path = out_dir / "T_Crucible_WaterNormal.png"
        to_image(water_map).save(path, optimize=False)
        manifest["files"].append({"name": path.name, "layer": "Water", "tileMetres": water["tileMetres"],
                                  "sha256": hashlib.sha256(path.read_bytes()).hexdigest()})
        # Still water (ADR-076): the ripples' heights where nothing stirs the river, black and linear, so the river's
        # material and the ripples' step read nothing from it until the water's presentation gives them its own.
        path = out_dir / "T_Crucible_StillWater.png"
        to_image(np.zeros((STILL_WATER_TEXELS, STILL_WATER_TEXELS, 3))).save(path, optimize=False)
        manifest["files"].append({"name": path.name, "layer": "Water", "tileMetres": water["tileMetres"],
                                  "sha256": hashlib.sha256(path.read_bytes()).hexdigest()})
    if not only and not args.size:
        (SOURCE / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8", newline="\n")


if __name__ == "__main__":
    main()
