# Veyra World Production Bible

**Version:** 0.2  
**Status:** Production guidance for the Meridian Crucible  
**Engine target:** Unreal Engine 5.8.3, Epic source build  
**Read with:** [Context Map](../CONTEXT_MAP.md), [Architecture](../../ARCHITECTURE.md), [Project Structure](../../PROJECT_STRUCTURE.md), [Battleground Bible](../Design/Veyra_Battleground_Bible_v0.9.md), [Art Direction](../Design/Art_Direction_v0.1.md), [ADR-011](../ADR/ADR-011-battleground-runtime.md), [ADR-040](../ADR/ADR-040-crucible-world-authoring-toolchain.md), [Asset & VFX Pipeline](VEYRA_ASSET_AND_VFX_PIPELINE.md), [World Validation Standard](VEYRA_WORLD_VALIDATION_STANDARD.md)

## 1. Purpose

This document defines **how Claude, Codex, and human contributors build the Meridian Crucible as an actual Unreal world** without letting visual production become a second, undocumented version of the map.

The repository already has an authoritative gameplay-layout path:

```text
Battleground design canon
        ↓
Game/Tuning/World.json
        ↓
UVeyraBattlegroundMapCommandlet
        ↓
Game/Scripts/BuildBattlegroundMap.ps1
        ↓
L_Battleground.umap
```

This document extends that foundation into final-world production: terrain, water, authored geometry, PCG, foliage, lighting, VFX, screenshots, and iterative agent review.

The governing rule is:

> **Gameplay defines the world. Art dresses the gameplay. Procedural systems accelerate art production, but may not silently rewrite competitive geometry.**

## 2. Current source of truth

### 2.1 Gameplay layout

[`Game/Tuning/World.json`](../../Game/Tuning/World.json) currently owns the machine-readable battleground layout under ADR-011. It already contains, among other things:

- map extent;
- river width;
- lane polylines and widths;
- inhibitor and Spire distances;
- Fluxborn spawn distances;
- Prime Well, base tower, and fountain locations;
- Dense Fog positions;
- wildlife camp locations;
- Flux Well sites;
- gameplay sizes/radii and other world tuning.

Those values are gameplay data and participate in the project's validated tuning pipeline.

**Do not duplicate those coordinates into Blender, a PCG graph, Blueprint defaults, a hand-authored map, or a separate art-only JSON file.**

If final art production needs new semantic layout data — for example explicit river crossing IDs, jungle-region polygons, protected vista lanes, or art-only biome masks — extend the owning data model deliberately. Do not create an invisible second source.

### 2.2 Generated map

[`Game/Scripts/BuildBattlegroundMap.ps1`](../../Game/Scripts/BuildBattlegroundMap.ps1) runs the battleground map commandlet and writes:

`Game/Content/Veyra/World/Maps/L_Battleground.umap`

The generated map is a binary Git LFS artifact. It is **not** the gameplay-layout authority.

When a generated map disagrees with its source data/tooling, fix the source/tooling and regenerate.

### 2.3 Runtime-spawned gameplay actors

ADR-011 already makes structures and other battleground runtime entities spawn from the layout rather than treating map-placed actors as authoritative.

Final environment art must preserve that separation. A beautiful decorative tower mesh may visually represent a Spire, but the gameplay Spire's authority remains with the runtime world/gameplay systems.

## 3. Hard production rules

1. **The macro Crucible is authored through reviewed gameplay data.**
2. **The micro Crucible may be procedural.**
3. **Generated output is never the source of truth.**
4. **Procedural generation must be deterministic and reproducible.**
5. **Presentation may observe gameplay state; it may never own gameplay state.**
6. **Decorative systems may not silently change competitive topology.**
7. **Unreal is the final visual-validation environment.**
8. **Agents inspect before editing.**
9. **Every substantial visual change has a rollback path.**
10. **Reusable visual families should be generated from shared Veyra visual grammar rather than authored as disconnected one-offs.**
11. **The Epic engine source checkout is read-only by default.** Project world work belongs in Veyra-owned modules/plugins; an engine fork requires explicit author approval and its own ADR.
12. **Production world automation belongs in `Game/Plugins/VeyraWorldTools/`.** Packaged gameplay code may not depend on that editor plugin.
13. **The production Crucible does not assume Z=0.** Terrain-bound runtime actors resolve the real playable surface through the Veyra-owned world contract.
14. **A successful generator run is not visual acceptance.** Agents must inspect repeatable Unreal captures and iterate on the owning source/tooling.

## 4. Protected competitive geometry

