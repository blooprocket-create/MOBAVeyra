"""Import the generated Vanguard bodies (ADR-064) into Unreal; BuildVanguardBodies.ps1 runs it in an editor commandlet.

Each FBX GenerateVanguardBodies.py wrote is checked against the manifest, then imported as SK_<Id> with its own
skeleton and animation sequences under /Game/Veyra/Vanguards/<Id>. Every body wears M_VeyraVanguardBody, which this
script builds: its colour is the vertex colour, and the vertex alpha marks what glows. DA_VanguardArt then holds every
body in the manifest by Vanguard ID. A validation report records each body's height, triangles and animations.
"""
import hashlib
import json
import stat
import sys
from pathlib import Path

import unreal

GAME = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
sys.path.insert(0, str(GAME / "Scripts"))
import vanguard_body_material  # noqa: E402
from KitMaterials.inputs import INPUTS_TAG, kept, material_inputs_hash  # noqa: E402
from KitMaterials.spec import body_problems  # noqa: E402
from VanguardBodies.inputs import stale_in  # noqa: E402

SOURCE = GAME / "ArtSource" / "Vanguards"
SAVED = GAME / "Saved" / "VanguardKit"
KIT_BYTES = (SOURCE / "VanguardKit.json").read_bytes()
KIT = json.loads(KIT_BYTES)
MANIFEST = json.loads((SOURCE / "manifest.json").read_text())
DEST = KIT["destination"]
MATERIAL_PATH = DEST + "/M_VeyraVanguardBody"
# The generator's rest-pose take (GenerateVanguardBodies.py BIND_TAKE).
BIND_TAKE = "_Bind"
EDIT = unreal.MaterialEditingLibrary
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
# -VeyraOnly=A,B imports just those Vanguards' bodies, and -VeyraOnly= none (the art set is still written from the
# manifest: what a body pours or how far it strides can change when its mesh does not).
ONLY = next(([vanguard for vanguard in token.split("=", 1)[1].split(",") if vanguard] for token in unreal.SystemLibrary.get_command_line().split()
             if token.startswith("-VeyraOnly=")), None)

# Every body the art set will hold, not only those imported now, must have been made from what the kit and
# Vanguards.json give it today, by today's generator: a partial build cannot pass off a body made from an older kit.
STALE = stale_in(GAME)
assert not STALE, "Regenerate these bodies (GenerateVanguardBodies.py): their inputs changed since they were built: " + ", ".join(STALE)
MATERIAL_PROBLEMS = body_problems(KIT)
assert not MATERIAL_PROBLEMS, "VanguardKit.json: " + "; ".join(MATERIAL_PROBLEMS)
assert DEST.startswith("/Game/Veyra/"), "Vanguard bodies live under /Game/Veyra"
SELECTED = [asset for asset in MANIFEST["assets"] if ONLY is None or asset["id"] in ONLY]
assert ONLY is None or len({asset["id"] for asset in SELECTED}) == len(ONLY), "Unknown Vanguard in -VeyraOnly"


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


def toon_light():
    """MPC_VeyraToonLight, the toon light every body is shaded by (ADR-068 §2): the direction toward the sun (ToSun) and
    its colour (SunColor). The presentation sets both from the map's sun each frame; the kit's values are the defaults a
    world without a sun, or an editor preview, shades by. A parameter that already exists keeps its ID, so the
    materials that read it stay linked. Returns it, and whether this import created it."""
    toon = KIT["bodyMaterial"]["toon"]
    path = DEST + "/" + toon["lightCollection"]
    writable(path)
    created = not unreal.EditorAssetLibrary.does_asset_exist(path)
    if created:
        collection = TOOLS.create_asset(toon["lightCollection"], DEST, unreal.MaterialParameterCollection, unreal.MaterialParameterCollectionFactoryNew())
    else:
        collection = unreal.load_asset(path)
    assert isinstance(collection, unreal.MaterialParameterCollection), path
    length = sum(channel * channel for channel in toon["toSun"]) ** 0.5
    wanted = {"ToSun": [channel / length for channel in toon["toSun"]] + [0.0], "SunColor": toon["sunColor"] + [1.0]}
    existing = {str(parameter.get_editor_property("parameter_name")): parameter for parameter in collection.get_editor_property("vector_parameters")}
    parameters = []
    for name, value in wanted.items():
        parameter = existing.get(name) or unreal.CollectionVectorParameter()
        parameter.set_editor_property("parameter_name", name)
        parameter.set_editor_property("default_value", unreal.LinearColor(*value))
        parameters.append(parameter)
    collection.set_editor_property("scalar_parameters", [])
    collection.set_editor_property("vector_parameters", parameters)
    assert unreal.EditorAssetLibrary.save_loaded_asset(collection, only_if_is_dirty=False), "Save failed: " + path
    return collection, created


