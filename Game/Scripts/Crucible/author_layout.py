"""One-time migration of the approved whole-map plan (Game/Review/Crucible/2026-10-12-spatial-plan) into World.json
schema 8 (ADR-073 §1). After it runs, World.json is the layout's authority and this script is only its record.

The plan is drawn in a screen frame (X right, Y up); World.json is in world units (x up the screen, y to its right), so
x = screen Y and y = screen X. That swap mirrors handedness: a Spire's side, +1 for the owner's left, flips sign.

The river keeps its canon shape (Battleground Bible §2): each Flux Well stands on an island, the main channel passing
it on one side and a side channel parting and rejoining around the other.
"""
from __future__ import annotations

import contextlib
import io
import math
import re
import runpy
import sys

from crucible_geometry import REPO, SCHEMA_VERSION, load_world, sample_width_curve, write_world

if "--rewrite" not in sys.argv[1:]:
    sys.exit("World.json is the layout's authority now; this migration would overwrite it from the plan. "
             "Pass --rewrite only to redo the migration on purpose.")

PLAN = REPO / "Game" / "Review" / "Crucible" / "2026-10-12-spatial-plan" / "plan.py"
with contextlib.redirect_stdout(io.StringIO()):
    plan = runpy.run_path(str(PLAN))


def w(p):
    """Screen frame to world: x = screen Y, y = screen X."""
    return {"x": round(p[1]), "y": round(p[0])}


def points(pts):
    return [w(p) for p in pts]


def slug(name):
    return "_".join(re.findall(r"[a-z0-9]+", name.lower()))


# ---------------------------------------------------------------- river: the plan's S-curve, split around each Well
WELL_S, SPLIT, SPAN = plan["WELL_S"], 1000.0, 1800.0
WATER, CHANNEL_WATER = plan["RIVER_WATER"], 600.0


def bump(s):
    d = abs(s - WELL_S)
    return 0.5 * (1.0 + math.cos(math.pi * d / SPAN)) if d < SPAN else 0.0


def river_at(s, offset):
    return plan["river_point"](s, offset)


main_s = [0, 1250, 2500] + list(range(2700, 6200, 450)) + [6500, 7750, 9000, 10250, 11500, 12750, 14000, 15250]
main = [dict(w(river_at(s, -SPLIT * bump(s))), width=WATER) for s in main_s]
main[0] = {"x": 0, "y": 0, "width": WATER}
channel_s = [WELL_S - SPAN + i * SPAN / 4 for i in range(9)]
channel = [dict(w(river_at(s, SPLIT * bump(s))), width=CHANNEL_WATER) for s in channel_s]
well_site = w(river_at(WELL_S, 0.0))

# walkableEnd: the arc length along the main channel from the centre to where the plan's falls begin.
samples = sample_width_curve([(m["x"], m["y"]) for m in main], [m["width"] for m in main], 12)
end_point = w(river_at(plan["RIVER_END"], 0.0))
travelled, walkable_end = 0.0, None
for a, b in zip(samples, samples[1:]):
    travelled += math.dist(a[:2], b[:2])
    if math.dist(b[:2], (end_point["x"], end_point["y"])) < 300:
        walkable_end = round(travelled)
        break
assert walkable_end, "The falls must lie on the main channel"

# ---------------------------------------------------------------- the layout
world = load_world()
old = world["layout"]
spire_sides = {lane: [-s for s in sides] for lane, sides in plan["spire_sides"].items()}
lanes = []
for lane in old["lanes"]:
    name = lane["lane"]
    lanes.append({
        "lane": name,
        "points": points(plan["lanes"][name]),
        "width": plan["ROAD"],
        "inhibitorDistance": 0,
        "spireDistances": plan["spire_distances"][name],
        "spireSides": spire_sides[name],
        "fluxbornSpawnDistance": lane["fluxbornSpawnDistance"],
    })
