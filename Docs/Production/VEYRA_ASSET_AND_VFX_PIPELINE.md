# Veyra Asset & VFX Pipeline

**Version:** 0.1  
**Status:** Production guidance for Veyra-authored visual assets and effects  
**Engine target:** Unreal Engine 5.8  
**Read with:** [Context Map](../CONTEXT_MAP.md), [Architecture](../../ARCHITECTURE.md), [Art Direction](../Design/Art_Direction_v0.1.md), [World Production Bible](VEYRA_WORLD_PRODUCTION_BIBLE.md), [World Validation Standard](VEYRA_WORLD_VALIDATION_STANDARD.md), [Visual Production Standard](VEYRA_VISUAL_PRODUCTION_STANDARD.md)

## 1. Purpose

This document defines how agents build visual content for Veyra without creating a pile of one-off meshes, one-off materials, one-off Blueprint behaviors, and one-off effects that cannot be maintained later.

The governing split is:

> **Blender authors source assets. Unreal owns the shipped game and runtime presentation.**

Blender may generate and prepare geometry. Unreal owns final materials, runtime VFX, gameplay-driven state, lighting response, collision integration, scalability, and final validation.

The second governing rule is:

> **When a request represents a reusable visual family, build or extend a generator instead of solving it as a one-off.**

The intended long-term result is an internal **Veyra Asset Forge**: coherent, deterministic, inspectable, and safe for Claude/Codex to iterate on directly.

## 2. Responsibility matrix

| Concern | Blender | Unreal |
|---|---|---|
| Static source geometry | **Owns** | Imports/uses |
| Procedural mesh-family generation | **Owns** | Requests/reimports |
| UVs / source normals / source transforms | **Owns** | Validates |
| Authoring pivots / sockets/helpers | **Owns** | Consumes |
| Preview materials | Optional | Not authoritative |
| Shipping materials/shaders | No | **Owns** |
| Runtime particles | No | **Niagara owns** |
| Runtime gameplay-driven VFX | No | **Owns** |
| Runtime material animation | No | **Owns** |
| Skeletal source rig/animation | May author | **Runs/integrates** |
| Final gameplay collision | May provide helpers | **Owns** |
| Nanite settings | Prepares compatible geometry | **Owns** |
| Foliage runtime wind/interaction | Source mesh only | **Owns** |
| Runtime destruction/physics | May author pieces | **Unreal/approved gameplay system owns** |
| Gameplay state | Never | **Authoritative C++ systems own** |
| Final visual validation | Preview only | **Authoritative** |

If Blender can simulate something, that does not mean Blender owns the runtime effect.

## 3. Asset Forge strategy

The project should establish a reusable generator framework rather than asking Codex to make every environmental mesh from scratch independently.

Likely generator families include:

```text
RockGenerator
CliffGenerator
TreeGenerator
RootGenerator
RuinGenerator
PillarGenerator
ArchGenerator
WallGenerator
BridgeGenerator
RubbleGenerator
FluxCrystalGenerator
FluxConduitGenerator
FluxCorruptionGenerator
GroundPropGenerator
```

Exact repository paths are an implementation decision, but generator scripts/source scenes must be version-controlled and discoverable.

### 3.1 Reusable-family rule

Before creating an asset, ask:

1. Is it truly unique?
2. Does a generator for this family already exist?
3. Can an existing generator be extended cleanly?
4. Will the world need variations?
5. Is there a shared shape/material language this object should inherit?

If it is a family, improve the family generator.

Do not create:
- dozens of unrelated hand-coded Blender scripts;
- `SM_Rock_Final_Final2`;
- a separate material architecture for one prop;
- unique wind logic for one tree;
- separate Niagara systems that differ only by color.

## 4. Generator requirements

Every production generator must:
- accept an explicit seed;
- expose meaningful named parameters;
- produce stable output names;
- record generator/version metadata;
- avoid dependence on hidden Blender UI state;
- fail clearly on invalid inputs;
- support command-line/headless execution where practical;
- preserve the source scripts/scenes needed to regenerate output;
- reproduce the same result from the same generator version, seed, and inputs.