**Production redesign authorization (author ruling, 2026-10-02):** the Battleground Bible section 2 opens the current spatial arrangement to redesign. Only three lanes, a river, inner jungle between the lanes, and outer jungle beyond top and bot are fixed spatial requirements. Existing coordinates and blocker layouts are prototype inputs, not canon. Terrain fairness, terrain sight blocking and fog of war remain required. During this pass, intentional layout edits belong in World.json and must be reported and validated; art generation must consume the resulting layout without silently changing it. The protection rules below apply to the selected gameplay layout and do not prohibit this explicitly authorized redesign.


The following are protected and may not be altered by ordinary art dressing, Blender generation, PCG, foliage placement, water decoration, or lighting work:

- lane/Fluxway centerlines;
- lane widths;
- lane travel surfaces;
- inner- and outer-jungle connectivity;
- river crossings;
- objective access routes;
- wildlife camp access and leash spaces;
- Flux Well approach/capture spaces;
- Spire, inhibitor, base tower, Prime Well, and fountain locations;
- Fluxborn spawn and pathing corridors;
- Dense Fog gameplay locations and extents;
- traversable versus blocked boundaries;
- sightline-critical blockers;
- navigation-critical elevation changes;
- combat-clearance and spawn-clearance zones.

A generator must consume those constraints mechanically where possible.

Prompt text such as "don't block the lane" is not a safety system.

### 4.1 What counts as a gameplay-map change

These require explicit gameplay/map review:

- adding/removing a crossing;
- opening/closing a route;
- changing a choke width;
- moving a camp, structure, objective, or Dense Fog zone;
- creating a new traversable ledge;
- changing a decorative wall into blocking collision;
- changing river traversability;
- changing travel time materially;
- changing line-of-sight geometry in a tactically meaningful way.

Do not hide these inside an "environment polish" PR.

## 5. World construction order

Build and validate in this order:

```text
1. Gameplay layout in World.json
2. Runtime collision/navigation contract
3. Generated battleground substrate
4. Broad terrain / Landscape
5. River and water rendering
6. Large cliffs / architecture / landmark forms
7. Veyra material pass
8. PCG dressing
9. Foliage
10. Environmental VFX
11. Lighting / atmosphere
12. Flux-driven visual state
13. Gameplay-camera review
14. Performance / cook / package validation
```

Later stages may iterate earlier **presentation**, but stages 5–12 may not silently alter stages 1–2.

## 6. Terrain strategy

### 6.1 Unreal owns the continuous playable terrain

The default final-world approach is:

- **Unreal Landscape** for the continuous playable ground, as locked by ADR-040;
- **Blender-authored meshes** for cliffs, overhangs, shelves, roots, caves, retaining structures, bridge geometry, and hero formations;
- **PCG** for constrained dressing;
- **shared Veyra materials** for visual unity.

Do not replace the entire Crucible with one giant Blender terrain mesh unless a later architecture decision deliberately changes this strategy.

### 6.2 Terrain is derived from gameplay shape

Terrain shaping must respect lane/jungle/river geometry from `World.json`.

`VeyraWorldTools` must derive or query these constraints from Veyra-owned layout/world APIs:
- lane envelopes;
- river corridor;
- base pads;
- structure clearances;
- wildlife camp clearances;
- Flux Well clearances;
- Dense Fog regions;
- protected navigation bands.

Broad terrain may be sculpted artistically around these constraints, but its playable result must remain valid against the same source.

### 6.3 Height and verticality

Verticality must remain readable from the MOBA camera.

Decorative height is welcome, but production terrain is not decorative-only: the river basin, lane benches, jungle shelves/ridges and background relief should use real world-space elevation where appropriate.

The runtime must not keep flat-greybox assumptions. Structures, Fluxborn, wildlife, Flux Wells, team starts and other terrain-bound actors must resolve their standing transform from the actual playable surface rather than constructing Z from zero plus capsule height.

Rules:
- traversability must be explicit;
- cliff tops must not accidentally become reachable;
- decorative steps cannot become hidden pathing changes;
- tall foreground forms may require camera fade/cutaway behavior;
- visual elevation may not imply a shortcut that navigation forbids.

## 7. River and water

The river needs an explicit system because it is both a **gameplay spatial feature** and a major visual feature.

### 7.1 Separate gameplay water from rendering water

Use this conceptual split:

```text
World.json / Veyra world gameplay contract
        ↓
Veyra water presentation interface
        ↓
Chosen Unreal water renderer/material/Niagara implementation
```

Gameplay must not depend on a particular rendering plugin.

### 7.2 River inputs

The final river production representation should expose or derive:

