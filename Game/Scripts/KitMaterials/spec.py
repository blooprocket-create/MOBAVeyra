"""What valid material data in the art kits is (ADR-040, ADR-064 §4): the values each kit's importer builds its generated
materials from. Pure Python, so CI checks the shipped kits without Unreal (tests/test_kit_materials_spec.py), and each
importer checks its own kit with it before it changes any asset. Every value is required: nothing falls back to a default.

A glow's strength is in multiples of what the scene's exposure maps to white. Each importer scales it by the inverse of
the exposure (veyra_material_graph.unexposed), so a glow reads the same under the Crucible's physical sun as anywhere.
"""
import numbers


def _number(value):
    return isinstance(value, numbers.Real) and not isinstance(value, bool)


def _positive(value):
    return _number(value) and value > 0.0


def _unit(value):
    """From 0 to 1, both included."""
    return _number(value) and 0.0 <= value <= 1.0


def _open_unit(value):
    """Strictly between 0 and 1."""
    return _number(value) and 0.0 < value < 1.0


def _colour(value, channels):
    return isinstance(value, list) and len(value) == channels and all(_number(channel) and channel >= 0.0 for channel in value)


def surface_problems(kit):
    """Every problem with a structure or Fluxborn kit's materials (StructureKit.json, FluxbornKit.json): each a named
    surface with its RGBA colour, metallic and roughness, and its emission, the Flux glow, of at least 0 (0 for a surface
    that does not glow)."""
    materials = kit.get("materials")
    if not isinstance(materials, list) or not materials:
        return ["materials: needs at least one material"]
    problems = []
    names = set()
    for index, material in enumerate(materials):
        name = material.get("name") if isinstance(material, dict) else None
        where = f"materials[{index}]" + (f" ({name})" if isinstance(name, str) else "")
        if not isinstance(name, str) or not name.startswith("M_"):
            problems.append(f"{where}: name must be a material asset name (M_...)")
        elif name in names:
            problems.append(f"{where}: name is used twice")
        names.add(name)
        if not isinstance(material, dict):
            continue
        if not _colour(material.get("color"), 4):
            problems.append(f"{where}: color must be four non-negative numbers (RGBA)")
        for key in ("metallic", "roughness"):
            if not _unit(material.get(key)):
                problems.append(f"{where}: {key} must be a number from 0 to 1")
        if not _number(material.get("emission")) or material["emission"] < 0.0:
            problems.append(f"{where}: emission must be a number of at least 0")
    return problems


def glyph_problems(look):
    """Every problem with the environment kit's Flux glyphs (CrucibleKit.json look): an RGB colour, a strength above 0,
    and a slow breath whose depth keeps the glow from ever going below none."""
    problems = []
    if not _colour(look.get("glyphColor"), 3):
        problems.append("look.glyphColor: must be three non-negative numbers (RGB)")
    if not _positive(look.get("glyphStrength")):
        problems.append("look.glyphStrength: must be a number above 0")
    if not _number(look.get("glyphPulseRate")) or look["glyphPulseRate"] < 0.0:
        problems.append("look.glyphPulseRate: must be a number of at least 0")
    if not _unit(look.get("glyphPulseDepth")):
        problems.append("look.glyphPulseDepth: must be a number from 0 to 1")
    return problems


def _vector(value, length=3):
    return isinstance(value, list) and len(value) == length and all(_number(channel) for channel in value)


def _tint(value):
    """Three channels from 0 to 1: a colour a light or a shadow multiplies a body's own by."""
    return _vector(value) and all(0.0 <= channel <= 1.0 for channel in value)


def body_problems(kit):
    """Every problem with the Vanguard bodies' material (VanguardKit.json bodyMaterial): the strength of what the
    vertex alpha marks as glowing, above 0, and its toon light (ADR-068 §2)."""
    material = kit.get("bodyMaterial")
    if not isinstance(material, dict):
        return ["bodyMaterial: needs glowGain and toon"]
    problems = []
    if not _positive(material.get("glowGain")):
        problems.append("bodyMaterial.glowGain: must be a number above 0")
    toon = material.get("toon")
    if not isinstance(toon, dict):
        return problems + ["bodyMaterial.toon: needs the toon light's values"]
    checks = {
        "lightCollection": (lambda value: isinstance(value, str) and value.startswith("MPC_"), "an asset name starting MPC_"),
        "toSun": (lambda value: _vector(value) and sum(channel * channel for channel in value) > 0.0, "a direction of three numbers, not all 0"),
        "sunColor": (_tint, "three numbers from 0 to 1"),
        "litTint": (_tint, "three numbers from 0 to 1"),
        "shadowTint": (_tint, "three numbers from 0 to 1"),
        "bandThreshold": (lambda value: _number(value) and -1.0 < value < 1.0, "a number strictly between -1 and 1"),
        "bandSoftness": (_positive, "a number above 0"),
        "rimExponent": (_positive, "a number above 0"),
        "rimStart": (_unit, "a number from 0 to 1"),
        "rimEnd": (_unit, "a number from 0 to 1"),
        "rimStrength": (lambda value: _number(value) and value >= 0.0, "a number of at least 0"),
        "brightness": (_positive, "a number above 0"),
    }
    for key, (check, wanted) in checks.items():
        if not check(toon.get(key)):
            problems.append(f"bodyMaterial.toon.{key}: must be {wanted}")
    if _unit(toon.get("rimStart")) and _unit(toon.get("rimEnd")) and toon["rimEnd"] <= toon["rimStart"]:
        problems.append("bodyMaterial.toon.rimEnd: must lie past rimStart")
    return problems + _veil_problems(material.get("veil"))


