"""Editor-only import and validation for the provisional Crucible structure kit.

Invoked by BuildStructureArt.ps1 with PythonScriptPlugin enabled for that process.
Does not edit a map or install a runtime plugin. With -VeyraMaterials=A,B it rebuilds
only the materials named, from the kit, and imports no mesh.
"""
import hashlib
import json
import sys
from pathlib import Path

import unreal

sys.path.insert(0, str(Path(__file__).resolve().parent))
from KitMaterials.spec import surface_problems  # noqa: E402
from veyra_material_graph import kit_material, materials_named, refuse_locked  # noqa: E402


GAME = Path(__file__).resolve().parents[1]
SOURCE = GAME / "ArtSource" / "Structures"
SAVED = GAME / "Saved" / "StructureKit"
DEST = "/Game/Veyra/World/Structures/Greybox"
KIT = json.loads((SOURCE / "StructureKit.json").read_text())
MANIFEST = json.loads((SOURCE / "manifest.json").read_text())
assert MANIFEST["worldSha256"] == hashlib.sha256((GAME / "Tuning" / "World.json").read_bytes()).hexdigest(), "Regenerate after World.json changes."
PROBLEMS = surface_problems(KIT)
assert not PROBLEMS, "StructureKit.json: " + "; ".join(PROBLEMS)
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
# Commandlets do not initialize the interactive editor's subsystem collection.
# These asset-only helpers have no per-instance state.
EDITOR = unreal.get_default_object(unreal.StaticMeshEditorSubsystem)
# Materials named on the command line are rebuilt alone, and no mesh is imported; otherwise everything is.
NAMED = materials_named([spec["name"] for spec in KIT["materials"]])
SELECTED = [spec for spec in KIT["materials"] if NAMED is None or spec["name"] in NAMED]
refuse_locked(GAME, [DEST + "/Materials/" + spec["name"] for spec in SELECTED]
              + ([] if NAMED is not None else [DEST + "/Meshes/" + spec["name"] for spec in MANIFEST["assets"]]))
MATERIALS = []


def create_material(spec):
    path = DEST + "/Materials/" + spec["name"]
    material = unreal.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else None
    if not material:
        material = TOOLS.create_asset(spec["name"], DEST + "/Materials", unreal.Material, unreal.MaterialFactoryNew())
    unreal.MaterialEditingLibrary.delete_all_material_expressions(material)
    kit_material(material, spec)
    unreal.MaterialEditingLibrary.recompile_material(material)
    assert unreal.EditorAssetLibrary.save_loaded_asset(material), "Material save failed: " + path
    unreal.log("VEYRA_STRUCTURE_MATERIAL: " + material.get_path_name())
    return material


for spec in SELECTED:
    MATERIALS.append(create_material(spec))

RESULTS = []
for spec in MANIFEST["assets"] if NAMED is None else []:
    filename = SOURCE / spec["file"]
    assert hashlib.sha256(filename.read_bytes()).hexdigest() == spec["sha256"], "FBX differs from manifest"
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", str(filename))
    task.set_editor_property("destination_path", DEST + "/Meshes")
    task.set_editor_property("destination_name", spec["name"])
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("save", False)
    task.set_editor_property("factory", unreal.FbxFactory())
    options = unreal.FbxImportUI()
    options.set_editor_property("automated_import_should_detect_type", False)
    options.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_STATIC_MESH)
    options.set_editor_property("import_mesh", True)
    options.set_editor_property("import_materials", False)
    options.set_editor_property("import_textures", False)
    options.set_editor_property("import_as_skeletal", False)
    data = options.static_mesh_import_data
    data.set_editor_property("combine_meshes", True)
    data.set_editor_property("auto_generate_collision", False)
    data.set_editor_property("generate_lightmap_u_vs", False)
    data.set_editor_property("convert_scene", True)
    data.set_editor_property("convert_scene_unit", True)
    data.set_editor_property("normal_import_method", unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS)
    task.set_editor_property("options", options)
    TOOLS.import_asset_tasks([task])
    asset = unreal.load_asset(DEST + "/Meshes/" + spec["name"])
    assert isinstance(asset, unreal.StaticMesh), spec["name"] + " did not import"
    # Slot names, not importer ordering, keep Flux separate from the physical shell.
    for i, slot in enumerate(asset.get_editor_property("static_materials")):
        name = str(slot.get_editor_property("material_slot_name"))
        match = next((mat for mat in MATERIALS if mat.get_name() == name), None)
        assert match, "Unrecognized material slot: " + name
        asset.set_material(i, match)
    # No simple shapes were imported. Complex geometry must not become an alternate
    # collision owner when an artist drags a preview mesh into a map.
    body = asset.get_editor_property("body_setup")
    body.set_editor_property("collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_SIMPLE_AS_COMPLEX)
    instance = body.get_editor_property("default_instance")
    instance.set_editor_property("collision_profile_name", "NoCollision")
    instance.set_editor_property("collision_enabled", unreal.CollisionEnabled.NO_COLLISION)
    body.set_editor_property("default_instance", instance)
    asset.set_editor_property("light_map_coordinate_index", 1)
    asset.set_editor_property("light_map_resolution", KIT["geometry"]["lightmapResolution"])
    bounds = asset.get_bounds()
    dimensions = [2*bounds.box_extent.x, 2*bounds.box_extent.y, 2*bounds.box_extent.z]
    expected = spec["dimensionsCm"]
    # Import axes may exchange X and Y. Z and the footprint dimensions must agree.
    assert all(abs(a-b) < .1 for a, b in zip(sorted(dimensions[:2]), sorted(expected[:2]))), (spec["name"], dimensions, expected)
    assert abs(dimensions[2]-expected[2]) < .1, (spec["name"], dimensions, expected)
    assert abs(bounds.origin.z-bounds.box_extent.z) < .1, "Ground pivot changed"
    assert EDITOR.get_num_uv_channels(asset, 0) >= 2, "Missing lightmap UVs"
    assert EDITOR.get_simple_collision_count(asset) == 0
    assert body.get_editor_property("default_instance").get_editor_property("collision_enabled") == unreal.CollisionEnabled.NO_COLLISION
    assert unreal.EditorAssetLibrary.save_loaded_asset(asset), "Save failed"
    RESULTS.append({"asset": asset.get_path_name(), "dimensionsCm": dimensions,
                    "uvChannels": EDITOR.get_num_uv_channels(asset, 0), "simpleCollisionCount": 0,
                    "defaultCollision": "NoCollision", "lightmapChannel": 1})

SAVED.mkdir(exist_ok=True, parents=True)
(SAVED / "unreal-validation.json").write_text(json.dumps({"status": "passed", "mode": "import" if NAMED is None else "materials",
                                                         "materials": [m.get_path_name() for m in MATERIALS], "assets": RESULTS}, indent=2)+"\n")
unreal.log("VEYRA_STRUCTURE_IMPORT_PASSED: " + str(len(MATERIALS)) + " materials, " + str(len(RESULTS)) + " static meshes")
