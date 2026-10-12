"""Crucible whole-map spatial design proposal (playable spaces first).

Screen frame: X right, Y up; Team A's keep bottom-left, Team B's top-right.
World frame (World.json): world.x = Y, world.y = X. Team B = half turn of Team A.
Outputs: proposal.json (design data, screen frame), metrics.json, plan.svg, current.svg.
"""
from __future__ import annotations

import heapq
import json
import math
import subprocess
from pathlib import Path

from shapely.geometry import LineString, Point, Polygon, box
from shapely.ops import unary_union

OUT = Path(__file__).resolve().parent
REPO = Path("C:/dev/MOBAVeyra")

# ---------------------------------------------------------------- proportions
HALF_EXTENT = 10000          # was 9000
BOUNDARY = 800               # boundary cliff band (unchanged)
P = HALF_EXTENT - BOUNDARY   # playable half width: 9200
ROAD, SHOULDER = 700, 250
ENV = ROAD / 2 + SHOULDER    # lane envelope half width: 600
CORRIDOR = 600               # jungle route width
GATE = 900                   # keep gate opening
WALL = 220                   # keep wall thickness (drawn)
KEEP = 4500                  # keep walls stand 4500 from the centre on each axis
CHAMFER = 5800               # chamfer from (-5800,-4500) to (-4500,-5800)
SIDE = 6300                  # side lanes' straight legs at |X| or |Y| = 6300
ELBOW_R = 3800               # rounded lane elbows
RIVER_A, RIVER_L = 600, 6000 # river S-curve amplitude and half-wavelength
RIVER_WATER, RIVER_BANK = 1000, 250
WELL_S = 4400                # Flux Well sites along the river
ARENA_R, ISLAND_R = 1350, 700
SPEED = 330                  # typical Vanguard move speed (Vanguards.json)

U = (-math.sqrt(0.5), math.sqrt(0.5))  # along the river, centre -> top-left
V = (math.sqrt(0.5), math.sqrt(0.5))   # across the river, toward Team B


def rot(p):
    return (-p[0], -p[1])


def river_p(s):
    return RIVER_A * math.sin(math.pi * s / RIVER_L)


def river_point(s, offset=0.0):
    p = river_p(s) + offset
    return (s * U[0] + p * V[0], s * U[1] + p * V[1])


def arc(center, radius, a0, a1, step_deg=5):
    n = max(2, int(abs(a1 - a0) / step_deg) + 1)
    return [(center[0] + radius * math.cos(math.radians(a0 + (a1 - a0) * i / (n - 1))),
             center[1] + radius * math.sin(math.radians(a0 + (a1 - a0) * i / (n - 1)))) for i in range(n)]


# ---------------------------------------------------------------- keeps
keep_a = Polygon([(-P, -P), (-P, -KEEP), (-CHAMFER, -KEEP), (-KEEP, -CHAMFER), (-KEEP, -P)])
keep_b = Polygon([rot(p) for p in keep_a.exterior.coords])
mid_gate_c = (-(CHAMFER + KEEP) / 2, -(CHAMFER + KEEP) / 2)
g = GATE / 2 * math.sqrt(0.5)
keep_walls_a = [  # architectural perimeter, gaps are the three gates
    [(-P, -KEEP), (-SIDE - GATE / 2, -KEEP)],
    [(-SIDE + GATE / 2, -KEEP), (-CHAMFER, -KEEP), (mid_gate_c[0] - g, mid_gate_c[1] + g)],
    [(mid_gate_c[0] + g, mid_gate_c[1] - g), (-KEEP, -CHAMFER), (-KEEP, -SIDE + GATE / 2)],
    [(-KEEP, -SIDE - GATE / 2), (-KEEP, -P)],
]
gates_a = {"Top": (-SIDE, -KEEP), "Mid": mid_gate_c, "Bottom": (-KEEP, -SIDE)}
prime_well_a = (-7000, -7000)
fountain_a, fountain_r = (-8450, -8450), 650
base_towers_a = [(prime_well_a[0] + 742, prime_well_a[1] + 248), (prime_well_a[0] + 248, prime_well_a[1] + 742)]
inhib_a = {"Top": (-SIDE, -5000), "Mid": (-5500, -5500), "Bottom": (-5000, -SIDE)}

# ---------------------------------------------------------------- lanes (A -> B)
top_lane = ([inhib_a["Top"], (-SIDE, -KEEP), (-SIDE, 2500)]
            + arc((-2500, 2500), ELBOW_R, 180, 90)[1:]
            + [(5000, SIDE)])
bottom_lane = [rot(p) for p in reversed(top_lane)]
mid_lane = [inhib_a["Mid"], (5500, 5500)]
lanes = {"Top": top_lane, "Mid": mid_lane, "Bottom": bottom_lane}
spire_distances = {"Top": [900, 3900, 7300], "Mid": [800, 3000, 5200], "Bottom": [900, 3900, 7300]}
# Spires stand beside the road, not on the centre line (author, 2026-10-12): 450 to one side, alternating along each
# lane, measured from the owner's walking direction (+1 its left) so the half turn keeps both teams' towers equivalent.
SPIRE_OFFSET = 450
# Top and bottom share their sides: the half turn carries one onto the other, keeping left as left along each walk.
spire_sides = {"Top": [1, -1, 1], "Mid": [-1, 1, -1], "Bottom": [1, -1, 1]}


