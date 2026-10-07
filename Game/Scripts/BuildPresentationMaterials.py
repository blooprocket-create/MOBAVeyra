"""Build the presentation's generated materials from ArtSource/Presentation/PresentationMaterials.json (ADR-063).

Run through BuildPresentationMaterials.ps1, in an editor commandlet. Each material's graph comes from its kind
below and its values from the spec, so the same spec and generator version always build the same asset. The spec is
checked whole by PresentationMaterials/spec.py, which CI also runs, before any asset changes.

The Crucible is lit physically (a sun of tens of thousands of lux under a manual exposure), so an unlit emissive of 1
reads thousands of times too dark. Every glow here is scaled by the inverse of the scene's exposure (unexposed below),
so it reads the same under any light; its strength in the spec is in multiples of what the exposure maps to white.
"""
import hashlib
import json
import stat
import sys
from pathlib import Path

import unreal

GAME = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
sys.path.insert(0, str(GAME / "Scripts"))
from PresentationMaterials.spec import GENERATOR_VERSION, GRAPHIC_SHAPES, RULES, validate  # noqa: E402
from veyra_material_graph import unexposed  # noqa: E402

SPEC_FILE = GAME / "ArtSource" / "Presentation" / "PresentationMaterials.json"
SPEC = json.loads(SPEC_FILE.read_text())
SAVED = GAME / "Saved" / "PresentationMaterials"
EDIT = unreal.MaterialEditingLibrary
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()

PROBLEMS = validate(SPEC)
assert not PROBLEMS, "PresentationMaterials.json: " + "; ".join(PROBLEMS)
DEST = SPEC["destination"]


def expression(material, kind, x, y, **properties):
    node = EDIT.create_material_expression(material, kind, x, y)
    for key, value in properties.items():
        node.set_editor_property(key, value)
    return node


def build_overlay_flash(material, spec):
    """Emissive = colour x strength x glowGain, brightest at the silhouette, by the inverse of the scene's exposure:
    additive and unlit, for a mesh's overlay, so it brightens a sunlit body alike under any light. It is drawn over
    static bodies and kit art and over skinned Vanguard bodies (ADR-064 §3), so it is marked for skeletal meshes: a
    cooked game draws a material without that mark on one as the engine's default."""
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_ADDITIVE)
    material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    material.set_editor_property("used_with_skeletal_mesh", True)
    color = expression(material, unreal.MaterialExpressionVectorParameter, -1100, -200,
                       parameter_name=spec["colorParameter"], default_value=unreal.LinearColor(*spec["defaultColor"]))
    strength = expression(material, unreal.MaterialExpressionScalarParameter, -1100, 0,
                          parameter_name=spec["strengthParameter"], default_value=0.0)
    fresnel = expression(material, unreal.MaterialExpressionFresnel, -1100, 200, exponent=spec["rimExponent"], base_reflect_fraction=0.0)
    floor = expression(material, unreal.MaterialExpressionConstant, -1100, 380, r=spec["rimFloor"])
    full = expression(material, unreal.MaterialExpressionConstant, -1100, 460, r=1.0)
    rim = expression(material, unreal.MaterialExpressionLinearInterpolate, -800, 260)
    assert EDIT.connect_material_expressions(floor, "", rim, "A"), "rim floor"
    assert EDIT.connect_material_expressions(full, "", rim, "B"), "rim full"
    assert EDIT.connect_material_expressions(fresnel, "", rim, "Alpha"), "fresnel"
    lit = expression(material, unreal.MaterialExpressionMultiply, -800, -100)
    assert EDIT.connect_material_expressions(color, "RGB", lit, "A"), "colour"
    assert EDIT.connect_material_expressions(strength, "", lit, "B"), "strength"
    rimmed = expression(material, unreal.MaterialExpressionMultiply, -600, 0)
    assert EDIT.connect_material_expressions(lit, "", rimmed, "A"), "lit"
    assert EDIT.connect_material_expressions(rim, "", rimmed, "B"), "rim"
    gained = expression(material, unreal.MaterialExpressionMultiply, -450, 0, const_b=spec["glowGain"])
    assert EDIT.connect_material_expressions(rimmed, "", gained, "A"), "rimmed"
    emissive = unexposed(material, gained, -250, 0)
    assert EDIT.connect_material_property(emissive, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR), "emissive"


