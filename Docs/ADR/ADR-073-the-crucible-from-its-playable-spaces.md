# ADR-073: The Crucible from its playable spaces

**Status:** Proposed. On 2026-10-12 the author reviewed a whole-map spatial plan and called it "SO much better, layout wise". They added notes: more camps, the river must not run off the map, wider mid Spires, Spires beside the road, and more Dense Fog including on lane edges. They then asked for it to be built "in a way that looks good", using "the best system to build the Crucible", whether that is Mesh Terrain or something else, chosen on evidence. This record sets the data model and the build system. It becomes Accepted when the author merges the pull request that adds it.
**Date:** 2026-10-12
**Related:**
- [ADR-043](ADR-043-map-walls-and-terrain-sight.md): map walls as layout data (§1), raised as terrain (§2), blocking sight (§3). §1–§2 and its 2026-10-05 natural-walls amendment are superseded here; §3's rule stands, fed by new pieces.
- [ADR-040](ADR-040-crucible-world-authoring-toolchain.md): §3, Landscape as the terrain substrate, amended by §5 here; §5, World.json as the spatial authority, kept; §7, PCG never decides topology, kept.
- [ADR-011](ADR-011-battleground-runtime.md) §12: the layout is tuning, hashed and compared at login.
- [ADR-001](ADR-001-unreal-version-policy.md): Experimental engine features are not approved for production by their presence in the engine.
- [Battleground Bible](../Design/Veyra_Battleground_Bible_v0.9.md): §2 (redesign authority, terrain fairness, half-turn symmetry), §6 (Flux Wells), §7 (jungle geometry), §8 (wildlife), §10 (Spires), §11 (Dense Fog).
- [World Production Bible](../Production/VEYRA_WORLD_PRODUCTION_BIBLE.md) §2.1, §4 and §6, and the [World Validation Standard](../Production/VEYRA_WORLD_VALIDATION_STANDARD.md) gates 5–8.

## Context

**What the author rejected:**
- **Wall splines produced strips.** Today `World.json` authors walls as variable-width spline strips in Team A's half (ADR-043 amendment, 2026-10-05). The generator raises each strip as a ridge, so the jungle reads as an open field crossed by raised strips. Two redesign passes on that model were rejected on 2026-10-12:
  - walls consumed playable jungle;
  - bases had no physical boundary or intentional entrances;
  - Dense Fog sat inside lanes;
  - the composition didn't add up as a whole map.
- **The model is the obstacle.** Strips are drawn first and the playable space is whatever is left, so no rule can protect a clearing's size or a route's width.

**The plan the author approved** reverses this:
- Lay out the playable spaces first: lanes, walled keeps with gates, camp clearings, routes of three widths, the river with its Wells and falls.
- Treat what remains as rock and forest, with no leftover strip thinner than 300.

Its source and measurements are in `Game/Review/Crucible/2026-10-12-spatial-plan/`.

**Terrain technology, evaluated against this map on 2026-10-12.** Findings verified in the engine install and Epic's UE 5.8 documentation:
- **Mesh Terrain is two Experimental plugins** (`MeshTerrainMode`, `MeshPartition`; both `.uplugin` files set `IsExperimentalVersion`).
- **It requires World Partition.**
  - The editor subsystem refuses a world without it, and its builder runs only through the World Partition builder commandlet.
  - `L_Battleground` is not partitioned, and ADR-040 §9 forbids adopting World Partition without a measured need.
- **Its edges only approximate the gameplay polygons.** Each section's surface is remeshed and simplified, so a cliff's footprint only approximates the polygon behind it.
- **Ground and cliff share one collision mesh** under one profile. That conflicts with the `VeyraGround` channel rule that a ground query never finds a wall.
- **Its strengths don't apply here.** It's built for overhangs, tunnels, variable resolution and streaming large worlds. An 18,400-unit arena viewed from above needs none of them.
- **Geometry Script can build what this map needs.** The `GeometryScripting` plugin, which isn't flagged Experimental, has every operation required:
  - polygon offsets and morphological opening;
  - sweeping a profile around a closed polygon;
  - triangulating caps;
  - noise, bevels, box-projected UVs and vertex colours;
  - saving a static mesh asset with optional Nanite.
- **The Landscape can't draw a cliff.** It samples about every 44 cm, so a cliff can only be a slope, and diagonal edges stair-step.
- **Polygon booleans are not available to runtime modules.** They live in `GeometryAlgorithms`, inside the Beta `GeometryProcessing` plugin, which the game doesn't enable. Runtime geometry must not depend on them.

## Decision

### 1. World.json authors the open spaces (schema 8)

`layout` loses `walls`, `wallSamplesPerSegment` and `wallHalfHeight`. It gains the playable spaces, all in Team A's half, with Team B's given by the half turn about the centre:

