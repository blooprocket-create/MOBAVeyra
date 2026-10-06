"""Build the presentation's generated materials from ArtSource/Presentation/PresentationMaterials.json (ADR-063).

Run through BuildPresentationMaterials.ps1, in an editor commandlet. Each material's graph comes from its kind
below and its values from the spec, so the same spec and generator version always build the same asset.
"""
import hashlib
import json
import stat
from pathlib import Path

import unreal

GAME = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
SPEC_FILE = GAME / "ArtSource" / "Presentation" / "PresentationMaterials.json"
SPEC = json.loads(SPEC_FILE.read_text())
SAVED = GAME / "Saved" / "PresentationMaterials"
GENERATOR_VERSION = 1
EDIT = unreal.MaterialEditingLibrary
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()

assert SPEC["schemaVersion"] == 1, "Unknown spec schema"
assert SPEC["generatorVersion"] == GENERATOR_VERSION, "The spec was written for another generator version"
DEST = SPEC["destination"]
assert DEST.startswith("/Game/Veyra/UI/"), "Presentation materials live where the UI's content is always cooked"


def expression(material, kind, x, y, **properties):
    node = EDIT.create_material_expression(material, kind, x, y)
    for key, value in properties.items():
        node.set_editor_property(key, value)
    return node


def build_overlay_flash(material, spec):
    """Emissive = colour x strength, brightest at the silhouette: additive and unlit, for a mesh's overlay."""
    assert 0.0 <= spec["rimFloor"] <= 1.0 and spec["rimExponent"] > 0.0
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_ADDITIVE)
    material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    # It overlays the generated bodies (skeletal meshes) as well as the grey-box shapes: a cooked game has only the
    # shaders a material's saved usages ask for.
    material.set_editor_property("used_with_skeletal_mesh", True)
    color = expression(material, unreal.MaterialExpressionVectorParameter, -900, -200,
                       parameter_name=spec["colorParameter"], default_value=unreal.LinearColor(*spec["defaultColor"]))
    strength = expression(material, unreal.MaterialExpressionScalarParameter, -900, 0,
                          parameter_name=spec["strengthParameter"], default_value=0.0)
    fresnel = expression(material, unreal.MaterialExpressionFresnel, -900, 200, exponent=spec["rimExponent"], base_reflect_fraction=0.0)
    floor = expression(material, unreal.MaterialExpressionConstant, -900, 380, r=spec["rimFloor"])
    full = expression(material, unreal.MaterialExpressionConstant, -900, 460, r=1.0)
    rim = expression(material, unreal.MaterialExpressionLinearInterpolate, -600, 260)
    EDIT.connect_material_expressions(floor, "", rim, "A")
    EDIT.connect_material_expressions(full, "", rim, "B")
    EDIT.connect_material_expressions(fresnel, "", rim, "Alpha")
    lit = expression(material, unreal.MaterialExpressionMultiply, -600, -100)
    EDIT.connect_material_expressions(color, "RGB", lit, "A")
    EDIT.connect_material_expressions(strength, "", lit, "B")
    emissive = expression(material, unreal.MaterialExpressionMultiply, -300, 0)
    EDIT.connect_material_expressions(lit, "", emissive, "A")
    EDIT.connect_material_expressions(rim, "", emissive, "B")
    EDIT.connect_material_property(emissive, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)


OUTLINE_HLSL = """
// The hovered unit's outline (ADR-063, section 3): a pixel outside every stencilled shape, near one, takes that shape's colour.
// An enemy's reach wins over an ally's or a neutral's; reaches grow with the view's height past ReferenceHeight.
float3 Base = Scene.rgb;
if (Stencil.r > 0.5)
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
        else if (Kind > 0.5 && Found == 0.0 && Distance <= OtherReach)
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
    assert len(set(stencils.values())) == 3 and all(1 <= value <= 255 for value in stencils.values()), "Three distinct stencils, 1-255"
    assert spec["thicknessPixels"]["enemy"] >= spec["thicknessPixels"]["other"] > 0.0 and spec["referenceHeight"] > 0.0
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


def build_particle_smoke(material, spec):
    """A particle's puff of stylized smoke: a solid, hard-edged blob (masked) whose edge is ragged by noise and which
    erodes through growing holes as the particle's alpha fades with age. It is lit as a ball (a normal domed from the
    sprite's centre), so the sun shades it like any solid in the scene; its albedo is the particle's colour darkened,
    and it glows in that colour while young, by an amount that does not depend on the scene's exposure."""
    for key in ("ragged", "erosion", "glowGain", "noiseScale", "albedo"):
        assert spec[key] > 0.0, key
    assert spec["glowFloor"] >= 0.0 and 0.0 < spec["clip"] < 1.0 and spec["albedo"] <= 1.0
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
    exposure = expression(material, unreal.MaterialExpressionEyeAdaptationInverse, -800, -50)
    unexposed = expression(material, unreal.MaterialExpressionMultiply, -600, -100)
    assert EDIT.connect_material_expressions(glow, "", unexposed, "A"), "glow"
    assert EDIT.connect_material_expressions(exposure, "", unexposed, "B"), "exposure"
    # Brightest at the puff's heart, so the glow reads as embers inside the smoke rather than its surface.
    heart = expression(material, unreal.MaterialExpressionMultiply, -500, -100)
    assert EDIT.connect_material_expressions(unexposed, "", heart, "A"), "unexposed"
    assert EDIT.connect_material_expressions(domed, "", heart, "B"), "domed"
    emissive = expression(material, unreal.MaterialExpressionMultiply, -400, -200)
    assert EDIT.connect_material_expressions(color, "", emissive, "A"), "colour"
    assert EDIT.connect_material_expressions(heart, "", emissive, "B"), "heart"
    assert EDIT.connect_material_property(emissive, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR), "emissive"


BUILDERS = {"overlayFlash": build_overlay_flash, "postProcessOutline": build_post_process_outline, "particleSmoke": build_particle_smoke}

# -VeyraOnly=A,B builds just those materials; without it, every one.
ONLY = next((token.split("=", 1)[1].split(",") for token in unreal.SystemLibrary.get_command_line().split() if token.startswith("-VeyraOnly=")), None)
SELECTED = [spec for spec in SPEC["materials"] if ONLY is None or spec["name"] in ONLY]
assert ONLY is None or len(SELECTED) == len(ONLY), "Unknown material in -VeyraOnly: " + ",".join(ONLY)

# Validate the whole spec, and that no target is locked against writing, before changing any asset.
destination_disk = GAME / "Content" / DEST.removeprefix("/Game/")
for spec in SPEC["materials"]:
    assert spec["kind"] in BUILDERS, "Unknown material kind: " + spec["kind"]
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
