# ADR-035: Sea States, crashing rides and summoned companions, for Neris

**Status:** Proposed. The author's standing instruction (2026-09-28) is to keep working unreviewed toward a viable game, choosing a provisional answer where canon is silent. §9 lists every such answer for the author to overturn. This ADR becomes Accepted when the author merges the pull request that adds it.
**Date:** 2026-10-01
**Related:**
- [Initial Roster Character Bible](../Design/Veyra_Initial_Roster_Character_Bible_v0.6.md) §11: Neris, The Tidebound, and the Waterling.
- [Combat Bible](../Design/Veyra_Combat_Bible_v0.5.md):
  - §6: restoration;
  - §9: forced movement;
  - §24: Ghosted;
  - §32: summons and companions;
  - §56: ride states.
- [ADR-018](ADR-018-kit-primitives.md): §5, lingering areas; §7, rides, mounted actions and vehicles.
- [ADR-031](ADR-031-stances-focus-shadows-and-cross-marks.md) §3: stances.
- [ADR-034](ADR-034-companions-for-marek-and-nix.md): companions and commands, which this ADR extends to a summoned companion.

## Context

Neris rides the sea and switches between two Sea States.
- **Passive, Sea State:** Calm Waters emphasise healing and support; Storm Waters emphasise damage.
- **Q, Breaking Wave:** she rides a living wave forward with slight steering. A recast, or the ride's end, crashes it: it damages enemies and heals allies. Calm strengthens the heal and Storm the damage.
- **W, Little Current:** one targetable, killable Waterling, only one at a time.
  - In Calm it follows an ally, heals it now and then and grants slight Movement Speed.
  - In Storm it hunts an enemy, dealing magic damage and slowing.
  - A recast redirects it.
- **E, Change the Weather:** switches between Calm and Storm, with a cooldown. It must not become a free toggle to spam.
- **R, TIDEBREAKER:** a huge steerable wave.
  - Its direct impact damages enemy Vanguards and knocks them aside, and burst-heals allies.
  - The cast locks the current Sea State and leaves a trail:
    - Calm's Healing Wake gives allies Movement Speed and regeneration;
    - Storm's Riptide gives enemies damage over time and a slow.

The survey for this ADR found that:
- **Rides** (§56) already give a set speed, a limited turn rate, ghosting, mounted actions and decay. A ride ends with nothing but an optional vehicle skillshot, and no mounted action can end it.
- **Stances** swap their slots' own abilities and stow the others. Cooldowns are held by ability ID, so each set cools down apart. A Sea State switch would therefore refresh every slot, which is exactly the spam the bible forbids. Every cooldown is read through the loadout's `CooldownIdOf`.
- **Area zones** reach enemies only. Heals reach their caster, or allies around their caster.
- **Companions** follow their owner or hold a point, last for good and reform. Nothing summons one for a while, binds it to another unit, or lets its attacks carry statuses.

## Decision

### 1. Casts that share another ability's cooldown (Abilities)

- A cast gains **`cooldownOf`**: at most one other ability whose cooldown it shares. Its cooldown is held under that ability's ID, so casting either starts the one cooldown, at the length of the ability cast.
- The named ability must be defined and must name none itself.
- Neris's Storm abilities share their Calm counterparts' cooldowns, so switching never resets a slot. Change the Weather's own cooldown stops it being a free toggle.

### 2. Casts refused while a status holds (Abilities)

- A cast gains **`refusedWhile`**: status IDs on its caster that refuse it, as `HeldBack`. (`Locked` stays the refusal of a Flux Spell slot not yet unlocked.)
- Tidebreaker gives Neris its lock status for as long as it rides, and Change the Weather names that status. That is how the ultimate locks the Sea State.

### 3. Rides that crash (Abilities)

- A ride gains **`crashZones`**, innermost first. They erupt where the rider is as the ride ends, as the rider's hit, on every end but death.
- **The `dismount` archetype** ends its caster's ride at once. A ride's mounted action may be one, so a recast crashes the ride early.
- Breaking Wave is a ride whose mounted Q dismounts. Its crash zones damage enemies and, by §4, heal allies.

