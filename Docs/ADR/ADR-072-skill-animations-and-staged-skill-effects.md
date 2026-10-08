# ADR-072: Skill animations and staged skill effects

**Status:** Proposed. On 2026-10-08 the author asked for Vanguards' skills to have "animations and effects, not just an indicator", by adding effects to the models, updating their animations, or both. They chose both, as the first part of M60 (character animation), piloted on Cairn, Oriel and Varkesh before the rest of the roster follows. This ADR becomes Accepted when the author merges the pull request that adds it.
**Date:** 2026-10-08
**Related:**
- [ADR-064](ADR-064-generated-animated-vanguards.md): §2, each archetype's eight-clip set, amended here; §3, the art set and the animation runtime; §4, generated bodies and their validation.
- [ADR-063](ADR-063-combat-readability-cues-effects-sound-and-the-fountain-shop.md): §1, cues read from replicated state; §4, generated effect systems.
- [ADR-068](ADR-068-toon-characters-and-graphic-combat-effects.md): §1, graphic effects; §4, the effect families; §5, the frame budget.
- [ADR-071](ADR-071-playtest-readability-strands-kit-rings-and-a-ready-last-hit.md): §4, abilities' own cast effects, which this record extends into stages.
- [Initial Roster Character Bible](../Design/Veyra_Initial_Roster_Character_Bible_v0.6.md): each skill's description, which its motion and effects follow.
- [Asset & VFX Pipeline](../Production/VEYRA_ASSET_AND_VFX_PIPELINE.md): §12, effects are presentation events and never authoritative.

## Context

Before this record:
- **One cast clip:** every archetype has one Cast clip, so every skill a Vanguard owns plays the same short rise and release. Cairn's hook, his slam, his brace and his ultimate look alike.
- **One cast flash:** every cast commits with the shared flash, unless `AbilityCastEffects` names a one-shot replacement. Only Patch's abilities do, and they reuse existing systems.
- **Nothing between commit and result:** a projectile wears the shared trail whatever it is, and a cast shows nothing while it winds up or channels except its telegraph.
- **Bodies don't react:** a body's glow and its bone effects (Varkesh's embers, Cairn's drip) look the same whether it is idle or straining through an ultimate.

What clients already receive:
- each caster's cast state, including its ability, phase, the time the phase ends and its direction (`FVeyraCastState`);
- each caster's latest commit (`FVeyraCastCommit`);
- each projectile's ability and launch data (`AVeyraProjectile`).

Everything this record adds is drawn from that state. No new replication is needed.

## Decision

### 1. A skill may have its own clip
- **The kit names it:** a Vanguard's entry in `VanguardKit.json` may list `skills`. Each one gives:
  - an ability ID from `Abilities.json`;
  - a **motion**, one of the shared motion primitives (§2);
  - the clip's length, the share of it that rises to the release, and any options its motion takes (which arm; how far it reaches).
  
  These are art values, never tuning.
- **One take per skill:** the generator keys each listed skill as its own take, named `Skill_<ability>`, beside the archetype's eight. The importer imports it as `AS_<Id>_Armature_Skill_<ability>`.
- **The art set holds them by ability:** `FVeyraVanguardBody::AbilityCasts` maps an ability ID to its sequence and release share. A status body lists its own, since its skeleton may differ.
- **The fallback is the Cast clip:** a skill with no clip of its own plays Cast exactly as before. Every Vanguard keeps working while the roster is done one by one.
- **The generator validates the list:** every listed ability must be in that Vanguard's slots in `Vanguards.json`, and every motion must exist for the body's archetype.

### 2. Motion primitives
- **Where they live:** `Game/Scripts/VanguardBodies/motions.py` holds poses for the upper body, shared by every archetype with the humanoid's arm and spine chain: humanoids, colossi, constructs, and riders' riders.
- **How each is shaped:** each primitive has anticipation, a release at its release share, follow-through and a settle, so it reads as weight rather than a pose change.
- **Archetypes add their own parts:** a colossus braces its legs and lowers its pelvis. A construct gathers or scatters its orbiting parts and lifts its core.
- **The pilot set:**

