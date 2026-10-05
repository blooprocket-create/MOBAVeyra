"""Editor-only import of the Crucible's terrain textures and its Landscape material (ADR-040 §3).

Invoked by BuildTerrainArt.ps1 in an editor commandlet with the PythonScriptPlugin. Imports the generated textures
of Game/ArtSource/Environment/Terrain and builds M_CrucibleTerrain from them: four painted layers blended by their
heights, a large-scale variation over all of them, rock on whatever is steep, and darker, glossier ground at the
water's edge. Every number comes from TerrainTextures.json or the material's parameters, which the generated map's
style profile may override. Does not edit a map.
"""
import hashlib
import json
from pathlib import Path

import unreal

GAME = Path(__file__).resolve().parents[1]
SOURCE = GAME / "ArtSource" / "Environment" / "Terrain"
SAVED = GAME / "Saved" / "TerrainArt"
DEST = "/Game/Veyra/World/Environment/Terrain"
PROFILE_PATH = SOURCE / "TerrainTextures.json"
PROFILE = json.loads(PROFILE_PATH.read_text(encoding="utf-8"))
MANIFEST = json.loads((SOURCE / "manifest.json").read_text(encoding="utf-8"))
assert MANIFEST["profileSha256"] == hashlib.sha256(PROFILE_PATH.read_bytes()).hexdigest(), "Regenerate the textures after profile changes."

# Which generated layer paints each Landscape layer the World authoring pass weights (VeyraLandscapeBuild.cpp).
LANDSCAPE_LAYERS = [("Jungle", "Moss"), ("Lane", "Causeway"), ("Bank", "Shore"), ("Cliff", "Slate")]

TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
EDIT = unreal.MaterialEditingLibrary
ASSETS = unreal.EditorAssetLibrary


def import_texture(entry):
    filename = SOURCE / entry["name"]
    assert hashlib.sha256(filename.read_bytes()).hexdigest() == entry["sha256"], f"{entry['name']} differs from the manifest"
    name = filename.stem
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", str(filename))
    task.set_editor_property("destination_path", DEST + "/Textures")
    task.set_editor_property("destination_name", name)
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("save", False)
    TOOLS.import_asset_tasks([task])
    texture = unreal.load_asset(f"{DEST}/Textures/{name}")
    assert texture, f"{name} did not import"
    if name.endswith("_WaterNormal"):
        # Ripple normals with the shore's foam in alpha: kept whole, so not compressed as a normal map.
        texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_BC7)
        texture.set_editor_property("srgb", False)
        texture.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_WORLD)
    elif name.endswith("_Normal"):
        texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
        texture.set_editor_property("srgb", False)
        texture.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_WORLD_NORMAL_MAP)
    elif name.endswith("_BaseColor"):
        texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_DEFAULT)
        texture.set_editor_property("srgb", True)
        texture.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_WORLD)
    else:
        # Packed data with alpha (ambient occlusion, roughness, metallic, height), or the macro variation: linear.
        texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_BC7)
        texture.set_editor_property("srgb", False)
        texture.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_WORLD)
    ASSETS.save_loaded_asset(texture)
    return texture


class Graph:
    """A small helper over MaterialEditingLibrary that lays nodes out in columns."""

    def __init__(self, material):
        self.material = material
        self.rows = {}

    def node(self, cls, column, **props):
        row = self.rows.get(column, 0)
        self.rows[column] = row + 1
        expression = EDIT.create_material_expression(self.material, cls, -2200 + column * 320, -1200 + row * 140)
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


def sample(graph, column, texture, uv, sampler_type):
    expression = graph.node(unreal.MaterialExpressionTextureSample, column, texture=texture, sampler_type=sampler_type,
                            sampler_source=unreal.SamplerSourceMode.SSM_WRAP_WORLD_GROUP_SETTINGS)
    graph.link(uv, "", expression, "UVs")
    return expression


