# ADR-051: Veil Needle, its four Masterworks and Memoryglass Reliquary

**Status:** Proposed. The author's standing instruction (2026-09-28) is to keep working unreviewed toward a viable game, choosing a provisional answer where canon is silent. §9 lists every such answer for the author to overturn. This ADR becomes Accepted when the author merges the pull request that adds it.
**Date:** 2026-10-02
**Related:**
- [Item Bible](../Design/Veyra_Item_Bible_v0.3.md):
  - §7: Veil Needle.
  - §8: Blank Sigil, Cutline Mantle, Oathpiercer and Witnessless Edge.
  - §9: Memoryglass Reliquary.
- [ADR-023](ADR-023-crit-and-the-full-item-catalog.md): Attunements and the dealt-damage event.
- [ADR-025](ADR-025-mythicals-quest-items-and-spell-shields.md): holder-and-target Attunement state, as Doom keeps it.
- [ADR-017](ADR-017-match-statistics.md): damage resolution and whose shields took it.

## Context

Six Item Bible items are not in the catalog:
- **Veil Needle**, a Tier 2 Physical Power and flat Physical Penetration assembly.
- **Four physical burst Masterworks built on it:**
  - **Blank Sigil (No Allegiance):** a setup, then a *different* payoff action on the same Vanguard.
  - **Cutline Mantle (Clean Break):** a takedown grants decaying Movement Speed and a shield sized by the damage dealt.
  - **Oathpiercer (Through the Guard):** personally breaking an ally-granted shield detonates part of what it absorbed.
  - **Witnessless Edge (No One Coming):** an isolated Vanguard is marked, a damage threshold locks the mark, and the next hit pays off.
- **Memoryglass Reliquary (Reenactment),** a Mage Masterwork: after a quiet period, the first ability wound on a Vanguard is remembered, and repositioning before hitting them again replays part of it as Magic Damage.

**What exists today:**
- Equipment stats carry flat Magic Penetration but no flat Physical Penetration.
- The Attunement subsystem sees each dealt damage instance, each death and each cast commit.
- Damage resolution reports which provider's shield absorbed what, but not whether the hit broke it.

## Decision

### 1. Flat Physical Penetration is an item stat

`FVeyraEquipmentStats` and `Items.json` stats gain `physicalPenetrationFlat`, which feeds the offence attribute of the same name (Combat Bible §3). The shop lists it as Physical Penetration. `Items.json` moves to schema v5, and every item states the stat.

### 2. Combat says when a hit breaks a shield

`FVeyraShieldShare` gains `bBroken`: the share emptied that shield. Damage resolution carries it, so Attunements read it without asking the absorption ledger.

### 3. The Attunements

All five live in `UVeyraAttunementSubsystem` and act only against **enemy Vanguards**, with every number in `Items.json`. As with every Attunement, a hit by the holder's companion or Echo sets off none of them (ADR-023).

- **No Allegiance (Blank Sigil):**
  - **What counts as an action:** a basic attack, or an ability hit, identified by the holder's latest committed cast. Procs and effects over time are not actions.
  - **Opening:** after `quietSeconds` without damaging an enemy Vanguard, the holder's next action against one opens it for `openingSeconds`.
  - **Payoff:** the holder's next *different* action against that Vanguard consumes the Opening. It deals a bonus Physical hit (Proc) of `bonus.base + bonus.physicalPowerRatio × Physical Power`, which carries `penetration` extra flat Physical Penetration.
- **Clean Break (Cutline Mantle):**
  - **The tally:** the subsystem counts, for each holder and enemy Vanguard, the damage the holder dealt within the last `windowSeconds`.
  - **The trigger:** when that Vanguard dies within `windowSeconds` of the holder's last hit on them, the holder gains the `speed` status and a shield of `shieldShare × tally`, capped at `shieldCap`, lasting `shieldSeconds`.
  - **The speed** is a stacking status whose stacks decay one at a time.
  - **Refresh, not stack:** a later takedown refreshes the speed and replaces the shield, which keeps one identity.
- **Through the Guard (Oathpiercer):**
  - **Branding:** when the holder's hit is absorbed by an enemy Vanguard's shield whose provider is another unit, that shield, by provider and identity, is branded for `brandSeconds`. A self-granted shield never qualifies.
  - **Breach:** while it is branded, `breachShare` of what the holder's hits make it absorb is recorded.
  - **Detonation:** if one of the holder's hits breaks the branded shield in time, the Breach detonates as a bonus Physical hit (Proc) of `breachConversion × Breach`.
  - **Loss:** an expiry, another source's break, or the brand running out loses the Breach.