def body_material(collection, created):
    """The one material every generated body wears (ADR-064 §4, ADR-068 §2), its graph built by vanguard_body_material
    from the kit's bodyMaterial and linked to collection, the toon light (created: made by this import). It is kept as
    it is while what it is built from is unchanged (KitMaterials/inputs.py): that hash is recorded on the asset, and a
    rebuild, which makes the same graph with new expression IDs, would rewrite the asset for no change. Otherwise its
    graph is rebuilt whole, so the generated asset always matches its definition."""
    values = KIT["bodyMaterial"]
    # Its graph links to the toon light's parameters, by the IDs a collection keeps (toon_light) and one made anew
    # (created) does not: then the graph is linked again.
    bindings = {"collection": collection.get_path_name(),
                "parameters": sorted(str(parameter.get_editor_property("parameter_name")) for parameter in collection.get_editor_property("vector_parameters"))}
    inputs = material_inputs_hash(values, [GAME / "Scripts" / "vanguard_body_material.py", GAME / "Scripts" / "veyra_material_graph.py"], bindings)
    material = unreal.load_asset(MATERIAL_PATH) if unreal.EditorAssetLibrary.does_asset_exist(MATERIAL_PATH) else None
    if material and not created and kept(unreal.EditorAssetLibrary.get_metadata_tag(material, INPUTS_TAG), inputs):
        unreal.log("VEYRA_VANGUARD_BODY_MATERIAL_UNCHANGED: " + MATERIAL_PATH)
        return material
    writable(MATERIAL_PATH)
    if material:
        EDIT.delete_all_material_expressions(material)
    else:
        material = TOOLS.create_asset("M_VeyraVanguardBody", DEST, unreal.Material, unreal.MaterialFactoryNew())
    vanguard_body_material.build(material, collection, values)
    EDIT.recompile_material(material)
    unreal.EditorAssetLibrary.set_metadata_tag(material, INPUTS_TAG, inputs)
    assert unreal.EditorAssetLibrary.save_loaded_asset(material, only_if_is_dirty=False), "Material save failed"
    unreal.log("VEYRA_VANGUARD_BODY_MATERIAL_REBUILT: " + MATERIAL_PATH)
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
    # The rest-pose take the generator puts first, so the skeleton binds at rest, is no animation of the body's.
    bind_take = folder + "/" + sequence_name(asset, BIND_TAKE)
    if unreal.EditorAssetLibrary.does_asset_exist(bind_take):
        assert unreal.EditorAssetLibrary.delete_asset(bind_take), "Could not remove " + bind_take
    mesh = unreal.load_asset(folder + "/" + asset["name"])
    assert isinstance(mesh, unreal.SkeletalMesh), asset["name"] + " did not import as a skeletal mesh"
    materials = mesh.get_editor_property("materials")
    # Each slot by index: iterating the array gives copies of its structs, which would change nothing.
    for index in range(len(materials)):
        slot = materials[index]
        slot.set_editor_property("material_interface", material)
        materials[index] = slot
    mesh.set_editor_property("materials", materials)
    assert all(slot.get_editor_property("material_interface") == material for slot in mesh.get_editor_property("materials")), (asset["name"], "a slot does not wear its material")
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
    # Imported at its scale and in its rest pose. Taller would mean the import rebound the body to a posed frame, as
    # it does when a bone has no bind pose (the generator anchors every bone so that none lacks one).
    assert abs(height - asset["heightCm"]) < max(2.0, asset["heightCm"] * 0.02), (asset["name"], "height changed on import", height, asset["heightCm"])
    return {"asset": mesh.get_path_name(), "skeleton": skeleton.get_path_name(), "heightCm": round(height, 2),
            "capsuleHalfHeightCm": asset["capsuleHalfHeightCm"], "triangles": asset["triangles"], "animations": animations}


