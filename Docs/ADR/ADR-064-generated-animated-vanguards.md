# ADR-064: Generated animated Vanguards: body archetypes, Blender rigs and cue-driven animation

**Status:** Proposed. The author's standing instruction (2026-09-28) is to keep working unreviewed toward a viable game, choosing a provisional answer where canon is silent. §9 lists every such answer for the author to overturn. This ADR becomes Accepted when the author merges the pull request that adds it.
**Date:** 2026-10-03
**Related:**
- The author's decision after the first playtest (2026-10-03): after feel fixes and grey-box readability, real animated characters, Blender-authored per the production pipeline, in their own milestone with an ADR. The same day: Veyra is built in Unreal Engine 5.8 and should use it as fully as it can visually.
- [Initial Roster Character Bible](../Design/Veyra_Initial_Roster_Character_Bible_v0.6.md): each Vanguard's `**Visual language:**` paragraph, and art precedence (approved art is canon).
- [Art Direction](../Design/Art_Direction_v0.1.md): silhouette first; the idle, move, cast and ultimate sheet slots are captured in engine.
- [Asset & VFX Pipeline](../Production/VEYRA_ASSET_AND_VFX_PIPELINE.md):
  - §2: Blender may author rigs and animation, and Unreal runs and integrates them;
  - §4: seeded, deterministic generators;
  - §6–§7: naming, export and import;
  - §25: the Blender pin and export format per category are open, and this record decides them for characters.
- [ADR-006](ADR-006-unreal-project-scaffold.md) §6: asset references in Data Assets by stable ID; §9: binary assets are locked before they are replaced.
- [ADR-063](ADR-063-combat-readability-cues-effects-sound-and-the-fountain-shop.md): the combat cues, body feedback, outlines, effects and sound this record builds on.

## Context
- **Cylinders:** Vanguards are drawn as cylinders. The first playtest asked to see what is happening and when hits land. M52 added flashes, swings, effects and sound to the grey-box; real bodies and animation are the next step the author chose.
- **No 3D spec:** no Vanguard has a 3D model, rig or animation, and no document specifies one. Each Vanguard's look is its Visual language paragraph and, where it exists, its approved art.
- **One roster, many body plans:**
  - mostly humanoid riders, casters and fighters, some small, heavy or digitigrade;
  - colossi: Silt, Relay, Varkesh and Cairn;
  - beasts: Korruk, six-legged, and Moro, four-legged;
  - constructs whose parts float about a core: Torr, Oriel and Aurelisse.
- **The toolchain exists:** generators in background Blender write FBX with hashed manifests, Unreal Python imports them with validation, and kit art already stands in for Fluxborn and structure capsules. No skeletal mesh or animation code exists yet.

## Decision

### 1. Body archetypes
- **Four archetypes:** Humanoid, Colossus, Beast and Construct. Each Vanguard has one (the table below).
- **What an archetype defines:** a skeleton (bone names and hierarchy), a parametric body and a procedural animation set, in its generator.
- **Each Vanguard's entry** in `Game/ArtSource/Vanguards/VanguardKit.json` gives its proportions, build, hair, features, props, colours and seed.
  - These are art parameters, read from its Visual language paragraph. They are never gameplay tuning.
  - The body is fitted to its Vanguard's capsule from `Vanguards.json`, which stays its only collision and movement.
- **Until a Vanguard's archetype is generated,** it keeps its grey-box body. Humanoids come first, since they are most of the roster.

| Archetype | Vanguards |
|---|---|
| Humanoid | Raska, Kade, Patch, Tavi, Vera, Marek, Neris, Qazharr, Angeru, Sylra, Mavra, Bryn, Mimzi, Celandrine, Gorraveth, Eudora |
| Colossus | Silt, Relay, Varkesh, Cairn |
| Beast | Korruk, Moro |
| Construct | Torr, Oriel, Aurelisse |

### 2. Animation
- **Each archetype's set:** Idle and Run (cycles), Attack Windup, Attack Strike (a swing for melee Vanguards, a release for ranged ones), Cast, Hit, Death and Recall (a cycle).
- **Generated per Vanguard** by the archetype's code, on the Vanguard's own skeleton: each skeleton carries its proportions, and its animations fit them.
- **No root motion:** the server moves the capsule. The Run cycle records how far a stride carries the body, so it plays at the speed the capsule moves.

