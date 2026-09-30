# ADR-030: Invisibility, Untargetable, placed markers and marked-target follow-ups, for Tavi

**Status:** Proposed. The author's standing instruction (2026-09-28) is to keep working unreviewed toward a viable game, choosing a provisional answer where canon is silent. §11 lists every such answer for the author to overturn. This ADR becomes Accepted when the author merges the pull request that adds it.
**Date:** 2026-09-30
**Related:**
- [Initial Roster Character Bible](../Design/Veyra_Initial_Roster_Character_Bible_v0.6.md) §6: Tavi, including the Vision ruling of 2026-09-20 that **Hide!** is Invisibility.
- [Combat Bible](../Design/Veyra_Combat_Bible_v0.5.md):
  - §9: blinks, dashes;
  - §10: Untargetable;
  - §11: Camouflage, Invisibility, the general stealth rules;
  - §32: decoys and owned entities.
- [Vision Bible](../Design/Veyra_Vision_Bible_v0.1.md) §5: True Sight is the only reveal of Invisibility.
- [ADR-003](ADR-003-owned-field-entities.md): the placed-marker primitive, which this ADR builds first.
- [ADR-016](ADR-016-vision.md): the fog gate and True Sight.
- [ADR-018](ADR-018-kit-primitives.md): Camouflage, recast windows. Untargetable was an open item.
- [ADR-028](ADR-028-blind-grounding-and-collision.md): Grounded, and the Dash verb's refusal.

## Context

Tavi is a magic assassin whose kit rests on a mark, **It**.
- **Passive:** she moves faster toward It, and her next basic attack on It spends it.
- **Q:** a ball that comes back when it strikes It.
- **W:** turns her Invisible and leaves an illusion behind.
- **E:** dashes through a target and may repeat against It.
- **R:** she vanishes and reappears beside an enemy she recently hurt.

None of these exist yet: Invisibility for a Vanguard, Untargetable, a blink, cooldown refunds, an owned entity of any kind, a dash through a unit, a conditional follow-up, or a shot that returns.

## Decision

### 1. Invisible (Combat, Vision)

- Combat gains the status kind `Invisible`.
- An Invisible unit is hidden from its enemies regardless of distance. Only True Sight reveals it (Combat Bible §11; Vision Bible §5).
- Like Camouflage, it ends when its holder attacks or casts an offensive ability. Taking damage does not end it.
- It does not make its holder Untargetable. A targeted projectile already launched still connects.
- The fog gate treats an Invisible unit as a ward is treated: seen by its own side, by True Sight and by a tether to it, and by nothing else.

### 2. Untargetable (Combat)

Combat gains the status kind `Untargetable`, with the Combat Bible §10 rules. While a unit holds it:
- enemies cannot acquire it: orders, targeted casts, basic attacks, towers, Fluxborn and bots;
- enemy skillshots, areas and cleaves pass over it;
- a targeted projectile flying at it fails if it is still Untargetable on arrival.

Allies' effects still reach it. It does not stop its holder from acting, and it cleanses nothing.

### 3. Cooldown refunds (Abilities)

- `VeyraCooldowns::Reduce` and `UVeyraCooldownComponent::Reduce` shorten a running cooldown by a fraction of what remains, or by seconds.
- Refunds key by the loadout's cooldown id, as starting a cooldown does. Everything that reads `ReadyAt` sees the change: the HUD, the bots, the recast checks.

### 4. Blink (Combat)

- `VeyraCombat::Blink` moves a unit instantly to the nearest legal ground at a destination (Combat Bible §9). It ends any dash, and stops the unit's orders.
- Root and Grounded refuse it, as they refuse dashes (ADR-028 §2). An ability that blinks reports that it moves its caster.

### 5. Placed markers (Combat)

`AVeyraPlacedMarker` (VeyraCombat, `Entities/`) is ADR-003's placed marker: an owned thing on the battleground with no combat behaviour of its own.
- **Ownership:**
  - It has an owner participant, and that owner's side.
  - What it causes is credited to its owner (Combat Bible §32).
  - Killing it pays nothing and is not killing its owner. It inherits no items, buffs, crit or on-hit.
- **Lifetime:** it lasts a tuned time and ends early when its owner dies.
- **Health:** a marker may have Health. A marker with Health is a unit of kind `Marker` that enemies can target, and it dies to one hit.
- **Events:** it announces when it is struck and when it ends, with the reason: expired, destroyed, recalled or owner dead.
- **Appearance:** it can present itself as its owner. Enemies then see its owner's body, name, level and Health bar, and its minimap dot is its owner's. That is what makes a decoy deceive.
- **Networking:** it replicates as a gated unit (ADR-016).

Angeru's False Body will reuse it.

