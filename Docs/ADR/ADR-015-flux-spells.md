# ADR-015: Flux Spells: two locked slots, permanent-Flux unlocks, free preselection and fountain swaps

**Status:** Proposed. The author's standing instruction (2026-09-28) is to keep working unreviewed toward a viable game, choosing a provisional answer where canon is silent; §10 lists every such answer for the author to overturn. It becomes Accepted when the author merges the M10b pull request that adds it.  
**Date:** 2026-09-29  
**Related:** [ADR-002](ADR-002-gameplay-ability-system.md) (GAS), [ADR-008](ADR-008-vanguard-definitions-and-ability-composition.md) (ability archetypes), [ADR-009](ADR-009-runtime-combat-primitives.md) (statuses, delivery), [ADR-010](ADR-010-play-flow.md) (selection, the assignment), [ADR-011](ADR-011-battleground-runtime.md) (Team Flux, Match routing), [ADR-012](ADR-012-items-and-shop.md) (the fountain shop), [ADR-013](ADR-013-ai-vanguards.md) (bots), [Battleground Bible](../Design/Veyra_Battleground_Bible_v0.9.md) §12, §14, [Economy & Progression Bible](../Design/Veyra_Economy_Progression_Bible_v0.1.md) §13.2, [Combat Bible](../Design/Veyra_Combat_Bible_v0.5.md) §14, §15, §21, [Pre-Game Client UX Bible](../Design/Veyra_Pre_Game_Client_UX_Bible_v0.1.md) 36–40, [Match Flow Bible](../Design/Veyra_Match_Flow_Bible_v0.1.md) §1, [Architecture Constitution](../../ARCHITECTURE.md) §1.3, §1.11.

## Context

Flux Spells are the last Battleground Bible system a player would look for first: two loadout spells, on D and F. Canon fixes their rules but names no spell:
- **Two dedicated slots** (Battleground §14; Economy §13.2), separate from the six item slots and the vision-tool slot.
- **Free preselection:** each player picks up to two before the match, in champion select, at no Gold cost. Either slot may stay empty (Pre-Game UX 36).
- **Locked slots:** the picks start equipped but locked.
  - **25 permanent Team Flux** unlocks the first slot and **75** the second, at once and anywhere, for the rest of the match.
  - Temporary Flux never counts, and casting spends no Flux.
  - Thresholds belong to slots, not to spells.
- **Fountain swaps only:** a player replaces an equipped spell at their own fountain shop, for Gold.
  - A swap never bypasses the slot's threshold.
  - A remote queued purchase cannot change a spell.
- **Fixed cooldowns** (Combat §21): there is no Flux Haste, and neither Ability Haste nor Item Haste applies.
- **Blocked by crowd control:** Polymorph and similar control block Flux Spells, as they block abilities (Combat §8). A rider may cast them.
- **Champion select** shows both slots, each spell's effect and each slot's threshold; picking never extends the timer (Pre-Game UX 36).
  - The slots prefill with the last starting loadout actually taken into a match with that Vanguard (UX 37).
  - "Your Match Setup" shows them after lock-in (UX 38), and recovery restores them (UX 39).

What the code has today (surveyed 2026-09-29):
1. **One loadout owns every active slot.** `UVeyraAbilityLoadoutComponent` (Abilities, on the PlayerState) grants any `Abilities.json` entry into an `EVeyraAbilitySlot`: Q–R from `Vanguards.json`, and `Item1`–`Item6` from the shop.
   - One cast path serves them all: `ServerIssueCastOrder` → `AVeyraGameMode::HandleCastOrder` → `VeyraAbilities::TryCast`.
   - The slot enum's own comment reserves room for Flux Spells.
