# ADR-046: Party Chat, friend messages, champion-select chat and post-match chat

**Status:** Proposed. The author's standing instruction (2026-09-28) is to keep working unreviewed toward a viable game, choosing a provisional answer where canon is silent. §9 lists every such answer for the author to overturn. This ADR becomes Accepted when the author merges the pull request that adds it.
**Date:** 2026-10-02
**Related:**
- [Chat & Communication Bible](../Design/Veyra_Chat_Communication_Bible_v0.1.md):
  - §3: Party Chat across client handoffs.
  - §4: friend direct messages, including during a match.
  - §5 and §10: optional post-match chat.
  - §6: privacy.
  - §9: champion-select chat.
  - §11: open items.
- [Pre-Game Client UX Bible](../Design/Veyra_Pre_Game_Client_UX_Bible_v0.1.md):
  - UX-3: the friends and chat sidebar.
  - UX-17: Reconnect-only.
  - UX-33–34: the single selection chat.
  - UX-59–60: post-match chat.
- [ADR-029](ADR-029-in-match-chat.md): in-match Team and All Chat. Its §6 deferred these channels to the backend.
- [ADR-044](ADR-044-party-and-social-client.md): parties, friends and blocks. Its deferrals include Party Chat.
- [ADR-024](ADR-024-player-settings.md): the settings stores, including the Account-scope `communication_all_chat`.
- [ADR-004](ADR-004-unified-unreal-client-states.md): one Unreal application for every client state.

## Context

ADR-029 gave the match server Team and All Chat, which end with the match. The Chat Bible also locks four channels the match server cannot own:
- Party Chat follows the party through pre-game, champion select, the match, the results screen and a restart (§3).
- Friends message each other in the client and during a match (§4).
- An optional cross-team chat lives on the results screen (§5, §10).
- Champion select has one compact panel that addresses the team, with `/p` for Party (§9).

The backend owns parties, friendships, blocks, selects, matches and the All Chat preference. It has no push channel; clients poll it. The client's polls run only in the Shell and Lobby states today, and nothing reaches the backend during a live match.

## Decision

### 1. A backend chat domain

`Backend/internal/chat` owns every message outside a live match's Team and All Chat.
- It reaches other domains only through narrow interfaces that `main.go` adapts: parties, friendships and blocks, selects, matches, and preferences. It imports none of them.
- In-match Team and All Chat stay on the match server (ADR-029).

### 2. Conversations and who reads them

Each message belongs to one conversation. Who may read it is decided when it is sent and again each time it is delivered. A client can never fetch a message by changing views (Bible §2).

| Kind | Key | Readers |
|---|---|---|
| **Party** | the party | The party's current members. A member reads only messages sent since they joined, so joining grants no earlier history (Bible §3). |
| **Direct** | the two accounts | Two friends with no block either way. Unfriending or blocking ends the conversation for both. |
| **Select** | the select and a side | The seats on that side while the select is active. It ends when the select starts its match or is cancelled. Bots are not seats. |
| **Post-match** | the match | The match's human participants who have opted in (§5). |

- **Blocks:** delivery never passes a message between two accounts with a block either way, in any kind.
- **Bots** never send or receive.

### 3. Sending

- **Cleaning:** control characters become spaces and the ends are trimmed. The cleaned text must not be empty and is at most `chat.maxCharacters`.
- **Rate:** a sender may send at most `chat.maxPerWindow` messages in any `chat.windowSeconds`. This is counted from stored messages, so it holds across backend instances.
- **The client's message ID:** each send carries one, unique per sender. A resend after a lost answer returns the first message instead of storing a second.
- **The sender's name:** each message carries the sender's account and display name as they were at sending.
- **The conversation written in:** a party or select send names the party or select the player wrote in. If the sender is no longer in it when the send arrives, it is refused, so text never reaches a party or team it was not written for.
- **Refusals:**
  - `not_in_party`, `not_friends`, `blocked`, `no_select`, `conversation_changed`;
  - `not_participant`, `postmatch_closed`, `all_chat_off`;
  - `empty_message`, `message_too_long`, `rate_limited`.

### 4. Delivery

- **One poll:** `GET /v1/me/chat?after=<sequence>` returns up to `chat.pageSize` messages the account may read now, oldest first, and the cursor for the next poll. The server's sequence orders everything; clients never sort by their own clock.
- **No cursor:** the poll returns the newest `chat.historyMessages` the account may read. A restarted client therefore recovers its party and direct conversations (Bible §3, §10). Messages it may no longer read do not count toward the limit; older ones fill it.
- **Retention:** messages older than `chat.retentionHours` are neither served nor kept. A pruner removes them every `chat.pruneInterval`, whether or not anyone sends.

