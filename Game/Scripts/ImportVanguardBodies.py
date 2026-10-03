"""Import the generated Vanguard bodies (ADR-064) into Unreal; BuildVanguardBodies.ps1 runs it in an editor commandlet.

Each FBX GenerateVanguardBodies.py wrote is checked against the manifest, then imported as SK_<Id> with its own
skeleton and animation sequences under /Game/Veyra/Vanguards/<Id>. Every body wears M_VeyraVanguardBody, which this
script builds: its colour is the vertex colour, and the vertex alpha marks what glows. DA_VanguardArt then holds every
body in the manifest by Vanguard ID. A validation report records each body's height, triangles and animations.
"""
import hashlib
import json
import stat
from pathlib import Path

import unreal

GAME = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
SOURCE = GAME / "ArtSource" / "Vanguards"
SAVED = GAME / "Saved" / "VanguardKit"
KIT_BYTES = (SOURCE / "VanguardKit.json").read_bytes()
KIT = json.loads(KIT_BYTES)
MANIFEST = json.loads((SOURCE / "manifest.json").read_text())
DEST = KIT["destination"]
MATERIAL_PATH = DEST + "/M_VeyraVanguardBody"
EDIT = unreal.MaterialEditingLibrary
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
ONLY = next((token.split("=", 1)[1].split(",") for token in unreal.SystemLibrary.get_command_line().split() if token.startswith("-VeyraOnly=")), None)

assert MANIFEST["kitSha256"] == hashlib.sha256(KIT_BYTES).hexdigest(), "Run GenerateVanguardBodies.py after changing the kit"
assert DEST.startswith("/Game/Veyra/"), "Vanguard bodies live under /Game/Veyra"
SELECTED = [asset for asset in MANIFEST["assets"] if ONLY is None or asset["id"] in ONLY]
assert ONLY is None or len(SELECTED) == len(ONLY), "Unknown Vanguard in -VeyraOnly"


def folder_of(asset):
    return DEST + "/" + asset["name"].removeprefix("SK_")


def sequence_prefix(asset):
    return "AS_" + asset["name"].removeprefix("SK_")


def sequence_name(asset, clip):
    return sequence_prefix(asset) + "_Armature_" + clip


def disk_path(asset_path):
    return GAME / "Content" / (asset_path.removeprefix("/Game/") + ".uasset")


def writable(asset_path):
    target = disk_path(asset_path)
    if target.exists() and getattr(target.stat(), "st_file_attributes", 0) & stat.FILE_ATTRIBUTE_READONLY:
        raise RuntimeError("Acquire the Git LFS lock before reimporting: " + str(target))


# Validate every source and target before changing any asset.
for asset in SELECTED:
    source = (SOURCE / asset["file"]).resolve()
    assert source.is_relative_to(SOURCE.resolve()), "An FBX path escapes the kit"
    assert hashlib.sha256(source.read_bytes()).hexdigest() == asset["sha256"], asset["name"] + " differs from the manifest"
    writable(folder_of(asset) + "/" + asset["name"])


