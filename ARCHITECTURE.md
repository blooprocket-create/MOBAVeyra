# Veyra Architecture Constitution

**Status:** Locked foundation rules  
**Scope:** One unified Unreal client application, dedicated match server, gameplay framework, tools, tests, and presentation code  
**Purpose:** Preserve a codebase that can evolve quickly without allowing rapid iteration or coding agents to collapse the project into tightly coupled spaghetti.

This document is the highest-level technical authority in the repository. Lower-level implementation documents may refine these rules, but they must not contradict them without an explicit Architecture Decision Record (ADR) and deliberate approval.

## 1. Core architecture laws

### 1.1 The server is authoritative

Authoritative gameplay state lives on the server. The server decides the outcomes of damage, healing, shields, crowd control, movement validation, cooldowns, ability costs, gold, XP, inventory mutations, item purchases, Flux, Flux Spell unlocks, objectives, deaths, respawns, and match state.

Clients may predict presentation and latency-sensitive actions where appropriate, but prediction is never authority. A client request is an intent that the server validates.

Every networked gameplay feature must answer:

1. Who owns the authoritative state?
2. What is replicated?
3. What, if anything, is client-predicted?
4. What requests may a client send?
5. What validation does the server perform?
6. How does the client recover from a rejected or corrected prediction?

### 1.2 C++ owns reusable gameplay systems and rules

Reusable gameplay behavior belongs in C++ modules and well-defined engine-facing systems. Examples include damage resolution, attributes, status effects, movement/displacement primitives, inventory rules, item recipes, economy, Flux, objectives, targeting, ability execution primitives, respawning, and match rules.

Champion-specific code should express what makes a Vanguard unique by composing reusable primitives. It should not reimplement the foundations those primitives already provide.

### 1.3 No hardcoded gameplay tuning or magic numbers

**Project-wide rule: no hardcoded gameplay tuning values and no unexplained numeric literals in gameplay logic.** Designers must be able to tune the game's values without changing or recompiling C++ or modifying Blueprint logic.

- Data Assets, Data Tables, validated configuration assets, and other data-driven definitions own all gameplay and balance parameters: base stats, ratios, cooldowns, Gold/XP rewards, inventory limits where configurable, costs, item recipes, Flux thresholds, jungle values, objective and wave timings, respawn/buyback rules, AI/AFK timings, distances, radii, speeds, caps, and similar settings.
- Match-flow schedules must be explicit data, including **first wave spawn time, phase boundaries, per-phase spawn intervals, lane offsets if any, and any later cadence changes**. Never bury a proposed schedule inside a timer callback or branch with literal elapsed-time checks.
- Veyra has its **own map geometry and travel times**. Values borrowed from another MOBA are provisional data entries to validate against Veyra playtests, not engine-level assumptions.
- Systems load and validate tunable data once through the owning domain, expose it through a clear typed contract, and derive dependent behavior from that data. Avoid copying the same value into unrelated classes, assets, clients, test fixtures, or widgets.
- Never replace literals with meaningless local constants that remain just as hard to tune, or use a default that silently disguises missing/invalid tuning data. Validate ranges, required fields, and compatibility on startup/build, with explicit failures for missing required configuration.
- Fixed mathematical identities, array indices, protocol/schema version markers, and genuinely invariant algorithmic constants are not gameplay tuning. They may remain code constants when **named or self-evident**, documented when non-obvious, and tested. Do not interpret this rule as outlawing the literal `0` or `1` in arithmetic.
- Any gameplay literal added to C++/Blueprint requires justification that it is truly invariant; otherwise it must move into editable data. Temporary prototype numbers belong in data too.

**No magic numbers** means code should describe what a number represents and obtain all configurable values from their authoritative data source; adding a named constant in C++ does not make a gameplay balance value data-driven.

### 1.4 Blueprints stay thin

Blueprints are allowed and useful, but they are not the default home for core game rules.

Blueprints should primarily handle:

- visual assembly and content wiring;
- animation hooks;
- VFX and audio triggers;
- simple presentation behavior;
- designer-facing composition over stable C++ primitives.

Large Blueprint graphs that own combat, economy, Flux, networking, item rules, or other core systems are architecture violations unless an ADR explicitly approves the exception.

### 1.5 UI observes gameplay; UI does not own gameplay

UI may display authoritative state and send user intent. It must not become the source of truth.

Examples:

- The shop UI asks the economy/item systems whether a purchase is legal; it does not decide affordability itself.
- The HUD displays Health supplied by gameplay state; it does not calculate Health.
- A Flux Spell button requests a cast; it does not decide whether the Flux threshold or cooldown rules are satisfied.

No gameplay module may depend on UI in order to function.

### 1.6 One authoritative owner for each truth

Every important rule or state domain has one canonical owner. There is one canonical damage pipeline, one economy authority, one Flux authority, one inventory rule set, one status-effect system, and so on.

Do not duplicate a calculation because it is convenient locally. Other systems query, call, or subscribe to the authoritative owner.

### 1.7 Dependencies are explicit, directional, and acyclic

Module and subsystem dependencies must flow in one direction. Circular dependencies are prohibited.

