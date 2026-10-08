# ADR-071: Playtest readability: strands, kit rings and a Ready last hit

**Status:** Proposed. On 2026-10-08 the author reported a two-player playtest: the player on Patch "had no idea what was happening", neither player could see his tether, last-hitting was very hard, and movement "felt like sliding on ice". The author asked for real effects for combat readability, not more indicators, and chose a two-stage last-hit cue. This ADR becomes Accepted when the author merges the pull request that adds it.
**Date:** 2026-10-08
**Related:**
- [ADR-063](ADR-063-combat-readability-cues-effects-sound-and-the-fountain-shop.md): cues read from replicated state, and the generated effect systems.
- [ADR-064](ADR-064-generated-animated-vanguards.md): §3, Run stepping at the speed the body is carried.
- [ADR-065](ADR-065-first-match-feedback-waves-spires-rewards-and-patch.md): §6, the last-hit cue (amended by §5 here); §8, Patch's tether siphon; §11, bodies drawn larger than their capsules.
- [ADR-062](ADR-062-side-based-unit-collision-local-avoidance-and-hosted-diagnostics.md): §3, Vanguard avoidance settings.
- [ADR-068](ADR-068-toon-characters-and-graphic-combat-effects.md): §4, the graphic effect families.
- [ADR-018](ADR-018-kit-primitives.md): the tether primitive (Combat Bible §43).

## Context

Patch's kit was mostly invisible:
- His tether lived only in the server's tether subsystem. Clients received the target's status, but not which unit held the other end.
- The aura and the shove of The Thing Inside, and the fear pulse that ends Play Dead, had no presentation.
- The haunting mark and the hug had none of their own.
- Every cast showed the same flash.

Two other problems came from the same playtest:
- **Sliding:** a body's Run cycle was fitted to the stride its generator measured at the size it was made. The presentation then drew it 1.65× larger (ADR-065 §11), so its feet swept 1.65× farther than its capsule moved.
- **Last hits:** the last-hit cue lit only once one attack would finish the unit. A ranged attack needs 0.7–1.1 s from click to damage, so by then the wave had usually taken the kill.

## Decision

### 1. A body's stride grows with the size it is drawn at
- The animation's shape carries the scale its skin is drawn at.
- One Run cycle carries the body its stride times that scale, so Run plays slower on a larger body and its feet stay planted.
- The animation instance reads the scale from its skin each frame, so a later change to the body scale cannot bring the sliding back.

### 2. Statuses carry the body that applied them, and a tether shows as a strand
- **The ledger names the source:** `FVeyraStatusEntry` carries `SourceBody`, the avatar of the unit that applied it, and `SourceTeam`, its side. Both replicate with the ledger for presentation, and nothing in gameplay reads them.
- **The side outlives the body:** where `SourceBody` does not resolve (its unit is beyond the viewer's relevancy), what shows the status keeps the applier's colour from `SourceTeam` rather than taking the holder's.
- **This is the general primitive:** any status can be drawn between its two units. That avoids a second, tether-only replication path.
- **Which statuses draw a strand:** those listed as `StatusStrands` in the kit presentation settings.
- **How it looks:**
  - a strand between the two units at a share of their drawn height, in the source's side colour;
  - beads of the trail effect flow along it from the holder to the source, which reads as the siphon.
- **Visibility:** a strand shows only while both ends do.

### 3. Self-buff auras and end payloads show as rings, from Abilities.json
- **How a buff is recognised:** a self-buff is known on every machine by the first of its statuses, which its recipient holds from the cast: its caster, or an ally a `CasterOrAlly` buff was cast on.
- **Auras:** while the buff's aura lasts, a ring at its radius surrounds the buff's holder, as the aura itself follows its recipient, with a second ring sweeping across it.
- **End payloads:** the buff fires its first payload alone, from its caster's body. At its `afterSeconds` a ring spreads from the caster (the status's applier) to its radius and the burst effect plays. Only a living caster's payload shows.
- **Only what always comes shows:** a payload that needs hits its caster took (`minHits` above 0) cannot be told from what a client receives, so it shows no burst rather than one that may never come. Showing those needs a replicated signal that the payload fired; it is deferred.
- **Reach and timing come from `Abilities.json`:** no radius or time is written twice.
- **Scope:** this covers every Vanguard with an aura or an end payload, not only Patch.

### 4. Status marks and abilities' own cast effects
- **Marks:** `StatusMarks` lists statuses shown by an effect on the unit holding them, in the colour of the side that applied them. Patch's haunting mark is smoke; his hugs are mist.
- **Cast effects:** `AbilityCastEffects` lists abilities whose cast plays its own effect in place of the shared flash.
  - Only one-shot burst systems may be listed: a looping system on a pooled, self-releasing component would never end.
- **No new assets:** both reuse the generated effect systems. New systems for these slots come with M60's animation and effect work.

### 5. A Ready stage before the last hit
- **Before gold:** a damaged enemy Fluxborn's or creature's bar reads Ready while an attack started now would land as its Health falls to one attack. It turns gold once it gets there.
- **How Ready is judged:**
  - the attack's lead is its windup, from the attacker's current attack timing and its profile's windup fraction, plus its projectile's flight to the unit;
  - the unit's loss is the Health it lost over the last `LastHitLossWindowSeconds`, sampled from its Health each frame. Shield damage is left out, since an attack must break a shield before Health falls, and a heal only moves the mark Health falls from.
- **A unit nothing else is hitting goes straight to gold.** Waiting costs nothing there.
- **It remains a hint:** the server decides who lands the last hit.

### 6. Vanguard avoidance considers a smaller radius
- `Match.json` `orders.avoidanceConsiderationRadius` goes from 300 to 150 (provisional), so a Vanguard weaves less through an enemy wave on its way to a click.
- The own-body lead (ADR-067 §2) is unchanged. With §1, its Run before the server's movement arrives is one round trip's starting step, not a slide.

### Configuration
- Strands, rings, marks and cast effects: `[/Script/VeyraUI.VeyraKitPresentationSettings]` in `Game/Config/DefaultGame.ini`. Presentation, validated at start.
- The Ready colour and the loss window: in the grey-box settings, beside the last-hit colours.
- `UVeyraKitPresentationSubsystem` draws §2–§4. It is a separate presentation subsystem, so the grey-box subsystem does not grow further.

## Consequences

- Patch's tether, aura, pulses, haunting and hug are visible on every client, and other kits' auras and end payloads light up the same way.
- The status ledger grows by one object reference per entry.
- A source the viewer's machine does not hold, such as one out of relevancy, resolves to null, and its strand does not show.
- The rings and the strand are line batches at foot height. On steep terrain a ring may cut into a slope; projecting rings onto the ground is deferred.
- Per-ability animation (a real hug, a real collapse) remains M60's.