Randomness without a recorded seed is prototype behavior, not production behavior.

### 4.1 Shared Veyra visual grammar

Generators should consume shared style profiles rather than each inventing unrelated defaults.

Profiles should eventually describe approved ranges for:
- silhouette exaggeration;
- edge softness/bevel language;
- proportion;
- structural massing;
- surface-detail frequency;
- erosion;
- fracture/damage;
- asymmetry;
- architectural motifs;
- Flux geometry;
- foliage shape language.

The Art Direction document owns the look. The generator profile translates that look into repeatable production controls.

## 5. Blender source standard

### 5.1 Units and scale

Unreal uses centimeters. The pipeline must consistently export at correct scale without per-asset guesswork.

Before export:
- units are deliberate;
- transforms are intentional/applied where appropriate;
- negative scale is resolved unless specifically required;
- object origin is deliberate;
- forward/up conversion is handled by a shared exporter;
- names are stable.

The exporter should own routine conversion. Do not rely on each agent remembering UI checkboxes.

### 5.2 Pivots

Choose pivots for intended use:

- ground props/rocks: stable ground-contact pivot;
- modular architecture: grid/snap-friendly pivot;
- doors/gates: hinge/rotation point;
- trees: trunk base;
- hanging assets: attachment point;
- VFX helper meshes: emitter/material behavior;
- bridge/modules: modular seam/grid origin.

A pretty mesh with a useless pivot is unfinished.

### 5.3 Geometry health

Source validation should catch:
- stray vertices;
- accidental duplicate geometry;
- unintended internal faces;
- broken normals;
- invalid smoothing;
- unresolved modifier dependencies;
- pathological microgeometry;
- unwanted non-manifold regions where manifoldness is expected.

Nanite does not excuse broken source geometry.

### 5.4 UV strategy

Every asset declares one primary texture strategy:

- tileable/world-aligned;
- trim sheet;
- unique unwrap;
- atlas;
- vertex color/mask;
- procedural material.

Do not automatically create unique 4K texture sets just because a mesh can be unwrapped.

The Crucible terrain importer side-projects cliff colour and packed surface data,
and uses the shared world-aligned normal helper for steep faces. Top-projected
overgrowth fades out with the same slope mask that introduces rock. The environment
kit blends moss normals with moss colour coverage, so moss-covered caps do not
inherit exposed slate grooves. Both importers share `world_aligned` in
`Game/Scripts/veyra_material_graph.py`; its normal output uses the engine function's
default tangent-space convention.

Environment moss colour and foliage detail use the full `XYZ Texture` projection,
including upward-facing surfaces. Do not use the side-only `XY Texture` output
as a top projection: it stretches into directional bands on caps and canopies.
Review both tops and steep sides from `Play_Jungle_A0` and `Play_Wall_03` after
rebuilding these shared materials.

### 5.5 Material slots

Keep slots intentional.

One rock should not become five draw-material sections because the Blender source was convenient to author that way.

Prefer shared Veyra materials plus masks.

### 5.6 Collision helpers

Blender may generate simplified collision/helper geometry, but Unreal owns the final collision policy.

Render detail and gameplay collision are separate concerns.

## 6. Naming

Use consistent Unreal-style prefixes.

Recommended baseline:

```text
SM_   Static Mesh
SK_   Skeletal Mesh
T_    Texture
M_    Master Material
MI_   Material Instance
MF_   Material Function
NS_   Niagara System
NE_   Niagara Emitter
PCG_  PCG asset
DA_   Data Asset
BP_   Blueprint presentation/assembly asset
RT_   Render Target
VT_   Virtual Texture
```

Generated families should be understandable:

```text
SM_CrucibleRock_Large_A_001
SM_CrucibleRock_Large_A_002
SM_CrucibleArch_Broken_B_004
```