| Motion | What it shows | Pilot skills |
|---|---|---|
| Hurl | one arm wound back across the body, then flung out straight at full reach and held there | Cairn Iron Grasp |
| Slam | both arms raised high on a rising chest, then driven into the ground before the body | Cairn Crushing Hold |
| Brace | feet planted wide, the body sinking, forearms crossed before the chest | Cairn Immovable |
| Anchor | arms raised, then driven down into the ground on bent knees and held there until the eruption | Cairn Burden of the Depths |
| Lob | one arm swung low and back, then heaved up and over in an arc | Varkesh Slagshot |
| Harden | hunched in on itself with arms drawn in, then flexed out as the shell sets | Varkesh Tempered Shell |
| Sweep | one arm swept low across the body, pouring along a line | Varkesh Molten Ground |
| Heave | both arms scooped low behind, then hurled forward and up | Varkesh Forge Divide |
| Clench | a fist raised and crushed shut, the arm snapping down | Varkesh Shatterforge |
| Thrust | one hand drawn back to the shoulder, then driven forward and open | Oriel Splinter Lance |
| Summon | one arm raised high, palm up, the body lifting with it | Oriel Shattered Sky |
| Veil | arms folded in close, the body turning inside its parts | Oriel Mirror Veil |
| Beam | both hands raised and joined overhead, then thrust forward and held for the channel | Oriel Final Radiance |

### 3. The clip is timed to the cast
- **Windups end on time:** a skill's clip rises to its release over the time left in its windup (`PhaseEndsAt`), at the play rate that makes it land as the cast commits. This is the rule ADR-064 §3 applies to an attack's windup, within the same play-rate limits.
  - A cast with no windup plays from its release at once.
  - The Cast fallback keeps its own pace, as before.
- **Holds and cancels as before:** the clip holds at its release while the cast is held (a windup waiting, or a channel such as Final Radiance), plays its release on commit, and lowers without it if cancelled. These are the existing Cast rules, applied to whichever clip the cast plays.
- **What carries the ability:**
  - The CastWindup cue now carries the time its windup ends, as an attack's cue does.
  - The animation inputs carry the held cast's ability, so a body first seen mid-cast takes up the right clip.
- **Presentation only:** nothing reads the animation back.

### 4. Staged skill effects
`AbilityCastEffects` becomes `AbilityEffects`, a per-ability recipe in the kit presentation settings. Each stage is optional, and the shared presentation stands in for any stage left out.

