"""Editor-only import of the Crucible's terrain textures and its Landscape and river materials (ADR-040 §3, §6).

Invoked by BuildTerrainArt.ps1 in an editor commandlet with the PythonScriptPlugin. Imports the generated textures
of Game/ArtSource/Environment/Terrain and builds M_CrucibleTerrain (crucible_terrain_material.py), M_CrucibleWater
(crucible_water_material.py) and the river's ripple step, M_CrucibleWaterStir (crucible_water_stir_material.py, ADR-076),
from them. Every number comes from TerrainTextures.json, the water's settings in DefaultGame.ini (which name the ripple
parameters) or the materials' parameters, which the generated map's style profile may override. A texture or material whose inputs are unchanged since it was last
written is kept as it is, unwritten (TerrainArt/inputs.py, KitMaterials/inputs.py). Does not edit a map.
"""
import hashlib
import json
from pathlib import Path

import sys

import unreal

SCRIPTS = Path(__file__).resolve().parent
sys.path.insert(0, str(SCRIPTS))
import crucible_terrain_material  # noqa: E402
import crucible_water_material  # noqa: E402
import crucible_water_stir_material  # noqa: E402
from KitMaterials.inputs import INPUTS_TAG, material_inputs_hash  # noqa: E402
from KitMaterials.inputs import kept as material_kept  # noqa: E402
from TerrainArt.inputs import (SOURCE_TAG, kept, material_bindings, stir_material_values, terrain_material_values,  # noqa: E402
                               texture_settings, water_material_values, water_settings)

GAME = SCRIPTS.parent
SOURCE = GAME / "ArtSource" / "Environment" / "Terrain"
SAVED = GAME / "Saved" / "TerrainArt"
DEST = "/Game/Veyra/World/Environment/Terrain"
PROFILE_PATH = SOURCE / "TerrainTextures.json"
PROFILE = json.loads(PROFILE_PATH.read_text(encoding="utf-8"))
MANIFEST = json.loads((SOURCE / "manifest.json").read_text(encoding="utf-8"))
assert MANIFEST["profileSha256"] == hashlib.sha256(PROFILE_PATH.read_bytes()).hexdigest(), "Regenerate the textures after profile changes."
WATER_SETTINGS = water_settings((GAME / "Config" / "DefaultGame.ini").read_text(encoding="utf-8"))

TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
EDIT = unreal.MaterialEditingLibrary
ASSETS = unreal.EditorAssetLibrary
# The enum each texture setting names its value in (TerrainArt/inputs.py texture_settings); srgb is a plain bool.
SETTING_ENUMS = {"compression_settings": unreal.TextureCompressionSettings, "lod_group": unreal.TextureGroup}


def import_texture(entry):
    """The texture entry names, imported from its PNG and given its settings, unless the asset already is that import:
    the source hash it records is the PNG's and it has those settings (TerrainArt/inputs.py). Then it is kept as it is,
    neither imported nor saved, since a reimport of the same PNG would rewrite the asset for no change. Returns the
    texture, and whether this run imported it."""
    filename = SOURCE / entry["name"]
    assert hashlib.sha256(filename.read_bytes()).hexdigest() == entry["sha256"], f"{entry['name']} differs from the manifest"
    name = filename.stem
    path = f"{DEST}/Textures/{name}"
    wanted = {key: getattr(SETTING_ENUMS[key], value) if key in SETTING_ENUMS else value for key, value in texture_settings(name).items()}
    texture = unreal.load_asset(path) if ASSETS.does_asset_exist(path) else None
    if texture and kept(ASSETS.get_metadata_tag(texture, SOURCE_TAG), entry["sha256"],
                        {key: texture.get_editor_property(key) for key in wanted}, wanted):
        unreal.log("VEYRA_TERRAIN_TEXTURE_UNCHANGED: " + path)
        return texture, False
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", str(filename))
    task.set_editor_property("destination_path", DEST + "/Textures")
    task.set_editor_property("destination_name", name)
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("save", False)
    TOOLS.import_asset_tasks([task])
    texture = unreal.load_asset(path)
    assert texture, f"{name} did not import"
    for key, value in wanted.items():
        texture.set_editor_property(key, value)
    ASSETS.set_metadata_tag(texture, SOURCE_TAG, entry["sha256"])
    assert ASSETS.save_loaded_asset(texture, only_if_is_dirty=False), f"Save failed (does it need its Git LFS lock?): {path}"
    unreal.log("VEYRA_TERRAIN_TEXTURE_IMPORTED: " + path)
    return texture, True


