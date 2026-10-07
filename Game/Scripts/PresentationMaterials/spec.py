"""What a valid PresentationMaterials.json is (ADR-063; ADR-064 §1): each material's kind and the values that kind needs.

Pure Python, so the spec is checked without Unreal (tests/test_presentation_materials_spec.py, in CI) and by
BuildPresentationMaterials.py before it changes any asset. Every value is required: nothing falls back to a default.
"""
import numbers
import re

SCHEMA_VERSION = 1
# The version of BuildPresentationMaterials.py's graphs; a spec names the version it was written for. Version 3: the
# toon characters' ink (ADR-068 §3), and a hover outline that looks only for the hover's own stencils. Version 4: the
# combat effects' graphic shapes (ADR-068 §4).
GENERATOR_VERSION = 4
# Presentation materials live where the UI's content is always cooked.
DESTINATION_ROOT = "/Game/Veyra/UI/"
ASSET_NAME = re.compile(r"^[A-Za-z][A-Za-z0-9_]*$")
SIDES = ("enemy", "ally", "neutral")
# The shapes an effect particle may take: a camera-facing sprite (a ball), or a ribbon (a strand).
EFFECT_SHAPES = ("sprite", "ribbon")
# The graphic shapes a combat effect's sprite may draw (ADR-068 §4), and the values each needs beyond the common ones: a
# flare (a hard disc that shrinks), a ring (a band that widens out from start and thins to nothing), a star (a four-point
# spark, its points thinner as its exponent falls, that shrinks) and a streak (a long diamond, for a sprite drawn along
# its velocity, that shrinks).
GRAPHIC_SHAPES = {
    "flare": ("shrink",),
    "ring": ("start", "thickness"),
    "star": ("shrink", "exponent"),
    "streak": ("shrink",),
}
GRAPHIC_SHAPE_RULES = {
    "shrink": (lambda value: _is_number(value) and 0.0 <= value < 1.0, "a number from 0 to below 1"),
    "start": (lambda value: _is_number(value) and 0.0 <= value < 1.0, "a number from 0 to below 1"),
    "thickness": (lambda value: _is_number(value) and 0.0 < value <= 1.0, "a number above 0 and at most 1"),
    "exponent": (lambda value: _is_number(value) and 0.0 < value < 1.0, "a number strictly between 0 and 1"),
}


def _is_number(value):
    return isinstance(value, numbers.Real) and not isinstance(value, bool)


def _positive(value):
    return _is_number(value) and value > 0.0


def _non_negative(value):
    return _is_number(value) and value >= 0.0


def _unit(value):
    """From 0 to 1, both included."""
    return _is_number(value) and 0.0 <= value <= 1.0


def _albedo(value):
    """Above 0, at most 1: a surface reflects some light and never more than it receives."""
    return _is_number(value) and 0.0 < value <= 1.0


def _open_unit(value):
    """Strictly between 0 and 1."""
    return _is_number(value) and 0.0 < value < 1.0


def _name(value):
    return isinstance(value, str) and bool(value.strip())


def _colour(value):
    return isinstance(value, list) and len(value) == 4 and all(_non_negative(channel) for channel in value)


# Each kind's values: (check, what the check asks for).
RULES = {
    "overlayFlash": {
        "colorParameter": (_name, "a parameter name"),
        "strengthParameter": (_name, "a parameter name"),
        "defaultColor": (_colour, "four non-negative numbers (RGBA)"),
        "rimFloor": (_unit, "a number from 0 to 1"),
        "rimExponent": (_positive, "a number above 0"),
        "glowGain": (_positive, "a number above 0"),
    },
    "postProcessOutline": {
        "referenceHeight": (_positive, "a number above 0"),
    },
    "postProcessInk": {
        "stencilParameter": (_name, "a parameter name"),
        "stencil": (lambda value: isinstance(value, int) and not isinstance(value, bool) and 1 <= value <= 255, "a whole number from 1 to 255"),
        "thicknessPixels": (_positive, "a number above 0"),
        "referenceHeight": (_positive, "a number above 0"),
        "darken": (_unit, "a number from 0 to 1"),
        "inkTint": (lambda value: _colour(value) and all(channel <= 1.0 for channel in value), "four numbers from 0 to 1 (RGBA), which the ink's colour is multiplied by"),
        "solidNeighbours": (lambda value: isinstance(value, int) and not isinstance(value, bool) and 1 <= value <= 8, "a whole number from 1 to 8"),
        "depthGap": (_positive, "a number above 0"),
        "visibleSlack": (_non_negative, "a number of at least 0"),
    },
    "particleSmoke": {
        "noiseScale": (_positive, "a number above 0"),
        "ragged": (_positive, "a number above 0"),
        "erosion": (_positive, "a number above 0"),
        "glowFloor": (_non_negative, "a number of at least 0"),
        "glowGain": (_positive, "a number above 0"),
        "albedo": (_albedo, "a number above 0 and at most 1"),
        "clip": (_open_unit, "a number strictly between 0 and 1"),
    },
    "graphicShape": {
        "shape": (lambda value: value in GRAPHIC_SHAPES, "one of " + ", ".join(GRAPHIC_SHAPES)),
        "coreShare": (_open_unit, "a number strictly between 0 and 1"),
        "coreWhiten": (_unit, "a number from 0 to 1"),
        "glowGain": (_positive, "a number above 0"),
        "tailGlow": (_unit, "a number from 0 to 1"),
    },
    "particleEffect": {
        "shape": (lambda value: value in EFFECT_SHAPES, "one of " + ", ".join(EFFECT_SHAPES)),
        "noiseScale": (_positive, "a number above 0"),
        "ragged": (_positive, "a number above 0"),
        "erosion": (_positive, "a number above 0"),
        "clip": (_open_unit, "a number strictly between 0 and 1"),
        "albedo": (_albedo, "a number above 0 and at most 1"),
        "tailShade": (_unit, "a number from 0 to 1"),
        "glowFloor": (_non_negative, "a number of at least 0"),
        "glowGain": (_positive, "a number above 0"),
        "glowFalloff": (_positive, "a number above 0"),
        "coreWhiten": (_unit, "a number from 0 to 1"),
    },
}


