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


def cellular(size, cells, rng, jitter=0.9, stretch=(1.0, 1.0)):
    """Periodic cellular noise on a `cells` grid: nearest and second-nearest distances, and the nearest cell's id."""
    points = rng.uniform(0.5 - jitter / 2, 0.5 + jitter / 2, (cells, cells, 2))
    coords = (np.arange(size) + 0.5) * cells / size
    x, y = np.meshgrid(coords, coords, indexing="xy")
    cx, cy = np.floor(x).astype(int), np.floor(y).astype(int)
    f1 = np.full((size, size), np.inf)
    f2 = np.full((size, size), np.inf)
    ids = np.zeros((size, size), dtype=np.int64)
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
            f1 = np.where(closer, d, f1)
    return f1, f2, ids


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
    """Pale weathered stone flags: uneven slabs, worn edges, moss creeping from the joints, lichen, a few cracks."""
    cells = spec["cells"]
    f1, f2, ids = cellular(size, cells, rng, jitter=0.7, stretch=(1.0, 0.85))
    edge = f2 - f1
    joint_width = spec["grout"] * (0.7 + 0.6 * fbm(size, 4, 3, rng))
    grout = 1.0 - smoothstep(joint_width * 0.45, joint_width, edge)
    bevel = smoothstep(0.0, joint_width * 3.0, edge)
    detail = fbm(size, 8, 6, rng)
    grain = fbm(size, 48, 3, rng)
    pits = smoothstep(0.7, 0.85, fbm(size, 24, 3, rng))
    _, crack_f2, _ = cellular(size, cells * 3, rng, jitter=1.0)
    crack_f1 = cellular(size, cells * 3, np.random.default_rng(rng.integers(1 << 30)), jitter=1.0)[0]
    cracked = per_cell(ids, cells, rng, 1.0) > 0.45
    cracks = (1.0 - smoothstep(0.0, 0.01, np.abs(crack_f2 - crack_f1))) * cracked * bevel
    # Each slab sits a little higher or lower and tilts a little: the old causeway has settled unevenly.
    level = per_cell(ids, cells, rng, 0.12)
    coords = (np.arange(size) + 0.5) / size
    x, y = np.meshgrid(coords, coords, indexing="xy")
    tilt = per_cell(ids, cells, rng, 0.6) * np.sin(2 * np.pi * x * cells) * 0.04
    height = 0.5 * bevel + 0.2 * detail + 0.08 * grain - 0.1 * pits - 0.2 * cracks + level + tilt
    height = height * (1 - grout) + grout * (0.02 + 0.08 * detail)

    brightness = 1.0 + per_cell(ids, cells, rng, spec["stoneVariation"])
    warmth = per_cell(ids, cells, rng, spec["warmCool"])
    stone = np.asarray(spec["stone"])[None, None, :] * brightness[..., None]
    stone = stone + np.stack([warmth, warmth * 0.3, -warmth], axis=-1)
    stone = stone * (0.84 + 0.22 * detail[..., None] + 0.08 * grain[..., None]) * (1 - 0.2 * pits[..., None])
    # Wear lightens the worn tops; the joints' moss spills onto the edges.
    stone = stone * (0.92 + 0.12 * bevel[..., None])
    spill = (1 - smoothstep(joint_width, joint_width * 4.0, edge)) * smoothstep(0.45, 0.7, fbm(size, 12, 4, rng))
    lichen_f1, _, lichen_ids = cellular(size, 40, rng)
    lichen_mask = (1 - smoothstep(0.15, 0.45, lichen_f1)) * (per_cell(lichen_ids, 40, rng, 1.0) > 1 - 2 * spec["lichenCover"]) * bevel
    albedo = mix(stone, spec["lichen"], lichen_mask * 0.55)
    groutcol = np.asarray(spec["groutColor"])[None, None, :] * (0.65 + 0.7 * detail[..., None])
    albedo = mix(albedo, groutcol, np.clip(grout + spill * 0.8, 0, 1))
    albedo = albedo * (1 - 0.4 * cracks[..., None])
    rough_lo, rough_hi = spec["roughness"]
    roughness = rough_lo + (rough_hi - rough_lo) * np.clip(0.4 * detail + grout + spill + 0.3 * lichen_mask, 0, 1)
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