2. **Cooldowns already have a haste category.** `UVeyraCooldownComponent` starts each cooldown with an `EVeyraCooldownHaste` chosen by the slot. `Item` applies no haste yet; it waits for Item Haste.
3. **Archetypes are data.** An ability is an entry in one archetype map (`targetedDamage`, `area`, `selfBuff`, `skillshot`, `dash`, `empoweredAttack`); no ability is named in C++. What the spell roster (§10) needs that the archetypes lack:
   - a heal, and a way to reach an ally;
   - a damage-over-time status (Combat §14);
   - a status that reduces the damage a unit deals;
   - a way to restrict a targeted ability to certain kinds of unit.
4. **Permanent Flux is kept but unread.** `UVeyraTeamFluxSubsystem::GetPermanent` notes that Flux Spell unlocks will read it, and `OnTeamFluxChanged` fires on every grant and expiry. Match's `FVeyraBattlegroundLink` routes it only to World's Fluxborn.
5. **Selection stores only Vanguards.**
   - The backend's selection seat holds a hover and a lock.
   - `startMatch` copies the lock into `match.participants.vanguard_id`.
   - The match assignment is schema v3.
   - There is no client-side saved preference of any kind.
6. **The layers.** Items (the shop) sits below Battleground (Flux), so the shop cannot read Team Flux. Match, above both, can.

## Decision

### 1. Two slots in the one loadout

- **`EVeyraAbilitySlot` gains `Spell1` and `Spell2`,** with `VeyraAbilitySlots::Spells` and `IsSpellSlot`.
- **Spells are granted and cast as items' Actives are:** through the loadout, `TryCast` and the same cast order.
- **Rank:** a spell slot reads rank 1, as an item slot does.
- **Input:** keys D and F by default, in `UVeyraInputSettings` beside the item keys, with the same Quick Cast.
- **No new component owns the slots.** The loadout already owns what each slot holds, and replicates it to its owner.

### 2. Fixed cooldowns

`EVeyraCooldownHaste` gains **`Fixed`**, which no haste of any kind rescales. `HasteOf` gives spell slots `Fixed`. When Item Haste arrives, it will apply to `Item` alone, and spells stay fixed.

### 3. Spells are archetype entries, listed as a roster

- **The roster:** `Abilities.json` gains `fluxSpells: { provenance, roster: [ids] }`. Each ID is an ordinary entry in one archetype map, and validation requires it to be defined there, with a single-rank cooldown. Nothing in code names a spell.
- **Small, reusable archetype options:**
  - **`targetedDamage`** gains `statuses` (applied to the target on hit) and `targetKinds` (the `EVeyraUnitKind`s it may target; empty means any hostile unit). A target of another kind is refused as `InvalidTarget`. Its damage may be 0 when it applies statuses.
  - **`selfBuff`** gains at most one `heal`: `{ amount, amountPerLevel, allyRange, statuses }`. It restores Health to the caster and to the allied Vanguard within `allyRange` with the lowest Health fraction, through `VeyraCombat::RestoreHealth` (never above Max Health), and puts `statuses` on each unit it heals.
  - **`targetedDamage`'s damage** gains `damagePerLevel`, and **a status** gains `magnitudePerLevel`. Both read the caster's Level when the ability commits: `amount + perLevel × (Level − 1)`. Combat §14's snapshot rule covers statuses.
- **Two status kinds join Combat:**
  - **`DamageOverTime` (Combat §14):**
    - Each tick deals `magnitude` of the status's `damageType` in its source's name, every `tickSeconds`, on world time (a pause holds it).
    - There is no tick at application and no partial tick at expiry.
    - It ends at death, and a reapplication from the same source refreshes it.
    - Invulnerability makes a tick 0.
    - The ticks use a new delivery kind, **`Periodic`**, so Omnivamp's DoT effectiveness can find them later. It does not damage structures.
  - **`Weaken`:** the unit's outgoing damage is multiplied by `1 − magnitude`. It is the hostile counterpart of `DamageAmplification`, on the same outgoing-damage multiplier. Like generic reduction (Combat §15), it does not reduce True damage.

### 4. Unlocks: Match routes permanent Flux to each loadout

