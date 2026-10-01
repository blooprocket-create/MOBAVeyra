# ADR-031: Stances, Focus, rank shapes, shadows and cross-discipline marks, for Angeru

**Status:** Proposed. The author's standing instruction (2026-09-28) is to keep working unreviewed toward a viable game, choosing a provisional answer where canon is silent. §12 lists every such answer for the author to overturn. This ADR becomes Accepted when the author merges the pull request that adds it.
**Date:** 2026-10-01
**Related:**
- [Initial Roster Character Bible](../Design/Veyra_Initial_Roster_Character_Bible_v0.6.md) §15: Angeru, including the skill-point ruling of 2026-09-25.
- [Economy & Progression Bible](../Design/Veyra_Economy_Progression_Bible_v0.1.md) §1: ranks, and Angeru's documented exception.
- [Combat Bible](../Design/Veyra_Combat_Bible_v0.5.md):
  - §9: dashes and blinks;
  - §16: On Ability Hit;
  - §18: takedowns;
  - §27: resources;
  - §32: owned entities;
  - §56: replacement sets keep their own cooldowns.
- [Ride State Open Questions](../Design/Ride_State_Open_Questions_v0.1.md): the ride's mounted set follows Angeru's stance precedent.
- [ADR-018](ADR-018-kit-primitives.md): slot overrides, recast windows and replacement sets.
- [ADR-030](ADR-030-stealth-markers-and-marked-follow-ups.md): placed markers, blinks, cooldown refunds, dashes through a target.

## Context

Angeru is an extreme-difficulty physical assassin with two stances.
- **Veil:** Shadow Needle, False Body, Black Step.
- **Blade:** Flowing Cut, Severing Arc, Passing Step.
- **R:** Forsake the Schools switches stance. It is available from Level 1, costs no skill point and never ranks.
- **Ranks:** each skill point in Q, W or E ranks both stances' abilities in that slot, to rank 6.
- **Resource:** Focus, shared by both stances.

The two stances keep fully independent cooldowns. Veil abilities mark enemy Vanguards **Veiled**, and Blade abilities mark them **Drawn**. Each discipline spends the other's mark: a Blade hit on a Veiled enemy triggers **Execution**, and a Veil hit on a Drawn enemy triggers **Vanish**.

None of the following exists yet:
- a second resource family;
- per-Vanguard rank limits, or an ability learnt without a point;
- a stance;
- a marker placed at a point;
- a blink to a unit or to one's own marker;
- a projectile thrown by a marker;
- a cast during one's own dash;
- a per-target lockout;
- a cooldown reset on takedown;
- a passive that spends one ability's mark with another.

## Decision

### 1. Focus (Vanguards, UI)

- `EVeyraResourceFamily` gains `Focus`, and a Vanguard record may name it.
- Focus is spent, regenerates and is refunded through the same Resource attributes as Mana (Combat Bible §27). The family changes:
  - its data: Focus has no growth per level;
  - its presentation: a bar colour of its own on the HUD.
- No item grants a resource, so no item rule changes. When one does, the item decides whether it grants Focus.

### 2. Rank shapes (Economy)

- `Progression.json` gains named **rank shapes**: `{ basicAbilityMaxRank, ultimate: Ranked | Innate }`.
  - A shape must spend exactly the standard total of skill points: 3 × basic + ultimate ranks.
  - The first shape is `dual_stance`: Q, W and E to 6, and an innate R.
- A Vanguard record names at most one shape; with none, it ranks as standard.
- The progression component holds its unit's shape and replicates it.
  - Rank-ups check against the shape.
  - An innate ultimate starts at rank 1 and refuses every point.
  - Basic abilities keep no level gate; an innate ultimate needs none.
- Ability tuning accepts one value per rank for any rank count a shape uses (1, 3, 5 and 6 here).
- The HUD shows each slot's pips from the shape; an innate slot shows none. Bots rank through the component's check.

### 3. Stances (Abilities)

- A slot's **own ability** may now be swapped. The loadout **stows** the present one, keeping its grant, and the slot takes another.
  - A stowed ability keeps its slot, so it shares that slot's rank (§2).
  - It keeps its cooldown, which is held by its ability ID. So the two sets cool down independently, as the bible requires.
  - A stowed ability whose cast is still running finishes that cast.
- **A follow-up belongs to the own ability whose cast opened it.**
  - While its slot holds another own ability, it is hidden. Its window keeps running, and it returns when its ability does.
  - So False Body's swap is unavailable in Blade Stance and returns in Veil Stance while the shadow stands.
  - A follow-up that shares a cooldown shares its own ability's.
  - Replacement sets that hold a slot whatever its own ability, such as variants and a ride's mounted set, are unchanged.
