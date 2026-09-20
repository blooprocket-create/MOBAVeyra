# Veyra Custom Matches & Private Lobbies Bible

**Version:** 0.1 — Invite-only sandbox match decisions checkpoint  
**Status:** Working design canon for rules explicitly marked *Locked*; host controls and nonstandard-match resolution still need design  
**Scope:** Custom lobby entry and host permissions, human/AI team composition, selection access, bot difficulty, adjustable match rules, rewards and replay cross-references.  
**Related:** [Modes & Access](Veyra_Modes_Access_Bible_v0.1.md) owns matchmade mode/rotation policy; [Client & Platform](Veyra_Client_Platform_Bible_v0.1.md) owns pre-game and Unreal client handoffs; [Parties & Social](Veyra_Parties_Social_Matchmaking_Bible_v0.1.md) owns friends/blocks and matchmade queues; [Account, Collection & Mastery](Veyra_Account_Collection_Mastery_Bible_v0.1.md) owns reward exclusions and Vanguard entitlement; [Replay & Spectating](Veyra_Replay_Spectator_Bible_v0.1.md) owns playback and retention; [Match Flow](Veyra_Match_Flow_Bible_v0.1.md) owns standard-match adjudication.

> **Design intent:** A custom match is a host-configured sandbox for experimentation and practice, **not** a matchmade PvP/Co-op queue, a way to grant account rewards, or a bypass of human Vanguard ownership. The trusted game server still validates the lobby configuration, participant identity, selections and gameplay. Exact host UI, preset data and infrastructure are not yet specified.

## 1. Locked — invite-only custom lobbies

- A custom lobby is **invite-only**: no public custom-lobby browser, no publicly discoverable rooms and no matchmaking of strangers into open seats. The host invites the human participants they want.
- The host decides which human player occupies which team/slot. Custom-lobby membership and host assignment are distinct from matchmaking-party ownership/readiness; being in an ordinary five-player party is not required to design a custom match.
- The host may fill vacant positions with AI on **either team**, including friendly bots, opposing bots and mixed human/AI teams. This permission is **custom-only**: matchmade Co-op vs AI still requires five human allies versus five opposing AI and **never** substitutes friendly bots.
- The ordinary battleground has two teams with up to five Vanguard slots each. **Custom matches do not require all ten slots occupied**, symmetrical sides, equal humans or an opponent. A host can launch **1v1, 1v5, mixed human/bot, or solo 1v0 practice**, so long as at least one valid human player is in the match. Empty slots stay empty unless the host explicitly places a bot; no automatic friendly-bot fill.
- Solo practice means **one human Vanguard on the battleground with no other player or AI Vanguard**, not a separately required matchmaking queue. How the base victory/end-of-match system should behave without an opposing team is **deliberately open**; launching the session must not immediately end it just because one team is empty.

## 2. Locked — human selection vs AI Vanguard choice

- **Every human must permanently own the Vanguard they select or have that Vanguard available through the current weekly free rotation.** This applies independently to the host and each invited player; host privileges and custom lobby options cannot confer human entitlement.
- **Any AI slot may use any released Vanguard**, regardless of what the host or the other humans own and regardless of weekly rotation eligibility. The host chooses each bot's Vanguard independently.
- **The host may configure Vanguard-selection rules** for custom matches, but those rules cannot permit a human to select an unowned Vanguard outside their free rotation. Detailed restrictions or allowances for duplicates, cross-team mirrors, bans, draft order and AI-vs-human duplicates **remain to be designed** rather than silently copying the default matchmade rules.
- Selection eligibility, the current rotation snapshot, AI release availability and any trade/reassignment that changes a human's assigned Vanguard must be validated by trusted services, not just a client dropdown.

## 3. Locked — per-bot AI difficulty

