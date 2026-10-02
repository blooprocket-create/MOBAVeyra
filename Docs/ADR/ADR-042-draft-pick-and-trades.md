# ADR-042: Draft Pick, and trades between locked teammates

**Status:** Proposed. The author's standing instruction (2026-09-28) is to keep working unreviewed toward a viable game, choosing a provisional answer where canon is silent. §7 lists every such answer for the author to overturn. This ADR becomes Accepted when the author merges the pull request that adds it.
**Date:** 2026-10-01
**Related:**
- [Battleground Bible](../Design/Veyra_Battleground_Bible_v0.9.md): the shared lock-in rules, Casual Select and Draft Pick (bans 1-2-2-1, picks 1-2-2-2-2-1).
- [Modes, Access & Weekly Rotation Bible](../Design/Veyra_Modes_Access_Bible_v0.1.md) §1: Draft Pick is five against five, with owned and rotation Vanguards; a trade must keep each assignment legal.
- [Pre-Game Client UX Bible](../Design/Veyra_Pre_Game_Client_UX_Bible_v0.1.md) proposals 28, 29 and 35: team overview, true pick eligibility and bans, selection status.
- [ADR-010](ADR-010-play-flow.md): the select and its open items, "Draft Pick bans and turn order" and "the select trade protocol", which this record closes.
- [ADR-039](ADR-039-weekly-rotation-and-co-op-vs-ai.md) §6: the Play page's Draft Pick card.

## Context

The Play page lists Draft Pick under Casual and Ranked Draft Pick under Ranked, both not yet available. A select is a single phase today: every seat hovers and locks until one deadline, enemy hovers are hidden, and locks are permanent. Nothing models bans, turns or trades. Casual Select also lacks the trade its own rules allow.

## Decision

### 1. A draft select (Backend: selection)

A new select kind, `draft`, runs the Battleground Bible's two phases in turns:
- **Ban phase, 1-2-2-1:** side A bans one, B two, A two, B one, three bans a side.
- **Pick phase, 1-2-2-2-2-1:** A picks one, B two, A two, B two, A two, B one.

The sequence is data (`draftPick.turns`), so a test can run a shorter one; the committed configuration holds the bible's.

- **Who acts in a turn:** the turn's side's next seats, in seat order. A ban turn of two goes to the side's next two banners; a pick turn of two to its next two unpicked seats. With fewer seats than a turn's count, as in a script's 1v1, the side's seats act again in order.
- **What an acting seat does:**
  - In a ban turn, it hovers and locks a ban. Its hover is hidden from the enemy, and a locked ban is seen by everyone.
  - In a pick turn, it hovers and locks a Vanguard, as in Casual Select.
  - A seat that is not acting may still hover its intended pick, which its own team sees. It cannot lock out of turn.
- **A turn ends** when every acting seat has locked, or its time runs out:
  - In a ban turn, a seat that banned nothing bans its hover, or nothing.
  - In a pick turn, a seat that picked nothing locks its hover if it may. Otherwise the select is cancelled, timed out, as Casual Select's is.
- **Banned and picked Vanguards** cannot be picked by either team, which extends Casual Select's `Taken`. Bans cannot name a Vanguard already banned.
- **Persistence:** the session records its phase, its turn and the turn's deadline, and each ban with its side and seat.

### 2. Trades (Backend: selection; Battleground Bible's shared lock-in rules)

- **Two locked human teammates may swap their locked Vanguards,** in Casual Select and Draft Pick.
  - One offers, the other accepts or declines.
  - An offer lapses when either player's assignment changes, another trade completes, or the select leaves its trading window.
- **A trade must keep each assignment legal:** each player must own the other's Vanguard or have it in the week's rotation, as for a pick (Modes Bible §1). A trade the service refuses changes nothing.
- **Each player takes their new Vanguard's saved loadout** of starting Flux Spells, not the spells chosen for the one they gave up (Pre-Game Client UX Bible 37), and may choose again before the match.
- **The trading window:** from a seat's lock until the select starts. After the last lock, the select waits `finalDuration` before it starts, so a team can still trade (§7.3).

### 3. Matchmaking and configuration

- **A mode's `matchmaking` may be `draftPick`:** humans on both sides, five a side, opening a draft select.
- **The configuration's `draftPick` block** holds:
  - the turn sequence;
  - `banDuration` and `pickDuration` per turn;
  - `finalDuration`;
  - the presence timeout.

  `casualSelect` gains its own `finalDuration`.
- **Draft Pick's card becomes playable.** Ranked Draft Pick stays not yet available: Ranked is deferred (Modes Bible §2).

### 4. API and client

- **`GET /v1/me/select` adds:**
  - the phase;
  - the turn's side, its acting seats and its deadline;
  - the bans by side;
  - each seat's pending trade offers to and from the player.
- **Routes:** `PUT /v1/me/select/ban/hover` and `POST /v1/me/select/ban` (a ban turn's hover and lock, apart from a pick's so a late request never turns a ban into a pick), `POST /v1/me/select/trade` (an offer to a teammate), `POST /v1/me/select/trade/accept` and `POST /v1/me/select/trade/decline`.
- **The select screen shows:**
  - a ban row for each side;
  - a turn banner, such as "Your turn to ban" with its timer;
  - each seat's status: Waiting, Not Locked In or Locked In (UX 35);
  - a trade button on each locked teammate, and the offer to accept or decline.

  Banned Vanguards show on the roster as unavailable (UX 29).

### 5. Ranked

Ranked uses this structure and adds its owned-only rule and entry gates when it is built. It stays deferred.

### 6. Tests

- **Go:** the turn sequence and who acts; bans and their visibility; timeouts in each phase; picks refused out of turn or when banned; trades accepted, declined, lapsed and refused as illegal.
- **C++:** the protocol, the select model's turns, bans and trades, and the screen.
- **A smoke** (`-Flow Draft`): two clients, one a side through `scripted.json`, ban and pick through a whole draft and play the match.

### 7. Provisional answers where canon is open

1. **Who acts in a turn:** the side's next seats in seat order.
2. **A missed ban bans nothing;** a missed pick locks the hover if it may, or cancels the select as timed out, as Casual Select does.
3. **The trading window** lasts from a seat's lock until the select starts. `finalDuration` follows the last lock, in both kinds.
4. **Turn and final times** are configuration (Provisional): ban 30 s, pick 30 s, final 10 s.
5. **One trade offer at a time** per player.

## Consequences

- Draft Pick can be queued and played; Casual Select gains trades and a short final window.
- A select is no longer one phase: the session carries a phase and a turn, and the client shows whose turn it is.

## Amendments to earlier records

- **ADR-010:** its open items "Draft Pick bans and turn order" and "the select trade protocol" are decided here.