- **The `stance` archetype** names the slots it swaps and their abilities.
  - Casting it installs those abilities; casting it again puts back the stowed ones.
  - It is a self-cast with a cast tuning like any other. Its cooldown is short, and §10 shortens it.
  - Angeru's own record lists his Veil Stance; Forsake the Schools lists the Blade one.

### 4. A marker placed at a point (Abilities)

- **The `placement` archetype** places one of ADR-030's markers at a point within its cast range, on the navigable ground nearest that point.
  - It uses the same marker tuning as a self-buff's marker: lifetime, hits to destroy, look, burst.
  - A second placement replaces the first.
- Its recast window works as any other (ADR-018 §1), with one addition: **a follow-up opened by a placement ends when its marker ends.**
- False Body is a `Plain` marker no one can target, which stands for its lifetime. Its follow-up is the swap.

### 5. Blink to a unit or to one's own marker (Abilities)

- **The `blink` archetype** moves its caster with ADR-030's blink verb. The target depends on `to`:

  | `to` | Where the caster goes |
  |---|---|
  | `EnemyUnit` | beside an enemy unit of the listed kinds |
  | `OwnMarker` | to the caster's standing marker from a named ability |
  | `EnemyUnitOrOwnMarker` | either, chosen by the unit the cast names |

- **Range:** the cast range limits the unit or marker it names. A cast range of 0 lets an `OwnMarker` blink reach its marker at any distance.
- **Effects** land on an enemy it blinks to. A blink to one's own marker hits nothing.
- With `swap`, the marker takes the caster's old place: the two exchange positions.
- **Offensive:** a blink that may name an enemy is offensive, so it ends stealth (ADR-030 §1). One only to its own marker is not.
- **Shadow casts:**
  - False Body's swap is an `OwnMarker` blink with `swap`.
  - Black Step is an `EnemyUnitOrOwnMarker` blink.

### 6. A marker that throws too (Abilities)

- A skillshot may name a `mimic`: a marker ability, and the effects of a repeat hit.
- While the caster's marker from that ability stands, each cast also throws the same projectile from the marker toward the cast's point.
- The two projectiles share one record of the units they strike. A unit struck by both takes the full effects from the first and only the repeat effects from the second, so it never takes two full copies.
- A unit only the marker's projectile strikes takes the full effects. The shadow is an angle, not a weaker copy.

### 7. Casting during one's own dash (Abilities, Combat)

- An ability that moves its caster is now refused (`Busy`) while its caster dashes. Before, the dash was paid for and then went nowhere.
- A dash marked `takesOverDash`, and every blink, is the exception: it takes over from where the caster is.
  - The dash under way ends with the new reason `Replaced`. Like an interrupted dash, it lands none of its end zones.
  - The new dash, or the blink, then begins.
- So Flowing Cut may be cast during Passing Step, whose damage and mark already landed as it set off (ADR-030 §6).

### 8. A per-target lockout (Abilities)

- A cast may name `targetMustNotHold`: a status. It refuses a target holding that status from its caster, the inverse of `targetMustHold` (ADR-030 §7).
- Passing Step applies a lockout status to each unit it passes through and refuses a unit holding it. Each target is locked out on its own.

### 9. Cooldown reset on takedown (Abilities, Combat)

- A cast may name `takedownRefund`: the fraction of its remaining cooldown refunded when its caster takes part in a takedown. 1 is a full reset.
- Takedown participants are found once, in Combat: the credited killer and the assisters of an enemy Vanguard's death (Combat Bible §18). The status extension (ADR-009 §1) and this refund both read them.
- Abilities subscribes to deaths and refunds every ability in the participant's loadout that asks for it: its own, its stowed ones and its follow-ups.

### 10. The passive: two disciplines' marks (Vanguards)

**The `disciplines` passive map** lists marks, per-ability bonuses, and a slot whose cooldown each spending shortens.

Each mark names:
- **its status.** The other discipline's abilities apply it through their own effects, on enemy Vanguards only.
- **the abilities that spend it.**
- **an extra strike:** damage type, amount, amount per level, Physical Power ratio, and the fraction of Armor it ignores.
- **a resource refund:** a fraction of the spending ability's cost.
- **statuses for the caster.**

When one of the listed abilities hits an enemy Vanguard holding the mark from the passive's owner (On Ability Hit, §16):
1. The mark is removed.
2. The extra strike is dealt, scaled by that ability's bonus damage multiplier, if any.
3. The refund and the caster statuses follow.
4. That ability's bonus cooldown refund applies, if any.
5. The named slot's remaining cooldown shortens by the stated seconds.

For Angeru:

| Mark | Applied by | Spent by | Payoff |
|---|---|---|---|
| **Veiled** | Shadow Needle, Black Step | Flowing Cut, Severing Arc, Passing Step | **Execution:** extra physical damage that partly ignores Armor; Severing Arc doubles it |
| **Drawn** | Flowing Cut, Severing Arc, Passing Step | Shadow Needle, Black Step | **Vanish:** half the Focus cost back and a burst of Movement Speed; Black Step also gets half its cooldown back |

