# ADR-077: Smooth bodies everywhere, and the player's own body drawn ahead along its path

**Status:** Proposed. On 2026-10-13, after playing 0.2.6 hosted on their own PC, the author reported "rubberbanding jitter" and asked for the next step to "make it feel as if im playing it client authoritive when its really server authoritive. like flawless movement", then: "please fix that problem. its the biggest problem and keeping the game from being playable." This record amends ADR-006 §7 (its M3 author ruling, no client prediction) and ADR-067 §2, for the player's own body's drawing only. It becomes Accepted when the author merges the pull request that adds it.
**Date:** 2026-10-13
**Related:**
- [ADR-006](ADR-006-unreal-project-scaffold.md) §7: the server moves every unit, from orders, through its own controller. That stays.
- [ADR-065](ADR-065-first-match-feedback-waves-spires-rewards-and-patch.md) §12: drawn bodies ease between the server's updates.
- [ADR-067](ADR-067-responsive-own-body-and-skillshot-paths.md) §1–§2: ease times, and the player's own body turning toward its order at once.
- [ARCHITECTURE.md](../../ARCHITECTURE.md) §1.1: clients may predict presentation; prediction is never authority, and must be reconcilable.

## Context

The author's round trip was near zero, since the server ran on the same PC, yet movement rubber-banded and jittered. The code shows why.

- **Machines slid bodies past the truth.** A machine that only shows a unit steps its capsule to each server update, 30 a second. Between updates, the engine slid the capsule on at the last velocity it was sent, then snapped it back to the next update. The drawn body eased after the capsule, so every stop and every turn overshot and was pulled back.
- **The time-based ease had no clock.** The engine can ease a body evenly from one update to the next over the server's own time between them. That needs the server to stamp each update with its time, and the server never sent the stamps.
- **The player's own body waited.** It is drawn from the server's updates like any other body, so after a click it stood for a round trip plus up to a server tick. ADR-067's lead only turned it and set its legs running.

## Decision

### 1. Every body is drawn between the server's last two updates, never past the latest

- The server stamps each update of a body with its own time: `UVeyraMovementComponent` sets `bNetworkAlwaysReplicateTransformUpdateTimestamp`.
- On a machine that only shows the body, its capsule holds the place the server last sent and does not slide on at its last velocity. Its velocity stays as sent, for the animation.
- The drawn body eases from where the last update drew it to the newest, over the server's time between the two (Linear smoothing). ADR-067's ease times become the least it eases over.
- The cost: other bodies are drawn about one update behind the server, about 33 ms for Vanguards and 100 ms for Fluxborn. In exchange they never overshoot.

### 2. The player's own body is drawn ahead along its path

On the player's own machine only, and in presentation only:

- **When it moves.** The moment an order is given, the drawn body walks toward the order's goal at the body's own speed. That is a move's point, an Attack Move's, or an attack's target beyond its reach.
- **How far ahead it is drawn.** Ahead of where its smoothing has it by what the order will have done by now. That is the smoothing's delay, plus `OwnLeadPingShare` of the round trip, plus `OwnLeadServerWaitSeconds` for the server's next tick, at most `OwnLeadMaxSeconds`.
- **Its limits.** It is never drawn:
  - past what it could have walked since the click;
  - past where it stops (a move where the server's path following accepts it as arrived; an attack where the server stops it to attack, its range to that target edge to edge, with what the target's statuses from it add: `VeyraHud::AttackReachTo`, by `VeyraTargeting::CentreReach`);
  - more than `OwnLeadMaxDistance` from its smoothed place;
  - backward while it leads.
