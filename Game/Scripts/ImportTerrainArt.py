"""Editor-only import of the Crucible's terrain textures and its Landscape material (ADR-040 §3).

Invoked by BuildTerrainArt.ps1 in an editor commandlet with the PythonScriptPlugin. Imports the generated textures
of Game/ArtSource/Environment/Terrain and builds M_CrucibleTerrain from them: four painted layers blended by their
heights, a large-scale variation over all of them, rock on whatever is steep, and darker, glossier ground at the
water's edge. Every number comes from TerrainTextures.json or the material's parameters, which the generated map's
style profile may override. Does not edit a map.
"""
import hashlib
import json
import re
from pathlib import Path

import sys

import unreal

sys.path.insert(0, str(Path(__file__).resolve().parent))
from veyra_material_graph import Graph, link_named, sample, world_aligned  # noqa: E402

GAME = Path(__file__).resolve().parents[1]
SOURCE = GAME / "ArtSource" / "Environment" / "Terrain"
SAVED = GAME / "Saved" / "TerrainArt"
DEST = "/Game/Veyra/World/Environment/Terrain"
PROFILE_PATH = SOURCE / "TerrainTextures.json"
PROFILE = json.loads(PROFILE_PATH.read_text(encoding="utf-8"))
LOOK = PROFILE["material"]
MANIFEST = json.loads((SOURCE / "manifest.json").read_text(encoding="utf-8"))
assert MANIFEST["profileSha256"] == hashlib.sha256(PROFILE_PATH.read_bytes()).hexdigest(), "Regenerate the textures after profile changes."

# The ripples' shader takes this many sources: UVeyraWaterSettings::SourceSlots, which the C++ checks its settings against.
STIR_SOURCE_SLOTS = 16


def water_settings():
    """The water's presentation settings (ADR-076; UVeyraWaterSettings in DefaultGame.ini): the material parameters the
    river's ripples are set by are named there, once, and read here."""
    text = (GAME / "Config" / "DefaultGame.ini").read_text(encoding="utf-8")
    section = re.search(r"^\[/Script/VeyraUI\.VeyraWaterSettings\]\s*$(.*?)(?=^\[|\Z)", text, re.M | re.S)
    assert section, "DefaultGame.ini needs [/Script/VeyraUI.VeyraWaterSettings]"
    return dict(re.findall(r"^(\w+)=(.*?)\s*$", section.group(1), re.M))


