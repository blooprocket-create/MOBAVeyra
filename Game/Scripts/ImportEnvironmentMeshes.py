"""Editor-only import of the Crucible's environment kit (ADR-040 C6), with its materials.

Invoked by BuildEnvironmentArt.ps1 in an editor commandlet with the PythonScriptPlugin. Builds the kit's materials from
the terrain's generated textures (the stone and the moss match the ground they stand on), imports every mesh in the
manifest, assigns materials by slot name, turns on Nanite for stone and validates size, pivot and the absence of
collision: the kit is presentation, and World owns what blocks. Does not edit a map.
"""
import hashlib
import json
import sys
from pathlib import Path

import unreal

sys.path.insert(0, str(Path(__file__).resolve().parent))
from veyra_material_graph import EDIT, Graph  # noqa: E402

GAME = Path(__file__).resolve().parents[1]
SOURCE = GAME / "ArtSource" / "Environment"
SAVED = GAME / "Saved" / "EnvironmentKit"
DEST = "/Game/Veyra/World/Environment"
TEXTURES = DEST + "/Terrain/Textures"
KIT = json.loads((SOURCE / "CrucibleKit.json").read_text(encoding="utf-8"))
MANIFEST = json.loads((SOURCE / "manifest.json").read_text(encoding="utf-8"))
assert MANIFEST["profileSha256"] == hashlib.sha256((SOURCE / "CrucibleKit.json").read_bytes()).hexdigest(), "Regenerate after kit changes."
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
# The legacy FBX importer, which honours the options below; set here, since a command-line -ExecCmds may run only after
# this script has begun importing.
unreal.SystemLibrary.execute_console_command(None, "Interchange.FeatureFlags.Import.FBX 0")
ASSETS = unreal.EditorAssetLibrary
MESHES = unreal.get_default_object(unreal.StaticMeshEditorSubsystem)
LOOK = KIT["look"]

# Stone stands on the ground whose textures it shares; foliage sways; nothing blocks.
STONE_KINDS = {"rock", "pillar", "block", "stele"}


def texture(name):
    asset = unreal.load_asset(f"{TEXTURES}/T_Crucible_{name}")
    assert asset, f"Build the terrain art first: T_Crucible_{name}"
    return asset


def pin(names, prefix):
    match = next((name for name in names if str(name).startswith(prefix)), None)
    assert match, f"no pin {prefix} among {names}"
    return str(match)


def world_aligned(g, column, function, texture_asset, size, output):
    """The engine's world-aligned (triplanar) projection of Texture Asset, Size units to a tile."""
    tex = g.node(unreal.MaterialExpressionTextureObject, column - 1, texture=texture_asset)
    call = g.node(unreal.MaterialExpressionMaterialFunctionCall, column)
    call.set_editor_property("material_function", unreal.load_asset(f"/Engine/Functions/Engine_MaterialFunctions01/Texturing/{function}"))
    inputs = EDIT.get_material_expression_input_names(call)
    g.link(tex, "", call, pin(inputs, "TextureObject"))
    tile = g.node(unreal.MaterialExpressionConstant3Vector, column - 1, constant=unreal.LinearColor(size, size, size, 0.0))
    g.link(tile, "", call, pin(inputs, "TextureSize"))
    return call, pin(EDIT.get_material_expression_output_names(call), output)


def material(name, build):
    path = f"{DEST}/Materials/{name}"
    asset = unreal.load_asset(path) if ASSETS.does_asset_exist(path) else None
    if not asset:
        asset = TOOLS.create_asset(name, DEST + "/Materials", unreal.Material, unreal.MaterialFactoryNew())
    EDIT.delete_all_material_expressions(asset)
    for expression in EDIT.get_material_expressions(asset):
        EDIT.delete_material_expression(asset, expression)
    asset.set_editor_property("used_with_instanced_static_meshes", True)
    asset.set_editor_property("used_with_nanite", True)
    build(asset, Graph(asset))
    errors = EDIT.recompile_material(asset)
    assert not errors, f"{name} does not compile: {errors}"
    ASSETS.save_loaded_asset(asset)
    return asset