def build_material(graph, name, textures, values):
    """The material graph builds (crucible_terrain_material, crucible_water_material or crucible_water_stir_material),
    as DEST/name from values and
    the textures it samples. It is kept as it is while what it is built from is unchanged (KitMaterials/inputs.py): its
    values, the code that builds its graph, and the textures it links to with the settings its samplers rely on. That
    hash is recorded on the asset, since a rebuild makes the same graph with new expression IDs, a changed file for no
    change. Otherwise its graph is rebuilt whole and must compile. Returns it, and whether this run rebuilt it."""
    path = f"{DEST}/{name}"
    bound = {texture: textures[texture] for texture in graph.TEXTURES}
    bindings = material_bindings({texture: asset.get_path_name() for texture, asset in bound.items()})
    inputs = material_inputs_hash(values, [Path(graph.__file__), SCRIPTS / "veyra_material_graph.py"], bindings)
    material = unreal.load_asset(path) if ASSETS.does_asset_exist(path) else None
    if material and material_kept(ASSETS.get_metadata_tag(material, INPUTS_TAG), inputs):
        unreal.log("VEYRA_TERRAIN_MATERIAL_UNCHANGED: " + path)
        return material, False
    if material:
        EDIT.delete_all_material_expressions(material)
        # Custom output nodes can outlive the bulk delete: a material may hold only one water output.
        for expression in EDIT.get_material_expressions(material):
            EDIT.delete_material_expression(material, expression)
    else:
        material = TOOLS.create_asset(name, DEST, unreal.Material, unreal.MaterialFactoryNew())
    graph.build(material, bound, values)
    errors = EDIT.recompile_material(material)
    assert not errors, f"{material.get_name()} does not compile: {errors}"
    ASSETS.set_metadata_tag(material, INPUTS_TAG, inputs)
    assert ASSETS.save_loaded_asset(material, only_if_is_dirty=False), f"Save failed (does it need its Git LFS lock?): {path}"
    unreal.log("VEYRA_TERRAIN_MATERIAL_REBUILT: " + path)
    return material, True


def main():
    SAVED.mkdir(parents=True, exist_ok=True)
    water_only = "-veyrawateronly" in unreal.SystemLibrary.get_command_line().lower().split()
    textures = {}
    imported = []
    for entry in MANIFEST["files"]:
        if water_only and entry["layer"] != "Water":
            continue
        texture, fresh = import_texture(entry)
        textures[Path(entry["name"]).stem] = texture
        if fresh:
            imported.append(texture.get_path_name())
    rebuilt = []
    material = None
    if not water_only:
        material, fresh = build_material(crucible_terrain_material, "M_CrucibleTerrain", textures, terrain_material_values(PROFILE))
        if fresh:
            rebuilt.append(material.get_path_name())
    water, fresh = build_material(crucible_water_material, "M_CrucibleWater", textures, water_material_values(PROFILE, WATER_SETTINGS))
    if fresh:
        rebuilt.append(water.get_path_name())
    stir, fresh = build_material(crucible_water_stir_material, "M_CrucibleWaterStir", textures, stir_material_values(WATER_SETTINGS))
    if fresh:
        rebuilt.append(stir.get_path_name())
    report = {"material": material.get_path_name() if material else None, "textures": sorted(t.get_path_name() for t in textures.values()),
              "importedTextures": sorted(imported), "water": water.get_path_name(), "stir": stir.get_path_name(), "rebuiltMaterials": sorted(rebuilt),
              "layers": [] if water_only else [name for name, _ in crucible_terrain_material.LAYERS]}
    (SAVED / "import.json").write_text(json.dumps(report, indent=2), encoding="utf-8")
    unreal.log(f"Imported {len(imported)} of {len(textures)} textures and rebuilt {len(rebuilt)} materials (the rest unchanged); "
               f"built {'water only' if water_only else 'terrain and water'}.")


main()
