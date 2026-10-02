# ADR-021: Custom lobbies, custom rules, and friends in the client

**Status:** Proposed. The author's standing instruction (2026-09-28) is to keep working unreviewed toward a viable game, choosing a provisional answer where canon is silent. §8 lists every such answer for the author to overturn. This ADR becomes Accepted when the author merges the M16 pull requests that add it.  
**Date:** 2026-09-29  
**Related:**
- [ADR-007](ADR-007-match-join-contract.md): the match assignment and join contract.
- [ADR-010](ADR-010-play-flow.md): the play flow, solo practice and End Custom Match.
- [ADR-013](ADR-013-ai-vanguards.md): AI Vanguards, their difficulties and lanes.
- [ADR-019](ADR-019-match-flow.md): absence and votes.
- [ADR-020](ADR-020-camera-minimap-kill-economy.md): buyback.
- [Custom Matches Bible](../Design/Veyra_Custom_Matches_Bible_v0.1.md): §1–§5 and §7.
- [Parties & Social Bible](../Design/Veyra_Parties_Social_Matchmaking_Bible_v0.1.md): friends, blocks and invites.
- [Modes & Access Bible](../Design/Veyra_Modes_Access_Bible_v0.1.md): §4, Co-op composition.
- [Architecture Constitution](../../ARCHITECTURE.md): §1.1 and §1.3.

## Context

A player can play a matchmade 1v1 or solo practice against a fixed line-up of four Beginner bots. Neither lets friends play together or choose the bots. The Custom Matches Bible §1–§4 is Locked on what a custom match is:
- **Membership (§1):** an invite-only lobby; the host decides which human takes which team and slot.
- **Bots (§1–§3):** the host may place bots on either side, with any released Vanguard and each bot's own difficulty.
- **Composition (§1):** uneven sides and 1v0 are valid, provided at least one human plays.
- **Rules (§4):** the host changes rules such as starting Gold and whether normal victory is enabled. They are editable, validated data applied only to that session, and End Custom Match ends it with no winner.
- **Rewards (§5):** none.

§7 leaves open the configuration menu, the lobby lifecycle, host transfer and the duplicate policy; this ADR takes provisional answers there (§8).

The code already has most of the parts:
- **Backend:** friends, blocks and party invites (`Backend/internal/social`, `party`), none of which the client calls; and champion select (`selection`), which is generic over seats and sides.
- **Match service:** it creates a match from a `Spec` and writes the assignment to the server (`match`, `docker`).
- **Game server:** it plays bots from the assignment (ADR-013).

But solo practice is hard-coded as the only kind of match with a host or bots: `match.Create` and `ValidatePractice`, `validateResult`, `UVeyraMatchHostSubsystem::SetAssignment`, `CheckEndCustomMatch` and `CanEndCustomMatch`. Starting Gold is global (`Economy.json` `gold.starting`).

## Decision

### 1. A lobby is its own backend object

A new `Backend/internal/lobby` package owns custom lobbies, stored in Postgres (migration 0018). A lobby is not a party; §1 keeps the two distinct.

A lobby has:
- a host;
- per side, `customLobby.playersPerSide` slots, each empty, a human, or a bot `{vanguardId, difficulty}`;
- its rules `{victoryEnabled, startingGold}`;
- its pending invites.

**Rules:**
- **Hosting:** creating a lobby makes its creator the host, seated on side A.
- **Invites:** only the host invites, and only a friend who blocks no member and is blocked by none (the social checks the party uses). An invite expires after `customLobby.inviteLifetime`. An invitee who accepts takes the first empty slot on the side with fewer humans; with no empty slot, the accept is refused.
- **Host powers:** the host moves a human to any empty slot; adds, changes and removes bots; sets the rules; kicks a human; and launches.
- **Bots:** a bot may play any released Vanguard (§2), at Beginner or Intermediate (§3). Within a side, each Vanguard appears once, bots included.
- **Leaving:** any human may leave. If the host leaves, the next human by join order becomes host. A lobby with no human closes.
- **Busy:** one lobby per account. A lobby member is busy: they cannot queue, start practice or join a party queue, and the matchmaker and `party.StartQueue` see it.
- **Rule ranges:** starting Gold lies within `customLobby.startingGold {min, max}`. `victoryEnabled` defaults to on when both sides have a Vanguard, and to off for 1v0 (§1: 1v0 is open-ended). Victory cannot be turned on with an empty side.