def _outline_problems(where, material):
    """The hover outline's nested values: a parameter per side's colour and stencil, three distinct stencils from 1 to
    255, and an enemy outline at least as thick as the others'."""
    problems = []
    for group in ("colorParameters", "stencilParameters"):
        names = material.get(group)
        if not isinstance(names, dict) or any(not _name(names.get(side)) for side in SIDES):
            problems.append(f"{where}: {group} needs a parameter name for each of " + ", ".join(SIDES))
    stencils = material.get("stencils")
    values = [stencils.get(side) for side in SIDES] if isinstance(stencils, dict) else []
    if (len(values) != len(SIDES) or any(not isinstance(value, int) or isinstance(value, bool) or not 1 <= value <= 255 for value in values)
            or len(set(values)) != len(SIDES)):
        problems.append(f"{where}: stencils needs three distinct whole numbers from 1 to 255, one for each of " + ", ".join(SIDES))
    thickness = material.get("thicknessPixels")
    if (not isinstance(thickness, dict) or not _positive(thickness.get("other")) or not _is_number(thickness.get("enemy"))
            or thickness["enemy"] < thickness["other"]):
        problems.append(f"{where}: thicknessPixels needs other above 0 and enemy at least as thick")
    return problems


def validate(spec):
    """Every problem with spec, as "where: what"; empty when the generator can build it."""
    problems = []
    if spec.get("schemaVersion") != SCHEMA_VERSION:
        problems.append(f"schemaVersion: must be {SCHEMA_VERSION}")
    if spec.get("generatorVersion") != GENERATOR_VERSION:
        problems.append(f"generatorVersion: the spec was written for another generator version (this is {GENERATOR_VERSION})")
    destination = spec.get("destination")
    if not isinstance(destination, str) or not destination.startswith(DESTINATION_ROOT):
        problems.append(f"destination: must lie under {DESTINATION_ROOT}, which is always cooked")
    materials = spec.get("materials")
    if not isinstance(materials, list) or not materials:
        return problems + ["materials: needs at least one material"]
    seen = set()
    for index, material in enumerate(materials):
        name = material.get("name") if isinstance(material, dict) else None
        where = f"materials[{index}]" + (f" ({name})" if isinstance(name, str) else "")
        if not isinstance(material, dict):
            problems.append(f"{where}: must be an object")
            continue
        if not isinstance(name, str) or not ASSET_NAME.match(name):
            problems.append(f"{where}: name must be an asset name")
        elif name in seen:
            problems.append(f"{where}: name is used twice")
        seen.add(name)
        if not _name(material.get("purpose")):
            problems.append(f"{where}: purpose must say what it is for")
        kind = material.get("kind")
        if kind not in RULES:
            problems.append(f"{where}: unknown kind {kind!r}")
            continue
        for key, (check, wanted) in RULES[kind].items():
            if key not in material:
                problems.append(f"{where}: {key} is required")
            elif not check(material[key]):
                problems.append(f"{where}: {key} must be {wanted}")
        if kind == "postProcessOutline":
            problems += _outline_problems(where, material)
        if kind == "graphicShape" and material.get("shape") in GRAPHIC_SHAPES:
            for key in GRAPHIC_SHAPES[material["shape"]]:
                check, wanted = GRAPHIC_SHAPE_RULES[key]
                if key not in material:
                    problems.append(f"{where}: {key} is required")
                elif not check(material[key]):
                    problems.append(f"{where}: {key} must be {wanted}")
    # The ink marks characters by a stencil of its own: one a hover outline's stencils share would outline every character
    # as hovered, or hide the hovered one's ink (ADR-068 §3).
    hover = {value for material in materials if isinstance(material, dict) and material.get("kind") == "postProcessOutline"
             for value in (material.get("stencils") or {}).values()}
    for material in materials:
        if isinstance(material, dict) and material.get("kind") == "postProcessInk" and material.get("stencil") in hover:
            problems.append(f"{material.get('name')}: stencil must differ from every hover outline's stencils")
    return problems
