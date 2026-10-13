"""The Crucible's layout geometry (ADR-073): what the runtime and the generator must agree on, in one place.

- Curves are sampled exactly as VeyraWidthCurve samples them (uniform Catmull-Rom, widths linear per span), and the
  river is assembled exactly as FVeyraRiverShape assembles it.
- The open spaces are read from World.json: lane envelopes, the walkable river, keeps, clearings and routes, each
  team's the other's half turn.
- Masses are what the playable square leaves: pieces thinner than minThickness or smaller than minArea are opened,
  convex corners rounded to cornerRadius (necks kept), outlines simplified, Team A's kept by their centroids and Team B's
  written as their exact half turns.
- inputSha256 hashes the canonical JSON of every input, so a stale `masses` is caught without recomputing it.

Everything but the hash needs shapely (pinned in requirements.txt beside this file).
"""
from __future__ import annotations

import hashlib
import json
import math
from pathlib import Path

SCHEMA_VERSION = 8
# Segments to each quarter circle of every buffer: arcs within half a unit of true for radii to 1,500, so a mass edge stands
# where the validator measures it (ADR-073 §2). Resolution, not tuning.
ARC_SEGMENTS = 64
REPO = Path(__file__).resolve().parents[3]
WORLD_JSON = REPO / "Game" / "Tuning" / "World.json"


# ---------------------------------------------------------------- reading and writing World.json


def load_world(path: Path = WORLD_JSON) -> dict:
    return json.loads(path.read_text(encoding="utf-8"))


def _is_scalar(value) -> bool:
    return not isinstance(value, (dict, list))


def _inline(value) -> str:
    return json.dumps(value, ensure_ascii=False, separators=(", ", ": "))


def format_tuning(value, indent: int = 0) -> str:
    """World.json's house style: two-space indents, small all-scalar objects and scalar arrays on one line."""
    pad = "  " * indent
    inner = "  " * (indent + 1)
    if isinstance(value, dict):
        if not value:
            return "{}"
        if all(_is_scalar(v) for v in value.values()) and len(value) <= 6:
            return "{ " + ", ".join(f"{json.dumps(k)}: {_inline(v)}" for k, v in value.items()) + " }"
        items = [f"{inner}{json.dumps(k)}: {format_tuning(v, indent + 1)}" for k, v in value.items()]
        return "{\n" + ",\n".join(items) + "\n" + pad + "}"
    if isinstance(value, list):
        if not value:
            return "[]"
        if all(_is_scalar(v) for v in value):
            return "[" + ", ".join(_inline(v) for v in value) + "]"
        items = [f"{inner}{format_tuning(v, indent + 1)}" for v in value]
        return "[\n" + ",\n".join(items) + "\n" + pad + "]"
    return _inline(value)


def write_world(world: dict, path: Path = WORLD_JSON) -> None:
    path.write_text(format_tuning(world) + "\n", encoding="utf-8", newline="\n")


# ---------------------------------------------------------------- curves, exactly as the runtime samples them


def _cubic(p0, t0, p1, t1, a):
    a2, a3 = a * a, a * a * a
    h00, h10, h01, h11 = 2 * a3 - 3 * a2 + 1, a3 - 2 * a2 + a, -2 * a3 + 3 * a2, a3 - a2
    return (h00 * p0[0] + h10 * t0[0] + h01 * p1[0] + h11 * t1[0], h00 * p0[1] + h10 * t0[1] + h01 * p1[1] + h11 * t1[1])


def sample_width_curve(points, widths, samples_per_segment):
    """VeyraWidthCurve::Sample: [(x, y, width)]."""
    count = len(points)
    if count < 2 or len(widths) != count or samples_per_segment < 1:
        return []

    def at(i):
        return points[max(0, min(count - 1, i))]

    samples = []
    for i in range(count - 1):
        p0, p1 = at(i), at(i + 1)
        t0 = ((at(i + 1)[0] - at(i - 1)[0]) / 2.0, (at(i + 1)[1] - at(i - 1)[1]) / 2.0)
        t1 = ((at(i + 2)[0] - at(i)[0]) / 2.0, (at(i + 2)[1] - at(i)[1]) / 2.0)
        for step in range(samples_per_segment):
            t = step / samples_per_segment
            x, y = _cubic(p0, t0, p1, t1, t)
            samples.append((x, y, widths[i] + (widths[i + 1] - widths[i]) * t))
    samples.append((points[-1][0], points[-1][1], widths[-1]))
    return samples


