# ADR-027: Mobile attacks and ally-targeted casts, for Celandrine and Aurelisse

**Status:** Proposed. The author's standing instruction (2026-09-28) is to keep working unreviewed toward a viable game, taking League of Legends' answer where canon is silent. §9 lists every such answer for the author to overturn. This ADR becomes Accepted when the author merges the pull request that adds it.
**Date:** 2026-09-30
**Related:**
- [ADR-008](ADR-008-vanguard-definitions-and-ability-composition.md): kits as data, passives as classes.
- [ADR-009](ADR-009-runtime-combat-primitives.md): basic attacks, empowerment and secondary impacts.
- [ADR-018](ADR-018-kit-primitives.md): self-buff options, auras and lingering areas.
- [ADR-026](ADR-026-reactive-kit-primitives.md): the M20a primitives.
- [Roster Bible](../Design/Veyra_Initial_Roster_Character_Bible_v0.6.md): §22 (Celandrine), §24 (Aurelisse).
- [Combat Bible](../Design/Veyra_Combat_Bible_v0.5.md): §7 (shields), §17 (empowered attacks), §48 (attack timing).
- [Architecture Constitution](../../ARCHITECTURE.md): §1.1, §1.3 and §1.5.

## Context

M20a released Moro, Korruk and Mavra. The 2026-09-30 survey ranked Celandrine and Aurelisse next. A code survey the same day found what their kits need and the engine lacks:

1. **Attacking while moving.** Never Break Stride lets Celandrine move during her basic-attack windup at reduced speed; OPEN ROAD! removes the penalty. Today every move order cancels a windup (the Vanguard controller), and the basic attack component cannot tell that its attacker moves.
2. **Several empowered attacks.** Doubletime empowers the next three attacks and shortens their windups. An empowerment is one slot: a newer one replaces it, and the first attack spends it.
3. **Piercing while a buff lasts.** OPEN ROAD!'s attacks pierce a line behind their primary target. A secondary impact comes only from an empowerment or a passive's attack hook, never from a buff for its duration.
4. **Ally-targeted casts.** Windward shields Aurelisse or one allied Vanguard, and ROOM TO BREATHE centres on herself or a nearby ally. No archetype accepts an allied unit as its target: every `CheckTarget` asks `CheckEnemyTarget`, the self-buff ignores its target, and the cast events do not carry one.
5. **A shield that rewards what it absorbed.** Windward's recipient gains a second speed burst once the shield absorbs a meaningful amount. Combat reports what each provider's shields absorbed, but not which shield, and no event marks a shield's end.
6. **A delayed area that lingers.** Rising Current knocks up after a warning, then leaves an updraft. Only an area that hits at once may linger.
7. **Currents.** Slipstream, the updraft and ROOM TO BREATHE's field speed allies moving in a direction.

## Decision

### 1. Mobile attacks

- Combat gains the status kind `MobileAttack`. Its magnitude is the share of Movement Speed a unit keeps while a basic attack winds up, in (0, 1]; the strongest applies.
- A unit's basic attack component also has a base share, 0 unless a passive sets it (`SetWindupMovement`). The share in force is the larger of the two.
- With a share above 0, a move order does not cancel a windup; the unit walks at that share of its speed until Commit. The target must still be in range at Commit (Combat §48), so walking out of range ends the attack at no cost, as a cancelled windup does.
- The movement component takes the share as a speed scale that the attack component sets when a windup starts and clears when it ends. Combat keeps no knowledge of attacks.

### 2. Empowered attacks with charges

`EmpoweredAttack` gains:
- `attacks`: how many basic attacks it empowers, at least 1. Each attack spends one, and the empowerment ends with the last or with its time.
- `windupScale`: what the empowered attacks' windups are multiplied by, in (0, 1]. It shortens the windup only: the interval between attacks is unchanged, so there is no attack-timer bypass (Roster Bible §22).

### 3. Attack impacts while a buff lasts

A self-buff gains `attackSecondaryImpact`, at most one: a secondary impact and its `seconds`, normally the buff's statuses' duration. For that long, each of its caster's basic attacks offers the impact, with its priority, as an empowerment does. Its damage is read from the caster's power at the cast. It stays Proc damage: no Crit, On-Hit or item procs (ADR-009 §5). Death ends it, as it ends a waiting empowerment.

