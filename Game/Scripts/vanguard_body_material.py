"""M_VeyraVanguardBody's graph (ADR-064 §4, ADR-068 §2), built by ImportVanguardBodies.py from the kit's bodyMaterial.

Kept apart from the importer because its text is one of the material's inputs (KitMaterials/inputs.py): the importer
rebuilds the material only when this code, veyra_material_graph.py, the kit's bodyMaterial or the toon light's
parameters change, so an import that changes none of them leaves the material's asset as it was.
"""
import unreal

from veyra_material_graph import Graph, link_named

EDIT = unreal.MaterialEditingLibrary

# The engine's temporal dither function.
DITHER = "/Engine/Functions/Engine_MaterialFunctions02/Utility/DitherTemporalAA.DitherTemporalAA"

VEIL_HLSL = """
// The hidden body's veil (ADR-068, section 6): how much of the body shows (x) and how brightly it glows (y). Bands rise
// through it: a triangle wave of height that climbs with time, thickest where it peaks. Its silhouette, where the surface
// turns from the view past RimStart (Edge, a fresnel), stays whole and glows, its edge a pixel wide wherever it stands.
float Band = abs(frac(Z * ShimmerScale - Time * ShimmerSpeed) * 2.0 - 1.0);
float Shows = Opacity * lerp(1.0, Band, ShimmerDepth);
float Silhouette = saturate((Edge - RimStart) / max(fwidth(Edge), 1e-5) + 0.5);
return float2(lerp(1.0, max(Shows, Silhouette), Veil), Veil * Silhouette * lerp(1.0 - ShimmerDepth, 1.0, Band));
"""


