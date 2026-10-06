# ADR-040: Crucible world-authoring toolchain

**Status:** Proposed. This ADR defines the implementation boundary for the first production-quality Meridian Crucible world pass. It becomes Accepted when the author merges the implementation pull request that adds the Veyra world-authoring toolchain and upgrades the generated battleground to use it.  
**Date:** 2026-10-01  
**Related:**
- [ADR-001](ADR-001-unreal-version-policy.md): Unreal Engine 5.8.3 from Epic's source repository is the pinned engine.
- [ADR-006](ADR-006-unreal-project-scaffold.md): generated binaries must remain reproducible from reviewable source.
- [ADR-011](ADR-011-battleground-runtime.md): `Game/Tuning/World.json` and the generated battleground are the runtime/layout path.
- [World Production Bible](../Production/VEYRA_WORLD_PRODUCTION_BIBLE.md): terrain, river, PCG, lighting and agent workflow.
- [World Validation Standard](../Production/VEYRA_WORLD_VALIDATION_STANDARD.md): acceptance gates for the generated world.

## Context

The existing battleground build is intentionally a structural greybox. `Game/Scripts/BuildBattlegroundMap.ps1` invokes `UVeyraBattlegroundMapCommandlet`, and the current builder creates a flat floor, starts, navigation bounds, the runtime marker and basic lighting. Runtime placements are still fundamentally 2D and several spawn paths assume the playable surface is at Z=0.

That is sufficient for gameplay scaffolding but not for the intended Meridian Crucible. The production world needs real landscape relief, a shaped river, cliffs and shelves, constrained PCG dressing, repeatable visual review and a deterministic way for coding agents to edit/regenerate the world without turning the generated `.umap` into an undocumented source of truth.

Because Veyra uses Epic's source-built engine, agents could technically modify `Engine/Source`. That power must not become the default solution for project-level world work.

## Decision

### 1. Engine source is read-only by default

The pinned Unreal Engine source checkout is an external platform dependency, not Veyra's ordinary implementation surface.

- Claude, Codex and other agents may read engine source to understand APIs and behavior.
- They may build and run the source-built editor/commandlets.
- They may not modify `Engine/Source`, `Engine/Plugins`, engine build files or engine-owned content for ordinary Veyra world work.
- An engine-source modification requires explicit author approval and a separate ADR describing why a project/plugin solution is insufficient and how the fork will be maintained.

No inconvenience in an Unreal API is, by itself, permission to fork the engine.

### 2. VeyraWorldTools is a project-owned editor plugin

Create the world-authoring surface as a project plugin at:

`Game/Plugins/VeyraWorldTools/`

The plugin is the preferred home for editor-only world generation, regeneration, capture and validation helpers. Its production-facing code must remain editor/developer tooling; packaged gameplay modules may not depend on it.

The plugin may:
- consume `Game/Tuning/World.json` through Veyra-owned layout APIs;
- create/update Unreal Landscape data;
- create river presentation from the approved river contract;
- invoke editor-time PCG;
- place generated art-only world content;
- create named review cameras and debug captures;
- emit generation manifests and deterministic seeds;
- expose commandlet/Python/editor utility entry points for agent automation.

The plugin may depend on production modules to read their contracts. Production modules must not depend on the plugin.

### 3. Landscape is the continuous terrain substrate

The first production Crucible uses Unreal Landscape for the continuous playable ground.

Landscape generation must be source-driven and reproducible. The generated Landscape inside `L_Battleground.umap` is output, not authority.

The terrain must support real vertical relief:
- a river basin lower than surrounding ground;
- readable lane benches and approaches;
- raised jungle shelves/ridges where they do not create unapproved routes;
- cliffs/overhang meshes where Landscape alone is unsuitable;
- inaccessible vista/background relief beyond combat space.

The map must not regress to one flat plane with decorative rocks pretending to be elevation.

### 4. Runtime gameplay resolves the real surface; it does not assume Z=0

VeyraWorld owns the runtime contract for resolving a 2D gameplay placement onto the actual playable surface.

All runtime spawn paths that currently construct locations from XY plus capsule half-height must be audited. Structures, Fluxborn, team starts, wildlife, Flux Wells and any other battleground entities that can stand on authored terrain must use one Veyra-owned ground/surface-resolution path rather than each inventing its own trace.