OUTLINE_HLSL = """
// The hovered unit's outline (ADR-063, section 3): a pixel outside every stencilled shape, near one, takes that shape's colour.
// An enemy's reach wins over an ally's or a neutral's; reaches grow with the view's height past ReferenceHeight.
float3 Base = Scene.rgb;
float Own = round(Stencil.r);
if (Own == EnemyStencil.r || Own == AllyStencil.r || Own == NeutralStencil.r)
{
    return Base;
}
float2 UV = GetDefaultSceneTextureUV(Parameters, {stencil});
float2 Texel = View.BufferSizeAndInvSize.zw;
float Scale = max(View.ViewSizeAndInvSize.y / ReferenceHeight.r, 1.0);
float EnemyReach = EnemyThickness.r * Scale;
float OtherReach = OtherThickness.r * Scale;
int Reach = (int)ceil(max(EnemyReach, OtherReach));
float Found = 0.0;
[loop] for (int X = -Reach; X <= Reach; ++X)
{
    [loop] for (int Y = -Reach; Y <= Reach; ++Y)
    {
        float Distance = length(float2(X, Y));
        float Kind = round(SceneTextureLookup(UV + float2(X, Y) * Texel, {stencil}, false).r);
        if (Kind == EnemyStencil.r && Distance <= EnemyReach)
        {
            Found = Kind;
        }
        else if ((Kind == AllyStencil.r || Kind == NeutralStencil.r) && Found == 0.0 && Distance <= OtherReach)
        {
            Found = Kind;
        }
    }
}
if (Found == EnemyStencil.r) return EnemyColor.rgb;
if (Found == AllyStencil.r) return AllyColor.rgb;
if (Found == NeutralStencil.r) return NeutralColor.rgb;
return Base;
"""


def custom_input(name):
    entry = unreal.CustomInput()
    entry.set_editor_property("input_name", name)
    return entry


def build_post_process_outline(material, spec):
    """A post-process pass, after tonemapping, that outlines custom-depth stencilled shapes by their stencil."""
    stencils = spec["stencils"]
    material.set_editor_property("material_domain", unreal.MaterialDomain.MD_POST_PROCESS)
    material.set_editor_property("blendable_location", unreal.BlendableLocation.BL_SCENE_COLOR_AFTER_TONEMAPPING)
    # The engine's own ids for the scene textures, not numbers copied by hand.
    stencil_id = int(unreal.SceneTextureId.PPI_CUSTOM_STENCIL.value)
    scene = expression(material, unreal.MaterialExpressionSceneTexture, -1200, -400, scene_texture_id=unreal.SceneTextureId.PPI_POST_PROCESS_INPUT0)
    stencil = expression(material, unreal.MaterialExpressionSceneTexture, -1200, -250, scene_texture_id=unreal.SceneTextureId.PPI_CUSTOM_STENCIL)
    inputs = [("Scene", scene, "Color"), ("Stencil", stencil, "Color")]
    y = -100
    for side in ("enemy", "ally", "neutral"):
        node = expression(material, unreal.MaterialExpressionVectorParameter, -1200, y, parameter_name=spec["colorParameters"][side],
                          default_value=unreal.LinearColor(1.0, 1.0, 1.0, 1.0))
        inputs.append((side.title() + "Color", node, "RGB"))
        y += 150
    for side in ("enemy", "ally", "neutral"):
        node = expression(material, unreal.MaterialExpressionScalarParameter, -1200, y, parameter_name=spec["stencilParameters"][side],
                          default_value=float(stencils[side]))
        inputs.append((side.title() + "Stencil", node, ""))
        y += 100
    for name, value in (("EnemyThickness", spec["thicknessPixels"]["enemy"]), ("OtherThickness", spec["thicknessPixels"]["other"]),
                        ("ReferenceHeight", spec["referenceHeight"])):
        node = expression(material, unreal.MaterialExpressionScalarParameter, -1200, y, parameter_name=name, default_value=float(value))
        inputs.append((name, node, ""))
        y += 100
    custom = expression(material, unreal.MaterialExpressionCustom, -600, 0,
                        code=OUTLINE_HLSL.replace("{stencil}", str(stencil_id)), description="VeyraHoverOutline",
                        output_type=unreal.CustomMaterialOutputType.CMOT_FLOAT3,
                        inputs=[custom_input(name) for name, _, _ in inputs])
    for name, node, output in inputs:
        EDIT.connect_material_expressions(node, output, custom, name)
    EDIT.connect_material_property(custom, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)