- **Thresholds:** `Flux.json` gains `spellSlots: { provenance: Canon, thresholds: [25, 75] }`, one per slot, ascending.
- **The rule is pure:** `VeyraFlux::UnlockedSpellSlots(Permanent, Thresholds)` counts the thresholds reached.
- **The loadout keeps the count.** It holds a replicated `UnlockedSpellSlots`, which Match sets.
  - `FVeyraBattlegroundLink`, already subscribed to `OnTeamFluxChanged`, sets it on each participant of that team.
  - Match also sets it when a Vanguard first spawns.
  - A temporary grant or expiry cannot lower it, because the count reads permanent Flux only.
- **`TryCast` refuses a locked spell slot** with the new `EVeyraCastRejection::Locked`, before any other check, whatever it holds.

### 5. Preselection is server-authoritative, end to end

- **Backend selection:**
  - A seat gains `fluxSpells`: two entries, each a roster ID or empty, with no ID in both.
  - A new route, `PUT /v1/me/select/spells`, sets them while the session is picking, before or after lock-in. It is refused for a spell off the roster, or one used twice.
  - The backend config gains the roster; a contract test holds it equal to `Abilities.json`'s, as the Vanguard roster is held to `Vanguards.json`.
- **Saved loadout per Vanguard (UX 37) is the backend's, not the client's.**
  - While a seat's spells are unedited, they follow its current Vanguard, hovered or locked. They are the spells that account last took into a match with that Vanguard, read from `match.participants`, or empty.
  - Once the player edits them, they stay.
  - So recovery (UX 39) restores accepted spells, and nothing is kept on the machine.
- **The match:**
  - `startMatch` copies the spells into `match.participants.flux_spells`.
  - Migration `0013_flux_spells` adds the seat's and the participant's columns.
  - The **assignment becomes v4**: each participant carries `fluxSpells`, two strings, where empty means an empty slot. A development match without selection carries two empty slots.
  - The game server checks each against the roster (`VeyraMatchRules`) and grants them into `Spell1` and `Spell2` when the Vanguard first spawns. The loadout keeps them across deaths.
- **Bots choose theirs when they are seated,** as a player would in champion select: from `Bots.json`, by seat. Match grants them the same way.

### 6. Swapping at the fountain

