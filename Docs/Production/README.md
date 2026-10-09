# Veyra Production Guidance

This directory defines **how Veyra's visual world and client-facing presentation are produced, reviewed, and validated**. It does not replace game-design canon, approved client behavior, the art direction, or repository architecture.

## Authority order

When documents overlap, use this order:

1. [`ARCHITECTURE.md`](../../ARCHITECTURE.md) and accepted [ADRs](../ADR/) own technical boundaries and approved project-level technology decisions.
2. Current files in [`Docs/Design/`](../Design/) own gameplay rules, world canon, battleground topology, and art direction.
3. [`Game/Tuning/World.json`](../../Game/Tuning/World.json) is the current machine-readable gameplay-layout source for the Meridian Crucible under [ADR-011](../ADR/ADR-011-battleground-runtime.md). Production tooling must consume it rather than inventing a second layout.
4. The production documents in this directory own the **repeatable method** for creating, assembling, validating, and regenerating world content around that gameplay source.
5. Generated Unreal maps, imported meshes, PCG output, screenshots, and baked files are results/evidence, not higher authority than their source data or generator.

If a production task would require changing gameplay topology, art canon, an architecture rule, the meaning of `World.json`, an engine/plugin policy, or another unresolved project-level decision, stop and surface that decision rather than hiding it in an asset or editor change.

## Mandatory production documents

- [`VEYRA_WORLD_PRODUCTION_BIBLE.md`](VEYRA_WORLD_PRODUCTION_BIBLE.md) — how the Meridian Crucible is authored as a playable world: the existing `World.json` layout, generated battleground map, protected gameplay geometry, terrain, water, PCG, environmental state, and agent review.
- [`VEYRA_ASSET_AND_VFX_PIPELINE.md`](VEYRA_ASSET_AND_VFX_PIPELINE.md) — how Blender and Unreal divide responsibility for meshes, materials, foliage, animation, Niagara, dynamic effects, and reusable asset-family generators.
- [`VEYRA_WORLD_VALIDATION_STANDARD.md`](VEYRA_WORLD_VALIDATION_STANDARD.md) — acceptance gates for generated assets and map changes: reproducibility, navigation, readability, performance, screenshots, cooking/packaging, and rollback.
- [`VEYRA_VISUAL_PRODUCTION_STANDARD.md`](VEYRA_VISUAL_PRODUCTION_STANDARD.md) — cross-game presentation quality gates for client, HUD, Vanguards, animation, VFX, Crucible, rendering, accessibility, profiling, and author art approval.
- [`VEYRA_VISUAL_UPGRADE_ROADMAP.md`](VEYRA_VISUAL_UPGRADE_ROADMAP.md) — baseline audit, reusable presentation systems, vertical-slice sign-off, phased rollout, and hardening sequence.
- [`VEYRA_VISUAL_REVIEW_TEMPLATE.md`](VEYRA_VISUAL_REVIEW_TEMPLATE.md) — fillable implementation/PR evidence, captures, measurements, defect grading, and sign-off.

Visual work must also read [`Docs/Design/Art_Direction_v0.1.md`](../Design/Art_Direction_v0.1.md). Crucible layout/world-authoring work must also read [`Docs/Design/Veyra_Battleground_Bible_v0.9.md`](../Design/Veyra_Battleground_Bible_v0.9.md), [ADR-011](../ADR/ADR-011-battleground-runtime.md), and [ADR-040](../ADR/ADR-040-crucible-world-authoring-toolchain.md).

## Core production doctrine

**Gameplay defines the world. Art dresses the gameplay.**

**Blender authors source assets. Unreal owns the shipped game and runtime presentation.**

**The macro Crucible is authored from reviewed gameplay data; the micro Crucible may be procedural.**

**Generated output is reproducible output.** Seeds, generator versions, inputs, and validation results must be recorded.

**Competitive geometry is protected.** Decorative generation may not silently change lane widths, jungle connectivity, river crossings, objective access, structure approach space, Dense Fog placement, collision, navigation, sightline-critical geometry, or other competitive topology.

**Readable first, spectacular second.** Visual richness must preserve Vanguard, ability, objective, and navigation readability from the actual gameplay camera.

## Existing battleground build path

The repository already has a reproducible map pipeline:

```text
Docs/Design/Veyra_Battleground_Bible_v0.9.md
        ↓
Game/Tuning/World.json
        ↓
UVeyraBattlegroundMapCommandlet
        ↓
Game/Scripts/BuildBattlegroundMap.ps1
        ↓
Game/Content/Veyra/World/Maps/L_Battleground.umap
```

That path remains the gameplay-layout spine.

Future art/terrain/water/PCG tooling must extend around it or deliberately evolve it through an ADR/reviewed migration. Do not create a second private copy of lane, structure, camp, Dense Fog, or Flux Well coordinates inside Blender, a Blueprint, a PCG graph, or a hand-edited map.

## Agent workflow

For substantial world/art work:

```text
read context map + canon + production rules
        ↓
identify gameplay/layout source
        ↓
identify visual source/generator
        ↓
edit source data/generator/source asset
        ↓
validate source
        ↓
generate/export/import
        ↓
regenerate only the affected world scope
        ↓
run gameplay/navigation/performance checks
        ↓
capture standard Unreal screenshots
        ↓
visually review the actual in-engine result
        ↓
iterate at the source
```

Do not hand-edit a generated result when the correct fix belongs in its generator, source file, `World.json`, shared material/VFX system, or another authoritative owner.

## Engine target

The repository targets **Unreal Engine 5.8.3 built from Epic's source repository** under [ADR-001](../ADR/ADR-001-unreal-version-policy.md). [ADR-040](../ADR/ADR-040-crucible-world-authoring-toolchain.md) makes that engine checkout read-only by default for project work and places Crucible generation/editor automation in the project-owned `Game/Plugins/VeyraWorldTools/` plugin. Production guidance must be interpreted against the version pinned by the repository.

Experimental engine features are not automatically approved merely because UE 5.8 contains them. Their use must follow ADR-001 and the specific cautions in these documents.