### 3. Runtime
- **The art set:** `UVeyraVanguardArtSet` (`DA_VanguardArt`) holds, by Vanguard ID, the skeletal mesh, its animations by name, its Run stride and its upper-body bone. The importer writes it, and the grey-box settings name it.
- **The animation instance:** `UVeyraVanguardAnimInstance` (C++, in VeyraUI) has no Animation Blueprint. Its proxy samples the sequences and blends them itself.
  - A pure state, `FVeyraVanguardAnimState`, chooses what plays from the drawn unit's ground speed, life and recall, and from the combat cues (ADR-063 §1).
  - Idle and Run blend by speed, and Run plays at the rate its stride matches.
  - Windup, Strike, Cast and Hit play over locomotion and fade in and out. While the body runs they play on the upper body only.
  - An attack's windup plays at the rate that ends it when the replicated windup ends, so a fast attacker swings fast.
  - Death plays and holds until the unit lives again. Recall loops while the unit recalls.
  - It holds no gameplay logic, and nothing reads it back.
- **The swap:** the grey-box presentation dresses a Vanguard in its skeletal mesh through the art set, as kit art dresses Fluxborn and structures. The capsule, its collision and its movement do not change.
  - The cylinder becomes a thin disc under the body's feet, in the colour it showed: the viewer's own, ally or enemy, tinted while stunned, slowed or Camouflaged. A body in its Vanguard's own colours still shows its side and its statuses.
- **M52 carries over:** the hit flash overlay and the hover outline apply to the skeletal mesh. Where a body is animated, the lean, snap, recoil and collapse give way to its animation.
- **Data:** the blend times and the speed at which Run takes over are validated grey-box presentation settings.

### 4. Formats and tools
- **Blender:** 5.2 LTS is pinned for the character generators. They run in an isolated background process.
- **Export:** each Vanguard is one binary FBX in centimetres, with its armature, its skinned mesh and every animation as a take. The armature object is named `Armature`, so the import does not add a root bone above the skeleton's own.
- **Import:** Unreal Python imports, under `/Game/Veyra/Vanguards/<Id>/`:
  - `SK_<Id>`, the skeletal mesh;
  - `SK_<Id>_Skeleton`, its skeleton;
  - `AS_<Id>_Armature_<Clip>`, its animation sequences, named by the import from each take.
- **Material:** every body wears one generated material, `M_VeyraVanguardBody`: its colour is the vertex colour, and the vertex alpha marks what glows.
- **Validation:**
  - The generator checks the triangle budget and that the body stands on the ground.
  - The import checks source hashes, each body's height against the generator's, that every animation imported, and that it is on the body's skeleton.
  - A preview renders each body in its key poses, front and side, for review.

### 5. Fidelity and look
- **The job:** the first pass is generated and stylised: broad, chunky and bright, so it reads from the match camera. Its job is readability: silhouette, motion and timing.
- **Later art:** authored models can replace a body on its skeleton, keeping the animation runtime.
- **The sheet captures** of idle, move, cast and ultimate wait until the author approves the look.
- **Motion blur off:** the match camera pans fast, and per-object motion blur smears the whole field on every pan. The project's renderer default turns it off.

### 6. Ownership
- **Gameplay** owns everything a body does in a match.
- **VeyraUI** owns the art set, the animation instance and the swap.
- **The generators** own the assets: change the kit or the generator and regenerate, rather than editing an imported asset.

### 7. Not in scope
Final authored art, faces, cloth simulation, skins, LODs, and Fluxborn rigs (later, on the same pipeline).

### 8. Consequences
- Every Vanguard with art is a skeletal mesh evaluated each frame on clients. With ten Vanguards in a match, this is small next to the waves. The milestone's gate measures it.
- Servers draw nothing, so no server loads these assets.
- A change to a generator rebuilds that archetype's assets, which needs the Git LFS locks on them.

### 9. Provisional answers where canon is open
1. The four archetypes, and each Vanguard's.
2. Each Vanguard's proportions, props and colours as read from its Visual language paragraph.
3. The animation set and its timings.
4. Blender 5.2 LTS, and FBX for characters.
5. Generated stylised bodies as the first pass.
6. Motion blur off by default.