Keep seeds/parameter manifests in metadata rather than stuffing every parameter into the filename.

## 7. Export/import path

Normal path:

```text
generator/source scene
        ↓
source validation
        ↓
automated export
        ↓
Unreal import/reimport
        ↓
Veyra material assignment
        ↓
Nanite/collision/settings
        ↓
benchmark scene
        ↓
in-engine screenshots
        ↓
accept / iterate at source
```

Prefer reproducible exporter/importer scripts over repeated manual editor configuration.

Import validation should detect:
- wrong scale;
- suspicious bounds;
- missing source metadata;
- name collisions;
- wrong content directory;
- missing material mapping;
- missing collision policy;
- unexpected transform changes on reimport.

## 8. Material architecture

Veyra should use shared material families rather than bespoke graphs per asset.

Conceptually:

```text
M_Veyra_Surface
M_Veyra_Rock
M_Veyra_Architecture
M_Veyra_Foliage
M_Veyra_Water
M_Veyra_Flux
M_Veyra_Decal
```

Exact graph inheritance may evolve.

### 8.1 Centralized controls

Share reusable controls for:
- palette response;
- base color;
- roughness/specular treatment;
- macro variation;
- normals/detail normals;
- edge/wear response;
- dirt/moss/wetness;
- terrain blending;
- distance treatment;
- world-position variation;
- Flux masks/emissive;
- foliage wind;
- quality/scalability switches.

A global Veyra art-direction change should be possible without editing hundreds of independent material graphs.

**Glow under physical light.** The Crucible is lit physically, under a manual exposure that maps an emissive of a few units to black ([ADR-040](../ADR/ADR-040-crucible-world-authoring-toolchain.md)). Every generated glow is therefore scaled by the inverse of the scene's exposure, through one shared helper (`unexposed` in `Game/Scripts/veyra_material_graph.py`). Its strength is data in its spec or kit, in multiples of what the exposure maps to white. A new emissive material uses the helper; it never uses a raw emissive value tuned by eye under one light. A bright, saturated glow keeps its hue at about 1. Much above that, the tone curve washes it toward white, so a side colour should stay near 1. An effect meant to flare white-hot can go higher.

### 8.2 Texture packing

Do not invent a new channel convention per asset.

Baseline packed mask, unless a family documents otherwise:

```text
R = Ambient Occlusion
G = Roughness
B = Metallic
A = Reserved/family-specific mask
```

A different profile is allowed only when named and documented.

### 8.3 Painterly-realistic from gameplay distance

The Art Direction calls for painterly-realistic non-human materials with believable physical response.

That does **not** mean maximum microdetail.

From Veyra's gameplay camera:
- silhouette matters first;
- large value breakup matters;
- material separation matters;
- edge language matters;
- microdetail must not become combat noise.

Judge materials in the Crucible, not only on a close-up sphere.

The generated Crucible moss exposes `cushionContrast` and `cushionRelief` in
`Game/ArtSource/Environment/Terrain/TerrainTextures.json`, each in [0, 1]. These
control fine cushion colour and height contrast independently of broad moss/soil
coverage. Keep small cushions subordinate to the broad colour field at gameplay
distance; verify shared use on ground, cliff caps and foliage after regeneration.

For the Shore layer, tune pebble coverage, roughness and normal strength together:
visible sand between pebble drifts should remain legible at the crossing cameras.
Texture tuning does not validate or alter the bank footprint, crossing geometry
or decorative placement. Review wet-edge glare separately from dry-bank detail.

