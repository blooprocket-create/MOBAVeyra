# ADR-056: Bot competence: consumables, base defence, buyback, grouping and builds

**Status:** Proposed. The author's standing instruction (2026-09-28) is to keep working unreviewed toward a viable game, choosing a provisional answer where canon is silent. §7 lists every such answer for the author to overturn. This ADR becomes Accepted when the author merges the pull request that adds it.
**Date:** 2026-10-02
**Related:**
- [ADR-013](ADR-013-ai-vanguards.md): bot brains. It sets the sense–decide–act rules and the players' order paths. §8.7 deferred consumables.
- [ADR-039](ADR-039-weekly-rotation-and-co-op-vs-ai.md): Co-op vs AI keeps Standard rules, buyback included.
- [ADR-051](ADR-051-assassin-items-and-memoryglass-reliquary.md): bots cast item Actives. It deferred bots buying the burst items.
- The Economy & Progression Bible §15 (buyback) and the Item Bible §12 (consumables and undo).

## Context

Bots play whole matches, but four habits that opponents in Co-op vs AI and custom matches rely on are missing:
- **Consumables:** bots never buy or drink them.
- **Defence:** a bot sees only its own lane's structures, so a siege on its inhibitors or its Prime Well goes unanswered.
- **Buyback:** bots never use it.
- **Late game:** every bot keeps to its lane or its jungle all match, so a side never pushes together.

Bot builds also skip the burst items. When a bot casts an item's Active, the purchase stays undoable, because only the player's path ends the undo.

## Decision

### 1. Consumables
- **Data:** `Bots.json` gains `consumables`:
  - the item;
  - how many each difficulty carries;
  - the Health fraction below which a bot drinks;
  - the match time after which none are bought.
- **Buying:** `VeyraBotRules::NextPurchase` tops the carried count up only while the build's next step is unaffordable, so the build is never starved.
- **Drinking:** a pure `VeyraBotRules::NextDrink` runs each think, beside shopping and ranking.
  - A bot below the line, away from its fountain, drinks through `UVeyraShopSubsystem::UseConsumable`, the rule players use.
  - That rule refuses a drink while one is still restoring, or in Stasis.

### 2. Base defence
- **Senses:** the bot learns which of its side's base structures is under the most threat: an inhibitor, a base tower or the Prime Well with enemy Vanguards the side sees within `defence.threatRadius`.
- **`Decide`:** a defence step comes after the bot's own retreat, tower, secure, guard and fight steps, and before shopping.
  - **Far from the threat** (beyond `defence.recallDistance`) **and with no enemy near:** the bot recalls, since the fountain is in the base.
  - **Otherwise:** it walks to the threatened structure, and the fight step takes over once foes are in sight.
  - Junglers answer too.

### 3. Buyback
A dead bot buys back through `AVeyraGameMode::HandleBuybackOrder`, as a player does, when all of these hold:
- its difficulty allows buyback;
- its base is under threat;
- it would otherwise wait at least `buyback.minWaitSeconds`;
- its Gold covers the quote plus `buyback.reserveGold`.

The quote is the economy's own (`VeyraBuyback::Quote`).

### 4. Grouping
- From `grouping.startSeconds`, a side's laners share one push lane instead of their own: the lane where the side has destroyed the most enemy structures.
- Ties break by `grouping.laneOrder`.
- The jungler keeps jungling.
- Retreat and base defence still come first.

### 5. Builds and Actives
- Some Vanguards' builds take burst items that suit their damage.
- `AVeyraGameMode::HandleCastOrder` ends a purchase's undo once an item's Active is cast, for any participant. Players and bots then share one path, and the player controller no longer does it.
- The Echo Actives stay `Never`. Commanding an Echo stays out of scope.

### 6. Ownership

| Piece | Owner |
|---|---|
| The rules: buying, drinking, defence, buyback, grouping | VeyraBots (`VeyraBotRules`, pure) |
| What a bot knows: threats, respawn wait, buyback quote, push lane | VeyraBots (`VeyraBotSenses`) |
| The values | `Bots.json` (schema 8), validated |
| Ending undo on an Active | VeyraMatch (`AVeyraGameMode::HandleCastOrder`) |

### 7. Provisional answers where canon is silent
1. **Consumables:**
   - Field Tonics: Beginner carries 2 and Intermediate 3.
   - A bot drinks below 55% Health.
   - None are bought after 20:00.
2. **Defence:**
   - A threat is an enemy Vanguard within 1,500 units of an allied inhibitor, base tower or Prime Well.
   - A bot more than 4,000 units away recalls when safe.
3. **Buyback:**
   - Intermediate only.
   - Only while the base is under threat.
   - Only when at least 20 s of respawn wait remain.
   - The bot keeps 300 Gold in reserve.
4. **Grouping:** from 25:00, laners push one lane together, with ties broken Mid, then Bottom, then Top.
5. **Builds:** the burst items suit the builds of Vanguards that deal burst, chosen by damage type.

## Out of scope
- Sweeper and Quick Sight, swapping tools, and walking to ward spots.
- Commanding an Echo; fighting companions and Echoes.
- Pings and chat; reading enemy cooldowns and items.
