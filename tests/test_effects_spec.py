"""Tests for the presentation effects' spec (ADR-063 §4): each system it builds, and each emitter in it, is named once."""
from __future__ import annotations

import json
import unittest
from collections import Counter
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SPEC_FILE = ROOT / "Game" / "ArtSource" / "Presentation" / "Effects.json"
SHIPPED = json.loads(SPEC_FILE.read_text(encoding="utf-8"))


def repeated(names):
    return sorted(name for name, count in Counter(names).items() if count > 1)


class EffectsSpec(unittest.TestCase):
    def test_each_system_is_named_once(self):
        # A name is the asset it builds: two specs of one name would rebuild the same package, the last one winning.
        self.assertEqual(repeated(system["name"] for system in SHIPPED["systems"]), [])

    def test_each_emitter_is_named_once_in_its_system(self):
        for system in SHIPPED["systems"]:
            with self.subTest(system=system["name"]):
                self.assertEqual(repeated(emitter["name"] for emitter in system.get("emitters", [])), [])


if __name__ == "__main__":
    unittest.main()
