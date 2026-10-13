"""Tests for when the terrain importer keeps an asset (ADR-040 §3). A texture is kept, neither imported nor saved, while
the source hash it records is that of its PNG now and it has the settings an import would give it; a material while the
hash of its values, its graph's code and the textures it samples is the one it records."""
from __future__ import annotations

import ast
import copy
import importlib.util
import json
import sys
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SCRIPTS = ROOT / "Game" / "Scripts"


def load(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    assert spec and spec.loader
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


inputs = load("terrain_art_inputs", SCRIPTS / "TerrainArt" / "inputs.py")
material_inputs = load("terrain_art_kit_material_inputs", SCRIPTS / "KitMaterials" / "inputs.py")

TERRAIN = ROOT / "Game" / "ArtSource" / "Environment" / "Terrain"
MANIFEST = json.loads((TERRAIN / "manifest.json").read_text(encoding="utf-8"))
PROFILE = json.loads((TERRAIN / "TerrainTextures.json").read_text(encoding="utf-8"))
INI = (ROOT / "Game" / "Config" / "DefaultGame.ini").read_text(encoding="utf-8")
SETTINGS = inputs.water_settings(INI)
# The graphs the importer builds each material with, as it hashes them.
GRAPHS = {"terrain": SCRIPTS / "crucible_terrain_material.py", "water": SCRIPTS / "crucible_water_material.py",
          "stir": SCRIPTS / "crucible_water_stir_material.py"}
SOURCE = "a" * 64
OTHER = "b" * 64


class TextureSettingsTests(unittest.TestCase):
    def test_only_base_colour_is_srgb(self):
        for entry in MANIFEST["files"]:
            name = Path(entry["name"]).stem
            self.assertEqual(inputs.texture_settings(name)["srgb"], name.endswith("_BaseColor"), name)

    def test_surface_normals_compress_as_normal_maps_and_the_water_normal_keeps_its_alpha(self):
        self.assertEqual(inputs.texture_settings("T_Crucible_Slate_Normal"),
                         {"compression_settings": "TC_NORMALMAP", "srgb": False, "lod_group": "TEXTUREGROUP_WORLD_NORMAL_MAP"})
        # Its alpha is the shore's foam, which normal-map compression would drop.
        self.assertEqual(inputs.texture_settings("T_Crucible_WaterNormal"),
                         {"compression_settings": "TC_BC7", "srgb": False, "lod_group": "TEXTUREGROUP_WORLD"})

    def test_packed_data_and_the_macro_variation_are_linear_with_alpha(self):
        for name in ("T_Crucible_Moss_ORMH", "T_Crucible_Macro"):
            self.assertEqual(inputs.texture_settings(name), {"compression_settings": "TC_BC7", "srgb": False, "lod_group": "TEXTUREGROUP_WORLD"})

    def test_every_texture_is_given_its_compression_colour_space_and_lod_group(self):
        for entry in MANIFEST["files"]:
            name = Path(entry["name"]).stem
            self.assertEqual(set(inputs.texture_settings(name)), {"compression_settings", "srgb", "lod_group"}, name)


class KeptTests(unittest.TestCase):
    WANTED = inputs.texture_settings("T_Crucible_Moss_BaseColor")

    def test_an_asset_imported_from_the_same_png_with_its_settings_is_kept(self):
        self.assertTrue(inputs.kept(SOURCE, SOURCE, dict(self.WANTED), self.WANTED))

    def test_an_asset_recording_no_source_is_imported(self):
        # Every texture imported before the importer recorded its source, and any made another way.
        self.assertFalse(inputs.kept("", SOURCE, dict(self.WANTED), self.WANTED))
        self.assertFalse(inputs.kept(None, SOURCE, dict(self.WANTED), self.WANTED))

    def test_a_changed_png_is_imported(self):
        self.assertFalse(inputs.kept(OTHER, SOURCE, dict(self.WANTED), self.WANTED))

    def test_an_asset_whose_settings_differ_from_an_imports_is_imported(self):
        # The settings changed since it was imported (or by hand): an import gives it them again.
        for key, value in (("compression_settings", "TC_BC7"), ("srgb", False), ("lod_group", "TEXTUREGROUP_WORLD_NORMAL_MAP")):
            self.assertFalse(inputs.kept(SOURCE, SOURCE, dict(self.WANTED, **{key: value}), self.WANTED), key)

    def test_the_committed_manifest_records_each_textures_source_hash(self):
        # What the importer records on each asset and compares with: hashlib's lowercase hex digest of the PNG.
        self.assertTrue(MANIFEST["files"])
        for entry in MANIFEST["files"]:
            self.assertRegex(entry["sha256"], r"^[0-9a-f]{64}$", entry["name"])


class MaterialValuesTests(unittest.TestCase):
    def changed(self, change):
        profile = copy.deepcopy(PROFILE)
        change(profile)
        return profile

    @staticmethod
    def layer(profile, layer_id):
        return next(layer for layer in profile["layers"] if layer["id"] == layer_id)

    def test_a_layers_recipe_is_no_input_of_the_terrain_material(self):
        # It makes only the layer's textures, which a reimport replaces at the paths the material samples.
        profile = self.changed(lambda p: self.layer(p, "Moss").update(cushionContrast=self.layer(p, "Moss")["cushionContrast"] + 0.1))
        self.assertEqual(inputs.terrain_material_values(PROFILE), inputs.terrain_material_values(profile))

    def test_tile_sizes_look_and_overgrowth_are_inputs_of_the_terrain_material(self):
        for label, change in (("layer tile", lambda p: self.layer(p, "Slate").update(tileMetres=self.layer(p, "Slate")["tileMetres"] + 1.0)),
                              ("macro tile", lambda p: p.update(macroTileMetres=p["macroTileMetres"] + 1.0)),
                              ("look", lambda p: p["material"].update(wetDarkening=p["material"]["wetDarkening"] + 0.1)),
                              ("overgrowth", lambda p: p["overgrowth"].update(strength=p["overgrowth"]["strength"] + 0.1))):
            self.assertNotEqual(inputs.terrain_material_values(PROFILE), inputs.terrain_material_values(self.changed(change)), label)

    def test_what_only_the_water_texture_is_made_from_is_no_input_of_the_water_material(self):
        ripples = self.changed(lambda p: p["water"].update(rippleStrength=p["water"]["rippleStrength"] + 0.1))
        self.assertEqual(inputs.water_material_values(PROFILE, SETTINGS), inputs.water_material_values(ripples, SETTINGS))
        rougher = self.changed(lambda p: p["water"].update(roughness=p["water"]["roughness"] / 2.0))
        self.assertNotEqual(inputs.water_material_values(PROFILE, SETTINGS), inputs.water_material_values(rougher, SETTINGS))

    def test_the_crests_froth_is_an_input_of_the_water_material(self):
        for control in ("stirFoam", "stirFoamFrom", "frothRoughness"):
            changed = self.changed(lambda p: p["water"].update({control: p["water"][control] / 2.0}))
            self.assertNotEqual(inputs.water_material_values(PROFILE, SETTINGS), inputs.water_material_values(changed, SETTINGS), control)

    def test_the_water_material_refuses_roughness_or_specular_outside_zero_to_one(self):
        for control, value in (("roughness", 1.5), ("specular", -0.1), ("roughness", "0.5"), ("frothRoughness", 1.1), ("stirFoam", -1.0)):
            with self.assertRaises(AssertionError, msg=control):
                inputs.water_material_values(self.changed(lambda p: p["water"].update({control: value})), SETTINGS)

    def test_the_committed_profile_gives_each_material_its_values(self):
        self.assertTrue(inputs.water_material_values(PROFILE, SETTINGS))
        self.assertTrue(inputs.stir_material_values(SETTINGS))
        self.assertTrue({"Moss", "Causeway", "Shore", "Slate", "Earth"} <= set(inputs.terrain_material_values(PROFILE)["tileMetres"]))


class WaterSettingsTests(unittest.TestCase):
    """The ripple parameters' names come from the water's settings (ADR-076), which the C++ that sets them reads too: a
    renamed parameter must rebuild the materials that read it."""

    def test_the_shipped_settings_name_every_ripple_parameter(self):
        self.assertEqual(set(inputs.water_material_values(PROFILE, SETTINGS)["stir"]), set(inputs.WATER_STIR_NAMES))
        stir = inputs.stir_material_values(SETTINGS)
        self.assertEqual(set(stir["names"]), set(inputs.STIR_NAMES))
        self.assertEqual(stir["defaults"]["Resolution"], float(SETTINGS["Resolution"]))

    def test_a_renamed_parameter_changes_the_values_of_the_materials_that_read_it(self):
        renamed = dict(SETTINGS, HeightParameter="StirHeights")
        self.assertNotEqual(inputs.water_material_values(PROFILE, SETTINGS), inputs.water_material_values(PROFILE, renamed))
        self.assertEqual(inputs.stir_material_values(SETTINGS), inputs.stir_material_values(renamed))
        damped = dict(SETTINGS, Damping="0.9")
        self.assertNotEqual(inputs.stir_material_values(SETTINGS), inputs.stir_material_values(damped))
        self.assertEqual(inputs.water_material_values(PROFILE, SETTINGS), inputs.water_material_values(PROFILE, damped))

    def test_a_missing_or_malformed_setting_is_refused(self):
        for broken in (dict(SETTINGS, WindowParameter=""), {k: v for k, v in SETTINGS.items() if k != "WindowParameter"}):
            with self.assertRaises(AssertionError):
                inputs.water_material_values(PROFILE, broken)
        for broken in (dict(SETTINGS, Damping="soft"), dict(SETTINGS, Resolution="0"), dict(SETTINGS, SourceParameterPrefix="1x")):
            with self.assertRaises(AssertionError):
                inputs.stir_material_values(broken)

    def test_only_the_water_section_is_read(self):
        other = "[/Script/Other.Settings]\nHeightParameter=Elsewhere\n"
        self.assertEqual(inputs.water_settings(other + INI)["HeightParameter"], SETTINGS["HeightParameter"])
        with self.assertRaises(AssertionError):
            inputs.water_settings(other)


class MaterialHashTests(unittest.TestCase):
    """The hash the importer records on each material: KitMaterials/inputs.py over its values, its graph's code and the
    shared graph helpers, and its textures' bindings."""

    TEXTURES = sorted(Path(entry["name"]).stem for entry in MANIFEST["files"] if entry["layer"] != "Water")

    def paths(self, folder="/Game/Veyra/World/Environment/Terrain/Textures"):
        return {name: f"{folder}/{name}.{name}" for name in self.TEXTURES}

    def hash(self, profile=PROFILE, paths=None):
        sources = [GRAPHS["terrain"], SCRIPTS / "veyra_material_graph.py"]
        return material_inputs.material_inputs_hash(inputs.terrain_material_values(profile), sources,
                                                    inputs.material_bindings(paths or self.paths()))

    def test_it_holds_while_nothing_the_material_is_built_from_changes(self):
        self.assertEqual(self.hash(), self.hash())
        moss = copy.deepcopy(PROFILE)
        next(layer for layer in moss["layers"] if layer["id"] == "Moss")["cushionRelief"] += 0.1
        self.assertEqual(self.hash(), self.hash(profile=moss))
        self.assertTrue(material_inputs.kept(self.hash(), self.hash()))

    def test_a_value_or_a_texture_moved_elsewhere_changes_it(self):
        tiles = copy.deepcopy(PROFILE)
        tiles["macroTileMetres"] += 1.0
        self.assertNotEqual(self.hash(), self.hash(profile=tiles))
        self.assertNotEqual(self.hash(), self.hash(paths=self.paths("/Game/Veyra/Elsewhere")))

    def test_a_binding_carries_the_settings_its_sampler_relies_on(self):
        bindings = inputs.material_bindings({"T_Crucible_Slate_Normal": "/Game/T.T", "T_Crucible_Slate_BaseColor": "/Game/B.B"})
        self.assertEqual(bindings["T_Crucible_Slate_Normal"], dict(inputs.texture_settings("T_Crucible_Slate_Normal"), path="/Game/T.T"))
        self.assertEqual(bindings["T_Crucible_Slate_BaseColor"]["compression_settings"], "TC_DEFAULT")

    def test_the_graphs_reach_nothing_but_their_values_the_engine_and_the_hashed_helpers(self):
        # Whatever else a graph read would be no input of its material's hash, and a change to it would leave the
        # material stale.
        for name, graph in GRAPHS.items():
            tree = ast.parse(graph.read_text(encoding="utf-8"))
            imported = {alias.name for node in ast.walk(tree) if isinstance(node, ast.Import) for alias in node.names}
            imported |= {node.module for node in ast.walk(tree) if isinstance(node, ast.ImportFrom)}
            self.assertEqual(imported, {"unreal", "veyra_material_graph"}, name)


if __name__ == "__main__":
    unittest.main()