Lobby state is polled by its members, as the party's is (`customLobby.pollInterval` on the client). Chat's ADR will decide the push channel both need.

### 2. Launch goes through champion select

Only the host launches. It needs:
- at least one human (§1);
- every human free: no match, select or queue.

The humans then enter champion select with a new `KindCustom`:
- it has a host, as practice does, and a seat per human on its side;
- the bots appear as locked seats with their Vanguards;
- humans pick only what they own or what the rotation offers (`account.MayPick`, §2);
- within a side, each Vanguard appears once, so a human cannot pick what a bot on their side plays; mirrors across sides are allowed.

If select ends without a match (a player leaves, the timer lapses unfilled, or the host cancels), every member returns to the lobby, which is kept.

When everyone locks in, the match is created with `Spec{Rules: custom, Host, Seats, Bots, Settings}`. The bots' order within a side is their slot order, which sets their lanes (ADR-013).

### 3. `custom` rules, end to end

- **Backend (`match`):**
  - `RulesCustom` joins `standard` and `practice`.
  - Custom and practice are the two kinds with a host (`Rules.HasHost`), so the practice checks become host checks rather than a third branch.
  - `validateResult` accepts, for custom:
    - `host_ended` with no winner;
    - with victory enabled, `prime_well_destroyed` and `surrender`;
    - `abandoned`, as for every kind.
  - Migration 0018 widens the rules and select-kind CHECKs.
- **The assignment, schema 5.** The Go constant, the C++ `SchemaVersion` and the JSON schema's enum change together:
  - rules gain `"Custom"`;
  - bots are allowed for Practice and Custom;
  - a `settings {victoryEnabled, startingGold}` object is present exactly when the rules are Custom.
  - The game server refuses a non-finite or negative starting Gold. The ranges are the backend's (§1).
- **Game server (`EVeyraMatchRules::Custom`):** the settings reach the systems they change through the match's rules, never through global tuning:
  - **Victory:** a fallen Prime Well wins only for Standard, or for Custom with victory enabled.
  - **Starting Gold:** it is the settings' value in Custom, and `Economy.json`'s otherwise.
  - **End Custom Match:** the Custom host may end any custom session, host-ended with no winner (§4, sandbox), as the practice host does.
  - **Votes:** surrender votes work in a Custom match with victory enabled. Remake and pause are for matchmade matches only.
  - **Absence:** the AFK and leaver machinery stays with Standard matches, since a custom match awards nothing (§5). Rejoin works for every kind.
  - **Buyback:** it runs in Custom as in Standard. It stays off in Practice (ADR-020).

### 4. No rewards

Custom results are recorded, with `rules=custom`, and appear in Match History with their statistics. They award nothing (§5); the game has no Account XP or Mastery yet in any case.

### 5. Solo practice stays

`POST /v1/practice` and `RulesPractice` stay as today's one-click practice. A lobby with one human and no bots is the same thing built by hand; merging the two is left for later.

### 6. The client

