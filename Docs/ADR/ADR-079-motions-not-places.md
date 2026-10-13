# ADR-079: Every machine draws bodies from the server's motions, not its places

**Status:** Proposed. After playing 0.2.7 on 2026-10-13, the author reported that ADR-077 made movement more responsive but not smooth: "the character jitters, ability casts feel unreliable, dash/lunge/blink abilities feel" wrong. They asked for the approach to be rethought, chose this direction over the alternatives below, and wrote: "if u need to update the adrs, go for it. i need the game to be playable and smooooth." This record supersedes ADR-077 §1–§3 and ADR-067 §1–§2, and amends ADR-006 §7 and ADR-009 §2. It becomes Accepted when the author merges the pull request that adds it.
**Date:** 2026-10-13
**Related:**
- [ADR-006](ADR-006-unreal-project-scaffold.md) §7: the server moves every unit, from orders, through its own controller. That stays.
- [ADR-009](ADR-009-runtime-combat-primitives.md) §2, §6: forced movement and dashes are the server's; no client predicts an ability. That stays.
- [ADR-067](ADR-067-responsive-own-body-and-skillshot-paths.md) and [ADR-077](ADR-077-smooth-bodies-and-the-players-own-body-ahead.md): what this record replaces.
- [ARCHITECTURE.md](../../ARCHITECTURE.md) §1.1: clients may predict presentation; prediction is never authority, and must be reconcilable.
- [Combat Bible](../Design/Veyra_Combat_Bible_v0.5.md) §26: brief input buffering near the end of action lockouts.

## Context

Bodies were drawn from the server's **places**: thirty a second for Vanguards, ten for Fluxborn. Every machine eased each body from one place to the next, and the player's own machine pushed its body ahead of that with a lead tuned by fifteen settings. Each layer corrected the one under it, and their seams were what the author felt.

- **Jitter.** The engine's time-based ease lets a drawn body run at most 15% of an update interval past the newest place (`SmoothClientPosition_Interpolate`, `MaxClientTimeAheadPercent`), about 5 ms at 30 Hz. Any update later than that halts the body until it arrives, then lurches it on. The hosted server measured 13–15 ms of jitter, so bodies stuttered many times a second at any round trip. The lead was drawn from that stuttering place, so it wobbled with it.
- **Dashes and blinks read as wrong.**
  - A dash was sampled like a walk: 30 places a second, one update behind, plus the round trip. The 4,000 units-a-second Flux Spell moved 133 units between places and showed as a smear.
  - For 0.12 s after a dash began, the fading lead dragged the drawn body up to 100 units back toward the walk it had left.
  - On landing, the server re-planned the walk from a standing start, so the body halted.
  - A blink slid, half slid or snapped by its length, at the engine's 256 and 384 unit limits.
  - A blink also dropped the order it was walking: `Blink` called the controller's `StopMovement` outside its lock guard, so the body stood after every blink.
