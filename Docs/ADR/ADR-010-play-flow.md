# ADR-010: The play flow: launcher, client states and Custom practice

**Status:** Proposed. It becomes Accepted when the author merges the M6a pull request that adds it.  
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
  - custom lobbies with invites, team slots and bots;
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
  - each participant's Vanguard.
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

### 11. Provisional answers where canon is silent

1. **A pick timer that expires without a lock** locks the player's hovered Vanguard. With no hover, the select is cancelled: practice returns to the shell, and in M6b it counts as leaving.
2. **The starter pool** is every released Vanguard (canon: try 3–5). Choosing a starter completes the tutorial requirement.
3. **The stand-in rotation** holds every released Vanguard until 12 exist.
4. **A practice select** cannot be cancelled by the player; only its timer ends it.
5. **Reconnect-only** offers Reconnect, and says plainly that the server still refuses rejoining (ADR-007 open item). It waits there until the match ends, then shows results.
6. **M6b, after a Match Found decline:** accepters keep their original queue time. A dodge is recorded, with no timed penalty, since Match Flow §2 defines no schedule.
7. **M6b:** everyone is Not Ready once a match starts (UX-15).

### 12. Values are data

| Value | Owner | Status |
|---|---|---|
| Released Vanguards and starters; rotation slots (12) and the stand-in | backend configuration | released is contract-tested; starters and the stand-in are provisional; 12 is canon |
| Practice pick duration (30 s) and the host's side | backend configuration | provisional |
| Match Found accept duration (15 s); Casual Select pick duration (60 s); select presence timeout (10 s); the local 1v1 team size | backend configuration | provisional; canon gives no values, and its team size is 5 |
| Select, results and reconnect polling; retries; how long to wait for results | `UVeyraServicesSettings` | operational |
| Menu style and key | presentation settings | presentation |
| How long the launcher waits for the game to be ready, and for sign-in | launcher configuration | provisional |

Every configuration is parsed strictly: each field is required, and an unknown field is an error.

## Consequences

- A player reaches a match the way canon describes, and the flow can be tested end to end: a scripted client drives the same intents the UI uses (`Smoke.ps1 -Flow`).
- Champion select, entitlements and match creation are trusted-service decisions. The client and the match server only carry them out.
- The client no longer quits on a backend or network failure. Every failure has a screen.
- The handshake changes what the launcher, `veyra-devlaunch` and the smoke scripts do. Old clients still work with a code written at once.
- Assignment v2 breaks match-server images built before it. The smoke scripts already rebuild the image.
- `Play.ps1` stays useful for fast developer play once the real flow exists.

## Amendments to earlier records

- **ADR-004:** the coordinator is `FVeyraClientFlow` in VeyraServices. The front end is a generated map with absolute travel, and menus are UMG built in C++.
- **ADR-005 L3:** the launch handshake (§5).
- **ADR-007:**
  - §5: assignment v2;
  - §7: `host_ended`;
  - §10: select-created matches, and the participant results route.
- **ADR-008 §8:** assigned matches take Vanguards from the roster.

## Open items

- Custom lobbies with invites, team slots and bots, whose screen falls under the UX-93 pause (Custom §7).
- The real tutorial, starter trials and the weekly rotation.
- Draft Pick bans and turn order; Co-op AI.
- Queue-dodge penalties; the select trade protocol.
- Rejoining a running match.
- Victory conditions and the winner.
- Remembered launcher login, and install and patching.
- A push channel instead of polling.

## Alternatives considered

- **A GameInstance subclass as the coordinator:** ADR-004 rejects a god GameInstance, and the coordinator needs no engine class of its own.
- **The coordinator in VeyraUI:** the UI would talk to the backend and decide state, against Architecture §2 and ADR-007 §12.
- **A new client-flow module and layer:** it would separate the HTTP client from its only client-side user. Worth revisiting when Test Skin or Replay arrive.
- **UMG widget Blueprints or CommonUI:** binary assets agents cannot review or edit (ADR-006 §6).
- **Champion select on the match server:** the select must finish before a server exists, and choices are validated against entitlements the backend owns.
- **A longer launch-code lifetime instead of the handshake:** it widens the window in which a leaked code works, and still races a slow start.
- **A Node-built launcher UI:** a JavaScript toolchain for a few static screens.