def spire_points(lane, from_b=False):
    pts = list(reversed(lanes[lane])) if from_b else lanes[lane]
    line = LineString(pts)
    out = []
    for d, side in zip(spire_distances[lane], spire_sides[lane]):
        a, b = line.interpolate(max(0, d - 5)), line.interpolate(d + 5)
        dx, dy = b.x - a.x, b.y - a.y
        n = math.hypot(dx, dy)
        c = line.interpolate(d)
        out.append((c.x - dy / n * side * SPIRE_OFFSET, c.y + dx / n * side * SPIRE_OFFSET))
    return out

# ---------------------------------------------------------------- river and Wells
river_line = [river_point(s) for s in range(-15000, 15001, 250)]
well_n = river_point(WELL_S)
well_s = rot(well_n)
ford_s = 10800  # outer fords where the corner routes meet the river
ford_tl = river_point(ford_s)
ford_br = rot(ford_tl)
RIVER_END = ford_s + 700  # past this the river drops through a gorge: water, not ground
river_walk_line = [river_point(s) for s in range(-RIVER_END, RIVER_END + 1, 250)]
gorge_lines = [[river_point(s) for s in range(RIVER_END, 15001, 250)], [river_point(-s) for s in range(RIVER_END, 15001, 250)]]

# ---------------------------------------------------------------- Team A jungle (playable spaces)
# Route widths: a hierarchy, not uniform tubes.
ROTATION, APPROACH, OUTER = 1000, 700, 900
# Clearings: name -> (centre, radius, kind). Camps keep their current species and roles.
clearings_a = {
    "stonehorn":  ((-4300, -1700), 800, "camp"),
    "ashfang":    ((-3800, 1650), 800, "camp"),
    "miremother": ((-2000, -4600), 800, "camp"),
    "skittermaw": ((900, -4500), 900, "camp"),
    "razorback":  ((-7950, 1100), 800, "camp"),
    "gloomwing":  ((-300, -8000), 800, "camp"),
    "top crossroads": ((-3000, -500), 900, "junction"),
    "bot crossroads": ((-700, -2950), 850, "junction"),
    "top river mouth": (river_point(1800, -1100), 800, "junction"),
    "bot river mouth": (river_point(-1800, -1100), 800, "junction"),
    "top-outer glade": ((-7900, -2300), 800, "junction"),
    "bot-outer glade": ((3600, -8100), 800, "newcamp"),
    "bot elbow pocket": ((5600, -6700), 850, "junction"),
    "top elbow pocket": ((-8100, 4500), 800, "newcamp"),
}


def lane_at(lane, y=None, x=None):
    """The lane's centre-line point at a given Y (vertical leg) or X (horizontal leg)."""
    best = None
    for a, b in zip(lanes[lane], lanes[lane][1:]):
        seg = LineString([a, b])
        probe = LineString([(-20000, y), (20000, y)]) if y is not None else LineString([(x, -20000), (x, 20000)])
        hit = seg.intersection(probe)
        if not hit.is_empty:
            pt = hit if hit.geom_type == "Point" else hit.representative_point()
            if best is None or (y is not None and abs(pt.x) > abs(best[0])) or (x is not None and abs(pt.y) > abs(best[1])):
                best = (pt.x, pt.y)
    return best


