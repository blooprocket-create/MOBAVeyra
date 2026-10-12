# Larger, varied Dense Fog footprints

Pass 11 responds to the author's review that Pass 10 was too small and repetitive.
The eight pockets per half now use three to five overlapping lobes with distinct
ribbons, hooks, bends, a crescent and broader ambush pockets. Half-turn symmetry
preserves equivalent coverage for both teams. There are 66 lobes in 16 separate
connected banks. Approximate union coverage is 4.2–6.0 times the corresponding
Pass 10 pocket, measured on a 20-world-unit grid.

Only `Game/Tuning/World.json`'s `layout.denseFog` changed in this revision. The
Battleground Bible §11 records the author's updated placement direction. Lane,
wall, river and objective source data are unchanged. The initial enlargement
was adjusted to satisfy the existing wall-clearance validator; protected walls
were not moved to accommodate fog. Positions and radii remain provisional,
validated designer-editable data.

The Pass 10 shared presentation, material, emission and motion are retained.
Vision still owns circle-union membership and concealment. No C++, networking,
replication, dependencies or gameplay-rule changes were needed in Pass 11.

Regenerate with `Game/Scripts/BuildBattlegroundMap.ps1` (CrucibleStyle seed 48173)
while holding the map's LFS lock, then run `ValidateBattleground.ps1` and
`Test.ps1 -Filter Veyra.World`. Capture `Overview,Play_DenseFog,Play_Jungle_A0`
through `CaptureBattleground.ps1` at high and low profiles. The local Pass11
comparison pins camera positions to Pass10's capture manifest because the
generated Dense Fog camera follows the first layout lobe. Motion evidence uses
30 fixed-step Niagara captures separated by 0.1 simulation seconds.

Map generation and navigation/collision/symmetry validation passed with no
findings. All 95 World automation tests passed (report 20261010-135801). Tuning and documentation checks passed. A data comparison verifies
that only Dense Fog layout changed; a connected-component check verifies 16
separate banks including mirrored placements. Screenshots are unmodified Unreal
outputs. The motion preview is controlled simulation, not live-match footage.

Live combat and telegraph readability, entry/exit balance and GPU cost remain
open. Larger coverage is author-directed tuning, not competitive balance approval.

| Low | High |
|---|---|
| ![Low](low-dense-fog.png) | ![High](high-dense-fog.png) |

![Whole-map placement](overview.png)
