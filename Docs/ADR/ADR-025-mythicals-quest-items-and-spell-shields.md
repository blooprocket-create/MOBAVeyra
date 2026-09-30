# ADR-025: Mythicals, Quest Items, Spell Shields and the Item Bible's defensive catalog

**Status:** Proposed. The author's standing instruction (2026-09-28) is to keep working unreviewed toward a viable game, taking League of Legends' answer where canon is silent. §8 lists every such answer for the author to overturn. This ADR becomes Accepted when the author merges the M19 pull request that adds it.
**Date:** 2026-09-30
**Related:**
- [ADR-012](ADR-012-items-and-shop.md): items and the shop.
- [ADR-023](ADR-023-crit-and-the-full-item-catalog.md): crit, Attunement maps, and the dealt-damage event.
- [ADR-009](ADR-009-runtime-combat-primitives.md): statuses and the combat verbs.
- [Item Bible](../Design/Veyra_Item_Bible_v0.3.md): §2.5 (Quest Items), §4–§11, §13.
- [Combat Bible](../Design/Veyra_Combat_Bible_v0.5.md): §7 (Temporary Health), §19 (Spell Shields).
- [Architecture Constitution](../../ARCHITECTURE.md): §1.1, §1.3 and §1.5.

## Context

The Item Bible's 2026-09-30 revision adds 19 items to the 37 ADR-012 and ADR-023 built:
- Tier 1 components: Warforged Grip, Titansteel Grip, Marchplate, Shatterdeep Crystal.
- Tier 2 assemblies: Picket Plating, Canyonward, Breaker Aegis, Resonant Wardstone, Foundation Plate, Waymark Weave, Rescue Rig, Killstring Assembly.
- Masterworks: Riverhold Bastion, Blackreef Bell, Harborline Harness, Doombringer Bow.
- Quest Items: Flux Reclaimer, and Wayline Reservoir, its evolved form.
- Tier 4 Mythical: The Last Harbor.

It also adds three kinds of rule:
- **Tier 4 Mythicals** (§11): one per player per match, and buying one locks the rest; exactly two Attunements each.
- **Quest Items** (§2.5): bought normally, evolved for free by match objectives, one copy at a time, never farmed through the shop.
- **Design rules**: roster synergy, lore names, and outside games as inspiration only. These need no code, but this ADR and later ones describe mechanics in Veyra's terms, not as another game's items.

A survey of the code (2026-09-30) found:
1. **Armor and Magic Resist exist but items cannot grant them.** The attributes are in `UVeyraDefenceSet`, but `FVeyraItemStatsTuning`, `FVeyraEquipmentStats` and the equipment effect lack them. Ten of the new items need them.
2. **Validation allows Attunements only on Tier 3, one each, and every Tier ≥ 2 needs a recipe.** A Tier 4 with two Attunements and an evolution-only Tier 2 both fail.
3. **The shop buys a recipe's missing components for the player.** An evolution-only ingredient would be bought outright.
4. **No Spell Shield exists** (Combat §19). Hostile ability hits apply their damage and statuses at several sites: effect delivery, targeted damage, auras, lingering areas, tethers and displacement.
5. **Attunements see only the damage their holder deals.** None reacts to damage its holder takes.
6. **The dealt-damage event carries no crit.** Doom counts crits twice.

## Decision

### 1. Armor and Magic Resist are item stats

- `Items.json` (schema v3) gives every item `armor` and `magicResist` beside its other stats.
- `FVeyraEquipmentStats`, the equipment effect and `VeyraCombat::SetEquipmentStats` add them, as they add Health and Physical Power.

### 2. Tier 4 Mythicals

- **Tiers.** Tier 4 exists. Validation requires exactly two Attunements on a Tier 4 and one on a Tier 3, and none below.
- **One Mythical per player.** The inventory component holds `Mythical`: the content ID of the player's Mythical this match, replicated with the slots.
  - Buying or queuing a Tier 4 sets it.
  - Buying or queuing another Tier 4 is refused with `MythicalTaken`.
  - Undoing the purchase that set it releases it.
  - Selling it does not release it, so the player may buy the same Mythical again.
