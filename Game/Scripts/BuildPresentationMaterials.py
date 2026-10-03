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


BUILDERS = {"overlayFlash": build_overlay_flash, "postProcessOutline": build_post_process_outline}

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
