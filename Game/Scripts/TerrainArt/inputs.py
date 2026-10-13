"""What the Crucible's terrain importer builds each asset from, and whether it keeps a texture asset as it is (ADR-040 §3).

Pure Python, so CI checks it without Unreal (tests/test_terrain_art_inputs.py). ImportTerrainArt.py records the SHA-256
of the PNG it imported a texture from on the asset (its metadata, under SOURCE_TAG) and keeps the asset untouched,
neither imported nor saved, while that is the hash of the PNG now and the asset still has the settings it would be given:
a reimport of the same PNG still writes a changed file, a new version of a lockable Git LFS asset for no change. Each
material is kept the same way while the hash of what it is built from (KitMaterials/inputs.py) is unchanged; its values
are those below, all its graph is given.
"""

# The asset metadata tag the importer records a texture's source PNG hash under.
SOURCE_TAG = "VeyraSourceSha256"

# What of the profile's water only its texture is made from (GenerateTerrainTextures.py): not given to its material, so a
# change to it reimports the texture without rebuilding the material.
WATER_TEXTURE_ONLY = ("rippleStrength",)


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


def material_bindings(paths):
    """What of other assets a material's graph links to (KitMaterials/inputs.py bindings), from paths, each texture it
    samples by name: the texture's path, and the settings its sampler relies on (a normal sampler needs a normal map, so
    a change to them must recompile the material)."""
    return {name: dict(texture_settings(name), path=path) for name, path in paths.items()}


def terrain_material_values(profile):
    """All of the profile M_CrucibleTerrain's graph is given (crucible_terrain_material.py): each layer's tile size, the
    variation's, its look and its overgrowth. A layer's recipe makes only its textures, so a change to one reimports
    them without rebuilding the material, which samples them by path."""
    return {"tileMetres": {layer["id"]: layer["tileMetres"] for layer in profile["layers"]},
            "macroTileMetres": profile["macroTileMetres"], "look": profile["material"], "overgrowth": profile["overgrowth"]}


def water_material_values(profile):
    """All of the profile M_CrucibleWater's graph is given (crucible_water_material.py): its water, but for what only
    the water's texture is made from. Its roughness and specular must be in [0, 1]."""
    water = {key: value for key, value in profile["water"].items() if key not in WATER_TEXTURE_ONLY}
    for control in ("roughness", "specular"):
        assert isinstance(water[control], (int, float)) and 0.0 <= water[control] <= 1.0, f"water.{control} must be in [0, 1]"
    return water