| Stage | When it plays | How it plays |
|---|---|---|
| **Windup** | from the CastWindup cue until the cast is no longer held (committed with no channel, cancelled, its caster dead) | a looping system on named bones of the caster's body, deactivated when the stage ends |
| **Channel** | while the cast is in its channel phase | a looping system from the caster along the cast's direction. Its length is a user parameter, read from the ability's zone in `Abilities.json` (Final Radiance's beam), so no reach is written twice |
| **Commit** | on the CastCommit cue | a one-shot burst at the caster's chosen bone, or at the cast's location, in place of the shared flash |
| **Travel** | while a projectile of the ability flies | its own trail in place of the shared trail |
| **Impact** | where a projectile of the ability ends, or where a delayed area of it lands | a one-shot burst. The landing time is the ability's `delaySeconds` from `Abilities.json`, from its commit |

- **Colour:** every stage is tinted by the caster's side, as the presentation's other effects are.
- **Validation:** recipes are validated at start like the rest of the kit presentation settings, every stage's system must load, and a test checks that every listed ability exists in `Abilities.json`.
- **Which systems go where:** the Windup, Channel and Travel stages take continuous systems, which their stage ends. Commit and Impact take one-shot bursts, which end on their own. This is a contract on the settings, not a load-time check: Niagara's looping flag does not tell a continuous emitter apart (it reads false for `NS_VeyraEmbers`).
- **No new decisions:** an Impact shows where the projectile's flight ended or the area landed, never whether it hit. Damage stays the hit cue's (ADR-063 §1), and an effect never shows a hit before its commit (Combat Bible §48).

### 5. Bodies strain while they cast
- **The glow surges:** while a cast holds its caster (a windup or a channel), its body's glow rises over `CastGlowRiseSeconds` to `CastGlowGain` times its usual glow, and eases back over `CastGlowFallSeconds`.
  - The body material carries a multiplier on its glow, named by the kit's `bodyMaterial.castGlow.parameter` (`CastGlow`, 1 at rest). The presentation sets it, so changing a gain regenerates no body.
  - The gains and timings are presentation settings (`UVeyraGreyboxSettings`), beside the veil's.
  - Varkesh's seams blaze, Cairn's moss-light wakes and Oriel's core flares.
  - A body gets a material instance of its own only once it first casts, as the veil does (ADR-068 §6). The veil and the glow share one refresh, so neither overwrites the other.
- **Bone effects surge with it:** effects poured off a body's bones (ADR-064 §1) grow to `CastEffectGain` of their size at full strain.

### 6. New generated effect systems
These are generated from `Effects.json` in the graphic language of ADR-068 §1: hard shapes, white-hot cores, side colours.
- **`NS_VeyraChargeSwirl`** (looping): motes spiralling in to a point, for windups.
- **`NS_VeyraShockwave`** (one-shot): a flat ground ring with flung debris, for slams and eruptions.
- **`NS_VeyraSlashArc`** (one-shot): a crescent of light, for thrusts and sweeps.
- **`NS_VeyraBeam`** (looping): a core ribbon with a sheath along a length parameter, for channels.
- **`NS_VeyraShardRain`** (one-shot): shards falling onto a circle, for delayed areas.
- **`NS_VeyraMoltenSplash`** (one-shot): molten blobs and a scorched ring, for molten impacts.
- **`NS_VeyraShellSet`** (one-shot): plates snapping inward to a shell, for hardening and shields.
- **`NS_VeyraChainTrail`** (looping): a heavy chain-link trail, for hooks.

The pilot kits' existing effects (`NS_VeyraEmbers`, `NS_VeyraDrip`) serve as their bodies' windup surges.

### 7. Order of work
1. The runtime and data: §1, §3, §4 and §5, with the Cast fallback, so nothing changes for a Vanguard without data.
2. The pilot: Cairn, Oriel and Varkesh, captured in engine at the gameplay camera before and after, and reviewed by the author.
3. The rest of the roster in batches, after the author approves the pilot's look.

## Configuration
- **Skill clips:** the kit's `skills` per Vanguard, and the motion primitives in the generator. Art values.
- **Effect recipes:** `[/Script/VeyraUI.VeyraKitPresentationSettings]` `AbilityEffects` in `Game/Config/DefaultGame.ini`. Presentation, validated at start.
- **Cast glow:** the material parameter in the kit's `bodyMaterial.castGlow` (art, read by the importer); `BodyCastGlowParameter`, `CastGlowGain`, `CastEffectGain`, `CastGlowRiseSeconds` and `CastGlowFallSeconds` in `[/Script/VeyraUI.VeyraGreyboxSettings]` (presentation, validated at start).
- **Channel length:** `ChannelLengthParameter` in the kit presentation settings names the user float a channel's effect takes its length by. The effects generator gives a system user floats of its own (`userFloats` in `Effects.json`), as `NS_VeyraBeam`'s `Length`.

## Consequences
- **Each skill reads as itself:** a skill with data shows its own motion, windup, release and result, and the telegraph becomes one cue among several.
- **More takes per FBX:** a Vanguard's FBX grows by one take per listed skill, typically four or five. A body rebuilt for new takes is new LFS content, so each Vanguard's skills land in one rebuild.
- **More effects per fight:** the windup and channel stages add pooled looping components while casts last. ADR-068 §5's budget applies, and the pilot's gate measures a ten-Vanguard fight with every skill in use.
- **`AbilityCastEffects` migrates:** Patch's entries move to `AbilityEffects` as Commit stages, unchanged in look.

## Provisional answers for the author
1. Every skill's motion is chosen from its Character Bible text (§2's table).
2. A skill clip's rise is fitted to its windup, and the Cast fallback keeps its own pace (§3).
3. The stages and their names (§4).
4. The glow surge and its values (§5).
5. The new effect families (§6).