def rot(p):
    return (-p[0], -p[1])


def river_channels(river):
    """FVeyraRiverShape's channels: (name, samples). The main runs Team B's half in from the far end, then Team A's."""
    main = [rot((c["x"], c["y"])) + (c["width"],) for c in reversed(river["main"][1:])]
    main += [(c["x"], c["y"], c["width"]) for c in river["main"]]
    sps = river["samplesPerSegment"]
    channels = [("Main", sample_width_curve([(m[0], m[1]) for m in main], [m[2] for m in main], sps))]
    for island in river["islands"]:
        pts = [(c["x"], c["y"]) for c in island["channel"]]
        widths = [c["width"] for c in island["channel"]]
        channels.append(("Island", sample_width_curve(pts, widths, sps)))
        channels.append(("Island", sample_width_curve([rot(p) for p in pts], widths, sps)))
    return channels


def walkable_main(samples, river):
    """The main channel's samples within walkableEnd of the centre along the curve, both ways (ADR-073 §1)."""
    centre = (len(river["main"]) - 1) * river["samplesPerSegment"]
    limit = river["walkableEnd"]
    keep = {centre}
    for direction in (1, -1):
        travelled, i = 0.0, centre
        while 0 <= i + direction < len(samples):
            a, b = samples[i], samples[i + direction]
            travelled += math.dist(a[:2], b[:2])
            if travelled > limit:
                break
            i += direction
            keep.add(i)
    return [samples[i] for i in sorted(keep)]


# ---------------------------------------------------------------- inputs and their hash

INPUT_LAYOUT_KEYS = ("halfExtent", "terrain", "river", "lanes", "keep", "clearings", "routes", "massRules")


def inputs_of(world: dict) -> dict:
    layout = world["layout"]
    inputs = {key: layout[key] for key in INPUT_LAYOUT_KEYS}
    # Only the terrain values the masses read: the floor's rim and the lanes' shoulders.
    inputs["terrain"] = {"boundaryWidth": layout["terrain"]["boundaryWidth"], "laneShoulder": layout["terrain"]["laneShoulder"]}
    inputs["lanes"] = [{"points": lane["points"], "width": lane["width"]} for lane in layout["lanes"]]
    return inputs


def input_sha256(world: dict) -> str:
    canonical = json.dumps(inputs_of(world), sort_keys=True, separators=(",", ":"))
    return hashlib.sha256(canonical.encode("utf-8")).hexdigest()


# ---------------------------------------------------------------- open spaces and masses (shapely)


def _shapely():
    import shapely  # noqa: F401 - imported here so the hash works without it
    from shapely.geometry import LineString, Point, Polygon, box
    from shapely.ops import unary_union
    from shapely import affinity
    return LineString, Point, Polygon, box, unary_union, affinity


def _band(samples, extra, LineString, unary_union):
    pieces = []
    for a, b in zip(samples, samples[1:]):
        r = max(a[2], b[2]) / 2.0 + extra
        pieces.append(LineString([a[:2], b[:2]]).buffer(r, quad_segs=ARC_SEGMENTS))
    return unary_union(pieces)


def open_spaces(world: dict):
    LineString, Point, Polygon, box, unary_union, affinity = _shapely()
    layout = world["layout"]
    rules = layout["massRules"]
    turned = lambda g: affinity.rotate(g, 180, origin=(0, 0))
    shoulder = layout["terrain"]["laneShoulder"]
    lanes = [LineString([(p["x"], p["y"]) for p in lane["points"]]).buffer(lane["width"] / 2.0 + shoulder, quad_segs=ARC_SEGMENTS)
             for lane in layout["lanes"]]
    channels = river_channels(layout["river"])
    river = []
    for name, samples in channels:
        if name == "Main":
            samples = walkable_main(samples, layout["river"])
        river.append(_band(samples, rules["riverMargin"], LineString, unary_union))
    keep = layout["keep"]
    keep_floor = Polygon([(p["x"], p["y"]) for p in keep["floor"]])
    clearings = [Point(c["center"]["x"], c["center"]["y"]).buffer(c["radius"], quad_segs=ARC_SEGMENTS) for c in layout["clearings"]]
    routes = [LineString([(p["x"], p["y"]) for p in r["points"]]).buffer(r["width"] / 2.0, quad_segs=ARC_SEGMENTS) for r in layout["routes"]]
    team_a = unary_union([keep_floor] + clearings + routes)
    return unary_union(lanes + river + [team_a, turned(team_a)])


