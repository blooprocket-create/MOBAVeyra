"""Moss detail controls preserve deterministic, finite source textures and reduce local contrast."""
import importlib.util
import json
from pathlib import Path
import unittest

import numpy as np

ROOT = Path(__file__).resolve().parents[1]
module_spec = importlib.util.spec_from_file_location('terrain_recipe', ROOT/'Game/Scripts/GenerateTerrainTextures.py')
recipe = importlib.util.module_from_spec(module_spec)
module_spec.loader.exec_module(recipe)


class MossRecipeTests(unittest.TestCase):
    def setUp(self):
        profile = json.loads((ROOT/'Game/ArtSource/Environment/Terrain/TerrainTextures.json').read_text())
        self.spec = next(layer for layer in profile['layers'] if layer['id'] == 'Moss')

    def render(self, spec):
        return recipe.moss(spec, 128, np.random.default_rng(48173))

    def test_deterministic_finite_output(self):
        for first, second in zip(self.render(self.spec), self.render(self.spec)):
            np.testing.assert_array_equal(first, second)
            self.assertTrue(np.isfinite(first).all())

    def test_reduced_cushions_reduce_local_colour_and_height_contrast(self):
        quiet = self.render(dict(self.spec, cushionContrast=0.10, cushionRelief=0.20))
        strong = self.render(dict(self.spec, cushionContrast=0.35, cushionRelief=0.60))
        for index in (0, 1):
            energy = lambda data: sum(np.mean(np.diff(data, axis=axis)**2) for axis in (0, 1))
            self.assertLess(energy(quiet[index]), energy(strong[index]))

    def test_invalid_controls_are_rejected(self):
        for field in ('cushionContrast', 'cushionRelief'):
            for value in (-0.1, 1.1, float('nan'), True):
                with self.subTest(field=field, value=value), self.assertRaises(ValueError):
                    self.render(dict(self.spec, **{field: value}))


if __name__ == '__main__':
    unittest.main()
