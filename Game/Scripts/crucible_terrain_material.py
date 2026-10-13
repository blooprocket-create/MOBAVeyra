"""M_CrucibleTerrain's graph (ADR-040 §3), built by ImportTerrainArt.py from the terrain's generated textures.

Five painted layers blended by their heights, a large-scale variation over all of them, rock on whatever is steep, and
darker, glossier ground at the water's edge. Every number comes from the values it is given (TerrainArt/inputs.py
terrain_material_values) or the material's parameters, which the generated map's style profile may override. Kept apart
from the importer because its text is one of the material's inputs (KitMaterials/inputs.py): the importer rebuilds the
material only when this code, veyra_material_graph.py, its values or the textures it samples change.
"""
import unreal

from veyra_material_graph import Graph, sample, world_aligned

EDIT = unreal.MaterialEditingLibrary

# Which generated layer paints each Landscape layer the World authoring pass weights (VeyraLandscapeBuild.cpp).
LAYERS = [("Jungle", "Moss"), ("Lane", "Causeway"), ("Bank", "Shore"), ("Cliff", "Slate"), ("Earth", "Earth")]
# The textures it samples, by name.
TEXTURES = [f"T_Crucible_{layer}_{kind}" for _, layer in LAYERS for kind in ("BaseColor", "Normal", "ORMH")] + ["T_Crucible_Macro"]


def build(material, textures, values):
    """Builds the Crucible's Landscape material into material, which has no expressions, from textures (by name, those
    TEXTURES names) and values (terrain_material_values)."""
    look = values["look"]
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
        for index, (name, _) in enumerate(LAYERS):
            layer = unreal.LayerBlendInput()
            layer.set_editor_property("layer_name", name)
            layer.set_editor_property("blend_type", unreal.LandscapeLayerBlendType.LB_HEIGHT_BLEND)
            layer.set_editor_property("preview_weight", 1.0 if index == 0 else 0.0)
            layers.append(layer)
        blend.set_editor_property("layers", layers)
        blends[output] = blend

    slate = {}
    bases = {}
    for name, layer in LAYERS:
        tile = g.scalar(2, f"{layer}TileSize", values["tileMetres"][layer] * 100.0, "Tiling")
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
    steep_from = g.scalar(2, "CliffNormalZFull", look["cliffNormalZFull"], "Cliffs")
    steep_to = g.scalar(2, "CliffNormalZNone", look["cliffNormalZNone"], "Cliffs")
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
    softened = g.scalar(6, "CliffNormalFlatten", look["cliffNormalFlatten"], "Cliffs")
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
    macro_tile = g.scalar(2, "MacroTileSize", values["macroTileMetres"] * 100.0, "Variation")
    macro_uv = g.op(unreal.MaterialExpressionDivide, 3, plan, macro_tile)
    macro = sample(g, 4, textures["T_Crucible_Macro"], macro_uv, unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
    dark = g.scalar(6, "MacroDarkest", look["macroDarkest"], "Variation")
    light = g.scalar(6, "MacroBrightest", look["macroBrightest"], "Variation")
    brightness = g.node(unreal.MaterialExpressionLinearInterpolate, 7)
    g.link(dark, "", brightness, "A")
    g.link(light, "", brightness, "B")
    g.link(macro, "R", brightness, "Alpha")
    cool = g.node(unreal.MaterialExpressionVectorParameter, 6, parameter_name="MacroCool", group="Variation",
                  default_value=unreal.LinearColor(*look["macroCool"], 1.0))
    warm = g.node(unreal.MaterialExpressionVectorParameter, 6, parameter_name="MacroWarm", group="Variation",
                  default_value=unreal.LinearColor(*look["macroWarm"], 1.0))
    tint = g.node(unreal.MaterialExpressionLinearInterpolate, 7)
    g.link(cool, "RGB", tint, "A")
    g.link(warm, "RGB", tint, "B")
    g.link(macro, "G", tint, "Alpha")
    varied = g.op(unreal.MaterialExpressionMultiply, 9, painted_and_rock, brightness)
    varied = g.op(unreal.MaterialExpressionMultiply, 9, varied, tint)

    # Overgrowth: patches of moss over the old paving and the pads, from the variation map's third channel, so a
    # broad stretch of stone never shows its tiling.
    overgrowth = values["overgrowth"]
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
    wet_darkening = g.scalar(8, "WetDarkening", look["wetDarkening"], "Water")
    wet_roughness = g.scalar(8, "WetRoughness", look["wetRoughness"], "Water")
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
