"""Whether the Crucible's terrain importer keeps a texture asset as it is (ADR-040 §3).

Pure Python, so CI checks it without Unreal (tests/test_terrain_art_inputs.py). ImportTerrainArt.py records the SHA-256
of the PNG it imported a texture from on the asset (its metadata, under SOURCE_TAG) and keeps the asset untouched,
neither imported nor saved, while that is the hash of the PNG now and the asset still has the settings it would be given:
a reimport of the same PNG still writes a changed file, a new version of a lockable Git LFS asset for no change.
"""

# The asset metadata tag the importer records a texture's source PNG hash under.
SOURCE_TAG = "VeyraSourceSha256"


def texture_settings(name):
    """The settings the importer gives the texture called name, by the family its name ends with: the property names of
    its compression, whether it is sRGB colour and its LOD group, each with the name of its value."""
    if name.endswith("_WaterNormal"):
        # Ripple normals with the shore's foam in alpha: kept whole, so not compressed as a normal map.
        return {"compression_settings": "TC_BC7", "srgb": False, "lod_group": "TEXTUREGROUP_WORLD"}
    if name.endswith("_Normal"):
        return {"compression_settings": "TC_NORMALMAP", "srgb": False, "lod_group": "TEXTUREGROUP_WORLD_NORMAL_MAP"}
    if name.endswith("_BaseColor"):
        return {"compression_settings": "TC_DEFAULT", "srgb": True, "lod_group": "TEXTUREGROUP_WORLD"}
    # Packed data with alpha (ambient occlusion, roughness, metallic, height), or the macro variation: linear.
    return {"compression_settings": "TC_BC7", "srgb": False, "lod_group": "TEXTUREGROUP_WORLD"}


def kept(recorded, source_sha256, settings, wanted):
    """Whether a texture asset stays as it is: the source hash it records (empty when it has none) is source_sha256, the
    hash of the PNG it would be imported from now, and its settings are wanted, those an import would give it."""
    return bool(recorded) and recorded == source_sha256 and settings == wanted