def body_material():
    """The one material every generated body wears: vertex colour for colour, vertex alpha for glow."""
    material = unreal.load_asset(MATERIAL_PATH) if unreal.EditorAssetLibrary.does_asset_exist(MATERIAL_PATH) else None
    if material:
        return material
    material = TOOLS.create_asset("M_VeyraVanguardBody", DEST, unreal.Material, unreal.MaterialFactoryNew())
    color = EDIT.create_material_expression(material, unreal.MaterialExpressionVertexColor, -800, 0)
    EDIT.connect_material_property(color, "RGB", unreal.MaterialProperty.MP_BASE_COLOR)
    glow = EDIT.create_material_expression(material, unreal.MaterialExpressionScalarParameter, -800, 250)
    glow.set_editor_property("parameter_name", "GlowStrength")
    glow.set_editor_property("default_value", 6.0)
    masked = EDIT.create_material_expression(material, unreal.MaterialExpressionMultiply, -500, 150)
    EDIT.connect_material_expressions(color, "RGB", masked, "A")
    EDIT.connect_material_expressions(color, "A", masked, "B")
    emissive = EDIT.create_material_expression(material, unreal.MaterialExpressionMultiply, -250, 150)
    EDIT.connect_material_expressions(masked, "", emissive, "A")
    EDIT.connect_material_expressions(glow, "", emissive, "B")
    EDIT.connect_material_property(emissive, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    roughness = EDIT.create_material_expression(material, unreal.MaterialExpressionScalarParameter, -800, 450)
    roughness.set_editor_property("parameter_name", "Roughness")
    roughness.set_editor_property("default_value", 0.75)
    EDIT.connect_material_property(roughness, "", unreal.MaterialProperty.MP_ROUGHNESS)
    # Skinned meshes use it.
    material.set_editor_property("used_with_skeletal_mesh", True)
    EDIT.recompile_material(material)
    assert unreal.EditorAssetLibrary.save_loaded_asset(material), "Material save failed"
    return material


def import_body(asset, material):
    folder = folder_of(asset)
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", str(SOURCE / asset["file"]))
    task.set_editor_property("destination_path", folder)
    task.set_editor_property("destination_name", asset["name"])
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("replace_existing_settings", True)
    task.set_editor_property("save", False)
    task.set_editor_property("factory", unreal.FbxFactory())
    options = unreal.FbxImportUI()
    options.set_editor_property("automated_import_should_detect_type", False)
    options.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_SKELETAL_MESH)
    options.set_editor_property("import_mesh", True)
    options.set_editor_property("import_as_skeletal", True)
    options.set_editor_property("import_animations", True)
    options.set_editor_property("import_materials", False)
    options.set_editor_property("import_textures", False)
    options.set_editor_property("create_physics_asset", False)
    # Sequences are named AS_<Id>_Armature_<Clip>: Blender names each take after its armature and action, and a
    # reimport finds and replaces the same sequences by those names.
    options.set_editor_property("override_animation_name", sequence_prefix(asset))
    mesh_data = options.skeletal_mesh_import_data
    mesh_data.set_editor_property("import_morph_targets", False)
    mesh_data.set_editor_property("convert_scene", True)
    mesh_data.set_editor_property("convert_scene_unit", True)
    mesh_data.set_editor_property("import_uniform_scale", 1.0)
    mesh_data.set_editor_property("vertex_color_import_option", unreal.VertexColorImportOption.REPLACE)
    mesh_data.set_editor_property("normal_import_method", unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS)
    anim_data = options.anim_sequence_import_data
    anim_data.set_editor_property("convert_scene", True)
    anim_data.set_editor_property("convert_scene_unit", True)
    anim_data.set_editor_property("import_uniform_scale", 1.0)
    anim_data.set_editor_property("animation_length", unreal.FBXAnimationLengthImportType.FBXALIT_EXPORTED_TIME)
    task.set_editor_property("options", options)
    TOOLS.import_asset_tasks([task])
    mesh = unreal.load_asset(folder + "/" + asset["name"])
    assert isinstance(mesh, unreal.SkeletalMesh), asset["name"] + " did not import as a skeletal mesh"
    materials = mesh.get_editor_property("materials")
    for slot in materials:
        slot.set_editor_property("material_interface", material)
    mesh.set_editor_property("materials", materials)
    assert unreal.EditorAssetLibrary.save_loaded_asset(mesh), "Save failed: " + asset["name"]
    skeleton = mesh.get_editor_property("skeleton")
    assert skeleton, asset["name"] + " has no skeleton"
    unreal.EditorAssetLibrary.save_loaded_asset(skeleton)
    # Its animations land beside it, one sequence per generated action.
    animations = {}
    for clip in asset["animations"]:
        path = folder + "/" + sequence_name(asset, clip["name"])
        sequence = unreal.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else None
        assert isinstance(sequence, unreal.AnimSequence), (asset["name"], "animation did not import", path)
        assert sequence.get_editor_property("skeleton") == skeleton, (path, "is on another skeleton")
        assert unreal.EditorAssetLibrary.save_loaded_asset(sequence), "Save failed: " + path
        animations[clip["name"]] = {"asset": sequence.get_path_name(), "seconds": round(sequence.get_play_length(), 3)}
    bounds = mesh.get_bounds()
    height = 2 * bounds.box_extent.z
    assert abs(height - asset["heightCm"]) < max(2.0, asset["heightCm"] * 0.02), (asset["name"], "height changed on import", height, asset["heightCm"])
    return {"asset": mesh.get_path_name(), "skeleton": skeleton.get_path_name(), "heightCm": round(height, 2),
            "capsuleHalfHeightCm": asset["capsuleHalfHeightCm"], "triangles": asset["triangles"], "animations": animations}


def write_art_set():
    """DA_VanguardArt: every body in the manifest by Vanguard ID, with its animations by name (ADR-064 §3)."""
    path = DEST + "/DA_VanguardArt"
    writable(path)
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        art_set = unreal.load_asset(path)
    else:
        factory = unreal.DataAssetFactory()
        factory.set_editor_property("data_asset_class", unreal.VeyraVanguardArtSet)
        art_set = TOOLS.create_asset("DA_VanguardArt", DEST, unreal.VeyraVanguardArtSet, factory)
    assert isinstance(art_set, unreal.VeyraVanguardArtSet), path
    entries = {}
    for asset in sorted(MANIFEST["assets"], key=lambda entry: entry["id"]):
        mesh = unreal.load_asset(folder_of(asset) + "/" + asset["name"])
        assert isinstance(mesh, unreal.SkeletalMesh), asset["name"] + " is not imported"
        body = unreal.VeyraVanguardArt()
        body.set_editor_property("mesh", mesh)
        body.set_editor_property("animations", {unreal.Name(clip["name"]): unreal.load_asset(folder_of(asset) + "/" + sequence_name(asset, clip["name"]))
                                                for clip in asset["animations"]})
        body.set_editor_property("run_stride", asset["runStrideCm"])
        body.set_editor_property("cast_release_share", asset["castReleaseShare"])
        body.set_editor_property("upper_body_bone", unreal.Name(asset["upperBodyBone"]))
        entries[unreal.Name(asset["id"])] = body
    art_set.set_editor_property("art", entries)
    assert unreal.EditorAssetLibrary.save_loaded_asset(art_set, only_if_is_dirty=False), "Save failed: " + path
    unreal.log("VEYRA_VANGUARD_ART_SET: " + path + " dresses " + ", ".join(asset["id"] for asset in MANIFEST["assets"]))


material = body_material()
results = [import_body(asset, material) for asset in SELECTED]
write_art_set()
SAVED.mkdir(parents=True, exist_ok=True)
(SAVED / "unreal-validation.json").write_text(json.dumps({"status": "passed", "bodies": results}, indent=2) + "\n")
for result in results:
    unreal.log("VEYRA_VANGUARD_BODY_ASSET: " + result["asset"] + " (" + str(len(result["animations"])) + " animations)")
unreal.log("VEYRA_VANGUARD_BODIES_IMPORTED: " + str(len(results)))