### 4. Ally-targeted self-buffs

- A self-buff gains `recipient`: `Caster`, the default, or `CasterOrAlly`.
- With `CasterOrAlly`, a cast that names an allied, living Vanguard within cast range gives it the buff: statuses, shield, Temporary Health, heal and aura, the aura following it. A cast that names nothing, or the caster, gives the buff to the caster.
- Targeting gains `CheckAllyTarget` and the rejection `NotAllied`.
- `FVeyraCastEvent` gains the cast's target actor, so passives such as Slipstream can see whom a cast named.

### 5. Shields that reward what they absorbed

- A shield's tuning gains `absorbedReward`, at most one: a `fraction` of the granted amount and `statuses`. Once that shield has absorbed at least that fraction, its holder gains the statuses once, from its provider.
- Combat's `FVeyraShieldShare` gains the shield's `Id`, so an Abilities listener can sum what one grant absorbed. A new grant with the same Id starts the count again.

### 6. Delayed areas that linger

- A delayed area may linger. The lingering area is prepared at Commit, as its effects are (Combat §50), and armed when the delayed area resolves, at its placement.
- The lingering area's preparation moves from the area ability to `VeyraAreaDelivery`, so the ability and the delayed area share it.

### 7. Currents

Currents ship as undirected Movement Speed for allies inside a shape (§9.4):
- **Slipstream** is a passive: on each of its owner's ally-targeted casts, it arms a lingering rectangle from her to the ally that gives allied Vanguards inside a MoveSpeed status.
- **Rising Current's updraft** and **ROOM TO BREATHE's field** give their allies' statuses as lingering areas and auras already do.

### 8. The two Vanguards

- **Celandrine** (physical, ranged). Never Break Stride is an `attackStride` passive: its windup share, and a status for each primary basic attack that hits an enemy Vanguard. Doubletime empowers three attacks. Briar Scatter is an area whose patch lingers with a slow. Sidebound is a short dash. OPEN ROAD! is a self-buff with speed, a full-speed `MobileAttack` and a piercing secondary impact.
- **Aurelisse** (magic, enchanter). Slipstream is described in §7. Crosswind is a piercing skillshot that pushes enemies aside from its path. Rising Current is a delayed knockup that lingers as an updraft. Windward is a `CasterOrAlly` buff with a shield whose absorbed reward is a speed burst. ROOM TO BREATHE is a `CasterOrAlly` buff whose zone, cast at the recipient, pushes enemies outward once, and whose aura follows the recipient with a shield and speed.

Every number is provisional tuning.

## 9. League answers where canon is open (provisional)

1. **Walking out of range during a mobile windup ends the attack at no cost**, as a cancelled windup does. The bible keeps range and hit validation.
2. **Doubletime shortens the windup, not the interval**, as the bible's "no attack-timer bypass" requires; League's attack resets are not used.
3. **Windward's "meaningful amount" is half the shield**, as League's shields that reward absorption count a share of their value.
4. **Currents are undirected speed** for now. League's movement zones, such as Janna's tailwind or Ivern's brush, buff whoever is inside; direction is left to a later presentation pass.
5. **ROOM TO BREATHE pushes once** from the recipient and never again, as the bible says, and the field's shield is limited by the aura's refresh: a status-carried shield is not re-granted while one from it remains.

## Consequences

- Mobile attacks are a Combat status any later kit can grant.
- Several abilities in the backlog want ally targeting: Neris and Sylra's heals, and Cairn's future guard.
- The shield Id on `FVeyraShieldShare` also lets statistics credit individual shields.

## Tests

- **Combat:** `MobileAttack` keeps a windup through a move order at its share of speed, and the strongest applies.
- **Abilities:** charges and windup scale; a buff's secondary impact while it lasts; `CasterOrAlly` recipients and refusals; the absorbed reward; a delayed area's linger.
- **Vanguards:** Never Break Stride and Slipstream; each kit on the archetype test world.
- **Smokes:** a bot match with both.