The editor plugin may use that contract or an editor equivalent, but the editor plugin must not become gameplay authority.

### 5. World.json remains the spatial authority

`Game/Tuning/World.json` remains the machine-readable owner of gameplay layout.

The schema may be extended when final-world production requires explicit semantic geometry, including:
- a stable river path/control points;
- width/profile information;
- named crossing/clearance regions if gameplay needs them;
- terrain/elevation semantic regions when a height change is gameplay-significant;
- protected combat/navigation clearances required by generation.

Do not create a second coordinate map in Blueprints, PCG graphs, Python, Blender files or an art-only JSON file.

Generator style profiles may hold non-spatial presentation parameters such as seed, erosion character, noise amplitude, rock density, foliage density or material variation. They may reference stable region IDs but may not duplicate the region's coordinates.

### 6. The river is a real authored spline, not a diagonal placeholder

The production river must follow an explicit, reviewable, naturally shaped path derived from the canonical diagonal relationship, with intentional bends, width variation, banks and elevation.

A straight line from one edge of the map to the other is not an acceptable final implementation.

The river system must keep these layers separate:

`World.json / Veyra spatial contract -> Veyra river presentation contract -> renderer`

For the first production implementation, Epic's UE 5.8 Water plugin is approved as a **replaceable presentation implementation** behind the Veyra-owned contract.

- Gameplay modules may not depend on Water-plugin actor classes.
- Water-plugin use must not own traversal, objectives, Team Flux, damage or other gameplay rules.
- The implementation must cook/package and meet the World Validation Standard.
- If Water fails performance, packaging or maintainability gates, it may be replaced without changing the gameplay layout contract.

This approval does not grant general permission to use Experimental engine features elsewhere.

### 7. PCG is editor-time, constrained and region-scoped

Production Crucible PCG defaults to editor/build-time generation through VeyraWorldTools.

PCG graphs consume Veyra-owned masks/clearances. They may decorate; they may not decide competitive topology.

Generation is split by scale:
- macro: large rock/cliff/architecture compositions;
- medium: rocks, roots, shrubs, rubble and bank dressing;
- micro: grass, pebbles, decals and litter.

Every generated region records its graph/generator version, deterministic seed, source layout revision and inputs. Region-only regeneration is preferred over whole-map regeneration.

### 8. Agent worldbuilding is inspect -> generate -> render -> critique -> correct

The normal agent loop is:

```text
read canon + layout + production rules
        ↓
change source/tooling
        ↓
regenerate affected world scope
        ↓
build/open the source-built Unreal editor
        ↓
capture standard gameplay/debug cameras
        ↓
visually inspect the actual result
        ↓
classify the defect by owner
        ↓
fix source/tooling and repeat
```

Agents must not treat a successful commandlet exit as proof that the battleground looks good.

### 9. No World Partition decision is made here

Do not adopt World Partition merely to solve world-authoring convenience. The Crucible may use ordinary level organization/Data Layers until a measured need justifies a separate architecture decision.

## Consequences

- Veyra gains a deterministic, project-owned worldbuilding API that coding agents can safely operate.
- The source-built editor becomes an advantage without making the game depend on a private engine fork.
- Real elevation requires runtime spawn/surface code to become 3D-aware.
- `World.json` may need a schema revision to carry explicit river and other gameplay-significant spatial semantics.
- The binary battleground map remains generated output and can be regenerated from repo-owned source/tooling.
- Epic Water is permitted for the first production pass but remains replaceable and isolated from gameplay authority.
- Visual quality becomes an iterative rendered-output problem rather than a one-shot procedural generation problem.

## Alternatives considered

### Modify Epic's engine source directly

Rejected as the default. It creates a Veyra-specific engine fork and long-term upgrade/maintenance burden for capabilities that can be implemented at project/plugin level.

### Keep the flat battleground and decorate it heavily

Rejected. It cannot produce the intended river basin, landscape hierarchy or believable world-scale composition.

### Generate one giant terrain mesh in Blender

Rejected for the continuous playable substrate. Blender remains useful for cliffs, overhangs, roots, bridges and hero formations, while Unreal Landscape owns broad playable terrain.

### Let PCG invent the map

Rejected. PCG is a constrained environment-production system, not the owner of lane, jungle, river, objective or navigation topology.

### Hand-sculpt the final .umap and treat it as canon

