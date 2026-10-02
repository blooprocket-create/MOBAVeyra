# ADR-044: The party and social client

**Status:** Proposed. The author's standing instruction (2026-09-28) is to keep working unreviewed toward a viable game, choosing a provisional answer where canon is silent. §7 lists every such answer for the author to overturn. This ADR becomes Accepted when the author merges the pull request that adds it.
**Date:** 2026-10-01
**Related:**
- [Parties, Social & Matchmaking Bible](../Design/Veyra_Parties_Social_Matchmaking_Bible_v0.1.md) §1–§2 (membership, leader, privacy, invites, readiness, the queue lock), §5 (friends) and §6 (blocks).
- [Pre-Game Client UX Bible](../Design/Veyra_Pre_Game_Client_UX_Bible_v0.1.md) proposals 7–11: invitations from the sidebar, a solo invite making a mode-less party, member cards with contextual actions, and the confirmed **Make Party Leader**.
- [ADR-010](ADR-010-play-flow.md): the client flow, its intents, and the party it polls.
- [ADR-021](ADR-021-custom-lobbies.md): the friends panel. Its open item "party invites in the client", which waited for a matchmade mode for several humans, is closed here: Draft Pick (ADR-042) and Co-op vs AI (ADR-039) both take parties.

## Context

The backend has owned the whole party since M5: invitations, accepting and declining them, leaving, removal, leader transfer, privacy, joining a Public party, and blocks. The client calls none of it. A player can choose a mode, ready up and queue, but only alone: nobody can bring a friend into a party, so two friends cannot queue together, and the Bible's five-person parties exist only in tests.

## Decision

### 1. Party invitations (Bible §1; UX-9)

- **Any member invites a friend** from the friends panel: each friend's line in the shell offers **Invite to Party**.
  - It is offered while the player's party is idle, or while they have none. Sending one without a party makes a new mode-less party the player leads (UX-9); the backend already does this.
  - It is not offered for a friend already in the party.
  - Capacity is the backend's to check, at acceptance (Bible §1).
- **Invitations to the player** show in the friends panel beside custom-lobby invitations, with **Join** and **Decline**.
  - Joining moves the player out of their current party, which must be idle (Bible §2: a queued party's membership is locked). The backend moves them in one unit of work.
  - Joining is offered in the shell. Decline is offered in the shell and the lobby, as a lobby invitation's is.
- **Refusals** show in the friends panel, never as the screen's problem, as other social refusals do: a full party, a locked party, an expired invitation, a busy player. A block is never revealed (Bible §6).

### 2. The party panel's member cards (UX-10, UX-11)

- **Each member is a card:** name, "(you)" and "(leader)" markers, and Ready or Not Ready.
- **Contextual actions.** The leader selects another member's card to open its actions:
  - **Make Party Leader** asks a confirmation that names the recipient (UX-11), then hands leadership over. Membership, mode and Ready states are kept.
  - **Remove from Party** removes the member at once. Removal carries no penalty (Bible §1).
  - A card selected again closes. Only one card is open at a time.
- **While the party is in matchmaking** the cards show status only (UX-10). Their actions return when the party is idle.
- **Leave Party** is any member's, while the party is idle or queued. Leaving a queued party takes the whole party out of the queue (Bible §2), which the backend does. The leader's departure hands leadership on by the backend's rule.
- **Privacy:** the leader switches the party between **Public** and **Private** (Bible §1). Every member sees which it is.

### 3. Joining a friend's Public party (Bible §1)

- **Backend:** `GET /v1/friends` gains `joinableParties`, a map from a friend's account to the party the player could join directly. It lists each friend whose party is Public, idle, not full and not the player's own (`party.Service.JoinableParties`).
- **Client:** that friend's line offers **Join Party**, which calls `POST /v1/parties/{partyId}/join`. It is offered in the shell while the player's own party is idle or absent.
- **The backend still decides.** The join checks blocks, capacity and the party's privacy again when it runs.

### 4. Blocks (Bible §5–§6)

- **Block** is offered on each friend's line and on each incoming friend request. A block ends the friendship and removes the two players from a shared party, so it asks a confirmation naming the player first.
- **Blocked players** are listed in the friends panel, read from `GET /v1/blocks`, each with **Unblock**.
- **A blocked player never learns of a block.** Their refusals read as "cannot be asked", as today.

### 5. Outgoing friend requests

Each pending request the player sent offers **Cancel** (`DELETE /v1/friends/requests/{accountId}`).

### 6. Reading social state

- The social read gains party invitations (`GET /v1/party/invites`) and blocks (`GET /v1/blocks`), after friends and lobby invitations.
- As before, a failed read keeps what was last read and never raises the screen's problem.
- The party is read by its own poll, as before. A social action that changes the party (joining, leaving, a block) also reads the party again at once.

### 7. Provisional answers

1. Party invitations to the player show in the friends panel. The UX Bible's nonblocking notifications (UX-24) do not exist yet.
2. Leave Party and Remove from Party ask no confirmation. Block and Make Party Leader do.
3. A friend's Public party is offered when it is idle and has room. A full or queued party is not offered, though the backend would refuse it anyway.
4. The leader may change privacy whenever the party panel shows, queued or not. Privacy changes nothing while the party is locked.
5. Member-card actions are locked while queued (UX-10). Leave Party is not (Bible §2: members may still depart).
6. Invite to Party is offered for any friend not already in the party, whatever the party's size. The invitee learns at acceptance if the party filled.

## Deferred

- **Appear Offline and presence** (Bible §5): they need a presence service. Friends show no online state yet.
- **Recent Players** (Bible §5) and blocks from the post-match player list (Bible §6; UX-57).
- **Party Chat** (Chat & Communication Bible): its own record, [ADR-046](ADR-046-party-direct-select-postmatch-chat.md).
- **Ready-up requests and invitation notifications** (UX-24).
- **Going offline in a party, and the post-match grace period** (Bible §4): they need presence.
- **Profile icons on member cards, and an empty slot's invite entry point** (UX-10): there are no profile icons yet. Invite to Party is offered from the friends panel instead.
- **The automatic replacement leader's rule** (Bible §1): still open. The backend's provisional rule, the longest-standing member, stands.

## Consequences

- Friends can party up, hand over leadership and queue together for Draft Pick and Co-op vs AI, as the Bible describes.
- The client's intents grow by eleven, each a thin call to a route that already exists. The backend's only change is `joinableParties`.
- Every refusal stays a backend decision: the client offers what the rules allow, and the backend checks again.
