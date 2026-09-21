# Veyra Chat & Communication Bible

**Version:** 0.1 — Text-only communication decisions checkpoint  
**Status:** Working design canon for locked communication channels and All Chat preference; detailed messaging UI and backend remain open; approved personal chat presentation and persistence preferences are in the Settings & Accessibility Bible  
**Scope:** No built-in voice chat; in-match Team/All Chat, persistent Party Chat, friend direct messages, optional post-match chat, mute/report integration, spectator privacy and player settings.  
**Related:** [Client & Platform](Veyra_Client_Platform_Bible_v0.1.md) owns pre-game/Unreal handoffs; [Parties & Social](Veyra_Parties_Social_Matchmaking_Bible_v0.1.md) owns friendship, party membership, blocks and Appear Offline; [Moderation & Telemetry](Veyra_Moderation_Telemetry_Bible_v0.1.md) owns report eligibility, sanctions and restricted evidence; [Replay & Spectator](Veyra_Replay_Spectator_Bible_v0.1.md) owns public playback/chat separation; [Match Flow](Veyra_Match_Flow_Bible_v0.1.md) owns post-match lifecycle and live-match phases.

> **Intent:** Keep communication available without forcing people to read it or interrupting combat. A channel being *visible to its authorized recipients live* is not permission to put its history into public replays or share it outside those recipients. Messages, membership and settings have trusted authority; client widgets may display and submit but never grant themselves access to an unauthorized conversation.

## 1. Locked — no in-game voice chat

- Veyra **does not provide built-in voice chat**: no Veyra party voice, team voice, proximity voice or all-voice channel. Players may independently use external voice services; Veyra does not claim access to, capture, moderate or archive external voice calls.
- Text-based communication is supported; the precise ping/quick-communication system, its interface and any other communication tools remain separate design work. No voice-chat implementation requirement may be inferred from spectator/coaching features.

## 2. Locked — live in-match Team Chat and All Chat

- **Team Chat** is available to participating human allies during a live match; players on the opposing team do not receive these messages. A team with fewer than five humans or friendly AI retains the same human-recipient boundary.
- **All Chat** lets human participants on opposing teams converse in the same live match. **It is enabled by default** for a player who has not changed their preference.
- Each player may **disable All Chat entirely in Settings**, **without disabling Team Chat or Party Chat**. When disabled, opposing-team All Chat is not displayed to that player and that player does not send to All Chat; disabling All Chat must not disable normal in-match gameplay, Party Chat or friend direct messages. Team and All Chat remain distinct recipient scopes regardless of how their UI is presented.
- Individual player **mute** is available without leaving the match or navigating through disruptive menus; a player can stop seeing messages from another participant. Muting is not a report and does not automatically change existing friendship, party membership or future matchmaking/block restrictions.
- Individual muting, the All Chat preference and relevant chat restrictions are enforced at the message delivery/composition boundary, not simply by deleting a visible widget. A client must not be able to fetch chat it is forbidden to receive because it changed views.
- **All Chat is not a moderation exemption.** Players may report abusive communication through the existing participant-based match report flow; an allegation does not trigger an automatic sanction. The exact behavioral policy, filters and staff review standards belong in the Moderation Bible.

## 3. Locked — Party Chat across client handoffs

- **Party Chat persists across the player journey:** pre-game, champion select, live match, post-match and return to the persistent pre-game client. Launching/exiting the Unreal executable does not wipe the party conversation or require party members to re-create the chat merely to continue talking.
- Party Chat recipients are **current authorized party members only**, rather than every player on the same battleground team or every player in the custom match. During a match, it is a separate channel from Team Chat and All Chat.
- Party membership, parties that persist across a live match and block enforcement follow the Parties & Social Bible. A party-chat message is never broadcast to enemies or non-party allies by default.
- Detailed behavior for historic messages when someone leaves/is kicked/joins, offline backlogs, notification persistence, post-party deletion and custom-lobby group chat is still **open**; the continuity requirement does not approve access to prior party messages for a new member.

## 4. Locked — friend direct messages, including during a match