C = {k: v[0] for k, v in clearings_a.items()}
bl3_lane = (2500 + ELBOW_R * math.cos(math.radians(-80)), -2500 + ELBOW_R * math.sin(math.radians(-80)))
bl3_bank = (3650, -4950)
pocket_lane = (2500 + ELBOW_R * math.cos(math.radians(-55)), -2500 + ELBOW_R * math.sin(math.radians(-55)))
# Corridors: name -> (polyline, width). Lane ends sit on the lane centre line.
corridors_a = {
    # top-side inner jungle: a crossroads hub with a loop by the river
    "TL1 lane mouth": ([lane_at("Top", y=-2700), (-5200, -2300), C["stonehorn"]], APPROACH),
    "stonehorn back door (mid)": ([(-4200, -4200), (-4600, -3000), C["stonehorn"]], APPROACH),
    "stonehorn - crossroads": ([C["stonehorn"], (-3600, -1250), C["top crossroads"]], ROTATION),
    "M1 mid mouth": ([(-2500, -2500), (-2450, -1550), C["top crossroads"]], APPROACH),
    "TL2 lane mouth": ([lane_at("Top", y=-300), (-4700, 100), C["top crossroads"]], APPROACH),
    "crossroads - ashfang": ([C["top crossroads"], (-3300, 600), C["ashfang"]], ROTATION),
    "crossroads - river mouth": ([C["top crossroads"], (-2100, 300), C["top river mouth"]], ROTATION),
    "ashfang - river mouth": ([C["ashfang"], (-2600, 1350), C["top river mouth"]], APPROACH),
    "TL3 lane mouth": ([lane_at("Top", y=3300), (-5000, 2700), C["ashfang"]], APPROACH),
    "ashfang - north Well": ([C["ashfang"], (-3450, 2700), well_n], ROTATION),
    # bot-side inner jungle: a low path along the lane instead of the top side's loop
    "BL1 lane mouth": ([lane_at("Bottom", x=-3300), (-3000, -5250), C["miremother"]], APPROACH),
    "miremother back door (mid)": ([(-3900, -3900), (-3200, -3950), C["miremother"]], APPROACH),
    "miremother - crossroads": ([C["miremother"], (-1450, -3650), C["bot crossroads"]], ROTATION),
    "M2 mid mouth": ([(-1500, -1500), (-1100, -2100), C["bot crossroads"]], APPROACH),
    "crossroads - river mouth": ([C["bot crossroads"], C["bot river mouth"]], ROTATION),
    "crossroads - skittermaw": ([C["bot crossroads"], (200, -3600), C["skittermaw"]], ROTATION),
    "skittermaw - river mouth": ([C["skittermaw"], (800, -3300), C["bot river mouth"]], APPROACH),
    "miremother - skittermaw low path": ([C["miremother"], (-700, -5000), C["skittermaw"]], APPROACH),
    "BL2 lane mouth": ([lane_at("Bottom", x=2200), (2000, -5300), C["skittermaw"]], APPROACH),
    "skittermaw - south Well": ([C["skittermaw"], (1500, -3400), well_s], ROTATION),
    "BL3 elbow river path": ([bl3_lane, (3500, -5600), bl3_bank], APPROACH),
    # top-side outer jungle (left strip): a looped route around a central mass
    "OL0 gate mouth": ([lane_at("Top", y=-3900), (-7300, -3500), C["top-outer glade"]], APPROACH),
    "outer route south": ([C["top-outer glade"], (-7650, -700), C["razorback"]], OUTER),
    "outer west loop": ([C["top-outer glade"], (-8800, -1300), (-8800, 500), C["razorback"]], APPROACH),
    "OL1 lane mouth": ([lane_at("Top", y=1100), C["razorback"]], APPROACH),
    "outer route north": ([C["razorback"], (-8450, 3200), C["top elbow pocket"]], OUTER),
    "OL2 elbow mouth": ([arc((-2500, 2500), ELBOW_R, 180, 90)[5], (-6600, 4800), C["top elbow pocket"]], APPROACH),
    "top outer ford": ([C["top elbow pocket"], (-8300, 6500), ford_tl], OUTER),
    # bot-side outer jungle (bottom strip): one long route with glades
    "OB0 gate mouth": ([lane_at("Bottom", x=-3900), (-3700, -7400), (-2200, -8100), C["gloomwing"]], OUTER),
    "OB1 lane mouth": ([lane_at("Bottom", x=1500), (1300, -7300), C["gloomwing"]], APPROACH),
    "outer route east": ([C["gloomwing"], (1800, -8500), C["bot-outer glade"]], OUTER),
    "outer route to pocket": ([C["bot-outer glade"], (4700, -7700), C["bot elbow pocket"]], OUTER),
    "OB3 lane mouth": ([(2500 + ELBOW_R * math.cos(math.radians(-78)), -2500 + ELBOW_R * math.sin(math.radians(-78))), (3450, -7200), C["bot-outer glade"]], APPROACH),
    "OB2 elbow mouth": ([pocket_lane, (4900, -6300), C["bot elbow pocket"]], APPROACH),
    "bot outer ford": ([C["bot elbow pocket"], (7000, -7200), ford_br], OUTER),
}
fog_a = [  # Dense Fog banks: groups of overlapping lobes (centre, radius)
    ("top-inner lane mouth", [((-5150, -350), 300), ((-4850, -100), 280), ((-5100, 200), 250)]),
    ("top river entrance", [((-2050, 700), 300), ((-1800, 430), 260), ((-2300, 950), 240)]),
    ("ashfang flank", [((-4800, 2300), 300), ((-4550, 2620), 260)]),
    ("stonehorn approach", [((-5000, -2550), 260), ((-5150, -2250), 240)]),
    ("top-outer bend", [((-7600, -1450), 320), ((-7850, -1150), 280)]),
    ("top elbow approach", [((-7900, 3900), 300), ((-7600, 4200), 280), ((-7350, 4450), 240)]),
    ("bot-inner crossroads flank", [((-1250, -3550), 300), ((-950, -3850), 260)]),
    ("skittermaw lane flank", [((1750, -5150), 280), ((2050, -4950), 250)]),
    ("bot elbow river path", [((3050, -4750), 260), ((3300, -5000), 240)]),
    ("bot-outer west", [((-2600, -7600), 300), ((-2300, -7900), 270)]),
    ("bot-outer east", [((3000, -7950), 300), ((3300, -7650), 260)]),
    ("north Well approach", [((-3350, 2750), 260)]),
]


def edge_ribbon(lane, d0, d1, side, r=260, step=380):
    """A fog ribbon hugging a lane shoulder from outside: lobes at the envelope edge, never on it.
    side +1 is the lane's left (seen walking from Team A's end), -1 its right."""
    line = LineString(lanes[lane])
    lobes, d = [], d0
    while d <= d1 + 1:
        a, b = line.interpolate(max(0, d - 5)), line.interpolate(d + 5)
        dx, dy = b.x - a.x, b.y - a.y
        n = math.hypot(dx, dy)
        nx, ny = -dy / n * side, dx / n * side
        c = line.interpolate(d)
        lobes.append(((round(c.x + nx * (ENV + r + 15)), round(c.y + ny * (ENV + r + 15))), r))
        d += step
    return lobes


