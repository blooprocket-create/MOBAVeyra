# Veyra World Validation Standard

**Version:** 0.2  
**Status:** Acceptance standard for Meridian Crucible world, asset, and VFX changes  
**Engine target:** Unreal Engine 5.8.3, Epic source build  
**Read with:** [Context Map](../CONTEXT_MAP.md), [Architecture](../../ARCHITECTURE.md), [World Production Bible](VEYRA_WORLD_PRODUCTION_BIBLE.md), [Asset & VFX Pipeline](VEYRA_ASSET_AND_VFX_PIPELINE.md), [Battleground Bible](../Design/Veyra_Battleground_Bible_v0.9.md), [Art Direction](../Design/Art_Direction_v0.1.md), [ADR-011](../ADR/ADR-011-battleground-runtime.md), [ADR-040](../ADR/ADR-040-crucible-world-authoring-toolchain.md)

## 1. Purpose

This document defines what must be true before agent-built visual/world content is considered complete.

A change is not accepted merely because:
- Blender exported it;
- Unreal imported it;
- the map opens;
- one beauty screenshot looks good;
- C++ compiles.

The final result must preserve **gameplay integrity, Veyra visual identity, competitive readability, reproducibility, runtime correctness, and performance**.

The core rule is:

> **Validate the shipped result in Unreal from the gameplay camera and gameplay conditions players will actually experience.**

## 2. Validation levels

Use the smallest level that covers the blast radius.

### Level A — source asset

For a Blender asset/generator/source change.

Validate:
- deterministic generation;
- geometry health;
- scale/orientation/pivot;
- UV/material policy;
- provenance;
- export reproducibility.

### Level B — Unreal asset

For an imported mesh/material/VFX asset.

Validate Level A plus:
- import/reimport;
- material assignment;
- collision;
- Nanite decision;
- bounds/shadows;
- runtime behavior;
- benchmark screenshots;
- local performance.

### Level C — world region

For a lane segment, jungle area, river section, objective site, base area, or PCG region.

Validate:
- protected gameplay geometry;
- collision/navigation;
- gameplay-camera readability;
- PCG determinism;
- water/foliage/VFX interaction;
- regional performance;
- standard camera regression.

### Level D — full Crucible

For systemic changes such as:
- gameplay-layout changes;
- terrain framework;
- river/water renderer;
- global material changes;
- global foliage changes;
- global lighting;
- PCG framework;
- Flux world-state presentation;
- world-generation tooling.

Validate all prior levels plus:
- full-map route checks;
- functional symmetry/fairness;
- complete camera suite;
- representative 5v5 combat stress;
- scalability;
- cook/package;
- relevant smoke/headless paths.

A systemic change does not become Level B because it happened to be implemented in one file.

## 3. Gate 1 — authority/source review

Before evaluating visuals:

```text
[ ] Docs/CONTEXT_MAP.md route identified
[ ] authoritative Design Bible section read
[ ] relevant ADRs read
[ ] Production docs read
[ ] World.json ownership checked for map gameplay
[ ] correct source/generator is being edited
[ ] generated output is not being hand-patched
[ ] affected region/IDs listed
[ ] rollback point exists
[ ] Epic engine checkout remains unmodified unless a separately approved engine-fork ADR exists
[ ] world-authoring/editor automation is project-owned under VeyraWorldTools
```

If an art change requires changing `World.json` gameplay geometry, classify it as a gameplay-map change.

## 4. Gate 2 — deterministic regeneration

Generated content must be reproducible.

Record where applicable:
- generator/tool version;
- explicit seed;
- source revision;
- input asset IDs;
- `World.json` revision/hash;
- parameter/style profile;
- output region/family;
- regeneration command/tool.

For a new or materially changed generator, validate at least one clean regeneration from source.

"Claude clicked around until it looked right" is not a reproducible production process.

## 5. Gate 3 — repository checks

Documentation/code/world work should run the repository checks relevant to the change.

At minimum for documentation changes:
- `python3 scripts/check_doc_context.py --write`
- `python3 scripts/check_doc_context.py --check`

For tuning/layout changes:
- `scripts/check_tuning.py` through the repository's normal test/check path.

For module changes:
- module-layer checks.

For Unreal changes:
- the appropriate `Game/Scripts/Build.ps1`, `Test.ps1`, `Package.ps1`, and smoke paths required by the owning ADR.

