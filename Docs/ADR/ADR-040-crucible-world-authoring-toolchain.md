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