keep = {
    "floor": points(list(plan["keep_a"].exterior.coords)[:-1]),
    "walls": [{"points": points(run), "thickness": plan["WALL"], "height": 480} for run in plan["keep_walls_a"]],
    "gates": [{"lane": lane, "center": w(c), "width": plan["GATE"]} for lane, c in plan["gates_a"].items()],
}
CAMP_IDS = {"top elbow pocket": "corner_camp", "bot-outer glade": "strip_camp"}
clearings = []
for name, (c, r, kind) in plan["clearings_a"].items():
    cid = CAMP_IDS.get(name, slug(name))
    clearings.append({"id": cid, "center": w(c), "radius": r, "kind": "Camp" if kind in ("camp", "newcamp") else "Glade"})
clearings.append({"id": "mid_ford", "center": {"x": 0, "y": 0}, "radius": 1100, "kind": "Glade"})
clearings.append({"id": "well_island", "center": well_site, "radius": 450, "kind": "Glade"})

routes, seen = [], {}
for name, (pl, width) in plan["corridors_a"].items():
    rid = slug(name)
    seen[rid] = seen.get(rid, 0) + 1
    routes.append({"id": rid if seen[rid] == 1 else f"{rid}_{seen[rid]}", "width": width, "points": points(pl)})
fog = [{"center": w(c), "radius": r} for _, lobes in plan["fog_a"] for c, r in lobes]

layout = {
    "provenance": "Provisional",
    "halfExtent": plan["HALF_EXTENT"],
    "surface": old["surface"],
    "terrain": dict(old["terrain"], boundaryWidth=plan["BOUNDARY"], laneShoulder=plan["SHOULDER"]),
    "river": {
        "samplesPerSegment": old["river"]["samplesPerSegment"], "surfaceZ": old["river"]["surfaceZ"],
        "bedZ": old["river"]["bedZ"], "flowSpeed": old["river"]["flowSpeed"],
        "walkableEnd": walkable_end, "main": main, "islands": [{"site": 0, "channel": channel}],
    },
    "lanes": lanes,
    "spireOffset": plan["SPIRE_OFFSET"],
    "base": {"primeWell": w(plan["prime_well_a"]), "baseTowers": points(plan["base_towers_a"]),
             "fountain": w(plan["fountain_a"]), "fountainRadius": old["base"]["fountainRadius"]},
    "keep": keep,
    "clearings": clearings,
    "routes": routes,
    "denseFog": fog,
    "wallHalfHeight": old["wallHalfHeight"],
    "massRules": {"minThickness": 300, "minArea": 250000, "simplifyTolerance": 10, "riverMargin": plan["RIVER_BANK"],
                  "forestMinArea": 1200000, "edgeThickness": 120, "edgeMaxLength": 600, "cornerRadius": 150,
                  "visualTolerance": 10},
    "masses": {"inputSha256": "", "items": []},
}
world["schemaVersion"] = SCHEMA_VERSION
world["layout"] = layout

# ---------------------------------------------------------------- camps: eight a team, each on its clearing
species = {c["species"]: c for c in world["wildlife"]["camps"]}
NEW_CAMPS = {"corner_camp": "gloomwing", "strip_camp": "razorback"}  # D6: provisional reuse until the author chooses
camps = []
for clearing in clearings:
    if clearing["kind"] != "Camp":
        continue
    kind = NEW_CAMPS.get(clearing["id"], clearing["id"])
    template = species[kind]
    camps.append({"id": clearing["id"], "species": kind, "count": template["count"], "center": clearing["center"],
                  "spawnSeconds": 67 if clearing["id"] in NEW_CAMPS else template["spawnSeconds"],
                  "respawnSeconds": template["respawnSeconds"], "leashRadius": template["leashRadius"], "spacing": template["spacing"]})
world["wildlife"]["camps"] = camps
world["fluxWells"]["sites"] = [{"x": well_site["x"], "y": well_site["y"]}, {"x": -well_site["x"], "y": -well_site["y"]}]
write_world(world)
print(f"World.json schema {SCHEMA_VERSION}: {len(lanes)} lanes, {len(clearings)} clearings, {len(routes)} routes, "
      f"{len(fog)} fog circles, {len(camps)} camps, river walkable to {walkable_end}. Now run BuildCrucibleMasses.py.")
