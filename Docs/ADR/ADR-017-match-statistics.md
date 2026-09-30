# ADR-017: Match statistics, the scoreboard, results and Match History

**Status:** Proposed. The author's standing instruction (2026-09-28) is to keep working unreviewed toward a viable game, choosing a provisional answer where canon is silent; §9 lists every such answer for the author to overturn. It becomes Accepted when the author merges the M12 pull requests that add it.  
**Date:** 2026-09-29  
**Related:** [ADR-007](ADR-007-match-join-contract.md) (match results), [ADR-009](ADR-009-runtime-combat-primitives.md) (the combat pipeline, attribution), [ADR-010](ADR-010-play-flow.md) (results in the shell), [ADR-011](ADR-011-battleground-runtime.md) (rewards, structures), [ADR-012](ADR-012-items-and-shop.md) (inventory), [ADR-014](ADR-014-jungle-and-flux-wells.md) (wildlife, Wells), [ADR-015](ADR-015-flux-spells.md) (spell slots), [ADR-016](ADR-016-vision.md) (wards), [Match Statistics Bible](../Design/Veyra_Match_Statistics_Bible_v0.1.md), [Pre-Game Client UX Bible](../Design/Veyra_Pre_Game_Client_UX_Bible_v0.1.md) §6–§7, [Settings Bible](../Design/Veyra_Settings_Accessibility_Bible_v0.1.md) #56, [Architecture Constitution](../../ARCHITECTURE.md) §1.3.

## Context