`UVeyraShopSubsystem::SwapFluxSpell(Participant, Slot, Spell)`, through a new controller order `ServerSwapFluxSpell`, which is rate-limited and gated by the phase like the other shop orders:
- **It is refused:**
  - away from the fountain (the shop's own test, where the dead count as there), as `NotAtFountain`; it is never queued;
  - for a spell off the roster;
  - for a spell already equipped in either slot (new `AlreadyEquipped`);
  - without the Gold, as `NotEnoughGold`.
- **It charges** `Economy.json`'s `fluxSpells.swapCost`, for filling an empty slot too. Economy §13's resale rules never refund it.
- **A locked slot may be swapped, and stays locked:** the threshold belongs to the slot.
- **The cooldown carries over:** if the replaced spell was cooling down, the new one starts on its full cooldown, so a swap cannot reset one. Otherwise it is ready.

### 7. What players see

- **Champion select** (practice and Casual): two slot pickers show the roster with each spell's name and effect (string table), each slot's threshold (from `Flux.json`), and an empty choice. **Your Match Setup** summarises the locked Vanguard and both spells.
- **HUD:** two spell rows beside the items, each showing its key and spell with one of three states: locked, with the permanent Flux it needs; ready; or cooling down.
- **Shop screen:** a Flux Spell row with the two slots, the roster and the swap cost. Its buttons work only at the fountain.
- **Icons (2026-09-29):** each spell, like each Vanguard's passive and Q W E R, shows its icon wherever it appears (`ConceptArt/Skills`, imported by `Game/Scripts/BuildIconArt.ps1 -Kind Abilities`), or its name or initials until it has one.

### 8. Bots

- **Choosing:** `Bots.json` names each seat's spells. The seat list becomes objects of role and spells, and a spell's use joins the existing ability uses.
- **Casting:** senses list the spell slots, with their lock state. The rules use a spell when its use fits the moment:
  - **Escape** (Blink): retreating hurt with an enemy near, cast toward home.
  - **Defend** (Mend): low Health in a fight.
  - **Damage** (Scorch) and **Engage** (Enfeeble): on the enemy Vanguard being fought.
  - **Secure,** a new use (Wildstrike): on a creature or a Well within range whose Health the spell's damage would end, so a jungler finishes its camp and steals a Well.

### 9. Values are data

- **`Abilities.json`:** the roster and its entries, and the new status kinds.
- **`Flux.json` v3:** the thresholds.
- **`Economy.json` v4:** the swap cost.
- **`Bots.json` v3:** the seats.
- **The backend config:** the roster.

Canon gives the two thresholds; every other value is Provisional.

### 10. Provisional answers where canon is silent (for the author to overturn)

1. **The roster, five spells (an escape, sustain, kill pressure, disruption and a jungle finisher):**

   | Spell | What it does | Cooldown |
   |---|---|---|
   | **Blink** | an instant dash of 400 units toward the cursor | 300 s |
   | **Mend** | heals the caster and the most-wounded ally within 850 units for 80 (+14 per Level), each gaining 30% movement speed for 1 s | 240 s |
   | **Scorch** | True damage over 5 s to one enemy Vanguard, 18 per second (+4 per Level) | 180 s |
   | **Enfeeble** | one enemy Vanguard is slowed 30% and deals 35% less damage for 2.5 s | 240 s |
   | **Wildstrike** | 600 True damage to wildlife, a Flux Well or a Fluxborn, never a Vanguard or structure; it can steal a Well | 90 s |

   A speed boost, a shield, a cleanse and a teleport can follow as more roster entries.
2. **Blink stops at terrain,** as every dash does, and never crosses a wall. Wildstrike has one charge.
3. **Swap cost:** 150 Gold, a little under one basic component.
4. **Which spells a slot may hold:** any, in either slot, but not the same spell twice.
5. **Bots take a standard pick per seat:**
   - Top and Mid: Blink and Scorch.
   - Jungle: Blink and Wildstrike.
   - The first Bottom: Blink and Mend.
   - The second (support): Blink and Enfeeble.

**What canon changes:** the spells are not ready at 0:00. The first slot waits for the team's first Spire or base tower (25 permanent Flux), so an early jungler has no Wildstrike.

## Consequences

**What changes:**
- **Abilities:**
  - two slots;
  - a cooldown category;
  - two archetype options (targeting kinds and statuses on `targetedDamage`, a heal on `selfBuff`);
  - Level-scaled amounts;
  - one cast rejection;
  - a slot-lock count on the loadout.
- **Combat:** two status kinds and one delivery kind.
- **Flux:** one pure rule and the thresholds.
- **Items:** the swap.
- **Match:** the routing, the grant at spawn and the v4 assignment.
- **Backend:** one route, one migration, a config roster and a contract test.
- **UI:** champion select, the HUD and the shop.

**What does not change:** no module changes layer, and nothing names a spell in code. Item Actives, Vanguard abilities and Team Flux's other uses are untouched.

## Amendments to earlier records

- **ADR-008:** the `targetedDamage` and `selfBuff` options; Level-scaled amounts.
- **ADR-009:** the `DamageOverTime` and `Weaken` status kinds and the `Periodic` delivery.
- **ADR-010:** selection's spells, the saved loadout and assignment v4.
- **ADR-011:** permanent Flux routed to spell-slot unlocks.
- **ADR-012:** the fountain swap.
- **ADR-013:** seats with spells; the `Secure` use.

## Open items

- Playtest every value above.
- Healing Reduction (Combat §6) and Scorch's anti-heal wait for the healing categories.
- Results 49's final-build snapshot of the two spells waits for match statistics.
- Teammates' spells in champion select: only your own are shown for now.
- A speed boost, a shield, a cleanse and a teleport.