INK_HLSL = """
// The toon characters' ink (ADR-068, section 3). Characters write their custom depth with InkStencil. A pixel takes ink
// where a neighbour within reach is a character's visible surface lying nearer than this pixel by more than DepthGap:
// the character's silhouette against what lies behind it, and one part of it in front of a farther part. The ink is
// the nearest such surface's own colour, darkened and tinted, so a line reads as a deeper tone of what it outlines. It is
// solid where SolidNeighbours of the eight neighbours see that surface and fainter where fewer do, which softens its
// edge; the temporal upscaler, which runs after this pass, smooths it with the scene.
// Places are viewport UVs, each mapped into its own scene texture. Colours are read as the pass's input is (the scene's
// own brightness, before exposure), so darkening and tinting are multiplications.
float2 Here = GetViewportUV(Parameters);
float2 Reach = Thickness.r * max(View.ViewSizeAndInvSize.y / ReferenceHeight.r, 1.0) * View.ViewSizeAndInvSize.zw;
float HereScene = SceneTextureLookup(ViewportUVToSceneTextureUV(Here, {scenedepth}), {scenedepth}, false).r;
float HereCustom = SceneTextureLookup(ViewportUVToSceneTextureUV(Here, {customdepth}), {customdepth}, false).r;
bool HereCharacter = round(SceneTextureLookup(ViewportUVToSceneTextureUV(Here, {stencil}), {stencil}, false).r) == InkStencil.r
    && HereCustom <= HereScene + VisibleSlack.r;
float Depth = HereCharacter ? HereCustom : HereScene;
float3 Ink = Scene.rgb;
float Nearest = Depth;
float Seen = 0.0;
const float2 Around[8] = { float2(1, 0), float2(-1, 0), float2(0, 1), float2(0, -1), float2(0.7071, 0.7071), float2(-0.7071, 0.7071),
                           float2(0.7071, -0.7071), float2(-0.7071, -0.7071) };
[unroll] for (int Index = 0; Index < 8; ++Index)
{
    float2 There = Here + Around[Index] * Reach;
    if (round(SceneTextureLookup(ViewportUVToSceneTextureUV(There, {stencil}), {stencil}, false).r) != InkStencil.r)
    {
        continue;
    }
    float ThereCustom = SceneTextureLookup(ViewportUVToSceneTextureUV(There, {customdepth}), {customdepth}, false).r;
    // Hidden behind the world, as behind a wall, a character draws no ink.
    if (ThereCustom > SceneTextureLookup(ViewportUVToSceneTextureUV(There, {scenedepth}), {scenedepth}, false).r + VisibleSlack.r
        || Depth - ThereCustom <= DepthGap.r)
    {
        continue;
    }
    Seen += 1.0;
    if (ThereCustom < Nearest)
    {
        Nearest = ThereCustom;
        Ink = SceneTextureLookup(Parameters, ViewportUVToSceneTextureUV(There, {scene}), {scene}, false).rgb * Darken.r * InkTint.rgb;
    }
}
return lerp(Scene.rgb, Ink, saturate(Seen / SolidNeighbours.r));
"""


def build_post_process_ink(material, spec):
    """A post-process pass that inks the toon characters' silhouettes and overlaps (ADR-068 §3). It runs after depth of field and before the
    temporal upscaler, at the rendering's own resolution, so the upscaler smooths its lines as it does the scene's edges."""
    material.set_editor_property("material_domain", unreal.MaterialDomain.MD_POST_PROCESS)
    material.set_editor_property("blendable_location", unreal.BlendableLocation.BL_SCENE_COLOR_AFTER_DOF)
    ids = {"{scene}": unreal.SceneTextureId.PPI_POST_PROCESS_INPUT0, "{stencil}": unreal.SceneTextureId.PPI_CUSTOM_STENCIL,
           "{customdepth}": unreal.SceneTextureId.PPI_CUSTOM_DEPTH, "{scenedepth}": unreal.SceneTextureId.PPI_SCENE_DEPTH}
    code = INK_HLSL
    for token, texture in ids.items():
        code = code.replace(token, str(int(texture.value)))
    # Each scene texture the code reads must also be an input, so the pass binds it.
    scene = expression(material, unreal.MaterialExpressionSceneTexture, -1200, -500, scene_texture_id=ids["{scene}"])
    inputs = [("Scene", scene, "Color")]
    y = -350
    for name, texture in (("StencilTexture", ids["{stencil}"]), ("CustomDepthTexture", ids["{customdepth}"]), ("SceneDepthTexture", ids["{scenedepth}"])):
        node = expression(material, unreal.MaterialExpressionSceneTexture, -1200, y, scene_texture_id=texture)
        inputs.append((name, node, "Color"))
        y += 150
    stencil = expression(material, unreal.MaterialExpressionScalarParameter, -1200, y, parameter_name=spec["stencilParameter"], default_value=float(spec["stencil"]))
    inputs.append(("InkStencil", stencil, ""))
    y += 100
    for name, value in (("Thickness", spec["thicknessPixels"]), ("ReferenceHeight", spec["referenceHeight"]), ("Darken", spec["darken"]),
                        ("DepthGap", spec["depthGap"]), ("VisibleSlack", spec["visibleSlack"]), ("SolidNeighbours", spec["solidNeighbours"])):
        node = expression(material, unreal.MaterialExpressionScalarParameter, -1200, y, parameter_name=name, default_value=float(value))
        inputs.append((name, node, ""))
        y += 100
    tint = expression(material, unreal.MaterialExpressionVectorParameter, -1200, y, parameter_name="InkTint", default_value=unreal.LinearColor(*spec["inkTint"]))
    inputs.append(("InkTint", tint, "RGB"))
    custom = expression(material, unreal.MaterialExpressionCustom, -600, 0, code=code, description="VeyraToonInk",
                        output_type=unreal.CustomMaterialOutputType.CMOT_FLOAT3, inputs=[custom_input(name) for name, _, _ in inputs])
    for name, node, output in inputs:
        assert EDIT.connect_material_expressions(node, output, custom, name), "ink input " + name
    assert EDIT.connect_material_property(custom, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR), "ink"