Spending either mark shortens Forsake the Schools' remaining cooldown.

### 11. Angeru's kit

| Slot | Veil Stance (own) | Blade Stance (`stance`) |
|---|---|---|
| Q | **Shadow Needle:** a piercing skillshot that marks Veiled; `mimic` from False Body | **Flowing Cut:** a short dash toward the point with a sweep where it lands; marks Drawn; `takesOverDash` |
| W | **False Body:** a `placement`; its follow-up is the swap | **Severing Arc:** a wide cone after a windup. Its outer band is a stronger zone, the sweet spot, by the area's innermost-first zones. Marks Drawn |
| E | **Black Step:** an `EnemyUnitOrOwnMarker` blink with a slash that marks Veiled; `takedownRefund` 1 | **Passing Step:** a `ThroughTarget` dash (ADR-030 §6) that marks Drawn and locks its target out |
| R | **Forsake the Schools:** the `stance` | |

His record uses Focus and the `dual_stance` rank shape.

## 12. Provisional answers where canon is open

1. **Focus numbers:**
   - 200 Focus, regenerating 10 per second, with no growth.
   - Costs: Shadow Needle 60, False Body 40, Black Step 40, Flowing Cut 30, Severing Arc 50, Passing Step 0, Forsake the Schools 0.
   - The bible's "weaving restores efficiency" is Vanish's refund of half the cost.
2. **Damage, cooldowns and ranges** are provisional; Tuning holds them. Forsake the Schools' cooldown is 3 s, shortened by 1 s for each mark spent.
3. **Execution:**
   - 30 + 8 per level physical damage, plus 25% of Physical Power, ignoring 30% of Armor.
   - It is dealt as an extra strike right after the triggering hit, rather than raising that hit, so the bonus and its Armor rule stay together.
4. **Vanish:** +30% Movement Speed for 1.5 s.
5. **Marks:** Veiled and Drawn last 4 s and land on enemy Vanguards only.
6. **False Body:**
   - It appears at once at its point rather than travelling.
   - It lasts 5 s and cannot be targeted.
   - The swap is used once.
7. **Black Step:**
   - It lands beside the enemy, on the caster's side of it.
   - It may name the shadow within its cast range.
   - Against Fluxborn and wildlife it slashes but marks nothing: marks are Vanguard terms.
8. **Passing Step:**
   - It deals its light damage to any enemy unit it passes through; only Vanguards become Drawn. The bible names only Vanguards; damaging others is the common practice for such dashes.
   - Its per-target lockout is 8 s, and its cooldown is 0.5 s.
9. **Flowing Cut cast during a dash** ends that dash where the caster is, then dashes, rather than waiting for the first dash to land.
10. **Shadow Needle:** a target struck by both needles takes 60% of the damage from the second, and is marked once.
11. **Stance:** the stance persists through death and respawn.
12. **The refusal of a dash during a dash** (§7) applies to every Vanguard. No kit relied on the old behaviour, which spent the cast for nothing.

## Consequences

- Abilities.json moves to schema v16: every cast gains `targetMustNotHold` and `takedownRefund`, and there are new maps (`stance`, `placement`, `blink`) and fields (skillshot `mimic`, dash `takesOverDash`).
- Vanguards.json moves to v15: `rankShape`, the `Focus` family and the `disciplines` map.
- Progression.json gains `rankShapes`.
- The loadout's swaps and stowed entries generalise a slot's own ability. Later stance-like kits (Combat Bible §56) can reuse them rather than add another rule.
- A dash during a dash is now refused. Bots that tried one get a rejection rather than a wasted cast.

## Tests

- **Rank shapes:** validation of the total; the innate ultimate at rank 1; rank 6 accepted, rank 7 and any R point refused.
- **Stances:**
  - a swap keeps both sets' cooldowns and ranks;
  - a running stowed cast finishes;
  - a follow-up is hidden in the other stance and returns.
- **Placement:** places within range; its follow-up ends with its marker.
- **Blink:**
  - to an enemy, with its effects;
  - to the shadow, and with a swap;
  - refused beyond range or without a marker.
- **Mimic:** both needles fly; a unit struck by both takes the repeat effects; one struck by the shadow's alone takes the full effects.
- **Dash take-over:** a plain dash is refused mid-dash; a take-over dash replaces it, with no end zones for the first.
- **Lockout and takedown refund:** a locked-out target is refused; a takedown resets Black Step.
- **Disciplines:** Execution on Veiled, amplified by Severing Arc; Vanish on Drawn; the stance cooldown shortens.
- **Angeru from the committed tuning:** both stances cast, and the follow-up swap works.
- **Packaged smokes:** Practice, CasualVictory, an eight-bot match with an Angeru bot, and the `angeru,torr` kit smoke.
