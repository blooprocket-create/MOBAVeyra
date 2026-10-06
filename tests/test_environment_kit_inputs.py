"""Tests for what the Crucible's environment meshes are made from (ADR-040): the committed manifest matches the kit, and
only the kit's look, which the importer's materials alone read, can change without regenerating the meshes."""
from __future__ import annotations

import copy
import importlib.util
import json
import sys
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
GAME = ROOT / "Game"
SCRIPT = GAME / "Scripts" / "EnvironmentKit" / "inputs.py"
spec = importlib.util.spec_from_file_location("environment_kit_inputs", SCRIPT)
assert spec and spec.loader
inputs = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = inputs
spec.loader.exec_module(inputs)

KIT = json.loads((GAME / "ArtSource" / "Environment" / "CrucibleKit.json").read_text(encoding="utf-8"))


class EnvironmentKitInputs(unittest.TestCase):
    def test_the_committed_meshes_were_made_from_the_committed_kit(self):
        self.assertIsNone(inputs.stale(GAME))

    def test_the_look_is_not_a_mesh_input(self):
        changed = copy.deepcopy(KIT)
        changed["look"]["glyphStrength"] += 1.0
        changed["look"]["rockTint"] = [1.0, 1.0, 1.0]
        self.assertEqual(inputs.mesh_profile_sha256(changed), inputs.mesh_profile_sha256(KIT))

    def test_a_family_or_the_seed_is(self):
        family = copy.deepcopy(KIT)
        family["families"][0]["variants"] += 1
        seeded = dict(KIT, seed=KIT["seed"] + 1)
        for changed in (family, seeded):
            self.assertNotEqual(inputs.mesh_profile_sha256(changed), inputs.mesh_profile_sha256(KIT))


if __name__ == "__main__":
    unittest.main()
