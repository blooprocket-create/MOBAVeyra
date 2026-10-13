"""M_CrucibleWater's graph (ADR-040 §6), built by ImportTerrainArt.py from the river's generated normal texture.

Kept apart from the importer because its text is one of the material's inputs (KitMaterials/inputs.py): the importer
rebuilds the material only when this code, veyra_material_graph.py, its values (TerrainArt/inputs.py
water_material_values, which carry the names its ripple parameters take from the water's settings) or its textures
change.
"""
import unreal

from veyra_material_graph import Graph, link_named, sample

EDIT = unreal.MaterialEditingLibrary

# The textures it samples, by name: the flowing ripples, and the still water its stirred rings read where nothing stirs it.
TEXTURES = ["T_Crucible_WaterNormal", "T_Crucible_StillWater"]

# The stirred rings' slope and crest, from the heights the water's presentation simulates over a window of the river
# (ADR-076): Window is its corner, its size and a texel's share of it. Zero outside the window.
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


def custom_input(name):
    entry = unreal.CustomInput()
    entry.set_editor_property("input_name", name)
    return entry


def build(material, textures, values):
    """Builds Single Layer Water over the generated river surface into material, which has no expressions: its ripples
    flow downstream along the flow its vertices carry, with foam along the shore where they say the water is
    shallowest, and bends and froths where the units wading it stir it (ADR-076). textures holds the TEXTURES by name;
    values is the profile's water with the names of its ripple parameters (water_material_values)."""
    water = values
    stir = values["stir"]
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
    heights = g.node(unreal.MaterialExpressionTextureObjectParameter, 9, parameter_name=stir["HeightParameter"], group="Stir",
                     texture=textures["T_Crucible_StillWater"], sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
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
    level = g.node(unreal.MaterialExpressionConstant, 10, r=0.0)
    tilt3 = g.node(unreal.MaterialExpressionAppendVector, 11)
    g.link(tilt, "", tilt3, "A")
    g.link(level, "", tilt3, "B")
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
