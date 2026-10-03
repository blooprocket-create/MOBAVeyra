# ADR-061: Presence, Appear Offline and offline removal from parties

**Status:** Proposed. The author's standing instruction (2026-09-28) is to keep working unreviewed toward a viable game, choosing a provisional answer where canon is silent. §7 lists every such answer for the author to overturn. This ADR becomes Accepted when the author merges the pull request that adds it.
**Date:** 2026-10-02
**Related:**
- [Parties, Social & Matchmaking Bible](../Design/Veyra_Parties_Social_Matchmaking_Bible_v0.1.md):
  - §1: leadership after an offline departure;
  - §2: a departure during matchmaking cancels the queue;
  - §4: the post-match reconnect grace period and offline removal;
  - §5: presence and Appear Offline.
- [ADR-044](ADR-044-party-and-social-client.md): the party and social client, which deferred presence to this record.
- [ADR-046](ADR-046-party-direct-select-postmatch-chat.md): chat, which presence does not gate.
- [ADR-060](ADR-060-queue-dodge-restrictions-and-in-match-invites.md): no party invitation reaches a player in a live match.

## Context

Friends show no online state, and nothing removes an absent player from a party. The canon locks:
- **Offline removal:** a player who goes offline in a pre-game party is removed from it, with no penalty. Reconnecting does not rejoin it. Leadership passes on, and a queue is cancelled.
- **The post-match grace period:** a member who doesn't return to the pre-game client after a match keeps their place for a configurable short grace period, then is removed.
- **Appear Offline:**
  - The player looks offline to friends outside their current party, and those friends can't send them party invitations.
  - It doesn't remove them from their party, whose members still see them.
  - It lasts until the player changes it, across matches.
- **Presence is visibility.** It never bypasses blocks, eligibility, penalties or matchmaking rules.

The thresholds are open.

## Decision

### 1. The backend owns presence
- **Ownership:** a new `presence` domain keeps each account's last-seen time and Appear Offline setting in Postgres (migration 0030). Presence persists across restarts and needs no single-instance assumption.
- **Being seen:** every signed-in request marks the account seen, at most once every `presence.touchEvery` per account, so the shell's own polling keeps a player online.
- **Clients:** the client also reads `GET /v1/me/presence` every `PresencePollIntervalSeconds` in every signed-in state, which keeps a player seen on screens that poll nothing else.

### 2. Status
A friend's status is the first that applies:

| Status | When |
|---|---|
| `in_match` | the account plays a live match, Reconnect-only included |
| `in_select` | the account is in a champion select |
| `in_queue` | the account's party is queued or in Match Found |
| `online` | seen within `presence.offlineAfter` |
| `offline` | otherwise |

A live match or select counts whether or not the client is seen, since those owners already handle disconnects.

### 3. Appear Offline
- **What others see:** `PUT /v1/me/presence {"appearOffline": true}`. Friends outside the player's party see `offline`, and party members see the real status.
- **Invitations:** party invitations from friends outside the party are refused as if the player were offline (§5).
- **Joinable parties:** the friends list stops offering the player's Public party to join, since offering it would show they are online.
- **What stays the same:** the player keeps their party and may still browse, queue, play and invite. The setting persists until changed.

### 4. Offline removal
- **The sweep:** every `presence.sweepInterval` the backend removes from their party each member who:
  - hasn't been seen within `presence.offlineAfter`;
  - isn't in a live match;
  - and whose last match didn't end within `presence.postMatchGrace`.
- **Which parties:** only Idle and Queued parties are swept. Match Found and champion select have their own timeouts.
- **What it reuses:** removal is the party's existing departure. A queue is cancelled and readiness reset (§2), leadership passes to the longest-standing member (§1), and the empty party goes.
- **Afterwards:** the removed player is not rejoined on return. Appear Offline never causes removal, because removal uses the real last-seen time.

### 5. Invitations need the invitee present
A party invitation to a friend who shows `offline` to the inviter, whether really offline or appearing offline, is refused with `invitee_offline`. One refusal for both keeps Appear Offline private.

### 6. The client
- **Friends:** the friends list carries each friend's status. The friends panel shows it on every card and sorts available friends first.
- **Invite:** offered only to friends who are `online` or `in_queue`.
- **Appear Offline:** a toggle in the friends panel, saying what it does.
- **Refusals:** `invitee_offline` has its text.

### 7. Provisional answers where canon is open
1. `presence.offlineAfter` 30 s; `presence.touchEvery` 5 s; `presence.sweepInterval` 5 s; `presence.postMatchGrace` 2 minutes; the client's presence poll every 10 s.
2. Statuses `in_queue` and `in_select` are shown to friends, as `in_match` is.
3. Invitations to really offline friends are refused, the same as to friends appearing offline.

## Out of scope
- Custom lobby invitations and Appear Offline (party invitations only).
- Telling a removed player on return that they were removed.
- Recent Players and the notifications layer (the next milestone).

## Consequences
- **One owner:** presence is one backend owner that the friends list, invitations and the party sweep consult; no client decides who is online.
- **Writes:** a signed-in account costs at most one write per `presence.touchEvery`.
