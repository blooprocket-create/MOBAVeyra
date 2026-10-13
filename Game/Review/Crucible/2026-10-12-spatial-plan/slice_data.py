"""Data for the ADR-073 §6 prototype slice: Team A's keep, top lane and top inner and outer jungle.

Runs plan.py for the layout, cuts the slice out of it and writes Game/Saved/CrucibleSlice/slice.json in WORLD
coordinates (World.json frame: world.x = screen Y, world.y = screen X). BuildCrucibleSlice.py reads it inside the
editor. Masses carry their stepped profile as layer polygons; the floor is a vertex-coloured grid; trees are scattered
on forest tops. Seeded, so two runs write the same file.
"""
from __future__ import annotations

import contextlib
import hashlib
import io
import json
import math
import random
import runpy
from pathlib import Path

import numpy as np
import shapely
from shapely.geometry import LineString, MultiPolygon, Point, Polygon, box
from shapely.ops import unary_union

HERE = Path(__file__).resolve().parent
OUT = Path("C:/dev/MOBAVeyra/Game/Saved/CrucibleSlice")
OUT.mkdir(parents=True, exist_ok=True)

with contextlib.redirect_stdout(io.StringIO()):
    plan = runpy.run_path(str(HERE / "plan.py"))

SLICE = box(-9200, -8200, 400, 4400)  # screen frame: keep, top lane leg, top inner and outer jungle, north Well
FLOOR_STEP = 100                      # floor grid spacing
SEED = 7303

# Profiles: (inset from the gameplay edge, bottom share of height, top share). The first layer is never inset, so the
# footprint at walking height IS the gameplay edge; every later layer only steps inward (ADR-073 §5).
PROFILES = {
    "cliff": {"height": 900, "layers": [(0, None, 0.56), (70, 0.56, 0.80), (150, 0.80, 1.0)]},
    "rock": {"height": 650, "layers": [(0, None, 0.62), (80, 0.62, 1.0)]},
    "forest": {"height": 320, "layers": [(0, None, 0.72), (50, 0.72, 1.0)]},
}
SINK = 160          # mass bases sink below the floor so no seam shows at their foot
FOREST_MIN_AREA = 1_200_000  # 120 m2: larger interior masses are forest, smaller ones rock outcrops


def to_world(p):
    return [round(p[1], 1), round(p[0], 1)]


def rings(geom):
    polys = geom.geoms if isinstance(geom, MultiPolygon) else [geom]
    return [p for p in polys if p.geom_type == "Polygon" and not p.is_empty]


def polygon_points(poly, tolerance=12):
    simple = poly.simplify(tolerance, preserve_topology=True)
    coords = list(simple.exterior.coords)[:-1]
    # Clockwise by the shoelace sum in WORLD x/y: Unreal is left-handed, so this is the order whose caps face up when
    # Geometry Script extrudes the polygon (found on the first capture: the other order culls every cap).
    world = [to_world(c) for c in coords]
    area = sum(a[0] * b[1] - b[0] * a[1] for a, b in zip(world, world[1:] + world[:1]))
    return list(reversed(world)) if area > 0 else world


def jitter_inward(poly, rng, reach=55.0):
    """Moves each vertex of an upper stratum part of the way toward a ring inside it, so its face reads as broken rock.
    Only ever inward: the outline stays within its layer, never past the gameplay edge."""
    inner = poly.buffer(-reach)
    if inner.is_empty or inner.geom_type != "Polygon":
        return poly
    simple = poly.simplify(10, preserve_topology=True)
    moved = []
    for x, y in list(simple.exterior.coords)[:-1]:
        q = inner.exterior.interpolate(inner.exterior.project(Point(x, y)))
        f = rng.uniform(0.0, 0.8)
        moved.append((x + (q.x - x) * f, y + (q.y - y) * f))
    candidate = Polygon(moved).buffer(0)
    return candidate if candidate.geom_type == "Polygon" and candidate.within(poly.buffer(1)) else poly


