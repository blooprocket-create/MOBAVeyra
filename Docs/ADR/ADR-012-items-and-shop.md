# ADR-012: Items and the shop: inventory, the purchase queue, equipment stats and Recall

**Status:** Proposed. The author's standing instruction (2026-09-28) is to keep working unreviewed toward a viable game, taking League of Legends' answer where canon is silent; §9 lists every such answer for the author to overturn. It becomes Accepted when the author merges the M8 pull request that adds it.  
**Date:** 2026-09-28  
**Related:** [ADR-006](ADR-006-unreal-project-scaffold.md) (§3 modules and layers, §4 GAS placement), [ADR-008](ADR-008-vanguard-definitions-and-ability-composition.md) (content as data, archetype maps, provenance; open item "Ability Haste as a stat, and item-driven modifiers"), [ADR-009](ADR-009-runtime-combat-primitives.md) (statuses, cooldowns), [ADR-011](ADR-011-battleground-runtime.md) (§11 Gold, fountain recovery; open item "Recall, the shop and buyback"), [Item Bible](../Design/Veyra_Item_Bible_v0.3.md), [Economy & Progression Bible](../Design/Veyra_Economy_Progression_Bible_v0.1.md) §10–§12, §16, [Combat Bible](../Design/Veyra_Combat_Bible_v0.5.md) §5, §6, §21, §33, §41, [Architecture Constitution](../../ARCHITECTURE.md) §1.1, §1.3, §1.5, [Project Structure](../../PROJECT_STRUCTURE.md) §2 (VeyraItems, VeyraEconomy).

## Context

After M7, Gold accrues but buys nothing. The Item Bible defines the first catalog and its tier rules; Economy §10–§12 defines the fountain shop, the remote purchase queue, delivery, selling and undo, and §16 splits ownership between Economy (Gold, purchase accounting, pending transactions, refunds) and Items (item instances, recipes, slots, activation, atomic delivery). Canon sets no prices or stat values (Item §3: "prices are data").

Several facts in the code shape the design:

1. **Items have no layer they can live in.** `ModuleLayers.json` notes "Items later" in the Abilities layer, but an item Active runs as an ability archetype, and a module may not depend on its own layer — the problem ADR-011 §2 solved for World and Flux.
2. **Ability Haste does not exist.** The cooldown component keeps each cooldown's starting duration so Ability Haste can rescale it later; nothing grants it.
3. **Percentage modifiers multiply** (Combat §41, `VeyraAttributePolicy.h`). Stacked Attack Speed items would compound, not add as League's do.
4. **Economy cannot see items**, yet validating the pending queue needs recipe and slot simulation.
5. **The fountain is Match's.** Match owns the side's start and the fountain-recovery radius (`Match.json`); Items sits below Match.

## Decision

### 1. Scope (M8)

