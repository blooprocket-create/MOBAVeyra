"""The causeway's paving is deterministic, finite, and lays its setts and gaps as its profile asks (ADR-073 §5)."""
import importlib.util
import json
from pathlib import Path
import unittest

import numpy as np

ROOT = Path(__file__).resolve().parents[1]
module_spec = importlib.util.spec_from_file_location('terrain_recipe', ROOT / 'Game/Scripts/GenerateTerrainTextures.py')
recipe = importlib.util.module_from_spec(module_spec)
module_spec.loader.exec_module(recipe)


class PavingRecipeTests(unittest.TestCase):
    def setUp(self):
        profile = json.loads((ROOT / 'Game/ArtSource/Environment/Terrain/TerrainTextures.json').read_text())
        self.spec = next(layer for layer in profile['layers'] if layer['id'] == 'Causeway')

    def render(self, spec):
        return recipe.flagstones(spec, 128, np.random.default_rng(48173))

    def test_deterministic_finite_output(self):
        for first, second in zip(self.render(self.spec), self.render(self.spec)):
            np.testing.assert_array_equal(first, second)
            self.assertTrue(np.isfinite(first).all())

    def test_a_gone_flag_shows_earth_only_when_asked(self):
        # The share of gaps draws nothing of its own, so two renders differ only where a flag is gone: there the ground
        # is as rough as the paving gets. With no share, nothing differs.
        def render(share):
            return self.render(dict(self.spec, missingShare=share, settShare=0.0))

        none, again, some = render(0.0), render(0.0), render(0.4)
        np.testing.assert_array_equal(none[2], again[2])
        changed = np.any(none[0] != some[0], axis=-1)
        self.assertGreater(changed.mean(), 0.0)
        self.assertTrue(np.all(some[2][changed] == self.spec['roughness'][1]))

    def test_setts_add_joints(self):
        # Setts split flags into smaller stones, so more of the paving is joint.
        def joints(spec):
            _, _, roughness = self.render(spec)
            return np.mean(roughness > spec['roughness'][0] + 0.8 * (spec['roughness'][1] - spec['roughness'][0]))

        self.assertGreater(joints(dict(self.spec, settShare=0.6, missingShare=0.0)), joints(dict(self.spec, settShare=0.0, missingShare=0.0)))


if __name__ == '__main__':
    unittest.main()