Low-level systems must never depend on presentation. Domain modules should depend on stable lower-level contracts rather than reaching sideways into unrelated implementations.

When two peer systems need to communicate, prefer an interface, event/message, command, or orchestrating higher-level system rather than mutual hard references.

### 1.8 No god classes

A class must have a coherent responsibility. `VeyraPlayerCharacter`, `GameMode`, `GameState`, a Vanguard class, or any other convenient central object must not accumulate unrelated responsibilities simply because it is easy to access.

A growing class is not automatically wrong, but adding a new responsibility must be justified by ownership, not proximity.

### 1.9 No champion-specific hacks in core systems

Core systems must not special-case named Vanguards or items.

Bad pattern:

```cpp
if (ChampionId == Raska)
{
    Damage *= 1.15f;
}
```

Instead, expose reusable mechanics and let the champion's ability/effect definition compose them. If a mechanic is truly unique, isolate it behind a clean extension point owned by that content domain.

### 1.10 Reusable verbs beat repeated low-level logic

Gameplay authors should be able to use stable primitives such as:

```text
DealDamage
ApplyHealing
GrantShield
ApplyStatus
Displace
SpawnProjectile
SpendGold
GrantGold
AddTeamFlux
ApplyGameplayEffect
```

A new ability should not independently rediscover target validation, armor math, death handling, assist credit, networking, UI notification, and combat logging.

### 1.11 Items and Flux Spells compose shared mechanics

Common item effects such as Health, Physical Power, Magic Power, Attack Speed, cleave, shields, stacking haste, penetration, omnivamp, and movement effects should be implemented as reusable behavior where practical.

Flux Spells use dedicated spell slots and the same general gameplay primitives as abilities where appropriate; they should not create a parallel duplicate combat framework.

### 1.12 Gameplay Tags are vocabulary, not string soup

Use Gameplay Tags (or another centrally defined typed vocabulary if later chosen) for semantic categories such as damage types, states, item categories, ability traits, and Flux Spell families.

Tags must be centrally named and documented. Do not scatter arbitrary free-form strings throughout gameplay code.

## 2. Layering model

The intended conceptual dependency direction is:

```text
Foundation
    ↓
Reusable Gameplay Systems
    ↓
Game Content / Vanguards / Items / Objectives
    ↓
Presentation / UI / VFX / Audio
```

Presentation may observe lower layers. Lower layers do not know presentation exists.

`VeyraDeveloper`-style test/debug tooling may depend on gameplay modules; production gameplay modules must never depend on developer tooling.

Veyra targets **Unreal Engine 5.8** under [`ADR-001`](Docs/ADR/ADR-001-unreal-version-policy.md) and adopts Unreal's **Gameplay Ability System (GAS)** under [`ADR-002`](Docs/ADR/ADR-002-gameplay-ability-system.md). Exact Veyra module names remain provisional until the project is scaffolded. `PROJECT_STRUCTURE.md` records the current intended split.

## 3. State ownership

Before implementing a feature, identify its authoritative owner.

| Domain | Expected authoritative owner |
|---|---|
| Damage / healing / shields / mitigation | Combat system |
| Attributes and status effects | Combat/attribute system |
| Ability execution and cooldown state | Ability system |
| Gold and purchase affordability | Economy system |
| XP, levels and skill points | Progression system (separate owner in the Economy module) |
| Inventory and item ownership | Item/inventory system |
| Team Flux totals and thresholds | Flux system: one per-team ledger of permanent Flux and active temporary grants (design rules: Battleground Bible) |
| Flux Spell loadout and unlock state | Flux Spell system, validated against team Flux |
| Objectives and capture state | Objective/world system |
| Runtime navigation changes (ability-created terrain) | World/navigation system |
| Map vision, Dense Fog, wards, stealth reveal | Vision system |
| Ride states and other movement modes | Combat system (movement primitives) |
| Teams, score, match phase, victory | Match state systems |
| HUD / shop / scoreboard rendering | UI only; never authoritative |

If ownership is ambiguous, resolve it before adding code. Do not create a second owner.

## 4. Networking rules

- Dedicated servers are the intended authority for competitive matches.
- Never trust client-supplied damage, currency, inventory, objective, cooldown, or match-result values.
- RPCs/requests should communicate intent and minimally necessary parameters, not final outcomes.
- Replicate the minimum state required for correct play and presentation.
- Prediction must be deliberate and reconcilable; do not add speculative client authority as a shortcut.
- Server-only systems should avoid unnecessary rendering/presentation dependencies.
- Network cost is part of feature design, not a cleanup task after implementation.

## 5. Data-driven content rules

- Gameplay tuning values **must be data-driven**, not hardcoded, including prototype numbers and wave schedules. A rare genuinely invariant algorithmic constant must be justified and named/documented as needed.
- Data definitions must be validated on load/build where practical.
- Stable IDs should be used for content references that must survive renames or serialization.
- Code should not depend on display names.
- Recipe and effect data should point to reusable definitions rather than duplicating behavior.
- Data schema migrations must be explicit when shipped content changes shape.

