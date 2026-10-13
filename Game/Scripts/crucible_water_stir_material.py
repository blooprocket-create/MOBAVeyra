"""M_CrucibleWaterStir's graph (ADR-076), built by ImportTerrainArt.py: one step of the river's ripples, which the water's
presentation draws into a render target, its parameters named by the water's settings.

Kept apart from the importer because its text is one of the material's inputs (KitMaterials/inputs.py): the importer
rebuilds the material only when this code, veyra_material_graph.py, its values (TerrainArt/inputs.py
stir_material_values, from the water's settings) or its texture change.
"""
import unreal

from veyra_material_graph import Graph

EDIT = unreal.MaterialEditingLibrary

# The textures it samples, by name: still water, its state until the presentation gives it one.
TEXTURES = ["T_Crucible_StillWater"]

# The sources one step takes: UVeyraWaterSettings::SourceSlots, which the C++ checks its settings against.
STIR_SOURCE_SLOTS = 16

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


def build(material, textures, values):
    """Builds one ripple step into material, which has no expressions: unlit, its emissive the next state. textures
    holds the TEXTURES by name; values is stir_material_values: its parameters' names and their defaults."""
    names = values["names"]
    defaults = values["defaults"]
    material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    material.set_editor_property("allow_negative_emissive_color", True)
    g = Graph(material)
    step = g.node(unreal.MaterialExpressionCustom, 3)
    step.set_editor_property("description", "Ripple step")
    step.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    step.set_editor_property("code", STIR_STEP_HLSL)
    pins = ["State", "UV", "Texel", "Shift", "Damping", "EdgeFade", "Phase"] + [f"S{slot}" for slot in range(STIR_SOURCE_SLOTS)]
    step.set_editor_property("inputs", [custom_input(name) for name in pins])
    state = g.node(unreal.MaterialExpressionTextureObjectParameter, 0, parameter_name=names["StateParameter"], group="Stir",
                   texture=textures["T_Crucible_StillWater"], sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
    g.link(state, "", step, "State")
    g.link(g.node(unreal.MaterialExpressionTextureCoordinate, 0), "", step, "UV")
    g.link(g.scalar(1, names["TexelParameter"], 1.0 / defaults["Resolution"], "Stir"), "", step, "Texel")
    shift = g.node(unreal.MaterialExpressionVectorParameter, 1, parameter_name=names["ShiftParameter"], group="Stir",
                   default_value=unreal.LinearColor(0.0, 0.0, 0.0, 0.0))
    g.link(shift, "RGBA", step, "Shift")
    g.link(g.scalar(1, names["DampingParameter"], defaults["Damping"], "Stir"), "", step, "Damping")
    g.link(g.scalar(1, names["EdgeFadeParameter"], defaults["EdgeFade"], "Stir"), "", step, "EdgeFade")
    g.link(g.scalar(1, names["PhaseParameter"], 0.0, "Stir"), "", step, "Phase")
    for slot in range(STIR_SOURCE_SLOTS):
        source = g.node(unreal.MaterialExpressionVectorParameter, 2, parameter_name=f"{names['SourceParameterPrefix']}{slot}", group="Stir",
                        default_value=unreal.LinearColor(0.0, 0.0, 0.0, 0.0))
        g.link(source, "RGBA", step, f"S{slot}")
    assert EDIT.connect_material_property(step, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR), "emissive"