- **The shop screen** lists Tier 4 under **Mythicals**. The ones the player can no longer buy show as locked.

### 3. Quest Items

- **Category.** `category` gains `Quest`.
- **Quests.** `quests` maps a Quest Item's ID to its quest:
  - `objective`: `LaneFluxbornLastHits`, the only one today;
  - `threshold`;
  - `evolvesInto`: the Quest Item it becomes.
- **Evolution-only items.** An item a quest evolves into, such as Wayline Reservoir, is never sold. No field marks it: being some quest's `evolvesInto` is what makes it so, and the data cannot sell one by mistake.
  - It has no recipe, whatever its tier, and costs 0. Its Gold is its base form's, which `TotalCost` counts for a recipe built on it.
  - A recipe that needs it waits until the player holds it; the shop never buys it for them (`NotForSale`).
  - Flux Reclaimer is Tier 1 and Wayline Reservoir Tier 2, so The Last Harbor's recipe stays below Tier 4.
- **One quest line at a time.** An item evolved from a Quest Item belongs to the same line. Holding any item of a line refuses buying another.
- **Progress.** Progress lives on the holding slot (`FVeyraInventorySlot::QuestProgress`), replicated for the HUD.
  - A credited last hit on an enemy lane Fluxborn advances it, through Combat's `OnDeath` with the holder's unit as killer.
  - Reaching the threshold evolves the item in place. It keeps the Gold paid, so it resells at the base form's price. It ends the purchase's undo, as any benefit does.
  - Selling or undoing loses the progress. Nothing pays Gold on completion, so the shop cannot farm it.
- **Passives.** A Quest Item's passive, such as Residual Current, sits in its `attunement` list and is defined in an Attunement-kind map, like any Attunement. Validation allows at most one on a Quest Item.

### 4. Spell Shields (Combat §19)

- **The primitive.** Combat gains a **Spellward**: a status kind (`SpellShield`) that blocks the next hostile ability hit on its holder.
- **Checked at impact.** Every site that lands a hostile ability hit on a unit asks `VeyraCombat::BlockAbilityHit(Target, Source)` first:
  - effect delivery;
  - targeted damage;
  - lingering areas;
  - aura statuses;
  - tethers;
  - displacement.

  A blocked hit deals no damage, applies no crowd control or status, and raises no hit event. The Spellward is consumed.
- **What it ignores.** Basic attacks, Procs and damage over time already applied pass through, as §19 says.
- **Multi-hit abilities.** Only the first hit is blocked; later hits land.
- **Tests.** The Combat tests cover each impact site.

### 5. Attunements that react to damage taken

- `UVeyraAttunementSubsystem` also listens on the target's side of the dealt-damage event. Drag the Tempo and Quieting Chime use it.
- A per-holder "last enemy-Vanguard damage taken" time serves Quieting Chime and Residual Current.

### 6. Crit on the dealt-damage event

- `FVeyraDamageDealtEvent` gains `bCritical`, set from the attack plan of the basic attack that dealt it. Marked for Doom reads it.

### 7. The new mechanics

Numbers are prototype tuning in `Items.json`, provisional per §8.