### 5. Post-match chat

- **Opt-in by first message:** a player joins by sending their first message (UX-59). They read only what is sent after it; nothing earlier.
- **When it ends:** a player's participation ends when they leave the results screen or enter another select or match (UX-60). Moving on is recorded, so the chat stays closed even once that select or match is over. It also closes `chat.postMatchMinutes` after the match ends.
- **All Chat off:**
  - A player with All Chat off cannot send, so cannot opt in.
  - An opted-in player who turns All Chat off receives nothing.
  - The preference is read from the backend's settings store.
- **Mutes:** a participant mutes another for that match's post-match chat. The server stops delivering the muted player's messages to the muter.

### 6. The client

- **The chat poll:**
  - It runs on its own schedule in every signed-in state except Reconnect-only (UX-17), so state changes do not stop it.
  - It never blocks a screen: a failed poll waits for the next.
  - Sends show as pending until answered, then confirmed or "not sent".
- **The shell's sidebar** (UX-3) holds Party Chat while in a party and one open direct conversation. Friend cards offer **Message** and show unread counts.
- **Champion select** (UX-33–34) has one compact, collapsible chat panel beside the ally column.
  - It addresses Team; a leading `/p` addresses Party.
  - The composer names its recipient, and Party lines are marked.
- **In a match:**
  - The HUD chat log adds the party and direct lines to the match's own, labelled `[Party]`, `From <name>` and `To <name>`.
  - Nothing pops up, sounds or takes focus (Bible §4: non-obtrusive in combat).
  - The composer accepts `/p <text>` (Party), `/r <text>` (reply to the latest direct message) and `/msg <name> <text>` (a friend by name).
  - These live in VeyraUI, which reaches the client flow; VeyraMatch does not depend on VeyraServices.
- **The results screen** has a compact post-match chat panel.
  - Before the first message it shows only an invitation to say something.
  - It takes `/mute <name>` and `/unmute <name>`, as in a match.
  - Continue leaves it.

### 7. Configuration

Backend `chat` (`Backend/config/*.json`), validated at startup:
- `maxCharacters`, `maxPerWindow`, `windowSeconds`;
- `historyMessages`, `pageSize`;
- `retentionHours`, `postMatchMinutes`, `pruneInterval`.

Client: `ChatPollIntervalSeconds` and `ChatKeepMessages` (`DefaultGame.ini`, `UVeyraServicesSettings`).

### 8. Out of scope

- Party and direct mutes, offline-delivery receipts, Appear Offline, and custom-lobby group chat (Bible §11).
- Chat reports and moderation evidence (Moderation Bible).
- Any chat in spectating or replays (Bible §6): none exists.
- A push channel.

## 9. Provisional answers where canon is open

1. **Polling, not push.** One poll per `ChatPollIntervalSeconds` (1 s) covers every conversation. Push is the lever if load grows.
2. **The limits:** 250 characters and five messages in five seconds, as in-match chat (ADR-029 §8.2). A week's retention.
3. **Party Chat is read from the moment a member joined;** a direct conversation is read only while still friends.
4. **A player with All Chat off cannot opt into post-match chat.** The Bible leaves it open, and this never routes opponents' messages around the preference.
5. **Post-match chat closes 10 minutes after the match ends.**
6. **In-match direct-message commands are `/r` and `/msg`.** There is no popup, sound or focus change.
7. **Champion-select chat is delivered by the backend,** because the select is a backend session and no match server exists yet.
8. **Mutes exist only in post-match chat,** as in a match. Party and direct mutes stay open.

## Consequences

- The backend gains a chat schema, one poll route and seven send or manage routes. Every client polls once a second outside Reconnect-only.
- The client reaches the backend during a live match for the first time, for chat only. A failure there is silent.
- In-match chat now has two sources: the match server's Team and All lines, and the backend's Party and direct lines. The HUD shows them in one log.

## Tests

- **Go:**
  - cleaning, length, and the rate window;
  - each kind's readers, join-time Party history, and blocks at delivery;
  - an idempotent resend, history with no cursor, and retention;
  - post-match opt-in, leaving, closing, mutes and All Chat off.

  Against both stores; the Postgres store with `-Postgres`. Plus each route.
- **Services:**
  - the protocol;
  - the chat poll in every allowed state and none in Reconnect-only;
  - pending and confirmed sends, with a resend that keeps its ID;
  - a party change clearing the party lines.
- **UI:** the sidebar panels, the select panel and its `/p`, the HUD log merge and commands, and the post-match panel.
- **Smoke:**
  - `-Flow Chat`: two friends in a party exchange Party Chat and direct messages.
  - `-Flow Party`: the select chat, in-match lines, and a post-match exchange.
