"""The masses' look (ADR-073 §5), derived from World.json's generated masses and the style profile's massArt.

For each of Team A's masses it writes:
- its strata: the gameplay outline itself, then steps inset from it and broken inward only, each up to its share of
  the kind's height, so the footprint at walking height is the gameplay edge and nothing reaches past it;
- on a forest, the canopy standing on its top, and shrubs round its rim;
- its ink lines: one round its foot just outside the edge, one round the lip of each top step.

Team B's are the half turn of Team A's, so only Team A's are written. Heights are above the mass's base, which the world
build takes from the floor under it. Presentation only: VeyraWorldTools extrudes and places what this writes; nothing
here changes what blocks. Needs shapely (requirements.txt beside this file) for everything but the hashes.
"""
from __future__ import annotations

import hashlib
import json
import random
from pathlib import Path

from crucible_geometry import REPO

STYLE_JSON = REPO / "Game" / "Plugins" / "VeyraWorldTools" / "Config" / "CrucibleStyle.json"
ART_JSON = REPO / "Game" / "Plugins" / "VeyraWorldTools" / "Config" / "CrucibleMassArt.json"


def load_style(path: Path = STYLE_JSON) -> dict:
    return json.loads(path.read_text(encoding="utf-8"))


def profile_sha256(style: dict) -> str:
    canonical = json.dumps(style["massArt"], sort_keys=True, separators=(",", ":"))
    return hashlib.sha256(canonical.encode("utf-8")).hexdigest()


def _rounded(coords, digits=1):
    return [[round(x, digits), round(y, digits)] for x, y in coords]


def _parts(geom):
    if geom.is_empty:
        return []
    return [g for g in (geom.geoms if hasattr(geom, "geoms") else [geom]) if g.geom_type == "Polygon" and not g.is_empty]


def _jitter_inward(poly, rng, reach, share, tolerance):
    """Moves each corner of a step part of the way toward a ring inside it, so its face reads as broken rock. Only ever
    inward: the step stays within its own outline, so never past the gameplay edge."""
    from shapely.geometry import Point, Polygon

    inner = poly.buffer(-reach)
    if inner.is_empty or inner.geom_type != "Polygon":
        return poly
    simple = poly.simplify(tolerance, preserve_topology=True)
    moved = []
    for x, y in list(simple.exterior.coords)[:-1]:
        q = inner.exterior.interpolate(inner.exterior.project(Point(x, y)))
        f = rng.uniform(0.0, share)
        moved.append((x + (q.x - x) * f, y + (q.y - y) * f))
    candidate = Polygon(moved).buffer(0)
    return candidate if candidate.geom_type == "Polygon" and candidate.within(poly.buffer(1)) else poly


def derive_art(world: dict, style: dict) -> dict:
    from shapely.geometry import Point, Polygon

    art = style["massArt"]
    seed = art["seed"]
    team_a = [m for m in world["layout"]["masses"]["items"] if m["id"].startswith("a")]
    masses = []
    for index, mass in enumerate(team_a):
        profile = art["profiles"][mass["kind"]]
        outline = Polygon([(p["x"], p["y"]) for p in mass["points"]])
        levels = [round(step["top"] * profile["height"], 1) for step in profile["strata"]]
        strata = []
        for level, step in enumerate(profile["strata"]):
            if step["inset"] == 0:
                # The first step is the gameplay outline itself, corner for corner.
                strata.append({"level": level, "points": [[p["x"], p["y"]] for p in mass["points"]]})
                continue
            shape = outline.buffer(-step["inset"], join_style="round", quad_segs=4)
            for part in _parts(shape):
                if part.area < art["minStepArea"]:
                    continue
                part = Polygon(part.exterior)  # a hole would lie inside rock anyway; the step's top covers it
                part = _jitter_inward(part, random.Random(seed * 31 + index * 7 + step["inset"]), art["jitterReach"], art["jitterShare"], art["outlineTolerance"])
                coords = list(part.simplify(art["outlineTolerance"], preserve_topology=True).exterior.coords)[:-1]
                strata.append({"level": level, "points": _rounded(coords)})

        canopy = []
        if mass["kind"] == "Forest":
            spec = art["canopy"]
            rng = random.Random(seed + index)
            inner = outline.buffer(-spec["inset"])
            minx, miny, maxx, maxy = outline.bounds
            y = miny + rng.uniform(0, spec["spacing"])
            while y < maxy:
                x = minx + rng.uniform(0, spec["spacing"])
                while x < maxx:
                    px, py = x + rng.uniform(-spec["jitter"], spec["jitter"]), y + rng.uniform(-spec["jitter"], spec["jitter"])
                    if inner.contains(Point(px, py)):
                        canopy.append({"at": [round(px, 1), round(py, 1)], "kind": "Tree" if rng.random() < spec["treeShare"] else "Shrub",
                                       "scale": round(rng.uniform(*spec["scale"]), 2), "yaw": round(rng.uniform(0, 360), 1)})
                    x += spec["spacing"]
                y += spec["spacing"]
            for part in _parts(outline.buffer(-spec["rimInset"])):
                rim = part.exterior
                along = 0.0
                while along < rim.length:
                    p = rim.interpolate(along)
                    canopy.append({"at": [round(p.x, 1), round(p.y, 1)], "kind": "Shrub",
                                   "scale": round(rng.uniform(*spec["rimScale"]), 2), "yaw": round(rng.uniform(0, 360), 1)})
                    along += spec["rimSpacing"]

        inks = []
        spec = art["inks"]
        foot = outline.buffer(spec["footOffset"], join_style="mitre").exterior.simplify(spec["tolerance"])
        inks.append({"at": "Foot", "size": spec["footSize"], "points": _rounded(list(foot.coords)[:-1])})
        top = len(levels) - 1
        for stratum in strata:
            if stratum["level"] == top:
                inks.append({"at": "Lip", "size": spec["lipSize"], "points": stratum["points"]})

        masses.append({"id": mass["id"], "kind": mass["kind"], "levels": levels, "strata": strata, "canopy": canopy, "inks": inks})
    return {
        "description": "GENERATED by Game/Scripts/Crucible/BuildCrucibleMassArt.py from World.json's masses and CrucibleStyle.json's massArt; never edit by hand (ADR-073 §5). Team A's masses only: Team B's are their half turn.",
        "massesInputSha256": world["layout"]["masses"]["inputSha256"],
        "profileSha256": profile_sha256(style),
        "sink": art["sink"],
        "masses": masses,
    }