def is_acute(a, v, b):
    """Whether the counter-clockwise outline turns left at v by more than a right angle: a convex corner sharper than one."""
    d1 = (v[0] - a[0], v[1] - a[1])
    d2 = (b[0] - v[0], b[1] - v[1])
    return d1[0] * d2[1] - d1[1] * d2[0] > 0 and d1[0] * d2[0] + d1[1] * d2[1] < 0


def _parts(geom):
    if geom.is_empty:
        return []
    return list(geom.geoms) if geom.geom_type in ("MultiPolygon", "GeometryCollection") else [geom]


def round_corners(solid, radius):
    """Rounds every convex corner of a mass to radius and cuts back every spike narrower than twice it (ADR-073 §3), so
    no corner is sharper than a right angle: an edge box lies flush along its span, and at a sharper corner it would stand
    out of the mass. A neck narrower than twice radius that holds two parts together is kept, so no new way opens through
    a mass. It only ever removes mass, so open ground never shrinks."""
    _, _, _, _, unary_union, _ = _shapely()
    opened = solid.buffer(-radius, quad_segs=16).buffer(radius, quad_segs=16)
    kept = _parts(opened)
    necks = [piece for piece in _parts(solid.difference(opened))
             if piece.area > 1.0 and sum(1 for part in kept if piece.buffer(1.0).intersects(part)) >= 2]
    return _parts(unary_union(kept + necks))


def derive_masses(world: dict) -> list:
    LineString, Point, Polygon, box, unary_union, affinity = _shapely()
    layout = world["layout"]
    rules = layout["massRules"]
    half = layout["halfExtent"] - layout["terrain"]["boundaryWidth"]
    playable = box(-half, -half, half, half)
    blocked = playable.difference(open_spaces(world))
    edge = playable.exterior.buffer(5)
    pieces = list(blocked.geoms) if blocked.geom_type == "MultiPolygon" else [blocked]
    team_a = []
    for piece in pieces:
        if piece.is_empty or piece.area < rules["minArea"] or piece.buffer(-rules["minThickness"] / 2.0).is_empty:
            continue
        solid = Polygon(piece.exterior)  # open ground wholly inside a mass is unreachable: the mass keeps it
        for part in round_corners(solid, rules["cornerRadius"]):
            if part.area < rules["minArea"]:
                continue
            simple = Polygon(part.exterior).simplify(rules["simplifyTolerance"], preserve_topology=True)
            centroid = simple.centroid
            if centroid.x + centroid.y >= 0:
                continue  # Team B's side of the river line: written as Team A's half turn below
            kind = "Cliff" if simple.intersects(edge) else ("Forest" if simple.area >= rules["forestMinArea"] else "Rock")
            coords = [(round(x), round(y)) for x, y in list(simple.exterior.coords)[:-1]]
            area = sum(a[0] * b[1] - b[0] * a[1] for a, b in zip(coords, coords[1:] + coords[:1]))
            if area < 0:
                coords.reverse()  # counter-clockwise in world x/y
            sharp = [coords[i] for i in range(len(coords)) if is_acute(coords[i - 1], coords[i], coords[(i + 1) % len(coords)])]
            if sharp:
                raise ValueError(f"A mass keeps corners sharper than a right angle at {sharp}: widen cornerRadius or the open spaces there.")
            team_a.append({"kind": kind, "points": coords, "key": (round(centroid.x), round(centroid.y))})
    team_a.sort(key=lambda m: m["key"])
    masses = []
    for i, m in enumerate(team_a):
        masses.append({"id": f"a{i:02d}", "kind": m["kind"], "points": [{"x": x, "y": y} for x, y in m["points"]]})
    for i, m in enumerate(team_a):
        masses.append({"id": f"b{i:02d}", "kind": m["kind"], "points": [{"x": -x, "y": -y} for x, y in m["points"]]})
    return masses