def fill_body(asset, body):
    """Body, one of an art entry's bodies, filled from its manifest asset: its mesh, animations and fitting."""
    mesh = unreal.load_asset(folder_of(asset) + "/" + asset["name"])
    assert isinstance(mesh, unreal.SkeletalMesh), asset["name"] + " is not imported"
    body.set_editor_property("mesh", mesh)
    body.set_editor_property("animations", {unreal.Name(clip["name"]): unreal.load_asset(folder_of(asset) + "/" + sequence_name(asset, clip["name"]))
                                            for clip in asset["animations"]})
    body.set_editor_property("run_stride", asset["runStrideCm"])
    body.set_editor_property("cast_release_share", asset["castReleaseShare"])

    def ability_cast(cast):
        # A skill's own clip, among the body's animations, and where it releases (ADR-072 §1).
        art = unreal.VeyraAbilityCastArt()
        art.set_editor_property("sequence", unreal.load_asset(folder_of(asset) + "/" + sequence_name(asset, cast["clip"])))
        art.set_editor_property("release_share", cast["releaseShare"])
        return art
    body.set_editor_property("ability_casts", {unreal.Name(cast["ability"]): ability_cast(cast) for cast in asset.get("abilityCasts", [])})
    body.set_editor_property("upper_body_bone", unreal.Name(asset["upperBodyBone"]))
    body.set_editor_property("priority", asset.get("priority", 0))
    ik = asset.get("ik", {})

    def chain(names):
        limb = unreal.VeyraLimbChain()
        limb.set_editor_property("root", unreal.Name(names[0]))
        limb.set_editor_property("joint", unreal.Name(names[1]))
        limb.set_editor_property("end", unreal.Name(names[2]))
        return limb
    body.set_editor_property("foot_chains", [chain(names) for names in ik.get("feet", [])])
    if ik.get("offHand"):
        body.set_editor_property("off_hand", chain(ik["offHand"]["chain"]))
        body.set_editor_property("off_hand_anchor", unreal.Name(ik["offHand"]["anchor"]))
    springs = asset.get("springs", {})
    loose = []
    for entry in springs.get("chains", []):
        art = unreal.VeyraSpringChainArt()
        art.set_editor_property("bones", [unreal.Name(bone) for bone in entry["bones"]])
        art.set_editor_property("stiffness", entry["stiffness"])
        art.set_editor_property("drag", entry["drag"])
        art.set_editor_property("damping", entry["damping"])
        art.set_editor_property("max_angle_degrees", entry["maxAngle"])
        loose.append(art)
    body.set_editor_property("spring_chains", loose)
    colliders = []
    for entry in springs.get("colliders", []):
        collider = unreal.VeyraSpringColliderArt()
        collider.set_editor_property("from", unreal.Name(entry["from"]))
        collider.set_editor_property("to", unreal.Name(entry["to"]))
        collider.set_editor_property("radius", entry["radius"])
        colliders.append(collider)
    body.set_editor_property("spring_colliders", colliders)
    if asset.get("effect"):
        effect = unreal.load_asset(asset["effect"]["system"])
        assert isinstance(effect, unreal.NiagaraSystem), (asset["name"], "its effect does not load; build it with BuildEffects.ps1", asset["effect"]["system"])
        body.set_editor_property("effect", effect)
        body.set_editor_property("effect_bones", [unreal.Name(bone) for bone in asset["effect"]["bones"]])
        body.set_editor_property("effect_color", unreal.LinearColor(*asset["effect"]["color"]))
        body.set_editor_property("effect_scale", asset["effect"]["scale"])
    return body


def write_art_set():
    """DA_VanguardArt: every Vanguard's own body by its ID, with the bodies it wears while it holds a status (a rider's
    ride), each with its animations by name (ADR-064 §1, §3)."""
    path = DEST + "/DA_VanguardArt"
    writable(path)
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        art_set = unreal.load_asset(path)
    else:
        factory = unreal.DataAssetFactory()
        factory.set_editor_property("data_asset_class", unreal.VeyraVanguardArtSet)
        art_set = TOOLS.create_asset("DA_VanguardArt", DEST, unreal.VeyraVanguardArtSet, factory)
    assert isinstance(art_set, unreal.VeyraVanguardArtSet), path
    entries, status_bodies = {}, {}
    for asset in sorted(MANIFEST["assets"], key=lambda entry: (entry["id"], entry.get("status", ""))):
        if asset.get("status"):
            status_bodies.setdefault(asset["id"], {})[unreal.Name(asset["status"])] = fill_body(asset, unreal.VeyraVanguardBody())
        else:
            entries[asset["id"]] = fill_body(asset, unreal.VeyraVanguardArt())
    for owner, bodies in status_bodies.items():
        assert owner in entries, owner + " has a status body but no body of its own"
        entries[owner].set_editor_property("status_bodies", bodies)
    # Companions (the other half of a pair, as Nix) by companion ID, the Vanguards by Vanguard ID.
    companions = {asset["id"] for asset in MANIFEST["assets"] if asset.get("companion")}
    art_set.set_editor_property("art", {unreal.Name(owner): entry for owner, entry in entries.items() if owner not in companions})
    art_set.set_editor_property("companion_art", {unreal.Name(owner): entry for owner, entry in entries.items() if owner in companions})
    assert unreal.EditorAssetLibrary.save_loaded_asset(art_set, only_if_is_dirty=False), "Save failed: " + path
    unreal.log("VEYRA_VANGUARD_ART_SET: " + path + " dresses " + ", ".join(sorted(entries)))


material = body_material(*toon_light())
results = [import_body(asset, material) for asset in SELECTED]
write_art_set()
SAVED.mkdir(parents=True, exist_ok=True)
(SAVED / "unreal-validation.json").write_text(json.dumps({"status": "passed", "bodies": results}, indent=2) + "\n")
for result in results:
    unreal.log("VEYRA_VANGUARD_BODY_ASSET: " + result["asset"] + " (" + str(len(result["animations"])) + " animations)")
unreal.log("VEYRA_VANGUARD_BODIES_IMPORTED: " + str(len(results)))