playable = plan["playable"]
boundary_ring = playable.exterior.buffer(5)
masses = []
for index, mass in enumerate(m for m in plan["blocked"].geoms):
    piece = mass.intersection(SLICE)
    for part in rings(piece):
        if part.area < 200_000:
            continue
        touches_edge = part.intersects(boundary_ring)
        kind = "cliff" if touches_edge else ("forest" if part.area >= FOREST_MIN_AREA else "rock")
        profile = PROFILES[kind]
        height = profile["height"]
        layers = []
        for inset, bottom, top in profile["layers"]:
            shape = part if inset == 0 else part.buffer(-inset, join_style="round", quad_segs=4)
            for sub in rings(shape):
                if sub.area < 60_000:
                    continue
                if sub.interiors:
                    sub = Polygon(sub.exterior)  # a hole would be inside rock anyway; the top cap covers it
                if inset > 0:
                    sub = jitter_inward(sub, random.Random(SEED * 31 + index * 7 + inset))
                layers.append({
                    "points": polygon_points(sub),
                    "bottom": -SINK if bottom is None else round(bottom * height, 1),
                    "top": round(top * height, 1),
                })
        trees = []
        if kind == "forest":
            rng = random.Random(SEED + index)
            canopy = part.buffer(-140)
            minx, miny, maxx, maxy = part.bounds
            step = 200
            y = miny + rng.uniform(0, step)
            while y < maxy:
                x = minx + rng.uniform(0, step)
                while x < maxx:
                    px, py = x + rng.uniform(-90, 90), y + rng.uniform(-90, 90)
                    if canopy.contains(Point(px, py)):
                        trees.append({"at": to_world((px, py)) + [height * profile["layers"][-1][2]],
                                      "kind": "tree" if rng.random() < 0.72 else "shrub",
                                      "scale": round(rng.uniform(0.8, 1.25), 2), "yaw": round(rng.uniform(0, 360), 1)})
                    x += step
                y += step
        if kind == "forest":
            rim = part.buffer(-75).exterior if part.buffer(-75).geom_type == "Polygon" else None
            if rim is not None:
                d = 0.0
                while d < rim.length:
                    p = rim.interpolate(d)
                    trees.append({"at": to_world((p.x, p.y)) + [height * profile["layers"][-1][2]], "kind": "shrub",
                                  "scale": round(rng.uniform(0.75, 1.05), 2), "yaw": round(rng.uniform(0, 360), 1)})
                    d += 150.0
        masses.append({"id": f"M{index:02d}", "kind": kind, "height": height, "areaM2": round(part.area / 1e4, 1),
                       "edge": polygon_points(part, tolerance=4),
                       "layers": layers, "trees": trees})

# ---------------------------------------------------------------- floor: heights and vertex colours on a grid
lane_lines = [LineString(l) for l in plan["lanes"].values()]
river_line = LineString(plan["river_walk_line"])
keep_a = plan["keep_a"]
islands = plan["islands"]
arenas = plan["arenas"]
camps = unary_union([Point(c).buffer(r) for c, r, k in plan["clearings_a"].values() if k in ("camp", "newcamp")])
ENV = plan["ENV"]
WATER_HALF = plan["RIVER_WATER"] / 2
BANK = plan["RIVER_BANK"]
minx, miny, maxx, maxy = SLICE.bounds
xs = np.arange(minx, maxx + 1, FLOOR_STEP)
ys = np.arange(miny, maxy + 1, FLOOR_STEP)
gx, gy = np.meshgrid(xs, ys)
px, py = gx.ravel(), gy.ravel()


def smooth(edge0, edge1, x):
    t = np.clip((x - edge0) / (edge1 - edge0), 0.0, 1.0)
    return t * t * (3 - 2 * t)


JUNGLE_Z, KEEP_Z, BED_Z = 45.0, 70.0, -290.0


def ground(px, py):
    """The floor at screen-frame points: its height and what it's made of, for the grid and for anything set on it."""
    points = shapely.points(px, py)
    d_lane = np.min([shapely.distance(line, points) for line in lane_lines], axis=0)
    d_river = shapely.distance(river_line, points)
    in_keep = shapely.contains_xy(keep_a, px, py)
    in_island = shapely.contains_xy(islands, px, py)
    in_arena = shapely.contains_xy(arenas, px, py)
    in_camp = shapely.contains_xy(camps, px, py)
    camp_depth = np.where(in_camp, shapely.distance(camps.boundary, points), 0.0)  # how far inside a camp's ground
    jungle = smooth(ENV - 120, ENV + 260, d_lane)               # 0 on the road and shoulder, 1 in the jungle
    height = JUNGLE_Z * jungle
    height = np.where(in_keep, np.maximum(height, KEEP_Z), height)
    # River: the bed under the water, banks rising to the jungle; arenas' water ring is wet too, islands stand dry.
    wet_d = np.where(in_arena & ~in_island, 0.0, d_river)
    bank = smooth(WATER_HALF - 120, WATER_HALF + BANK, wet_d)
    height = np.where(wet_d < WATER_HALF + BANK, BED_Z + (height - BED_Z) * bank, height)
    height = np.where(in_island, 20.0, height)
    road = 1.0 - smooth(plan["ROAD"] / 2 - 40, plan["ROAD"] / 2 + 60, d_lane)
    road = np.maximum(road, np.where(in_keep, 0.85, 0.0))
    shore = np.clip(1.0 - smooth(WATER_HALF + BANK - 60, WATER_HALF + BANK + 220, wet_d), 0, 1) * (1 - road)
    # Camp ground: trodden earth that fades in from the clearing's edge, never a hard disc.
    earth = 0.42 * smooth(0.0, 420.0, camp_depth) * (1 - road) * (1 - shore)
    moss = np.clip(1.0 - road - shore - earth, 0, 1)
    return {"height": height, "road": road, "moss": moss, "earth": earth, "shore": shore, "wet_d": wet_d,
            "d_lane": d_lane, "in_keep": in_keep, "camp_depth": camp_depth, "in_island": in_island}