- **Catching up.** It may move at up to `OwnLeadCatchUpScale` times its speed to regain its lead.
- **The path it follows.** A straight line to the goal first. Then the server's own path, once it arrives (§3); the step between the two closes evenly and is gone once `OwnLeadSwitchSeconds` have passed. It faces the way its path runs, turning at `OwnLeadTurnDegreesPerSecond`.
- **When the lead goes.** Over `OwnLeadBlendSeconds`, when:
  - the server's body stands, below `OwnLeadStandingSpeedShare` of its speed, for `OwnLeadStallSeconds` while its path runs on (a stun, a root, a stop to attack);
  - or the server's body strays more than `OwnLeadOffPathDistance` from the path (other bodies push it);
  - or the body is dead, may not move, or something other than its orders moves it (a displacement, dash, Fear or attach shows as a custom movement mode).

  At the end of its path it waits for the server's body to stand there, then lets go with nothing left to pull back.
- **Facing only.** An attack's target already within reach is only faced, as ADR-067 §2 has it.
- **The server stays in charge.** Only the drawn mesh moves, through `UVeyraMovementComponent::SetDrawnLead`. The capsule, collision, targeting, damage and every rule follow the server's updates. Nothing the drawn body does reaches the server.

### 3. The server sends the player's own body's path to that player alone

- The controller the player's orders go to now sends the path it follows to that player's `AVeyraPlayerState` (`FVeyraOwnPath`, replicated `COND_OwnerOnly` and sent at once). That is the Vanguard's own, or the controller of a body the player commands in its stead, such as an Echo. The GameMode tells each controller whose orders it takes as it passes an order on, so a Vanguard and a commanded body never send over each other. It sends the path as an order plans it, and again as a chased unit moves.
- The path carries the body it moves and the order it answers: the point a move or Attack Move named, or the unit an attack chases. The player's machine uses only a path that answers its latest order, within `OwnLeadOrderMatchDistance`.
- No other machine receives it, so a Vanguard's destination stays private. A bot's Vanguard sends nothing.
- The player's machine does no pathfinding of its own and builds no navigation data.

### 4. Data

Every value is in `[/Script/VeyraUI.VeyraGreyboxSettings]` in `DefaultGame.ini` and validated at start. The arrival tolerance is the orders' own (`Match.json` `orders.arrivalTolerance`).

## Consequences

- Other bodies move smoothly and never snap back, at the cost of about one update of delay.
- The player's own Vanguard answers a click within a frame and walks on without waiting. At round trips up to a few hundred milliseconds it stays within about 100 units of the server's truth.
- It can be drawn a little ahead of where the server has it. At 60 ms of round trip that is about 15 units at 350 speed. A skillshot can hit a body that looked just clear of it; the server's answer stands.
- A stun or a push shows a short slide back, over `OwnLeadBlendSeconds`, where before it showed a snap.
- **Tests:**
  - `Veyra.UI.OwnLead.*` walks the lead through a start, a stop, a stall, a push off the path, a switch to the server's path, an attack's reach and a reversal.
  - `Veyra.Net.VanguardMoveOrders.TheServersPathReachesItsOwnPlayerAlone` and `.AnotherMachineDrawsAWalkingVanguardWhereTheServerLastSentIt` check the path's privacy and that no machine draws a body past its latest update.

## Not decided here

- The server's tick rate stays 30 Hz.
- The project's push-model replication is compiled in but `Net.IsPushModelEnabled` is not set. Enabling it needs its own audit.
- Predicting abilities, dashes and displacement on the player's machine. ADR-009 §6 keeps them server-only.

## Provisional values

| Setting | Value |
|---|---|
| `OwnLeadPingShare` | 0.5 |
| `OwnLeadServerWaitSeconds` | 0.017 (half a 30 Hz tick) |
| `OwnLeadMaxSeconds` | 0.25 |
| `OwnLeadCatchUpScale` | 1.25 |
| `OwnLeadMaxDistance` | 100 |
| `OwnLeadBlendSeconds` | 0.12 |
| `OwnLeadStandingSpeedShare` | 0.2 |
| `OwnLeadStallSeconds` | 0.15 |
| `OwnLeadOffPathDistance` | 80 |
| `OwnLeadSwitchSeconds` | 0.1 |
| `OwnLeadOrderMatchDistance` | 2 |