def _veil_problems(veil):
    """The hidden body's veil (ADR-068 §6): the parameters the presentation drives, how much of the body shows, its
    shimmer and its rim."""
    if not isinstance(veil, dict):
        return ["bodyMaterial.veil: needs the hidden body's values"]
    name = (lambda value: isinstance(value, str) and bool(value.strip()), "a parameter name")
    checks = {
        "parameter": name,
        "tintParameter": name,
        "opacity": (lambda value: _number(value) and 0.0 < value < 1.0, "a number strictly between 0 and 1"),
        "shimmerScale": (_positive, "a number above 0"),
        "shimmerSpeed": (lambda value: _number(value) and value >= 0.0, "a number of at least 0"),
        "shimmerDepth": (_unit, "a number from 0 to 1"),
        "rimExponent": (_positive, "a number above 0"),
        "rimStart": (_open_unit, "a number strictly between 0 and 1"),
        "rimStrength": (lambda value: _number(value) and value >= 0.0, "a number of at least 0"),
    }
    problems = [f"bodyMaterial.veil.{key}: must be {wanted}" for key, (check, wanted) in checks.items() if not check(veil.get(key))]
    if veil.get("parameter") == veil.get("tintParameter"):
        problems.append("bodyMaterial.veil.tintParameter: must differ from parameter")
    return problems


def model_problems(kit, models):
    """Every problem with the kit's production models (ADR-069 §3): each entry naming a model names a script in models
    (the scripts there, by name), a triangle budget above 0 and a voxel size above 0. A model is flat-coloured: it
    names no texture settings."""
    problems = []
    for spec in list(kit.get("vanguards", [])) + list(kit.get("companions", [])):
        model = spec.get("model")
        if model is None:
            continue
        where = "%s.model" % spec.get("id")
        if not isinstance(model, dict):
            problems.append(where + ": must be an object")
            continue
        if model.get("script") not in models:
            problems.append(where + ".script: must name a script in VanguardBodies/models")
        if not (isinstance(model.get("triangleBudget"), int) and model["triangleBudget"] > 0):
            problems.append(where + ".triangleBudget: must be a whole number above 0")
        if not _positive(model.get("voxelCm")):
            problems.append(where + ".voxelCm: must be a number above 0")
        for key in ("textureSize", "denseTexels"):
            if key in model:
                problems.append(where + "." + key + ": models are flat-coloured (ADR-069 §2); remove it")
    return problems


def spring_problems(kit):
    """Every problem with the kit's loose parts (ADR-069 §7): each entry's springs map a part to its spring back to its
    clip (stiffness, above 0, per second squared), the speed it loses through the air (drag) and relative to its clip
    (damping), each 0 or more per second, and how far it may turn from its clip (maxAngle, degrees above 0, at most
    180)."""
    problems = []
    for spec in list(kit.get("vanguards", [])) + list(kit.get("companions", [])):
        springs = spec.get("springs")
        if springs is None:
            continue
        where = "%s.springs" % spec.get("id")
        if not isinstance(springs, dict) or not springs:
            problems.append(where + ": must map at least one part to its spring")
            continue
        for part, values in springs.items():
            if not isinstance(values, dict):
                problems.append("%s.%s: must be an object" % (where, part))
                continue
            if not _positive(values.get("stiffness")):
                problems.append("%s.%s.stiffness: must be a number above 0" % (where, part))
            for key in ("drag", "damping"):
                if not (_number(values.get(key)) and values[key] >= 0.0):
                    problems.append("%s.%s.%s: must be a number of at least 0" % (where, part, key))
            if not (_positive(values.get("maxAngle")) and values["maxAngle"] <= 180.0):
                problems.append("%s.%s.maxAngle: must be degrees above 0, at most 180" % (where, part))
    return problems