def build(material, collection, values):
    """Builds the one material every generated body wears into material, which has no expressions: unlit, lit by
    collection, its own toon light, with values (the kit's bodyMaterial).

    The vertex colour is the body's colour. The side facing the collection's sun is lit (colour x litTint x SunColor),
    the side away from it is in a painted shadow (colour x shadowTint), and the two meet in one soft band where the
    surface turns from the sun past bandThreshold. A rim of the sun's light (a fresnel from rimStart to rimEnd) edges the
    lit side. The whole is brightness times what the scene's exposure maps to white, so a body reads the same under the
    Crucible's physical sun as in a preview; what the vertex alpha marks glows on top at glowGain times that white."""
    toon = values["toon"]
    g = Graph(material)

    def vector(column, name, rgb):
        return g.node(unreal.MaterialExpressionVectorParameter, column, parameter_name=name, default_value=unreal.LinearColor(*rgb, 1.0), group="Toon")

    def light(column, name):
        """A colour (RGB) of the toon light. The collection is set first: naming the parameter then looks its ID up in
        the collection (the expression's PostEditChangeProperty)."""
        assert name in [str(parameter.get_editor_property("parameter_name")) for parameter in collection.get_editor_property("vector_parameters")], name
        node = g.node(unreal.MaterialExpressionCollectionParameter, column, collection=collection)
        node.set_editor_property("parameter_name", name)
        rgb = g.node(unreal.MaterialExpressionComponentMask, column + 1, r=True, g=True, b=True, a=False)
        g.link(node, "", rgb, "")
        return rgb

    # A vertex colour's colour output is unnamed; R, G, B and A are its others. A connection to a name that is not an
    # output fails quietly, leaving the body black, so every connection is checked (Graph.link asserts).
    color = g.node(unreal.MaterialExpressionVertexColor, 0)
    # The normal the bands and rim read: the vertex normal, turned with the side shown (a cloth sheet shows both).
    normal = g.op(unreal.MaterialExpressionMultiply, 2, g.node(unreal.MaterialExpressionVertexNormalWS, 1), g.node(unreal.MaterialExpressionTwoSidedSign, 1))
    # How squarely the surface faces the sun, from -1 (away) to 1.
    to_sun = g.node(unreal.MaterialExpressionNormalize, 2)
    g.link(light(0, "ToSun"), "", to_sun, "")
    facing = g.op(unreal.MaterialExpressionDotProduct, 3, normal, to_sun)
    # One soft band: 0 in shadow, 1 lit.
    threshold, softness = toon["bandThreshold"], toon["bandSoftness"]
    band = g.node(unreal.MaterialExpressionSmoothStep, 4, const_min=threshold - softness, const_max=threshold + softness)
    g.link(facing, "", band, "Value")
    # Lit and shadowed colour, and the band between them.
    sun = light(0, "SunColor")
    lit = g.op(unreal.MaterialExpressionMultiply, 3, g.op(unreal.MaterialExpressionMultiply, 2, color, vector(1, "LitTint", toon["litTint"]), "", "RGB"), sun)
    shadow = g.op(unreal.MaterialExpressionMultiply, 3, color, vector(2, "ShadowTint", toon["shadowTint"]), "", "RGB")
    shaded = g.node(unreal.MaterialExpressionLinearInterpolate, 4)
    g.link(shadow, "", shaded, "A")
    g.link(lit, "", shaded, "B")
    g.link(band, "", shaded, "Alpha")
    # The rim: the sun's light along the lit side's silhouette.
    fresnel = g.node(unreal.MaterialExpressionFresnel, 2, exponent=toon["rimExponent"], base_reflect_fraction=0.0)
    g.link(normal, "", fresnel, "Normal")
    rim_edge = g.node(unreal.MaterialExpressionSmoothStep, 3, const_min=toon["rimStart"], const_max=toon["rimEnd"])
    g.link(fresnel, "", rim_edge, "Value")
    rim_strength = g.scalar(3, "RimStrength", toon["rimStrength"], "Toon")
    rim = g.op(unreal.MaterialExpressionMultiply, 5, g.op(unreal.MaterialExpressionMultiply, 4, rim_edge, band), rim_strength)
    rimmed = g.op(unreal.MaterialExpressionAdd, 6, shaded, g.op(unreal.MaterialExpressionMultiply, 5, sun, rim))
    bright = g.op(unreal.MaterialExpressionMultiply, 7, rimmed, g.scalar(6, "Brightness", toon["brightness"], "Toon"))
    # The glow, on top: what the vertex alpha marks, at glowGain.
    glow = g.op(unreal.MaterialExpressionMultiply, 2, g.op(unreal.MaterialExpressionMultiply, 1, color, color, "", "A"),
                g.scalar(1, "GlowStrength", values["glowGain"], "Toon"))
    # Raised while a cast holds the body (ADR-072 §5): 1 at rest, the presentation's gain at full strain.
    glow = g.op(unreal.MaterialExpressionMultiply, 3, glow, g.scalar(2, values["castGlow"]["parameter"], 1.0, "Toon"))
    total = g.op(unreal.MaterialExpressionAdd, 8, bright, glow)
    # The veil (ADR-068 §6): while the presentation raises Veil, a body on the viewer's side hidden from its enemies
    # thins to opacity of itself, dithered, under bands rising through it, while its silhouette stays whole and glows in VeilTint.
    veil = values["veil"]
    hidden = g.scalar(5, veil["parameter"], 0.0, "Veil")
    tint = g.node(unreal.MaterialExpressionVectorParameter, 5, parameter_name=veil["tintParameter"], default_value=unreal.LinearColor(1.0, 1.0, 1.0, 1.0),
                  group="Veil")
    height = g.node(unreal.MaterialExpressionComponentMask, 5, r=False, g=False, b=True, a=False)
    g.link(g.node(unreal.MaterialExpressionWorldPosition, 4), "", height, "")
    veil_edge = g.node(unreal.MaterialExpressionFresnel, 5, exponent=veil["rimExponent"], base_reflect_fraction=0.0)
    inputs = [("Veil", hidden), ("Z", height), ("Time", g.node(unreal.MaterialExpressionTime, 5)), ("Edge", veil_edge)]
    for key in ("opacity", "shimmerScale", "shimmerSpeed", "shimmerDepth", "rimStart"):
        inputs.append((key[0].upper() + key[1:], g.node(unreal.MaterialExpressionConstant, 5, r=float(veil[key]))))
    entries = []
    for name, _ in inputs:
        entry = unreal.CustomInput()
        entry.set_editor_property("input_name", name)
        entries.append(entry)
    shimmer = g.node(unreal.MaterialExpressionCustom, 6, code=VEIL_HLSL, description="VeyraVeil", output_type=unreal.CustomMaterialOutputType.CMOT_FLOAT2,
                     inputs=entries)
    for name, node in inputs:
        g.link(node, "", shimmer, name)
    shows = g.node(unreal.MaterialExpressionComponentMask, 7, r=True, g=False, b=False, a=False)
    g.link(shimmer, "", shows, "")
    # The engine's temporal dither, which the upscaler resolves into a smooth fade.
    dither = g.node(unreal.MaterialExpressionMaterialFunctionCall, 8, material_function=unreal.load_asset(DITHER))
    link_named(g, shows, "", dither, ("Alpha Threshold", "AlphaThreshold"))
    assert EDIT.connect_material_property(dither, "", unreal.MaterialProperty.MP_OPACITY_MASK), "opacity mask"
    lit_rim = g.node(unreal.MaterialExpressionComponentMask, 7, r=False, g=True, b=False, a=False)
    g.link(shimmer, "", lit_rim, "")
    veil_rim = g.op(unreal.MaterialExpressionMultiply, 8, lit_rim, g.scalar(7, "VeilRimStrength", veil["rimStrength"], "Veil"))
    tinted_rim = g.op(unreal.MaterialExpressionMultiply, 9, veil_rim, tint, "", "RGB")
    emissive = g.unexposed(10, g.op(unreal.MaterialExpressionAdd, 9, total, tinted_rim))
    assert EDIT.connect_material_property(emissive, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR), "emissive"
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_MASKED)
    # The dither spreads its noise about the alpha, so a clip of one half keeps that share of the pixels; a solid body
    # (alpha 1) keeps every one.
    material.set_editor_property("opacity_mask_clip_value", 0.5)
    material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    # Cloth is a single sheet (ADR-069 §4): both its faces show.
    material.set_editor_property("two_sided", True)
    # Skinned meshes use it.
    material.set_editor_property("used_with_skeletal_mesh", True)
