# ADR-066: The selected unit's frame and abilities the player cannot afford

**Status:** Proposed. The author's standing instruction (2026-09-28) is to keep working unreviewed toward a viable game, choosing a provisional answer where canon is silent. The closing section lists every such answer for the author to overturn. This ADR becomes Accepted when the author merges the pull request that adds it.
**Date:** 2026-10-06
**Related:**
- [ADR-059](ADR-059-hud-scaling-cooldowns-statuses-and-chat-readability.md): the HUD's layout, scales and cooldown display.
- [ADR-041](ADR-041-casting-modes-and-targeting-aids.md) §1: the Select click casts an ability waiting for one.
- [ADR-015](ADR-015-flux-spells.md): Flux Spells, their slots unlocked by Team Flux, and their fixed cooldowns.
- [ADR-033](ADR-033-charge-attack-spent-statuses-movement-fields-and-grids.md) §3: a least of the resource an ability may need besides its cost.
- [ADR-030](ADR-030-stealth-markers-and-marked-follow-ups.md) §5: a marker that presents as its owner.
- [ADR-065](ADR-065-first-match-feedback-waves-spires-rewards-and-patch.md) §11: bodies drawn larger than their capsules, and the cursor's pick radius.
- [Settings & Accessibility Bible](../Design/Veyra_Settings_Accessibility_Bible_v0.1.md) §3.4: cooldown displays; colour alone is never the only sign.
- [Combat Bible](../Design/Veyra_Combat_Bible_v0.5.md) §27: costs.

## Context
After 0.2.1 the author asked for two things (2026-10-06):
- "selecting an enemy with left click should show their health, mana, items, flux availability";
- "when a ability doesnt have enough mana, it should get greyout so the player knows they cant use it".

The code:
- **Left click.** The Select click casts an ability that waits for one (ADR-041 §1), sends a waiting Attack Move, and with a ping key held pings; over the minimap it moves the camera. Otherwise it does nothing.
- **What reaches every client.** A unit's Health, its resource, Armor, Magic Resist, Move Speed, Level and items.
- **What reaches only the owner.** The loadout (what each slot holds) and the cooldown ledger, so no client knows another participant's Flux Spells or when they are ready.
- **The deck.** The deck greys a cooling slot behind its sweep, but shows a slot its owner cannot afford as ready. The server refuses that cast (InsufficientResource) by the cast validator's own test: the archetype's cost at the slot's rank, plus a share of the resource held (ADR-033 §3), times the cost reductions held, and any least the ability needs.
- **Canon.** No bible defines a selected unit's frame. Settings Bible §3.4 adds cooldown display settings for the player's own abilities, items and Flux Spells, and says "No enemy-cooldown tracking is introduced."

## Decision

### 1. An ability the player cannot afford shows it
- **One test.** `VeyraAbilities::CanAffordCast` asks the ability's own archetype whether its caster holds enough of the resource now. It is the same test the cast validator applies before it refuses a cast as InsufficientResource. A recast that only ends a lasting effect is free (ADR-008 §9), so it counts as affordable.
- **The deck.** A learned slot that is ready but unaffordable is darkened with the resource's colour, and its cost shows on it in figures. So colour is never the only sign (Settings Bible §3.4). A cooling slot keeps its sweep.

### 2. Left click selects a unit
- **The click.** The Select click selects the unit under the cursor when nothing else claims it: no ability waits for it, no Attack Move waits, no ping key is held, and it falls on no HUD panel or the minimap. Over open ground it clears the selection.
- **Which unit.** It takes the nearest unit along the cursor's line, as the cursor's own pick finds them (ADR-065 §11). With Target Vanguards Only held, it takes the nearest Vanguard.
- **Presentation only.** Selection lives on the local controller and never reaches the server; it orders nothing.
- **When it ends.** It ends when the unit leaves this client, which happens when fog hides it or its body goes, or when stealth hides it.
- **The HUD's panels.** A HUD hit test, beside the minimap's, tells the controller when the cursor is over a panel (the deck, the minimap or the selected unit's frame), so a click on the HUD neither selects nor clears.
- **A ring.** The selected unit is ringed on the ground in its side's colour.

### 3. The selected unit's frame
The frame stands in the top left, under Team Flux. It shows what the selected unit presents to the viewer's side, so a marker presenting as its owner shows its owner (ADR-030 §5).
- **Every unit:** its name, its side's colour, and its Health with shields, in a bar with figures.
- **A Vanguard also:**
  - its face and Level;
  - its resource, in a bar with figures, in its resource's colour;
  - its six item slots;
  - its two Flux Spells, each either locked (its team has not unlocked the slot yet), ready, or cooling down with the seconds left and a sweep.

### 4. Every participant's Flux Spells reach every client
**Author ruling (2026-10-06).** A selected enemy's Flux Spell availability shows. Settings Bible §3.4's "No enemy-cooldown tracking" governs the settings that display the player's own cooldowns. The frame tracks nothing: it shows what a selected, visible unit holds at that moment, and forgets it when the selection ends.

To show it, two slices of owner-only state now reach every client, and nothing more:
- **The loadout** shares the Flux Spell in each spell slot and how many slots its team has unlocked. Team Flux is shown to everyone already (ADR-011 §10).
- **The cooldown ledger** shares the cooldowns of the abilities its loadout names shared, which are the Flux Spells. The ledger keeps one arithmetic: the shared entries are the ledger's own entries, filtered, refreshed whenever the ledger changes.

Ability ranks, ability cooldowns, power and the slots' other contents stay the owner's.

The participant's state is always relevant, so these shared values reach a client even while the participant is unseen. Only the selected unit's frame shows them, and only while the unit is visible.

### 5. Out of scope
The frame shows no stats, statuses or abilities, and nothing about an ally beyond what an enemy shows. Each would need its own decision.

## Consequences
- The deck now says why a ready ability cannot be cast.
- A player can check an enemy's items, resource and Flux Spells by clicking it, as the author asked.
- Two owner-only slices widen to every client. A client modified to read memory could follow every participant's Flux Spell cooldowns all match. That is accepted as the cost of showing them at all.
- Left click now does something on open ground: it clears a selection. Nothing else used that click there.

## Provisional answers for the author
1. A slot the player cannot afford shows its cost in figures over a darkened tile in the resource's colour (§1).
2. Selection takes the nearest unit under the cursor, and the nearest Vanguard while Target Vanguards Only is held (§2).
3. Selection ends when the unit leaves the client or hides; a click on open ground clears it (§2).
4. The frame stands in the top left under Team Flux, and holds name, Level, face, Health, resource, items and Flux Spells (§3).
5. Every participant's Flux Spells and their cooldowns reach every client, even while unseen; only the frame shows them (§4).
