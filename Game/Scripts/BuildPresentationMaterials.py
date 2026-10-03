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


BUILDERS = {"overlayFlash": build_overlay_flash}

# Validate the whole spec, and that no target is locked against writing, before changing any asset.
destination_disk = GAME / "Content" / DEST.removeprefix("/Game/")
for spec in SPEC["materials"]:
    assert spec["kind"] in BUILDERS, "Unknown material kind: " + spec["kind"]
    target = destination_disk / (spec["name"] + ".uasset")
    if target.exists() and getattr(target.stat(), "st_file_attributes", 0) & stat.FILE_ATTRIBUTE_READONLY:
        raise RuntimeError("Acquire the Git LFS lock before rebuilding: " + str(target))

RESULTS = []
for spec in SPEC["materials"]:
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
