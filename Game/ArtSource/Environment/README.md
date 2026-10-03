# Crucible environment kit

Veyra-authored source geometry. `CrucibleKit.json` owns family dimensions, seed, materials and geometry settings; it contains no map coordinates or gameplay collision. The generator is `Game/Scripts/GenerateEnvironmentMeshes.py`. No third-party mesh or texture content is incorporated into these assets.

Run `Game/Scripts/BuildEnvironmentArt.ps1 -Blender <executable>` from the repository root to regenerate the FBX family, source manifest, Unreal meshes and materials. Use `-ImportOnly` to import unchanged FBX. The source manifest records the profile hash, file hashes and geometry digests; the Unreal report is written to `Game/Saved/EnvironmentKit/unreal-validation.json`.

The generator checks finite geometry, positive triangle area, closed rock topology, normalized transforms and two UV channels. Import checks dimensions, UV channels and the absence of gameplay collision. Materials explicitly support instanced static meshes so baked PCG resources retain their materials in packaged play. New mesh families still require visual, scalability and performance review under the World Validation Standard.

Generated assets belong in `Game/Content/Veyra/World/Environment`. Existing tracked binary assets require the repository's LFS lock before replacement. Never edit FBX or UAssets as an independent authoring source.
