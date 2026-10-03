# ADR-060: Queue-dodge restrictions and in-match invites

**Status:** Proposed. The author's standing instruction (2026-09-28) is to keep working unreviewed toward a viable game, choosing a provisional answer where canon is silent. §6 lists every such answer for the author to overturn. This ADR becomes Accepted when the author merges the pull request that adds it.
**Date:** 2026-10-02
**Related:**
- [Match Flow Bible](../Design/Veyra_Match_Flow_Bible_v0.1.md) §2: leaving champion select.
- [Parties, Social & Matchmaking Bible](../Design/Veyra_Parties_Social_Matchmaking_Bible_v0.1.md):
  - §3: matchmaking acceptance and dodges;
  - §4: invitations;
  - §5: friends;
  - §6: blocking.
- [Moderation & Telemetry Bible](../Design/Veyra_Moderation_Telemetry_Bible_v0.1.md): sanctions are kept apart from dodge restrictions.
- [ADR-010](ADR-010-play-flow.md): the play flow, where leaving a select has had no timed penalty.
- [ADR-042](ADR-042-draft-pick-and-trades.md): Draft Pick.
- [ADR-044](ADR-044-party-and-social-client.md): party and social intents.
- [ADR-047](ADR-047-reports-commendation-player-menu.md): the player menu.

## Context

Leaving a matchmade champion select already does three things:
- it cancels the select;
- it takes the leaver's party out of the queue;
- it returns everyone else to the queue in their place.

The canon also locks a personal queue-dodge penalty:
- The leaver can't queue for a while.
- A party that holds a restricted player can't queue until the restriction ends, though its other members may queue without them.
- A disconnect in champion select is never a dodge.
- Declining or missing Match Found is never penalised at launch.
- The schedule is open.

The canon also locks two invitation rules the backend doesn't yet enforce:
- A player in a live match receives no party invitation, and none is queued for later.
- A player can block another from the post-match player list.

## Decision

### 1. A dodge restricts the leaver
- **What counts:** a player who leaves a matchmade champion select on purpose (Casual or Draft) is restricted from queueing for `dodges.restriction`.
- **What doesn't:**
  - A select cancelled because a player stopped answering (`presence_lost`) restricts no one.
  - Custom and practice selects never restrict.
  - Declining or missing Match Found never restricts.
- **Repeats:** a dodge while restricted starts the restriction again from that moment. There is no escalation.

### 2. The restriction is the backend's
- **Ownership:** a new `dodges` domain keeps each account's restriction end. Selection reports a dodge to it through a narrow interface, so neither domain owns the other.
- **Storage:** the restriction lives in Postgres (migration 0029) and survives a restart.
- **It is not a sanction.** Moderation's matchmaking suspensions stay separate (Moderation L35).

### 3. Parties
- **Refusal:** `StartQueue` refuses `queue_restricted` while any member is restricted.
- **The party view:** it gives each member's remaining restriction in whole seconds, 0 when free, so the party sees who holds it back and for how long, and nothing more.
- **Leaving:** the others may leave the party and queue without the restricted player.
- **The player's own restriction:** the player reads it through `GET /v1/me/restriction`.

### 4. Invitations to a player in a live match
- **Refusal:** a party invitation to a player with an active match is refused with `invitee_in_match` and is not queued.
- **Friend requests** still go through, and the player answers them later.

### 5. The client
- **After leaving:** a player who left a select learns how long they can't queue.
- **Find Match** names the restricted member and counts down their time.
- **Refusals:** both new refusal codes have their text.
- **Blocking:** the results screen's player menu gains Block, behind the same confirmation the friends card uses.

### 6. Provisional answers where canon is open
1. One restriction length, 5 minutes, configured as `dodges.restriction`, with no escalation.
2. A dodge while restricted starts the restriction again rather than adding to it.
3. The party view shows each member's remaining seconds, which tells a party why it can't queue.

## Out of scope
- An escalating schedule, and how long repeat dodges are remembered (canon open).
- Presence, Appear Offline and offline removal from parties, which are the next milestone.
