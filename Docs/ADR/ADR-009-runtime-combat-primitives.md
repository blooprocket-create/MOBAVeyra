# ADR-009: Runtime combat primitives

**Status:** Proposed. It becomes Accepted when the author merges the M5 pull request that adds it.  
**Date:** 2026-09-26  
**Approved in:** Author decisions for M5 (2026-09-26), as for ADR-008.  
**Related:** [ADR-002](ADR-002-gameplay-ability-system.md), [ADR-006](ADR-006-unreal-project-scaffold.md) (§4 GAS placement and the §41 allow-list, §5 Iris, §7 server-only movement, §8 world-time pause), [ADR-008](ADR-008-vanguard-definitions-and-ability-composition.md) (the definitions that use these primitives), [Combat Bible](../Design/Veyra_Combat_Bible_v0.5.md) §4, §7–§9, §12–§13, §16–§17, §22–§23, §26, §28, §40, §44–§50, §54, [Architecture Constitution](../../ARCHITECTURE.md) §1.10, §3, §12.

## Context

The first four kits need primitives the engine does not have yet: crowd control and timed buffs, forced movement and dashes, projectiles and ground areas, shaped hit tests, shields with identity and caps, "in combat" tracking, assist credit, and basic attacks. Each primitive will serve every later Vanguard, item and Flux Spell (Architecture §1.10–§1.11), so each needs one owner and one shape now.

Two engine facts constrain the design:

- **Movement and casting are server-only, with no client prediction** (ADR-006 §7). A server AI controller moves each Vanguard; clients send intents and see replicated results.
- **Iris ignores a struct's own `NetSerialize`** (ADR-006 §5 amendment). Replicated state must be plain reflected properties.

## Decision

### 1. Statuses: a Combat-owned ledger with one native Gameplay Effect

- **`UVeyraStatusComponent`** on the PlayerState is the truth for each status: its ID, source, Cast ID, kind, magnitude and timing. It applies the stacking policy each status declares (Combat §46: Unique–Refresh, Unique–Replace Strongest, Stacking, Independent Sources) and replicates plain structs for presentation.
- **Stat changes** go through one native, duration-based Gameplay Effect whose modifiers are MultiplyCompound lines read from SetByCaller values. They sit inside the §41 allow-list with no new execution, freeze with the world during a pause, and are removed on death by the existing cleanup (Combat §44). Statuses with no stat line (Stun, Slow) use a marker effect for their lifetime.
- **GAS stacking is not used.** The ledger enforces Combat's rules, as ADR-002 requires.
- **Rules**, as pure functions: Tenacity shortens reducible durations when applied, never below 0.3 s (Combat §8, a data value); the strongest Slow controls speed while weaker ones stay tracked; the longest Stun controls; each unit's move, attack and cast blocks are derived from its active statuses.
- **Kinds** are a typed enum bound from tuning: Stun, Slow, Attack Speed, Move Speed, Move Speed toward enemy Vanguards, Tenacity, Damage Reduction, Displacement Resistance and Attack Cleave. Crowd-control Gameplay Tags wait until a GAS tag query needs them.
- A status may be **extended by takedowns** up to a cap (Qazharr R).

### 2. Movement, displacement and dashes

- **`UVeyraMovementComponent`**, Combat-owned, replaces the Vanguard's character movement. It adds `Displaced` and `Dashing` modes.
- **Effective speed** is one chain: Move Speed → conditional bonus → strongest Slow → Combat §23 soft caps (415 and 490) → the slow floor (100) → zero under Stun. All values are data.
- **Displacement** (Pull, Knockback) and **dashes** compute their path once: the body is swept against terrain and stops at the nearest legal point (Combat §9), and the end is projected onto the navmesh. Displacement Resistance shortens the distance. A newer displacement replaces an older one and interrupts a dash.
- **Locks:** while a unit is displaced, dashing, stunned or casting with movement locked, the server controller pauses its path and resumes the latest move order afterwards. Normal move commands never override forced movement (Combat §9).

### 3. Shields, Combat State and attribution

