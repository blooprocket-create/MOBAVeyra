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
| Rider (status body) | Yes | Raska on Hound, worn only while her ride's `raska_ride_body` status lasts |
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
already written: what the generator has left to import since the last import, or the
Vanguards `-Vanguards` names. `-Blender '<path to blender.exe>'` names Blender when it is not
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
  three tail bones; three cape bones from the upper back. Every humanoid has every bone, and a bone a body does not use
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
  animation and its skeleton, in the import. Each body records the hash of what it is made from (its kit entry, its
  archetype's settings, the frame rate, the generator version, its capsule: `VanguardBodies/inputs.py`), and of what
  made it (the generator's code and the Blender version). The import, and CI (`.github/workflows/vanguard-bodies.yml`),
  refuse any body in the manifest whose inputs have changed since, so a partial build cannot keep a stale one. The
  kit pins the Blender release (`blender`): the generator will not run under another, and a manifest another built
  is stale whole. CI also checks every FBX is the file the manifest recorded, by its Git LFS pointer.
- **Rebuild everything, rewrite what changed:** each body also records a hash of its content (vertices, weights,
  colours, skeleton, every key). A rebuild that gives a body the same content keeps its FBX and imported assets, and
  `BuildVanguardBodies.ps1` imports only the Vanguards whose bodies changed, still rewriting the art set. After any
  change to the generator's code, run it without `-Vanguards`. A body the kit removes or renames is dropped whole:
  its FBX by the generator and its imported folder by the build, after the same lock check, so nothing unreferenced
  stays tracked or cooked.
- **Binding at rest:** every bone has skin (an unused one gets a speck), and every FBX begins with a one-frame rest
  take (`_Bind`, removed on import), so a skeleton always binds at rest and the import measures the rest pose.

## The other archetypes

- **Companions:** the kit's `companions` section gives a companion that is half of a pair its own body (Nix: a
  quadruped in a bone-white skull mask, and its horned true form; Neris's Waterling: a little living wave, `waterBody`,
  low on the ground with a foam crest leaning over its lit eyes), fitted to its capsule in `Abilities.json` and filed
  in the art set's `CompanionArt` by companion ID.
- **Bodies of particles:** a body entry's `effect` (system, bones, colour) is drawn by Niagara where no mesh can show
  it. The `smokeBody` feature leaves only the mask as mesh, and `NS_VeyraSmokeBody` (`../Presentation/Effects.json`)
  pours stylized smoke off the listed bones. The effect's scale is the body's `bodyScale`. Rebuild the system with
  `BuildEffects.ps1` and its puff material with `BuildPresentationMaterials.ps1`; `CaptureVanguards.ps1` pours it in
  the review shots.
- **Status bodies:** a body a Vanguard wears in its own's place while it holds a status, listed in its kit entry's
  `statusBodies` (status, name, and the entries that differ from its own) and imported beside its own body as
  `SK_<Id>_<Name>`. A body shows the state the kit puts the Vanguard in: Raska fights on foot, and Hound arrives only
  with her rides, which hold `raska_ride_body` for their length. Neris always rides her wave (`waveBase`: a swell with a
  crescent crest on the cape bones). Breaking Wave and TIDEBREAKER (`neris_wave_body`, `neris_tidebreaker_body`) lift
  her on a deeper, breaking one, and she stays her own size. When a unit holds several statuses with bodies, the
  status body with the highest `priority` (inside its `body`) is worn, then the first by status ID.
- **Stance bodies:** an entry of `statusBodies` may name a `stance` ability in place of a `status`. Its body is worn
  while that stance's set is in the Vanguard's slots, which the loadout replicates to every machine (ADR-031 §3). Each
  entry names exactly one. Neris's Storm (`neris_change_the_weather`) darkens her sea to slate, whitens its foam and
  throws `NS_VeyraSpray` off the crest. Her rides keep `priority` 1, so she rides the wave in either weather.
- **Moro** (`barkBody`): a dark fur trunk, with grey timber plates down the flanks of the haunch and shoulder, a
  shaggy cream ruff, and branching antlers with leaves. Wildlight runs as veins down each side of the back, where the
  camera sees it, as leaf panels on the flanks, as eyes at the shoulders and haunches, and as veins down the legs.
  In his own ground (`moro_wild_dominion_power`) it surges (`wildlightSurge`): wider, paler, up the neck, and into
  the antler leaves. Wildstorm (`moro_wildstorm_stride`, priority 1) adds the surge plus `NS_VeyraWildstorm` gusting
  off his body and legs.
- **Rider:** a humanoid seated on a mount that is half its silhouette, worn as a status body. The mount's chain and the
  rider's both hang from the root. Raska rides Hound, a heavy brawler's machine on fat knobbled tyres that turn once a
  Run cycle. Wherever Hound moves the silhouette (an idle shudder, a lunge, a wheelie to cast, a jolt when hit), she is
  posed to follow it. In death Hound goes down on its side and she is thrown clear.
- **Humanoid stances and dress:** an `aim` stance holds a long weapon two-handed at the shoulder, the arms keeping
  their hold while the upper body carries it (Kade, mid-sight). A `punch` strike drives the named hand's fist straight
  out from a guard with a lunge (Raska's bracer). A long cloak rides three cape bones that stream back as the body runs.
  A `shoulderCarry` stance rests a heavy blade over the right shoulder at rest, running, flinching and recalling, its
  flat turned up to the camera; an attack or a cast takes it off. Qazharr carries his boarding blade (a broad cleaver
  head, a pale edge, an anchor fluke on its back) so, in a long coat open over a bare chest (`openJacket` with
  `jacketUnder: "skin"`), with `tattoos` (coils on the chest, bands down the forearms), a `ropeCoil` slung over the
  shoulder, an `anchorBelt` and red sash, and long dark curls (`curlyLong`). NO QUARTER (`qazharr_no_quarter_frenzy`)
  takes the blade off his shoulder into a restless, forward-leaning ready stance. Angeru is black but for a torn red
  `waistSash` and a narrow cloak-tail (`longCloak` with `cloakWidth`) that streams behind him: his hood down
  (`hood`), Veil cloth bound round his forearms and shins (`wrapBindings`), Blade House formal panels and a standing
  collar (`formalVest`), every house mark cut out (`cutInsignia`), a `crossedToken` at his hip, his long sword sheathed
  across his back with its hilt over his left shoulder, a dagger in his right hand and throwing `needles` in his left.
  He shows Veil Stance, his own. In Blade Stance (`angeru_forsake_the_schools`) he holds the sword drawn in his right
  hand (`longSword`: a cord grip, a round guard, a long blade in a slight upward curve with a pale edge), and the
  scabbard on his back is empty (`swordBack` with `empty`). Sylra holds a large `pilotLantern` out at arm's length (`lanternOut`: the arm raised forward
  and out, the hand turned back so the lantern hangs plumb) under a `deepHood` that peaks behind and drapes her
  shoulders, her face in shadow but for a band of dark markings. Her grey-blue storm cloth is cut into ribbons at every
  hem (`ribbonHems`); her sleeves and gloves cover her arms (`sleeves`, `gloves`); her chained coat carries working
  `pilotGear` (bells, sea-glass, keys, weights, a ship's-wheel charm); and `NS_VeyraMist` gathers round her hem.
  Mavra works in a stained `hazardCoat` open over its red `lining`, its ragged hem in red-and-white hazard stripes,
  with `canisterBelts` of labelled, valved canisters crossed over her body, a red `headband` and `workGoggles` pushed
  up on her forehead. Her `dispenserRig` is a banded steel tank on her back with a glowing reagent window, a gauge-glass
  band and a hazard placard on its pump, its armoured hoses over her shoulder to the nozzle in her hand, one lit.
  Bryn is a compact veteran gunner in a worn dark-teal naval coat (`coatSkirt`) with salt-and-pepper hair, a red
  `waistSash`, a `gunnerKit` (ammunition pouches round the belt, a rope coil at the left hip) and `heavyBoots`.
  Mournwake (`harborGun`) is carried on both sides of her (`braced`): its banded brass-and-iron barrel runs forward
  and down under her right arm to a wide muzzle with Flux light standing in the bore, past two blue-lit Flux chambers,
  and its breech and recoil assembly rides up over her left shoulder. Both hands stay on it as she runs, swings and
  casts, and with no sight to look down she keeps her head up and clear of the gun.
  Mimzi reads as a small animal, never a person with fox ears. She is small in frame (`shoulderShare` sizes her
  shoulders by her own height rather than her capsule, and the disc under her shows her footprint). Her face is a
  fox's (`foxFace`): a short pale muzzle with a dark nose, big amber eyes set wide, and flaring cheek tufts. Her
  `fennecEars` are enormous, pale outside and pink within, and each is folded into a cup facing forward and up. They
  stand through a `bigHood` with a rolled brim, brass rivets at the temples and a point falling behind. A `brushTail`
  as long as she is tall to the shoulder sweeps up behind her, paler at the tip. Over her teal coat she wears a brown
  `harness` and a `trinketSatchel` (a buckled satchel on a cross-strap, a cut blue crystal charm and a brass cog, two
  belt pouches). Her red `trailingScarf` is the one hot colour on her. `furCuffs` of her own fur show at her wrists and
  boot tops. The Tinkertwins (`rings`) orbit her hands, each as broad as her head: a brass ring with a gimbal crossed
  inside it and a cut blue crystal at its heart, turning inside a ring of its own light with star-sparks.
  Celandrine is a hare, longer-legged and more athletic than Mimzi, and nothing she carries is magical. Her
  `hareFace` has a rounder muzzle and a pink nose. Her `hareEars` are long and swept back over her shoulders, dark at
  the tips (`earTips`), cupped like Mimzi's (one `cupped_ear` makes both). Her `hareLegs` are powerful, with furred
  haunches over bare legs (`shorts`) and long fur feet (`footShare`, `feet`). She wears an olive courier jacket with
  the hood up (`bigHood`), a step from Mimzi's teal, and her red scarf streams behind her. Cream running wraps bind
  her forearms and shins (`wrapBindings` with `wrap`). Her `courierSatchel` holds letters standing from under its
  flap, with a map tube strapped along its top. A mechanical `springbow` is in each fist: a green-painted steel stock,
  brass limbs swept back across its front with a cord drawn between them, exposed coil springs, a winding drum and
  a bolt laid in, all unlit.
- **Colossus:** the humanoid skeleton without its tail, in colossal proportions: a
  towering, forward-leaning trunk, enormous shoulders, long arms over short legs, the
  head small and low between the shoulders, or none. Each is built in its material:
  sediment sheets over a dark wet core (Silt, who walks on his clawed forelimbs),
  bone-white slabs over dark mechanism (Relay), iron plates whose seams glow (Varkesh),
  or rough riverstone (Cairn, whose hook arm is the larger). A ranged colossus throws
  overhead; a cast is a two-fisted slam. Varkesh towers near twice a person's height (`heightShare` above 1: his
  capsule stays the same): rings of dark plates, a little askew, over a molten core that glows through every seam
  (`seamGap`), the plates of his forearms and shins riding clear, his crown plated over. An orange-white spiral burns
  in his chest, a tattered crimson drape hangs on iron rings, molten shards orbit his open hands, and
  `NS_VeyraEmbers` sheds from them. Tempered Shell (`varkesh_shell_tenacity`, held while its shield holds) cools him:
  black plates, dark closed seams, no embers, the core still lit. Cairn is dark, weathered riverstone with dressed
  bridge `masonry` worked into him: his outsized rusted crescent hangs on heavy chain links from the hook arm, the
  chain wound round his torso, over his shoulder and trailing behind him along the ground (`chainWrap`), moss on his
  upper surfaces, and `NS_VeyraDrip` running off his ledges. Immovable (`cairn_immovable_guard`) heaves further
  layers of riverstone over him (`reinforced`) and plants his feet wider (`stanceSpread`).
- **Beast:** a horizontal skeleton: hips at the back (the pelvis), the spine running
  forward to the chest, neck and head ahead; forelegs on the arm bones, hind legs on
  the leg bones, and a middle pair for six legs (Korruk). It trots (a tripod gait on six
  legs), lunges to bite or arches to fire spines, rears to cast, topples onto its side,
  and lies down to recall.
- **Construct:** the humanoid's upper body over a floating core (the pelvis bone),
  with six orbit bones carrying parts that circle it and a trailing chain beneath: a
  column of plates (Torr), a point of shards (Oriel) or a cyclone (Aurelisse). A halo
  rides over the shoulders or behind the head. It bobs, sweeps its trailing parts
  back as it moves, gathers its parts to strike and scatters them as it falls. Oriel is built of `pane`s, flat
  pieces of any outline, each in gold leading. Her slender bodice is rings of irregular jewel panes, some cut corner
  to corner, burning warm over the heart. Below the waist, ragged shards hang apart, splayed and swept back about a
  narrow point that floats clear of the ground. Her limbs are crossed glass the whole way through, with gold joints
  and long gold fingers. Her head is a pointed gem of panes, open at the face where her core burns in a gold frame,
  under a crest of shards. A ring of gold framework stands behind it, tilted back and throwing rays. Kite panes, each
  cut in four like a harlequin, fan from her shoulders like a window opened out: four on her left, three on her
  right. Shards drift about her, two to each orbit.
