# Crucible visual review — 2026-10-09

Original 1920 × 1080 Unreal editor captures, seed 48173. These images are validation evidence, not authoring inputs. Veyra-authored generated world assets; no third-party visual content was introduced. Image bytes are unmodified; hashes and original capture locations are in `evidence.json`.

| Scope | Before | After |
|---|---|---|
| Foliage forms, Play_Jungle_A0 | ![Before](foliage-before.png) | ![After](foliage-after.png) |
| Skylight fill, Play_Jungle_A0 | ![Before](lighting-before.png) | ![After](lighting-after.png) |
| River specular, Review_Overview | ![Before](water-before.png) | ![After](water-after.png) |

Pairs isolate their respective passes; they do not represent the entire change from the original baseline. Water animation phases are not synchronized. The overview fraction above 98% display luminance fell from 0.50% to 0.17%; strong glare still remains on the upper bend.

## Validation limits

- Lighting review: 66 high-profile cameras captured, 60 with matching pre/post transforms. Six Spire cameras moved during map regeneration and were excluded from direct comparisons. Six low-profile cameras also captured.
- Final water review: eight matching high-profile pairs and eight current low-profile views; map, World.json and all 14 source texture hashes unchanged in that pass.
- Map regeneration validation passed before/after. Mid Team A measured route length changed from 12791 to 12784 cm; do not infer byte-identical geometry from a passing validator. World.json is unchanged.
- Low overview has checkerboard patches on the outer rim. Introduction and cause are unverified; this is not full low-profile acceptance.
- Live combat, temporal quality, GPU budgets, cook/package and final art acceptance remain pending. Cliff repetition, uniform bank dressing and residual water glare remain open.

![Low-profile overview: unresolved outer-rim checkerboard patches](low-overview-open-issue.png)

## Reproduction and ownership

See [Asset and VFX Pipeline](../../../../Docs/Production/VEYRA_ASSET_AND_VFX_PIPELINE.md), [World Production Bible](../../../../Docs/Production/VEYRA_WORLD_PRODUCTION_BIBLE.md) and [World Validation Standard](../../../../Docs/Production/VEYRA_WORLD_VALIDATION_STANDARD.md).

The source owners are `Game/ArtSource/Environment/CrucibleKit.json`, `Terrain/TerrainTextures.json`, their generators/importers, and `Game/Plugins/VeyraWorldTools/Config/CrucibleStyle.json`. All-world art presentation is affected; protected gameplay layout is unchanged. Regenerate with `BuildTerrainArt.ps1`, `BuildEnvironmentArt.ps1 -Blender <executable>`, then `BuildBattlegroundMap.ps1`, `ValidateBattleground.ps1` and `CaptureBattleground.ps1` on high/low. These script names are under `Game/Scripts/`. Water-only iterations support `BuildTerrainArt.ps1 -WaterOnly`.