- **Shields** gain an identity and a source. A grant can merge into an existing shield of the same identity up to a cap, and can belong to a cap group whose total is bounded (Cairn's passive and R share one cap). Absorption order is unchanged (Combat §7).
- **Combat State** (Combat §28): a unit is in combat after dealing or taking Vanguard damage, until a data delay (5 s) passes.
- **Attribution:** damage and hostile statuses are recorded per victim within a data assist window. The death event carries assisters as well as the killer, so takedowns can extend statuses.

### 4. Delivery: projectiles, delayed areas and shapes

- **Projectiles** are server actors with no Ability System Component (ADR-006 §4).
  - Their payload is a damage spec **prepared at Commit** and applied at impact, so attacker values are snapshotted at Commit and defender values read at impact (Combat §50).
  - The server sweeps against units and terrain. A skillshot stops at the first eligible unit or pierces; a homing projectile follows its target and fizzles if the target becomes invalid, with no refund (Combat §54).
  - **Terrain blocks skillshots but not homing projectiles** (a provisional answer in ADR-008 §9).
  - Clients receive launch data only and simulate the visual path from server time, so it freezes during a pause.
- **Delayed areas** replicate their shape, placement and resolve time for telegraphs, and resolve on a world-time timer.
- **Shapes:** circle, sector (arc or cone) and rectangle, with an origin and a direction. A unit is hit when its body circle intersects the shape in 2D, edges included (Combat §40). Hits are ordered by distance, then a stable ID, for determinism.

### 5. Basic attacks

- **`UVeyraBasicAttackComponent`**, owned by Abilities, runs each attack: windup → Commit → backswing (Combat §4, §48). The target must be valid and in range at start and at Commit. A melee attack resolves at Commit; a ranged attack launches a homing projectile that can no longer be escaped by leaving range. Moving or casting before Commit cancels the attack; backswing can be cancelled.
- **Attack Speed** resolves in one pure function: the 2.5 cap and 0.2 minimum (Combat §22), a personal minimum interval where a kit declares one (Bryn), and overflow damage always measured against the 2.5 reference, never against a personal floor.
- **Hooks:** On Attack and On Hit (Combat §16) as events. Modifiers can add damage components, per-event penetration and one secondary impact with a priority, so a higher-priority impact replaces a lower one (Bryn's Q explosion replaces the passive one). Secondary impacts are proc damage and never re-enter the hit pipeline.
- **Empowered next attack** (Combat §17), **cleave** and a **hit chain** (consecutive hits on one enemy Vanguard, reset on a target switch, on leaving combat and on death) are generic.
- **Orders:** the server controller runs attack and attack-move orders, chasing to attack range.

### 6. No prediction for these categories

Dashes, displacement, skillshots, ground areas, channels and basic attacks are **server-only, with no client prediction**. The client sends the intent (a unit, a location or a direction) and shows the replicated result. This extends ADR-006 §7 and closes Architecture §12's open prediction item for these categories. A later category that needs prediction gets its own decision.

### 7. Networking and Vision obligations

- All new replicated state is plain reflected properties.
- New per-participant state (statuses, cast state, progression, passives) replicates to everyone until Vision exists. When Vision lands, it must adopt the per-player fog gate (`COND_NetGroup`, ADR-006 §5 amendment) like the rest of the PlayerState.
- `VeyraTargeting::CanAcquire` is the hook Vision will implement. Until then it allows every target, so the Vision-dependent parts of Bryn W and Oriel's passive are deferred (ADR-008).

## Consequences

- Every later kit, item and Flux Spell composes the same statuses, movement, delivery and attack primitives; none rediscovers them.
- Statuses stay inside the §41 allow-list and the death cleanup with no new execution.
- Forced movement and dashes are server-authoritative and exact; clients may see smoothing on fast movement.
- Replicated combat state is visible to every client until Vision gates it.

## Open items

- Heals, damage over time, Health regeneration, Healing Reduction.
- Root, silence, knockup, taunt, fear and the other crowd-control types; cleanse, crowd-control immunity, Unstoppable, Spell Shields and projectile interception.
- The Non-Stacking Protected policy.
- Critical strikes, and Ability Haste as a stat.
- Idle auto-attack acquisition and target priority (Combat is silent; ADR-008 §9).
- Prediction for any later category that needs it.

## Alternatives considered

- **Gameplay Tags and GAS effect stacking for crowd control:** GAS stacking cannot express Combat §46's policies, and custom Gameplay Effect contexts need a `NetSerialize` that Iris ignores.
- **Root-motion sources for displacement:** built for predicted movement, which ADR-006 §7 rules out; a custom movement mode is simpler and exact on the server.
- **Client-predicted dashes and skillshots:** needs the control model revisited (the M3 target-data spike showed GAS refuses client activation on a simulated-proxy avatar).
- **Projectiles as GAS target data:** clients would decide hits, against server authority.
- **Basic attacks as Gameplay Abilities:** they need no cost or cooldown ledger, and their cadence is Attack Speed, not a cooldown.
