# Dense Fog: irregular pockets and toon motion

Pass 10 replaces the reed-filled map circles and runtime floor markers with a
shared local-fog and Niagara presentation. The author's requested placement
change uses three overlapping lobes per pocket: eight connected pockets per
half, mirrored by half-turn. Lane-edge and jungle pockets remain provisional
competitive tuning until playtested.

## Ownership and reproduction

- Layout: `Game/Tuning/World.json`, only `layout.denseFog` changed. This deliberately
  changes concealment footprints; lane, wall, river and objective data are unchanged.
- Rules: Vision still owns membership in connected circle unions, concealment
  and targeting. No networking, replication, ability lifetime or Vision rules changed.
- Presentation: `VeyraDenseFogVisuals` in VeyraUI is shared by runtime banks and the
  editor generator. It resolves the playable surface through VeyraSurfacePlacement.
- Visual tuning: `DefaultGame.ini` DenseFogVisualSettings; emission and motion in
  `Effects.json`; scalloped shade bands, opacity and ground-depth fade in
  `PresentationMaterials.json`. No outside visual assets were introduced.
- Editor dependency: VeyraWorldTools uses VeyraUI through a non-server build guard.
  Packaged runtime code does not depend on the editor plugin. No new plugin or
  engine strategy; engine source remains unchanged.
- World generation uses CrucibleStyle seed 48173. Particle sampling is stochastic;
  fixed-step captures repeat the timing, not identical random particle positions.

With binary locks held, run from the repository root:

```powershell
./Game/Scripts/Build.ps1 -Target VeyraEditor
./Game/Scripts/BuildPresentationMaterials.ps1 -Materials M_VeyraDenseFog
./Game/Scripts/BuildEffects.ps1 -Effects NS_VeyraDenseFog
./Game/Scripts/BuildBattlegroundMap.ps1
./Game/Scripts/ValidateBattleground.ps1
./Game/Scripts/CaptureBattleground.ps1 -Profile low -Views 'Overview,Play_DenseFog,Play_Jungle_A0'
./Game/Scripts/CaptureBattleground.ps1 -Profile high -Views 'Overview,Play_DenseFog,Play_Jungle_A0'
```

The standard capture warms authored fog by 180 Niagara steps at 1/60 second and
pauses it before shader preparation. The local Pass10 motion preview advances
six additional steps between each of 30 original captures at Play_DenseFog.
It is a controlled simulation sequence, not a live-match recording. Images use
1920 × 1080, Lit, normal distance-based LOD (`r.ForceLOD=-1`).

## Verification and limits

Editor build passed. Vision automation: 49 passed; World automation: 95 passed.
Generated-map navigation, collision and symmetry validation has no findings.
Material/effect specification tests: 14 passed. Tuning validation, module-layer
validation, documentation routing and whitespace checks passed.

Architecture review: one layout owner and one Vision owner; shared presentation
builder; valid dependency direction; no gameplay state in UI, Blueprint or
Niagara; no named-content special cases or duplicated gameplay calculations.
All changed gameplay radii/positions are validated editable World.json data.

Still required: live combat and telegraph readability within the banks, boundary
entry/exit playtests, ability-created bank playback, temporal rendering at game
frame rates, GPU budgets and packaged-client acceptance. This pass is not final
competitive balance or performance approval.

## Gameplay-camera captures

| Low | High |
|---|---|
| ![Low](low-dense-fog.png) | ![High](high-dense-fog.png) |

Unmodified Unreal outputs; SHA-256 and source paths are in `evidence.json`.
