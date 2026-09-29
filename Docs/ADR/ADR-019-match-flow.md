# ADR-019: Match flow: reconnect, autopilot, AFK, votes and personal results

**Status:** Proposed. The author's standing instruction (2026-09-28) is to keep working unreviewed toward a viable game, taking League of Legends' answer where canon is silent. §9 lists every such answer for the author to overturn. This ADR becomes Accepted when the author merges the M14 pull requests that add it.  
**Date:** 2026-09-29  
**Related:**
- [ADR-006](ADR-006-unreal-project-scaffold.md): §7, server-side movement.
- [ADR-007](ADR-007-match-join-contract.md): join tickets and results.
- [ADR-010](ADR-010-play-flow.md): the client-state coordinator, Reconnect-only.
- [ADR-011](ADR-011-battleground-runtime.md): lanes and towers.
- [ADR-013](ADR-013-ai-vanguards.md): bots.
- [ADR-017](ADR-017-match-statistics.md): the record and Match History.
- [Match Flow Bible](../Design/Veyra_Match_Flow_Bible_v0.1.md): §3–§11.
- [Modes & Access Bible](../Design/Veyra_Modes_Access_Bible_v0.1.md): §4.
- [Architecture Constitution](../../ARCHITECTURE.md): §1.1 and §1.3.

## Context

A match can be won, but it has none of the Match Flow Bible's safety valves:
- **Rejoining is refused.** The server turns away a participant who has already joined ("rejoining waits for reconnect"), although the client's Reconnect-only state polls `/v1/me/match` and travels with the ticket the backend derives again.
- **An absent player's Vanguard just stands there.** Its pawn and PlayerState survive the disconnect: the pawn is possessed by the server-only Vanguard controller, and the PlayerState is kept inactive. Nothing gives them back or moves the Vanguard to safety.
- **There is no AFK detection, remake, surrender or pause vote.** The only pause is a developer command.
- **Results know only who joined and who was connected at the end.**

## Decision

### 1. Rejoin gives back the same Vanguard

- A rostered account that has joined and is not connected may join again until the match ends.
- **A no-show keeps its seat (Match Flow Bible §3).** When preparation begins, the game mode seats each rostered account that never connected:
  - an inactive PlayerState with its account, side, Vanguard and Flux Spells;
  - a Vanguard spawned with everyone else's;
  - tracked as disconnected from 0:00, so the autopilot, the personal-loss clock and the votes treat it as any disconnected player.

  Its player's late first login takes that seat as a returning player's does, even into a full match.
- On login the game mode finds the account's kept PlayerState:
  - It gives that PlayerState to the new PlayerController, reactivates it and destroys the fresh one.
  - The Vanguard controller keeps its pawn. No second Vanguard spawns.
  - Everything the PlayerState owns is where it was: the Ability System Component, Gold, items, cooldowns and statistics.
- The statistics record keeps one line per participant: leaving and returning are notes on it, not a new line.

### 2. Autopilot moves only

- A disconnected or AFK Vanguard is walked by its own Vanguard controller:
  - First, to a point behind the nearest standing allied tower, `autopilot.behindTowerDistance` from it toward its fountain.
  - After `autopilot.fountainAfterSeconds`, to its fountain.
  - With no allied tower standing, straight to the fountain.
- It never attacks, casts, shops, ranks or recalls, and it gets no immunity.
- It ends on reconnect, or on meaningful activity.

### 3. Activity, absence and personal loss belong to Match

- **Meaningful activity** is an accepted move, attack, attack-move, cast, Recall or vision-tool order through the game mode. A move counts only if it lands more than `activity.minimumMoveDistance` from the last counted one; skill ranks do not count. Shopping and chat never count, and bots are never absent.
- **AFK:** after `absence.afkAfterSeconds` without activity, the player is warned and autopilot begins. `absence.afkPenaltyAfterSeconds` later, a personal loss is triggered.
- **Disconnect:** autopilot begins at once. After `absence.disconnectPenaltyAfterSeconds` without returning, a personal loss is triggered.
- **Absence clocks run on the match clock, so they stop during a pause.**
  - The continuous clock resets on return.
  - The cumulative total never resets.