- Friends can send each other direct text messages in the pre-game client **and while one or both are in a live match**. A player in a match can receive and reply without leaving the game.
- The direct-message conversation remains available when the player returns from Unreal to the pre-game client. Receiving a message should be **non-obtrusive during combat**, not cover or steal control from an active teamfight.
- Either-direction blocks prevent direct messaging as defined in the Parties & Social Bible. **Appear Offline** governs outward presence and outside-party inviteability; it is not an instruction to silently override a player's ability to send/receive permitted messages. Detailed status/notification rules for Appear Offline and DMs remain open.
- Direct messages are private to their authorized conversation, not public match/party/team chat, not automatic replay content and not an unapproved source of unrestricted moderator evidence. DM abuse reporting, access, retention and any privacy/legal safeguards require explicit design.

## 5. Locked — optional post-match chat

- After a game, a **post-match chat space includes human participants from both teams** on the results screen. It is optional; nobody must type, read, wait for other players, remain in the results screen or stay in that chat to move on.
- The player may leave the results screen, return to their pre-game party or queue for the next match **immediately**, without post-match chat acting as a lobby readiness gate.
- Individual mute and participant-based match reporting remain available. The exact post-match channel lifetime, persistence when players leave, recipient rules after a participant reconnects and relation to the player's All Chat toggle remain open; do not silently treat the optional post-match space as archived/published conversation.

## 6. Locked — spectator/replay privacy and moderation separation

- **Live participating players** can read their authorized channels while playing. This does not grant **replay viewers or live spectators** the match, team, party or private-message transcripts.
- **Public replay and spectator playback never displays archived/live match-associated chat overlays or transcript data**, even for a replay participant watching their own replay. Host-invited zero-delay custom spectators receive camera/state permission, **not chat permission**. Spectator Mode offers **no spectator-to-active-player messaging channel**.
- Moderation evidence access is separately permissioned and audited; reporting eligibility remains participant-based for match reports. Chat evidence scope, DM/party evidence policies, data retention and staff safeguards are undecided in the Moderation Bible. Do not infer public chat access from a shared replay link or secretly record external voice calls.

## 7. Deliberately open — later Chat UX and Settings Bible

- The [Settings & Accessibility Bible v0.1](Veyra_Settings_Accessibility_Bible_v0.1.md) now owns approved personal Settings, including chat background mode (Transparent/Standard/High Contrast), message fade duration, timestamps, the approved general incoming-message-sound toggle (not per-channel audio categories), separate chat text sizes, in-match changes and cross-device preference persistence. **All Chat still defaults On and can be switched entirely Off without disabling Team/Party Chat** under this bible's recipient and channel rules. Detailed messaging UI, channel-specific notification/mute behavior not explicitly approved, and backend policies remain open.
- Whether the in-match chat composer defaults to Team or All, channel shortcuts, message length, rate limits, spam prevention, text filtering, translation/localization, quick-chat and full ping system; individual mute scope/duration and how party-chat mute/DM muting should interact with blocks.
- Post-match chat departure/expiration and exact interaction with All Chat being disabled; do **not** override a player's disabled All Chat preference by silently funneling opponent messages through another channel.
- DM storage, offline delivery/reply status, friend removal, Appear Offline interplay and whether non-friend messaging or requests should ever exist.
- Party history visibility, invite/kick/leave timing, reconnect delivery, private-chat consent, and detailed custom-lobby player/spectator discussion features.
- Channel-level authorization, chat-restriction enforcement, participant/report evidence association and case-based restricted staff access. Avoid sharing private chat with third parties or replay clients by convenience.

**This bible locks the communication choices reached so far; it does not authorize inventing chat backend architecture, retention policies, moderation thresholds, Settings options, or ping controls.**


## Settings checkpoint — chat presentation and voice boundary (2026-09-21)

The Settings & Accessibility Bible now records **Standard/Large/Extra Large independent chat text** (Proposal 66), **Transparent/Standard default/High Contrast in-match chat backdrop** (Proposal 67), and **optional ping-type text labels** (Proposal 68). These are player-local presentation choices; chat authorization, recipient scopes, permitted ping information and delivery are unchanged.

**No built-in voice chat** remains the explicit rule (Proposal 72 rejected); push-to-talk/microphone/voice-channel settings must not be introduced. Proposal 73 rejected a separate All Messages/DM Only/Off chat-sound classification feature; the earlier, general incoming-chat-message sound toggle (Proposal 23) still exists. Proposal 51 rejected an additional configurable Do Not Disturb During Matches mode; friend DMs must nonetheless remain non-obtrusive during combat per §4.