A match ends today with a winner, an end reason and a duration; the results page says Victory or Defeat and nothing else. Canon approves much more (Match Statistics Bible, UX Bible §7):
- **Per player, server-recorded:** K/D/A; enemy-Vanguard health damage apart from other damage; damage dealt and taken by Physical, Magic and True; effective shielding provided; effective healing, self and teammate; final level; total gold earned, by source; minion and jungle last hits apart; direct damage to enemy towers; Flux Wells secured, damage to Wells and the final hit; crowd control on enemy Vanguards by type and duration; vision (reveals, and wards placed and destroyed where wards exist); the final items and both Flux Spell slots.
- **Definitions (§3):** damage is health actually removed, and what a shield absorbs is not also damage; shielding is credited to the shield's provider for what it actually absorbed; healing is health actually restored, without overheal.
- **One responsible statistics service**; the client never computes a statistic; what is not yet known shows as pending.
- **Results:** a two-team Scoreboard, Detailed Statistics by category and a team summary. **Match History:** newest first, filtered by Vanguard, mode and outcome, loaded in batches, opening each record into the same views.
- **In match:** a scoreboard opened by a key held (default) or toggled (Settings #56); its contents are left to design.

What the code has (surveyed 2026-09-29):
1. Combat broadcasts only deaths (`OnDeath`, with credited killer, assisters and contributions) and hostile damage without amounts (`OnHostileDamage`). The Vitals set knows, per damage component, the health lost and the shield absorbed, but reports neither, and the absorption ledger spends shields without naming whose. `RestoreHealth` takes no provider and does not return what it restored. Statuses report nothing.
2. Economy grants Gold with a reason and keeps only the balance: nothing totals what was earned.
3. Nothing counts kills, deaths, assists or last hits per player.
4. The result the server posts carries the roster's join flags only; the backend refuses unknown fields and has no history query.

## Decision

### 1. Combat reports what happened; it computes no statistic

New server-only events on `UVeyraCombatEventSubsystem`, beside `OnDeath` and `OnHostileDamage`:
- **`OnDamageResolved`**, once per damage component that reaches a unit: source, target, damage type, health lost, temporary Health spent, and what each shield absorbed with its provider. The absorption ledger's spend reports each shield's share and its provider.
- **`OnHealthRestored`**: the provider, if any, the target, and the Health actually restored after the clamp. `RestoreHealth` gains an optional provider and returns what it restored. Heals cast or used by a unit name it; regeneration and the fountain do not.
- **`OnStatusApplied`**: source, target, kind and the duration applied.

### 2. Economy reports every grant

`UVeyraGoldComponent::OnGoldGranted(Amount, Reason)`. Economy still keeps no statistic.

### 3. Match owns the record

`UVeyraMatchStatisticsSubsystem` (VeyraMatch, `Statistics/`) is the one statistics service (§1 of the bible). It keeps an `FVeyraPlayerStatistics` for every participant, bots too, fed only by the events above, `OnDeath`, the Flux Well and ward events. Pure rules hold the arithmetic that could go wrong: the union of crowd-control intervals from one source on one target, and which Gold reasons count as earned.
- **K/D/A** from each death's victim, credited killer and assisters.
- **Last hits:** a Fluxborn's to the unit that landed it, if a Vanguard; a creature's to its credited killer.
- **Damage and healing** by type and target category, with shielding to each shield's provider.
- **Objectives:** health removed from enemy structures; Wells secured with participation, damage to Wells, the final hit.
- **Vision:** wards placed and destroyed.
- **Final equipment** as the match ends: the inventory's slots and both spell slots.

What the in-match scoreboard shows is public: `UVeyraScoreComponent` on the PlayerState replicates K/D/A and last hits to everyone, while the rest stays on the server until the result.

### 4. The in-match scoreboard

A key held (default Tab, rebindable; the Toggle mode arrives with the Settings screen) opens both teams: each player's Vanguard, level, K/D/A, last hits and items, from replicated data only.

### 5. The result carries a scoreboard

`FVeyraMatchResult.Players` lists every player, human or bot, including one who left before the end with what they had as they left: side, name, Vanguard, account (or none for a bot), the statistics and the final equipment. `FVeyraMatchResult.Wells` lists each Flux Well secured: its site, the side and the match-clock time (Match Statistics Bible §5), so the team summary counts each capture once. The server's result body sends it; the backend validates its shape (known fields, finite non-negative numbers, one entry per roster account plus bots, sides that exist) and stores it as a document on `match.results` (migrations 0014 and 0015), returned with the verified result without account IDs, with `you` on the viewer's line. A result may still arrive without a scoreboard (an abandoned match before anyone played): the server sends `null`, never an empty list, and the backend reads an empty list as none; the client then shows its statistics as pending.

### 6. Results and Match History in the shell

- **Results** read the verified record: a Scoreboard per team, Detailed Statistics grouped as Combat, Objectives, Economy and Vision, and a team summary (kills, gold earned, Wells counted once per capture).
- **Match History:** `GET /v1/me/matches` lists the player's completed matches newest first, filtered by Vanguard, mode and personal outcome, with a cursor for "Load More" and every mode the player has a saved match in, so the mode filter reaches every record; opening one shows the same Scoreboard and Detailed Statistics.

### 7. Values

Statistics are recorded facts, not tuning. The in-match scoreboard's key is input data; its layout is presentation.

### 8. Delivery

- **M12a:** the events, the service, the public score and the in-match scoreboard.
- **M12b:** the result's scoreboard end to end, results, and Match History.

### 9. Provisional answers where canon is silent (for the author to overturn)

1. **Gold earned** counts starting Gold and every reward; sales, undone purchases and developer grants are not earned.
2. **Healing done** counts heals from abilities and items (Mend, a Field Tonic; lifesteal when it exists). Regeneration and the fountain are not a player's healing.
3. **Crowd control** counts the kinds that exist, Stun and Slow, by duration on enemy Vanguards.
4. **The in-match scoreboard** shows each player's Vanguard, level, K/D/A, last hits (minions and monsters) and items. Enemy Flux Spells stay hidden for now: they replicate to their owner.
5. **Reveal statistics and Vision Score** wait: the bible leaves Vision Score's formula undecided, and a reveal needs its own attribution. Wards placed and destroyed are recorded.

## Consequences

- Combat gains three events and a provider on healing; every heal and shield names who gave it.
- The PlayerState gains a public score component.
- The result's schema and the backend's storage grow; older servers' results (without a scoreboard) stay valid.
- A second screen joins the shell: Match History.

## Amendments to earlier records

- **ADR-007:** the result carries a scoreboard; the backend lists a player's matches.
- **ADR-009:** Combat's events report resolved damage, restored Health and applied statuses; `RestoreHealth` names its provider.
- **ADR-010:** results show the scoreboard; the shell gains Match History.

## Open items

- Reveal events and duration, and Vision Score.
- Account XP and Vanguard Mastery on the results page (no account progression yet).
- Reporting, commendation and post-match chat on results.
- The in-match scoreboard's Toggle mode.