- **`keep`:** the keep's floor polygon and its wall runs.
  - A wall run is architecture: a polyline with a thickness, broken only where a **gate** stands.
  - Each gate has a centre, a width and the lane it serves.
  - `base.primeWell`, `base.fountain`, `base.baseTowers` and each lane's inhibitor keep their meaning. Inhibitors stand just inside their gates.
  - `padRadius` is retired: whatever asked "is this the base?" now asks whether a point is inside the keep polygon.
- **`clearings`:** circles `{id, center, radius, kind}`. Kind is `camp` (a wildlife camp's ground) or `glade` (a junction, river mouth or pocket).
- **`routes`:** polylines `{id, points, width}`. The widths come from `routeWidths`: rotation, approach or outer, values `1000`, `700` and `900`, all provisional.
- **`river`:** the existing course, plus `walkableEnd`. Past the outer fords, the river is no longer ground: it drops through a gorge of falls to the map's edge.
- **`fluxWells`:** each site's arena radius and island radius join the existing Well data.
- **`denseFog`:** banks of lobes, `{id, lobes[{center, radius}]}`. A bank may touch a lane shoulder's outer edge but never overlap a road or shoulder, and every lobe stays at least `fogStructureClearance` (500) from every structure.
- **`lanes`:** each Spire gains a side, which offsets it `spireOffset` (450) from the centre line.
  - +1 is the owner's left, seen walking from the owner's end, so the half turn keeps both teams' towers equivalent.
  - The mid lane's Spire distances become 800, 3,000 and 5,200.
- **`wildlife.camps`:** each camp gets an `id`.
  - A camp's ground is the clearing of the same id.
  - Two camps may share a species, so anything keyed by species is rekeyed by camp id (the validator's anchors, review cameras).
  - Each team has 8 camps. The two new ones reuse existing species provisionally until the author chooses their wildlife.
- **The map:** `halfExtent` becomes 10,000. Keep walls stand 4,500 from the centre, the side lanes run at 6,300, and the elbows are rounded at radius 3,800. All of these values are provisional.

### 2. Masses are generated data, guarded by a hash

Rock and forest masses are computed, never drawn. `Game/Scripts/BuildCrucibleMasses.py`, with a pinned `shapely`, computes the playable square minus the union of:
- lane envelopes;
- the walkable river band;
- Well arenas;
- keep floors;
- clearings;
- routes;
- the mid ford.

It then removes every piece thinner than `minMassThickness` (300) or smaller than `minMassArea` (25 m²), and simplifies each outline within `massSimplifyTolerance` (10).

It writes the result to `layout.masses`, the polygons of both teams, plus `layout.massesInputSha256`: the hash of the canonical JSON of every input above.
- **Masses are whole-map data.** A mass that crosses the river line would otherwise belong to neither half. The half-turn symmetry becomes a validation rule.
- **The hash is checked in CI without recomputing.** A Python test recomputes the inputs' hash and refuses a stale `masses`, which makes shapely a build tool only.
- **Runtime validation stays cheap.** No polygon booleans are needed. `VeyraWorld::Validate` checks that:
  - each mass is a simple polygon;
  - the set maps onto itself under the half turn, within 1 unit;
  - no mass edge sample lies inside an open space.

### 3. The runtime's terrain is the masses' edges

- **Collision is a ring along each mass edge.** Gameplay collision and sight need only the boundary, since nothing can stand inside a closed mass.
  - The edges become `FVeyraTerrainBox`es, each one `massEdgeThickness` (120) deep, lying just inside the edge.
  - They are mitred at corners, and long edges are split so no box is longer than `massEdgeMaxLength` (600).
  - Vision's grid prefilter sizes its cells by the largest box, so this keeps cells small. ADR-043 §3's rule that map terrain blocks sight is unchanged.
- **Keep walls are terrain too:** their runs, gates excluded, become boxes of their own thickness.
- **Every machine builds them locally.**
  - The layout is identical on every machine: hashed and compared at login (ADR-011 §12).
  - So the server and each client spawn their own non-replicated terrain: one actor per mass, holding that mass's edge boxes as components, with the server's nav obstacles on it.
  - This replaces about 380 always-relevant replicated wall actors with a few dozen local ones.
  - Ability walls (Forge Divide) keep ADR-032's replicated path.
- **The minimap** draws mass polygons, triangulated with `GeometryCore`, a production runtime module.
- **"Is this jungle?"** (`VeyraLayout::IsJungle`) means outside the keeps, lanes and river.

### 4. Validation

`CrucibleValidation.json` anchors camps by id and adds routes to the new camps, gates and fords. The validator also checks:
- **Thickness:** every mass is at least `minMassThickness` thick.
- **Clear entrances:** each clearing has at least two route entrances wide enough for its class.
- **Fog:** no fog overlaps a road or shoulder.
- **Spire placement:** Spires stand on shoulders, not roads.
- **Visual edge fidelity (§5):** measured, not assumed.

The existing navigation, sight and fairness gates keep their tolerances.

### 5. The look: a Landscape floor and masses extruded from the exact polygons (amends ADR-040 §3)

- **The Landscape stays the walkable floor:**
  - painted layers, grass, causeways, river banks and gentle relief;
  - the `VeyraGround` channel and everything that stands on it, unchanged;
  - no ridges or cliffs from it any more.
- **Each mass is a generated static mesh**, built in `VeyraWorldTools` with Geometry Script from the same polygon the runtime uses:
  - a stylized cliff profile (lip, chamfer, ledge) swept around the outline;
  - a capped top;
  - noise that only ever pushes inward;
  - box-projected UVs and vertex colours for moss tops and slate faces.

  Its footprint at walking height lies within `massVisualTolerance` (10) of the gameplay edge. Its base sinks below the Landscape. It carries **no collision**, since §3's terrain is the only collision.
  - **Rock masses** stand tall and sheer.
  - **Forest masses** are lower, with a canopy dressed on top by the existing kit and PCG, which only dresses (ADR-040 §7).
  - Which kind a mass is follows a data rule from its size, with per-mass overrides.
- **Keep walls** are built from the architectural kit along their runs, with arches at the gates.
- **The gorge** at each river end is a generated mesh, and the falls are presentation.
- **Engine dependencies are editor-only.** `GeometryScripting` and its `GeometryProcessing` dependency are enabled with `"TargetAllowList": ["Editor"]`. The client and the Linux server ship only plain static meshes, and the server strips their render data.
- **Nanite is optional,** chosen by measurement.
- **Builds are reproducible.** Team B's meshes are Team A's, turned. A mesh whose inputs are unchanged keeps its bytes, as the Vanguard bodies' `fbx_content` rule does, so nothing changes in LFS for no reason.
- **Mesh Terrain is not adopted.** It would bring two policy decisions with it (World Partition, and an Experimental shipping dependency under ADR-001), approximate edges, and a single collision mesh for ground and cliff. It's reconsidered only if the §6 prototype fails the art bar, as a spike on a throwaway World Partition map, after an ADR-001 review.

### 6. A prototype gate before the whole map

Before the whole map is generated, one vertical slice is built and measured: Team A's top inner jungle with its lane and the keep's top gate. That's three masses (one with an acute corner, one with a concave notch, one on a river bank), a keep wall run and a gate. It must pass:

1. **Edge fidelity:** the mesh footprint lies within `massVisualTolerance` of the polygon, measured, with debug polygons over gameplay-camera captures.
2. **Collision agreement:** units pushed against each edge show no gap and no clipping at the gameplay camera.
3. **Occlusion:** units behind masses of two heights stay readable. A mass of height H hides about 0.58·H of ground behind it at −60°. Mass heights become data from this.
4. **Seams:** where mass feet meet the Landscape, with and without blending.
5. **Determinism:** two builds hash identically, and Team B's meshes match Team A's turned.
6. **Performance:** a packaged client with `Smoke.ps1 -PerfSeconds`, Nanite on and off, against ADR-040's 5.84 ms GPU baseline.
7. **Server:** a Linux server cook carries no mass render data, and its navigation time and memory are unchanged.
8. **Art:** the author reviews the slice against today's look on the same region.

## Consequences

- **Superseded and amended records:**
  - ADR-043 §1–§2 and its natural-walls amendment are superseded; §3 stands, fed by §3's pieces.
  - ADR-040 §3 is amended: cliffs, ridges and shelves come from generated masses, not Landscape relief.
  - ADR-011 §12's layout contents and §17's values change with schema 8.
- **Tests that pin walls are rewritten:** wall validation and rotation, wall boxes, terrain relief over walls, walls over the network, minimap walls, and the greybox's two pads.
- **Docs:** the Battleground Bible §2 and §7 describe the playable-space model, and its index locator is regenerated. The World Production Bible §6.2 describes masses.
- **A new editor-only dependency:** VeyraWorldTools gains Geometry Script, editor-only.
- **Generated data:** World.json gains a generated section. CI refuses it when stale, and only the script writes it.
- **Pass14's reusable parts are carried over,** from local commit `0e96ba97`:
  - the validator's inscribed-square probe fix;
  - map fog kept off lanes, with its test;
  - the 30-route navigation suite.

## Open values (provisional)

- **Map:** half extent 10,000.
- **Keep:** walls at 4,500, gates 900 wide.
- **Routes:** widths 1,000, 700 and 900.
- **Clearings:** camp radius 800, Skittermaw 900.
- **Masses:** minimum thickness 300 and area 25 m²; edge boxes 120 deep and at most 600 long; visual tolerance 10.
- **Fog:** at least 500 from structures.
- **Spires:** offset 450, sides alternating.
- **Mid Spire distances:** 800, 3,000 and 5,200.
- **The two new camps per team** reuse existing species until the author chooses.