River surface response is owned by `water` in
`Game/ArtSource/Environment/Terrain/TerrainTextures.json`. Review roughness at the
overview, all three gameplay crossings and both Flux Well cameras: a reduction
in peak glare must preserve water colour and bank readability. Reproduce a
water-only material iteration with `Game/Scripts/BuildTerrainArt.ps1 -WaterOnly`;
this regenerates the source texture manifest, imports only the water texture and
rebuilds `M_CrucibleWater`. The terrain material and map are not rebuilt. Use
`-ImportOnly` only when the manifest already matches the source profile. Capture
both high and low profiles through `Game/Scripts/CaptureBattleground.ps1` before
accepting the result; still captures do not validate moving reflections or combat.
`water.specular` supplies the `WaterSpecular` material parameter explicitly;
roughness and specular strength should be judged independently, since a broader
reflection can obscure more of the river even when its peak is less bright.

## 9. Nanite

### 9.1 Default candidates

Suitable static environmental geometry should be Nanite-ready by default:

- rocks;
- cliffs;
- ruins;
- architecture;
- statues;
- large roots;
- hero environmental props;
- dense static formations.

Nanite readiness means:
- source geometry is healthy;
- materials are compatible;
- collision is separate and intentional;
- runtime deformation assumptions are valid;
- fallback behavior is understood.

### 9.2 Nanite does not remove every budget

Still control:
- material slots;
- texture memory;
- translucency/overdraw;
- instance count;
- shadow cost;
- collision cost;
- hidden geometry;
- source/binary size.

### 9.3 UE 5.8 foliage caution

UE 5.8 contains newer Nanite foliage paths/features, but experimental engine features are not automatically approved under ADR-001.

Therefore:
- do not make the Crucible depend on an Experimental foliage feature without explicit approval;
- benchmark such features in isolated test content when authorized;
- keep source assets compatible with an approved fallback;
- validate canopy behavior, deformation, shadows, distance behavior, and performance in the actual engine.

## 10. Foliage

Trees are **Veyra MOBA trees**, not generic high-detail forest assets.

The current [Crucible environment kit](../../Game/ArtSource/Environment/README.md)
uses Two Sided Foliage shading for its thin grass/reed surfaces, with the existing
data-owned surface colour also supplying transmission colour. Solid canopy masses
retain Default Lit shading. Rebuild these shared materials through the importer;
judge their response under the Crucible's light at gameplay distance before raising
tints to compensate for dark back faces.

The environment generator's tree and shrub families expose `leafClusters`,
`clusterFlatten` and `edgeLeaves` for layered crowns with pointed silhouette
detail. Each generated cluster is bounded by its source mass ellipsoid; the
generator does not place trees or expand gameplay blockers. Grass and reeds expose
`bladeWidthRatio`, `leanRatio` and `bladeFold` for folded, tapered blades. All
controls live in `CrucibleKit.json`; regenerate the FBX and manifest, then import
through the existing collision-free environment pipeline. Check triangle budgets,
canopy obstruction, wind and shadows in Unreal; source bounds alone do not prove
competitive readability or acceptable GPU cost.

A tree generator should expose art-directable controls such as:

```text
species_profile
trunk_height
trunk_thickness
crown_width
branch_bias
canopy_density
canopy_cluster_size
asymmetry
age
damage
root_exposure
flux_corruption
seed
```

### 10.1 Gameplay-camera constraints

Foliage must:
- read from top-down;
- preserve Vanguard silhouettes;
- avoid opaque canopy coverage of critical combat space unless an approved fade/cutaway behavior exists;
- keep lane/objective areas less noisy than inaccessible scenery;
- avoid falsely implying path blockers;
- avoid motion that competes with ability telegraphs.

### 10.2 Wind

Blender prepares geometry/vertex data for wind.

Unreal owns actual runtime wind.

Prefer shared material/foliage wind functions. Never solve ambient tree movement with per-tree Blueprint Tick.

## 11. Dynamic-effect decision tree

When something moves or changes, use the simplest correct owner:

```text
Surface-only motion/appearance?
    → Material / vertex shader

Many lightweight independent particles/meshes?
    → Niagara

Trail, sparks, mist, embers, splash, motes?
    → Niagara

Needs bones / authored deformation?
    → Skeletal Mesh + animation

Can repeated deformation be baked efficiently?
    → Evaluate VAT / approved baked method

Permanent static result?
    → Bake into source geometry

Needs gameplay physics?
    → Unreal physics / Chaos through gameplay-owned logic

Needs authoritative gameplay state?
    → C++ gameplay/world system; presentation only observes it
```

