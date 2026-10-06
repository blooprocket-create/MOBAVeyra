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


def body_problems(kit):
    """Every problem with the Vanguard bodies' material (VanguardKit.json bodyMaterial): the strength of what the
    vertex alpha marks as glowing, above 0, and the surface's roughness."""
    material = kit.get("bodyMaterial")
    if not isinstance(material, dict):
        return ["bodyMaterial: needs glowGain and roughness"]
    problems = []
    if not _positive(material.get("glowGain")):
        problems.append("bodyMaterial.glowGain: must be a number above 0")
    if not _unit(material.get("roughness")):
        problems.append("bodyMaterial.roughness: must be a number from 0 to 1")
    return problems