def build_particle_smoke(material, spec):
    """A particle's puff of stylized smoke: a solid, hard-edged blob (masked) whose edge is ragged by noise and which
    erodes through growing holes as the particle's alpha fades with age. It is lit as a ball (a normal domed from the
    sprite's centre), so the sun shades it like any solid in the scene; its albedo is the particle's colour darkened,
    and it glows in that colour while young, by an amount that does not depend on the scene's exposure."""
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_MASKED)
    material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
    material.set_editor_property("used_with_niagara_sprites", True)
    material.set_editor_property("opacity_mask_clip_value", spec["clip"])
    color = expression(material, unreal.MaterialExpressionParticleColor, -1500, -300)
    # The dome: 1 at the sprite's centre falling to 0 at its edge (1 - the square of the UV's distance from the centre
    # in half-widths), so overlapping puffs merge into rounded shapes rather than points.
    uv = expression(material, unreal.MaterialExpressionTextureCoordinate, -1700, 150)
    offset = expression(material, unreal.MaterialExpressionSubtract, -1550, 150, const_b=0.5)
    assert EDIT.connect_material_expressions(uv, "", offset, "A"), "uv"
    across = expression(material, unreal.MaterialExpressionMultiply, -1400, 150, const_b=2.0)
    assert EDIT.connect_material_expressions(offset, "", across, "A"), "offset"
    squared = expression(material, unreal.MaterialExpressionDotProduct, -1250, 180)
    assert EDIT.connect_material_expressions(across, "", squared, "A"), "across"
    assert EDIT.connect_material_expressions(across, "", squared, "B"), "across again"
    dome = expression(material, unreal.MaterialExpressionOneMinus, -1100, 180)
    assert EDIT.connect_material_expressions(squared, "", dome, ""), "squared"
    # Its normal, as a ball's: the offset across the sprite, and up out of it by what the dome leaves.
    domed = expression(material, unreal.MaterialExpressionSaturate, -1000, 260)
    assert EDIT.connect_material_expressions(dome, "", domed, ""), "dome"
    rise = expression(material, unreal.MaterialExpressionSquareRoot, -900, 260)
    assert EDIT.connect_material_expressions(domed, "", rise, ""), "domed"
    normal = expression(material, unreal.MaterialExpressionAppendVector, -750, 200)
    assert EDIT.connect_material_expressions(across, "", normal, "A"), "across"
    assert EDIT.connect_material_expressions(rise, "", normal, "B"), "rise"
    assert EDIT.connect_material_property(normal, "", unreal.MaterialProperty.MP_NORMAL), "normal"
    # Noise in world space (the noise's own default position), from 0 to 1: puffs side by side share it, so their
    # holes run through the body together.
    noise = expression(material, unreal.MaterialExpressionNoise, -1300, 450, scale=spec["noiseScale"], levels=2,
                       output_min=0.0, output_max=1.0, turbulence=False)
    ragged = expression(material, unreal.MaterialExpressionMultiply, -1100, 450, const_b=spec["ragged"])
    assert EDIT.connect_material_expressions(noise, "", ragged, "A"), "noise"
    # Age: the share of its alpha the particle has lost, which the emitter fades over its life.
    age = expression(material, unreal.MaterialExpressionOneMinus, -1100, 600)
    assert EDIT.connect_material_expressions(color, "A", age, ""), "alpha"
    eroded = expression(material, unreal.MaterialExpressionMultiply, -950, 600, const_b=spec["erosion"])
    assert EDIT.connect_material_expressions(age, "", eroded, "A"), "age"
    # Kept where the dome stands above the noise and the erosion together.
    roughened = expression(material, unreal.MaterialExpressionSubtract, -800, 400)
    assert EDIT.connect_material_expressions(dome, "", roughened, "A"), "dome"
    assert EDIT.connect_material_expressions(ragged, "", roughened, "B"), "ragged"
    mask = expression(material, unreal.MaterialExpressionSubtract, -650, 450)
    assert EDIT.connect_material_expressions(roughened, "", mask, "A"), "roughened"
    assert EDIT.connect_material_expressions(eroded, "", mask, "B"), "eroded"
    assert EDIT.connect_material_property(mask, "", unreal.MaterialProperty.MP_OPACITY_MASK), "opacity mask"
    # Albedo: the colour darkened; matte.
    albedo = expression(material, unreal.MaterialExpressionMultiply, -600, -350, const_b=spec["albedo"])
    assert EDIT.connect_material_expressions(color, "", albedo, "A"), "colour"
    assert EDIT.connect_material_property(albedo, "", unreal.MaterialProperty.MP_BASE_COLOR), "base colour"
    rough = expression(material, unreal.MaterialExpressionConstant, -600, -250, r=1.0)
    assert EDIT.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS), "roughness"
    # Glow: glowFloor + glowGain x alpha squared, so a puff glows as it leaves the body and darkens as it thins; scaled
    # by the inverse of the scene's exposure, so it reads the same under any light.
    alpha_squared = expression(material, unreal.MaterialExpressionMultiply, -1100, -150)
    assert EDIT.connect_material_expressions(color, "A", alpha_squared, "A"), "alpha"
    assert EDIT.connect_material_expressions(color, "A", alpha_squared, "B"), "alpha again"
    gained = expression(material, unreal.MaterialExpressionMultiply, -950, -150, const_b=spec["glowGain"])
    assert EDIT.connect_material_expressions(alpha_squared, "", gained, "A"), "alpha squared"
    glow = expression(material, unreal.MaterialExpressionAdd, -800, -150, const_b=spec["glowFloor"])
    assert EDIT.connect_material_expressions(gained, "", glow, "A"), "gained"
    glowing = unexposed(material, glow, -600, -100)
    # Brightest at the puff's heart, so the glow reads as embers inside the smoke rather than its surface.
    heart = expression(material, unreal.MaterialExpressionMultiply, -500, -100)
    assert EDIT.connect_material_expressions(glowing, "", heart, "A"), "unexposed"
    assert EDIT.connect_material_expressions(domed, "", heart, "B"), "domed"
    emissive = expression(material, unreal.MaterialExpressionMultiply, -400, -200)
    assert EDIT.connect_material_expressions(color, "", emissive, "A"), "colour"
    assert EDIT.connect_material_expressions(heart, "", emissive, "B"), "heart"
    assert EDIT.connect_material_property(emissive, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR), "emissive"