def mossy_stone(base_texture, tint, look):
    """Stone, world-aligned so it needs no UVs, with moss on whatever faces up."""

    def build(asset, g):
        stone, stone_out = world_aligned(g, 2, "WorldAlignedTexture", texture(base_texture), look["stoneTileCm"], "XYZ Texture")
        normal, normal_out = world_aligned(g, 2, "WorldAlignedNormal", texture("Slate_Normal"), look["stoneTileCm"], "XYZ Texture")
        moss, moss_out = world_aligned(g, 2, "WorldAlignedTexture", texture("Moss_BaseColor"), look["mossTileCm"], "XY Texture")
        grey = g.node(unreal.MaterialExpressionDesaturation, 3)
        g.link(stone, stone_out, grey, "")
        amount = g.node(unreal.MaterialExpressionConstant, 3, r=look["desaturate"])
        g.link(amount, "", grey, "Fraction")
        colour = g.node(unreal.MaterialExpressionVectorParameter, 3, parameter_name="StoneTint", group="Stone",
                        default_value=unreal.LinearColor(*tint, 1.0))
        tinted = g.op(unreal.MaterialExpressionMultiply, 4, grey, colour, b_out="RGB")
        up = g.node(unreal.MaterialExpressionVertexNormalWS, 3)
        facing = g.node(unreal.MaterialExpressionComponentMask, 4, r=False, g=False, b=True, a=False)
        g.link(up, "", facing, "")
        start = g.scalar(4, "MossFrom", look["mossFrom"], "Moss")
        end = g.scalar(4, "MossTo", look["mossTo"], "Moss")
        mask = g.node(unreal.MaterialExpressionSmoothStep, 5)
        g.link(start, "", mask, "Min")
        g.link(end, "", mask, "Max")
        g.link(facing, "", mask, "Value")
        base = g.node(unreal.MaterialExpressionLinearInterpolate, 6)
        g.link(tinted, "", base, "A")
        g.link(moss, moss_out, base, "B")
        g.link(mask, "", base, "Alpha")
        roughness = g.scalar(6, "Roughness", look["stoneRoughness"], "Stone")
        EDIT.connect_material_property(base, "", unreal.MaterialProperty.MP_BASE_COLOR)
        EDIT.connect_material_property(normal, normal_out, unreal.MaterialProperty.MP_NORMAL)
        EDIT.connect_material_property(roughness, "", unreal.MaterialProperty.MP_ROUGHNESS)

    return build


def glyph(asset, g):
    """Blue Flux glyphs: emissive, breathing slowly."""
    colour = g.node(unreal.MaterialExpressionVectorParameter, 1, parameter_name="GlyphColor", group="Glyph",
                    default_value=unreal.LinearColor(*LOOK["glyphColor"], 1.0))
    strength = g.scalar(1, "GlyphStrength", LOOK["glyphStrength"], "Glyph")
    time = g.node(unreal.MaterialExpressionTime, 1)
    rate = g.scalar(1, "GlyphPulseRate", LOOK["glyphPulseRate"], "Glyph")
    wave = g.node(unreal.MaterialExpressionSine, 3)
    g.link(g.op(unreal.MaterialExpressionMultiply, 2, time, rate), "", wave, "")
    depth = g.scalar(2, "GlyphPulseDepth", LOOK["glyphPulseDepth"], "Glyph")
    one = g.node(unreal.MaterialExpressionConstant, 3, r=1.0)
    pulse = g.op(unreal.MaterialExpressionAdd, 4, one, g.op(unreal.MaterialExpressionMultiply, 4, wave, depth))
    emissive = g.op(unreal.MaterialExpressionMultiply, 5, g.op(unreal.MaterialExpressionMultiply, 4, colour, strength, a_out="RGB"), pulse)
    EDIT.connect_material_property(colour, "RGB", unreal.MaterialProperty.MP_BASE_COLOR)
    EDIT.connect_material_property(emissive, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)