Do not use the most complex system simply because it is visually impressive.

## 12. Niagara

Niagara is the primary runtime particle/VFX system.

Use Niagara for:
- ability particles;
- Flux motes;
- sparks;
- embers;
- smoke/mist;
- dust;
- splash/spray;
- ribbons/trails;
- impact debris;
- environmental particle fields;
- lightweight particle meshes.

### 12.1 Effect-event boundary

Gameplay code should not scatter arbitrary Niagara asset references everywhere.

Prefer a standardized presentation request/event layer. Conceptually:

```text
Impact
WaterImpact
GroundImpact
FluxPulse
StructureBreak
Burn
Heal
ShieldBreak
Recall
WellCapture
```

The exact API is an implementation decision.

Gameplay reports **what happened**. Presentation resolves **how it looks**.

### 12.2 Niagara is never authoritative

Niagara does not decide:
- hits;
- damage;
- crowd control;
- capture progress;
- objective state;
- vision;
- pathing;
- persistent gameplay collision;
- victory.

A visible projectile may follow an authoritative projectile; the visual emitter is not the projectile authority.

### 12.3 Parameterization

Prefer parameters over duplicate near-identical systems.

Useful parameters:
- team/Flux color;
- intensity;
- scale;
- source/target transform;
- surface type;
- quality level;
- state/progression stage.

Gameplay tuning still comes from the gameplay/tuning owner.

## 13. Blender particles and simulations

Blender may be used to:
- prototype motion;
- generate debris meshes;
- create source mesh particles;
- create ribbon/card geometry;
- render masks/textures;
- render flipbook source frames when justified;
- bake data for an approved Unreal runtime technique.

A Blender particle system is an **authoring tool**, not a runtime system.

If a Blender simulation is baked for shipping, document:
- why baking is preferable;
- bake format;
- loop/frame assumptions;
- memory cost;
- scalability/fallback;
- regeneration source.

## 14. Flux visual system

Flux should be a shared visual language, not "blue emissive everywhere."

The shared system may provide:
- geometric network channels;
- animated flow masks;
- emissive pulses;
- crystal/core response;
- corruption/cleansing states;
- Niagara motes/arcs;
- directional energy;
- water contamination;
- structure activation.

Form matters as much as hue.

Gameplay state comes from Veyra's authoritative systems; materials/Niagara observe it.

## 15. Water VFX

Water interactions stay in Unreal.

Typical split:

```text
Base water surface       → approved water renderer/material
Broad flow               → material / water parameters
Small ripples            → material and/or Niagara
Footstep wake            → Niagara/material interaction
Ability splash           → Niagara
Bank foam                → material + masks / approved method
Waterfall mist           → Niagara
Flux-in-water state      → shared water/Flux material parameters
```

Do not build general fluid simulation if shader/Niagara presentation is sufficient from the gameplay camera.

## 16. Destruction and deformation

Classify destruction before implementing it.

### Cosmetic

Examples:
- dust;
- non-colliding fragments;
- decals;
- shader cracks;
- small debris.

Presentation systems may own this.

### Gameplay relevant

Examples:
- changes collision;
- creates/removes path;
- creates cover;
- changes vision;
- persists as authoritative world state.

This requires gameplay/world ownership and replication.

Do not smuggle gameplay destruction in through Chaos/Niagara because the effect looks good.

## 17. Lighting hooks

Assets should work under shared Crucible lighting.

A prop may have:
- emissive material;
- justified local light;
- effect light;
- material response.

A prop task may not "fix" itself by changing global exposure or world lighting.

## 18. Surface metadata

Environment surfaces should carry semantic surface types so shared systems can route:
- footsteps;
- impact particles;
- impact audio;
- decals;
- dust/mud;
- water response;
- Flux interactions where needed.