def build_particle_effect(material, spec):
    """A stylized effect particle (ADR-063 §4): a solid, hard-edged shape (masked) whose edge is ragged by world-space
    noise and which erodes through growing holes as it ages. It flares white-hot as it is born, cools into its colour,
    then darkens as it frays away. Its age is the particle's normalized age, which every Niagara renderer passes, so the
    look keeps its timing whatever a template does with colour and alpha. A sprite is a ball, lit through a domed
    normal; a ribbon is a strand, domed across its width (its V) and lit flat. Its glow reads the same under any light."""
    sprite = spec["shape"] == "sprite"
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_MASKED)
    material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
    material.set_editor_property("used_with_niagara_sprites" if sprite else "used_with_niagara_ribbons", True)
    material.set_editor_property("opacity_mask_clip_value", spec["clip"])
    # Hue: the particle's colour folded under 1 by its brightest channel, so a template that brightens colour past 1
    # keeps its hue rather than washing the glow out to white.
    color = expression(material, unreal.MaterialExpressionParticleColor, -2100, -450)
    red_green = expression(material, unreal.MaterialExpressionMax, -1900, -350)
    assert EDIT.connect_material_expressions(color, "R", red_green, "A"), "red"
    assert EDIT.connect_material_expressions(color, "G", red_green, "B"), "green"
    brightest = expression(material, unreal.MaterialExpressionMax, -1750, -350)
    assert EDIT.connect_material_expressions(red_green, "", brightest, "A"), "red and green"
    assert EDIT.connect_material_expressions(color, "B", brightest, "B"), "blue"
    folding = expression(material, unreal.MaterialExpressionMax, -1600, -350, const_b=1.0)
    assert EDIT.connect_material_expressions(brightest, "", folding, "A"), "brightest"
    hue = expression(material, unreal.MaterialExpressionDivide, -1450, -450)
    assert EDIT.connect_material_expressions(color, "", hue, "A"), "colour"
    assert EDIT.connect_material_expressions(folding, "", hue, "B"), "folding"
    # Age, from 0 at birth to 1 at death, and youth, what is left of its life.
    relative = expression(material, unreal.MaterialExpressionParticleRelativeTime, -2100, 500)
    age = expression(material, unreal.MaterialExpressionSaturate, -1950, 500)
    assert EDIT.connect_material_expressions(relative, "", age, ""), "relative time"
    youth = expression(material, unreal.MaterialExpressionOneMinus, -1800, 350)
    assert EDIT.connect_material_expressions(age, "", youth, ""), "age"
    # The dome: 1 along the shape's heart, falling to 0 at its edge (1 - the square of the distance from the heart in
    # half-widths), so overlapping particles merge into rounded masses rather than points.
    uv = expression(material, unreal.MaterialExpressionTextureCoordinate, -2100, 150)
    if sprite:
        offset = expression(material, unreal.MaterialExpressionSubtract, -1950, 150, const_b=0.5)
        assert EDIT.connect_material_expressions(uv, "", offset, "A"), "uv"
    else:
        width = expression(material, unreal.MaterialExpressionComponentMask, -1950, 150, r=False, g=True, b=False, a=False)
        assert EDIT.connect_material_expressions(uv, "", width, ""), "uv"
        offset = expression(material, unreal.MaterialExpressionSubtract, -1850, 150, const_b=0.5)
        assert EDIT.connect_material_expressions(width, "", offset, "A"), "across"
    across = expression(material, unreal.MaterialExpressionMultiply, -1700, 150, const_b=2.0)
    assert EDIT.connect_material_expressions(offset, "", across, "A"), "offset"
    squared = expression(material, unreal.MaterialExpressionDotProduct if sprite else unreal.MaterialExpressionMultiply, -1550, 180)
    assert EDIT.connect_material_expressions(across, "", squared, "A"), "across"
    assert EDIT.connect_material_expressions(across, "", squared, "B"), "across again"
    dome = expression(material, unreal.MaterialExpressionOneMinus, -1400, 180)
    assert EDIT.connect_material_expressions(squared, "", dome, ""), "squared"
    domed = expression(material, unreal.MaterialExpressionSaturate, -1250, 260)
    assert EDIT.connect_material_expressions(dome, "", domed, ""), "dome"
    if sprite:
        # A ball's normal: the offset across the sprite, and up out of it by what the dome leaves.
        rise = expression(material, unreal.MaterialExpressionSquareRoot, -1100, 260)
        assert EDIT.connect_material_expressions(domed, "", rise, ""), "domed"
        normal = expression(material, unreal.MaterialExpressionAppendVector, -950, 200)
        assert EDIT.connect_material_expressions(across, "", normal, "A"), "across"
        assert EDIT.connect_material_expressions(rise, "", normal, "B"), "rise"
        assert EDIT.connect_material_property(normal, "", unreal.MaterialProperty.MP_NORMAL), "normal"
    # Kept where the dome stands above world-space noise (0 to 1) and the erosion of age together: particles side by
    # side share the noise, so holes run through a burst as one, and widen until the particle frays away.
    noise = expression(material, unreal.MaterialExpressionNoise, -1550, 450, scale=spec["noiseScale"], levels=2,
                       output_min=0.0, output_max=1.0, turbulence=False)
    ragged = expression(material, unreal.MaterialExpressionMultiply, -1400, 450, const_b=spec["ragged"])
    assert EDIT.connect_material_expressions(noise, "", ragged, "A"), "noise"
    eroded = expression(material, unreal.MaterialExpressionMultiply, -1400, 600, const_b=spec["erosion"])
    assert EDIT.connect_material_expressions(age, "", eroded, "A"), "age"
    roughened = expression(material, unreal.MaterialExpressionSubtract, -1100, 450)
    assert EDIT.connect_material_expressions(dome, "", roughened, "A"), "dome"
    assert EDIT.connect_material_expressions(ragged, "", roughened, "B"), "ragged"
    mask = expression(material, unreal.MaterialExpressionSubtract, -950, 500)
    assert EDIT.connect_material_expressions(roughened, "", mask, "A"), "roughened"
    assert EDIT.connect_material_expressions(eroded, "", mask, "B"), "eroded"
    assert EDIT.connect_material_property(mask, "", unreal.MaterialProperty.MP_OPACITY_MASK), "opacity mask"
    # Albedo: the hue at albedo, darkening to tailShade of that by death; matte, so the sun shades it as a solid.
    shade = expression(material, unreal.MaterialExpressionLinearInterpolate, -1100, -150, const_a=1.0, const_b=spec["tailShade"])
    assert EDIT.connect_material_expressions(age, "", shade, "Alpha"), "age"
    tinted = expression(material, unreal.MaterialExpressionMultiply, -1100, -350, const_b=spec["albedo"])
    assert EDIT.connect_material_expressions(hue, "", tinted, "A"), "hue"
    base = expression(material, unreal.MaterialExpressionMultiply, -900, -300)
    assert EDIT.connect_material_expressions(tinted, "", base, "A"), "tinted"
    assert EDIT.connect_material_expressions(shade, "", base, "B"), "shade"
    assert EDIT.connect_material_property(base, "", unreal.MaterialProperty.MP_BASE_COLOR), "base colour"
    rough = expression(material, unreal.MaterialExpressionConstant, -900, -200, r=1.0)
    assert EDIT.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS), "roughness"
    # The flare: youth to the power glowFalloff, so it is brightest at birth and spent well before death.
    flare = expression(material, unreal.MaterialExpressionPower, -1600, -50, const_exponent=spec["glowFalloff"])
    assert EDIT.connect_material_expressions(youth, "", flare, "Base"), "youth"
    # Glow: glowFloor + glowGain x the flare, its colour whitened by coreWhiten x the flare; brightest at the shape's
    # heart, so a newborn reads as a white-hot core inside a rim of its colour.
    gained = expression(material, unreal.MaterialExpressionMultiply, -1450, -50, const_b=spec["glowGain"])
    assert EDIT.connect_material_expressions(flare, "", gained, "A"), "flare"
    glow = expression(material, unreal.MaterialExpressionAdd, -1300, -50, const_b=spec["glowFloor"])
    assert EDIT.connect_material_expressions(gained, "", glow, "A"), "gained"
    heart = expression(material, unreal.MaterialExpressionMultiply, -1100, 0)
    assert EDIT.connect_material_expressions(glow, "", heart, "A"), "glow"
    assert EDIT.connect_material_expressions(domed, "", heart, "B"), "domed"
    glowing = unexposed(material, heart, -800, 0)
    heat = expression(material, unreal.MaterialExpressionMultiply, -1450, 50, const_b=spec["coreWhiten"])
    assert EDIT.connect_material_expressions(flare, "", heat, "A"), "flare"
    whitened = expression(material, unreal.MaterialExpressionLinearInterpolate, -900, -100, const_b=1.0)
    assert EDIT.connect_material_expressions(hue, "", whitened, "A"), "hue"
    assert EDIT.connect_material_expressions(heat, "", whitened, "Alpha"), "heat"
    emissive = expression(material, unreal.MaterialExpressionMultiply, -600, -100)
    assert EDIT.connect_material_expressions(whitened, "", emissive, "A"), "whitened"
    assert EDIT.connect_material_expressions(glowing, "", emissive, "B"), "glowing"
    assert EDIT.connect_material_property(emissive, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR), "emissive"


