"""Regression tests for the Vanguard bodies' input hashes (ADR-064 §4): what makes a generated body stale."""
from __future__ import annotations

import copy
import importlib.util
import sys
import unittest
from pathlib import Path


SCRIPT = Path(__file__).resolve().parents[1] / "Game" / "Scripts" / "VanguardBodies" / "inputs.py"
spec = importlib.util.spec_from_file_location("vanguard_body_inputs", SCRIPT)
assert spec and spec.loader
inputs = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = inputs
spec.loader.exec_module(inputs)

KIT = {
    "fps": 30,
    "archetypes": {
        "humanoid": {"triangleBudget": 9000, "animations": {"Run": {"seconds": 0.6, "loop": True}}},
        "rider": {"triangleBudget": 16000, "animations": {"Run": {"seconds": 0.6, "loop": True}}},
    },
    "vanguards": [
        {"id": "a", "archetype": "humanoid", "seed": 1, "features": [],
         "statusBodies": [{"status": "a_ride", "name": "Ride", "body": {"archetype": "rider"}}]},
        {"id": "b", "archetype": "humanoid", "seed": 2, "features": ["cloak"]},
    ],
}
VANGUARDS = {
    "a": {"body": {"capsuleRadius": 50, "capsuleHalfHeight": 95}, "basicAttack": {"projectile": []}},
    "b": {"body": {"capsuleRadius": 40, "capsuleHalfHeight": 90}, "basicAttack": {"projectile": [{"speed": 2000}]}},
}


def built(kit, vanguards):
    """The manifest assets a full build of kit writes: one per body, each with its input hash."""
    assets = []
    for vanguard in kit["vanguards"]:
        for body, status, suffix in inputs.bodies_of(vanguard):
            asset = {"id": vanguard["id"], "name": inputs.body_name(vanguard["id"], suffix), "inputSha256": inputs.input_hash(kit, vanguards, body)}
            if status:
                asset["status"] = status
            assets.append(asset)
    return assets


class VanguardBodyInputs(unittest.TestCase):
    def test_a_full_build_is_current(self):
        self.assertEqual(inputs.stale_assets(KIT, VANGUARDS, built(KIT, VANGUARDS)), [])

    def test_a_shared_setting_changed_since_stales_every_body_that_uses_it(self):
        # One Vanguard rebuilt after the humanoid's Run timing changed: the other, kept from before, is stale.
        assets = built(KIT, VANGUARDS)
        kit = copy.deepcopy(KIT)
        kit["archetypes"]["humanoid"]["animations"]["Run"]["seconds"] = 0.5
        rebuilt = [asset for asset in built(kit, VANGUARDS) if asset["id"] == "a"]
        kept = [asset for asset in assets if asset["id"] != "a"]
        self.assertEqual(inputs.stale_assets(kit, VANGUARDS, rebuilt + kept), ["SK_B"])

    def test_the_frame_rate_stales_every_body(self):
        kit = copy.deepcopy(KIT)
        kit["fps"] = 60
        self.assertEqual(sorted(inputs.stale_assets(kit, VANGUARDS, built(KIT, VANGUARDS))), ["SK_A", "SK_A_Ride", "SK_B"])

    def test_a_capsule_or_attack_change_stales_that_vanguards_bodies_only(self):
        vanguards = copy.deepcopy(VANGUARDS)
        vanguards["a"]["body"]["capsuleHalfHeight"] = 100
        self.assertEqual(sorted(inputs.stale_assets(KIT, vanguards, built(KIT, VANGUARDS))), ["SK_A", "SK_A_Ride"])
        vanguards = copy.deepcopy(VANGUARDS)
        vanguards["b"]["basicAttack"]["projectile"] = []
        self.assertEqual(inputs.stale_assets(KIT, vanguards, built(KIT, VANGUARDS)), ["SK_B"])

    def test_a_status_body_is_its_own_entry_over_its_vanguards(self):
        # Its entries over its Vanguard's: a change to the Vanguard's own entry stales the status body too.
        kit = copy.deepcopy(KIT)
        kit["vanguards"][0]["seed"] = 7
        self.assertEqual(sorted(inputs.stale_assets(kit, VANGUARDS, built(KIT, VANGUARDS))), ["SK_A", "SK_A_Ride"])
        kit = copy.deepcopy(KIT)
        kit["vanguards"][0]["statusBodies"][0]["body"]["archetype"] = "humanoid"
        self.assertEqual(inputs.stale_assets(kit, VANGUARDS, built(KIT, VANGUARDS)), ["SK_A_Ride"])

    def test_the_preflight_reads_a_project_and_fails_on_a_stale_body(self):
        import json
        import subprocess
        import tempfile
        with tempfile.TemporaryDirectory() as folder:
            game = Path(folder)
            (game / "ArtSource" / "Vanguards").mkdir(parents=True)
            (game / "Tuning").mkdir()
            (game / "ArtSource" / "Vanguards" / "VanguardKit.json").write_text(json.dumps(KIT))
            (game / "Tuning" / "Vanguards.json").write_text(json.dumps({"vanguards": VANGUARDS}))
            manifest = game / "ArtSource" / "Vanguards" / "manifest.json"
            manifest.write_text(json.dumps({"assets": built(KIT, VANGUARDS)}))
            self.assertEqual(inputs.stale_in(game), [])
            ok = subprocess.run([sys.executable, str(SCRIPT), str(game)], capture_output=True, text=True)
            self.assertEqual(ok.returncode, 0, ok.stdout + ok.stderr)
            kit = copy.deepcopy(KIT)
            kit["fps"] = 24
            (game / "ArtSource" / "Vanguards" / "VanguardKit.json").write_text(json.dumps(kit))
            stale = subprocess.run([sys.executable, str(SCRIPT), str(game)], capture_output=True, text=True)
            self.assertEqual(stale.returncode, 1)
            self.assertIn("SK_A_Ride", stale.stdout)

    def test_a_body_without_a_recorded_input_hash_is_stale(self):
        assets = built(KIT, VANGUARDS)
        del assets[0]["inputSha256"]
        self.assertEqual(inputs.stale_assets(KIT, VANGUARDS, assets), ["SK_A"])

    def test_a_body_the_kit_has_and_the_manifest_lacks_is_stale(self):
        # A status body added to a Vanguard the last partial build left out: the manifest it kept has no such body.
        kit = copy.deepcopy(KIT)
        kit["vanguards"][1]["statusBodies"] = [{"status": "b_form", "name": "Form", "body": {"features": []}}]
        self.assertEqual(inputs.stale_assets(kit, VANGUARDS, built(KIT, VANGUARDS)), ["SK_B_Form"])
        # And one the kit no longer has is stale too, so the art set is never written from it.
        kit = copy.deepcopy(KIT)
        del kit["vanguards"][0]["statusBodies"]
        self.assertEqual(inputs.stale_assets(kit, VANGUARDS, built(KIT, VANGUARDS)), ["SK_A_Ride"])


if __name__ == "__main__":
    unittest.main()