Rejected. It would make the binary map the hidden source of truth and break deterministic regeneration.

## Implementation contract — first production pass (2026-10-02)

The implementation lives in [`Game/Plugins/VeyraWorldTools`](../../Game/Plugins/VeyraWorldTools/README.md). Its sealed editor module registers `IVeyraWorldAuthoring`; the existing battleground commandlet consumes that optional service and refuses generation when it is absent. Production modules never query it or depend on it.

`World.json` schema 6 carries validated surface-search limits, the terrain's levels and runs (`layout.terrain`) and the river (`layout.river`). The river's main channel is authored for Team A's half only, from the centre outward and off the floor, and joined at the centre to its rotation; each island's side channel leaves and rejoins it around a Flux Well site, and its rotation rounds the other site. Controls carry full widths. `FVeyraRiverShape` samples every channel once (a Catmull-Rom curve through the controls, widths interpolated linearly) and is cached per river, so gameplay classification (jungle, camps, walls, Wells), the terrain field, Water splines and minimap ribbons consume one geometry; no channel is stored twice. Validation requires the main channel to start at the centre and leave the floor, each island channel's ends to lie in the main channel's water, and each Well to stand dry on an island the water closes around. The old scalar diagonal `riverWidth` is removed. Runtime placement resolves world-static playable ground, excludes terrain-wall tops, checks slope, and refuses missing ground rather than substituting Z=0.

The playable ground has its own collision object channel and profile (`VeyraGround`, DefaultEngine.ini): the Landscape and generated floors sit on it and every body blocks it, so units walk on it as before, while a query for ground finds only ground (never a wall standing on it) and a sweep for walls (WorldStatic) never meets a slope. `VeyraGround` in Combat, the lowest layer that moves bodies, owns the query; World's surface placement uses it within the layout's surface bounds, and lower layers within Combat's `ground.searchHeight` of the body or point concerned. Forced movement plans over the ground's plan: walls and the end of walkable ground stop a path, rising ground does not, and the body keeps its height above the ground as it goes and where it ends; a blink lands the same way. A cast's point, a placed marker, an echo, a companion, a ward and a move order's destination stand on the ground at their place; line projectiles keep their height above the ground along their path and homing ones close on their target's height. A map wall's collision reaches from below the lowest ground under it to its height above the highest, and a ridge's cliff rises within the wall's footprint, so nothing passes under a wall and the ground around it stays walkable.

`FVeyraTerrainField` composes the ground as a pure function of the layout: each lane's road level to its shoulder and climbing over `jungleRise` to the jungle's shelves, each base's pad raised, each Well's island a low platform, the river's basin cut with walkable banks and a shallow underwater shelf, a ridge raised on every wall after the river cuts its banks (so a wall by the water keeps its cliff), and a rim beyond the floor's edge, open where the river leaves. A style profile's relief is averaged with its rotation, so both halves stay the same. Validation checks every level and run, and the nominal grades of the walkable climbs against `surface.maxSlopeDegrees`. `VeyraDressing::Allows` is the one rule for where generated dressing may stand: off every road and its clearance, structure, fountain, camp leash, Flux Well, Dense Fog circle and the water.

The author's map ruling in Battleground section 2 supersedes preservation of the greybox's specific spatial design. This pass intentionally replaces its blocking arrangement with angled jungle shelves, ridges and curved banks, while deriving both teams' geometry from the same source. Under the author's 2026-10-05 ruling the layout is symmetric under a half turn about the centre, not a reflection: a reflection across the line between the bases could only carry a straight river, while the rotation lets one naturally curved river, and its Well islands, be the same for both teams. Team B's points, walls (with their facing) and Dense Fog are Team A's rotated; each lane rotated and reversed must be a lane (top onto bot, mid onto itself), and each team's lane structures stand at the same distances along the lane from its own end. Lane count, river, inner jungle, outer jungle, terrain fairness and authoritative sight blocking remain requirements. Existing bases/objectives remain provisional selected anchors, not newly canonized coordinates.

Landscape generation creates continuous collision ground and weighted lane, bank, jungle and exterior materials. Water disables landscape carving before editor actor-added callbacks run. PCG receives already constrained points, bakes them to ordinary non-colliding instances, verifies counts, and removes the generator components. Style profiles contain presentation parameters only. Generated cameras target layout-derived locations and the terrain under them. The provisional look (author ruling, 2026-10-05) is an overgrown highland ruin, recorded in the [World Production Bible](../Production/VEYRA_WORLD_PRODUCTION_BIBLE.md) §8. Every pass still requires the complete World Validation Standard; implementing this contract does not by itself establish acceptance.

