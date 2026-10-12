"""Shared helpers for Veyra's generated materials (ADR-040, ADR-063, ADR-064): a node layout over MaterialEditingLibrary,
shared-sampler texture samples, pin linking, the exposure-independent glow every generated glow goes through, the
art kits' surface material, and the checks a generator makes before it rewrites a material. Imported by the editor-only
art importers and material builders in this folder."""
import stat
from pathlib import Path

import unreal

EDIT = unreal.MaterialEditingLibrary


def unexposed(material, glow, x, y, output=""):
    """Glow x the inverse of the scene's exposure: an emissive that reads the same under any light.

    The Crucible is lit physically (CrucibleStyle.json: a sun of tens of thousands of lux under a manual exposure), and
    its exposure scales every pixel down thousands of times, so a plain emissive of a few units reads as black. Scaled by
    the inverse, a glow of 1 is what the exposure maps to white under any light, so each generated glow's strength, kept
    as data in its spec or kit, is in multiples of that (ADR-063, 2026-10-06 amendment)."""
    exposure = EDIT.create_material_expression(material, unreal.MaterialExpressionEyeAdaptationInverse, x - 200, y + 100)
    result = EDIT.create_material_expression(material, unreal.MaterialExpressionMultiply, x, y)
    assert EDIT.connect_material_expressions(glow, output, result, "A"), "glow"
    assert EDIT.connect_material_expressions(exposure, "", result, "B"), "exposure"
    return result


class Graph:
    """A small helper over MaterialEditingLibrary that lays nodes out in columns."""

    def __init__(self, material):
        self.material = material
        self.rows = {}

    def place(self, column):
        """The next free position in column."""
        row = self.rows.get(column, 0)
        self.rows[column] = row + 1
        return -2200 + column * 320, -1200 + row * 140

    def node(self, cls, column, **props):
        expression = EDIT.create_material_expression(self.material, cls, *self.place(column))
        for key, value in props.items():
            expression.set_editor_property(key, value)
        return expression

    def scalar(self, column, name, value, group):
        return self.node(unreal.MaterialExpressionScalarParameter, column, parameter_name=name, default_value=value, group=group)

    def link(self, source, output, target, input_name):
        assert EDIT.connect_material_expressions(source, output, target, input_name), f"{output} -> {input_name}"

    def op(self, cls, column, a, b, a_out="", b_out=""):
        expression = self.node(cls, column)
        self.link(a, a_out, expression, "A")
        self.link(b, b_out, expression, "B")
        return expression

    def unexposed(self, column, glow, output=""):
        """unexposed(), laid out in column."""
        return unexposed(self.material, glow, *self.place(column), output)


def sample(graph, column, texture, uv, sampler_type):
    expression = graph.node(unreal.MaterialExpressionTextureSample, column, texture=texture, sampler_type=sampler_type,
                            sampler_source=unreal.SamplerSourceMode.SSM_WRAP_WORLD_GROUP_SETTINGS)
    graph.link(uv, "", expression, "UVs")
    return expression


def world_aligned(g, column, function, texture_asset, size, output):
    """Shared world projection for static terrain and kit surfaces; size is cm or a scalar expression.

    WorldAlignedNormal's default outputs are tangent-space normals (WorldSpace defaults false).
    Keep that convention when blending them with ordinary tangent-space layer normals.
    """
    def pin(names, prefix):
        match = next((name for name in names if str(name).startswith(prefix)), None)
        assert match, f"no pin {prefix} among {names}"
        return str(match)

    tex = g.node(unreal.MaterialExpressionTextureObject, column - 1, texture=texture_asset)
    call = g.node(unreal.MaterialExpressionMaterialFunctionCall, column)
    call.set_editor_property("material_function", unreal.load_asset(f"/Engine/Functions/Engine_MaterialFunctions01/Texturing/{function}"))
    inputs = EDIT.get_material_expression_input_names(call)
    g.link(tex, "", call, pin(inputs, "TextureObject"))
    tile = (g.node(unreal.MaterialExpressionConstant3Vector, column - 1, constant=unreal.LinearColor(size, size, size, 0.0))
            if isinstance(size, (int, float)) else size)
    g.link(tile, "", call, pin(inputs, "TextureSize"))
    return call, pin(EDIT.get_material_expression_output_names(call), output)