- **Units started and stopped on ice.** Path following accelerated each unit to its speed and braked it to a halt (the engine's defaults), as the 2026-10-08 playtest felt.
- **Casts were refused when they should have waited.** A cast given during another's Recovery, or the caster's own dash, was refused as Busy, and the refusal could be lost on the way back.

## Decision

### 1. A unit's motion

The server tells every machine that sees a unit its **motion** (`FVeyraMotion`, replicated on `UVeyraMovementComponent`) instead of its place:
- where its centre stood as it began (`Start`), and when, in the server's world seconds (`StartTime`);
- the corners it walks through after that (`Corners`), at its speed (`Speed`);
- its facing as it began (`Yaw`);
- what it is doing (`Kind`): Stand, Walk (its orders: a path, a pursuit, a ride), Forced (a dash, displacement or Fear), or Attached (to a `Host`);
- a set-down serial that changes whenever the body is set down elsewhere at once: a blink, a respawn, a Recall;
- the number of the player's order it answers (`OrderNumber`, §5).

`VeyraMotion::Sample` gives where a motion has its body at any moment. Before its start it stands where it began; after its last corner it stands there, with no velocity, and never slides on. The engine's own place updates are off for these units (`SetReplicatingMovement(false)`): the motion is the only thing that moves them on other machines.

### 2. What the server tells, and when

After each frame's movement, the server compares what the body does with what it last told (`UVeyraMovementComponent::PublishMotion`). It tells afresh, at once (`ForceNetUpdate`), when:
- what the body does changes kind, host or order;
- its path changes;
- its speed changes by more than `SpeedChangeShare`;
- it strays more than `DriftDistance` from where its told motion has it now (pushed by others, a ride's arc, a steer);
- a walk has less than `RefreshSeconds` left of what was told, with more still to walk;
- a standing body turns more than `FacingDegrees`;
- it is found more than `SetDownDistance` from its motion, or it blinked: then it is told as set down.

A walk's corners come from its controller's path following (`UVeyraPathFollowingComponent`, which every unit's controller now uses), with the last moved back to where the engine's own reach test counts it arrived. So every machine walks the body to the very place the server's stops. A walk is told only as far as the body walks in `HorizonSeconds`: no more of a Vanguard's path than a watcher reads from its stride, so its destination stays its own (as ADR-077 §3 kept it). A forced move's path is planned whole as it begins (ADR-009 §2), so it is told whole. A body whose path runs on while others hold it in place, below `StandingSpeedShare` of its speed, is told standing rather than told afresh each frame.

### 3. How every machine draws a body

A machine that only shows the body moves its capsule and mesh every frame to where its motion has it on the server's clock now (§4), less `RemoteDelaySeconds`. Because the body is worked out from the motion rather than eased between places, it moves at its true speed every frame however late or rarely a message comes. It turns at a corner, stops at its stop and lands at a dash's end exactly as the server's does.
- **A new motion** never jumps the body. It is drawn from where it was onto the new motion, the difference closing over `CorrectionSeconds`. A difference beyond `CorrectionSnapDistance` is closed at once.
- **A body set down elsewhere** (a blink) is drawn there at once, never glided.
- **Ground.** It stands on the ground under it (`VeyraGround::StandingAt`), up a slope or down into the river.
- **Facing.** It faces the way it walks, or its told facing while it stands or something else moves it, turning no faster than its turn rate.
- **Stride.** Its velocity is its motion's, so its legs run as it moves.
- **Held bodies.** A body held on to its host is drawn at its host's back, wherever its host is drawn.

### 4. The server's clock on every machine

Each client times exchanges with the server's clock every `ClockExchangeSeconds` (`AVeyraPlayerController::ServerTimeClock` and `ClientTimeClock`). It keeps its latest `ClockExchangesKept` and trusts the one with the shortest round trip, the latest of equally quick ones. Its estimate (`UVeyraServerClock`) moves toward a newly trusted one at `ClockSlewRate`, so the drawn world never jumps, unless it is more than `ClockSnapSeconds` off. A paused world's clock stands still on both machines, so no exchange is timed while paused. Before its first exchange, the game state's coarser reading stands in.

### 5. The player's own body

On the player's own machine only, and in presentation only:
- **The moment an order is given**, the body it commands is drawn carrying it out (`VeyraOrderPrediction::Predict`, `UVeyraMovementComponent::PredictMotion`). It walks straight toward the order's goal at the speed its last motion told, stopping short as the server will: the orders' arrival tolerance for a move or an Attack Move, the attack's reach to its target for an attack. An attack already within reach turns the body to its target. Nothing is predicted for a body that may not walk: dead, held by its statuses, or moved by something else.
- **When the server answers**, its motion takes over: a motion with that order's number, or a later one. A walk that answers it is drawn ahead of its server motion by what the order waited to begin on the server, at most `OwnLeadMaxSeconds`. Its path is the server's own, so the body never steps back from where its prediction had it. Any other answer, such as a stun that came first, is drawn as it is, closing as §3 closes any new motion.
- **A refused order** ends its prediction: the reliable refusal names its number. So does an order the server has not answered within `OwnPredictionSeconds`.
- **Which body.** Whichever body the player commands: its Vanguard, or an Echo it commands in its stead. An order is predicted only for the body it was given to.
- **The server stays in charge.** Nothing the drawn body does reaches the server. The server records each order it takes on its body's motion (`SetMotionOrderNumber`) so the answer can name it.

### 6. Units start and stop at once

Path following sets a unit's velocity rather than accelerating it there (`bRequestedMoveUseAcceleration = false`). A unit walks at its speed from the moment it has somewhere to go, and stands the moment it arrives. A ride keeps its own turning (Combat Bible §56).

### 7. A cast given as a lockout ends is held

A cast refused only because its caster is Busy, with at most `orders.castBufferSeconds` left of its last lockout, is held on the server and cast the moment it may be (Combat Bible §26). The last lockout is a cast's Recovery or the caster's own dash. A windup or channel has more to come, so it never qualifies. The held cast is tried again each frame until its time runs out, and only then refused. A newer cast replaces it; any order drops it. Like every cast, it is judged in full when it is cast: buffering never bypasses a cooldown, crowd control, a cost or a targeting rule. A cast's refusal now always arrives (`ClientCastRejected` is reliable).

### 8. A blink keeps the order

A blink stops the path its body walked, since that path began where the body stood. Its controller keeps the order and walks on to it from where the body lands (`UVeyraMovementComponent::OnRelocated`).

### 9. Data

- Motion and clock values are `Combat.json` `motion`, validated by its schema. They are network presentation, not balance: none changes where the server moves a body.
- The cast buffer is `Match.json` `orders.castBufferSeconds`.
- ADR-077's `OwnLead*` and ADR-065's ease settings are removed from `VeyraGreyboxSettings`.

## Consequences

- **Smooth bodies.** Every body moves smoothly at any tick rate or jitter. A message late by up to `HorizonSeconds` less `RefreshSeconds` changes nothing a player sees.
- **Bodies drawn where they are.** Other bodies are drawn where the server has them now, not an update behind, so skillshots meet the bodies players aim at more closely.
- **Exact dashes and blinks.** A dash runs at its true speed to its true end, and a blink is one clean instant. A dash cut short by contact draws a little past its stop, then closes back.
- **Own clicks.** The player's own body answers a click within a frame and walks the server's own path once it answers. A stun given with the click shows a short step back, over `CorrectionSeconds`.
- **Network traffic.** It falls: a walking unit is told a few times a second rather than every tick, and a standing one only when it turns.
- **Tests:**
  - `Veyra.Combat.Motion.*`: sampling, horizons and the clock estimate.
  - `Veyra.Match.OrderPrediction.*`.
  - `Veyra.Net.VanguardMoveOrders.EveryMachineLearnsTheWalkButNotWhereItEnds`, `.ItsMotionCarriesTheSpeedTheServerWalksItAt`, `.AnotherMachineDrawsAWalkingVanguardOnItsMotion` and `.ItsOwnMachineWalksAnOrderTheMomentItIsGiven`.
  - `Veyra.Net.ForcedMovement.AnOrderWalksOnAfterABlink`.
  - `Veyra.Net.Vanguards.Oriel.ACastGivenAsARecoveryEndsIsHeldThenCast`.
  - The Echo tests in `Veyra.Net.SecondSelf`.

## Not decided here

- **The player's own dashes and casts, drawn on the key press.** ADR-009 §6 still keeps abilities the server's. The motion makes this a small next step, an own-machine prediction of a Forced motion, but it is its own decision.
- **A targeted cast out of range.** Walking into range and casting there, as an attack order chases, is a Controls decision for the design canon.
- **The server's tick rate.** It stays 30 Hz.

## Alternatives considered

- **Hand the player's own body to its machine, with the engine's movement and ability prediction.** This is the engine's way for direct-control games. It reverses ADR-006 §7's control model (server controllers, the autopilot on disconnect, commanding an Echo, avoidance), and with this much crowd control it would often mispredict and snap the player's own body back. It is also a far larger change.
- **Raise the tick to 60 Hz and add a jitter buffer to the engine's ease.** This lessens jitter only, with another layer on top, and does nothing for dashes, blinks or casts.
- **Mover.** Still Experimental in 5.8 (ADR-001).

## Provisional values

| Setting | Value |
|---|---|
| `motion.horizonSeconds` | 0.6 |
| `motion.refreshSeconds` | 0.25 |
| `motion.driftDistance` | 16 |
| `motion.speedChangeShare` | 0.02 |
| `motion.standingSpeedShare` | 0.1 |
| `motion.facingDegrees` | 10 |
| `motion.setDownDistance` | 300 |
| `motion.correctionSeconds` | 0.12 |
| `motion.correctionSnapDistance` | 250 |
| `motion.remoteDelaySeconds` | 0 |
| `motion.ownLeadMaxSeconds` | 0.25 |
| `motion.ownPredictionSeconds` | 0.5 |
| `motion.clockExchangeSeconds` | 0.5 |
| `motion.clockExchangesKept` | 8 |
| `motion.clockSlewRate` | 0.05 |
| `motion.clockSnapSeconds` | 0.25 |
| `orders.castBufferSeconds` | 0.25 |
