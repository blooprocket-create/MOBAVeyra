"""Tests for what a generated kit material is built from (ADR-040, ADR-064 §4): an importer keeps the material while its
inputs hash the same, and rebuilds it when its values, the code that builds it or what it links to change."""
from __future__ import annotations

import importlib.util
import json
import sys
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / "Game" / "Scripts" / "KitMaterials" / "inputs.py"
spec = importlib.util.spec_from_file_location("kit_materials_inputs", SCRIPT)
assert spec and spec.loader
inputs = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = inputs
spec.loader.exec_module(inputs)

BODY_MATERIAL = json.loads((ROOT / "Game" / "ArtSource" / "Vanguards" / "VanguardKit.json").read_text(encoding="utf-8"))["bodyMaterial"]
BINDINGS = {"collection": "/Game/Veyra/Vanguards/MPC_VeyraToonLight.MPC_VeyraToonLight", "parameters": ["SunColor", "ToSun"]}


class MaterialInputsHashTests(unittest.TestCase):
    def setUp(self):
        self.folder = tempfile.TemporaryDirectory()
        self.addCleanup(self.folder.cleanup)
        self.graph = Path(self.folder.name) / "body_material.py"
        self.helpers = Path(self.folder.name) / "material_graph.py"
        self.graph.write_bytes(b"def build():\n    pass\n")
        self.helpers.write_bytes(b"class Graph:\n    pass\n")

    def hash(self, values=None, bindings=None, sources=None):
        return inputs.material_inputs_hash(BODY_MATERIAL if values is None else values, sources or [self.graph, self.helpers],
                                           BINDINGS if bindings is None else bindings)

    def test_the_same_inputs_hash_the_same(self):
        self.assertEqual(self.hash(), self.hash())
        self.assertEqual(self.hash(), self.hash(values=json.loads(json.dumps(BODY_MATERIAL))))

    def test_key_order_and_source_order_do_not_matter(self):
        reordered = dict(reversed(list(BODY_MATERIAL.items())))
        self.assertEqual(self.hash(), self.hash(values=reordered))
        self.assertEqual(self.hash(), self.hash(sources=[self.helpers, self.graph]))

    def test_line_endings_do_not_matter(self):
        before = self.hash()
        self.graph.write_bytes(self.graph.read_bytes().replace(b"\n", b"\r\n"))
        self.assertEqual(before, self.hash())

    def test_a_changed_value_changes_it(self):
        changed = json.loads(json.dumps(BODY_MATERIAL))
        changed["toon"]["rimStrength"] = changed["toon"]["rimStrength"] + 0.5
        self.assertNotEqual(self.hash(), self.hash(values=changed))

    def test_changed_code_changes_it(self):
        for source in (self.graph, self.helpers):
            before = self.hash()
            source.write_bytes(source.read_bytes() + b"# another line\n")
            self.assertNotEqual(before, self.hash(), source.name)

    def test_code_moved_between_files_changes_it(self):
        before = self.hash()
        graph, helpers = self.graph.read_bytes(), self.helpers.read_bytes()
        self.graph.write_bytes(helpers)
        self.helpers.write_bytes(graph)
        self.assertNotEqual(before, self.hash())

    def test_changed_bindings_change_it(self):
        self.assertNotEqual(self.hash(), self.hash(bindings=dict(BINDINGS, collection="/Game/Veyra/Other/MPC_Other.MPC_Other")))
        self.assertNotEqual(self.hash(), self.hash(bindings=dict(BINDINGS, parameters=["ToSun"])))

    def test_a_material_is_kept_only_when_its_recorded_hash_is_current(self):
        current = self.hash()
        self.assertTrue(inputs.kept(current, current))
        self.assertFalse(inputs.kept("", current), "a material with no recorded hash is rebuilt")
        self.assertFalse(inputs.kept(self.hash(bindings=dict(BINDINGS, parameters=[])), current))

    def test_the_vanguard_body_material_hashes_from_its_committed_sources(self):
        scripts = ROOT / "Game" / "Scripts"
        sources = [scripts / "vanguard_body_material.py", scripts / "veyra_material_graph.py"]
        self.assertEqual(inputs.material_inputs_hash(BODY_MATERIAL, sources, BINDINGS), inputs.material_inputs_hash(BODY_MATERIAL, sources, BINDINGS))


if __name__ == "__main__":
    unittest.main()
