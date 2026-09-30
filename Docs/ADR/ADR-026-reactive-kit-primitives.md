# ADR-026: Reactive kit primitives for Moro, Korruk and Mavra

**Status:** Proposed. The author's standing instruction (2026-09-28) is to keep working unreviewed toward a viable game, choosing a provisional answer where canon is silent. §7 lists every such answer for the author to overturn. This ADR becomes Accepted when the author merges the M20 pull request that adds it.
**Date:** 2026-09-30
**Related:**
- [ADR-008](ADR-008-vanguard-kits-and-abilities.md): Vanguard kits, abilities as data, passives as classes.
- [ADR-009](ADR-009-runtime-combat-primitives.md): statuses and the combat verbs.
- [ADR-018](ADR-018-kit-primitives.md): the kit primitives of M13, among them lingering areas.
- [Roster Bible](../Design/Veyra_Initial_Roster_Character_Bible_v0.6.md): §8 (Korruk), §12 (Moro), §17 (Mavra).
- [Combat Bible](../Design/Veyra_Combat_Bible_v0.5.md): §8 (crowd control), §14 (damage over time).
- [Architecture Constitution](../../ARCHITECTURE.md): §1.1, §1.3 and §1.5.

## Context

Ten of the roster's 25 Vanguards are playable. A survey on 2026-09-30 mapped the other 15 kits against the ability types that exist. Moro, Korruk and Mavra are the cheapest to add well, and they share what is missing:

1. **Reactions.** Korruk's Rupture consumes the Splinters and Fractured on its target for a burst. Mavra's Flash Cure roots, but stuns an Unstable target instead. No effect today depends on a status its target already holds.
2. **Stacks that turn into something.** At maximum Splinters a target becomes Fractured; at maximum Exposure, Unstable. No status changes into another at its maximum.
3. **Root.** Flash Cure roots. No status kind stops movement alone as crowd control.
4. **Areas that act while they last and as they end.** Pressure Leak's cloud knocks up whoever is still inside as it ends; CODE BLACK's vessels rupture in pulses, and it collapses inward at its end. A lingering area today only gives statuses by side.
5. **Jungle terrain.** Moro's Wild Dominion is stronger in the jungle and heals from wildlife. No rule says where the jungle is.

## Decision

### 1. Reactions: effects that depend on the target's statuses

An effect bundle gains `reactions`: each names a status and what its presence adds to the hit.
- `status`: the status the target must hold, from any source. A status ID belongs to one Vanguard's kit, and a side holds one of each Vanguard, so the ID is enough.
- `consume`: `Keep` or `Consume`. A consumed status is removed after the reaction resolves.
- `damage`: extra damage, `Once` or `PerStack` (times the stacks the target held).
- `statuses`: extra statuses for the target.
- `replaces`: statuses of the bundle's own list that the reaction takes the place of, as a stun takes the place of Flash Cure's root.

Reactions resolve in their order, against the statuses the target held as the hit landed. Effect delivery, lingering-area pulses and end effects (§4) carry them.

### 2. Stacks that turn into another status

A status in `Abilities.json` gains `atMaxStacks`: at most one status. When an application brings the status to its maximum stacks, it is removed and that status is applied in its place, from the same source.
- Splinters becomes Fractured; Exposure becomes Unstable.
- An Abilities-level listener on Combat's status-applied event does it, so every site that applies a status is covered. Combat keeps no knowledge of the Abilities catalog.
- A status also gains `landsOn`: the kinds of unit it lands on, empty for every kind. Splinters and Exposure land only on Vanguards, as the bible says they build "in enemy Vanguards". Combat's status spec carries the field, so `ApplyStatus` refuses such a status on any other unit wherever it comes from.
- A passive made wholly of such statuses and the reactions to them, as Korruk's Embedded, is an entry in a new `kitStatuses` passive map. It names its statuses, for its description and its checks, and runs nothing of its own.

### 3. Root

Combat gains `EVeyraStatusKind::Root`: the unit cannot move, nor cast an ability that moves it (a dash, a leap or an attach), and may attack and cast the rest. It is crowd control: Tenacity shortens it, and Unstoppable refuses it (Combat Bible §8). A dash or leap already under way is not stopped: only a displacement interrupts a dash (Combat Bible §9), and the Root holds the unit where the dash ends.

### 4. Lingering areas that act