## 6. Blueprint and asset rules

- Prefer C++ base classes/components with small Blueprint children for presentation and assembly.
- Do not hide critical server rules inside animation Blueprints, widgets, level Blueprints, or arbitrary Actor event graphs.
- Level Blueprints should be nearly empty; reusable world behavior belongs in reusable actors/components/subsystems.
- Asset references across domains should avoid unnecessary hard-loading chains.
- Presentation assets may subscribe to gameplay events, but gameplay must function in a headless/server context without them.

## 7. Testing rules

Logic should be testable without rendering wherever practical.

A substantial feature is not complete until the relevant automated coverage exists or the PR explicitly explains why automated coverage is not currently practical.

Testing should grow in layers:

1. **Unit/automation tests** for deterministic rules and calculations.
2. **System tests** for interactions between gameplay domains.
3. **Network tests** for authority, replication, and prediction-sensitive behavior.
4. **Headless match smoke tests** for core end-to-end flows.
5. **Visual/playtest verification** for feel, readability, animation, VFX, and subjective game quality.

Passing a visual check does not substitute for testing deterministic rules.

## 8. Coding-agent rules

Coding agents are expected to move quickly **inside** the architecture, not around it.

An agent must not solve a task by:

- duplicating an existing calculation;
- creating a hard reference to an unrelated domain because it is convenient;
- placing core logic in UI or Blueprint presentation code;
- bypassing server authority;
- adding a named champion/item special case to a core system;
- creating a circular dependency;
- turning an existing class into a catch-all owner;
- silently changing architecture to make a feature compile.

If the clean implementation requires a new reusable primitive, interface, event, or module boundary, build that foundation first or explicitly flag the architectural decision.

## 9. Substantial-change architecture check

Every substantial gameplay PR must check:

- [ ] Is there one clear authoritative owner for every new piece of state?
- [ ] Is any logic duplicated from another system?
- [ ] Did this introduce a new hard dependency between domains?
- [ ] Is the dependency direction valid and acyclic?
- [ ] Did gameplay logic leak into UI, animation, or Blueprint presentation?
- [ ] Did any core system gain champion/item-specific branching?
- [ ] Is server authority preserved?
- [ ] Are **all** gameplay tuning values, thresholds, timing phases, and range/cost/cooldown numbers editable in validated data instead of C++/Blueprint magic numbers?
- [ ] Are relevant tests present and passing?
- [ ] Did any class gain responsibilities outside its domain?
- [ ] Does this decision deserve an ADR?

## 10. Architecture Decision Records

Use `Docs/ADR/` for major technical decisions that future contributors or agents should not casually reverse.

Good ADR subjects include:

- Unreal Engine version policy;
- Gameplay Ability System adoption and extension strategy;
- dedicated-server authority model;
- item definition format;
- Vanguard data definition format;
- messaging/event architecture;
- persistence/backend boundaries;
- deterministic replay strategy;
- source asset/version-control policy.

ADRs should record the context, decision, consequences, and alternatives considered. They do not need to be long.

## 11. Unified Unreal client-state architecture — ADR-004 (2026-09-23)

The author approved UX-90–92 and [ADR-004](Docs/ADR/ADR-004-unified-unreal-client-states.md). The launcher still owns authentication/install/patch; **one installed Unreal application** hosts ordinary pre-game browsing, Shop, social/party UI, Test Skin, Match Found, champion select, live gameplay, post-match results, replay, spectator and Reconnect-only as **separate controlled UI/input/resource states**. The dedicated authoritative match server and trusted account, commerce, matchmaking and party owners remain independent of process/state uptime.

**No parallel pre-game/game executable assumption** and **no second Vanguard combat implementation in Shop UI**. Test Skin may reuse installed assets and reusable C++/GAS gameplay primitives under isolated test-only rules, but may not mint progression, entitlements, match state or authoritative combat results. Transition logic coordinates states via explicit contracts/events; no single giant persistent map, god GameInstance/controller, circular dependency, UI-owned gameplay logic or duplicated authority calculations.

Match Found/committed selection/live reconnect outrank optional Test Skin loading and cache preloads; memory and disk caches are budgeted and version-validated, never sources of permission. Enter playable live match only after assignment, required assets and authenticated server connection. On process restart with assigned live match, pre-game **Reconnect is the only action**; verified match completion restores ordinary client state. Exact Unreal travel/world/asset implementation remains open to a deliberate engineering decision and automated transition tests.

## 12. Deliberately open decisions

The engine, ability-framework and unified-client choices are now locked by ADR: **Unreal Engine 5.8** (ADR-001), **GAS adoption** (ADR-002), and **one Unreal application with isolated client states** (ADR-004). The following implementation details remain open:

- exact final module names/count;
- exact GAS Ability System Component placement and Attribute Set decomposition;
- exact prediction model for each ability category;
- backend/database/matchmaking vendor choices;
- final build farm and CI provider;
- final asset-management/LFS policy;
- detailed replay/determinism implementation.

Do not invent these decisions in unrelated feature work. When one becomes necessary, decide it deliberately and record it if architectural.
