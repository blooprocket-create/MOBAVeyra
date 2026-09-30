# ADR-014: The jungle and Flux Wells: neutral units, camps, traits and the Well objective

**Status:** Proposed. The author's standing instruction (2026-09-28) is to keep working unreviewed toward a viable game, choosing a provisional answer where canon is silent; §9 lists every such answer for the author to overturn. It becomes Accepted when the author merges the M10a pull request that adds it.  
**Date:** 2026-09-28  
**Related:** [ADR-009](ADR-009-runtime-combat-primitives.md) (targeting, statuses, kill credit), [ADR-011](ADR-011-battleground-runtime.md) (World, Flux, rewards, the layout, Match routing), [ADR-013](ADR-013-ai-vanguards.md) (bots), [Battleground Bible](../Design/Veyra_Battleground_Bible_v0.9.md) §6–§9, §17, [Economy & Progression Bible](../Design/Veyra_Economy_Progression_Bible_v0.1.md) §7, §8.2, [Combat Bible](../Design/Veyra_Combat_Bible_v0.5.md) §29, §44, [Architecture Constitution](../../ARCHITECTURE.md) §1.3, §1.7.

## Context

After M9 the battleground has lanes, structures, waves, items and bots that play a whole match. Two of the Battleground Bible's defining systems are still missing, and players would miss them first:
- **Jungle wildlife** (§7, §8, §17; Economy §7): native fauna in the inner and outer jungle.
  - Camps have fixed identities, and each creature pays at its death: Gold to its credited killer only, and XP shared by that team's nearby living Vanguards.
  - A cleared camp grants a temporary species trait and respawns on its own timer.
  - Creatures leash home and reset. The first camps appear in two spawn groups, at 0:55 and 1:07.
- **North and South Flux Wells** (§6; Economy §8.2): neutral objectives that open at 6:00.
  - A Well's Health is drained by Vanguard damage and by capped allied presence; contested presence stalls the drain or slows it.
  - The last hit, by damage or by a presence tick, secures it, so it can be stolen.
  - A secure grants +50 temporary Team Flux for 3 minutes, and a small Gold pool to the capturers present at that moment; no XP.
  - The Well then waits 5 minutes before it opens again.

Flux Spells (§14) come in M10b, since they reach champion select, the assignment contract and the backend.

What the code has today (surveyed 2026-09-28):
1. **Targeting ignores team None.** `VeyraTargeting::AreHostile` is false whenever either side is None, and every attack, ability, area, skillshot, attack-move, kill-credit and attribution path goes through it, so a neutral unit cannot be fought at all. Making None a blanket enemy is wrong too: the Fluxborn's and towers' target gathering also goes through `AreHostile`, so lane minions and towers would fight wildlife.
2. **Structures cannot be Wells.** Combat §33's structure rules let only basic attacks and tower shots damage structures, but §6 says Vanguard abilities drain a Well. And the battleground link treats any destroyed structure without a Flux source as the Prime Well, so a neutral structure's death would end the match.
3. **Statuses give most traits.** Timed stat statuses already cover movement speed, attack speed and damage reduction; they run on world time (a pause holds them) and end at death (Combat §44). Sustain and impact have no status kind yet.
4. **The HUD already draws any unit.** It gives a unit on no side a grey bar and a grey body.

## Decision

### 1. Neutral units: a targeting category, not an implicit enemy (Combat §29)

`EVeyraUnitKind` gains **`Wildlife`** and **`Objective`**. Both are neutral: they belong to no side.

`VeyraTargeting::AreHostile(A, B)` keeps its rule for the two sides and adds one explicit category: a neutral unit is hostile to anything on a side except a Fluxborn or a structure, and the reverse. So:
- Vanguards and what they cast (projectiles, areas, their attacks) harm wildlife and Wells;
- wildlife fight back;
- Fluxborn and towers neither choose nor answer them, and two neutral units are never hostile.