GRAPHIC_SHAPE_HLSL = {
    # A hard disc that shrinks. Each shape gives how far inside its edge this pixel lies (Inside) and inside its core's
    # (InCore), in the shape's own measure: positive within, negative beyond.
    "flare": """
float R = length(P);
float Size = 1.0 - Shrink.r * Age;
float Inside = Size - R;
float InCore = Size * CoreShare.r - R;
""",
    # A band that bursts out from Start to the sprite's edge, fast then slowing, and thins from Thickness to nothing.
    "ring": """
float R = length(P);
float Out = 1.0 - (1.0 - Age) * (1.0 - Age);
float Outer = lerp(Start.r, 1.0, Out);
float Width = Thickness.r * (1.0 - Age);
float Inner = Outer - Width;
float Inside = min(Outer - R, R - Inner);
float InCore = min(R - Inner, Inner + Width * CoreShare.r - R);
""",
    # Four points, the sharper the lower their exponent (Points; an astroid at 2/3), shrinking: a star scaled by k holds
    # |x|^e + |y|^e <= k^e.
    "star": """
float2 A = abs(P);
float S = pow(A.x, Points.r) + pow(A.y, Points.r);
float Size = pow(max(1.0 - Shrink.r * Age, 0.0), Points.r);
float Inside = Size - S;
float InCore = Size * pow(CoreShare.r, Points.r) - S;
""",
    # A diamond filling the sprite, long along its long side (a sprite drawn along its velocity), shrinking.
    "streak": """
float2 A = abs(P);
float D = A.x + A.y;
float Size = 1.0 - Shrink.r * Age;
float Inside = Size - D;
float InCore = Size * CoreShare.r - D;
""",
}