- The shop and inventory: six slots; the Item Bible's T1 components (not Keensteel), Swift and War Boots, twelve T2 assemblies, four T3 Masterworks whose Attunements need no new combat hook (Weight of War, Overcharge, Spool Up, Overcycle), and Field Tonic.
- Buying, the remote queue, cancellation and revalidation, delivery at the fountain and on death, selling and undo.
- Item use by inventory slot (author ruling, 2026-09-28): keys 1–6 use the item in slots 1–6, as in League. A consumable is used up (Field Tonic); an item with an Active casts it (Razorwheel's Cleave) through Abilities by the ability's ID, not through the Q/W/E/R slots. An item Active's cooldown belongs to the item and uses Item Haste, never Ability Haste (Combat §21).
  - **Amendment (2026-09-29, M11; ADR-016 §6):** the default keys become League's 1 2 3 5 6 7, as the vision tool takes 4. The slots are unchanged, and every key stays rebindable. ADR-016 §11 lists this among the League answers for the author to overturn.
- Recall.
- **Deferred:** crit and its items; Arcane Boots (its amplification model is open); Flux Flask (needs Flux Wells); the remaining Attunements; Tier 4; buyback; vision-tool and Flux Spell swaps; Item Haste; Lifesteal and Omnivamp (they need Combat §6's healing categories).

### 2. The Items layer (amends ADR-006 §3 and Project Structure §2)

A new **Items** layer holds **VeyraItems**, directly above Abilities and below Battleground: Foundation → Rules → Economy → Abilities → **Items** → Battleground → Content → Orchestration → Services → Presentation. VeyraItems depends on Core, Combat, Economy and Abilities; nothing below Match depends on it.

### 3. Item definitions are data (ADR-008 pattern)

- `Game/Tuning/Items.json` with its schema: one record per item keyed by content ID — tier, category (component, boots, assembly, masterwork, consumable), price or recipe (components and completion cost), stats, an optional Active (a content ID in `Abilities.json`'s archetype maps, checked by `check_tuning.py`), an optional Attunement (a content ID in an Attunement map, as Vanguard passives are, ADR-008 §5), a stack limit, and resale rules.
- `VeyraItems::Validate` checks what the schema cannot: T1 has no recipe or Attunement; T2 has no Attunement; T3 has exactly one; Boots stop at T2; recipes are acyclic and name defined items; every total cost is positive.
- Every value is Provisional data; display names and descriptions live in `Game/Text/VeyraText.csv`.

### 4. Gold transactions stay in Economy

`UVeyraGoldComponent` gains `Spend(Amount, Reason)`, which returns a transaction ID or refuses when the balance is short (no debt, Economy §11.1), and `Refund(Id, Fraction)`. It keeps the owner-replicated list of open purchase transactions, so purchase accounting, pending refunds and their audit log are Economy's (§16). Economy never names an item: Items tells it how much and why.

### 5. The inventory and queue rules are Items' pure functions

`VeyraInventoryRules` simulates the six slots plus the pending results (§11.1): recipe consumption with reservation, no component consumed twice, uniqueness (§9.2), stack limits, cancellation with dependents, revalidation after any change, resale at the data's fraction of the present form's total cost, and undo while at the fountain and unused (§12). `UVeyraInventoryComponent` (on the PlayerState, so it survives death) holds the slots, replicated to everyone as League's scoreboard shows them, and the pending queue, replicated to its owner. `UVeyraShopSubsystem` is the server's transaction owner: it applies each outcome atomically — Gold through Economy, slots through the component.

### 6. Equipment stats through one Combat verb

`VeyraCombat::SetEquipmentStats(ASC, FVeyraEquipmentStats)` replaces one infinite native effect with the inventory's summed flat stats and percentage factors on every change, as `SetUnitScaling` does (ADR-011 §10); Max Health keeps its percentage (§41). Combat gains:

- **Ability Haste** (a new attribute, floored at 0, Combat §39): a cooldown lasts base × 100 / (100 + Ability Haste) (§21); gaining or losing Haste while a cooldown runs rescales its remaining time proportionally (§21), so the cooldown ledger rescales its running entries whenever the attribute changes.
- **Bonus Attack Speed** adds to level growth rather than compounding: an item's fraction becomes a flat Attack Speed modifier worth the Vanguard's base Attack Speed times that fraction, so base × (1 + growth) + base × bonus = base × (1 + growth + bonus). Statuses still multiply on top. This is §41's "unless explicitly stated otherwise", stated here, and matches League.

### 7. Match routes the fountain and forwards requests

Match's fountain check tracks each participant entering and leaving their own fountain (`Match.json` `fountain.radius`) and calls `UVeyraShopSubsystem::SetAtFountain`; the shop delivers on entry and ends undo on exit. Match's death handling asks the shop to deliver on death (§11.2). The player controller's server RPCs (buy, sell, undo, cancel, use slot) reach the shop through the game mode, which refuses them while paused or after the match ends (Match Flow §10.2).

### 8. Recall is a Match order

A channel on a world-time timer (`Match.json` `recall`), interrupted by a new move, attack or cast order (item Actives included), by hostile damage (Combat's hostile-damage event), by an interruption (a Stun or a displacement, Combat §9) or by death; on completion the living Vanguard is moved to its side's start. It is refused while dead, paused or ended, under crowd control that stops casting, and while another cast holds the Vanguard (its windup, channel or recovery), as a cast is. A channel sits in `UVeyraRecallComponent` on the PlayerState, which replicates its start and end for the HUD and watches Combat's events itself while it runs; the game mode starts it, ends it on each order the Vanguard takes, and moves the Vanguard home.

### 9. League answers where canon is silent (for the author to overturn)

1. "At the fountain" is the fountain-recovery zone; a purchase there is delivered at once.
2. Each T3 is unique per player; a player owns at most one pair of Boots; T1 and T2 may repeat.
3. Field Tonic stacks to 5 in a slot and is not interrupted by damage.
4. Bonus Attack Speed adds (§6).
5. Recall channels 8 s and is interrupted as in §8.
6. Undo ends on leaving the fountain or at the item's first benefit (canon, restated).
7. No Armor or Magic Resist items in M8: Item §4 adds components only as recipes need them.
8. Crit is deferred with its items.
9. A dead Vanguard shops as if at its fountain: purchases are delivered at once and give nothing until respawn, and selling and undo work (Economy §10: equipment bought while dead "is assigned at the fountain").
10. A recipe buys its missing components as part of the purchase, and uses owned ones (recursively) where it can: the completion cost is always paid, as Item §1 requires.
11. Using a consumable does not interrupt Recall; every other order does.
12. P opens the shop and B recalls, League's default keys; Escape closes an open shop before it opens the menu.

### 10. Values are data

| Area | Owner | Values (all Provisional) |
|---|---|---|
| Catalog | `Items.json` | League-like: T1 250–450 Gold; T2 total 700–1300; T3 total 2600–3200; Field Tonic 50 for 120 Health over 15 s, stack 5 |
| Resale | `Items.json` | 70% of the present form's total cost (Economy §12 prototype) |
| Recall | `Match.json` | Channel 8 s |
| Starting Gold | `Economy.json` | 500 (M7), sized for a T1 item or a component and Tonics (Economy §10) |

### 11. The shop screen is presentation

The shop is a UMG screen built in C++ in VeyraUI, beside the in-match menu (ADR-010 §4), and decides nothing. Its model reads the owner's replicated Gold and inventory and prices every item with `VeyraInventory::Quote`, the rule the server prices by, so it shows what the server will charge. So that it can offer selling and undo only when they work, the inventory replicates to its owner whether the shop is open to it (at the fountain, or dead) and how many purchases undo can take back. Each button asks through the player controller; the server checks the request again and the screen shows its refusal. The greybox HUD gains an item bar (keys 1–6, each slot's item, stack and Active cooldown, and the purchases waiting) and Recall's channel bar. Item names and descriptions live in the string table `Game/Text/VeyraText.csv`, and a test holds the catalog to it.

## Consequences

- Gold has a use, and Vanguards diverge by build as in League.
- One new module and layer; Combat gains two stats and a verb; Economy gains transactions; Match gains Recall and routing, not item logic.
- The queue rules are the riskiest part and are tested as pure functions, one test per canon sentence.

## Amendments to earlier records

- **ADR-006 §3:** the Items layer and VeyraItems.
- **ADR-008:** its open item "Ability Haste as a stat, and item-driven modifiers" is settled by §6.
- **ADR-011:** its open item "Recall, the shop and buyback" is settled except buyback.

## Open items

- The deferred Attunements, crit, Arcane Boots, Flux Flask, buyback, Item Haste, Lifesteal and Omnivamp.
- Whether inventories hide in fog once Vision exists.
- Final prices and stats (playtest).

## Alternatives considered

- **VeyraItems in the Abilities layer**, with Actives as Items-owned code: duplicates the archetypes Abilities already runs.
- **The pending queue in Economy**: Economy would need recipes and slots, which are Items' by §16.
- **Percentage Attack Speed compounding** (§41 default): diverges from League and makes Attack Speed stacking snowball.