- **No One Coming (Witnessless Edge):**
  - **The mark:** when the holder damages an enemy Vanguard with no allied Vanguard of its own within `protectionRadius`, it is marked Abandoned for `markSeconds`. The holder gains the `speed` status, Movement Speed toward enemy Vanguards, while the mark lasts.
  - **Lock-in:** if the holder deals `lockDamage` to it before an ally enters the radius, the mark locks in for `lockSeconds`. The holder's next hit on it then deals a bonus Physical hit (Proc) of `bonus.base + bonus.physicalPowerRatio × Physical Power`.
  - **Breaking:** an ally entering the radius before the lock breaks the mark. Proximity is checked on the Tempered by Conflict timer.
- **Reenactment (Memoryglass Reliquary):**
  - **Remembering:** after `quietSeconds` without damaging an enemy Vanguard, the holder's first ability hit on one is remembered: the target, its post-mitigation damage, and where the holder stood.
  - **Replaying:** within `memorySeconds`, the holder's next damaging hit on that target, at least `displacement` from the remembered spot, replays `replayShare` of the remembered damage as Magic Damage (Proc). The memory is then spent.
  - **Wound only:** the replay copies only the wound, none of the hit's other effects (Item Bible §9).

### 4. The items

| Item | Tier | Recipe | Stats |
|---|---|---|---|
| Veil Needle | 2 | Iron Grip + 350 | 20 Physical Power, 8 Physical Penetration |
| Blank Sigil | 3 | Veil Needle + Striker Assembly + Warforged Grip + 750 | 60 Physical Power, 15 Physical Penetration, 15 Ability Haste |
| Cutline Mantle | 3 | Veil Needle + War Harness + Timing Coil + 750 | 45 Physical Power, 12 Physical Penetration, 250 Health, 10 Ability Haste |
| Oathpiercer | 3 | Veil Needle + War Harness + Titansteel Grip + 750 | 70 Physical Power, 12 Physical Penetration, 200 Health |
| Witnessless Edge | 3 | Veil Needle + Striker Assembly + Quickcoil + 750 | 50 Physical Power, 12 Physical Penetration, 10 Ability Haste, 0.15 Attack Speed |
| Memoryglass Reliquary | 3 | Nullglass Shard + Catalyst Coil + Arc Crystal + 750 | 80 Magic Power, 12 Magic Penetration, 15 Ability Haste |

Each gets its text and a placeholder icon until the author's art replaces it.

### 5. Tests

- **Items:**
  - each Attunement's trigger and its refusals (not an enemy Vanguard, not in time, the same action, a self shield, an ally arriving, too little repositioning);
  - its payoff's amount;
  - the new stat in the holder's offence;
  - the catalog, its recipes and its validation.
- **UI:** the shop's stat line, item text and icons.

## 9. Provisional answers where canon is open

1. **An action** is a basic attack, or an ability hit attributed to the holder's latest committed cast. Procs and effects over time neither open nor consume an Opening.
2. **No Allegiance's extra penetration** rides on its bonus hit, not on the action's own damage, which has already resolved by the time the Opening is consumed. The bonus hit carries `penetration` extra flat Physical Penetration.
3. **No One Coming's Movement Speed** applies toward any enemy Vanguard while the mark lasts. Combat's existing toward-enemy speed has no single-target form, and the mark is short.
4. **Reenactment's remembered spot** is where the holder stood when the wound landed, not when the ability was cast: an ability hit carries no cast position.
5. **The values** are in `Items.json`:
   - No Allegiance: quiet 4 s, Opening 3 s, bonus 30 + 0.3 × Physical Power, penetration 10.
   - Clean Break: window 3 s; speed 4 stacks of 10%, each decaying after 0.5 s; shield 25% of the tally, at most 250, for 3 s.
   - Through the Guard: brand 3 s, Breach share 50%, conversion 1.
   - No One Coming: radius 1000, mark 4 s, speed 15%, lock at 200 damage for 2 s, bonus 30 + 0.3 × Physical Power.
   - Reenactment: quiet 5 s, memory 4 s, displacement 300, replay 40%.

## Out of scope

- The author's icons for these six items.
- Bots buying them: their builds are data, and these are left out for now.
