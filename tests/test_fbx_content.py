"""Tests for what a generated FBX holds (Game/Scripts/fbx_content.py): the same mesh exported in two runs compares equal
though its bytes differ, as does one whose material's values differ, while a moved vertex or a renamed material slot
does not; a file that is not an FBX fails loudly, and a generator keeps an unchanged FBX's bytes. The fixtures are real
Blender exports (tests/fixtures/fbx/make_fixtures.py)."""
from __future__ import annotations

import importlib.util
import shutil
import sys
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / "Game" / "Scripts" / "fbx_content.py"
FIXTURES = ROOT / "tests" / "fixtures" / "fbx"
spec = importlib.util.spec_from_file_location("fbx_content", SCRIPT)
assert spec and spec.loader
fbx = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = fbx
spec.loader.exec_module(fbx)

CUBE_A = FIXTURES / "cube_a.fbx"
CUBE_B = FIXTURES / "cube_b.fbx"
MOVED = FIXTURES / "cube_moved.fbx"
GLOWING = FIXTURES / "cube_glowing.fbx"
RENAMED = FIXTURES / "cube_renamed.fbx"


class Content(unittest.TestCase):
    def test_two_exports_of_one_mesh_differ_in_bytes_but_not_content(self):
        self.assertNotEqual(CUBE_A.read_bytes(), CUBE_B.read_bytes())
        self.assertEqual(fbx.content_sha256(CUBE_A), fbx.content_sha256(CUBE_B))

    def test_a_moved_vertex_is_a_change(self):
        self.assertNotEqual(fbx.content_sha256(CUBE_A), fbx.content_sha256(MOVED))

    def test_a_materials_values_are_not_content_but_its_slot_name_is(self):
        # Importers build materials from the kit and take only slot names from an FBX.
        self.assertNotEqual(CUBE_A.read_bytes(), GLOWING.read_bytes())
        self.assertEqual(fbx.content_sha256(CUBE_A), fbx.content_sha256(GLOWING))
        self.assertNotEqual(fbx.content_sha256(CUBE_A), fbx.content_sha256(RENAMED))

    def test_the_file_reads_as_its_whole_node_tree(self):
        names = [node[0] for node in fbx.read(CUBE_A)]
        for name in (b"FBXHeaderExtension", b"Objects", b"Connections"):
            self.assertIn(name, names)

    def test_what_is_not_an_fbx_fails_loudly(self):
        with tempfile.TemporaryDirectory() as folder:
            pointer = Path(folder) / "pointer.fbx"
            pointer.write_text("version https://git-lfs.github.com/spec/v1\noid sha256:0\nsize 1\n")
            truncated = Path(folder) / "truncated.fbx"
            truncated.write_bytes(CUBE_A.read_bytes()[:len(CUBE_A.read_bytes()) // 2])
            for broken in (pointer, truncated):
                with self.assertRaises(fbx.FbxError, msg=broken.name):
                    fbx.content_sha256(broken)


class Keeping(unittest.TestCase):
    def setUp(self):
        self.folder = tempfile.TemporaryDirectory()
        self.target = Path(self.folder.name) / "SM_FixtureCube.fbx"

    def tearDown(self):
        self.folder.cleanup()

    def export_of(self, source):
        return lambda path: shutil.copyfile(source, path)

    def test_an_unchanged_mesh_keeps_its_file(self):
        shutil.copyfile(CUBE_A, self.target)
        self.assertTrue(fbx.export_keeping_unchanged(self.export_of(CUBE_B), self.target))
        self.assertEqual(self.target.read_bytes(), CUBE_A.read_bytes())
        self.assertEqual([path.name for path in Path(self.folder.name).iterdir()], [self.target.name])

    def test_a_changed_mesh_replaces_it(self):
        shutil.copyfile(CUBE_A, self.target)
        self.assertFalse(fbx.export_keeping_unchanged(self.export_of(MOVED), self.target))
        self.assertEqual(self.target.read_bytes(), MOVED.read_bytes())

    def test_a_new_mesh_is_written(self):
        self.assertFalse(fbx.export_keeping_unchanged(self.export_of(CUBE_A), self.target))
        self.assertEqual(self.target.read_bytes(), CUBE_A.read_bytes())

    def test_an_unreadable_file_is_never_taken_as_unchanged(self):
        self.target.write_text("version https://git-lfs.github.com/spec/v1\n")
        with self.assertRaises(fbx.FbxError):
            fbx.export_keeping_unchanged(self.export_of(CUBE_A), self.target)
        self.assertEqual(self.target.read_text(), "version https://git-lfs.github.com/spec/v1\n")
        self.assertEqual([path.name for path in Path(self.folder.name).iterdir()], [self.target.name])


if __name__ == "__main__":
    unittest.main()
