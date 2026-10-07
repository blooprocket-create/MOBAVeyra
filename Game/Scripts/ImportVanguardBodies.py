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
from KitMaterials.spec import body_problems  # noqa: E402
from VanguardBodies.inputs import stale_in  # noqa: E402
from veyra_material_graph import Graph  # noqa: E402

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
    materials that read it stay linked."""
    toon = KIT["bodyMaterial"]["toon"]
    path = DEST + "/" + toon["lightCollection"]
    writable(path)
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        collection = unreal.load_asset(path)
    else:
        collection = TOOLS.create_asset(toon["lightCollection"], DEST, unreal.MaterialParameterCollection, unreal.MaterialParameterCollectionFactoryNew())
    assert isinstance(collection, unreal.MaterialParameterCollection), path
    length = sum(channel * channel for channel in toon["toSun"]) ** 0.5
    wanted = {"ToSun": [channel / length for channel in toon["toSun"]] + [0.0], "SunColor": toon["sunColor"] + [1.0]}
    kept = {str(parameter.get_editor_property("parameter_name")): parameter for parameter in collection.get_editor_property("vector_parameters")}
    parameters = []
    for name, value in wanted.items():
        parameter = kept.get(name) or unreal.CollectionVectorParameter()
        parameter.set_editor_property("parameter_name", name)
        parameter.set_editor_property("default_value", unreal.LinearColor(*value))
        parameters.append(parameter)
    collection.set_editor_property("scalar_parameters", [])
    collection.set_editor_property("vector_parameters", parameters)
    assert unreal.EditorAssetLibrary.save_loaded_asset(collection, only_if_is_dirty=False), "Save failed: " + path
    return collection


def body_material(collection):
    """The one material every generated body wears (ADR-064 §4, ADR-068 §2): unlit, lit by its own toon light. Its graph
    is rebuilt on every import, so the generated asset always matches this definition, and its values are the kit's
    bodyMaterial.

    The vertex colour is the body's colour. The side facing the collection's sun is lit (colour x litTint x SunColor),
    the side away from it is in a painted shadow (colour x shadowTint), and the two meet in one soft band where the
    surface turns from the sun past bandThreshold. A rim of the sun's light (a fresnel from rimStart to rimEnd) edges the
    lit side. The whole is brightness times what the scene's exposure maps to white, so a body reads the same under the
    Crucible's physical sun as in a preview; what the vertex alpha marks glows on top at glowGain times that white."""
    values = KIT["bodyMaterial"]
    toon = values["toon"]
    material = unreal.load_asset(MATERIAL_PATH) if unreal.EditorAssetLibrary.does_asset_exist(MATERIAL_PATH) else None
    if material:
        EDIT.delete_all_material_expressions(material)
    else:
        material = TOOLS.create_asset("M_VeyraVanguardBody", DEST, unreal.Material, unreal.MaterialFactoryNew())
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
    # How squarely the surface faces the sun, from -1 (away) to 1.
    to_sun = g.node(unreal.MaterialExpressionNormalize, 2)
    g.link(light(0, "ToSun"), "", to_sun, "")
    facing = g.op(unreal.MaterialExpressionDotProduct, 3, g.node(unreal.MaterialExpressionVertexNormalWS, 2), to_sun)
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
    rim_edge = g.node(unreal.MaterialExpressionSmoothStep, 3, const_min=toon["rimStart"], const_max=toon["rimEnd"])
    g.link(fresnel, "", rim_edge, "Value")
    rim_strength = g.scalar(3, "RimStrength", toon["rimStrength"], "Toon")
    rim = g.op(unreal.MaterialExpressionMultiply, 5, g.op(unreal.MaterialExpressionMultiply, 4, rim_edge, band), rim_strength)
    rimmed = g.op(unreal.MaterialExpressionAdd, 6, shaded, g.op(unreal.MaterialExpressionMultiply, 5, sun, rim))
    bright = g.op(unreal.MaterialExpressionMultiply, 7, rimmed, g.scalar(6, "Brightness", toon["brightness"], "Toon"))
    # The glow, on top: what the vertex alpha marks, at glowGain.
    glow = g.op(unreal.MaterialExpressionMultiply, 2, g.op(unreal.MaterialExpressionMultiply, 1, color, color, "", "A"),
                g.scalar(1, "GlowStrength", values["glowGain"], "Toon"))
    total = g.op(unreal.MaterialExpressionAdd, 8, bright, glow)
    emissive = g.unexposed(9, total)
    assert EDIT.connect_material_property(emissive, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR), "emissive"
    material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
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
    # The rest-pose take the generator puts first, so the skeleton binds at rest, is no animation of the body's.
    bind_take = folder + "/" + sequence_name(asset, BIND_TAKE)
    if unreal.EditorAssetLibrary.does_asset_exist(bind_take):
        assert unreal.EditorAssetLibrary.delete_asset(bind_take), "Could not remove " + bind_take
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
    body.set_editor_property("upper_body_bone", unreal.Name(asset["upperBodyBone"]))
    body.set_editor_property("priority", asset.get("priority", 0))
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


material = body_material(toon_light())
results = [import_body(asset, material) for asset in SELECTED]
write_art_set()
SAVED.mkdir(parents=True, exist_ok=True)
(SAVED / "unreal-validation.json").write_text(json.dumps({"status": "passed", "bodies": results}, indent=2) + "\n")
for result in results:
    unreal.log("VEYRA_VANGUARD_BODY_ASSET: " + result["asset"] + " (" + str(len(result["animations"])) + " animations)")
unreal.log("VEYRA_VANGUARD_BODIES_IMPORTED: " + str(len(results)))