- **A friends panel** is a persistent surface down the right of the shell and the lobby (Art Bible §7). It lists friends, adds one by exact display name (`GET /v1/accounts?displayName=`), answers friend requests, shows pending requests, and shows and answers lobby invitations. In the host's lobby each friend not yet in it has Invite.
- **Play** gains a Custom Game card, which creates a lobby. The Practice card stays beside it.
- **The lobby screen**:
  - two columns, Side A and Side B, of `playersPerSide` seats each. Each seat shows a portrait ringed in its side's colour, a name, and Host, Player or the bot's difficulty;
  - for the host, Add Bot on an empty seat and Change on a bot's. Each opens a picker of the Vanguards the lobby offers bots, at a chosen difficulty; a Vanguard another bot on that side plays is not offered. Also Remove on a bot, and Switch Side and Remove on a human. Switch Side moves the human to the other side's first empty seat, the one-click form of the host placing humans (§1);
  - the rules: Turn Victory On or Off, offered only when both sides hold a Vanguard, and starting Gold as Default Gold or one of the style's `LobbyStartingGoldChoices` that lies within the lobby's range. The choices are presentation; the backend's range decides;
  - Start Game (the host's) and Leave Lobby; the page navigation is hidden while in a lobby.
- **The lobby's answer names the bot choices.** `GET /v1/lobby` lists `botVanguards` (every released Vanguard, sorted) and `botDifficulties`, so the client offers exactly what the backend accepts.
- **The client flow** gains a `Lobby` state and lobby intents. Its reads:
  - After the profile, the flow reads `GET /v1/lobby`, so a restart resumes into the lobby. A backend with lobbies switched off answers 404, which reads as no lobby.
  - The lobby is read every `LobbyPollIntervalSeconds`. When it is selecting, the flow follows into `GET /v1/me/select`. A null lobby returns the player to the shell with the notice `lobby_gone`.
  - A cancelled or left custom select resumes, which finds the lobby open again. A custom select may be left, as a matchmade one may.
  - Friends and invitations are read every `SocialPollIntervalSeconds` in the shell and the lobby. These reads never raise the screen's problem: a failed read waits for the next.
  - Social refusals (`account_not_found`, `already_friends`, an expired invitation) show in the friends panel, never as the screen's problem. A block is never revealed.
- **After the match** the lobby is gone (its match started), so the players return to the shell. A post-game return to the lobby is deferred.

### 7. Deferred

- Spectator-only guests and the live-delay choice (§6).
- Rule overrides beyond victory and starting Gold: objective timers, item, Gold or level starts (§4, §7).
- A draft, ban or duplicate policy for customs beyond §2's per-side uniqueness.
- Presence and Appear Offline.
- Chat, and the push channel it and the lobby need.
- Merging solo practice into lobbies.
- Party invites in the client: the backend has them, but the one matchmade mode (`casual_select`) seats one human a side, so a party has nothing to queue for yet. They come with the first matchmade mode for several humans (Co-op vs AI, Draft). *Closed by [ADR-044](ADR-044-party-and-social-client.md).*
- Returning to the lobby after its match.

### 8. Provisional answers where canon is silent (for the author to overturn)

- **Joining:** an invitee who accepts joins the side with fewer humans.
- **Host transfer:** when the host leaves, the longest-present human becomes host.
- **Duplicates:** unique within a side, bots included; mirrors across sides are allowed (blind pick).
- **Selection for customs:** blind champion select for the humans; the bots are chosen in the lobby.
- **End Custom Match:** the host may end any custom session; canon's sandbox allows it.
- **Surrender:** allowed in a custom match with victory on.
- **The rules offered:** victory on or off, and starting Gold within the backend's range.
- **Moving humans:** one click moves a human to the other side; the host does it for anyone (§1).
- **Finding friends:** by exact display name; no search or suggestions.

## Consequences

- Friends can play together, and anyone can play a full 5v5 against, or beside, bots they choose. Playtesting stops depending on the matchmaker.
- The practice-only special cases become "rules with a host", which the next kind of hosted match will reuse.
- Assignment schema 5 means old server images reject new assignments. The smokes rebuild the image.
- Lobbies and friends are polled; chat must bring the push channel.

## Amendments to earlier records

- **ADR-007:** the assignment's schema is 5: `Custom` rules, bots for hosted rules, and `settings`.
- **ADR-010 §1 and §7:** custom lobbies arrive; End Custom Match extends to the custom host.
- **ADR-019:** absence stays Standard-only, and surrender votes also run in a Custom match with victory on.
- **ADR-020:** buyback runs in Custom matches.

## Open items

- The push channel for lobbies, invites and chat.
- Merging solo practice into lobbies.
- Host-configurable objective timers and further rules (§7).
- Spectator guests (§6).
