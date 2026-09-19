# Veyra Launcher, Pre-Game Client & Platform Bible

**Version:** 0.1 — Captured platform flow and responsibility boundaries; further design discussion required  
**Status:** Working design canon for rules explicitly marked *Locked*; not a finalized backend or UI specification  
**Scope:** Launcher authentication/install/patch flow, persistent pre-game client, Unreal match executable, reconnection and post-match handoff, and high-level trusted-state boundaries.  
**Companion documents:** [Account, Collection & Mastery Bible](Veyra_Account_Collection_Mastery_Bible_v0.1.md) owns persistent XP, Vanguard entitlements, currencies, skins and mastery; [Modes & Access Bible](Veyra_Modes_Access_Bible_v0.1.md) owns queues, rotation and Ranked gates; [Battleground Bible](Veyra_Battleground_Bible_v0.9.md) owns select format and battlefield rules; [Match Flow Bible](Veyra_Match_Flow_Bible_v0.1.md) owns disconnect/AFK, votes and result adjudication. Repository-root [ARCHITECTURE.md](../../ARCHITECTURE.md) governs engineering ownership/validation.

## 1. Locked — three distinct player-facing applications/states

| Surface | Primary responsibility | Must not own |
|---|---|---|
| **Launcher** | Account **login/authentication**, game installation, patching, repair and launch handoff. | In-match gameplay rules or a second persistent pre-game feature suite. |
| **Persistent pre-game client** | Account/profile progression, full Vanguard Collection and purchases, cosmetics/mastery, social/friends and parties, queue selection/matchmaking UX, champion select, post-match presentation and **Reconnect** entry point. | Gameplay authority, verified match outcomes, durable currency/ownership or authoritative matchmaking decisions. |
| **Unreal Engine 5.8 in-game client** | Boots **for a match**, connects to the dedicated authoritative match server, renders and interacts with live gameplay and in-match HUD/shop. | Launcher login, permanent account/commerce balances, client-decided match results. |

- **Login is in the launcher, not the pre-game client.** The launcher hands off authenticated session context by a secure, to-be-designed mechanism; no implementation should copy passwords, persist raw credentials in the game client, or invent a particular token provider without an explicit security decision.
- The launcher gets out of the player's way once authentication, install/update/repair and pre-game launch have succeeded. Whether it retains a nonintrusive background process is an **open engineering detail**, not a gameplay feature requirement.
- The **pre-game client remains running in the background** while Unreal is foregrounded for a match, retaining the user's navigation point and session state.
- After match completion, the player returns to the **pre-game client** for post-match results, persistent progression/rewards and the next queue. The match executable is not the home for ongoing social, ownership or account-navigation features.
- If the in-game client crashes or exits while its match is **still live**, the pre-game client should offer **Reconnect** and relaunch/join the **same match and assigned Vanguard**, consistent with the Match Flow Bible. A client crash is not a new match, free dodge or a new Vanguard selection.

## 2. Locked — standard player journey

1. Launch game launcher; authenticate; install/update/repair as required; enter the pre-game client.
2. First-time accounts follow the mandatory tutorial and permanent starter-Vanguard selection described in the Account Bible before normal play.
3. In the pre-game client: browse the **entire Vanguard Collection**, view owned and unowned Vanguards and mastery, purchase Vanguards/skins, manage party/social connections, select an available mode, and enter matchmaking.
4. Matchmaking and queue eligibility are authoritative: Ranked requires **Account Level 30 and 20 owned Vanguards**; rotation access and Co-op queues follow the Modes & Access Bible. The pre-game client displays state, not client-only eligibility decisions.
5. Champion select runs as part of the **pre-game flow**, using mode-specific pick/ban/trade rules and per-player entitlement validation. A deliberate pre-game dodge and a genuine disconnect follow the Match Flow Bible's distinct cancellation/penalty rules.
6. After confirmed match assembly/select, launch or foreground the **Unreal in-game client** and connect to the assigned dedicated match server. Follow authoritative loading and fountain preparation, then the live match.
7. If the in-game executable exits prematurely, keep the pre-game client available and show the reconnect path while that match is still active. On valid match resolution, show **post-match results** in the pre-game client and allow normal browsing/queueing again.

