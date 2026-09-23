# ADR-003: One Unreal application with isolated client states

**Status:** Accepted  
**Date:** 2026-09-23  
**Approved in:** Pre-Game Client UX Proposals 90–92 (author-approved; paused after Proposal 92)  
**Related:** [Architecture Constitution](../../ARCHITECTURE.md), [Project Structure](../../PROJECT_STRUCTURE.md), [Client & Platform Bible](../Design/Veyra_Client_Platform_Bible_v0.1.md), [Pre-Game Client UX Bible](../Design/Veyra_Pre_Game_Client_UX_Bible_v0.1.md).

## Context

Earlier platform text assumed a persistent pre-game process launching a separate Unreal executable for live gameplay/replay/spectator. The author subsequently decided that Veyra's **pre-game UI itself is Unreal**, and that **Test Skin is a miniature playable Unreal map inside the pre-game client experience**. Building a second process or reproducing Vanguard abilities in a separate preview engine would cause duplicated code, transition overhead and divergent visuals. The user approved **one Unreal application** and controlled state transitions after discussing these consequences.

## Decision

- The **official website** continues to handle account creation and ban appeals. A distinct **launcher** handles login/authentication, native PC installation, required patching and starting the game. The launcher is **not** an independent full pre-game feature suite.
- The installed **Unreal Engine 5.8 application** is one client executable that hosts distinct ordinary pre-game browsing/Shop/social/party, Test Skin, Match Found, committed champion-select, live assigned gameplay, verified results, Reconnect-only, Replay Mode and Spectator Mode states. **No second pre-game-to-game executable launch is required.** A separate *dedicated match server* remains authoritative for live match simulation and outcome.
- Each state has explicit authority and UI/input/resource boundaries. Normal browsing/purchases/party controls and Test Skin are **unavailable during committed select and assigned live-match Reconnect-only state**. Match Found is a priority blocking acceptance overlay during ordinary browsing or test. Gameplay's live HUD and allowed communication do not unlock ordinary pre-game navigation or mid-match party management.
- **Test Skin** launches an isolated test-only Unreal map from an ordinary Shop skin listing and returns to it on exit. Use the actual installed bespoke skin model, animations, ability VFX/SFX, real reusable Vanguard movement/ability primitives and appropriate test dummies. Include controlled Base/Skin and same-Vanguard skin swaps, repeatable test-only abilities and resources. No real match assignment, progression, economy, authoritative outcomes, permanent entitlements or duplicate client-owned Vanguard combat logic.
- Transitions within the **same process** preserve permitted party/queue and original Shop navigation through their proper authoritative/session owners, stop and release ineligible map/UI assets, and prevent Test Skin work from blocking Match Found. Accepted selection leads through truthful Match Starting / Preparing or Loading Gameplay / Connecting to Match into actual playable gameplay **only after valid assignment, required assets and authoritative connection**. Verified completed match transitions to results with ordinary shell restored and no automatic Ready/requeue.
- If the app exits/crashes or live-match connection fails while an assigned match is still live, on restart/return present **Reconnect as the only pre-game player action**. Reconnect targets the **same match and Vanguard**, not a fresh queue; the server alone owns continuing gameplay and AFK/disconnect timing. Do not let UI uptime or process transitions control match authority.
- Asset loading uses installed native assets by default; valid assets may be budgeted/cached, selectively preloaded, version-invalidated and evicted. Optional heavy work yields to Match Found, selection, assigned match and reconnect. Optional per-skin network download/streaming is **not** assumed; launcher verifies and patches required compatible game content.
- Maintain architecture constitution boundaries: C++ owns reusable gameplay/rules; data assets own tuning; Blueprints/presentation remain thin; UI observes state and sends intents; dedicated/trusted services own authoritative match, party, economy, commerce, identity and entitlements. **One Unreal executable does not imply one persistent map, one god GameInstance/controller, server-authoritative preview purchases or duplicated subsystem calculation.**

## Superseded wording

Earlier wording about “launching Unreal gameplay”, “pre-game remains open in the background while Unreal is foregrounded”, “a second Unreal game executable”, or “relaunching Unreal to reconnect” means a **state transition or process restart of the same Unreal application**, as applicable. Replays and spectating also use distinct modes in this same application; their authoritative permissions, delayed-data and retention rules remain unchanged.

The user-approved **player-visible outcomes** of earlier pre-game proposals still hold: Match Found priority, select commitment, verified results, reconnect-only and normal party preservation. This ADR changes the *application/process model*, not their authority.

## Consequences / engineering work left open

- Design per-state ownership, travel/streaming, asset loading, input focus, teardown and restoration with automated transition tests, including Match Found during Test Skin load, accepted assembly, declined/failed assembly, crash/restart during active match, and returning from Replay Mode.
- Define how UI/background client screens are suspended, how trusted state reconnects, and how replay/spectator modes are isolated from an assigned live match, without creating independent parallel full clients or unauthorized routes.
- Profile memory and load behavior and choose exact Unreal level/World travel and session integration only during implementation; the author has **not** selected a particular map streaming, widget, IPC, patcher or plugin stack.
- Preserve existing server-side loading/no-show, reconnect/absence, queue, shop and accessibility rules. No false guarantee of zero loading time.

## Alternatives considered

**Separate persistent pre-game process plus Unreal match process:** Superseded by the author's explicit unified Unreal application decision. It could allow independent window behavior but adds duplicate lifecycle/handoff complexity and is not the chosen default.

**Non-Unreal pre-game UI with emulated skin gameplay:** Rejected because Test Skin requires authentic model/animation/ability execution and the author chose Unreal for the pre-game client.

**One giant persistent game map or controller owning all states:** Rejected as contrary to modular authoritative architecture, performance isolation and predictable resource release.