If an agent cannot execute the local Unreal checks in its environment, it must say so and leave exact commands/evidence required for the Windows build machine.

## 6. Gate 4 — source/import integrity

For mesh content:

```text
[ ] scale is correct
[ ] orientation is correct
[ ] pivot is correct
[ ] bounds are plausible
[ ] normals/tangents are valid
[ ] duplicate/internal geometry is intentional
[ ] material slots are intentional
[ ] UV strategy matches the asset family
[ ] collision strategy is explicit
[ ] source metadata links back to generator/source
[ ] reimport does not destroy placement/material settings unexpectedly
```

For modular kits also verify:
- snap alignment;
- corners/end caps;
- repeated seams;
- rotation variants.

## 7. Gate 5 — protected gameplay geometry

An art/PCG change must prove it did not accidentally change:

- lane polylines/widths;
- river macro geometry/crossings;
- Prime Well/base/fountain positions;
- Spire/inhibitor/base-tower positions;
- wildlife camps;
- Flux Well sites;
- Dense Fog placement;
- Fluxborn spawn/pathing corridors;
- route connectivity;
- intended traversable/blocking geometry.

The best check compares the authoritative inputs before/after.

Generated art should consume `World.json`; it should not rewrite it as a side effect.

Any intentional gameplay-layout difference must be separately called out.

## 8. Gate 6 — collision

Check:
- no invisible blocker in intended walkable space;
- no visually solid wall that is unexpectedly walkable when gameplay says blocked;
- no decorative micro-collision snagging;
- no grass/small clutter collision unless intentional;
- large forms use deliberate collision rather than render-mesh complexity;
- destroyed/decorative state does not accidentally change pathing;
- camera/occluder behavior works around tall geometry.

Generated dressing should default to **non-gameplay collision** unless explicitly permitted.

## 9. Gate 7 — navigation/pathing

Validate:
- Vanguard routes;
- Fluxborn routes;
- wildlife camp access/leash;
- terrain-bound runtime actors spawn/stand on the real surface rather than at a Z=0 assumption;
- lane, objective, fountain and camp pads do not float above or sink into the generated Landscape;
- objective access;
- inner jungle;
- outer jungle;
- river crossings;
- base entrances;
- fountain/start areas.

Detect:
- disconnected navigation;
- unwalkable slope spikes or accidental terrain steps;
- spawn points resolving beneath/above the intended playable surface;
- blocked routes;
- accidental shortcuts;
- too-narrow corridors;
- structure placement clipping nav;
- generated geometry intruding into camp/Well spaces.

Travel-time and width tolerances are gameplay tuning/configuration, not magic validation constants.

## 10. Gate 8 — functional symmetry/fairness

The Battleground Bible requires competitive fairness without literal visual mirroring.

Measure representative equivalent routes, such as:
- fountain to lane;
- base to inner jungle;
- base to outer jungle;
- lane to river;
- lane to Flux Well;
- equivalent objective approaches.

Report both sides and the delta.

Acceptable deltas remain reviewed data/playtest constraints.

Visual asymmetry is welcome when functional constraints remain fair.

## 11. Gate 9 — gameplay-camera readability

Important world content is judged from the actual Veyra gameplay camera, not a ground-level cinematic camera.

Validate:
- Vanguard silhouette dominance;
- ability telegraph visibility;
- structure readability;
- lane readability;
- jungle entrance readability;
- objective readability;
- water boundary readability;
- Dense Fog distinction;
- foliage occlusion;
- environmental motion/noise;
- team/Flux visual hierarchy.

A hero asset may be gorgeous and still fail if it makes a fight unreadable.

## 12. Representative combat conditions

Readability validation should eventually include:
- small Vanguard;
- large Vanguard;
- Fluxborn wave;
- structures;
- wildlife/objective context;
- representative projectiles;
- ground AoE;
- shield/effect clutter;
- overlapping allied/enemy effects.

The environment should be tested under stress, not only when empty.

## 13. Standard screenshot suite

Maintain stable named camera anchors.

Minimum coverage:
- top lane both directions;
- mid lane both directions;
- bot lane both directions;
- representative inner-jungle entrances;
- outer-jungle routes;
- North Flux Well;
- South Flux Well;
- river crossings;
- both base approaches;
- both Prime Wells;
- Dense Fog area;
- dense foliage benchmark;
- heavy-combat benchmark.

