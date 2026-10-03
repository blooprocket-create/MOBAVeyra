# Crucible world authoring

Authority: [ADR-040](../../../Docs/ADR/ADR-040-crucible-world-authoring-toolchain.md), [World Production Bible](../../../Docs/Production/VEYRA_WORLD_PRODUCTION_BIBLE.md), and Battleground Bible section 2. `VeyraWorldTools` is an editor-only, sealed module. Gameplay does not depend on it. The existing battleground commandlet requests generation through `IVeyraWorldAuthoring`; an absent generator fails the build instead of silently producing a flat map.

## Sources and outputs

- `Game/Tuning/World.json`, schema 5: spatial authority. Surface search limits, sampled river controls/full widths, terrain heights/transitions, lanes, bases, objectives, camps, fog and blocking shelves live here.
- `Config/CrucibleStyle.json`: non-spatial sampling, lighting, cameras and dressing parameters. It contains no duplicate map coordinates.
- `Game/ArtSource/Environment/CrucibleKit.json`: deterministic mesh-family source; no map layout.
- `Game/Content/Veyra/World/Maps/L_Battleground.umap`: generated Landscape, layer weights/material, Water presentation, baked PCG instances, lights, review cameras, starts, navigation bounds and runtime marker. Never hand-patch the generated map.
- `Game/Saved/WorldGeneration/manifest.json`: successful generation record with source and output hashes; generation is not acceptance.
- `Game/Saved/WorldGeneration/Regions`: generated per-pass seed, mesh, count and normalized placement digest. These are evidence, not authoring inputs.

The river uses an authored curved channel and its derived reflected channel to produce one connected, symmetric basin. Both renderer and gameplay use the same sampled controls and full widths. Water is forbidden from carving Landscape or supplying gameplay collision. The World-owned terrain sampler supplies the basin, lane benches, jungle shelves, base plateaus and exterior rise. The Landscape import quantizes this surface using Unreal's height encoding. Generation enables commandlet rendering and finishes Landscape edit-layer composition before saving; a NullRHI-only save does not produce finished weight layers in the pinned engine.

Map shelves retain server-owned terrain collision and terrain sight occlusion. Their generated rocks are presentation only; a replicated map-terrain flag prevents duplicate greybox blocks. Ability-created walls retain their own presentation. All dressing is non-colliding and cannot affect navigation. Macro, medium and micro passes use independent deterministic seeds; the generator excludes protected footprints and reflects placements across the team boundary. PCG resources are baked and their source components removed, so packaged play does not run PCG generation.

## Reproduce

From the repository root:

```powershell
./Game/Scripts/Build.ps1 -Target VeyraEditor
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