- stable river ID;
- center/path;
- width profile;
- bank bounds;
- crossing IDs/clearances;
- surface height;
- depth metadata if gameplay needs it;
- flow direction/velocity for presentation;
- surface/physical material type;
- traversal/collision policy;
- VFX interaction hooks.

Today `World.json` provides the macro river width and diagonal map relationship. The production river must upgrade this to an explicit, reviewable spline/path with intentional bends and width variation. The exact schema may evolve, but gameplay-significant river control points, widths/crossings/clearances belong in the existing world data/tooling rather than a hidden art-only path.

A straight diagonal placeholder is not an acceptable final river. The path should read as a naturally landscaped watercourse while preserving the canonical macro relationship and protected gameplay topology.

### 7.3 Epic Water plugin

UE 5.8.3 includes Epic's Water system, including spline-driven river actors.

ADR-040 explicitly approves Epic Water for the first production Crucible as a **replaceable presentation implementation** behind Veyra's own river/world contract.

Therefore:

- VeyraWorldTools may create/configure Water-plugin river actors for the generated battleground;
- gameplay code must remain behind a Veyra-owned water/world contract;
- gameplay modules may not hard-depend on Water-plugin actor classes;
- the implementation must pass cook/package, readability and performance validation;
- the river visual implementation must remain replaceable without changing gameplay layout authority.

This is a scoped approval for the Crucible river, not a blanket approval of Experimental engine features.

### 7.4 What belongs in Unreal

Unreal owns:

- final water surface;
- flow shading;
- reflection/refraction strategy;
- shoreline treatment;
- foam;
- ripples;
- splashes;
- mist/spray;
- ability-water interactions;
- Flux contamination visuals;
- scalability.

Blender may author:
- bridge/culvert geometry;
- waterfall rock forms;
- bank meshes;
- debris/root assets;
- helper/source meshes for effects.

Blender does **not** own runtime river simulation.

### 7.5 Water performance philosophy

Veyra does not need expensive physically simulated fluid merely because Unreal can render it.

For the Crucible, water must primarily:
- look excellent from the gameplay camera;
- communicate direction and boundary;
- respond convincingly to movement/abilities;
- preserve telegraph readability;
- scale down gracefully;
- remain cheap during a 5v5 fight.

Prefer shader/Niagara techniques over general fluid simulation unless measured evidence justifies otherwise.

## 8. Large forms and environment hierarchy

**Provisional look (author ruling, 2026-10-05): an overgrown highland ruin.** Pale weathered stone causeways carry the Fluxways; moss-green jungle shelves rise between them over dark slate cliffs; the river runs clear teal; blue Flux glyphs glow in the old stonework; a warm late-afternoon sun lights the field. It guides materials, the environment kit and lighting until art direction replaces it.

The Crucible should be built in three visual layers.

### 8.1 Foundation assets

Examples:
- rocks;
- cliffs;
- roots;
- trees;
- shrubs;
- rubble;
- retaining walls;
- ground props.

These are ideal for reusable procedural families.

### 8.2 Identity assets

Examples:
- Fluxway structures;
- repeated Crucible ruin language;
- bridges;
- Spire surrounds;
- Flux Well surrounds;
- network conduits;
- corruption/repair motifs;
- inhibitor/base structural language.

These must visibly belong to Veyra.

### 8.3 Hero landmarks

Examples:
- Prime Well spaces;
- North/South Flux Well sites;
- major base silhouettes;
- major river crossing landmarks;
- unique map-orientation features.

Hero landmarks should be deliberately composed, even if their components come from generators.

A screenshot should read "Veyra" because of identity assets and landmarks, not because generic environment detail is unusually dense.

## 9. PCG policy

PCG is a **constrained environment crew**, not the owner of level design.

Good PCG uses:
- rock distribution;
- roots;
- shrubs;
- grass;
- riverbank dressing;
- rubble;
- decals;
- moss/wetness;
- art-only ruin fragments;
- inaccessible background scenery.

PCG must not independently:
- change lane/jungle connectivity;
- move gameplay actors;
- create or remove crossings;
- place blocking geometry in protected corridors;
- create traversable shortcuts;
- move Dense Fog gameplay;
- alter structure/objective clearances.

### 9.1 PCG must consume gameplay masks

Future PCG tooling should derive masks/constraints from `World.json` and the generated gameplay substrate.

Examples:

```text
LaneClearance
StructureClearance
CampClearance
FluxWellClearance
RiverCrossingClearance
DenseFogBoundary
NoOcclusionCombatZone
BackgroundOnly
```

Do not manually recreate those shapes inside every graph.

### 9.2 Determinism