Capture modes:
- beauty;
- gameplay;
- gameplay + representative actors/VFX;
- collision;
- navigation;
- PCG/debug;
- readability/value;
- performance/debug.

### 13.1 Existing screenshot path

ADR-006 already gives `Game/Scripts/Smoke.ps1 -Screenshot` a packaged-client screenshot role for the current grey-box presentation.

World production should extend that philosophy rather than creating an unrelated manual screenshot process.

Future standard-camera capture tooling should be callable reproducibly from script/commandlet/test infrastructure where practical.

## 14. Visual regression

Comparable screenshots require:
- same camera transform;
- same world/layout revision;
- same lighting profile;
- same graphics profile;
- same representative actor placements where practical.

Animated foliage/particles make pixel-perfect equality inappropriate. Automated image difference is a signal, not the final judgment.

Classify changes:
- expected;
- beneficial;
- accidental;
- uncertain/requires review.

## 15. Art-direction consistency

Review changed content beside approved Crucible content.

Check:
- silhouette;
- proportion;
- edge language;
- surface-detail frequency;
- material roughness/specular;
- palette/value hierarchy;
- foliage language;
- architecture motifs;
- Flux form language;
- scale.

The test is:

> **Does this belong in the same battleground and same game?**

A technically excellent asset that looks imported from another art direction fails.

## 16. Materials/shaders

Validate:
- correct Veyra master family;
- no unnecessary bespoke graph;
- documented mask packing;
- controlled material slots;
- justified translucency;
- shader complexity;
- distance behavior;
- emissive/bloom;
- quality switches.

Global material changes require Level D regression.

## 17. Nanite

For a Nanite candidate:
- inspect gameplay-distance result;
- inspect fallback behavior;
- validate material compatibility;
- validate shadows;
- validate collision separately;
- inspect memory/streaming;
- inspect instancing behavior;
- check for distance artifacts.

"Nanite compatible" is not established merely by importing a high-poly mesh successfully.

## 18. Foliage

Test foliage:
- still;
- moving camera;
- combat;
- wind;
- distance;
- multiple quality levels.

Check:
- canopy does not hide fights;
- motion does not compete with telegraphs;
- routes remain legible;
- shadows are acceptable;
- density does not create false blockers;
- low settings preserve required information.

Experimental UE 5.8 foliage features require explicit approval before becoming a shipping dependency.

## 19. Water

Validate separately:

### Gameplay contract
- crossings unchanged;
- traversal/collision correct;
- gameplay systems do not depend on visual plugin classes;
- surface type/interaction metadata correct.

### Visual result
- river follows the authored spline/path rather than a straight diagonal placeholder;
- bends, width changes and banks read as intentionally landscaped from gameplay and beauty cameras;
- the water surface and carved river basin stay spatially aligned;
- flow direction looks coherent;
- bank blending works;
- crossings remain visible;
- characters/telegraphs read over water;
- ripples/splashes trigger correctly;
- Flux-water visuals remain readable;
- reflection/refraction/scalability behave correctly.

Under ADR-040, Epic Water is the approved first production presentation implementation. Validate it explicitly as a replaceable dependency: packaging, performance, scalability and gameplay-module decoupling must all pass.

## 20. Niagara/dynamic VFX

Validate:
- effect event/input;
- presentation-only ownership;
- bounded spawn count;
- culling;
- translucency/overdraw;
- lights;
- gameplay-camera readability;
- stacked team-fight readability;
- quality/scalability.

Persistent effects should run long enough to catch:
- unbounded accumulation;
- component leaks;
- repeated spawn loops;
- memory growth.

## 21. Flux-driven world state

When the environment reacts to Team Flux:

```text
[ ] state source is authoritative
[ ] presentation only observes
[ ] thresholds come from gameplay/tuning owner
[ ] collision/navigation unchanged unless gameplay explicitly owns a change
[ ] permanent vs temporary Flux is not accidentally conflated
[ ] transition remains readable in combat
[ ] low settings preserve required information
```

Capture the same cameras before/after each meaningful state.

## 22. Lighting/atmosphere

Global lighting changes require full-map review.

Validate:
- exposure;
- champion separation;
- objective separation;
- Dense Fog distinction;
- water readability;
- VFX visibility;
- shadow cost;
- bloom;
- bright/dark surface extremes.

A single prop may not justify a global exposure fix.

## 23. Performance

Performance targets must be explicit, versioned, and configurable.