GRAPHIC_HLSL = """
// A combat effect's graphic shape (ADR-068, section 4), drawn in its sprite from the particle's normalized age: how much
// of this pixel it covers (alpha), and its colour, white-hot in its core while young and cooling to its own colour.
float2 P = UV * 2.0 - 1.0;
float Age = saturate(Life);
{shape}
// Each edge is a pixel's width wherever the sprite stands, so the shape stays crisp and smooth at any size: how far
// inside the edge a pixel lies, in pixels, from how fast the measure changes across the screen.
float Edge = saturate(Inside / max(fwidth(Inside), 1e-5) + 0.5);
float Core = saturate(InCore / max(fwidth(InCore), 1e-5) + 0.5);
// The particle's colour folded under 1 by its brightest channel, so a template that brightens colour keeps its hue.
float3 Hue = Color.rgb / max(max(max(Color.r, Color.g), Color.b), 1.0);
float3 Hot = lerp(Hue, float3(1.0, 1.0, 1.0), Core * CoreWhiten.r * (1.0 - Age));
return float4(Hot * GlowGain.r * lerp(1.0, TailGlow.r, Age), Edge);
"""


def build_graphic_shape(material, spec):
    """A combat effect's graphic shape (ADR-068 §4): a hard-edged flare, ring, star or streak drawn in its sprite, unlit,
    with a white-hot core that cools to the particle's colour and a glow that falls from glowGain to tailGlow of it as
    it ages; its glow reads the same under any exposure. Its shape grows or shrinks with the particle's normalized age,
    which every Niagara renderer passes.

    It is translucent, its edge anti-aliased in the shader a pixel wide: masked, two such sprites in one place (a flare
    over a ring) fought in depth, and one's whole square showed over the other."""
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    material.set_editor_property("used_with_niagara_sprites", True)
    inputs = [("UV", expression(material, unreal.MaterialExpressionTextureCoordinate, -1400, -400), ""),
              ("Life", expression(material, unreal.MaterialExpressionParticleRelativeTime, -1400, -250), ""),
              ("Color", expression(material, unreal.MaterialExpressionParticleColor, -1400, -100), "")]
    y = 50
    for key in ("coreShare", "coreWhiten", "glowGain", "tailGlow") + GRAPHIC_SHAPES[spec["shape"]]:
        # The star's exponent goes in as Points: a custom input named Exponent does not take a connection.
        name = "Points" if key == "exponent" else key[0].upper() + key[1:]
        inputs.append((name, expression(material, unreal.MaterialExpressionConstant, -1400, y, r=float(spec[key])), ""))
        y += 100
    custom = expression(material, unreal.MaterialExpressionCustom, -900, 0, code=GRAPHIC_HLSL.replace("{shape}", GRAPHIC_SHAPE_HLSL[spec["shape"]]),
                        description="VeyraGraphicShape", output_type=unreal.CustomMaterialOutputType.CMOT_FLOAT4,
                        inputs=[custom_input(name) for name, _, _ in inputs])
    for name, node, output in inputs:
        assert EDIT.connect_material_expressions(node, output, custom, name), "shape input " + name
    colour = expression(material, unreal.MaterialExpressionComponentMask, -700, -100, r=True, g=True, b=True, a=False)
    assert EDIT.connect_material_expressions(custom, "", colour, ""), "shape colour"
    emissive = unexposed(material, colour, -450, -100)
    assert EDIT.connect_material_property(emissive, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR), "emissive"
    covered = expression(material, unreal.MaterialExpressionComponentMask, -700, 100, r=False, g=False, b=False, a=True)
    assert EDIT.connect_material_expressions(custom, "", covered, ""), "shape coverage"
    assert EDIT.connect_material_property(covered, "", unreal.MaterialProperty.MP_OPACITY), "opacity"