Through the existing paths, kill credit, attribution, `OnHostileDamage` (so a creature's hit interrupts a Recall) and the bots' attack orders all work unchanged. No named content appears in Combat.

### 2. Wildlife is World's

- **`AVeyraWildlife`** follows the Fluxborn pattern: an `ACharacter` on `UVeyraMovementComponent`, its own Minimal ASC, the combat components and a basic attack. It is kind `Wildlife`, on no side, and replicated with push model at World's unit rate.
- **`AVeyraWildlifeController`** is a server AI on a world-time think timer:
  - it waits at its spot;
  - hit by anyone, its whole camp turns on the attacker (camp aggro), and each creature keeps to the latest Vanguard that hurt the camp;
  - a creature that would chase beyond its camp's leash radius, or loses every target, walks home and is restored to full Health (a reset);
  - it never leaves its camp's area.
- **`UVeyraJungleSubsystem`** (a World subsystem beside the battleground's, so the latter does not grow):
  - spawns each camp at its spawn time on the match clock, which Match starts at Live as it starts the waves;
  - reports each creature's death to Economy (`RewardWildlifeDeath`) with its species;
  - on a camp's clear, grants its trait to the Vanguard credited with the last kill and schedules the camp's own respawn;
  - stops at match end.
- **Traits** are statuses from `Abilities.json`, named by the species in `World.json`, applied through `VeyraCombat::ApplyStatus`.
  - Two status kinds join Combat: `HealthRegeneration` (a multiplier on Health Regeneration) and `DamageAmplification` (a multiplier on outgoing damage, the counterpart of `DamageReduction`).
  - Traits last their status's duration and end at death (Combat §44).

### 3. Wildlife rewards (Economy §7)

`UVeyraRewardSubsystem::RewardWildlifeDeath(Death, Species)` holds no formula. It pays through the existing rules and `Economy.json`'s `gold.wildlife` and `experience.wildlife`, keyed by species:
- **Gold:** all of it to the credited killer, a Vanguard, wherever it stands. There is no participation share.
- **XP:** shared by the credited killer's team's living Vanguards within the reward radius: 100% alone, or a 120% pool split among several, with Level 18 excluded.
- **No credited killer:** the Gold is unclaimed, and each side's nearby living Vanguards share the XP under the same rule.

### 4. Flux Wells are neutral objectives

- **`AVeyraFluxWell`** is an `APawn` of kind `Objective`, on no side. It has the ASC, vitals, resistances and combat components, but no attack and no movement. It is not a structure, so Vanguard abilities damage it.
- **`UVeyraFluxWellSubsystem`** (World) spawns the two Wells at the layout's sites and keeps each in one of three states: *closed* (invulnerable, until its opening time on the match clock), *open*, or *respawning* (after a secure).
- **Presence drain**, on a world-time timer (a pause holds it):
  - allied living Vanguards within the Well's radius count, up to a cap;
  - one side present drains at the rate for its count;
  - both sides present: equal counts stall it, and the larger side drains at a reduced rate for the difference;
  - each tick is True damage from one of the draining Vanguards, with a new delivery kind `Presence`, so a presence tick can land the last hit and Combat's death and kill credit decide the securing team;
  - damage from either side drains it as any damage does;
  - with nobody draining and no damage for a while, it regenerates.
- **Securing:** at 0 Health the Well is secured by the credited killer's side.
  - World broadcasts `OnFluxWellSecured(Team, Site)`.
  - Match grants that team the new Flux source `FluxWell` (`Flux.json`: +50, temporary, 180 s).
  - World pays the Gold pool through Economy (`RewardFluxWellSecured`), split evenly among the securing side's Vanguards counted present at that moment.
  - The Well then respawns after its cycle, at full Health.

### 5. The layout

`World.json` gains:
- **`wildlife`:** species (stats, basic attack, body, trait status), the camp AI, and Team A's camps (species, count, centre, spawn time, respawn time, leash radius). Team B's camps are their mirror, as its structures are.
- **`fluxWells`:** the sites, on the river's diagonal, so each is its own mirror; opening and respawn times, Health and resistances, radius, presence rates, cap, contested factor and regeneration.

World's validation keeps camps on the floor and off the lanes, on Team A's half, and keeps Well sites on the river. Tests use a compact layout of their own.

### 6. Match routes, as before

`FVeyraBattlegroundLink`:
- starts the jungle and the Wells when the match goes live;
- routes `OnFluxWellSecured` to Flux;
- stops them when the match ends.

No victory rule changes: neither kind of neutral unit is a structure.

### 7. Bots (amends ADR-013)

- The fifth seat plays jungle.
  - It clears its side's camps nearest first, backs and recalls when hurt, and ganks a lane when an enemy Vanguard there is hurt and near.
  - Its camps, route and thresholds are data in `Bots.json`.
- A laner whose lane is quiet joins an open Well within its reach.

### 8. Values are data

Every number above is in `World.json`, `Flux.json`, `Economy.json`, `Abilities.json` and `Bots.json`, marked Provisional unless canon gives it. Canon gives:
- the spawn groups (0:55, 1:07);
- a Well's opening time (6:00), grant (+50 temporary for 3 minutes) and cycle (5 minutes).

### 9. Provisional answers where canon is silent (for the author to overturn)

1. **Camps:**
   - Six per side, one of each working species (§8): Ashfang (movement speed), Stonehorn (damage reduction), Skittermaw pack (three creatures, attack speed), Miremother (Health Regeneration), Razorback (damage amplification), Gloomwing (no trait until Vision gives it tracking).
   - Four sit in the inner jungle and two in the outer, mirrored.
2. **Traits:** the Vanguard that lands the camp's last kill takes the trait, for 90 s.
3. **Camp aggro and reset:** a camp answers together. A creature beyond its leash, or without a target, walks home and heals to full.
4. **The Well:**
   - Health 4000, which one Vanguard takes in roughly 45 s by presence and attacks, and three in roughly 25 s.
   - Presence counts at most three per side.
   - Contested, the larger side drains at half rate for the difference.
   - It regenerates 2% of its Health a second once nobody has worked on it for 5 s, and an opened Well stays open until secured.
5. **Gold pool:** 80 Gold, about one large creature (§8.2).
6. **Bots:** the fifth bot jungles. Laners take a Well only when their lane is quiet.

## Consequences

- The jungle and Wells add two World subsystems, two actors and their controllers. Combat gains one hostility category, two status kinds and one delivery kind; Economy gains two reward entries; Flux gains one source; Match gains routing. No module changes layer, and nothing names a species or a Well in code.
- Fluxborn, towers and victory are untouched.
- The HUD shows neutral units already. Wells' states and labels join it.

## Amendments to earlier records

- **ADR-009:** neutral units are a hostility category (§1), and there are two new status kinds and the `Presence` delivery.
- **ADR-011 §10, §11:** the Flux Well source; wildlife and Well rewards.
- **ADR-013:** the jungle seat and Wells (§7).

## Open items

- Playtest the camp, trait and Well values.
- Gloomwing's tracking trait waits for Vision.
- Flux Spells are M10b.
