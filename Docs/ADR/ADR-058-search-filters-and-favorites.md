# ADR-058: Search, filters and favorites

**Status:** Proposed. The author's standing instruction (2026-09-28) is to keep working unreviewed toward a viable game, choosing a provisional answer where canon is silent. §6 lists every such answer for the author to overturn. This ADR becomes Accepted when the author merges the pull request that adds it.
**Date:** 2026-10-02
**Related:**
- [Pre-Game Client UX Bible](../Design/Veyra_Pre_Game_Client_UX_Bible_v0.1.md): 19 (the Vanguards roster), 29 (roster search and filters in champion select) and 30 (favorites).
- [Settings & Accessibility Bible](../Design/Veyra_Settings_Accessibility_Bible_v0.1.md): Proposal 58 (Focus Shop Search).
- [ADR-012](ADR-012-items-and-shop.md): the shop.
- [ADR-045](ADR-045-account-progression-collection-mastery.md): the Collection.
- [ADR-024](ADR-024-player-settings.md): bindings.

## Context

Players can't search anywhere:
- the in-match shop lists the whole catalog;
- the Collection shows every Vanguard in one list;
- champion select's roster can't be narrowed;
- there are no favorites.

The canon approves all four, with rules on what a filter may never do.

## Decision

### 1. Shop search (Proposal 58)
- **The field:** the shop gains a search field. A pure filter in the shop's model keeps the items whose name, or any stat they give, contains the text, ignoring case. An empty search shows everything.
- **The key:** a rebindable **Focus Shop Search** key opens the shop with the field focused, and focuses the field if the shop is already open. It's a Controls binding in Settings, kept in the UI input settings beside the shop's own key.
- **Typing:** while the field has focus, typed keys never reach the game, as in the chat composer.
- **Escape:** the first press leaves the field and keeps the shop open; the next closes the shop.

### 2. One roster filter (19, 29)
`VeyraRosterFilter`, a pure model, narrows a list of Vanguards:
- by search text, matching the name, ignoring case;
- by a tab: **All**, **Owned**, **Free Rotation** or, in champion select, **Favorites**.

It only narrows what shows. It never changes ownership, eligibility, bans or picks.

### 3. The Collection (19, 30)
- **The roster:** the Collection gains the search field and the All, Owned and Free Rotation tabs. Each card says **Owned**, **Free Rotation** or **Locked**, with Owned first.
- **Favorites:** each card has a Favorite toggle, the "details" of ordinary browsing until a details page exists.
- **Locked Vanguards** stay browsable, as before.

### 4. Champion select (29, 30)
- **Narrowing the roster:** the roster gains the search field and the All, Owned, Free Rotation and Favorites tabs. Its eligibility, ban and pick marks stay as they are.
- **Reset:** search and tab reset when a selection session begins.
- **Favorites are read-only here:** marking them belongs to ordinary browsing. A favorite never hovers, selects or locks anything.

### 5. Favorites are account data
- **The backend:**
  - keeps each account's favorite Vanguards in `account.favorite_vanguards` (a migration);
  - serves `GET /v1/me/favorites` and `PUT` / `DELETE /v1/me/favorites/{vanguardId}`, for released Vanguards only, up to a configured maximum;
  - refuses a change while the account is in champion select or a match.
- **The client:** the snapshot carries the favorites, and the Collection changes them through a client intent.

### 6. Provisional answers where canon is open
1. Shop search matches item names and the names of the stats an item gives, not descriptions.
2. A favorite changes from the Collection's cards until a Vanguard details page exists.
3. An account keeps at most as many favorites as there are released Vanguards: no lower cap. The limit is configuration.
4. A favorite can't change while the account is in champion select, matching "No leaving select to edit favorites", or in a match.

## Out of scope
- The Vanguard details page with Overview, Abilities, Lore, Skins and Mastery (UX-20), which waits for lore and skins.
- Shop categories beyond the existing groups.
- The screen reader's shop navigation (Proposal 93).