# Lane-edge banks (author, 2026-10-12): fog on lane edges for outplays, never on road or shoulder,
# and kept 500 from any lane structure. Distances run from Team A's inhibitor along each lane.
fog_a += [
    ("top lane edge by TL1 (inner)", edge_ribbon("Top", 2300, 3100, -1)),
    ("top lane edge by OL0 (outer)", edge_ribbon("Top", 2000, 2700, +1)),
    ("top lane edge by Razorback (outer)", edge_ribbon("Top", 5600, 6300, +1)),
    ("top elbow inner edge", edge_ribbon("Top", 9000, 9900, -1)),
    ("mid edge by the Stonehorn door (top side)", edge_ribbon("Mid", 1700, 2300, +1)),
    ("mid edge by M2 (bot side)", edge_ribbon("Mid", 3800, 4400, -1)),
    ("bot lane edge by the low path (inner)", edge_ribbon("Bottom", 4700, 5500, +1)),
    ("bot lane edge by OB1 (outer)", edge_ribbon("Bottom", 5600, 6300, -1)),
    ("bot elbow inner edge", edge_ribbon("Bottom", 9500, 10400, +1)),
]

# ---------------------------------------------------------------- geometry
playable = box(-P, -P, P, P)
keeps = unary_union([keep_a, keep_b])
lane_env = unary_union([LineString(l).buffer(ENV) for l in lanes.values()])
lane_road = unary_union([LineString(l).buffer(ROAD / 2) for l in lanes.values()])
river_band = LineString(river_walk_line).buffer(RIVER_WATER / 2 + RIVER_BANK, cap_style="flat").intersection(playable)
river_water = LineString(river_walk_line).buffer(RIVER_WATER / 2, cap_style="flat").intersection(playable)
gorge_water = unary_union([LineString(g).buffer(RIVER_WATER / 2 - 150, cap_style="flat") for g in gorge_lines]).intersection(playable)
arenas = unary_union([Point(w).buffer(ARENA_R) for w in (well_n, well_s)])
islands = unary_union([Point(w).buffer(ISLAND_R) for w in (well_n, well_s)])


def both(shapes):
    out = []
    for s in shapes:
        out.append(s)
        out.append(Polygon([rot(p) for p in s.exterior.coords]) if s.geom_type == "Polygon" else s)
    return out


def turned(geom):
    """Team B's copy of a Team A shape: the half turn about the centre."""
    from shapely import affinity
    return affinity.rotate(geom, 180, origin=(0, 0))


MIN_MASS_THICKNESS = 300   # no blocked strip thinner than this survives (no raised strips)
MIN_MASS_AREA = 250000     # 25 m2: smaller leftovers become open ground
mid_ford = Point(0, 0).buffer(1100)
clear_shapes_a = [Point(c).buffer(r) for c, r, _ in clearings_a.values()]
corr_shapes_a = [LineString(pl).buffer(w / 2) for pl, w in corridors_a.values()]
jungle_open_a = unary_union(clear_shapes_a + corr_shapes_a)
jungle_open = unary_union([jungle_open_a, turned(jungle_open_a)])
fixed_open = unary_union([lane_env, river_band, arenas, keeps, mid_ford])
open_ground = unary_union([fixed_open, jungle_open]).intersection(playable)
blocked_raw = playable.difference(open_ground)
kept, freed = [], []
for mass in (blocked_raw.geoms if blocked_raw.geom_type == "MultiPolygon" else [blocked_raw]):
    (freed if mass.area < MIN_MASS_AREA or mass.buffer(-MIN_MASS_THICKNESS / 2).is_empty else kept).append(mass)
blocked = unary_union(kept)
jungle_open = unary_union([jungle_open, *freed])
open_ground = unary_union([open_ground, *freed])
jungle_region = playable.difference(fixed_open)
walkable_jungle = jungle_region.intersection(jungle_open)

# Team A half = below the river centre line; for jungle shares measure the whole map (B mirrors A).
metrics = {
    "halfExtent": HALF_EXTENT,
    "playableSide": 2 * P,
    "areas_m2": {
        "playable": round(playable.area / 1e4),
        "lanes": round(lane_env.intersection(playable).area / 1e4),
        "river_and_arenas": round(unary_union([river_band, arenas]).area / 1e4),
        "keeps": round(keeps.area / 1e4),
        "jungle_region": round(jungle_region.area / 1e4),
        "jungle_open": round(walkable_jungle.area / 1e4),
        "blocked_masses": round(blocked.area / 1e4),
    },
}
metrics["jungle_open_share"] = round(walkable_jungle.area / jungle_region.area, 3)
masses = [g for g in (blocked.geoms if blocked.geom_type == "MultiPolygon" else [blocked])]
metrics["blocked_mass_count"] = len(masses)
metrics["smallest_mass_m2"] = round(min(m.area for m in masses) / 1e4, 1)
metrics["largest_mass_m2"] = round(max(m.area for m in masses) / 1e4, 1)
metrics["slivers_opened"] = len(freed)
edge_ring = playable.exterior.buffer(5)
edge_area = sum(m.area for m in masses if m.intersects(edge_ring))
metrics["blocked_edge_rim_share"] = round(edge_area / jungle_region.area, 3)
metrics["blocked_interior_share"] = round((blocked.area - edge_area) / jungle_region.area, 3)
metrics["thinnest_rule"] = MIN_MASS_THICKNESS