A lingering area gains:
- `pulseEffects`, at most one bundle, dealt to the enemy units inside each pulse, reactions included. A pulse is a tick, so it passes a Spell Shield as a lingering area's statuses do (ADR-025 §4); the end effects are a hit the shield blocks.
- `endEffects`, at most one bundle, dealt to the enemy units inside as it ends, measured from its centre, so a displacement toward the origin pulls them in.
- `endWarningSeconds`: the presentation marks the area this long before its end effects land. A rupture must be readable (Roster Bible §17).

An area also gains `delayWithin`: while its point lies inside the caster's lingering area of a named ability, its delay is that entry's instead. Flash Cure reacts faster inside CODE BLACK.

### 5. Jungle terrain and Wild Dominion

- **Jungle terrain**, a World rule: ground inside the battleground that is neither lane (inside a lane's width of its path), river (inside the river's width of the diagonal) nor base. It is derived from the layout, so a new layout needs no extra authoring.
- **Wild Dominion** is a new passive archetype, a `wildDominion` map in `Vanguards.json`. While its holder stands on jungle terrain, it holds the passive's statuses, refreshed each check. Damage it deals to wildlife restores `healFraction` of that damage.

### 6. The three Vanguards

- **Moro** (magic, jungle):
  - Arc Bolt: a skillshot with a wildlife multiplier.
  - Bursting Ground: a delayed area, with a slow and a wildlife multiplier.
  - Pounce: a dash to a point, whose landing zone hits and gives Moro speed per enemy Vanguard hit.
  - Wildstorm: a free-moving channelled area around him, with speed.
- **Korruk** (physical burst):
  - Splinters stack up to Fractured.
  - Spineburst: a sector whose centre zone embeds two.
  - Pressure Mine: a delayed area that knocks up a Fractured target and detonates it.
  - Rupture: a low-damage pulse with a reaction per Splinter and a large one for Fractured.
  - Shatterfield: a large lingering area whose pulses embed Splinters and whose end detonates Fractured.
- **Mavra** (magic control):
  - Contaminated is a refreshed damage over time; Exposure stacks up to Unstable.
  - Caustic Line: a rectangle whose residue keeps refreshing Contaminated.
  - Flash Cure: a delayed root, a stun on Unstable.
  - Pressure Leak: a lingering cloud with an end knockup.
  - CODE BLACK: a large lingering area of pulses that stun Unstable targets, with Flash Cure faster inside, and an inward pull and knockdown at its end.

Every number is provisional tuning in `Abilities.json` and `Vanguards.json`.

## 7. Provisional answers where canon is open

1. **A consumed Unstable** starts Exposure again; Contaminated goes on (Roster Bible §17 says only that Contaminated stays).
2. **Rupture takes every Splinter it consumes as damage**, and Fractured as a larger burst. Only Rupture, Pressure Mine and Shatterfield's end consume; Q adds.
3. **Pressure Mine erupts after its delay only.** A proximity trigger needs a placed object (ADR-003), not built yet.
4. **Root stops dashes and leaps**; it does not stop attacks or other casts.
5. **Wild Dominion heals a share of the damage** dealt to wildlife, sustaining the jungler between camps.
6. **Jungle terrain** is everything between the lanes that is not river or base.
7. **Splinters and Exposure land only on enemy Vanguards**, as the bible's passives say. Contaminated, a damage over time, lands on every unit.
8. **A refreshed damage over time keeps its cadence**: its next tick comes when it would have, and only its duration starts again (Combat Bible §14 renews the duration and says nothing of the ticks). Before, a refresh restarted the ticks, so Contaminated, refreshed by residue and clouds, could stop ticking altogether.

## Consequences

- Reactions, stack conversion and acting areas are general. Silt, Torr and Angeru's kits need them too (survey, 2026-09-30).
- Root is a Combat status for every future source.
- The World gains its first terrain rule; the passive archetype is Moro's alone for now.

## Tests

- **Combat:** Root blocks movement and moving casts, is shortened by Tenacity, and is refused by Unstoppable.
- **Abilities:** reactions (once, per stack, consume, replace), conversion at maximum, pulse and end effects, and `delayWithin`.
- **World:** jungle terrain on a fixture layout.
- **Vanguards:** Wild Dominion on and off the jungle, and its heal; each Vanguard's kit on the archetype test world.
- **Smokes:** a bot match with the three new Vanguards.