### 4. Zones that reach allies (Abilities)

- An area zone gains **`allyEffects`**:
  - a heal: `amount` and `amountPerLevel`, plus `magicPowerRatio` of the caster's Magic Power;
  - statuses;
  - `includesCaster`: whether the caster is among the allies it reaches.
- The heal is the caster's: it passes through `VeyraCombat::RestoreHealthFrom`, so every restoration rule and modifier applies (Combat Bible §6). It reaches allied Vanguards in the zone's shape.

### 5. Summoned companions (Abilities; extends ADR-034)

- **Two command orders:**
  - `Summon` forms the caster's companion, `companion` in the command's data, for `lifetimeSeconds`, bound to the unit the cast names:
    - an ally: it **escorts** that ally, keeping within its follow distance;
    - an enemy: it **hunts** that enemy, attacking it while it stays within the companion's leash range of its owner, and goes back to its owner once the enemy dies or leaves.
  - `Redirect`, the summon's follow-up, binds the living companion to a new unit, keeping its remaining time.
- **One at a time:** an owner keeps one companion (ADR-034 §3). Summoning again while one lives redirects it.
- **No reform:** a summoned companion that is killed or runs out of time is gone until the next cast. Killing it pays nothing, as for any companion (ADR-034 §1).
- **A companion definition gains:**
  - `escort`: a pulse (`pulseSeconds`, a heal and statuses) on the ally it escorts;
  - `attackStatuses`: statuses its basic attack's hit gives, as the Waterling's slow.
- **Modes:** the controller gains Escort and Hunt beside Follow and Hold.

### 6. Rides that strike what they meet, and leave a trail (Abilities)

- **A ride gains `contact`:** effects on each unit its rider's body meets, once per unit per cast.
  - `enemyEffects`: damage, statuses and a displacement. A knock aside is `AsideFromPath`, out of the rider's line.
  - `allyEffects`, as §4's.
  - `reach`: how far past the rider's edge it touches.
- **A ride gains `trail`:** lingering areas laid along its path every `spacing` units, each with its shape and its lingering statuses by side (ADR-018 §5).
- **Tidebreaker** is a ride with a larger body, contact and a trail. Calm's lays Healing Wake (allies' Movement Speed and regeneration) and Storm's lays Riptide (enemies' damage over time and a slow).

### 7. Neris (Vanguards)

- **Her kit is data:**
  - Calm and Storm variants of Q, W and R, the Storm ones sharing their Calm counterparts' cooldowns;
  - the stance E, with its own cooldown;
  - the lock status;
  - the Waterling;
  - her statuses and text.
- **The Sea State passive** is the variants' values.
- **Bots:**
  - ride Breaking Wave toward a fight;
  - send the Waterling with an ally in Calm, at an enemy in Storm;
  - switch Sea State as their stance profile says.

### 8. Vision and presentation

- **The Waterling** is a companion: gated, seeing by `sight.companion`, drawn as a unit with a bar.
- **A ride's crash, contact and trail** use the existing area presentation; trail areas are lingering areas.

### 9. Provisional answers where canon is open

1. **The Sea State passive** is the variants' values; there is no separate passive effect.
2. **Change the Weather** shares cooldowns between the states and has a cooldown of its own.
3. **The Waterling** lasts a fixed time and does not reform.
4. **A redirect** keeps the Waterling's remaining time. **An escort** may be its caster, so Neris alone in a lane can keep the Waterling herself.
5. **Breaking Wave's crash** happens on every end but death.
6. **Tidebreaker's impact** strikes each unit once per cast; the knock aside pushes a Vanguard off the wave's line.
7. **The lock** lasts as long as the Tidebreaker ride.
8. **The values** in Abilities.json and Vanguards.json are Provisional playtest settings.

## Consequences

- Rides gain an end that does something, and a body that strikes as it passes, both reusable by a later rider.
- A summoned, time-limited companion tests ADR-034's subsystem beyond Nix. Picket can build on it.
- A cast that shares another's cooldown serves any future two-mode kit without a refresh exploit.