- **Drag the Tempo** (Riverhold Bastion): an enemy Vanguard's basic attack that damages the holder slows the attacker's Attack Speed by `attackSpeedReduction` for `seconds`. It is refreshed, never stacked, and applied under the Attunement's ID.
- **Quieting Chime** (Blackreef Bell): the holder has a Spellward whenever `reformSeconds` have passed without enemy-Vanguard damage taken.
- **Safe Harbor** (Harborline Harness; The Last Harbor's first Attunement):
  - Banking: `reserveFraction` of post-mitigation damage the holder deals to enemy Vanguards, except Procs, banks as Reserve, up to `capMaxHealthFraction` of Max Health.
  - Converting: out of Vanguard combat (the Combat State off), Reserve converts to Health at `conversionMaxHealthFractionPerSecond`.
- **Marked for Doom** (Doombringer Bow):
  - Doom is held per (holder, target).
  - A basic attack on an enemy Vanguard adds `doomPerHit`, or `doomPerCrit` on a crit.
  - At `doomedAt` the target is Doomed; Doom expires `expirySeconds` after it was last added.
  - The holder's next basic attack on a Doomed target consumes it and deals `missingHealthRatio` of the target's missing Health as a Physical Proc. There is no cooldown.
- **Reclamation** (Flux Reclaimer): lane Fluxborn last hits advance the quest to `threshold`, then it evolves into Wayline Reservoir.
- **Residual Current** (Wayline Reservoir):
  - Each lane Fluxborn last hit stores `currentPerLastHit` Current, up to `currentCap`.
  - After `quietSeconds` without enemy-Vanguard damage taken, the holder spends `currentPerSecond`, while Current lasts, to multiply its Health Regeneration by `regenerationAmplification`.
  - Damage taken from an enemy Vanguard suspends this without clearing the stored Current.
- **High Tide** (The Last Harbor's second Attunement):
  - Current is gained as with Residual Current and spent while out of Vanguard combat.
  - While Current is being spent, regeneration is amplified and Safe Harbor's conversion speeds up by `reserveConversionAcceleration`.
  - Recovery beyond full Health, while both have energy left, becomes Temporary Health at `overflowToTemporaryHealth`, up to `temporaryHealthCapMaxHealthFraction` of Max Health (Combat §7).

### 8. League answers where canon is open (provisional)

1. **The Mythical lock** comes with the purchase and is released only by undoing it. Selling doesn't release it, and the same Mythical may be bought again.
2. **Flux Reclaimer and Wayline Reservoir are one quest line.**
3. **Quest progress is lost when the item is sold.** Completing the quest pays nothing.
4. **Reserve and Current survive death**, up to their caps.
5. **Doom** expires 5 s after it was last added. The consuming hit adds none. Its bonus uses missing Health after the hit.
6. **Spellward reforms** 40 s after the last enemy-Vanguard damage.
7. **Item damage (Procs) banks no Reserve.** Vanguard passives' cleaves are Procs too, so they don't bank either.
8. **Prices and stats sit on the catalog's scale.**
   - Components: Warforged Grip 875 (25 Physical Power), Titansteel Grip 1300 (40), Marchplate 300 (15 Armor), Shatterdeep Crystal 450 (25 Magic Resist).
   - Assemblies: 800 to 1300.
   - Masterworks: 2800 to 3200.
   - Flux Reclaimer: 450, evolving after 40 last hits.
   - The Last Harbor: about 4300 in total.
9. **Icons.** The 19 items show generated placeholder icons until the author's art arrives.
10. **Current is spent only while its holder is missing Health.** A holder at full Health keeps its stored Current for later.

## Consequences

- The catalog grows to 56 items. The shop gains a Mythicals group, locked Mythicals, and quest progress.
- Combat gains Spell Shields for every future source, not just Blackreef Bell.
- Items listens to both sides of the dealt-damage event, and to deaths for quests.
- Every item's text, icon and bot build must cover the new items. The existing tests enforce the text and icons.

## Tests

- **Catalog:** Armor and Magic Resist folded from items; validation of Tier 4, quests, `sale` and passives.
- **Inventory rules:** the Mythical lock (buy, queue, undo, sell, rebuy); the one quest line; evolution-only ingredients waiting.
- **Combat:** the Spellward blocks one ability hit at each impact site, never a basic attack, and only the first hit of a multi-hit ability.
- **Attunements:** each of §7's mechanics on a network test world.
- **Smokes:** Practice and CasualVictory still pass.
