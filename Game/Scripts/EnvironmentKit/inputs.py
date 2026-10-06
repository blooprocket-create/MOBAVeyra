"""What the Crucible's environment meshes are made from (ADR-040): the kit without its look.

Pure Python, shared by the generator (Blender), the importer (Unreal) and CI, so all derive the same answer. The manifest
records this hash, and the importer refuses meshes generated from another. The kit's look (stone tints, moss, glyph
glow, foliage colours, wind) is read only by the importer's materials, so changing it rebuilds materials without
regenerating a mesh; any other change to the kit is a change to the meshes and needs the generator.
"""
import hashlib
import json

# The sections of CrucibleKit.json only the importer's materials read; the generator reads none of them.
MATERIAL_ONLY = ("look",)


def mesh_profile_sha256(kit):
    """The hash of everything in kit that the generator makes meshes from."""
    meshes = {key: value for key, value in kit.items() if key not in MATERIAL_ONLY}
    return hashlib.sha256(json.dumps(meshes, sort_keys=True).encode("utf-8")).hexdigest()


def stale(game):
    """The problem with the environment kit's manifest in the project whose Game folder is game, or None: its meshes
    must have been generated from what the kit gives them now."""
    from pathlib import Path
    source = Path(game) / "ArtSource" / "Environment"
    kit = json.loads((source / "CrucibleKit.json").read_text(encoding="utf-8"))
    manifest = json.loads((source / "manifest.json").read_text(encoding="utf-8"))
    if manifest.get("meshProfileSha256") != mesh_profile_sha256(kit):
        return "Regenerate the environment meshes (BuildEnvironmentArt.ps1): the kit's meshes changed since they were generated."
    return None
