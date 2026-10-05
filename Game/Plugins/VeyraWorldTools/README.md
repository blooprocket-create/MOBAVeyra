# Crucible world authoring

Authority: [ADR-040](../../../Docs/ADR/ADR-040-crucible-world-authoring-toolchain.md), [World Production Bible](../../../Docs/Production/VEYRA_WORLD_PRODUCTION_BIBLE.md), and Battleground Bible section 2. `VeyraWorldTools` is an editor-only, sealed module. Gameplay does not depend on it. The existing battleground commandlet requests generation through `IVeyraWorldAuthoring`; an absent generator fails the build instead of silently producing a flat map.

## Sources and outputs

- `Game/Tuning/World.json`, schema 6: spatial authority. Surface search limits, terrain levels and runs, the river (Team A's half of its main channel and each Well island's side channel, controls with full widths), lanes, bases, objectives, camps, fog and walls live here. Team B's half is Team A's turned half a turn about the centre.
- `Config/CrucibleStyle.json`: non-spatial sampling, lighting, cameras and dressing parameters. It contains no duplicate map coordinates.
- `Game/ArtSource/Environment/CrucibleKit.json`: deterministic mesh-family source; no map layout.
- `Game/ArtSource/Environment/Terrain/TerrainTextures.json`: the terrain's and the river's look (seeded tileable textures, the water's colour); `BuildTerrainArt.ps1` generates the textures and builds `M_CrucibleTerrain` and `M_CrucibleWater` from it.
- `Game/Content/Veyra/World/Maps/L_Battleground.umap`: generated Landscape (on the playable-ground channel), layer weights, the river's surface, baked PCG instances, lights, review cameras, starts, navigation bounds and runtime marker. Never hand-patch the generated map.
- `Game/Saved/WorldGeneration/manifest.json`: successful generation record with source and output hashes; generation is not acceptance.
- `Game/Saved/WorldGeneration/Regions`: generated per-pass seed, mesh, count and normalized placement digest. These are evidence, not authoring inputs.

The river is one curve through the centre, Team A's authored half joined to its rotation, with a side channel parting and rejoining it around each Flux Well so each Well stands on an island. `FVeyraRiverShape` samples every channel once; gameplay classification, the terrain field, the river's surface and the minimap use the same samples and full widths. The surface is one generated mesh clipped to the water (ADR-040 §6, as amended 2026-10-05), shaded by `M_CrucibleWater` (Single Layer Water); it has no collision and never affects navigation. The World-owned `FVeyraTerrainField` supplies the basin and banks, level roads, jungle shelves, base pads, Well islands, wall ridges and the rim beyond the floor; any style relief is averaged with its rotation. The Landscape import quantizes this surface using Unreal's height encoding. Generation enables commandlet rendering and finishes Landscape edit-layer composition before saving; a NullRHI-only save does not produce finished weight layers in the pinned engine.

Map shelves retain server-owned terrain collision and terrain sight occlusion. Their generated rocks are presentation only; a replicated map-terrain flag prevents duplicate greybox blocks. Ability-created walls retain their own presentation. All dressing is non-colliding and cannot affect navigation. Macro, medium and micro passes use independent deterministic seeds; the generator excludes protected footprints (`VeyraDressing::Allows`) and turns each placement half a turn onto the other team's side. PCG resources are baked and their source components removed, so packaged play does not run PCG generation.

## Reproduce

From the repository root:

```powershell
./Game/Scripts/Build.ps1 -Target VeyraEditor
./Game/Scripts/BuildTerrainArt.ps1
./Game/Scripts/BuildEnvironmentArt.ps1 -Blender '<Blender executable>'
git lfs lock Game/Content/Veyra/World/Maps/L_Battleground.umap
./Game/Scripts/BuildBattlegroundMap.ps1
./Game/Scripts/CaptureBattleground.ps1 -Profile high
./Game/Scripts/CaptureBattleground.ps1 -Profile low
./Game/Scripts/Test.ps1 -Filter Veyra.World
```

An existing lock owned by the current account may be retained. Respect another owner's lock. The scripts resolve the source-built engine through the repository's normal engine association. `-ImportOnly` reuses the generated FBX kit. `-Views Overview,Mid_A,DenseFog` selects an initial capture subset; omit it for all named cameras.

## Validation boundary

This is a first production implementation under active validation. Generation success is not visual, navigation, gameplay, packaging or 120 FPS acceptance. Run the full [World Validation Standard](../../../Docs/Production/VEYRA_WORLD_VALIDATION_STANDARD.md) and report actual evidence. Editor beauty captures contain the generated environment; live actors, combat, fog and performance require gameplay runs. The current author's first-pass hardware target is this PC at 120 FPS, with resolution, quality and frame-time distributions recorded.