Every production PCG region must record:
- graph/generator version;
- seed;
- input IDs;
- parameter profile;
- region bounds;
- output category;
- last validation revision.

Same inputs + same version + same seed should reproduce the same output.

### 9.3 Generation scale

Use different passes for different scales:

```text
Macro: cliffs / large trees / architecture
Medium: rocks / roots / shrubs / rubble
Micro: grass / pebbles / decals / litter
```

Do not solve every scale with one scatter graph.

### 9.4 Runtime versus editor-time

Stable competitive map dressing should default to **editor/build-time generation through VeyraWorldTools**.

Runtime PCG needs explicit justification and performance/network validation. Randomly changing environment layout per match is not an implied goal.

## 10. Foliage and the MOBA camera

Foliage is authored for Veyra's camera, not for a first-person forest demo.

Rules:
- canopy silhouettes must read top-down;
- foliage cannot hide Vanguards in ordinary combat space unless an approved camera-fade/cutaway system makes that behavior explicit;
- lane/objective foliage should use restrained movement and density;
- inaccessible/background foliage may be richer;
- foliage must not imply gameplay concealment unless the Vision/Battleground systems say so;
- decorative foliage must not redefine Dense Fog.

The foliage pipeline lives in the Asset & VFX document.

## 11. Dense Fog versus atmospheric fog

Dense Fog is gameplay.

Environmental mist, waterfalls, ground fog, volumetrics, and atmospheric haze are presentation.

Never make decorative fog look so similar to Dense Fog that a player cannot tell whether visibility rules apply.

Dense Fog boundaries must remain legible on all supported graphics-quality levels.

No gameplay-critical fog boundary may depend solely on a particle system that disappears on low settings.

## 12. Lighting and atmosphere

Global lighting is a shared world system.

A local asset task may not solve itself by changing:
- global exposure;
- directional light;
- skylight;
- global fog;
- global color grading;
- tone mapping.

Global lighting changes require whole-map screenshot regression.

Local lights should be:
- justified;
- limited;
- performance-aware;
- subordinate to gameplay readability.

Emissive Flux/structure lighting may be visually strong without turning the battlefield into competing bloom.

## 13. Flux-driven environmental evolution

The Crucible may visually evolve as teams accumulate Team Flux.

That visual evolution must be driven by authoritative gameplay state.

Allowed presentation responses include:
- stronger network emissive;
- directional Flux flow;
- subtle glyph activation;
- controlled environmental particles;
- structure activation;
- light accents;
- art-only mesh/state swaps;
- non-blocking decorative growth;
- water contamination/tint;
- lane/network pulse effects.

Presentation must not:
- calculate Team Flux;
- change gameplay thresholds;
- change collision/navigation;
- alter structure vulnerability;
- change objective capture;
- decide win/loss.

Subtle animation is preferred over motion for motion's sake. The world should feel increasingly energized without becoming harder to read during fights.

## 14. Agent-driven map building

Claude/Codex should operate through reproducible tools wherever practical.

Preferred operations:
- read/extend the authoritative spatial contract in `World.json`;
- invoke `Game/Plugins/VeyraWorldTools/` editor/commandlet/Python entry points;
- build/run the pinned source-built UE 5.8.3 editor without modifying engine source;
- run map-generation commandlets;
- run Blender generators;
- import/reimport asset families;
- regenerate scoped PCG regions;
- rebuild navigation;
- capture named cameras;
- capture debug views;
- collect manifests/performance data.

A substantial world-editing task should identify:

```text
Region:
Gameplay source:
Protected layout fields:
Visual generators:
PCG graphs:
Assets/materials/VFX:
Validation cameras:
Tests/checks:
Rollback point:
```

### 14.1 Do not work blind

The intended production loop is:

```text
edit source
    ↓
regenerate affected content
    ↓
open/run Unreal
    ↓
capture standard gameplay cameras
    ↓
capture navigation/collision/debug views
    ↓
Claude/Codex visually reviews actual output
    ↓
classify problem by owner
    ↓
fix the owning source
    ↓
repeat
```

This is the reason Veyra-authored assets are valuable: the agents may inspect their source files and screenshots freely.

## 15. Standard review cameras

The final battleground should maintain stable named camera anchors for repeatable review.

At minimum:

- Top lane from both directions;
- Mid lane from both directions;
- Bot lane from both directions;
- representative inner-jungle entrances;
- representative outer-jungle routes;
- North Flux Well;
- South Flux Well;
- major river crossings;
- both base approaches;
- both Prime Wells;
- Dense Fog test areas;
- dense foliage benchmark;
- heavy-combat benchmark.