# Which generated layer paints each Landscape layer the World authoring pass weights (VeyraLandscapeBuild.cpp).
LANDSCAPE_LAYERS = [("Jungle", "Moss"), ("Lane", "Causeway"), ("Bank", "Shore"), ("Cliff", "Slate"), ("Earth", "Earth")]

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
    bases = {}
    for name, layer in LANDSCAPE_LAYERS:
        tile = g.scalar(2, f"{layer}TileSize", specs[layer]["tileMetres"] * 100.0, "Tiling")
        uv = g.op(unreal.MaterialExpressionDivide, 3, plan, tile)
        base = sample(g, 4, textures[f"T_Crucible_{layer}_BaseColor"], uv, unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
        normal = sample(g, 4, textures[f"T_Crucible_{layer}_Normal"], uv, unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
        orm = sample(g, 4, textures[f"T_Crucible_{layer}_ORMH"], uv, unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
        g.link(base, "RGB", blends["BaseColor"], f"Layer {name}")
        bases[layer] = base
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
    steep_from = g.scalar(2, "CliffNormalZFull", LOOK["cliffNormalZFull"], "Cliffs")
    steep_to = g.scalar(2, "CliffNormalZNone", LOOK["cliffNormalZNone"], "Cliffs")
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
    orm_x = sample(g, 4, textures["T_Crucible_Slate_ORMH"], uv_x, unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
    orm_y = sample(g, 4, textures["T_Crucible_Slate_ORMH"], uv_y, unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
    facing_x = g.node(unreal.MaterialExpressionComponentMask, 1, r=True, g=False, b=False, a=False)
    g.link(vertex_normal, "", facing_x, "")
    facing_x_abs = g.node(unreal.MaterialExpressionAbs, 2)
    g.link(facing_x, "", facing_x_abs, "")
    rock = g.node(unreal.MaterialExpressionLinearInterpolate, 5)
    g.link(rock_y, "RGB", rock, "A")
    g.link(rock_x, "RGB", rock, "B")
    g.link(facing_x_abs, "", rock, "Alpha")
    rock_orm = g.node(unreal.MaterialExpressionLinearInterpolate, 5)
    g.link(orm_y, "RGB", rock_orm, "A")
    g.link(orm_x, "RGB", rock_orm, "B")
    g.link(facing_x_abs, "", rock_orm, "Alpha")
    painted_and_rock = g.node(unreal.MaterialExpressionLinearInterpolate, 7)
    g.link(blends["BaseColor"], "", painted_and_rock, "A")
    g.link(rock, "", painted_and_rock, "B")
    g.link(steep, "", painted_and_rock, "Alpha")
    flat_normal = g.node(unreal.MaterialExpressionConstant3Vector, 6, constant=unreal.LinearColor(0.0, 0.0, 1.0, 0.0))
    projected_normal, projected_output = world_aligned(g, 6, "WorldAlignedNormal", textures["T_Crucible_Slate_Normal"], slate["tile"], "XYZ Texture")
    rock_normal = g.node(unreal.MaterialExpressionLinearInterpolate, 7)
    g.link(projected_normal, projected_output, rock_normal, "A")
    g.link(flat_normal, "", rock_normal, "B")
    softened = g.scalar(6, "CliffNormalFlatten", LOOK["cliffNormalFlatten"], "Cliffs")
    g.link(softened, "", rock_normal, "Alpha")
    normal_out = g.node(unreal.MaterialExpressionLinearInterpolate, 8)
    g.link(blends["Normal"], "", normal_out, "A")
    g.link(rock_normal, "", normal_out, "B")
    g.link(steep, "", normal_out, "Alpha")
    orm_out = g.node(unreal.MaterialExpressionLinearInterpolate, 8)
    g.link(blends["ORMH"], "", orm_out, "A")
    g.link(rock_orm, "", orm_out, "B")
    g.link(steep, "", orm_out, "Alpha")

    # Large-scale variation over every layer: brightness and a warm-cool shift, so no tiling shows across a lane.
    macro_tile = g.scalar(2, "MacroTileSize", PROFILE["macroTileMetres"] * 100.0, "Variation")
    macro_uv = g.op(unreal.MaterialExpressionDivide, 3, plan, macro_tile)
    macro = sample(g, 4, textures["T_Crucible_Macro"], macro_uv, unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
    dark = g.scalar(6, "MacroDarkest", LOOK["macroDarkest"], "Variation")
    light = g.scalar(6, "MacroBrightest", LOOK["macroBrightest"], "Variation")
    brightness = g.node(unreal.MaterialExpressionLinearInterpolate, 7)
    g.link(dark, "", brightness, "A")
    g.link(light, "", brightness, "B")
    g.link(macro, "R", brightness, "Alpha")
    cool = g.node(unreal.MaterialExpressionVectorParameter, 6, parameter_name="MacroCool", group="Variation",
                  default_value=unreal.LinearColor(*LOOK["macroCool"], 1.0))
    warm = g.node(unreal.MaterialExpressionVectorParameter, 6, parameter_name="MacroWarm", group="Variation",
                  default_value=unreal.LinearColor(*LOOK["macroWarm"], 1.0))
    tint = g.node(unreal.MaterialExpressionLinearInterpolate, 7)
    g.link(cool, "RGB", tint, "A")
    g.link(warm, "RGB", tint, "B")
    g.link(macro, "G", tint, "Alpha")
    varied = g.op(unreal.MaterialExpressionMultiply, 9, painted_and_rock, brightness)
    varied = g.op(unreal.MaterialExpressionMultiply, 9, varied, tint)

    # Overgrowth: patches of moss over the old paving and the pads, from the variation map's third channel, so a
    # broad stretch of stone never shows its tiling.
    overgrowth = PROFILE["overgrowth"]
    grow_from = g.scalar(8, "OvergrowthFrom", overgrowth["from"], "Variation")
    grow_to = g.scalar(8, "OvergrowthTo", overgrowth["to"], "Variation")
    patches = g.node(unreal.MaterialExpressionSmoothStep, 9)
    g.link(grow_from, "", patches, "Min")
    g.link(grow_to, "", patches, "Max")
    g.link(macro, "B", patches, "Value")
    strength = g.scalar(9, "OvergrowthStrength", overgrowth["strength"], "Variation")
    patches = g.op(unreal.MaterialExpressionMultiply, 10, patches, strength)
    # Top-projected moss belongs on the ground, not on the side-projected cliff faces.
    # Reuse the same slope transition as the cliff layer so the two cannot disagree.
    patches = g.op(unreal.MaterialExpressionMultiply, 10, patches, flatness)
    overgrown = g.node(unreal.MaterialExpressionLinearInterpolate, 10)
    g.link(varied, "", overgrown, "A")
    g.link(bases["Moss"], "RGB", overgrown, "B")
    g.link(patches, "", overgrown, "Alpha")
    varied = overgrown

    # The water's edge: darker and glossier ground up to a band above the river's surface.
    water_level = g.scalar(6, "WaterLevel", 0.0, "Water")
    wet_band = g.scalar(6, "WetBand", 60.0, "Water")
    wet_top = g.op(unreal.MaterialExpressionAdd, 7, water_level, wet_band)
    dryness = g.node(unreal.MaterialExpressionSmoothStep, 8)
    g.link(water_level, "", dryness, "Min")
    g.link(wet_top, "", dryness, "Max")
    g.link(height, "", dryness, "Value")
    wet_darkening = g.scalar(8, "WetDarkening", LOOK["wetDarkening"], "Water")
    wet_roughness = g.scalar(8, "WetRoughness", LOOK["wetRoughness"], "Water")
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


def build_water(textures):
    """M_CrucibleWater: Single Layer Water over the generated river surface, its ripples flowing downstream along the
    flow its vertices carry, foam along the shore where they say the water is shallowest."""
    water = PROFILE["water"]
    for control in ("roughness", "specular"):
        assert isinstance(water[control], (int, float)) and 0.0 <= water[control] <= 1.0, f"water.{control} must be in [0, 1]"
    path = f"{DEST}/M_CrucibleWater"
    material = unreal.load_asset(path) if ASSETS.does_asset_exist(path) else None
    if not material:
        material = TOOLS.create_asset("M_CrucibleWater", DEST, unreal.Material, unreal.MaterialFactoryNew())
    EDIT.delete_all_material_expressions(material)
    # Custom output nodes can outlive the bulk delete: a material may hold only one water output.
    for expression in EDIT.get_material_expressions(material):
        EDIT.delete_material_expression(material, expression)
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
    flowing = g.node(unreal.MaterialExpressionLinearInterpolate, 11)
    g.link(flat, "", flowing, "A")
    g.link(unpacked, "", flowing, "B")
    g.link(strength, "", flowing, "Alpha")

    # The rings the units wading it stir (ADR-076): the water's presentation simulates their heights over a window of
    # the river and sets them here; still water reads none. They bend the surface and froth on their crests.
    stir = water_settings()
    still = textures["T_Crucible_StillWater"]
    heights = g.node(unreal.MaterialExpressionTextureObjectParameter, 9, parameter_name=stir["HeightParameter"], group="Stir",
                     texture=still, sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
    window = g.node(unreal.MaterialExpressionVectorParameter, 9, parameter_name=stir["WindowParameter"], group="Stir",
                    default_value=unreal.LinearColor(0.0, 0.0, 1.0, 0.0))
    bend = g.scalar(9, stir["NormalStrengthParameter"], 0.0, "Stir")
    rings = g.node(unreal.MaterialExpressionCustom, 10)
    rings.set_editor_property("description", "Stirred rings")
    rings.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    rings.set_editor_property("code", STIR_SURFACE_HLSL)
    rings.set_editor_property("inputs", [custom_input(name) for name in ("Heights", "Place", "Window", "Bend")])
    g.link(heights, "", rings, "Heights")
    g.link(plan, "", rings, "Place")
    g.link(window, "RGBA", rings, "Window")
    g.link(bend, "", rings, "Bend")
    tilt = g.node(unreal.MaterialExpressionComponentMask, 11, r=True, g=True, b=False, a=False)
    g.link(rings, "", tilt, "")
    zero2 = g.node(unreal.MaterialExpressionConstant, 10, r=0.0)
    tilt3 = g.node(unreal.MaterialExpressionAppendVector, 11)
    g.link(tilt, "", tilt3, "A")
    g.link(zero2, "", tilt3, "B")
    normal = g.node(unreal.MaterialExpressionNormalize, 12)
    g.link(g.op(unreal.MaterialExpressionAdd, 12, flowing, tilt3), "", normal, "VectorInput")
    crest = g.node(unreal.MaterialExpressionComponentMask, 11, r=False, g=False, b=True, a=False)
    g.link(rings, "", crest, "")

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
    shore_foam = g.op(unreal.MaterialExpressionMultiply, 11, shore, foam_pattern)
    # Only a ring's strongest crests froth: above stirFoamFrom, rising by stirFoam.
    stir_foam = g.scalar(10, "StirFoam", water["stirFoam"], "Water")
    stir_foam_from = g.scalar(10, "StirFoamFrom", water["stirFoamFrom"], "Water")
    crest_foam = g.node(unreal.MaterialExpressionSaturate, 11)
    g.link(g.op(unreal.MaterialExpressionMultiply, 11, g.op(unreal.MaterialExpressionSubtract, 11, crest, stir_foam_from), stir_foam), "", crest_foam, "")
    foam = g.node(unreal.MaterialExpressionSaturate, 12)
    g.link(g.op(unreal.MaterialExpressionAdd, 12, shore_foam, crest_foam), "", foam, "")
    foam_color = g.node(unreal.MaterialExpressionVectorParameter, 10, parameter_name="FoamColor", group="Water",
                        default_value=unreal.LinearColor(*water["foamColor"], 1.0))
    base = g.op(unreal.MaterialExpressionMultiply, 12, foam_color, foam, a_out="RGB")
    still = g.scalar(10, "WaterRoughness", water["roughness"], "Water")
    specular = g.scalar(10, "WaterSpecular", water["specular"], "Water")
    frothy = g.scalar(10, "FrothRoughness", water["frothRoughness"], "Water")
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
    EDIT.connect_material_property(specular, "", unreal.MaterialProperty.MP_SPECULAR)
    errors = EDIT.recompile_material(material)
    assert not errors, f"{material.get_name()} does not compile: {errors}"
    ASSETS.save_loaded_asset(material)
    return material

STIR_SURFACE_HLSL = """
float2 uv = (Place - Window.xy) / max(Window.z, 1.0);
float t = Window.w;
float inside = step(0.0, uv.x) * step(uv.x, 1.0) * step(0.0, uv.y) * step(uv.y, 1.0);
float l = Texture2DSample(Heights, HeightsSampler, uv - float2(t, 0)).r;
float r = Texture2DSample(Heights, HeightsSampler, uv + float2(t, 0)).r;
float d = Texture2DSample(Heights, HeightsSampler, uv - float2(0, t)).r;
float u = Texture2DSample(Heights, HeightsSampler, uv + float2(0, t)).r;
float h = Texture2DSample(Heights, HeightsSampler, uv).r;
return float3((l - r) * Bend * inside, (d - u) * Bend * inside, abs(h) * inside);
"""

# One step of the ripples: each texel's height from its neighbours' (a wave), kept by the damping, faded toward the
# window's edge so nothing reflects off it, each unit's stir added as rings; the state carried with the window as it
# moves, so the ripples stay where they are on the water. Red is the height now, green the height a step ago.
STIR_STEP_HLSL = """
float2 from = UV + Shift.xy * Texel;
float inside = step(0.0, from.x) * step(from.x, 1.0) * step(0.0, from.y) * step(from.y, 1.0);
float4 here = Texture2DSample(State, StateSampler, from);
float n = Texture2DSample(State, StateSampler, from + float2(0, -Texel)).r;
float s = Texture2DSample(State, StateSampler, from + float2(0, Texel)).r;
float e = Texture2DSample(State, StateSampler, from + float2(Texel, 0)).r;
float w = Texture2DSample(State, StateSampler, from + float2(-Texel, 0)).r;
float next = ((n + s + e + w) * 0.5 - here.g) * Damping * inside;
float4 sources[16] = { S0, S1, S2, S3, S4, S5, S6, S7, S8, S9, S10, S11, S12, S13, S14, S15 };
for (int i = 0; i < 16; i++)
{
    float reach = length(UV - sources[i].xy) / max(sources[i].z, 1e-5);
    next += sources[i].w * sin(Phase + i * 2.39996) * saturate(1.0 - reach * reach) * 0.25;
}
float2 edge = min(UV, 1.0 - UV);
next *= saturate(min(edge.x, edge.y) / max(EdgeFade, 1e-5));
return float3(next, here.r * inside, 0);
"""


def custom_input(name):
    entry = unreal.CustomInput()
    entry.set_editor_property("input_name", name)
    return entry


def build_stir(textures):
    """M_CrucibleWaterStir (ADR-076): one step of the river's ripples, drawn into a render target by the water's
    presentation; its parameters are named by the water's settings."""
    stir = water_settings()
    path = f"{DEST}/M_CrucibleWaterStir"
    material = unreal.load_asset(path) if ASSETS.does_asset_exist(path) else None
    if not material:
        material = TOOLS.create_asset("M_CrucibleWaterStir", DEST, unreal.Material, unreal.MaterialFactoryNew())
    EDIT.delete_all_material_expressions(material)
    material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    material.set_editor_property("allow_negative_emissive_color", True)
    g = Graph(material)
    step = g.node(unreal.MaterialExpressionCustom, 3)
    step.set_editor_property("description", "Ripple step")
    step.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    step.set_editor_property("code", STIR_STEP_HLSL)
    names = ["State", "UV", "Texel", "Shift", "Damping", "EdgeFade", "Phase"] + [f"S{slot}" for slot in range(STIR_SOURCE_SLOTS)]
    step.set_editor_property("inputs", [custom_input(name) for name in names])
    state = g.node(unreal.MaterialExpressionTextureObjectParameter, 0, parameter_name=stir["StateParameter"], group="Stir",
                   texture=textures["T_Crucible_StillWater"], sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
    g.link(state, "", step, "State")
    g.link(g.node(unreal.MaterialExpressionTextureCoordinate, 0), "", step, "UV")
    g.link(g.scalar(1, stir["TexelParameter"], 1.0 / float(stir["Resolution"]), "Stir"), "", step, "Texel")
    shift = g.node(unreal.MaterialExpressionVectorParameter, 1, parameter_name=stir["ShiftParameter"], group="Stir",
                   default_value=unreal.LinearColor(0.0, 0.0, 0.0, 0.0))
    g.link(shift, "RGBA", step, "Shift")
    g.link(g.scalar(1, stir["DampingParameter"], float(stir["Damping"]), "Stir"), "", step, "Damping")
    g.link(g.scalar(1, stir["EdgeFadeParameter"], float(stir["EdgeFade"]), "Stir"), "", step, "EdgeFade")
    g.link(g.scalar(1, stir["PhaseParameter"], 0.0, "Stir"), "", step, "Phase")
    for slot in range(STIR_SOURCE_SLOTS):
        source = g.node(unreal.MaterialExpressionVectorParameter, 2, parameter_name=f"{stir['SourceParameterPrefix']}{slot}", group="Stir",
                        default_value=unreal.LinearColor(0.0, 0.0, 0.0, 0.0))
        g.link(source, "RGBA", step, f"S{slot}")
    assert EDIT.connect_material_property(step, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR), "emissive"
    errors = EDIT.recompile_material(material)
    assert not errors, f"{material.get_name()} does not compile: {errors}"
    ASSETS.save_loaded_asset(material)
    return material


def main():
    SAVED.mkdir(parents=True, exist_ok=True)
    water_only = "-veyrawateronly" in unreal.SystemLibrary.get_command_line().lower().split()
    textures = {}
    for entry in MANIFEST["files"]:
        if water_only and entry["layer"] != "Water":
            continue
        texture = import_texture(entry)
        textures[Path(entry["name"]).stem] = texture
    material = None if water_only else build_material(textures)
    water = build_water(textures)
    stir = build_stir(textures)
    report = {"material": material.get_path_name() if material else None, "textures": sorted(t.get_path_name() for t in textures.values()),
              "water": water.get_path_name(), "stir": stir.get_path_name(), "layers": [] if water_only else [name for name, _ in LANDSCAPE_LAYERS]}
    (SAVED / "import.json").write_text(json.dumps(report, indent=2), encoding="utf-8")
    unreal.log(f"Imported {len(textures)} textures and built {'water only' if water_only else 'terrain and water'}.")


main()