# Fog must stay clear of lane roads and shoulders.
fog_shapes_a = [Point(c).buffer(r) for _, lobes in fog_a for c, r in lobes]
fog_all = unary_union(fog_shapes_a + [Point(rot(c)).buffer(r) for _, lobes in fog_a for c, r in lobes])
metrics["fog_lane_overlap_m2"] = round(fog_all.intersection(lane_env).area / 1e4, 2)
metrics["fog_min_clearance_to_lane_envelope"] = round(min(Point(c).buffer(r).distance(lane_env) for _, lobes in fog_a for c, r in lobes))
metrics["fog_bank_count_per_team"] = len(fog_a)
structures = list(inhib_a.values()) + base_towers_a + [prime_well_a]
for lane in lanes:
    structures += spire_points(lane) + spire_points(lane, from_b=True)
structures += [rot(p) for p in list(inhib_a.values()) + base_towers_a + [prime_well_a]]
metrics["fog_min_clearance_to_structures"] = round(min(Point(c).buffer(r).distance(Point(s)) for _, lobes in fog_a for c, r in lobes for s in structures + [rot(x) for x in structures]))
metrics["fog_lobes_per_team"] = sum(len(lobes) for _, lobes in fog_a)

# Camps: clearance from lanes and the river.
camp_report = {}
for name, (c, r, kind) in clearings_a.items():
    if kind not in ("camp", "newcamp"):
        continue
    circle = Point(c).buffer(r)
    camp_report[name] = {
        "clear_of_lane_envelope": round(circle.distance(lane_env)),
        "clear_of_river_band": round(circle.distance(river_band)),
        "entrances": sum(1 for pl, _ in corridors_a.values() if Point(pl[0]).distance(Point(c)) < 1 or Point(pl[-1]).distance(Point(c)) < 1),
    }
metrics["camps"] = camp_report

# ---------------------------------------------------------------- route graph (Team A)
nodes: dict[str, tuple[float, float]] = {}
edges: dict[str, list[tuple[str, float]]] = {}


def add_node(name, p):
    nodes[name] = p
    edges.setdefault(name, [])


def link(a, b, length=None):
    d = length if length is not None else math.dist(nodes[a], nodes[b])
    edges[a].append((b, d))
    edges[b].append((a, d))


def polyline_nodes(prefix, pts, step=200):
    line = LineString(pts)
    n = max(2, int(line.length / step) + 1)
    names = []
    for i in range(n):
        p = line.interpolate(line.length * i / (n - 1))
        name = f"{prefix}{i}"
        add_node(name, (p.x, p.y))
        if names:
            link(names[-1], name)
        names.append(name)
    return names


lane_nodes = {lane: polyline_nodes(f"lane.{lane}.", pts) for lane, pts in lanes.items()}
# Team B's half of each lane is the same polyline; the graph covers the whole lane.
river_nodes = polyline_nodes("river.", river_walk_line)


def nearest(prefixes, p):
    best = min((n for n in nodes if any(n.startswith(x) for x in prefixes)), key=lambda n: math.dist(nodes[n], p))
    return best


for name, (c, r, kind) in clearings_a.items():
    add_node(name, c)
add_node("north Well", well_n)
add_node("south Well", well_s)
add_node("Fountain", fountain_a)
for lane, inh in inhib_a.items():
    link("Fountain", lane_nodes[lane][0], math.dist(fountain_a, inh))
for name, (pl, _w) in corridors_a.items():
    ends = []
    for p in (pl[0], pl[-1]):
        hit = next((n for n in nodes if not n.startswith(("lane.", "river.", "c.")) and math.dist(nodes[n], p) < 1), None)
        if hit is None:
            hit = nearest(("lane.", "river."), p)
            if math.dist(nodes[hit], p) > 1:
                add_node(f"c.{name}.{len(nodes)}", p)
                link(f"c.{name}.{len(nodes) - 1}", hit)
                hit = f"c.{name}.{len(nodes) - 1}"
        ends.append(hit)
    link(ends[0], ends[1], LineString(pl).length)
for w in ("north Well", "south Well"):
    link(w, nearest(("river.",), nodes[w]))


def dijkstra(src):
    dist = {src: 0.0}
    heap = [(0.0, src)]
    while heap:
        d, n = heapq.heappop(heap)
        if d > dist.get(n, math.inf):
            continue
        for m, w in edges[n]:
            nd = d + w
            if nd < dist.get(m, math.inf):
                dist[m] = nd
                heapq.heappush(heap, (nd, m))
    return dist


from_fountain = dijkstra("Fountain")
targets = ["stonehorn", "ashfang", "miremother", "skittermaw", "razorback", "gloomwing", "top elbow pocket", "bot-outer glade", "north Well", "south Well",
           "top crossroads", "bot crossroads"]