def build_material(textures):
    path = f"{DEST}/M_CrucibleTerrain"
    material = unreal.load_asset(path) if ASSETS.does_asset_exist(path) else None
    if not material:
        material = TOOLS.create_asset("M_CrucibleTerrain", DEST, unreal.Material, unreal.MaterialFactoryNew())
    EDIT.delete_all_material_expressions(material)
    g = Graph(material)

    world = g.node(unreal.MaterialExpressionWorldPosition, 0)
    plan = g.node(unreal.MaterialExpressionComponentMask, 1, r=True, g=True, b=False, a=False)
    g.link(world, "", plan, "")
    height = g.node(unreal.MaterialExpressionComponentMask, 1, r=False, g=False, b=True, a=False)
    g.link(world, "", height, "")

    blends = {}
    for output, sampler in (("BaseColor", unreal.MaterialSamplerType.SAMPLERTYPE_COLOR),
                            ("Normal", unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL),
                            ("ORMH", unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)):
        blend = g.node(unreal.MaterialExpressionLandscapeLayerBlend, 6)
        layers = []
        for index, (name, _) in enumerate(LANDSCAPE_LAYERS):
            layer = unreal.LayerBlendInput()
            layer.set_editor_property("layer_name", name)
            layer.set_editor_property("blend_type", unreal.LandscapeLayerBlendType.LB_HEIGHT_BLEND)
            layer.set_editor_property("preview_weight", 1.0 if index == 0 else 0.0)
            layers.append(layer)
        blend.set_editor_property("layers", layers)
        blends[output] = blend

    specs = {layer["id"]: layer for layer in PROFILE["layers"]}
    slate = {}
    for name, layer in LANDSCAPE_LAYERS:
        tile = g.scalar(2, f"{layer}TileSize", specs[layer]["tileMetres"] * 100.0, "Tiling")
        uv = g.op(unreal.MaterialExpressionDivide, 3, plan, tile)
        base = sample(g, 4, textures[f"T_Crucible_{layer}_BaseColor"], uv, unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
        normal = sample(g, 4, textures[f"T_Crucible_{layer}_Normal"], uv, unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
        orm = sample(g, 4, textures[f"T_Crucible_{layer}_ORMH"], uv, unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
        g.link(base, "RGB", blends["BaseColor"], f"Layer {name}")
        g.link(normal, "RGB", blends["Normal"], f"Layer {name}")
        g.link(orm, "RGB", blends["ORMH"], f"Layer {name}")
        for blend in blends.values():
            g.link(orm, "A", blend, f"Height {name}")
        if layer == "Slate":
            slate = {"tile": tile, "normal": normal, "orm": orm}

    # Rock wherever the ground is steep, whatever was painted: projected from the side, so cliffs are not smeared.
    vertex_normal = g.node(unreal.MaterialExpressionVertexNormalWS, 0)
    up = g.node(unreal.MaterialExpressionComponentMask, 1, r=False, g=False, b=True, a=False)
    g.link(vertex_normal, "", up, "")
    steep_from = g.scalar(2, "CliffNormalZFull", 0.62, "Cliffs")
    steep_to = g.scalar(2, "CliffNormalZNone", 0.82, "Cliffs")
    flatness = g.node(unreal.MaterialExpressionSmoothStep, 3)
    g.link(steep_from, "", flatness, "Min")
    g.link(steep_to, "", flatness, "Max")
    g.link(up, "", flatness, "Value")
    steep = g.node(unreal.MaterialExpressionOneMinus, 4)
    g.link(flatness, "", steep, "")
    side_x = g.node(unreal.MaterialExpressionComponentMask, 1, r=False, g=True, b=True, a=False)
    g.link(world, "", side_x, "")
    side_y = g.node(unreal.MaterialExpressionComponentMask, 1, r=True, g=False, b=True, a=False)
    g.link(world, "", side_y, "")
    uv_x = g.op(unreal.MaterialExpressionDivide, 3, side_x, slate["tile"])
    uv_y = g.op(unreal.MaterialExpressionDivide, 3, side_y, slate["tile"])
    rock_x = sample(g, 4, textures["T_Crucible_Slate_BaseColor"], uv_x, unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
    rock_y = sample(g, 4, textures["T_Crucible_Slate_BaseColor"], uv_y, unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
    facing_x = g.node(unreal.MaterialExpressionComponentMask, 1, r=True, g=False, b=False, a=False)
    g.link(vertex_normal, "", facing_x, "")
    facing_x_abs = g.node(unreal.MaterialExpressionAbs, 2)
    g.link(facing_x, "", facing_x_abs, "")
    rock = g.node(unreal.MaterialExpressionLinearInterpolate, 5)
    g.link(rock_y, "RGB", rock, "A")
    g.link(rock_x, "RGB", rock, "B")
    g.link(facing_x_abs, "", rock, "Alpha")
    painted_and_rock = g.node(unreal.MaterialExpressionLinearInterpolate, 7)
    g.link(blends["BaseColor"], "", painted_and_rock, "A")
    g.link(rock, "", painted_and_rock, "B")
    g.link(steep, "", painted_and_rock, "Alpha")
    flat_normal = g.node(unreal.MaterialExpressionConstant3Vector, 6, constant=unreal.LinearColor(0.0, 0.0, 1.0, 0.0))
    rock_normal = g.node(unreal.MaterialExpressionLinearInterpolate, 7)
    g.link(slate["normal"], "RGB", rock_normal, "A")
    g.link(flat_normal, "", rock_normal, "B")
    softened = g.scalar(6, "CliffNormalFlatten", 0.5, "Cliffs")
    g.link(softened, "", rock_normal, "Alpha")
    normal_out = g.node(unreal.MaterialExpressionLinearInterpolate, 8)
    g.link(blends["Normal"], "", normal_out, "A")
    g.link(rock_normal, "", normal_out, "B")
    g.link(steep, "", normal_out, "Alpha")
    orm_out = g.node(unreal.MaterialExpressionLinearInterpolate, 8)
    g.link(blends["ORMH"], "", orm_out, "A")
    g.link(slate["orm"], "RGB", orm_out, "B")
    g.link(steep, "", orm_out, "Alpha")

    # Large-scale variation over every layer: brightness and a warm-cool shift, so no tiling shows across a lane.
    macro_tile = g.scalar(2, "MacroTileSize", PROFILE["macroTileMetres"] * 100.0, "Variation")
    macro_uv = g.op(unreal.MaterialExpressionDivide, 3, plan, macro_tile)
    macro = sample(g, 4, textures["T_Crucible_Macro"], macro_uv, unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
    dark = g.scalar(6, "MacroDarkest", 0.82, "Variation")
    light = g.scalar(6, "MacroBrightest", 1.12, "Variation")
    brightness = g.node(unreal.MaterialExpressionLinearInterpolate, 7)
    g.link(dark, "", brightness, "A")
    g.link(light, "", brightness, "B")
    g.link(macro, "R", brightness, "Alpha")
    cool = g.node(unreal.MaterialExpressionVectorParameter, 6, parameter_name="MacroCool", group="Variation",
                  default_value=unreal.LinearColor(0.94, 1.0, 1.04, 1.0))
    warm = g.node(unreal.MaterialExpressionVectorParameter, 6, parameter_name="MacroWarm", group="Variation",
                  default_value=unreal.LinearColor(1.06, 1.0, 0.92, 1.0))
    tint = g.node(unreal.MaterialExpressionLinearInterpolate, 7)
    g.link(cool, "RGB", tint, "A")
    g.link(warm, "RGB", tint, "B")
    g.link(macro, "G", tint, "Alpha")
    varied = g.op(unreal.MaterialExpressionMultiply, 9, painted_and_rock, brightness)
    varied = g.op(unreal.MaterialExpressionMultiply, 9, varied, tint)

    # The water's edge: darker and glossier ground up to a band above the river's surface.
    water_level = g.scalar(6, "WaterLevel", 0.0, "Water")
    wet_band = g.scalar(6, "WetBand", 60.0, "Water")
    wet_top = g.op(unreal.MaterialExpressionAdd, 7, water_level, wet_band)
    dryness = g.node(unreal.MaterialExpressionSmoothStep, 8)
    g.link(water_level, "", dryness, "Min")
    g.link(wet_top, "", dryness, "Max")
    g.link(height, "", dryness, "Value")
    wet_darkening = g.scalar(8, "WetDarkening", 0.62, "Water")
    wet_roughness = g.scalar(8, "WetRoughness", 0.22, "Water")
    darkening = g.node(unreal.MaterialExpressionLinearInterpolate, 9)
    g.link(wet_darkening, "", darkening, "A")
    one = g.node(unreal.MaterialExpressionConstant, 8, r=1.0)
    g.link(one, "", darkening, "B")
    g.link(dryness, "", darkening, "Alpha")
    base_out = g.op(unreal.MaterialExpressionMultiply, 10, varied, darkening)
    roughness_in = g.node(unreal.MaterialExpressionComponentMask, 9, r=False, g=True, b=False, a=False)
    g.link(orm_out, "", roughness_in, "")
    roughness_out = g.node(unreal.MaterialExpressionLinearInterpolate, 10)
    g.link(wet_roughness, "", roughness_out, "A")
    g.link(roughness_in, "", roughness_out, "B")
    g.link(dryness, "", roughness_out, "Alpha")
    occlusion = g.node(unreal.MaterialExpressionComponentMask, 9, r=True, g=False, b=False, a=False)
    g.link(orm_out, "", occlusion, "")

    EDIT.connect_material_property(base_out, "", unreal.MaterialProperty.MP_BASE_COLOR)
    EDIT.connect_material_property(normal_out, "", unreal.MaterialProperty.MP_NORMAL)
    EDIT.connect_material_property(roughness_out, "", unreal.MaterialProperty.MP_ROUGHNESS)
    EDIT.connect_material_property(occlusion, "", unreal.MaterialProperty.MP_AMBIENT_OCCLUSION)
    errors = EDIT.recompile_material(material)
    assert not errors, f"{material.get_name()} does not compile: {errors}"
    ASSETS.save_loaded_asset(material)
    return material


def link_named(g, source, output, target, names):
    """Links to the first of `names` the target answers to: custom outputs name pins by property or display name."""
    for name in names:
        if EDIT.connect_material_expressions(source, output, target, name):
            return
    raise AssertionError(f"none of {names}")


def build_water(textures):
    """M_CrucibleWater: Single Layer Water over the generated river surface, its ripples flowing downstream along the
    flow its vertices carry, foam along the shore where they say the water is shallowest."""
    water = PROFILE["water"]
    path = f"{DEST}/M_CrucibleWater"
    material = unreal.load_asset(path) if ASSETS.does_asset_exist(path) else None
    if not material:
        material = TOOLS.create_asset("M_CrucibleWater", DEST, unreal.Material, unreal.MaterialFactoryNew())
    EDIT.delete_all_material_expressions(material)
    material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_SINGLE_LAYER_WATER)
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_OPAQUE)
    g = Graph(material)
    normal_texture = textures["T_Crucible_WaterNormal"]

    world = g.node(unreal.MaterialExpressionWorldPosition, 0)
    plan = g.node(unreal.MaterialExpressionComponentMask, 1, r=True, g=True, b=False, a=False)
    g.link(world, "", plan, "")
    tile = g.scalar(1, "RippleTileSize", water["tileMetres"] * 100.0, "Water")
    uv = g.op(unreal.MaterialExpressionDivide, 2, plan, tile)

    # Flow: each vertex's downstream direction, encoded 0..1 in red and green.
    color = g.node(unreal.MaterialExpressionVertexColor, 0)
    flow_raw = g.node(unreal.MaterialExpressionComponentMask, 1, r=True, g=True, b=False, a=False)
    g.link(color, "", flow_raw, "")
    two = g.node(unreal.MaterialExpressionConstant, 1, r=2.0)
    one = g.node(unreal.MaterialExpressionConstant, 1, r=1.0)
    flow = g.op(unreal.MaterialExpressionSubtract, 3, g.op(unreal.MaterialExpressionMultiply, 2, flow_raw, two), one)
    distance = g.scalar(2, "FlowDistance", water["flowDistance"], "Water")
    flow = g.op(unreal.MaterialExpressionMultiply, 4, flow, distance)

    # Two phases of the same flow, half a cycle apart, each faded out as it resets.
    time = g.node(unreal.MaterialExpressionTime, 0)
    cycle = g.scalar(1, "FlowCycleSeconds", water["flowCycleSeconds"], "Water")
    t = g.op(unreal.MaterialExpressionDivide, 2, time, cycle)
    half = g.node(unreal.MaterialExpressionConstant, 2, r=0.5)
    phase_a = g.node(unreal.MaterialExpressionFrac, 3)
    g.link(t, "", phase_a, "")
    phase_b = g.node(unreal.MaterialExpressionFrac, 3)
    g.link(g.op(unreal.MaterialExpressionAdd, 3, t, half), "", phase_b, "")
    uv_a = g.op(unreal.MaterialExpressionSubtract, 5, uv, g.op(unreal.MaterialExpressionMultiply, 4, flow, phase_a))
    uv_b = g.op(unreal.MaterialExpressionAdd, 6, g.op(unreal.MaterialExpressionSubtract, 5, uv, g.op(unreal.MaterialExpressionMultiply, 4, flow, phase_b)), half)
    ripple_a = sample(g, 7, normal_texture, uv_a, unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
    ripple_b = sample(g, 7, normal_texture, uv_b, unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
    fade = g.node(unreal.MaterialExpressionAbs, 6)
    g.link(g.op(unreal.MaterialExpressionSubtract, 5, phase_a, half), "", fade, "")
    fade = g.op(unreal.MaterialExpressionMultiply, 7, fade, two)
    ripple = g.node(unreal.MaterialExpressionLinearInterpolate, 8)
    g.link(ripple_a, "RGBA", ripple, "A")
    g.link(ripple_b, "RGBA", ripple, "B")
    g.link(fade, "", ripple, "Alpha")
    tangent = g.node(unreal.MaterialExpressionComponentMask, 9, r=True, g=True, b=True, a=False)
    g.link(ripple, "", tangent, "")
    unpacked = g.op(unreal.MaterialExpressionSubtract, 10, g.op(unreal.MaterialExpressionMultiply, 9, tangent, two), one)
    flat = g.node(unreal.MaterialExpressionConstant3Vector, 9, constant=unreal.LinearColor(0.0, 0.0, 1.0, 0.0))
    strength = g.scalar(9, "RippleNormalStrength", water["normalStrength"], "Water")
    normal = g.node(unreal.MaterialExpressionLinearInterpolate, 11)
    g.link(flat, "", normal, "A")
    g.link(unpacked, "", normal, "B")
    g.link(strength, "", normal, "Alpha")

    # Foam where the water meets the shore: the vertices' blue is 0 at the edge, 1 in the deep.
    depth = g.node(unreal.MaterialExpressionComponentMask, 1, r=False, g=False, b=True, a=False)
    g.link(color, "", depth, "")
    band = g.scalar(8, "FoamBand", water["foamBand"], "Water")
    zero = g.node(unreal.MaterialExpressionConstant, 8, r=0.0)
    deep = g.node(unreal.MaterialExpressionSmoothStep, 9)
    g.link(zero, "", deep, "Min")
    g.link(band, "", deep, "Max")
    g.link(depth, "", deep, "Value")
    shore = g.node(unreal.MaterialExpressionOneMinus, 10)
    g.link(deep, "", shore, "")
    foam_pattern = g.node(unreal.MaterialExpressionComponentMask, 9, r=False, g=False, b=False, a=True)
    g.link(ripple, "", foam_pattern, "")
    foam = g.op(unreal.MaterialExpressionMultiply, 11, shore, foam_pattern)
    foam_color = g.node(unreal.MaterialExpressionVectorParameter, 10, parameter_name="FoamColor", group="Water",
                        default_value=unreal.LinearColor(*water["foamColor"], 1.0))
    base = g.op(unreal.MaterialExpressionMultiply, 12, foam_color, foam, a_out="RGB")
    still = g.scalar(10, "WaterRoughness", water["roughness"], "Water")
    frothy = g.node(unreal.MaterialExpressionConstant, 10, r=0.6)
    roughness = g.node(unreal.MaterialExpressionLinearInterpolate, 12)
    g.link(still, "", roughness, "A")
    g.link(frothy, "", roughness, "B")
    g.link(foam, "", roughness, "Alpha")

    output = g.node(unreal.MaterialExpressionSingleLayerWaterMaterialOutput, 13)
    for pins, value in (((["ScatteringCoefficients", "Scattering Coefficients"]), g.node(unreal.MaterialExpressionVectorParameter, 12, parameter_name="ScatteringCoefficients", group="Water", default_value=unreal.LinearColor(*water["scattering"], 0.0))),
                        ((["AbsorptionCoefficients", "Absorption Coefficients"]), g.node(unreal.MaterialExpressionVectorParameter, 12, parameter_name="AbsorptionCoefficients", group="Water", default_value=unreal.LinearColor(*water["absorption"], 0.0))),
                        ((["PhaseG", "Phase G"]), g.scalar(12, "PhaseG", water["phaseG"], "Water")),
                        ((["ColorScaleBehindWater", "Color Scale Behind Water"]), g.scalar(12, "ColorScaleBehindWater", water["colorScaleBehindWater"], "Water"))):
        # Vector parameters give their colour; the coefficients are three channels.
        link_named(g, value, "RGB" if isinstance(value, unreal.MaterialExpressionVectorParameter) else "", output, pins)
    EDIT.connect_material_property(base, "", unreal.MaterialProperty.MP_BASE_COLOR)
    EDIT.connect_material_property(normal, "", unreal.MaterialProperty.MP_NORMAL)
    EDIT.connect_material_property(roughness, "", unreal.MaterialProperty.MP_ROUGHNESS)
    EDIT.connect_material_property(foam, "", unreal.MaterialProperty.MP_OPACITY)
    errors = EDIT.recompile_material(material)
    assert not errors, f"{material.get_name()} does not compile: {errors}"
    ASSETS.save_loaded_asset(material)
    return material

def main():
    SAVED.mkdir(parents=True, exist_ok=True)
    textures = {}
    for entry in MANIFEST["files"]:
        texture = import_texture(entry)
        textures[Path(entry["name"]).stem] = texture
    material = build_material(textures)
    water = build_water(textures)
    report = {"material": material.get_path_name(), "textures": sorted(t.get_path_name() for t in textures.values()),
              "water": water.get_path_name(), "layers": [name for name, _ in LANDSCAPE_LAYERS]}
    (SAVED / "import.json").write_text(json.dumps(report, indent=2), encoding="utf-8")
    unreal.log(f"Imported {len(textures)} terrain textures and built {material.get_path_name()}.")


main()
