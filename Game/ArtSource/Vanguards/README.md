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
| Humanoid | Yes | Raska, Kade, Patch, Tavi, Vera, Marek, Neris, Qazharr, Angeru, Sylra, Mavra, Bryn, Mimzi, Celandrine, Gorraveth, Eudora |
| Colossus | Not yet | Silt, Relay, Varkesh, Cairn |
| Beast | Not yet | Korruk, Moro |
| Construct | Not yet | Torr, Oriel, Aurelisse |

A Vanguard whose archetype is not generated keeps its grey-box body.

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
- `Game/Scripts/GenerateVanguardBodies.py`: the archetype's skeleton, parametric
  body, prop library and procedural animations. It runs in background Blender and
  writes the FBX and the manifest. `-- --preview` also renders each body in its
  key poses to `Game/Saved/VanguardKit/Preview/` (front three-quarter and side).
- `FBX/`: one binary FBX per Vanguard, in centimetres, with its armature (named
  `Armature`), its skinned mesh and every animation as a take.
- `manifest.json`: the generated bodies' hashes, triangles, heights, Run strides
  and animation lengths. It reports art output and does not own gameplay values.
- `Game/Scripts/ImportVanguardBodies.py`: the validated editor-only importer. It
  builds `M_VeyraVanguardBody` and imports `SK_<Id>`, `SK_<Id>_Skeleton` and
  `AS_<Id>_Armature_<Clip>`. Its report is `Game/Saved/VanguardKit/unreal-validation.json`.

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
- **Checks:** the triangle budget and that the body stands on the ground, in the
  generator; source hashes, height, every animation and its skeleton, in the import.