Do not use mesh-name string checks in gameplay code.

## 19. Asset validation before export

Where applicable:

```text
[ ] stable name
[ ] generator/version metadata
[ ] explicit seed
[ ] correct scale
[ ] correct orientation
[ ] transforms intentional
[ ] pivot correct
[ ] normals/tangents valid
[ ] no accidental duplicate/internal geometry
[ ] material slots intentional
[ ] UV strategy declared
[ ] collision strategy declared
[ ] deformation strategy declared
[ ] source retained
[ ] export reproducible
```

## 20. Unreal validation after import

```text
[ ] scale/bounds correct
[ ] Veyra master material family assigned
[ ] mask/texture profile correct
[ ] Nanite decision validated
[ ] collision correct
[ ] navigation impact correct
[ ] shadows correct
[ ] gameplay-camera silhouette/readability correct
[ ] benchmark screenshots reviewed
[ ] performance profile passes
[ ] cook/package passes
```

The World Validation Standard owns the full acceptance gates.

## 21. Benchmark scenes

Maintain small controlled scenes for:
- rocks/terrain;
- architecture;
- foliage;
- water;
- Flux;
- VFX;
- combat readability.

A generator/material system should pass its benchmark before it is allowed to spread across the Crucible.

This reduces the chance that a bad global generator change pollutes thousands of placed assets.

## 22. Screenshot-driven iteration

Blender answers:
> Is the source mesh plausible?

Unreal answers:
> Does it belong in Veyra and work in the actual game?

Agents should review in-engine captures and fix the correct owner:

- weak silhouette → source/generator;
- inconsistent surface → shared material;
- bad placement → PCG/world;
- excessive motion → Niagara/material;
- visual obstruction → composition/camera/foliage;
- frame cost → performance/scalability.

## 23. Provenance and AI-access policy

Every non-Veyra-authored asset/reference entering production requires a provenance record:

- creator/source;
- license;
- commercial-use status;
- modification rights;
- redistribution restrictions;
- AI-use restrictions;
- attribution requirement;
- acquisition date/version.

For content marked or licensed **NoAI / not allowed as generative-AI input**, Claude/Codex must not ingest, visually analyze, transform, embed, or otherwise use the protected content as generative-model input.

Repository access does not automatically grant AI-input permission.

Because the Veyra workflow intentionally uses agent screenshot review and asset inspection, **Veyra-authored assets or third-party assets explicitly compatible with that workflow are strongly preferred**.

## 24. Source control and binaries

ADR-006 already routes Unreal binary assets under `Game/` through Git LFS.

Asset-production rules:
- source Blender files and exports must follow the repository's chosen binary-storage policy;
- lock Unreal binaries before manual edits where required;
- generated outputs should link back to source/generator metadata;
- avoid opaque binary-only changes with no reproduction steps.

If binary volume outgrows Git LFS, follow the project's explicit source-control migration decision rather than improvising around it.

## 25. Open implementation decisions

This document intentionally leaves these unresolved until measured/approved:

- exact Asset Forge repository directory;
- exact Blender version pin;
- exact export format per asset category;
- exact texture-resolution/texel-density tiers;
- final master-material graph structure;
- final foliage runtime path;
- any Experimental UE 5.8 foliage feature;
- VAT/geometry-cache policy;
- exact effect-event API;
- final per-platform performance budgets.

Agents must not decide these incidentally.

## 26. Completion definition

An asset/VFX task is complete when:

1. the correct source/generator exists;
2. regeneration is deterministic;
3. source validation passes;
4. import/reimport is reproducible;
5. shared material/VFX architecture is respected;
6. runtime behavior belongs to the correct Unreal system;
7. gameplay authority remains outside presentation;
8. in-engine benchmark/gameplay screenshots are reviewed;
9. applicable performance/cook checks pass;
10. provenance is recorded;
11. the summary identifies source files, generator/version/seed, outputs, validation, and open decisions.
