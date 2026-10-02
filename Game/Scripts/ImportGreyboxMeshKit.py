"""Validated editor-only static mesh import for generated greybox art kits."""
import hashlib
import json
import stat
from pathlib import Path

import unreal


def import_kit(game, source_folder, kit_filename, destination, saved_folder, success_marker):
    GAME = Path(game)
    SOURCE = GAME / "ArtSource" / source_folder
    SAVED = GAME / "Saved" / saved_folder
    DEST = destination
    VERIFY_ONLY = "-VeyraArtVerifyOnly" in unreal.SystemLibrary.get_command_line()
    KIT = json.loads((SOURCE / kit_filename).read_text())
    MANIFEST = json.loads((SOURCE / "manifest.json").read_text())
    world_bytes = (GAME / "Tuning" / "World.json").read_bytes()
    if "worldSha256" in MANIFEST:
        assert MANIFEST["worldSha256"] == hashlib.sha256(world_bytes).hexdigest(), "Regenerate after World.json changes."
    else:
        units = json.loads(world_bytes)["fluxborn"]["units"]
        footprints = {key: {field: value[field] for field in ("capsuleRadius", "capsuleHalfHeight")} for key, value in units.items()}
        assert MANIFEST["footprintsCm"] == footprints, "Regenerate after Fluxborn footprint changes."
        assert MANIFEST["kitSha256"] == hashlib.sha256((SOURCE / kit_filename).read_bytes()).hexdigest(), "Regenerate after art settings change."
    # Validate the complete source set before mutating any Unreal assets.
    for entry in MANIFEST["assets"]:
        source_file = (SOURCE / entry["file"]).resolve()
        assert source_file.is_relative_to(SOURCE.resolve()), "FBX path escapes source kit"
        assert hashlib.sha256(source_file.read_bytes()).hexdigest() == entry["sha256"], "FBX differs from manifest"
    if not VERIFY_ONLY:
        destination_disk = GAME / "Content" / DEST.removeprefix("/Game/")
        targets = [destination_disk / "Materials" / (entry["name"] + ".uasset") for entry in KIT["materials"]]
        targets += [destination_disk / "Meshes" / (entry["name"] + ".uasset") for entry in MANIFEST["assets"]]
        for target in targets:
            if target.exists() and getattr(target.stat(), "st_file_attributes", 0) & stat.FILE_ATTRIBUTE_READONLY:
                raise RuntimeError("Acquire the Git LFS lock before reimporting: " + str(target))
    TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
    # Commandlets do not initialize the interactive editor's subsystem collection.
    # These asset-only helpers have no per-instance state.
    EDITOR = unreal.get_default_object(unreal.StaticMeshEditorSubsystem)
    MATERIALS = []


    def create_material(spec):
        path = DEST + "/Materials/" + spec["name"]
        material = unreal.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else None
        if VERIFY_ONLY:
            assert isinstance(material, unreal.Material), "Missing saved material: " + path
            return material
        if not material:
            material = TOOLS.create_asset(spec["name"], DEST + "/Materials", unreal.Material, unreal.MaterialFactoryNew())
        edit = unreal.MaterialEditingLibrary
        edit.delete_all_material_expressions(material)
        color = edit.create_material_expression(material, unreal.MaterialExpressionVectorParameter, -500, 0)
        color.set_editor_property("parameter_name", "FluxTint" if spec["emission"] else "SurfaceColor")
        color.set_editor_property("default_value", unreal.LinearColor(*spec["color"]))
        edit.connect_material_property(color, "RGB", unreal.MaterialProperty.MP_BASE_COLOR)
        for i, (key, prop) in enumerate([("metallic", unreal.MaterialProperty.MP_METALLIC), ("roughness", unreal.MaterialProperty.MP_ROUGHNESS)]):
            value = edit.create_material_expression(material, unreal.MaterialExpressionScalarParameter, -500, 180+i*120)
            value.set_editor_property("parameter_name", key.title())
            value.set_editor_property("default_value", spec[key])
            edit.connect_material_property(value, "", prop)
        if spec["emission"]:
            strength = edit.create_material_expression(material, unreal.MaterialExpressionScalarParameter, -500, -180)
            strength.set_editor_property("parameter_name", "FluxStrength")
            strength.set_editor_property("default_value", spec["emission"])
            multiply = edit.create_material_expression(material, unreal.MaterialExpressionMultiply, -200, -100)
            edit.connect_material_expressions(color, "RGB", multiply, "A")
            edit.connect_material_expressions(strength, "", multiply, "B")
            edit.connect_material_property(multiply, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
        edit.recompile_material(material)
        assert unreal.EditorAssetLibrary.save_loaded_asset(material), "Material save failed: " + path
        return material


    for spec in KIT["materials"]:
        MATERIALS.append(create_material(spec))

    RESULTS = []
    for spec in MANIFEST["assets"]:
        filename = SOURCE / spec["file"]
        assert hashlib.sha256(filename.read_bytes()).hexdigest() == spec["sha256"], "FBX differs from manifest"
        if not VERIFY_ONLY:
            task = unreal.AssetImportTask()
            task.set_editor_property("filename", str(filename))
            task.set_editor_property("destination_path", DEST + "/Meshes")
            task.set_editor_property("destination_name", spec["name"])
            task.set_editor_property("automated", True)
            task.set_editor_property("replace_existing", True)
            task.set_editor_property("replace_existing_settings", True)
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
            data.set_editor_property("import_uniform_scale", 1.0)
            data.set_editor_property("transform_vertex_to_absolute", True)
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
            if VERIFY_ONLY:
                assert slot.get_editor_property("material_interface") == match, "Saved material mapping changed"
            else:
                asset.set_material(i, match)
        # No simple shapes were imported. Complex geometry must not become an alternate
        # collision owner when an artist drags a preview mesh into a map.
        body = asset.get_editor_property("body_setup")
        if not VERIFY_ONLY:
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
        assert abs(bounds.origin.z-bounds.box_extent.z) < .1, (spec["name"], "Ground pivot changed", bounds.origin.z-bounds.box_extent.z)
        if "forwardSocket" in spec:
            socket = asset.find_socket(spec["forwardSocket"])
            assert socket, "Missing facing socket"
            facing = socket.get_editor_property("relative_location")
            assert facing.x > 0 and abs(facing.y) < .1, (spec["name"], "Expected +X forward", facing)
        assert EDITOR.get_num_uv_channels(asset, 0) >= 2, "Missing lightmap UVs"
        assert EDITOR.get_simple_collision_count(asset) == 0
        assert body.get_editor_property("default_instance").get_editor_property("collision_enabled") == unreal.CollisionEnabled.NO_COLLISION
        triangle_count = asset.get_num_triangles(0)
        assert 0 < triangle_count <= spec.get("triangleBudget", spec["triangles"]), (spec["name"], "Triangle budget", triangle_count)
        if not VERIFY_ONLY:
            assert unreal.EditorAssetLibrary.save_loaded_asset(asset), "Save failed"
        RESULTS.append({"asset": asset.get_path_name(), "dimensionsCm": dimensions,
                        "uvChannels": EDITOR.get_num_uv_channels(asset, 0), "simpleCollisionCount": 0,
                        "defaultCollision": "NoCollision", "lightmapChannel": 1,
                        "triangles": triangle_count, "forward": "+X" if "forwardSocket" in spec else "Not specified"})

    SAVED.mkdir(exist_ok=True, parents=True)
    (SAVED / ("unreal-verify.json" if VERIFY_ONLY else "unreal-validation.json")).write_text(json.dumps({"status": "passed", "mode": "saved assets" if VERIFY_ONLY else "import", "assets": RESULTS}, indent=2)+"\n")
    unreal.log(success_marker + ": " + str(len(RESULTS)) + " static meshes")