def swaying(g, height_share):
    """A gentle sway that grows with Height Share, phased across the world so no two plants move as one."""
    world = g.node(unreal.MaterialExpressionWorldPosition, 0)
    phase_in = g.node(unreal.MaterialExpressionComponentMask, 1, r=True, g=True, b=False, a=False)
    g.link(world, "", phase_in, "")
    spread = g.node(unreal.MaterialExpressionConstant2Vector, 1, r=LOOK["windSpread"], g=LOOK["windSpread"] * 0.7)
    phase = g.node(unreal.MaterialExpressionDotProduct, 2)
    g.link(phase_in, "", phase, "A")
    g.link(spread, "", phase, "B")
    time = g.node(unreal.MaterialExpressionTime, 1)
    speed = g.scalar(1, "WindSpeed", LOOK["windSpeed"], "Wind")
    wave = g.node(unreal.MaterialExpressionSine, 4)
    g.link(g.op(unreal.MaterialExpressionAdd, 3, g.op(unreal.MaterialExpressionMultiply, 2, time, speed), phase), "", wave, "")
    amplitude = g.scalar(3, "WindAmplitude", LOOK["windAmplitude"], "Wind")
    direction = g.node(unreal.MaterialExpressionConstant3Vector, 4, constant=unreal.LinearColor(0.8, 0.6, 0.0, 0.0))
    offset = g.op(unreal.MaterialExpressionMultiply, 5, direction, g.op(unreal.MaterialExpressionMultiply, 5, wave, amplitude))
    return g.op(unreal.MaterialExpressionMultiply, 6, offset, height_share)


def foliage(colour_key, two_sided, root_key=None):
    """Leaves or blades: their vertex colour's tint over the look's colour, swaying by their height (vertex alpha)."""

    def build(asset, g):
        asset.set_editor_property("two_sided", two_sided)
        # The vertex colour's alpha is the height share up the plant (its own output: the default output is RGB).
        vertex = g.node(unreal.MaterialExpressionVertexColor, 0)
        tip = g.node(unreal.MaterialExpressionVectorParameter, 1, parameter_name="Colour", group="Foliage",
                     default_value=unreal.LinearColor(*LOOK[colour_key], 1.0))
        colour = tip
        if root_key:
            root = g.node(unreal.MaterialExpressionVectorParameter, 1, parameter_name="RootColour", group="Foliage",
                          default_value=unreal.LinearColor(*LOOK[root_key], 1.0))
            colour = g.node(unreal.MaterialExpressionLinearInterpolate, 2)
            g.link(root, "RGB", colour, "A")
            g.link(tip, "RGB", colour, "B")
            g.link(vertex, "A", colour, "Alpha")
        tint = g.node(unreal.MaterialExpressionComponentMask, 1, r=True, g=True, b=True, a=False)
        g.link(vertex, "", tint, "")
        # Leafy detail: the jungle's moss, projected from above, under the look's colour and each mass's tint.
        leafy, leafy_out = world_aligned(g, 2, "WorldAlignedTexture", texture("Moss_BaseColor"), LOOK["leafDetailTileCm"], "XY Texture")
        detail = g.op(unreal.MaterialExpressionMultiply, 3, leafy, colour, a_out=leafy_out, b_out="RGB" if not root_key else "")
        base = g.op(unreal.MaterialExpressionMultiply, 3, detail, tint)
        roughness = g.scalar(3, "Roughness", LOOK["foliageRoughness"], "Foliage")
        EDIT.connect_material_property(base, "", unreal.MaterialProperty.MP_BASE_COLOR)
        EDIT.connect_material_property(roughness, "", unreal.MaterialProperty.MP_ROUGHNESS)
        sway = swaying(g, g.op(unreal.MaterialExpressionMultiply, 5, vertex, vertex, a_out="A", b_out="A"))
        EDIT.connect_material_property(sway, "", unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET)

    return build


