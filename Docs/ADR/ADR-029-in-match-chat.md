# ADR-029: In-match Team Chat and All Chat

**Status:** Proposed. The author's standing instruction (2026-09-28) is to keep working unreviewed toward a viable game, choosing a provisional answer where canon is silent. §8 lists every such answer for the author to overturn. This ADR becomes Accepted when the author merges the pull request that adds it.
**Date:** 2026-09-30
**Related:**
- [Chat & Communication Bible](../Design/Veyra_Chat_Communication_Bible_v0.1.md): §2 (Team and All Chat, All Chat preference, mute), §6 (spectator and replay privacy), §7 (chat presentation settings).
- [Settings & Accessibility Bible](../Design/Veyra_Settings_Accessibility_Bible_v0.1.md): SET-66 (chat text size), SET-67 (chat backdrop).
- [ADR-020](ADR-020-camera-minimap-kill-economy.md) §2: team pings, whose server-side pattern chat follows.
- [ADR-024](ADR-024-player-settings.md): the settings registry and stores.

## Context

The Chat Bible locks live in-match Team Chat and All Chat (§2), with an All Chat preference, individual mutes kept at delivery, and no transcript in spectating or replays (§6). Nothing in the game carries text between players yet. Pings (ADR-020 §2) already show the shape: a server-side owner validates and hands each message to its recipients' controllers.

## Decision

### 1. Channels and recipients

- **Team Chat** reaches the human players on the sender's side, the sender included.
- **All Chat** reaches the human players on both sides who keep All Chat on.
- Bots never receive chat.
- The server decides recipients at delivery: `UVeyraChatSubsystem` (VeyraMatch, `Chat/`) sends each message to each recipient's own controller through an owner-only client call. A client can never fetch chat it may not read.
- Spectators and replays never carry chat, since no replicated actor holds it (§6).

### 2. Validation

The server checks each message:
- the sender is a seated human participant;
- the match is in Preparation or Live (a pause does not stop chat);
- the text, once cleaned, is not empty and is no longer than `chat.maxCharacters` (Match.json). Cleaning turns control characters into spaces and trims the ends.
- the sender has sent at most `chat.maxPerWindow` messages in any `chat.windowSeconds` of real time;
- for All Chat, the sender's own All Chat is on.

The client learns of a refusal as it does for pings.

### 3. Mute

- A player mutes or unmutes a participant for the rest of the match. The server stops delivering the muted player's messages to the muter, so a mute holds even if the muter's client misbehaves.
- A mute is not a report or a block, and does not persist past the match (§2).
- Players mute from the chat composer: `/mute <name>` and `/unmute <name>` (§8.3).

### 4. The All Chat preference

- The setting `communication_all_chat` (Account scope, changeable anywhere, On by default) says whether a player takes part in All Chat.
- The player's controller tells the server its value as it joins the match and whenever it changes. The server then neither delivers All Chat to that player nor accepts All Chat from them.

### 5. Presentation

- The HUD draws the chat log at the bottom left, above the ability deck: the newest messages, each with its channel, sender and text, allies and enemies in their colours.
- Messages fade after a while unless the composer is open.
- The chat key (Enter by default, `controls_bind_chat`) opens the composer.
  - Enter sends; Escape closes it without sending.
  - Tab switches between Team and All, and a leading `/all` sends one message to All.
  - While the composer is open, typed keys never reach gameplay.
- Two settings shape the log (SET-66, SET-67): `communication_chat_text_size` (Standard, Large, Extra Large) and `communication_chat_backdrop` (Transparent, Standard, High Contrast).
- A client keeps its newest `chat.keepMessages` messages.

### 6. Out of scope

- Party Chat, friend direct messages and post-match chat (§3–§5). They need the backend's social services.
- Chat reports, filters and moderation evidence (the Moderation Bible).

## 8. Provisional answers where canon is open

1. **Enter opens Team Chat; `/all` or Tab sends to All**, and Shift+Enter opens the composer on All.
2. **A 250-character limit and five messages in five seconds**: room for a sentence, and a stop to floods.
3. **Mute by chat command**, `/mute` and `/unmute`. A scoreboard mute button waits on an interactive scoreboard.
4. **Chat in Preparation and Live only.** The loading screen has no chat yet, and after the match the results screen's optional post-match chat is a separate space (§5).

## Consequences

- Chat adds one server-owned subsystem, one set of controller calls and a HUD panel. It shares no state with pings.
- Party, direct and post-match chat will need the backend. The in-match channels stay game-server-owned.

## Tests

- **Match:** the chat rules: cleaning, the length and rate limits, who receives each channel, and a client's cap.
- **Net:** Team Chat reaches only the sender's side and All Chat both. A mute and All Chat off are kept at delivery, and a sender with All Chat off is refused.
- **UI:** the chat log's layout and the composer's commands.
