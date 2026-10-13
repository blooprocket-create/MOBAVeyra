"""Tests for when the terrain importer keeps a texture asset (ADR-040 §3): it is kept, neither imported nor saved, while
the source hash it records is that of its PNG now and it has the settings an import would give it."""
from __future__ import annotations

import importlib.util
import json
import sys
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / "Game" / "Scripts" / "TerrainArt" / "inputs.py"
spec = importlib.util.spec_from_file_location("terrain_art_inputs", SCRIPT)
assert spec and spec.loader
inputs = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = inputs
spec.loader.exec_module(inputs)

MANIFEST = json.loads((ROOT / "Game" / "ArtSource" / "Environment" / "Terrain" / "manifest.json").read_text(encoding="utf-8"))
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


if __name__ == "__main__":
    unittest.main()