**River presentation replaced (2026-10-05), as §6 allows.** Epic Water's bodies were generated correctly from the contract (their water-info meshes rendered in place), but its water mesh produced no visible tiles in the generated map after investigation, and it is an Experimental plugin the packaged game would otherwise depend on. The river is instead one project-owned surface: VeyraWorldTools builds a grid mesh clipped to the water of `FVeyraRiverShape` (every channel and junction in one mesh, a margin beyond the edge that the banks rise over), its vertices carrying each point's downstream direction and how far it lies from the shore; `M_CrucibleWater` shades it with the Single Layer Water model (absorption and scattering, ripples flowing along the carried direction, foam at the shore). It has no collision and never affects navigation; the Water plugin is no longer a dependency. The terrain's Landscape material, `M_CrucibleTerrain`, and the river's are generated from `Game/ArtSource/Environment/Terrain/TerrainTextures.json` by `BuildTerrainArt.ps1`: seeded, tileable textures for each painted layer (paving, moss, shore, slate), a large-scale variation over them, rock projected from the side wherever the ground is steep, and darker, glossier ground at the water's edge.

**Environment kit, dressing and light (2026-10-05).** `GenerateEnvironmentMeshes.py` (Blender 5.2, from `CrucibleKit.json`) builds the kit's families from their seeds alone: slate cliffs cut into facets with terraced ledges, river boulders, fluted pillars broken off, masonry blocks, steles with raised Flux glyphs on their own emissive slot, trees, shrubs, ferns, reeds and grass. `ImportEnvironmentMeshes.py` builds their materials from the terrain's own textures (stone world-aligned with moss on what faces up, glowing glyphs, foliage tinted and swaying by height), imports each mesh fresh, gives stone Nanite, and checks size, pivot and the absence of any collision. `VeyraDressingBuild` decides every placement by Veyra's rules and seeds, on Team A's half, then turns it half a turn for Team B: cliffs dressing each wall's footprint with trees on its ridge, a cliff line along the rim open where the river leaves, forest across the vista, shrubs, ferns and boulders only where `VeyraDressing::Allows`, reeds filling each Dense Fog circle, boulders along the banks, ruins beside the Fluxways and round each base's pad, steles at the Wells. Heights come from the built Landscape. PCG instances and bakes each region to non-colliding instances and records its manifest. The map's light is a warm late-afternoon sun from the gameplay camera's upper left, sky fill, a haze that leaves the play space clear and softens the vista, and a restrained grade; named gameplay views use exactly the player's camera settings. The Landscape declares its painted layers before importing them: the engine's import registers the components before it declares their layers, and its fix-up then deletes every weightmap of a layer the Landscape does not yet know and re-merges through a render path that crashes in the commandlet world. The engine stays unmodified.

**Smaller bases (2026-10-05, after the author noted the bases looked too large).** Each lane now begins, and its inhibitor stands, 2,800 units from its Prime Well instead of 4,200, and the base's pad shrinks from 4,200 to 3,100; every Spire's distance grows by the same 1,400, so the laning Spires stand where they stood. Provisional values, as ADR-011's.

**Navigation and fairness validation (2026-10-05).** `ValidateBattleground.ps1` runs VeyraWorldTools' `VeyraWorldValidate` commandlet on the saved map, with the profile `Config/CrucibleValidation.json`. The commandlet raises the server's own walls (`UVeyraBattlegroundSubsystem::RaiseWalls`) and builds the server's navigation over the Landscape with the project's agent. It then measures each listed route for both teams; Team B's route is Team A's turned half a turn. For each route it records the length, the climb and descent of the ground beneath it, and its steepest grade. It also walks each lane base to base along its waypoints, as the lane's Fluxborn walk it. It checks:

- that the terrain under every sample of Team A's half stands at the height of its rotation;
- that every anchor resolves onto playable surface at its rotation's height;
- that each structure has its rotated counterpart;
- that navigation marks each wall's footprint with the obstacle area;
- that generated presentation has no collision.