- **Forgiveness (§6):** a personal loss is cleared only when all three hold:
  - the team won;
  - cumulative absence is at most `absence.maxForgivenAbsentFraction` of the active duration, counting any absence still open at the end (being away at the end is no bar of its own);
  - the record shows a contribution since the player first came back after the absence that cost it: a takedown, an assist, damage to an enemy Vanguard or structure, or a Well secured.

  The contribution rule is Provisional; canon says it is still to be designed.

### 4. Votes

`UVeyraVoteSubsystem` (Match) holds at most one vote at a time. Its rules are pure functions over the vote and the roster.

| Vote | Started by | When | Passes on | A disconnected player | Window | Cooldown after a failure |
|---|---|---|---|---|---|---|
| Remake | either team | 0:00–5:00, to start | 3 of 5 | votes YES | 30 s | 60 s, that team |
| Surrender | either team | from 15:00 | 3 of 5 | abstains | 30 s | 180 s, that team |
| Pause | anyone | live | all 10 | votes YES (AFK too) | 60 s | 180 s |
| Early resume | anyone | intermission | all 10 | votes YES | — | — |

- Only the starting team votes, except on pause and resume.
- A recorded vote is locked.
- A vote fails as soon as it can no longer pass.
- Votes and the intermission run on real time, since a pause stops world time.
- The intermission lasts `votes.pause.intermissionSeconds`, then play resumes by itself.

### 5. Results

- **New end reasons:**
  - `surrender`: the other team wins;
  - `remake`: no winner, no contest.

  Both are for Standard rules only.
- **Each participant's result gains** `personalLoss` and `absentSeconds`.
- **Backend:** migration 0017 widens the end-reason check, ties the winner to `prime_well_destroyed` or `surrender`, and adds the two columns.
- **Screens:** the results screen and Match History show No Contest and a personal loss.

### 6. Values

The Match Flow Bible gives every value, as initial tuning, in `Game/Tuning/Match.json` (schema 7): `absence`, `activity`, `autopilot` and `votes`. `autopilot` and `activity.minimumMoveDistance` are Provisional: the behind-tower distance and the move threshold are Veyra's, and the fountain delay is the bible's.

### 7. Presentation

The UX Bible approves no layouts for these screens, so they stay grey-box:
- **AFK warning:** a banner under the match clock.
- **Vote panel:** under the match clock, with the kind, the tally, the time left, and Yes and No.
- **Match menu:** Surrender, Remake and Request Pause. Each is refused with the reason its rule gives.
- **Intermission:** "Paused — resumes in m:ss", with Resume Early.

### 8. Delivery

- **M14a:** rejoin, autopilot, AFK, absence, personal loss and forgiveness, the results fields, and a reconnect smoke.
- **M14b:** votes, surrender and remake results, pause votes and the intermission, and the presentation.

### 9. League answers where canon is silent (for the author to overturn)

- **AI participants in votes.** Modes & Access §4 leaves this "to be designed".
  - A bot never starts a vote.
  - In a team vote, bot teammates abstain. A team of one human and four bots can't surrender, and League's Co-op has no bot allies.
  - For pause and resume, enemy AI seats vote YES; otherwise Co-op could never pause.
  - Practice matches take no votes, since their host ends them.
- **The behind-tower point and the move threshold:** they are data, not canon.
- **Contribution after return:** any takedown, assist, damage to an enemy Vanguard or structure, or Well.

## Consequences

- A dropped player gets their Vanguard back, and their team isn't left with a statue in lane.
- Match results carry the bible's personal outcome, and the backend can later build penalties on it.
- Match owns a vote state machine, but its rules stay pure and tested row by row.

## Amendments to earlier records

- **ADR-007:** the server-side rebind is done (§1). There are new end reasons and participant fields (§5).
- **ADR-006 §7:** autopilot is a movement mode of the Vanguard controller, not AI combat.
- **ADR-017:** leaving and returning are notes on one line.

## Open items

- Penalties beyond the personal loss, such as queue restrictions and escalation, belong to a later design.
- Co-op matchmaking, whose AI-vote semantics §9 only stands in for.
- Spectators during an intermission.
