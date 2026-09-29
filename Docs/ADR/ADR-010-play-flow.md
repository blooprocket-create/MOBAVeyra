# ADR-010: The play flow: launcher, client states and Custom practice

**Status:** Accepted. The author approved it on 2026-09-27, before implementation began, and merged the M6 pull requests that add it and build on it ([#21](https://github.com/blooprocket-create/MOBAVeyra/pull/21), [#22](https://github.com/blooprocket-create/MOBAVeyra/pull/22)) on 2026-09-28.  
**Date:** 2026-09-27  
**Approved in:** Author decisions for M6 (2026-09-27): menus are UMG built in C++; Custom practice is solo only; the tutorial gate is a stubbed starter choice; available Vanguards are the owned starter plus a stand-in rotation; M6a is the solo path and M6b adds party, queue and Match Found with a local 1v1 team size.  
**Related:** [ADR-004](ADR-004-unified-unreal-client-states.md) (client states), [ADR-005](ADR-005-launcher-session-handoff-and-local-first-hosting.md) (L1–L4, build-order steps 4–5), [ADR-007](ADR-007-match-join-contract.md) (§5 assignment, §7 result, §10 match states), [ADR-008](ADR-008-vanguard-definitions-and-ability-composition.md) §8 (choosing a Vanguard), [Pre-Game Client UX Bible](../Design/Veyra_Pre_Game_Client_UX_Bible_v0.1.md) (UX-4, 5, 7, 13, 17, 35, 40), [Custom Matches Bible](../Design/Veyra_Custom_Matches_Bible_v0.1.md) §1, §4, §7, [Modes & Access Bible](../Design/Veyra_Modes_Access_Bible_v0.1.md) §1, §3, [Account, Collection & Mastery Bible](../Design/Veyra_Account_Collection_Mastery_Bible_v0.1.md) §1, §6, [Battleground Bible](../Design/Veyra_Battleground_Bible_v0.9.md) §15, [Match Flow Bible](../Design/Veyra_Match_Flow_Bible_v0.1.md) §1–2, [Parties, Social & Matchmaking Bible](../Design/Veyra_Parties_Social_Matchmaking_Bible_v0.1.md) §2–3, [Architecture Constitution](../../ARCHITECTURE.md) §4, §11.

## Context

After M5, four Vanguards are playable, but nobody can reach them the way a player would.

- Every client boots straight into the grey-box match map. There is no launcher, front end, menu or UI framework, and ADR-004's client states exist only on paper.
- The client handoff (`UVeyraSessionSubsystem`) is a straight line: redeem the launch code, wait for a match, travel. Any failure quits the game.
- The only way to create a match is the local, unauthenticated `POST /v1/dev/matches`. The assignment carries no mode and no Vanguard, so Vanguards still come from developer data (ADR-008 §8).
- Nothing brings a player back after a match.
- Canon leaves open the Custom start procedure, the tutorial for developer accounts, every champion-select duration, and what happens when a pick timer runs out.

M6 builds the flow the author asked for: open the launcher, sign in, the client starts, go to Play, choose a mode, start a game. Until it exists, `Game/Scripts/Play.ps1` starts a local match with one command: a development server, a chosen Vanguard, optional bots and no loading wait.

## Decision

### 1. Scope and delivery

- **M6a (one pull request):** launcher → sign in → starter choice → Play → Custom → Practice → champion select → match → End Custom Match → verified results → back to the shell.
- **M6b (a stacked pull request):** party → queue → Match Found → Casual Select champion select with several clients, then a match and results.
- **Deferred:**
  - custom lobbies with invites, team slots and host-chosen AI; bots that fight back;
  - Draft Pick, Co-op and Ranked;
  - the real tutorial and entitlements purchase;
  - the launcher's install, patch and self-update, and remembered login;
  - rejoining a running match (ADR-007 open item);
  - victory conditions;
  - Home content, Shop, Test Skin, social panels, chat, and skins in select.

### 2. The client-state coordinator lives in VeyraServices

VeyraServices is the only module that talks to the backend (ADR-007 §12), so the coordinator ADR-004 calls for lives there, not in the UI and not in a GameInstance subclass.

- **`FVeyraClientFlow`** (plain C++) owns:
  - the game session, in memory only;
  - an explicit state enum and a pure transition function;
  - the pollers and the intents;
  - an immutable snapshot for presentation.
- It reaches the backend and the engine only through two injected interfaces, a backend transport and a host (clock, travel, the handshake pipe), so tests drive the whole flow without a world or a server.
- **`UVeyraClientFlowSubsystem`** hosts it in a client's GameInstance. It exposes the snapshot, a change event, whether an intent is allowed now, and the intents themselves: choose starter, start practice, hover, lock, reconnect, continue from results, quit.
- **The UI observes and asks; the coordinator and the backend decide.**
- **It replaces `UVeyraSessionSubsystem`.**
  - The game never quits by itself. A failure shows its reason with Retry, and Quit is the player's choice.
- **Start-up order**, after sign-in:
  1. A live match leads to **Reconnect-only** (UX-17). It outranks everything else, the tutorial included.
  2. A select in progress resumes.
  3. No starter yet leads to the starter choice.
  4. Otherwise the shell.
- **States:**
  - signing in, sign-in failed, session ended;
  - starter choice;
  - shell;
  - practice select;
  - match starting, connecting, in match;
  - returning, awaiting verified results, results;
  - reconnect-only.
  - M6b adds party, queue and Match Found.

### 3. The front end and travel

- **`L_FrontEnd`** is generated by a commandlet, like `L_Greybox`, and becomes `GameDefaultMap`.
  - It is an empty world whose game mode, `AVeyraShellGameMode`, spawns no pawn. It lives in VeyraServices, so the map references nothing client-only.
  - `ServerDefaultMap` stays `L_Greybox`.
- **No transition map and no GameInstance subclass.** Every travel is absolute: front end → the assigned server's address, then back to the default map.
- **Leaving a match.**
  - The GameState now notifies phase changes. When a match reaches **Ended**, the coordinator clears the join ticket, travels to the front end and waits for **verified results** from the backend (UX-15).
  - It never presents the server's replicated end as the result.
  - If the server quits or the connection fails first, the engine's fallback to the default map is treated the same way. If the backend still reports the match live, the player gets Reconnect-only.

### 4. Menus: UMG built in C++

- The shell screens and the in-match menu are `UUserWidget` subclasses. Each builds its widget tree in C++, with no widget Blueprints, so every screen is reviewable text (ADR-006 §6).
- They live in `VeyraUI` (Presentation, client only), which gains UMG, Slate and VeyraServices as dependencies.
- **CommonUI is not used.** It needs binary input-data assets, and a mouse-driven grey box needs none of its gamepad routing.
- **The in-match menu** opens with a key from input settings. It offers Resume and, only to the host of a practice match, **End Custom Match** with a confirmation.
- **Style and keys** are presentation settings in `DefaultGame.ini` and `DefaultInput.ini`, validated like `UVeyraGreyboxSettings`.
- **What players read about Vanguards** (names, titles, and each ability's and passive's name and one-line description) is a string table, `Game/Text/VeyraText.csv`: reviewable, localisable text beside `Game/Tuning`, with no numbers, which stay in tuning. The HUD's panel and the shell read it; developer content without text shows its content ID. A test requires text for every Playable Vanguard's kit.
- **The HUD shows an empowerment waiting for the next basic attack.** The basic-attack component replicates which ability's empowerment waits and until when; the server's rules stay where they are.

### 5. The launcher

**Code layout** (`Launcher/`, Tauri v2, ADR-005 L2):
- A Rust core does all HTTP, so the web view makes no cross-origin calls.
- The UI is static HTML, JavaScript and CSS, with no Node build step.
- A thin Tauri app calls the core.
- A headless command-line launcher uses the same core for automation.

**Build and scope:**
- It builds with `cargo build`. Bundling, the installer and the signed updater arrive with install and patching, ADR-005's build-order step 6.
- It signs in with the developer login until an identity provider is chosen (ADR-005 H3), so it shows an account picker and no password.
- Remembered login waits for a real identity provider.

**The launch handshake**, amending ADR-005 L3:
1. The launcher starts the game with its standard input and output as pipes.
2. The game writes `veyra-handoff/1 awaiting-launch-code` on its standard output once it is ready to read.
3. Only then does the launcher request the launch code and write it to the game's standard input.
4. The game answers `veyra-handoff/1 signed-in`, or `veyra-handoff/1 failed <code>`.
5. The launcher exits on success (ADR-005 L4). Otherwise it shows the error and offers Retry.

**Rules for the handshake:**
- The markers never carry a secret.
- A game whose standard output is not a pipe still reads its code as before.
- A contract file shared by the C++, Go and Rust tests fixes the lines.

**Why:** the launch code lives only seconds, and a cold engine start can outlast it. The code's lifetime is unchanged; it now starts counting once the game is ready. `veyra-devlaunch` and `Smoke.ps1 -Handoff` use the same handshake.

### 6. Onboarding and which Vanguards a player may pick

- **The tutorial is stubbed** (Account §1).
  - After sign-in, a one-time screen says the tutorial is coming and asks the player to choose a starter.
  - The starter is owned permanently, and choosing it completes the tutorial requirement.
  - Developer accounts pass it like anyone else.
- **Available Vanguards are owned plus rotation** (Modes §3), limited to released Vanguards.
  - Canon's 12-slot weekly rotation cannot run until 12 Vanguards are released.
  - Until then, a **stand-in rotation** in the backend's configuration holds every released Vanguard. It is provisional.
- **The backend owns entitlements and onboarding**, in a new account domain.
- **The catalog.** The backend lists released Vanguards and starters in its configuration.
  - `Vanguards.json` marks each Vanguard `Playable` or `Developer`; `test_vanguard` is Developer.
  - A backend contract test reads `Vanguards.json`, so the two lists cannot drift.

### 7. Custom practice

- **It is a custom match, not a matchmaking mode** (Custom §1). It has its own configuration section, so party routes can never queue it.
- **Starting it.**
  - Only a player who has passed the tutorial, and has no match, select or queued party, can start it.
  - It opens a practice champion select at once, with no lobby. The player is alone on the configured side.
- **Ending it.** The match is open-ended (Custom §1, §4) and ends only by **End Custom Match**.
  - That is a host command the match server validates: practice rules, and the host only.
  - The result is **`host_ended`** with no winner, a new end reason valid only for practice.
- **Bots, so the player has targets** (Custom §1: AI participants; added after the author first played, 2026-09-27).
  - The backend's practice settings list them (`customPractice.bots`: a side and a released Vanguard each, with `playersPerSide` bounding each side, the host included). Every practice match gets them; they are stored with the match and sent in its assignment (§9).
  - The match server adds them when preparation begins, on their sides as their Vanguards. They are no accounts: no ticket, and no place in the result.
  - For now a bot is a target: it wanders near the middle of the map (`Match.json` `bots`) and does not fight back. Host-chosen and fighting AI come with custom lobbies and Co-op.
- **Amended by [ADR-011](ADR-011-battleground-runtime.md) §14:** practice plays on the battleground, with its structures, and has no victory condition. A destroyed Prime Well stays destroyed and the match goes on until End Custom Match.

### 8. Champion select belongs to the trusted services

- A select is a backend session with its own server-side countdown. Its states are picking → starting → started, or cancelled with a reason.
- **Hover and lock:**
  - A player hovers and locks a Vanguard from their available set. A lock is permanent (Battleground §15).
  - The backend validates every choice.
- **Creating the match:** once everyone has locked, the backend creates the match **exactly once**, with each player's locked Vanguard.
- **Practice select:** one player and no enemy.
- **M6b's Casual Select** (Modes §1):
  - one simultaneous pick phase with no bans;
  - picks unique across both teams;
  - hovers visible only to teammates, locks visible to everyone (Battleground §15).
- **Clients poll the select**; there is no push channel yet.

### 9. Match creation and assignment v2

- **Matches are now created from a specification**: mode, rules (Standard or Practice), the host for practice, and each seat's account, side and Vanguard.
  - The select creates them.
  - The development route stays for scripts, and now needs a Vanguard per participant.
- **Assignment v2** (amending ADR-007 §5) adds:
  - the mode;
  - the rules;
  - the practice host;
  - each participant's Vanguard;
  - the bots, each a side and a Vanguard. Only a practice match has any, and they count toward each side's size.
  - The game's schema, the golden example and the backend's contract test change together.
- **The server takes each participant's Vanguard from its roster** (amending ADR-008 §8).
  - `developerMatch.vanguards` and `-VeyraVanguard=` apply only to development servers without an assignment.
  - The server refuses an unknown Vanguard, and a Developer Vanguard in Shipping.
- **The result** (amending ADR-007 §7) gains `host_ended`.
- **Results for players:** they read their own match's verified result through a new participant-only route.

### 10. M6b: queue and Match Found

- **The matchmaker** is a backend loop. It forms matches from queued parties, oldest first, and never splits a party.
- **Team size.** Canon's team size is 5. A local, provisional configuration sets Casual Select to one human per team, so two clients can test the flow.
- **Match Found** (Parties §3):
  - every player must accept before a deadline from configuration;
  - a decline or timeout removes the decliner's party from the queue with Ready reset;
  - accepters return to the queue.
- **Other modes** show as not yet available.
- **The client** reads the modes and polls the party while in the shell, since there is no push channel. A party that is found enters a Match Found state, which holds every other intent until the player answers. A matchmade select offers Leave, which cancels it for everyone as a dodge.

### 11. Provisional answers where canon is silent

1. **A pick timer that expires without a lock** locks the player's hovered Vanguard. With no hover, the select is cancelled: practice returns to the shell, and in M6b it counts as leaving.
2. **The starter pool** is every released Vanguard (canon: try 3–5). Choosing a starter completes the tutorial requirement.
3. **The stand-in rotation** holds every released Vanguard until 12 exist.
4. **A practice select** cannot be cancelled by the player; only its timer ends it.
5. **Reconnect-only** offers Reconnect, and says plainly that the server still refuses rejoining (ADR-007 open item). It waits there until the match ends, then shows results.
6. **M6b, after a Match Found decline:** accepters keep their original queue time. A dodge is recorded, with no timed penalty, since Match Flow §2 defines no schedule.
7. **M6b:** everyone is Not Ready once a match starts (UX-15).
8. **M6b, a match found that did not go ahead** is explained by the player's own answer, then by their party. Either they declined, or someone else did not accept and they are queued again in their place, or their party left the queue. Nobody learns who declined.
9. **M6b, a standard match has no victory condition yet.** Outside Shipping, the in-match menu offers End Match (Developer), behind a confirmation. Its result says a developer ended it, with no winner. *Amended by [ADR-011](ADR-011-battleground-runtime.md) §13:* destroying the other side's Prime Well now wins a standard match, and the results screen says Victory or Defeat; End Match (Developer) stays for development.
10. **M6b, the grey box shows Match Found in place of the page**, not as an overlay above it. It blocks the same things, and the page returns as it was.
11. **M6b, a block placed after a match was found** stops that match before it exists (Parties §6: "all subsequent … match assembly"). Blocks are checked again, under the locks a block takes, when the last player accepts and when every pick is locked. Nobody is at fault, and every party returns to the queue in its place. The reason, `no_longer_matched`, names no block, so no player learns of another's. Once a select has begun starting its match, the match goes ahead, as a live match does.
12. **M6b, a party one of whose members is in a match or a champion select cannot queue** (`member_busy`). The matchmaker takes such a party out of the queue if it got there anyway.
13. **M6b, the matchmaker's search** backtracks, so it finds every grouping of whole parties that fits. When blocks make it combinatorial, a configured limit on its steps per party and pass bounds it; a party whose search runs out waits for the next pass. Canon leaves scale controls to future design (§6).

### 12. Values are data

| Value | Owner | Status |
|---|---|---|
| Released Vanguards and starters; rotation slots (12) and the stand-in | backend configuration | released is contract-tested; starters and the stand-in are provisional; 12 is canon |
| Practice pick duration (30 s) and the host's side | backend configuration | provisional |
| Practice bots (four enemies, one of each released Vanguard) and players per side (5) | backend configuration | provisional; 5 is canon's team size |
| How a bot wanders: how often it picks a point (4 s), and how far from the middle (600 units) | `Match.json` `bots` | provisional |
| Match Found accept duration (15 s); Casual Select pick duration (60 s); select presence timeout (10 s); the local 1v1 team size | backend configuration | provisional; canon gives no values, and its team size is 5 |
| The matchmaker's search limit (10 000 steps per party and pass) | backend configuration | provisional scale control |
| Select, party, Match Found, results and reconnect polling; retries; how long to wait for results | `UVeyraServicesSettings` | operational |
| Menu style and key; HUD colours | presentation settings | presentation |
| Names, titles and descriptions of Vanguards, abilities and passives | `Game/Text/VeyraText.csv` | text, not tuning |
| How long the launcher waits for the game to be ready, and for sign-in | launcher configuration | provisional |

Every configuration is parsed strictly: each field is required, and an unknown field is an error.

## Consequences

- A player reaches a match the way canon describes, and the flow can be tested end to end: a scripted client drives the same intents the UI uses (`Smoke.ps1 -Flow`).
- Champion select, entitlements and match creation are trusted-service decisions. The client and the match server only carry them out.
- The client no longer quits on a backend or network failure. Every failure has a screen.
- The handshake changes what the launcher, `veyra-devlaunch` and the smoke scripts do. Old clients still work with a code written at once.
- Assignment v2 breaks match-server images built before it. The smoke scripts already rebuild the image.
- `Play.ps1` now opens the launcher and plays the real flow; `-Direct` keeps the fast developer path, a match with bots and no launcher.

## Implementation and evidence (M6a, 2026-09-27)

**Backend.**
- New domains: `catalog` (released Vanguards, starters and the stand-in rotation, from configuration), `account` (onboarding and entitlements) and `selection` (champion select and Custom practice). Migrations `0004_match_rules`, `0005_account`, `0006_selection` and `0007_match_bots`.
- `match` creates matches from a specification, with the rules, the practice host and each seat's Vanguard, and accepts `host_ended` only for practice. A select creates its match exactly once, through a unique select ID.
- The routes in §6–§9, and the development routes the launcher and scripts use (`GET /v1/dev/accounts`, `POST /v1/dev/accounts/{name}/reset-onboarding`).

**Game.**
- `VeyraServices`:
  - `Client/`: `FVeyraClientFlow`, `IVeyraClientIntents` and `UVeyraClientFlowSubsystem`, which replaces `UVeyraSessionSubsystem`. The player API's parsers are in `VeyraBackendProtocol`.
  - `FrontEnd/`: `AVeyraShellGameMode`. `L_FrontEnd` is generated by `UVeyraFrontEndMapCommandlet` (`Game/Scripts/BuildFrontEndMap.ps1`).
  - The launch handshake's writer, and its contract, `Contracts/LaunchHandshake.json`.
- `VeyraMatch`: the match rules and host, End Custom Match on the server, and the GameState's phase event. The server takes each Vanguard from the roster; `Vanguards.json` v3 marks each Vanguard Playable or Developer. `Bots/`: `UVeyraBotWanderComponent`, a practice bot's behaviour; the GameMode adds the assignment's bots when preparation begins (`Match.json` v3 adds `bots`).
- `VeyraUI`: `Shell/` (the screens, their view models, the style), `Match/` (the in-match menu and its key), and `Text/` (`VeyraContentText`, the string table's reader). The grey-box HUD names each ability and passive, says what it does, and shows a waiting empowerment.
- `VeyraDeveloper`: `VeyraSmokeFlowSubsystem`, a scripted player that clicks the shell's and the menu's buttons.

**Launcher.** `Launcher/`: the core, the window, `veyra-launch-cli`, and `veyra-fake-game` for its tests. `Package.ps1` writes `VeyraBuild.json`, the build manifest the launcher reads. `Launcher/Check.ps1` and `.github/workflows/launcher.yml` run its checks.

**Scripts.**
- `Play.ps1` opens the launcher by default, and `-Direct` keeps the path without it.
- `Smoke.ps1 -Flow Practice` plays the flow; `-Launcher Script|Cli` chooses who plays the launcher's part, and `-Screenshot` saves each screen.
- `Smoke.ps1 -Handoff -Practice` plays a practice match through the handoff.

**Evidence.**
- Unit, world and network tests: 458 Unreal automation tests pass, among them `Veyra.Services.ClientFlow.*`, `Veyra.UI.*` (with `ContentText.*`), `Veyra.Match.HostedAssignment.*` and `Veyra.Net.HostedMatch.*` and `.HostedPractice.*` (bots join their sides as their Vanguards and wander near the middle; the result names only players). The Go tests pass, with Postgres. The launcher's 26 Rust tests pass: 18 in the core, and 8 end to end with a fake backend and a fake game.
- End to end, each against the backend and the Linux server container:
  - `Smoke.ps1 -Flow Practice`, with the launcher's part played by the script and then by `veyra-launch-cli`. A new account chooses a starter, starts practice, hovers and locks, plays, ends the match from the menu, sees the verified result (`host_ended`, no winner) and returns to the shell. The client then closes cleanly.
  - `-Handoff`, `-Handoff -Practice` and `-Vanguards cairn,qazharr,oriel,bryn`, as before M6.
  - `-Flow Practice -Screenshot` renders each screen in a window.
- The launcher window was driven through WebView2's DevTools port from `Play.ps1`: the accounts appeared, and Play signed in. The launcher closed, and the game reached the starter choice in its window. With no backend, the window shows the problem.
- The author played the flow by hand on 2026-09-27: launcher, sign-in, Play, Custom, Practice, select, the match, End Custom Match, results and the shell all worked. With nothing to hit, the kits were hard to read, and the HUD showed through the Esc menu; the practice bots, the HUD's text and the HUD overlay followed, and `-Flow Practice` now checks that the server added the configured bots.

**Settled while implementing.**
- **Tauri without its CLI** needs its `custom-protocol` feature. Without it, a plain `cargo build` produces a development build that looks for a development server.
- **Windows children inherit every inheritable handle.** The launcher clears inheritance on its own standard handles, or a script reading the launcher's output would wait for the game too.
- **A GameInstance's subsystems deinitialize in no set order.** The coordinator's subsystem announces its end while its client is still valid, and the shell's subsystem lets go of the client then. Without that, every client crashed on exit after its work was done. The smoke scripts now fail a client that crashes, even after it passed.
- **A UI-only input mode focuses a widget**, so the shell's screen and the in-match menu are focusable.
- **UMG widgets build under `-nullrhi`**, so the screens are tested headless; `-Screenshot` checks how they look.
- **The engine's HUD post-render event passes the debug canvas**, which is drawn above every widget, so the grey-box HUD showed through the menu. It now draws through an overlay actor the local HUD renders on its own canvas (`AVeyraHudOverlay`), under the widgets.

## Implementation and evidence (M6b, 2026-09-27)

**Backend.**
- `matchmaking`: a loop that locks queued parties with `FOR UPDATE SKIP LOCKED` and groups them oldest first. It never splits a party, and keeps blocked players off each other's match. Match Found takes accepts, declines and timeouts. Each mode names its matchmaking, `casualSelect` or `notImplemented`.
- `party`: the statuses idle, queued, found and selecting, and the time in the queue.
- `selection`: Casual Select, with sides, unique locks, hovers private to the team, presence by polling, and Leave as a dodge. When a matchmade select ends, it settles the parties in the same transaction.
- Migrations `0008_matchmaking`, `0009_casual_select` and `0010_no_longer_matched`.
- New routes: `GET /v1/me/match-found`, `POST /v1/me/match-found/accept` and `/decline`, and `POST /v1/me/select/leave`. The party gains `queuedSeconds`, and each mode its `matchmaking`.

**Game.**
- `VeyraServices`:
  - the coordinator's shell reads the modes and polls the party;
  - the new state `MatchFound`;
  - the intents `SelectMode`, `SetReady`, `FindMatch`, `CancelQueue`, `AcceptMatch`, `DeclineMatch` and `LeaveSelect`;
  - the transport gains `DELETE`;
  - `PartyPollIntervalSeconds` and `MatchFoundPollIntervalSeconds` are new settings.
- `VeyraUI`:
  - mode cards;
  - the party panel, with the queue's time and "Estimate unavailable" (UX-2);
  - Match Found;
  - champion select with both teams apart, picks another player locked, and Leave;
  - End Match (Developer).
- `VeyraDeveloper`: the scripted player's `casual`, `decline` and `requeue` scripts, and `opponent`, a sparring partner.

**Scripts.**
- `Smoke.ps1 -Flow Casual` and `-Flow CasualDecline` run two clients.
- `Play.ps1 -Opponent` plays a matchmade 1v1 against the sparring partner; with `-Check`, it runs `-Flow Casual`.

**Evidence.**
- Unreal automation tests, among them:
  - `Veyra.Services.MatchmakingApi.*`;
  - `Veyra.Services.ClientFlow.{QueueCancel, MatchFound*, LeavingACasualSelectDodges, AnOpponentLeavingRequeues, APracticeSelectCannotBeLeft}`;
  - `Veyra.UI.Shell.{MatchFoundModel, TeamSelectModel, PartyAndModeModels}`;
  - `Veyra.UI.ShellScreens.{PlayQueuesTheParty, MatchFoundBlocksTheShell, ACasualSelectShowsTheTeamsAndOffersLeave}`;
  - `Veyra.UI.MatchMenu.DeveloperEndVisibility`.
- The Go tests pass with Postgres, among them concurrent matchmaking passes and the HTTP path from queue to select.
- End to end, against the backend and the Linux server container, with `veyra-launch-cli` signing each client in:
  - `Smoke.ps1 -Flow Casual`. Two clients choose Casual Select, ready up, find a match and accept it, then lock different Vanguards. One walks and ends the match from its menu. Both see the verified result (`developer_request`, no winner, both joined and connected at the end) and return to the shell. The backend removes the server. With `-Screenshot`, each new screen renders in a window.
  - `Smoke.ps1 -Flow CasualDecline`. The second client declines once the first has accepted. The decliner is back in the shell and out of the queue. The first is queued again in its place, then leaves the queue. No match is created.
  - `-Flow Practice` still passes.
  - The sparring partner against a scripted player: it queued, accepted, locked a different Vanguard after the player locked, and queued again after the match.

**Settled while implementing.**
- **The backend reports only a Match Found that waits for answers**, so the client learns how one ended from where the player is afterwards: in its select, or in the shell. There, the first read of the party says whether they are queued again. A player who had not answered yet when someone else declined is queued again; they did not miss it.
- **Party answers are numbered**, so a poll overtaken by an intent's answer is not shown.
- **`Package.ps1` stages the client binary last built**, so a client change needs `Build.ps1 -Target VeyraClient` first.

## Amendment (2026-09-28): champion select in League's layout

The author asked for champion select to look like League's. §4 still holds: the screen is UMG built in C++, with no widget Blueprint. What changed:

- **The layout, League's:**
  - The roster runs across the top as a bench of portraits.
  - The countdown sits between two bars that drain toward it; the backend's select now carries `pickSeconds`, the timer's full length (`Deadline − CreatedAt`).
  - The player's team runs down the left: each seat shows a round portrait, the Vanguard, the player and the status, with your own starting Flux Spells beside your portrait.
  - The enemy team runs down the right, showing only its locks, as the backend already rules.
  - The shown Vanguard (your lock, else your hover) is large in the middle, framed. It also fills the screen behind everything, dimmed (UX 27). Its name and title sit under it, with **View Abilities**, which lays the passive and Q, W, E and R over the art.
  - Along the bottom: Your Match Setup (UX 38) where League keeps its chat, the two Flux Spell tiles and Lock In in the middle, and the mode in the corner.
  - Each spell tile opens a picker over everything, as League's summoner spells do. It lists None and each roster spell with its description, and the slot's threshold (UX 36).
- **The art is imported, not hand-made:**
  - `Game/Scripts/BuildVanguardArt.ps1` converts each Playable Vanguard's `ConceptArt/Vanguards/<id>/hero.webp` to PNG, since the engine decodes no WebP.
  - It then runs `UVeyraVanguardArtCommandlet`, which saves `/Game/Veyra/UI/Vanguards/T_<id>_Hero`: a UI texture with no mips and no streaming, BC7.
  - The textures are lockable LFS assets, reproducible from the illustrations, like the generated maps.
  - `/Game/Veyra/UI` is always cooked, because the shell loads the art by path (`VeyraShellArt`).
- **Portraits are crops, not separate images:**
  - A round portrait is the hero texture drawn as a `RoundedBox` brush at half-height radius, with a UV crop around the Vanguard's face.
  - Each face's position and crop size are presentation data in the shell style (`VanguardPortraits`, with a `DefaultPortrait`), validated like the rest.
  - A Vanguard whose art is not imported shows an empty disc and its name.
- **Nothing gameplay moved.** The screen still shows the coordinator's snapshot and asks through its intents. The picker and View Abilities are the screen's own state, as its page is.

## Amendment (2026-09-28): a match takes the screen

The author asked for the game to go fullscreen for a match and come back to its window for the results, as League's client and game do. Canon has only the Display Mode setting (Settings & Accessibility Bible 166: Windowed, Borderless Fullscreen, Fullscreen), so:
- **The pre-game client keeps its window**, which the launcher opens with `-windowed`.
- **A match takes the screen** from the loading after champion select (Match Starting) through Connecting and the match itself. It uses `UVeyraDisplaySettings`' `MatchDisplayMode` (Borderless Fullscreen by default) at the resolution of the monitor its window is on, ignoring the launch's `-windowed`.
- **The window comes back** when the match is over (Returning, then the results), at the size, mode and position it had.
- **Ownership:** `UVeyraMatchDisplaySubsystem` in VeyraUI follows the coordinator's state and decides nothing about the flow. It does nothing without a window.
- **For scripts:** `-VeyraMatchDisplay=<mode>` overrides the setting for one run. The smoke tests stay Windowed unless asked; `Smoke.ps1 -MatchDisplay BorderlessFullscreen` checks the switch both ways.
- **Later:** the player's own choice joins the Settings menu, with Settings 92's Keep/Revert.

## Amendments to earlier records

- **ADR-004:** the coordinator is `FVeyraClientFlow` in VeyraServices. The front end is a generated map with absolute travel, and menus are UMG built in C++.
- **ADR-005 L3:** the launch handshake (§5).
- **ADR-007:**
  - §5: assignment v2;
  - §7: `host_ended`;
  - §10: select-created matches, and the participant results route.
- **ADR-008 §8:** assigned matches take Vanguards from the roster.

## Open items

- Custom lobbies with invites, team slots and host-chosen AI, whose screen falls under the UX-93 pause (Custom §7); bots that fight back.
- The real tutorial, starter trials and the weekly rotation.
- Draft Pick bans and turn order; Co-op AI.
- Queue-dodge penalties; the select trade protocol.
- Rejoining a running match.
- Surrender (Match Flow §8). The Prime Well's victory arrived with [ADR-011](ADR-011-battleground-runtime.md) §13.
- Remembered launcher login, and install and patching.
- A push channel instead of polling.
- Presence while queued. A client that closes while its party is queued leaves the party queued until its next Match Found goes unanswered, which takes it out (Parties §3). Its opponent waits for that deadline.
- The real Match Found overlay above the page, and a queue estimate (UX-2).

## Alternatives considered

- **A GameInstance subclass as the coordinator:** ADR-004 rejects a god GameInstance, and the coordinator needs no engine class of its own.
- **The coordinator in VeyraUI:** the UI would talk to the backend and decide state, against Architecture §2 and ADR-007 §12.
- **A new client-flow module and layer:** it would separate the HTTP client from its only client-side user. Worth revisiting when Test Skin or Replay arrive.
- **UMG widget Blueprints or CommonUI:** binary assets agents cannot review or edit (ADR-006 §6).
- **Champion select on the match server:** the select must finish before a server exists, and choices are validated against entitlements the backend owns.
- **A longer launch-code lifetime instead of the handshake:** it widens the window in which a leaked code works, and still races a slow start.
- **A Node-built launcher UI:** a JavaScript toolchain for a few static screens.