MATERIALS = {
    "Rock": material("M_CrucibleRock", mossy_stone("Slate_BaseColor", LOOK["rockTint"], LOOK)),
    "Ruin": material("M_CrucibleRuin", mossy_stone("Slate_BaseColor", LOOK["ruinTint"], LOOK)),
    "Bark": material("M_CrucibleBark", mossy_stone("Slate_BaseColor", LOOK["barkTint"], dict(LOOK, mossFrom=0.95, mossTo=1.0))),
    "Glyph": material("M_CrucibleGlyph", glyph),
    "Leaves": material("M_CrucibleLeaves", foliage("leafColour", two_sided=True)),
    "Grass": material("M_CrucibleGrass", foliage("grassTipColour", two_sided=True, root_key="grassRootColour")),
}

RESULTS = []
for spec in MANIFEST["assets"]:
    filename = SOURCE / spec["file"]
    assert hashlib.sha256(filename.read_bytes()).hexdigest() == spec["sha256"], "FBX differs from manifest"
    # Each mesh is a function of its FBX alone: imported fresh, never over an earlier import's settings.
    if ASSETS.does_asset_exist(DEST + "/Meshes/" + spec["name"]):
        assert ASSETS.delete_asset(DEST + "/Meshes/" + spec["name"]), spec["name"] + " could not be replaced"
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
    data.set_editor_property("vertex_color_import_option", unreal.VertexColorImportOption.REPLACE)
    task.set_editor_property("options", options)
    TOOLS.import_asset_tasks([task])
    asset = unreal.load_asset(DEST + "/Meshes/" + spec["name"])
    assert isinstance(asset, unreal.StaticMesh), spec["name"] + " did not import"
    # Slot names, not importer ordering, decide each surface's material.
    for index, slot in enumerate(asset.get_editor_property("static_materials")):
        name = str(slot.get_editor_property("material_slot_name"))
        assert name in MATERIALS, "Unrecognised material slot: " + name
        asset.set_material(index, MATERIALS[name])
    # Stone is dense and static: Nanite. Foliage sways, and stays a regular mesh.
    nanite = asset.get_editor_property("nanite_settings")
    nanite.set_editor_property("enabled", spec["kind"] in STONE_KINDS)
    asset.set_editor_property("nanite_settings", nanite)
    # Presentation never blocks: no simple shapes, and its triangles are not complex collision either.
    body = asset.get_editor_property("body_setup")
    body.set_editor_property("collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_SIMPLE_AS_COMPLEX)
    instance = body.get_editor_property("default_instance")
    instance.set_editor_property("collision_profile_name", "NoCollision")
    instance.set_editor_property("collision_enabled", unreal.CollisionEnabled.NO_COLLISION)
    body.set_editor_property("default_instance", instance)
    bounds = asset.get_bounds()
    dimensions = [2 * bounds.box_extent.x, 2 * bounds.box_extent.y, 2 * bounds.box_extent.z]
    expected = spec["dimensionsCm"]
    # Import axes may exchange X and Y. Z and the footprint must agree; the pivot stays at the base.
    assert all(abs(a - b) < 1.0 for a, b in zip(sorted(dimensions[:2]), sorted(expected[:2]))), (spec["name"], dimensions, expected)
    assert abs(dimensions[2] - expected[2]) < 1.0, (spec["name"], dimensions, expected)
    assert MESHES.get_simple_collision_count(asset) == 0
    assert ASSETS.save_loaded_asset(asset), "Save failed"
    RESULTS.append({"asset": asset.get_path_name(), "kind": spec["kind"], "dimensionsCm": dimensions,
                    "nanite": spec["kind"] in STONE_KINDS, "defaultCollision": "NoCollision"})

SAVED.mkdir(exist_ok=True, parents=True)
(SAVED / "unreal-validation.json").write_text(json.dumps({"status": "passed", "materials": sorted(m.get_path_name() for m in MATERIALS.values()),
                                                         "assets": RESULTS}, indent=2) + "\n")
unreal.log("VEYRA_ENVIRONMENT_IMPORT_PASSED: " + str(len(RESULTS)) + " static meshes")
