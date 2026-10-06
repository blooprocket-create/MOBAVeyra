"""Regression tests for the Vanguard bodies' input hashes (ADR-064 §4): what makes a generated body stale."""
from __future__ import annotations

import copy
import hashlib
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
    "blender": "5.2",
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


SCRIPTS = SCRIPT.parents[1]
GENERATOR = inputs.generator_hash(SCRIPTS)
BLENDER = "5.2.0"


def built(kit, vanguards, generator=GENERATOR, blender=BLENDER):
    """The manifest assets a full build of kit writes: one per body, each with its input hash and what built it."""
    assets = []
    for vanguard in kit["vanguards"]:
        for body, status, suffix in inputs.bodies_of(vanguard):
            name = inputs.body_name(vanguard["id"], suffix)
            asset = {"id": vanguard["id"], "name": name, "inputSha256": inputs.input_hash(kit, vanguards, body),
                     "generatorSha256": generator, "blender": blender,
                     # Its FBX, written by write_fbx: the body's name as its bytes.
                     "file": "FBX/" + name + ".fbx", "sha256": hashlib.sha256(name.encode()).hexdigest()}
            if status:
                asset["status"] = status
            assets.append(asset)
    return assets


def write_fbx(game, assets):
    """Each asset's FBX, as built writes it into the project whose Game folder is game."""
    (game / "ArtSource" / "Vanguards" / "FBX").mkdir(parents=True, exist_ok=True)
    for asset in assets:
        (game / "ArtSource" / "Vanguards" / asset["file"]).write_bytes(asset["name"].encode())


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
            self.copy_generator(game)
            (game / "ArtSource" / "Vanguards" / "VanguardKit.json").write_text(json.dumps(KIT))
            (game / "Tuning" / "Vanguards.json").write_text(json.dumps({"vanguards": VANGUARDS}))
            manifest = game / "ArtSource" / "Vanguards" / "manifest.json"
            manifest.write_text(json.dumps({"blender": BLENDER, "assets": built(KIT, VANGUARDS)}))
            write_fbx(game, built(KIT, VANGUARDS))
            self.assertEqual(inputs.stale_in(game), [])
            ok = subprocess.run([sys.executable, str(SCRIPT), str(game)], capture_output=True, text=True)
            self.assertEqual(ok.returncode, 0, ok.stdout + ok.stderr)
            kit = copy.deepcopy(KIT)
            kit["fps"] = 24
            (game / "ArtSource" / "Vanguards" / "VanguardKit.json").write_text(json.dumps(kit))
            stale = subprocess.run([sys.executable, str(SCRIPT), str(game)], capture_output=True, text=True)
            self.assertEqual(stale.returncode, 1)
            self.assertIn("SK_A_Ride", stale.stdout)

    def project(self, folder, blender=BLENDER):
        """A project in folder with the fixture's kit, capsules, generator and a full build's manifest and FBX."""
        import json
        game = Path(folder)
        (game / "ArtSource" / "Vanguards").mkdir(parents=True)
        (game / "Tuning").mkdir()
        self.copy_generator(game)
        (game / "ArtSource" / "Vanguards" / "VanguardKit.json").write_text(json.dumps(KIT))
        (game / "Tuning" / "Vanguards.json").write_text(json.dumps({"vanguards": VANGUARDS}))
        assets = built(KIT, VANGUARDS, blender=blender)
        (game / "ArtSource" / "Vanguards" / "manifest.json").write_text(json.dumps({"blender": blender, "assets": assets}))
        write_fbx(game, assets)
        return game

    def test_the_preflight_names_a_missing_or_changed_fbx_and_reads_a_pointer_for_its_file(self):
        import subprocess
        import tempfile
        with tempfile.TemporaryDirectory() as folder:
            game = self.project(folder)
            fbx = game / "ArtSource" / "Vanguards" / "FBX"
            # A checkout without the large files (CI): each FBX is its Git LFS pointer, which names the same SHA-256.
            for path in fbx.glob("*.fbx"):
                oid = hashlib.sha256(path.read_bytes()).hexdigest()
                path.write_text("version https://git-lfs.github.com/spec/v1\noid sha256:" + oid + "\nsize 4\n")
            self.assertEqual(inputs.committed_mismatches(game), [])
            # One replaced without the manifest, one gone.
            (fbx / "SK_A.fbx").write_bytes(b"other")
            (fbx / "SK_B.fbx").unlink()
            self.assertEqual(sorted(inputs.committed_mismatches(game)), ["SK_A", "SK_B"])
            failed = subprocess.run([sys.executable, str(SCRIPT), str(game)], capture_output=True, text=True)
            self.assertEqual(failed.returncode, 1)
            self.assertIn("SK_B", failed.stdout)

    def test_a_manifest_built_by_another_blender_than_the_pinned_one_is_all_stale(self):
        import tempfile
        with tempfile.TemporaryDirectory() as folder:
            # Built whole and consistent, but by a release the kit does not pin.
            game = self.project(folder, blender="5.3.0")
            self.assertEqual(inputs.stale_in(game), ["SK_A", "SK_A_Ride", "SK_B"])
        with tempfile.TemporaryDirectory() as folder:
            # A patch release of the pinned one is that release.
            self.assertEqual(inputs.stale_in(self.project(folder, blender="5.2.1")), [])

    def test_a_build_drops_the_bodies_the_kit_no_longer_makes_and_only_among_those_it_built(self):
        previous = built(KIT, VANGUARDS)
        # The ride body renamed (a new name for the same status) and Vanguard b removed.
        kit = copy.deepcopy(KIT)
        kit["vanguards"][0]["statusBodies"][0]["name"] = "Mount"
        del kit["vanguards"][1]
        current = built(kit, VANGUARDS)
        self.assertEqual([asset["name"] for asset in inputs.removed_assets(previous, current)], ["SK_A_Ride", "SK_B"])
        # A build of a alone keeps b's bodies, which it did not make.
        self.assertEqual([asset["name"] for asset in inputs.removed_assets(previous, current, only={"a"})], ["SK_A_Ride"])

    def test_what_a_build_leaves_to_import_waits_until_an_import_takes_it(self):
        assets = built(KIT, VANGUARDS)
        # A build that only generated (no import) changed a; a later build changes nothing more, but a is still to import.
        self.assertEqual(inputs.pending_changed([], ["a"], assets), ["a"])
        self.assertEqual(inputs.pending_changed(["a"], [], assets), ["a"])
        self.assertEqual(inputs.pending_changed(["a"], ["b", "a"], assets), ["a", "b"])
        # A Vanguard the kit has since dropped has nothing left to import.
        self.assertEqual(inputs.pending_changed(["gone", "b"], [], assets), ["b"])
        # A dropped body's imported assets are still to delete after a build that only generated, unless the body is
        # back (imported again, which replaces its folder).
        self.assertEqual(inputs.pending_removed([], ["SK_Gone"], assets), ["SK_Gone"])
        self.assertEqual(inputs.pending_removed(["SK_Gone"], [], assets), ["SK_Gone"])
        self.assertEqual(inputs.pending_removed(["SK_Gone", "SK_A_Ride"], ["SK_Old"], assets), ["SK_Gone", "SK_Old"])

    @staticmethod
    def copy_generator(game):
        """The generator's code, as the project beside its manifest holds it."""
        import shutil
        (game / "Scripts" / "VanguardBodies").mkdir(parents=True)
        shutil.copy(SCRIPTS / "GenerateVanguardBodies.py", game / "Scripts")
        for module in (SCRIPTS / "VanguardBodies").glob("*.py"):
            shutil.copy(module, game / "Scripts" / "VanguardBodies")

    def test_a_body_built_by_other_generator_code_or_another_blender_is_stale(self):
        # A selective build after the generator's code changed keeps bodies the old code built.
        self.assertEqual(sorted(inputs.stale_assets(KIT, VANGUARDS, built(KIT, VANGUARDS, generator="old"), GENERATOR, BLENDER)),
                         ["SK_A", "SK_A_Ride", "SK_B"])
        # And one under another Blender than the manifest was last built by.
        assets = built(KIT, VANGUARDS)
        assets[2]["blender"] = "5.1.0"
        self.assertEqual(inputs.stale_assets(KIT, VANGUARDS, assets, GENERATOR, BLENDER), ["SK_B"])
        self.assertEqual(inputs.stale_assets(KIT, VANGUARDS, built(KIT, VANGUARDS), GENERATOR, BLENDER), [])

    def test_the_preflight_fails_when_the_generator_code_changes_but_not_for_its_line_endings(self):
        import json
        import subprocess
        import tempfile
        with tempfile.TemporaryDirectory() as folder:
            game = Path(folder)
            (game / "ArtSource" / "Vanguards").mkdir(parents=True)
            (game / "Tuning").mkdir()
            self.copy_generator(game)
            (game / "ArtSource" / "Vanguards" / "VanguardKit.json").write_text(json.dumps(KIT))
            (game / "Tuning" / "Vanguards.json").write_text(json.dumps({"vanguards": VANGUARDS}))
            (game / "ArtSource" / "Vanguards" / "manifest.json").write_text(json.dumps({"blender": BLENDER, "assets": built(KIT, VANGUARDS)}))
            write_fbx(game, built(KIT, VANGUARDS))
            # The same code checked out with other line endings is the same code.
            module = game / "Scripts" / "VanguardBodies" / "parts.py"
            module.write_bytes(module.read_bytes().replace(b"\r\n", b"\n").replace(b"\n", b"\r\n"))
            self.assertEqual(inputs.stale_in(game), [])
            # Changed code is not.
            module.write_bytes(module.read_bytes() + b"\n# changed\n")
            stale = subprocess.run([sys.executable, str(SCRIPT), str(game)], capture_output=True, text=True)
            self.assertEqual(stale.returncode, 1)
            self.assertIn("SK_B", stale.stdout)

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