Track where practical:
- CPU/GPU frame time;
- draw calls;
- shader/material cost;
- translucency/overdraw;
- shadow cost;
- Niagara cost;
- texture/streaming memory;
- mesh memory;
- foliage cost;
- water cost;
- PCG generation cost;
- navigation generation/runtime cost.

This document does not invent final hardware targets or frame budgets.

Once approved, store them in validation profiles/tools rather than prose-only guesses.

### 23.1 Stress cases

Maintain representative stress conditions:
- empty lane;
- dense jungle;
- river/objective;
- base/Prime Well;
- 5v5 fight with Fluxborn and effects;
- worst foliage view;
- worst translucent/VFX overlap.

## 24. Scalability

Validate intentional scaling of:
- foliage density;
- shadows;
- VFX density;
- water;
- reflections;
- decals;
- post process;
- volumetrics;
- environmental animation.

A low setting may reduce beauty; it may not remove gameplay-critical boundaries/information.

## 25. Cook/package/runtime

Editor success is not enough.

Relevant changes should verify:
- assets cook;
- no missing references;
- no editor-only dependency leaks;
- materials/shaders compile;
- packaged battleground loads;
- effects exist in package;
- dedicated server does not require visual systems to adjudicate gameplay.

The existing repository build/package/smoke scripts are the preferred path.

## 26. Provenance/license

Before acceptance:

```text
[ ] external source recorded
[ ] commercial rights compatible
[ ] modification rights known
[ ] attribution known
[ ] AI-use restrictions recorded
[ ] agent inspection/transformation permitted
[ ] NoAI content has not been used as prohibited model input
```

Veyra-authored assets record generator/source lineage instead.

## 27. Automation targets

The production test suite should grow toward:

### Source tests
- naming;
- metadata;
- scale;
- transforms;
- UV/material policy;
- provenance.

### Generator tests
- deterministic seed;
- invalid input;
- bounds;
- manifest/version output.

### Layout tests
- `World.json` source validity;
- gameplay anchors;
- mirror/fairness relationships;
- no art-only drift.

### Navigation tests
- route availability;
- route distance;
- Fluxborn paths;
- objective/camp access.

### Content tests
- material family;
- collision policy;
- Nanite policy;
- folder/layer ownership.

### Runtime smoke
- load map;
- spawn representative actors;
- trigger VFX;
- trigger Flux visual states;
- traverse key routes.

### Visual
- named screenshots;
- debug views;
- regression review.

### Package
- cook;
- package;
- smoke;
- server/headless compatibility.

## 28. Change-risk matrix

| Change | Minimum level |
|---|---|
| New rock variation | A + B |
| Rock generator change | A + B + affected C regions |
| New foliage family | A + B + C |
| PCG density change | C |
| Riverbank generation change | C |
| Water renderer change | D |
| Global material change | D |
| Global lighting change | D |
| Flux world-state presentation | D |
| Lane/jungle layout change | D + gameplay review |
| Structure/objective relocation | D + gameplay review |
| World generation framework change | D |

This is a floor, not a ceiling.

## 29. Required completion report

A substantial visual/world PR should report:

```text
Scope:
Authoritative owners:
World.json changed?:
Source files/generators:
Generated outputs:
Seeds/profiles:
Protected gameplay geometry changed?:
Collision/navigation checks:
Screenshot cameras reviewed:
Performance/scalability checks:
Cook/package/smoke checks:
Provenance changes:
Known risks/open decisions:
```

If a check cannot be run in the agent environment, say exactly which machine/tool must run it next.

## 30. Crucible world-authoring acceptance additions

For the ADR-040 production-world pass, Level D additionally requires:
- `Game/Plugins/VeyraWorldTools/` or its documented successor can regenerate the affected terrain/river/PCG scope from repo-owned source;
- no ordinary Veyra change modifies Epic's engine source tree;
- the final map contains meaningful, readable real elevation rather than a flat floor with cosmetic dressing;
- all terrain-bound runtime spawn categories audited by the change resolve onto the actual playable surface;
- the river is intentionally curved/shaped and visually integrated with the terrain;
- representative Unreal captures were visually reviewed and defects were corrected through source/tooling, not by undocumented hand-patching of the generated map.

## 31. Acceptance rule

A result can be beautiful and still fail.

A result can be fast and still fail.

A result can be deterministic and still fail.

World content is accepted only when **gameplay integrity, Veyra visual identity, competitive readability, reproducibility, performance, and runtime correctness** all hold together.
