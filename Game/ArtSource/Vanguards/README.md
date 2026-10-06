# Vanguard body kit

Generated, rigged and animated first-pass bodies for the Vanguards
([ADR-064](../../../Docs/ADR/ADR-064-generated-animated-vanguards.md)). This is a
stylised readability pass: silhouette, motion and timing. It is not approved
final character art. The imported assets are under `/Game/Veyra/Vanguards/<Id>`.

## Production owners

- [Initial Roster Character Bible](../../../Docs/Design/Veyra_Initial_Roster_Character_Bible_v0.6.md):
  each Vanguard's Visual language paragraph is the source of its kit entry.
- [Art Direction](../../../Docs/Design/Art_Direction_v0.1.md): silhouette first;
  approved art takes precedence over this pass.
- [Vanguards.json](../../Tuning/Vanguards.json) owns each Vanguard's capsule. The
  body is fitted to it, and the capsule stays the only collision and movement.
- [ADR-063](../../../Docs/ADR/ADR-063-combat-readability-cues-effects-sound-and-the-fountain-shop.md)
  owns the combat cues the animations play from.

## Archetypes

| Archetype | Generated | Vanguards |
|---|---|---|
| Humanoid | Yes | Kade, Patch, Tavi, Vera, Marek, Neris, Qazharr, Angeru, Sylra, Mavra, Bryn, Mimzi, Celandrine, Gorraveth, Eudora |
| Rider | Yes | Raska (on Hound) |
| Colossus | Yes | Silt, Relay, Varkesh, Cairn |
| Beast | Yes | Korruk, Moro |
| Construct | Yes | Torr, Oriel, Aurelisse |

Every Vanguard on the roster has a generated body. A Vanguard missing from the art set keeps its grey-box body.

## Reproduce

Run from the repository root with the project's editor built:

```powershell
./Game/Scripts/BuildVanguardBodies.ps1
```

`-Vanguards raska,kade` builds only those Vanguards. `-ImportOnly` imports the FBX
already written. `-Blender '<path to blender.exe>'` names Blender when it is not
on PATH or in Blender 5.2's default install. Blender 5.2.1 LTS and Unreal 5.8.3
were used for this pass.

- `VanguardKit.json`: each Vanguard's art entry: height share, head share, build,
  hair, features, props, colours and seed; and each archetype's triangle budget
  and animations (seconds, looping). No gameplay tuning.
- `Game/Scripts/GenerateVanguardBodies.py` and the `Game/Scripts/VanguardBodies/`
  package (one module per archetype, and the parts they share): each archetype's
  skeleton, parametric body, details and procedural animations. It runs in
  background Blender and writes the FBX and the manifest. `-- --preview` also renders each body in its
  key poses to `Game/Saved/VanguardKit/Preview/`: front three-quarter, side, and from the gameplay camera's pitch.
  Colours go out linear, as Unreal's materials read vertex colours.
- `FBX/`: one binary FBX per Vanguard, in centimetres, with its armature (named
  `Armature`), its skinned mesh and every animation as a take.
- `manifest.json`: the generated bodies' hashes, triangles, heights, Run strides
  and animation lengths. It reports art output and does not own gameplay values.
- `Game/Scripts/ImportVanguardBodies.py`: the validated editor-only importer. It
  rebuilds `M_VeyraVanguardBody` (checking every connection) and imports `SK_<Id>`, `SK_<Id>_Skeleton` and
  `AS_<Id>_Armature_<Clip>`. Its report is `Game/Saved/VanguardKit/unreal-validation.json`.

## Review a body in the game's light

```powershell
./Game/Scripts/CaptureVanguards.ps1 -Vanguards raska
```

It stands each named body on the Crucible's Bottom lane in its key poses: one row turned toward the camera, one in
profile. It captures them from the gameplay camera's pitch twice: `<Body>_Game.png` at the camera's own distance and
field of view, the size players see, and `<Body>_Close.png` through a narrow lens, to judge the silhouette. The images
go to `Game/Saved/VanguardKit/Review/`. Each Vanguard's silhouette is refined and validated this way, one at a time
(ADR-064 §4).

Rebuild from the kit and the generator rather than editing an imported asset.
Acquire Git LFS locks before replacing tracked `.uasset` files, per ADR-006
section 9. The existing LFS rules cover the FBX files.

## How a body is made

- **Skeleton:** root, pelvis, three spine bones, neck and head; clavicle, upper
  arm, forearm, hand and a prop bone each side; thigh, calf and foot each side;
  three tail bones. Every humanoid has every bone, and a bone a body does not use
  carries no weight.
- **Fitting:** the body's height is the capsule's height times the entry's height
  share. Its shoulders fill most of the capsule's radius, so its footprint reads
  as the unit's. A larger head share gives a smaller figure's shorter legs.
- **Body:** simple closed parts, each weighted rigidly to one bone. Colour is a
  vertex colour, and its alpha marks what glows.
- **Animations:** Idle, Run, Attack Windup, Attack Strike (a swing for melee
  Vanguards, a release for ranged ones), Cast, Hit, Death and Recall, generated as
  rotations on the Vanguard's own skeleton. There is no root motion.
- **Checks:** the triangle budget, that nothing sinks below the ground and that a
  walking body stands on it, in the generator; source hashes, height, every
  animation and its skeleton, in the import.

## The other archetypes

- **Rider:** a humanoid seated on a mount that is half its silhouette. The mount's chain and the rider's both hang
  from the root. Raska rides Hound, a heavy brawler's machine on fat knobbled tyres that turn once a Run cycle.
  Wherever Hound moves the silhouette (an idle shudder, a lunge into her punch, a wheelie to cast, a jolt when hit),
  she is posed to follow it. In death Hound goes down on its side and she is thrown clear.
- **Colossus:** the humanoid skeleton without its tail, in colossal proportions: a
  towering, forward-leaning trunk, enormous shoulders, long arms over short legs, the
  head small and low between the shoulders, or none. Each is built in its material:
  sediment sheets over a dark wet core (Silt, who walks on his clawed forelimbs),
  bone-white slabs over dark mechanism (Relay), iron plates whose seams glow (Varkesh),
  or rough riverstone (Cairn, whose hook arm is the larger). A ranged colossus throws
  overhead; a cast is a two-fisted slam.
- **Beast:** a horizontal skeleton: hips at the back (the pelvis), the spine running
  forward to the chest, neck and head ahead; forelegs on the arm bones, hind legs on
  the leg bones, and a middle pair for six legs (Korruk). It trots (a tripod gait on six
  legs), lunges to bite or arches to fire spines, rears to cast, topples onto its side,
  and lies down to recall.
- **Construct:** the humanoid's upper body over a floating core (the pelvis bone),
  with six orbit bones carrying parts that circle it and a trailing chain beneath: a
  column of plates (Torr), a point of shards (Oriel) or a cyclone (Aurelisse). A halo
  rides over the shoulders or behind the head. It bobs, sweeps its trailing parts
  back as it moves, gathers its parts to strike and scatters them as it falls.