- The host sets **each bot's difficulty separately**. Current defined AI behaviors include **Beginner** and **Intermediate**; different difficulties may be mixed within the same team or between opposing teams.
- A custom lobby could contain Beginner bots alongside one human against Intermediate bots, but the normal matchmade Beginner/Intermediate Co-op queues remain separate and retain their fixed all-enemy-AI composition.
- Additional AI behavior tiers and whether they are available in customs require separate design; merely having a per-bot selector does not create an unapproved new tier.

## 4. Locked — host-adjustable sandbox match rules

- The host may **change gameplay settings for the custom lobby**, including starting in-match **Gold**, **objective timers**, and **Vanguard selection rules**. Players can experiment with nonstandard setups without altering standard Casual, Draft, Co-op or eventual Ranked.
- The host may decide how many permitted human/AI Vanguard slots are occupied and which team each occupies. An intentionally uneven team composition is valid.
- Custom rule values must be represented as **editable validated game/mode data** and applied **only to that custom session** through an authoritative session configuration. Do not hardcode magic numbers, trust arbitrary client-supplied rules, overwrite global battleground defaults or reuse custom overrides in matchmade queues.
- This is permission to build a host-adjustable sandbox, **not** advance approval of every hypothetical option. Exact supported settings, safe ranges, which rules are immutable for technical integrity, match start procedure, victory condition adaptations, pause/vote handling and available presets remain separate design work.
- Custom games still use the normal server-authoritative combat/match rules unless a specific host-adjustable override has been explicitly defined; custom mode does not grant the host live-game authority over damage, results, purchases or player accounts.

## 5. Locked — progression and replay compatibility

- Custom/private matches award **no persistent Account XP and no Vanguard Mastery**, regardless of whether the host uses normal settings, one player, bots, or a full ten-human match. Human-owned Vanguards remain owned; a custom bot's selected Vanguard grants no human ownership or mastery.
- Other possible custom rewards/achievements, if any, are **not approved**; do not infer rewards from duration, win/loss, bot difficulty or custom setup.
- **Every custom match produces a gameplay replay recording**, including solo or uneven practice matches, under the [Replay & Spectating Bible](Veyra_Replay_Spectator_Bible_v0.1.md). Its agreed link/share, server-side Save Replay slots, retention and moderator-only chat permissions remain intact unless changed deliberately in replay design.
- An **invite-only live custom lobby** does not, by itself, imply that completed replay links are participant-only: existing replay canon permits anyone with an available link to view/save. Live nonparticipant custom-match spectator access remains **open** (friend spectating is the current established general rule).
- Match participation is needed to submit a match-specific report; being an invited guest, bot owner or replay viewer does not grant participant-report rights for matches one did not play.

## 6. Deliberately open — resolve before implementation

- Exact custom configuration menu: preset/custom modes, editable rule list, valid ranges, timers, item/gold/level starts, lanes/objective behavior, draft/duplicate/mirror policy, optional bot automation and permissions for host changes after all players Ready.
- **1v0 and other no-opponent sessions:** prevent instant victory/auto-close; define whether objectives/Fluxborn spawn, how to conclude an intentionally open-ended practice session, and how to create its authoritative replay/completion metadata.
- How to distinguish custom-lobby host from normal party leader, invite eligibility and expiration, ready/start consent, late join/rejoin, departure/host transfer, and team change while a session is launching.
- Live custom spectators, possible invited spectator-only seats, their authorization and the established three-minute delay; do not assume invite-only live play abolishes replay delay.
- Bot AI behaviors and navigation under modified objective timers, very uneven teams, absent enemy players and solo practice.
- Tests: one-human solo launch remains live until deliberate exit/end; 1v1 and uneven sides start with only assigned slots; an allied AI is allowed **only** in customs; AI can use an otherwise unowned released Vanguard while its human player cannot; per-bot mixed difficulties; custom starting Gold/objective settings never affect matchmade games; custom match grants no Account XP/Mastery; replay is created for nonstandard compositions.

**This bible captures sandbox product requirements. Backend service design, exact presets and nonstandard-match end conditions are not yet implementation-approved.**