### 6. A dash through a target (Abilities)

- A dash may be `ThroughTarget`: cast on an enemy unit, it carries the caster through that unit to a tuned distance beyond it.
- Its contact effects land on that unit as it passes.
- It needs the target in range and valid when it starts, as a targeted cast does.

### 7. Follow-ups that depend on a mark (Abilities)

A cast may name a **follow-up**: a recast that opens for a window only when a condition held.
- `ifTargetHeld` opens it when the cast's target already held a status from the caster as the cast landed (Tavi's E against It).
- `ifTargetFalls` opens it when the cast's target dies, with the caster credited, within a window (Tavi's R).

A follow-up ability may require its target to hold a status from the caster (`targetMustHold`). The recast itself is the ADR-018 recast window, opened by the condition instead of by every cast.

### 8. A returning shot (Abilities)

- A skillshot may `returnIfHeld`: when it strikes a unit that already held the named status from its caster, it flies back to the caster.
- When it reaches the caster it refunds a tuned fraction of the ability's remaining cooldown.
- The shot still applies its effects to what it struck, the status included.

### 9. Vanish and strike (Abilities)

A new archetype, `ambush`:
1. It is cast on an enemy Vanguard that the caster damaged within `recentSeconds`.
2. The caster turns Invisible and Untargetable for `vanishSeconds`.
3. It then blinks beside the target, if the target still lives, and delivers its effects.

Damage tuning gains `targetMissingHealthRatio`, a share of the target's missing Health, for any effect. Areas, skillshots and ambushes can all use it.

### 10. The passive: a quarry mark

A `quarry` passive map names:
- a mark status its owner's abilities apply;
- a move-speed bonus toward the mark's holder, within a range and angle, sampled on a timer;
- the next basic attack on the holder spending the mark, for bonus magic damage and a refund of a fraction of the owner's basic abilities' remaining cooldowns;
- a jump radius.

Rules:
- Only one enemy holds the mark at a time. Marking another takes it off the first.
- When the holder dies with the owner credited, the mark jumps to the nearest enemy Vanguard within the jump radius.

A missed (blinded) attack spends nothing (ADR-028 §1).

## 11. Provisional answers where canon is open

1. **Q, E and R mark It; W does not.** The mark lasts 6 s.
2. **Catch!**
   - The ball returns only when it strikes the Vanguard who is already It, and it always reaches Tavi.
   - Reaching her refunds 40% of Q's remaining cooldown.
   - A first ball marks; a second ball, on It, comes back.
3. **Hide!**
   - 1.25 s of Invisibility and a burst of speed.
   - The illusion stands where she was for 3 s and presents as Tavi.
   - One hit destroys it. Its destruction or her recast bursts it: magic damage and a slow around it.
4. **Tag!**
   - A dash through the target to 225 beyond it.
   - When the target was already It, a 3 s recast that can target only It.
5. **Ready or Not!**
   - Playmates are the enemy Vanguards she damaged in the last 4 s.
   - She vanishes for 0.6 s.
   - The strike deals base damage, plus Magic Power, plus 20% of the target's missing Health.
   - If the target falls within 2 s with her credited, a 6 s recast at half damage opens, once.
6. **You're It!**
   - 25% move speed toward It within 1200 and 45°.
   - The spending attack deals bonus magic damage by level and Magic Power, and refunds 25% of Q, W and E's remaining cooldowns.
   - The mark jumps up to 1000.
7. **Enemies cannot tell the illusion apart** from Tavi at a glance: same body, name, level and Health bar. Her side sees it as her illusion.

## Consequences

- Stealth becomes two grades on Vanguards, Camouflage (Mimzi) and Invisibility (Tavi), as the Roster Bible intends.
- The placed marker is the first owned entity. Its owner base (side, credit, lifetime, owner death) is where the combat entity (Nix, Picket, Waterling) starts.
- Untargetable changes acquisition everywhere at one point (`VeyraTargeting`), and every enemy gathering filter at one check.

## Tests

- **Combat:**
  - Invisible hides its holder from enemies at any distance, and True Sight reveals it;
  - an attack or offensive cast ends it;
  - Untargetable refuses acquisition, areas and a homing shot's arrival;
  - Blink refuses under Root and Grounded;
  - a marker credits its owner and dies to one hit.
- **Abilities:**
  - cooldown refunds;
  - the dash through a target;
  - follow-ups on a held mark and on a takedown;
  - the returning shot and its refund;
  - the ambush's vanish, blink and missing-Health damage.
- **Vanguards:** the quarry passive (one holder, speed toward it, spend on attack, jump on a kill), and Tavi's kit from the committed tuning.
- **Net:** an Invisible Tavi and her illusion as each side sees them.