def strata(spec, size, rng):
    """Dark slate cliff: terraced strata broken by near-vertical joints, moss in the deepest crevices."""
    bands = spec["strataBands"]
    warp = fbm(size, 2, 4, rng)
    coords = (np.arange(size) + 0.5) / size
    _, y = np.meshgrid(coords, coords, indexing="xy")
    phase = y * bands + warp * 1.2
    within = phase - np.floor(phase)
    # Each stratum is a ledge: flat on top, falling away to the one below.
    ledge = smoothstep(0.0, 0.18, within) * (1 - 0.55 * smoothstep(0.55, 1.0, within))
    joints_f1, joints_f2, block_ids = cellular(size, 12, rng, jitter=0.7, stretch=(3.0, 0.35))
    joints = 1.0 - smoothstep(0.0, 0.035, joints_f2 - joints_f1)
    blocky = per_cell(block_ids, 12, rng, 0.2)
    detail = fbm(size, 16, 5, rng)
    height = 0.45 * ledge + 0.25 * detail + 0.2 * (0.5 + blocky) - 0.4 * joints
    height = np.clip(height, 0, 1)
    light = smoothstep(0.45, 0.85, height)
    rock = mix(spec["rock"], spec["rockLight"], light * 0.75) * (0.82 + 0.3 * detail[..., None])
    crevice = smoothstep(0.3, 0.05, height) * smoothstep(0.4, 0.65, fbm(size, 6, 3, rng))
    albedo = mix(rock, spec["crevice"], crevice * 0.85)
    albedo = albedo * (1 - 0.35 * joints[..., None]) * (1 - 0.25 * (1 - ledge[..., None]))
    rough_lo, rough_hi = spec["roughness"]
    roughness = rough_lo + (rough_hi - rough_lo) * np.clip(0.5 * detail + 0.6 * crevice + 0.3 * joints, 0, 1)
    return albedo, height, roughness

RECIPES = {"flagstones": flagstones, "moss": moss, "pebbles": pebbles, "strata": strata}


def normal_map(height, strength):
    """A tangent-space normal map from the height field, wrapping at the edges (OpenGL-style +Y flipped for Unreal)."""
    dx = (np.roll(height, -1, axis=1) - np.roll(height, 1, axis=1)) * 0.5
    dy = (np.roll(height, -1, axis=0) - np.roll(height, 1, axis=0)) * 0.5
    size = height.shape[0]
    scale = strength * size / 256.0
    nx, ny, nz = -dx * scale, dy * scale, np.ones_like(height)
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
        albedo, height, roughness = RECIPES[spec["recipe"]](spec, size, rng)
        height = (height - height.min()) / max(height.max() - height.min(), 1e-6)
        normal = normal_map(height, spec["normalStrength"])
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
        rng = np.random.default_rng([profile["seed"], len(profile["layers"])])
        macro = np.stack([fbm(size, 2, 5, rng), fbm(size, 3, 4, rng), fbm(size, 2, 4, rng)], axis=-1)
        macro = (macro - macro.min(axis=(0, 1))) / np.maximum(macro.max(axis=(0, 1)) - macro.min(axis=(0, 1)), 1e-6)
        path = out_dir / "T_Crucible_Macro.png"
        to_image(macro).save(path, optimize=False)
        manifest["files"].append({"name": path.name, "layer": "Macro", "tileMetres": profile["macroTileMetres"],
                                  "sha256": hashlib.sha256(path.read_bytes()).hexdigest()})
        # The river's ripples: a normal map whose crests run across the flow (the texture's U axis), and a foam
        # pattern in its alpha for the shore.
        water = profile["water"]
        rng = np.random.default_rng([profile["seed"], len(profile["layers"]) + 1])
        ripples = 0.6 * fbm(size, 6, 4, rng) + 0.4 * fbm(size, 12, 3, rng)
        _, cells_f2, _ = cellular(size, 40, rng, stretch=(1.0, 0.6))
        cells_f1 = cellular(size, 40, np.random.default_rng(rng.integers(1 << 30)), stretch=(1.0, 0.6))[0]
        foam = smoothstep(0.55, 0.85, 1.0 - np.abs(cells_f2 - cells_f1) * 6.0) * smoothstep(0.35, 0.7, fbm(size, 8, 3, rng))
        water_map = np.concatenate([normal_map(ripples, water["rippleStrength"]), foam[..., None]], axis=-1)
        path = out_dir / "T_Crucible_WaterNormal.png"
        to_image(water_map).save(path, optimize=False)
        manifest["files"].append({"name": path.name, "layer": "Water", "tileMetres": water["tileMetres"],
                                  "sha256": hashlib.sha256(path.read_bytes()).hexdigest()})
    if not only and not args.size:
        (SOURCE / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8", newline="\n")


if __name__ == "__main__":
    main()