BUILDERS = {"overlayFlash": build_overlay_flash, "postProcessOutline": build_post_process_outline, "postProcessInk": build_post_process_ink,
            "particleSmoke": build_particle_smoke, "particleEffect": build_particle_effect, "graphicShape": build_graphic_shape}

# -VeyraOnly=A,B builds just those materials; without it, every one.
ONLY = next((token.split("=", 1)[1].split(",") for token in unreal.SystemLibrary.get_command_line().split() if token.startswith("-VeyraOnly=")), None)
SELECTED = [spec for spec in SPEC["materials"] if ONLY is None or spec["name"] in ONLY]
assert ONLY is None or len(SELECTED) == len(ONLY), "Unknown material in -VeyraOnly: " + ",".join(ONLY)

# The spec is valid (above); check that no target is locked against writing before changing any asset.
destination_disk = GAME / "Content" / DEST.removeprefix("/Game/")
assert set(BUILDERS) == set(RULES), "The spec checker and the builders know different kinds"
for spec in SELECTED:
    target = destination_disk / (spec["name"] + ".uasset")
    if target.exists() and getattr(target.stat(), "st_file_attributes", 0) & stat.FILE_ATTRIBUTE_READONLY:
        raise RuntimeError("Acquire the Git LFS lock before rebuilding: " + str(target))

RESULTS = []
for spec in SELECTED:
    path = DEST + "/" + spec["name"]
    material = unreal.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else None
    if not material:
        material = TOOLS.create_asset(spec["name"], DEST, unreal.Material, unreal.MaterialFactoryNew())
    assert isinstance(material, unreal.Material), "Not a material: " + path
    EDIT.delete_all_material_expressions(material)
    BUILDERS[spec["kind"]](material, spec)
    EDIT.recompile_material(material)
    assert unreal.EditorAssetLibrary.save_loaded_asset(material), "Material save failed: " + path
    RESULTS.append({"asset": material.get_path_name(), "kind": spec["kind"]})
    unreal.log("VEYRA_PRESENTATION_MATERIAL: " + material.get_path_name())

SAVED.mkdir(parents=True, exist_ok=True)
(SAVED / "build.json").write_text(json.dumps({
    "generatorVersion": GENERATOR_VERSION,
    "specSha256": hashlib.sha256(SPEC_FILE.read_bytes()).hexdigest(),
    "materials": RESULTS,
}, indent=2) + "\n")
unreal.log("VEYRA_PRESENTATION_MATERIALS_PASSED: " + str(len(RESULTS)) + " material(s)")