G = ground(px, py)
height = G["height"]
colours = np.stack([G["road"], G["moss"], G["earth"], G["shore"]], axis=1)
nx, ny = len(xs), len(ys)
vertices = [to_world((x, y)) + [round(float(z), 1)] for x, y, z in zip(px, py, height)]
triangles = []
for j in range(ny - 1):
    for i in range(nx - 1):
        a, b, c, d = j * nx + i, j * nx + i + 1, (j + 1) * nx + i, (j + 1) * nx + i + 1
        triangles += [[a, c, b], [b, c, d]]

# ---------------------------------------------------------------- ink lines and ground clutter
blocked = plan["blocked"]


def at_ground(points2d):
    arr = np.array(points2d, dtype=float)
    g = ground(arr[:, 0], arr[:, 1])
    return g


inks = []
for mass_spec, part in ((m, None) for m in masses):
    pass
# Base ink: a dark line where every mass meets the floor, just outside its gameplay edge, following the ground.
for mass in plan["blocked"].geoms:
    piece = mass.intersection(SLICE)
    for part in rings(piece):
        if part.area < 200_000:
            continue
        ring = part.buffer(10, join_style="mitre").exterior.simplify(14)
        pts = [c for c in list(ring.coords)]
        g = at_ground([(x, y) for x, y in pts])
        inks.append({"points": [to_world((x, y)) + [round(float(z) + 4, 1)] for (x, y), z in zip(pts, g["height"])], "size": 24})
# Top ink: the lip of each mass's top stratum.
for mass in masses:
    top = mass["layers"][-1]
    pts = [[x, y, top["top"] + 2] for x, y in top["points"]] + [[top["points"][0][0], top["points"][0][1], top["top"] + 2]]
    inks.append({"points": pts, "size": 16})

clutter = []
rng = random.Random(SEED + 99)
open_ground = SLICE.difference(blocked)
# Clumps along each mass's foot, where grass gathers against rock and root.
for mass in plan["blocked"].geoms:
    for part in rings(mass.intersection(SLICE)):
        if part.area < 200_000:
            continue
        ring = part.buffer(rng.uniform(70, 150)).exterior
        d = rng.uniform(0, 110)
        while d < ring.length:
            q = ring.interpolate(d)
            clutter.append({"kind": "fern" if rng.random() < 0.35 else "grass", "xy": (q.x, q.y)})
            d += rng.uniform(90, 160)
# Sparse tufts across open jungle ground.
for gx_ in np.arange(SLICE.bounds[0], SLICE.bounds[2], 380):
    for gy_ in np.arange(SLICE.bounds[1], SLICE.bounds[3], 380):
        clutter.append({"kind": "grass", "xy": (gx_ + rng.uniform(-150, 150), gy_ + rng.uniform(-150, 150))})
# Reeds along the river's banks.
for gx_ in np.arange(SLICE.bounds[0], SLICE.bounds[2], 150):
    for gy_ in np.arange(SLICE.bounds[1], SLICE.bounds[3], 150):
        clutter.append({"kind": "reeds", "xy": (gx_ + rng.uniform(-60, 60), gy_ + rng.uniform(-60, 60)), "bank": True})
cxy = np.array([c["xy"] for c in clutter])
g = ground(cxy[:, 0], cxy[:, 1])
inside_mass = shapely.contains_xy(blocked, cxy[:, 0], cxy[:, 1])
kept = []
for i, c in enumerate(clutter):
    if inside_mass[i] or g["in_keep"][i] or g["d_lane"][i] < ENV + 70 or g["camp_depth"][i] > 180:
        continue
    bank_zone = WATER_HALF - 30 < g["wet_d"][i] < WATER_HALF + BANK + 110
    if bool(c.get("bank")) != bank_zone:
        continue
    if not c.get("bank") and g["wet_d"][i] < WATER_HALF + BANK + 110:
        continue
    kept.append({"kind": c["kind"], "at": to_world(c["xy"]) + [round(float(g["height"][i]), 1)],
                 "yaw": round(rng.uniform(0, 360), 1), "scale": round(rng.uniform(0.8, 1.3), 2)})
clutter = kept

