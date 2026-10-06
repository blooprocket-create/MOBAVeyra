"""Import the synthesised cue sounds (ADR-063 section 5) as sound assets; BuildCueSounds.ps1 runs it in an editor commandlet.

It checks each WAV against the manifest GenerateCueSounds.py wrote, so the imported sounds are the generator's own.
VEYRA_CUE_SOUNDS_ONLY, a comma-separated list of names, imports only those, leaving the others' assets untouched.
"""
import hashlib
import json
import os
import stat
from pathlib import Path

import unreal

GAME = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
SOURCE = GAME / "ArtSource" / "Presentation"
SPEC_BYTES = (SOURCE / "CueSounds.json").read_bytes()
SPEC = json.loads(SPEC_BYTES)
MANIFEST = json.loads((SOURCE / "CueSounds.manifest.json").read_text())
DEST = SPEC["destination"]
assert DEST.startswith("/Game/Veyra/UI/"), "Cue sounds live where the UI's content is always cooked"
assert MANIFEST["specSha256"] == hashlib.sha256(SPEC_BYTES).hexdigest(), "Run GenerateCueSounds.py after changing the spec"
assert [entry["name"] for entry in MANIFEST["sounds"]] == [sound["name"] for sound in SPEC["sounds"]], "The manifest is stale"
ONLY = [name for name in os.environ.get("VEYRA_CUE_SOUNDS_ONLY", "").split(",") if name]
assert all(name in [entry["name"] for entry in MANIFEST["sounds"]] for name in ONLY), "VEYRA_CUE_SOUNDS_ONLY names a sound the spec lacks"
CHOSEN = [entry for entry in MANIFEST["sounds"] if not ONLY or entry["name"] in ONLY]

# Validate every source and target before changing any asset.
destination_disk = GAME / "Content" / DEST.removeprefix("/Game/")
for entry in CHOSEN:
    source = (SOURCE / entry["file"]).resolve()
    assert source.is_relative_to(SOURCE.resolve()), "A WAV path escapes the source folder"
    assert hashlib.sha256(source.read_bytes()).hexdigest() == entry["sha256"], entry["name"] + " differs from its manifest"
    target = destination_disk / (entry["name"] + ".uasset")
    if target.exists() and getattr(target.stat(), "st_file_attributes", 0) & stat.FILE_ATTRIBUTE_READONLY:
        raise RuntimeError("Acquire the Git LFS lock before reimporting: " + str(target))

tools = unreal.AssetToolsHelpers.get_asset_tools()
for entry in CHOSEN:
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", str(SOURCE / entry["file"]))
    task.set_editor_property("destination_path", DEST)
    task.set_editor_property("destination_name", entry["name"])
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("save", False)
    task.set_editor_property("factory", unreal.SoundFactory())
    tools.import_asset_tasks([task])
    asset = unreal.load_asset(DEST + "/" + entry["name"])
    assert isinstance(asset, unreal.SoundWave), entry["name"] + " did not import as a sound"
    assert unreal.EditorAssetLibrary.save_loaded_asset(asset), "Save failed: " + entry["name"]
    unreal.log("VEYRA_CUE_SOUND_ASSET: " + asset.get_path_name())
unreal.log("VEYRA_CUE_SOUNDS_PASSED: " + str(len(CHOSEN)) + " sound(s)")
