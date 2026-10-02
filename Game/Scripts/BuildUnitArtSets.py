"""Writes each art kit's unit art set: a UVeyraUnitArtSet Data Asset holding the kit's meshes by the stable IDs
of what they dress (ADR-006 §6), intact and fallen, and the material slot whose Flux takes each unit's colour.

It reads the kit's manifest and settings only; it changes no mesh or material. BuildUnitArtSets.ps1 runs it in
an editor commandlet once the kit's meshes are imported.
"""
import json
from pathlib import Path

import unreal

GAME = Path(__file__).resolve().parents[1]
# Each kit: its source folder, its settings file, where its meshes import to, and its art set's name there.
KITS = [
    {"source": "Structures", "settings": "StructureKit.json", "content": "/Game/Veyra/World/Structures/Greybox", "asset": "DA_StructureArt"},
]
# A manifest's states: what stands intact, and what is left once a unit falls.
INTACT = {"Standing", "Active"}
FALLEN = {"Destroyed", "Collapsed"}
# The colour parameter the importers give a kit's emissive (Flux) material.
FLUX_PARAMETER = "FluxTint"

TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
written = 0
for kit in KITS:
    source = GAME / "ArtSource" / kit["source"]
    manifest = json.loads((source / "manifest.json").read_text())
    settings = json.loads((source / kit["settings"]).read_text())
    flux = [m["name"] for m in settings["materials"] if m["emission"]]
    assert len(flux) == 1, (kit["source"], "needs exactly one emissive Flux material", flux)

    art = {}
    for entry in manifest["assets"]:
        mesh = unreal.load_asset(kit["content"] + "/Meshes/" + entry["name"])
        assert isinstance(mesh, unreal.StaticMesh), entry["name"] + " is not imported"
        state = entry["state"]
        assert state in INTACT | FALLEN, (entry["name"], state)
        pair = art.setdefault(entry["kind"], {})
        key = "intact" if state in INTACT else "fallen"
        assert key not in pair, (entry["kind"], "has two " + key + " meshes")
        pair[key] = mesh
    for kind, pair in art.items():
        assert set(pair) == {"intact", "fallen"}, (kind, "needs an intact and a fallen mesh")

    path = kit["content"] + "/" + kit["asset"]
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        asset = unreal.load_asset(path)
    else:
        factory = unreal.DataAssetFactory()
        factory.set_editor_property("data_asset_class", unreal.VeyraUnitArtSet)
        asset = TOOLS.create_asset(kit["asset"], kit["content"], unreal.VeyraUnitArtSet, factory)
    assert isinstance(asset, unreal.VeyraUnitArtSet), path
    entries = {}
    for kind, pair in sorted(art.items()):
        unit = unreal.VeyraUnitArt()
        unit.set_editor_property("intact", pair["intact"])
        unit.set_editor_property("fallen", pair["fallen"])
        entries[unreal.Name(kind)] = unit
    asset.set_editor_property("art", entries)
    asset.set_editor_property("flux_slot", unreal.Name(flux[0]))
    asset.set_editor_property("flux_parameter", unreal.Name(FLUX_PARAMETER))
    assert unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False), "Save failed: " + path
    unreal.log("VEYRA_ART_SET: " + path + " dresses " + ", ".join(sorted(art)))
    written += 1

unreal.log("VEYRA_ART_SETS_PASSED: " + str(written) + " art set(s)")