Capture modes should include:
- beauty;
- normal gameplay;
- gameplay with representative actors/effects;
- collision;
- navigation;
- PCG/debug;
- readability/value;
- performance/debug.

The Validation Standard owns the acceptance rules.

## 16. World content layers

Whether Veyra eventually uses World Partition or another level-organization strategy, keep these conceptual layers distinct:

```text
Gameplay
Terrain
Water
Architecture
Cliffs
Foliage
GroundDetail
VFX
Lighting
Debug
```

If World Partition/Data Layers are later approved, map these conceptual owners into Data Layers.

Do not adopt World Partition solely because PCG supports it. The current Crucible scale and runtime needs should determine that architecture decision.

## 17. Provenance and third-party content

The visible Crucible should default toward Veyra-authored assets/generators.

Any third-party content entering the production world must record:
- source;
- creator;
- license;
- acquisition date/version;
- commercial-use rights;
- redistribution restrictions;
- AI-use status;
- whether Claude/Codex may inspect, transform, screenshot, or use it as model input;
- modifications;
- attribution requirements.

If a license forbids generative-AI input, the asset may not be passed into an agent's vision/model context merely because it is present in the Unreal project.

The Asset & VFX document defines the production handling rule.

## 18. Regeneration and rollback

Large generation should be scoped.

Required capabilities where practical:
- preview/dry-run;
- region-only generation;
- deterministic seed reuse;
- cleanup of prior generated output;
- generated-manifest reporting;
- Git/LFS-aware rollback;
- post-generation validation.

Do not regenerate the entire Crucible to solve a local riverbank problem unless the tooling genuinely requires it.

## 19. Generated-content metadata

Generated content should explain itself.

Example:

```text
Generator: CrucibleRiverbank_v3
Seed: 44129
WorldLayoutRevision: <commit/hash>
Region: River.TopCrossing.02

Inputs:
  World.json river layout
  Crossing.Top.02
  FluxWell.North clearance
  WetRiverbank style profile

Outputs:
  128 rock instances
  74 reed clusters
  19 roots
  6 composed formations

Validated:
  UE 5.8
  Gameplay cameras PASS
  Navigation PASS
  Collision PASS
  Performance profile PASS
```

Exact serialization can be implemented later. The information must not disappear.

## 20. Integration with the existing battleground commandlet

The existing battleground commandlet currently builds the structural map substrate from `World.json`.

Final-world production should evolve from this rather than bypass it.

Likely future responsibilities may be split into stages such as:

```text
World.json validation
        ↓
Gameplay substrate generation
        ↓
Terrain/water authoring pass
        ↓
Art-layer generation/import
        ↓
PCG generation
        ↓
Validation capture
```

If the original commandlet becomes too broad, split tooling deliberately. Do not turn it into a god commandlet that contains art logic, gameplay rules, asset generation, screenshot analysis, and runtime systems in one class.

## 21. Source control

ADR-006 already places Unreal binary assets under Git LFS and marks `.uasset`/`.umap` lockable.

World-production rules:
- lock a binary before manual editing;
- prefer generated binary output from text/source code where feasible;
- record how to reproduce an asset-side change;
- do not commit derived caches/build output;
- do not treat an unreviewed binary diff as sufficient explanation for a map change.

For generated `L_Battleground.umap`, the source data/tooling should make the binary rebuildable.

## 22. Open implementation decisions

This document intentionally does not silently decide:

- exact Landscape resolution/component sizing and erosion/detail parameters;
- exact JSON field names/interpolation rules for the detailed river spline and terrain semantic regions;
- whether Epic Water remains the release renderer after full packaging/performance validation;
- whether World Partition is adopted;
- exact PCG graph asset organization and style-profile layout;
- exact world-performance targets/hardware tiers;
- final Flux environmental-state art language;
- exact standard-camera storage/capture tool;
- exact Blender-source storage layout;
- final source-control strategy if Git LFS stops scaling.

Those are explicit implementation/art-direction decisions.

## 23. Completion definition

A Crucible world task is complete only when:

1. the correct gameplay/visual source was changed;
2. generated output is reproducible;
3. no protected competitive geometry changed unless explicitly approved;
4. collision/navigation remain valid;
5. the result is visually coherent with Veyra;
6. actual Unreal gameplay-camera captures were reviewed;
7. applicable performance/scalability checks pass;
8. relevant cook/package checks pass;
9. third-party provenance is clean;
10. the summary states what changed, what was regenerated, what was validated, and what remains open.

The purpose of this pipeline is not to remove art direction. It is to make an agent-built world **directable instead of mysterious**.