metrics["fountain_routes"] = {t: {"units": round(from_fountain[t]), "seconds": round(from_fountain[t] / SPEED, 1)} for t in targets}
# Fords: the corner river nodes nearest each ford.
for label, f in (("top outer ford", ford_tl), ("bottom outer ford", ford_br)):
    n = nearest(("river.",), f)
    metrics["fountain_routes"][label] = {"units": round(from_fountain[n]), "seconds": round(from_fountain[n] / SPEED, 1)}
metrics["lanes"] = {lane: {"length": round(LineString(pts).length), "walk_seconds": round(LineString(pts).length / SPEED, 1)}
                    for lane, pts in lanes.items()}
metrics["outer_jungle_depth"] = round(P - (SIDE + ENV))
metrics["keep"] = {"depth_from_corner": P - KEEP, "gate_width": GATE,
                   "inhibitor_to_prime_well": {k: round(math.dist(v, prime_well_a)) for k, v in inhib_a.items()}}

# ---------------------------------------------------------------- current main (for comparison)
main_world = json.loads(subprocess.check_output(["git", "-C", str(REPO), "show", "origin/main:Game/Tuning/World.json"]))
L = main_world["layout"]


def w2s(p):  # world -> screen
    return (p["y"], p["x"])


current = {
    "halfExtent": L["halfExtent"],
    "lanes": {l["lane"]: [w2s(p) for p in l["points"]] for l in L["lanes"]},
    "river": [(w2s(p), p["width"]) for p in L["river"]["main"]],
    "walls": [[(w2s(p), p["width"]) for p in w["points"]] for w in L["walls"]],
    "fog": [(w2s(f["center"]), f["radius"]) for f in L["denseFog"]],
    "camps": [(c["species"], w2s(c["center"])) for c in main_world["wildlife"]["camps"]],
    "wells": [w2s(s) for s in main_world["fluxWells"]["sites"]],
    "primeWell": w2s(L["base"]["primeWell"]), "padRadius": L["base"]["padRadius"],
    "fountain": w2s(L["base"]["fountain"]),
}
cur_lane_len = {k: round(LineString(v).length) for k, v in current["lanes"].items()}
metrics["current"] = {"halfExtent": L["halfExtent"], "lane_lengths": cur_lane_len,
                      "outer_jungle_depth": round(L["halfExtent"] - L["terrain"]["boundaryWidth"] - 6500 - 350 - L["terrain"]["laneShoulder"]),
                      "base_pad_radius": L["base"]["padRadius"], "walls": len(L["walls"]), "fog_lobes": len(L["denseFog"])}

# ---------------------------------------------------------------- SVG
def path_d(geom):
    polys = geom.geoms if geom.geom_type in ("MultiPolygon", "GeometryCollection") else [geom]
    d = []
    for poly in polys:
        if poly.geom_type != "Polygon" or poly.is_empty:
            continue
        for ring in [poly.exterior, *poly.interiors]:
            pts = list(ring.coords)
            d.append("M" + " L".join(f"{x:.0f},{-y:.0f}" for x, y in pts) + " Z")
    return " ".join(d)


def pl_d(pts):
    return "M" + " L".join(f"{x:.0f},{-y:.0f}" for x, y in pts)


