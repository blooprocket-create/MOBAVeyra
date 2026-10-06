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
- **Archetypes:** Humanoid, Colossus, Beast and Construct; each Vanguard's own body has one (the table below). A fifth, **Rider**, is never a Vanguard's own body. It is worn only while a ride lasts (below).
- **A body shows the Vanguard's state in a match, which its kit decides.** Its Visual language and art say how it looks. Anything summoned, ridden or toggled is a state the presentation switches to, never part of the default body. Raska fights on foot: Hound arrives only with Kickstart and NO BRAKES, and she cannot basic-attack while mounted (author, 2026-10-05).
- **Status bodies:** a Vanguard may wear another body in its own's place while it holds a status. Its rides already apply one for their length (Raska's `raska_ride_body`; Neris's wave rides `neris_wave_body`).
  - Its kit entry lists them (`statusBodies`: status, name, the entries that differ), and each is generated and imported beside its own body.
  - The art set holds them by status ID. The presentation dresses the body for the statuses the unit's replicated ledger holds, so the swap follows gameplay and decides nothing.
  - A unit can hold several statuses that have bodies; the one with the highest `priority` in the kit is worn, and ties go by status ID. A brief burst outranks a state held all the while in some ground: Moro wears Wildstorm's storm (`moro_wildstorm_stride`, priority 1) over Wild Dominion's brighter Wildlight (`moro_wild_dominion_power`, refreshed while he is in the jungle).
- **Companions:** a companion that is half of its Vanguard's pair (Nix, to Marek) has a body of its own, generated from the kit's `companions` section and fitted to its definition's capsule in `Abilities.json`. The art set holds it by companion ID (`CompanionArt`), and the presentation dresses it as it dresses a Vanguard, status bodies included (Nix's horned true form while `nix_true_form_size` lasts). A companion's statuses sit beside its own ability system.
- **Bodies of particles:** a body made of something no mesh can show is drawn by an effect instead. Nix is smoke: only its bone mask is mesh, and its body is stylized smoke that Niagara pours off its skeleton's bones as it moves.
  - Each puff is a solid, sunlit blob with a ragged edge. It glows violet while young and erodes through holes as it ages.
  - The skeleton still animates, so the smoke runs, strikes and dies with it.
  - The body's art names the system, the bones it pours from, its colour and its scale. The scale is the body's own, so the true form pours larger smoke.
  - The system is generated like the M52 effects (`Effects.json`), with its sizes written in terms of a user scale. Its puff material (`M_VeyraSmoke`) glows by the inverse of the scene's exposure, so it reads the same under the Crucible's physical sun.
  - The effect decides nothing.
- **Rider:** a humanoid seated on a mount that is half of its silhouette, such as Raska on Hound.
  - The rider's chain and the mount's chain both hang from the root. A mount on wheels turns them as it rides, so the Run stride is one turn of a wheel.
  - Wherever the mount moves the whole silhouette (a wheelie, a lunge, a jolt), the rider is posed to follow it. Being separate chains, a crash can throw the rider clear of a mount falling on its side.
  - The mount fills the unit's footprint, so a rider keeps a figure's own shoulders rather than the capsule's.
- **What an archetype defines:** a skeleton (bone names and hierarchy), a parametric body and a procedural animation set, in its generator.
- **Each Vanguard's entry** in `Game/ArtSource/Vanguards/VanguardKit.json` gives its proportions, build, hair, features, props, colours and seed.
  - These are art parameters, read from its Visual language paragraph. They are never gameplay tuning.
  - The body is fitted to its Vanguard's capsule from `Vanguards.json`, which stays its only collision and movement.
- **Until a Vanguard's archetype is generated,** it keeps its grey-box body. Humanoids come first, since they are most of the roster.

| Archetype | Vanguards |
|---|---|
| Humanoid | Raska, Kade, Patch, Tavi, Vera, Marek, Neris, Qazharr, Angeru, Sylra, Mavra, Bryn, Mimzi, Celandrine, Gorraveth, Eudora |
| Rider (a status body) | Raska on Hound, while `raska_ride_body` lasts |
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
  - Each body records a hash of what it is made from and by: its data, the generator's code, and the Blender that built it. The import, and CI on any change to them, refuse a body whose inputs have changed since it was built, or whose name the kit has changed, so a partial build cannot keep a stale one.
  - Each body also records a hash of what it is: its vertices, weights, colours, skeleton and every animation key. A full rebuild redoes every body but rewrites only those whose content changed, so it costs little when little changed and the repository's large files are not rewritten for nothing. What changed is counted since the last import, not the last generator run: a body generated on its own (a preview, a failed import) waits for the next import, and so do a dropped body's imported assets.
  - Every FBX begins with a rest-pose take, and every bone has skin, so a skeleton binds at rest.
  - A preview renders each body in its key poses, front, side and from the gameplay camera's pitch, for review.
  - `CaptureVanguards.ps1` stands imported bodies in their key poses in the lit Crucible. It captures them from the gameplay camera, at its own distance and field of view and through a narrow lens.
- **Colour:** vertex colours are exported linear, as Unreal's materials read them. The importer rebuilds `M_VeyraVanguardBody` on every import and checks each connection, because a connection to a missing output fails quietly and leaves the body black.
- **One Vanguard at a time (author, 2026-10-05):** each Vanguard's silhouette is refined and validated on its own, against its whole kit, its Visual language paragraph and its approved art. Its preview is reviewed, then it is imported and captured at the gameplay camera before the next begins.
- **A regenerated body imports fresh:** its previous assets are removed first, since a reimport onto an existing skeleton keeps the old reference pose.

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
1. The archetypes, each Vanguard's, and the status bodies a ride is worn as.
2. Each Vanguard's proportions, props and colours as read from its Visual language paragraph.
3. The animation set and its timings.
4. Blender 5.2 LTS, and FBX for characters.
5. Generated stylised bodies as the first pass.
6. Motion blur off by default.