# ---------------------------------------------------------------- water, keep, structures, cameras
water = unary_union([plan["river_water"], arenas.difference(islands)]).intersection(SLICE)
water_polys = [polygon_points(p, 6) for p in rings(water) if p.area > 100_000]
# The keep's architecture as solids (polygon, bottom, top, material): wall bodies, merlons along their tops, and at each
# gate two pillars with a lintel between them. Gates are gaps in the wall runs (keep_walls_a).
WALL_H, MERLON_H, PILLAR = 480, 120, 300
solids = []
half = plan["WALL"] / 2
for run in plan["keep_walls_a"]:
    line = LineString(run)
    body = line.buffer(half, cap_style="flat", join_style="mitre")
    solids.append({"material": "ruin", "points": polygon_points(body, 1), "bottom": -60, "top": WALL_H})
    d = 110.0
    while d + 170 < line.length - 60:
        seg = LineString([line.interpolate(d), line.interpolate(d + 170)]).buffer(half + 12, cap_style="flat")
        solids.append({"material": "ruin", "points": polygon_points(seg, 1), "bottom": WALL_H, "top": WALL_H + MERLON_H})
        d += 330.0
WALL_DIRECTION = {"Top": (1.0, 0.0), "Bottom": (0.0, 1.0), "Mid": (math.sqrt(0.5), -math.sqrt(0.5))}
for lane, c in plan["gates_a"].items():
    ux, uy = WALL_DIRECTION[lane]
    reach = plan["GATE"] / 2 + PILLAR / 2
    ends = [(c[0] - ux * reach, c[1] - uy * reach), (c[0] + ux * reach, c[1] + uy * reach)]
    for e in ends:
        square = LineString([(e[0] - ux * PILLAR / 2, e[1] - uy * PILLAR / 2), (e[0] + ux * PILLAR / 2, e[1] + uy * PILLAR / 2)]).buffer(PILLAR / 2, cap_style="flat")
        solids.append({"material": "ruin", "points": polygon_points(square, 1), "bottom": -60, "top": 800})
    lintel = LineString(ends).buffer(half + 10, cap_style="flat")
    solids.append({"material": "ruin", "points": polygon_points(lintel, 1), "bottom": 590, "top": 740})
structures = (
    [{"kind": "PrimeWell", "at": to_world(plan["prime_well_a"])}]
    + [{"kind": "BaseTower", "at": to_world(p)} for p in plan["base_towers_a"]]
    + [{"kind": "Inhibitor", "at": to_world(p)} for p in plan["inhib_a"].values()]
    + [{"kind": "LaneSpire", "at": to_world(p)} for lane in ("Top", "Mid") for p in plan["spire_points"](lane)
       if SLICE.contains(Point(p))]
)


def cam(name, screen_xy, mode="game"):
    return {"name": name, "target": to_world(screen_xy), "mode": mode}


cameras = [
    cam("Gate_Top", (-6300, -4300)), cam("Keep_Inside", (-6600, -6300)), cam("Stonehorn", (-4300, -1700)),
    cam("TopCrossroads", (-3000, -500)), cam("Ashfang_River", (-3500, 1800)), cam("NorthWell", (-2796, 3200)),
    cam("TopLane_Mid", (-6300, -400)), cam("Razorback_Outer", (-7950, 1100)), cam("TopElbow", (-6200, 3700)),
    cam("Overview", (SLICE.centroid.x, SLICE.centroid.y), "overview"),
]
data = {
    "frame": "world (World.json): x = screen Y, y = screen X; Team A only; units cm",
    "seed": SEED, "camera": {"distance": 1600, "pitch": -60, "fov": 90},
    "masses": masses, "floor": {"vertices": vertices, "triangles": triangles, "colours": np.round(colours, 3).tolist()},
    "water": {"z": -220, "polygons": water_polys}, "solids": solids, "structures": structures,
    "fog": [{"center": to_world(c) + [0], "radius": r} for _, lobes in plan["fog_a"] for c, r in lobes if SLICE.contains(Point(c))],
    "cameras": cameras,
    "inks": inks,
    "clutter": clutter,
    # Lighting test against the style's provisional sun (CrucibleStyle.json lighting): a higher sun shortens the shadows
    # that blanket clearings at the gameplay camera, and more sky light lifts what shadow remains.
    "lighting": {"sunPitch": -58, "skyIntensity": 4.4},
}
text = json.dumps(data, separators=(",", ":"))
(OUT / "slice.json").write_text(text, encoding="utf-8")
summary = {k: sum(1 for m in masses if m["kind"] == k) for k in PROFILES}
print(json.dumps({"bytes": len(text), "sha256": hashlib.sha256(text.encode()).hexdigest()[:16], "masses": summary,
                  "trees": sum(len(m["trees"]) for m in masses), "floorVertices": len(vertices),
                  "layers": sum(len(m["layers"]) for m in masses), "water": len(water_polys),
                  "structures": len(structures), "fogLobes": len(data["fog"]), "inks": len(inks), "clutter": len(clutter)}, indent=1))