def svg_plan():
    s = []
    vb = HALF_EXTENT
    s.append(f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="{-vb} {-vb} {2*vb} {2*vb}" role="img" aria-label="Crucible whole-map design proposal">')
    s.append(f'<rect x="{-vb}" y="{-vb}" width="{2*vb}" height="{2*vb}" fill="#2a2f2a"/>')
    s.append(f'<path d="{path_d(blocked)}" fill="#33503a" stroke="#22392a" stroke-width="40"/>')
    s.append(f'<path d="{path_d(walkable_jungle)}" fill="#6f8f4e"/>')
    s.append(f'<path d="{path_d(unary_union([river_band, mid_ford]).difference(river_water))}" fill="#9fae7a"/>')
    s.append(f'<path d="{path_d(unary_union([river_water, arenas.difference(islands)]))}" fill="#3f8f9c"/>')
    s.append(f'<path d="{path_d(gorge_water)}" fill="#2f6f7a" stroke="#d9eef2" stroke-width="30" stroke-dasharray="60 140"/>')
    for sgn in (1, -1):
        half = RIVER_WATER / 2 + RIVER_BANK + 150
        a, b = river_point(sgn * RIVER_END, -half), river_point(sgn * RIVER_END, half)
        s.append(f'<path d="M{a[0]:.0f},{-a[1]:.0f} L{b[0]:.0f},{-b[1]:.0f}" stroke="#5b4f3f" stroke-width="320" stroke-linecap="round"/>')
        s.append(f'<path d="M{a[0]:.0f},{-a[1]:.0f} L{b[0]:.0f},{-b[1]:.0f}" stroke="#e8f4f6" stroke-width="60" stroke-dasharray="90 110" stroke-linecap="round"/>')
    s.append(f'<path d="{path_d(islands)}" fill="#86a463"/>')
    s.append(f'<path d="{path_d(lane_env.intersection(playable).difference(keeps))}" fill="#b49f74"/>')
    s.append(f'<path d="{path_d(lane_road.intersection(playable).difference(keeps))}" fill="#d8c597"/>')
    s.append(f'<path d="{path_d(keeps)}" fill="#a7a59c"/>')
    plaza_r = 900
    for flip in (False, True):
        f = rot if flip else (lambda p: p)
        paths = [LineString([f(fountain_a), f(prime_well_a)])] + [LineString([f(prime_well_a), f(inhib_a[k]), f(gates_a[k])]) for k in inhib_a]
        walk = unary_union([pth.buffer(330) for pth in paths] + [Point(f(prime_well_a)).buffer(plaza_r)])
        s.append(f'<path d="{path_d(walk)}" fill="#c9c5b8"/>')
    for walls, flip in ((keep_walls_a, False), (keep_walls_a, True)):
        for w in walls:
            pts = [rot(p) for p in w] if flip else w
            s.append(f'<path d="{pl_d(pts)}" fill="none" stroke="#3b3a36" stroke-width="{WALL}" stroke-linejoin="round" stroke-linecap="square"/>')
    s.append(f'<path d="{path_d(fog_all)}" fill="#f4f6f8" fill-opacity="0.72" stroke="#ffffff" stroke-width="30"/>')
    # structures
    for flip in (False, True):
        f = rot if flip else (lambda p: p)
        team = "#4f7fd6" if not flip else "#d65a4f"
        pw = f(prime_well_a)
        s.append(f'<rect x="{pw[0]-260}" y="{-pw[1]-260}" width="520" height="520" transform="rotate(45 {pw[0]} {-pw[1]})" fill="{team}" stroke="#111" stroke-width="40"/>')
        fo = f(fountain_a)
        s.append(f'<circle cx="{fo[0]}" cy="{-fo[1]}" r="{fountain_r}" fill="{team}" fill-opacity="0.35" stroke="{team}" stroke-width="40"/>')
        for t in base_towers_a:
            q = f(t)
            s.append(f'<circle cx="{q[0]}" cy="{-q[1]}" r="140" fill="{team}" stroke="#111" stroke-width="30"/>')
        for lane, inh in inhib_a.items():
            q = f(inh)
            s.append(f'<rect x="{q[0]-150}" y="{-q[1]-150}" width="300" height="300" fill="{team}" stroke="#111" stroke-width="30"/>')
            for sp in spire_points(lane, from_b=flip):
                s.append(f'<circle cx="{sp[0]:.0f}" cy="{-sp[1]:.0f}" r="150" fill="{team}" stroke="#111" stroke-width="30"/>')
    for w in (well_n, well_s):
        s.append(f'<path d="M{w[0]},{-w[1]-330} L{w[0]+330},{-w[1]} L{w[0]},{-w[1]+330} L{w[0]-330},{-w[1]} Z" fill="#c9a2ff" stroke="#3a1f66" stroke-width="40"/>')
    for name, (c, r, kind) in clearings_a.items():
        for q in (c, rot(c)):
            if kind == "camp":
                s.append(f'<circle cx="{q[0]}" cy="{-q[1]}" r="170" fill="#f0c04a" stroke="#3b2a05" stroke-width="40"/>')
            elif kind == "newcamp":
                s.append(f'<circle cx="{q[0]}" cy="{-q[1]}" r="250" fill="none" stroke="#fff6d6" stroke-width="60"/>')
                s.append(f'<circle cx="{q[0]}" cy="{-q[1]}" r="170" fill="#f0c04a" stroke="#3b2a05" stroke-width="40"/>')
    # labels (Team A side only, plus region names)
    def label(p, text, size=300, color="#f7f4ea", anchor="middle"):
        s.append(f'<text x="{p[0]:.0f}" y="{-p[1]:.0f}" font-size="{size}" fill="{color}" text-anchor="{anchor}" '
                 f'font-family="Inter, Segoe UI, sans-serif" font-weight="600" paint-order="stroke" stroke="#1c241e" stroke-width="{size/6:.0f}">{text}</text>')
    for name, (c, r, kind) in clearings_a.items():
        if kind == "camp":
            label((c[0], c[1] - r - 260), name.capitalize(), 260)
        elif kind == "newcamp":
            label((c[0], c[1] - r - 260), "New camp", 260, "#fff6d6")
    label((well_n[0] + 600, well_n[1] + 650), "North Flux Well", 260, "#ead9ff")
    label((well_s[0] - 600, well_s[1] - 900), "South Flux Well", 260, "#ead9ff")
    label((-7300, -7900), "Team A keep", 300)
    label((7300, 7700), "Team B keep", 300)
    label((-8050, -400), "outer", 240, "#d9e6c7"); label((-8050, -680), "jungle", 240, "#d9e6c7")
    label((-1500, -8750), "outer jungle", 240, "#d9e6c7")
    label((-3400, -1100), "inner", 240, "#d9e6c7")
    label((-300, -3300), "inner", 240, "#d9e6c7")
    label((-6700, 7600), "outer ford", 240, "#cfeaf0")
    label((-8500, 8700), "falls", 240, "#cfeaf0")
    label((8500, -8900), "falls", 240, "#cfeaf0")
    label((6700, -7450), "outer ford", 240, "#cfeaf0")
    s.append("</svg>")
    return "\n".join(s)


def _mirror_lane(lane):
    return {"Top": "Bottom", "Bottom": "Top", "Mid": "Mid"}[lane]


def svg_current():
    s = []
    vb = HALF_EXTENT  # same frame as the proposal for a fair comparison
    he = current["halfExtent"]
    pp = he - L["terrain"]["boundaryWidth"]
    s.append(f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="{-vb} {-vb} {2*vb} {2*vb}" role="img" aria-label="Current Crucible layout on main">')
    s.append(f'<rect x="{-vb}" y="{-vb}" width="{2*vb}" height="{2*vb}" fill="#2a2f2a"/>')
    s.append(f'<rect x="{-pp}" y="{-pp}" width="{2*pp}" height="{2*pp}" fill="#6f8f4e"/>')
    pads = unary_union([Point(current["primeWell"]).buffer(current["padRadius"]), Point(rot(current["primeWell"])).buffer(current["padRadius"])]).intersection(box(-pp, -pp, pp, pp))
    s.append(f'<path d="{path_d(pads)}" fill="#a7a59c"/>')
    for wall in current["walls"]:
        for pts in (wall, [(rot(p), w) for p, w in wall]):
            shape = unary_union([LineString([a[0], b[0]]).buffer((a[1] + b[1]) / 4) for a, b in zip(pts, pts[1:])])
            s.append(f'<path d="{path_d(shape)}" fill="#33503a" stroke="#22392a" stroke-width="40"/>')
    riv = [p for p, _ in current["river"]]
    river = unary_union([LineString(riv).buffer(500), LineString([rot(p) for p in riv]).buffer(500)]).intersection(box(-pp, -pp, pp, pp))
    s.append(f'<path d="{path_d(river)}" fill="#3f8f9c"/>')
    for lane, pts in current["lanes"].items():
        s.append(f'<path d="{pl_d(pts)}" fill="none" stroke="#d8c597" stroke-width="700" stroke-linejoin="miter"/>')
    fog = unary_union([Point(c).buffer(r) for c, r in current["fog"]] + [Point(rot(c)).buffer(r) for c, r in current["fog"]])
    s.append(f'<path d="{path_d(fog)}" fill="#f4f6f8" fill-opacity="0.72"/>')
    for sp, c in current["camps"]:
        for q in (c, rot(c)):
            s.append(f'<circle cx="{q[0]}" cy="{-q[1]}" r="170" fill="#f0c04a" stroke="#3b2a05" stroke-width="40"/>')
    for w in current["wells"]:
        s.append(f'<path d="M{w[0]},{-w[1]-330} L{w[0]+330},{-w[1]} L{w[0]},{-w[1]+330} L{w[0]-330},{-w[1]} Z" fill="#c9a2ff" stroke="#3a1f66" stroke-width="40"/>')
    for q, team in ((current["primeWell"], "#4f7fd6"), (rot(current["primeWell"]), "#d65a4f")):
        s.append(f'<rect x="{q[0]-260}" y="{-q[1]-260}" width="520" height="520" transform="rotate(45 {q[0]} {-q[1]})" fill="{team}" stroke="#111" stroke-width="40"/>')
    s.append(f'<rect x="{-pp}" y="{-pp}" width="{2*pp}" height="{2*pp}" fill="none" stroke="#11140f" stroke-width="60"/>')
    s.append("</svg>")
    return "\n".join(s)


(OUT / "plan.svg").write_text(svg_plan(), encoding="utf-8")
(OUT / "current.svg").write_text(svg_current(), encoding="utf-8")
(OUT / "metrics.json").write_text(json.dumps(metrics, indent=2), encoding="utf-8")
proposal = {
    "frame": "screen: X right, Y up; World.json x = Y, y = X; Team B = half turn",
    "halfExtent": HALF_EXTENT, "boundaryWidth": BOUNDARY, "laneEnvelopeHalfWidth": ENV, "routeWidths": {"rotation": ROTATION, "approach": APPROACH, "outer": OUTER}, "midFordRadius": 1100,
    "keep": {"polygon": list(keep_a.exterior.coords), "walls": keep_walls_a, "gates": gates_a, "gateWidth": GATE,
             "primeWell": prime_well_a, "fountain": fountain_a, "baseTowers": base_towers_a, "inhibitors": inhib_a},
    "lanes": {k: [list(map(round, p)) for p in v] for k, v in lanes.items()},
    "spires": {"distances": spire_distances, "sides": spire_sides, "offset": SPIRE_OFFSET},
    "river": {"amplitude": RIVER_A, "halfWavelength": RIVER_L, "water": RIVER_WATER, "bank": RIVER_BANK,
              "wellSites": [well_n, well_s], "fords": [ford_tl, ford_br], "arenaRadius": ARENA_R, "islandRadius": ISLAND_R},
    "clearings": {k: {"center": v[0], "radius": v[1], "kind": v[2]} for k, v in clearings_a.items()},
    "corridors": {k: {"width": w, "points": [list(map(round, p)) for p in pl]} for k, (pl, w) in corridors_a.items()},
    "denseFog": [{"bank": n, "lobes": [{"center": c, "radius": r} for c, r in lobes]} for n, lobes in fog_a],
}
(OUT / "proposal.json").write_text(json.dumps(proposal, indent=1), encoding="utf-8")
print(json.dumps(metrics, indent=1))