def link_named(g, source, output, target, names):
    """Links to the first of `names` the target answers to: custom outputs name pins by property or display name."""
    for name in names:
        if EDIT.connect_material_expressions(source, output, target, name):
            return
    raise AssertionError(f"none of {names}")


def kit_material(material, spec):
    """An art kit's surface (StructureKit.json, FluxbornKit.json; checked by KitMaterials/spec.py): its colour,
    metallic and roughness; and where it is Flux (emission above 0), a glow in its colour that ignores the scene's
    exposure. The Flux colour is the FluxTint parameter, which the presentation sets to the side colour a capsule would
    show (BuildUnitArtSets.py), and FluxStrength defaults to the kit's emission."""
    color = EDIT.create_material_expression(material, unreal.MaterialExpressionVectorParameter, -700, 0)
    color.set_editor_property("parameter_name", "FluxTint" if spec["emission"] else "SurfaceColor")
    color.set_editor_property("default_value", unreal.LinearColor(*spec["color"]))
    assert EDIT.connect_material_property(color, "RGB", unreal.MaterialProperty.MP_BASE_COLOR), "base colour"
    for i, (key, prop) in enumerate([("metallic", unreal.MaterialProperty.MP_METALLIC), ("roughness", unreal.MaterialProperty.MP_ROUGHNESS)]):
        value = EDIT.create_material_expression(material, unreal.MaterialExpressionScalarParameter, -700, 180 + i * 120)
        value.set_editor_property("parameter_name", key.title())
        value.set_editor_property("default_value", spec[key])
        assert EDIT.connect_material_property(value, "", prop), key
    if spec["emission"]:
        strength = EDIT.create_material_expression(material, unreal.MaterialExpressionScalarParameter, -700, -180)
        strength.set_editor_property("parameter_name", "FluxStrength")
        strength.set_editor_property("default_value", spec["emission"])
        glow = EDIT.create_material_expression(material, unreal.MaterialExpressionMultiply, -400, -100)
        assert EDIT.connect_material_expressions(color, "RGB", glow, "A"), "Flux colour"
        assert EDIT.connect_material_expressions(strength, "", glow, "B"), "Flux strength"
        emissive = unexposed(material, glow, -150, -100)
        assert EDIT.connect_material_property(emissive, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR), "emissive"


def materials_named(known):
    """The materials a -VeyraMaterials=A,B argument names, each checked against known; None without it. Naming
    materials rebuilds only those, and imports no mesh."""
    named = next((token.split("=", 1)[1].split(",") for token in unreal.SystemLibrary.get_command_line().split()
                  if token.startswith("-VeyraMaterials=")), None)
    if named is not None:
        unknown = sorted(set(named) - set(known))
        assert not unknown, "Unknown material in -VeyraMaterials: " + ", ".join(unknown)
    return named


def refuse_locked(game, asset_paths):
    """Fails, naming each, when any of asset_paths (/Game/... package paths) is checked out read-only: a committed
    binary asset needs its Git LFS lock before a generator rewrites it (ADR-006 section 9). Call it before changing any
    asset, so a missing lock stops the build with nothing changed."""
    locked = []
    for path in asset_paths:
        target = Path(game) / "Content" / (path.removeprefix("/Game/") + ".uasset")
        if target.exists() and getattr(target.stat(), "st_file_attributes", 0) & stat.FILE_ATTRIBUTE_READONLY:
            locked.append(str(target))
    if locked:
        raise RuntimeError("Acquire the Git LFS lock before rebuilding:\n" + "\n".join(locked))