**Co-op clarification:** PvP loading has **10 human connections**; Co-op vs AI has **5 human connections plus 5 server-controlled enemy AI Vanguards**. No friendly AI fills absent human slots. This modifies the human-connection interpretation of the older Match Flow Bible's shorthand “all ten connections”; it does not change the underlying match timeline or AFK adjudication for human players.

## 3. Locked — collection, progression & post-match UX

- Show **every released Vanguard in the Collection** irrespective of ownership and free-rotation state. Show the player's existing per-Vanguard mastery even when access to play the Vanguard is unavailable.
- Persistent account level and XP, earned Vanguard-unlock currency, premium currency, permanent Vanguard entitlements, skin entitlements and uncapped per-Vanguard mastery are **trusted backend state**; the pre-game UI observes them.
- The pre-game client displays shop offers and eligible Vanguard/bundle/skin purchases, but it cannot award currency, create entitlements, alter prices or validate a purchase solely on the client.
- **Post-match progression displays** must distinguish Account XP from Vanguard Mastery and the game's in-match Gold/XP/Team Flux. Co-op vs AI awards no account XP at Account Level 10+, but continues to award eligible Mastery. A remake does not award account XP; unresolved AFK/disconnect personal-loss penalties block account XP. Exact full reward/penalty semantics remain in the relevant design bibles.
- Show the **optional dismissible continuous-play reminder** in the pre-game client **after** a match once the configurable duration is reached; no play lockout, mandatory break or XP penalty.
- Mastery reward presentation is limited to the current-level mastery emote, whose visuals automatically upgrade at milestones. Loading-screen borders come from Ranked/events, not mastery; there is no public mastery leaderboard.

## 4. Authority, session integrity & implementation boundaries

- The **dedicated match server** is authoritative for live gameplay, participant identity/Vanguard assignment, reconnect continuity, match outcome, duration and participation/absence adjudication; each domain's detailed authority remains with its owning system per ARCHITECTURE.md.
- The **trusted persistent account/progression and commerce services** own permanent balances and entitlements. The **trusted matchmaking/queue/select services** must validate queue entry, rotation snapshot and legal picks/trades. These are *logical responsibility boundaries*, not a decision to create a particular number of microservices or choose a hosting/database vendor.
- End-of-match rewards must be derived from **verified, idempotently handled** server results; never use the pre-game or in-game client's self-reported match success, performance, payment success or ownership as proof.
- Launcher-to-pregame-to-Unreal session handoff and reconnect must enforce authenticated account binding, assigned match and Vanguard, expiring/revocable credentials, safe process lifecycle and authorization checks. Exact tokens, local process protocol, encryption/key storage and backend architecture are **not yet specified**.
- The in-game executable and pre-game client must recover consistently when results/reward updates are delayed or connection drops: show pending/refreshable trusted state rather than fabricating an award or silently re-awarding after retries.
- The pre-game client must not make the gameplay server depend on UI uptime; a transient pre-game client failure must not become authority over whether an already-live match exists.
- Editable validated settings own timeouts, retry policies, gates, reward values and product data. No duplicated tuning literals, client-authoritative progression or hard references between unrelated modules to shortcut development.

## 5. Deliberately open — resume design before implementation

- UI/UX layout, product branding, accessibility and platform-specific screen flows; specific social/party/chat presence and moderation behavior.
- Account profile/privacy, friends/blocked users, party leadership/invites, ready checks, queue cancellation and matchmaking acceptance.
- Ranked rating, season flow, profiles/awards and how Ranked/event borders are granted.
- Authentication provider, account recovery, device/session management, launcher update strategy, anti-cheat, trust boundaries and service topology.
- Purchases, checkout, regional pricing/tax, refunds/chargebacks, receipts/entitlement recovery, abuse prevention and premium purchase safety.
- Executable lifecycle, process recovery, client/version compatibility and safe champion-select→match / match→post-match handoff when services are offline.
- Tutorial technical implementation, exact matchmaking thresholds/AI logic, telemetry, player-reporting/support and publishing pipeline.

**This v0.1 is a checkpoint for further discussion, not approval to build unchosen infrastructure or start gameplay implementation.**