The tolerances are provisional review values in the profile, not code constants. Any finding fails the run and is written to `Saved/WorldGeneration/Validation.json`. The first full run passes: 22 routes and 3 lanes differ between the teams by at most 3 units, with equal climb; the terrain is identical under the half turn across 4,095 samples; and all 26 walls are obstacles. The plugin loads at the default phase so the editor finds its commandlet.
**First frame-time measurement (2026-10-05, World Validation Standard §23.1).** The setup was the author's PC (i7-12700KF, RTX 3070 Ti) and a packaged Development client at 1920×1080, screen percentage 100, uncapped. The match was live on the battleground with 8 playing bots and Fluxborn waves (`Smoke.ps1 -Map Battleground -PlayingBots 8 -PerfSeconds 60`). The median, 95th and 99th percentile frame times were 6.37, 7.76 and 8.60 ms; the GPU took 5.84 ms and the render thread 6.36 ms at the median. The grey box measures 4.71 ms at the median.

The first run was render-thread bound at 9.18 ms. Hiding the map's art (Landscape, meshes and dressing) left that time unchanged. The cost was the fog-of-war sheet: its line-batch mesh is rebuilt every frame, and on the Landscape each fog cell is subdivided to follow the ground. Sharing corners across each unseen run cut the sheet to a third of its vertices. The 99th percentile is still just over the 8.33 ms budget. A persistent fog mesh, uploaded only when the fog changes, is the next lever.
The merged ADR-054 seen-ground grid remains Vision-owned. UI terrain-following fog tiles and ground telegraphs use World surface resolution. Presentation tessellation does not change sight-cell resolution, information replication or terrain occlusion.

**Glow under the physical light (2026-10-06).** The light above (a 22,000 lux sun under a manual exposure of EV100 12) shows an emissive of a few units as nothing. The stele glyphs, the structures' Flux (`M_CrucibleFlux`) and the Fluxborn's (`M_FluxbornFlux`) therefore had no visible glow. The presentation's materials had the same problem, fixed in [ADR-063](ADR-063-combat-readability-cues-effects-sound-and-the-fountain-shop.md)'s 2026-10-06 amendment, and so did the Vanguard bodies ([ADR-064](ADR-064-generated-animated-vanguards.md) §4).

- **The fix:** each of these glows is scaled by the inverse of the scene's exposure, through one shared helper (`unexposed` in `Game/Scripts/veyra_material_graph.py`).
- **Strengths are kit data,** in multiples of what the exposure maps to white: `look.glyphStrength` in `CrucibleKit.json`, and each surface's `emission` in `StructureKit.json` and `FluxbornKit.json`. `Game/Scripts/KitMaterials/spec.py` checks them, in CI too.
- **One surface builder:** the structure and Fluxborn kits now share it, where each had its own copy.
- **Materials-only rebuilds:** each kit's build script takes `-Materials`. It rebuilds just the materials named, imports no mesh, and so does not check the meshes against their source.
- **The environment manifest** now hashes what its meshes are made from: the kit without its `look`, which only the importer's materials read (`Game/Scripts/EnvironmentKit/inputs.py`). CI checks the committed manifest against the kit, so a look change rebuilds materials and never regenerates the 33 meshes.
- **Unchanged meshes keep their files.** Blender stamps each FBX with its export time and numbers its objects afresh in every process, so a re-export rewrites bytes that hold nothing new.
  - The environment and Fluxborn generators therefore export to a scratch file and keep the existing FBX when its content is unchanged (`Game/Scripts/fbx_content.py`). The kept file keeps its bytes and its recorded hash.
  - Content is the FBX's node tree. It excludes the export's time stamp, file ID and creation time, renumbers the objects by first appearance, and ignores a material's own values, because every importer builds its materials from the kit and takes only slot names.
  - Each asset in the manifest records its `contentSha256`, as the Vanguard bodies' do. CI tests the comparison on real exports.
  - Writing the new manifests rewrote no FBX.
- **Values:** every glow here settled at 1, measured in what the exposure maps to white. That is the glyph strength, the Flux `emission` of both kits, and the bodies' `glowGain` (ADR-064 §4). These colours are bright and saturated, so at 1 their glow reads as light in sun and shade and keeps its hue. Above that, the filmic tone curve washes them toward white: the enemy red turns salmon and the ally blue turns pale cyan, which weakens side identity.
- **Provisional:** the values, judged in the lit Crucible from the gameplay camera.
