# Veyra Project Structure

**Status:** Provisional structure; architecture direction is locked, exact module names may evolve.  
**Engine target:** Unreal Engine 5.8.3, source build (`ADR-001`); Windows client, Linux dedicated server (`ADR-005`)  
**Ability framework:** Unreal Gameplay Ability System (GAS), per `ADR-002`  
**Application architecture:** Single Unreal client with controlled states, per [`ADR-004`](Docs/ADR/ADR-004-unified-unreal-client-states.md)  
**Read first:** [`ARCHITECTURE.md`](ARCHITECTURE.md)

The purpose of this document is to make ownership and dependency direction obvious before the Unreal project becomes large. It is not permission to create every listed module immediately. Start with the smallest useful set and split modules when boundaries become valuable.

## 1. Intended source domains

A likely long-term shape is:

```text
Source/
├── VeyraCore/
├── VeyraCombat/
├── VeyraAbilities/
├── VeyraEconomy/
├── VeyraItems/
├── VeyraFlux/
├── VeyraWorld/
├── VeyraVision/
├── VeyraMatch/
├── VeyraVanguards/
├── VeyraUI/
└── VeyraDeveloper/
```

These names are placeholders until the Unreal project is scaffolded. The domain responsibilities below matter more than the exact spelling.

### VeyraCore

Lowest-level Veyra-owned foundation.

Appropriate responsibilities:

- shared identifiers and lightweight value types;
- central Gameplay Tags/vocabulary;
- stable interfaces and message contracts;
- common utilities that are genuinely domain-neutral;
- shared serialization/version helpers where needed.

Must not depend on higher gameplay modules.

### VeyraCombat

Owns reusable combat truth.

- attributes;
- damage/healing/shield pipeline;
- mitigation/resistance calculations;
- status effects and crowd-control primitives;
- displacement primitives;
- movement modes, including ride states (Combat Bible §56: a ride state is a movement mode on an ordinary pathing agent, not a separate system);
- combat event data;
- death-trigger inputs (not full match respawn policy).

Must never special-case named Vanguards or items.

### VeyraAbilities

Owns reusable ability execution behavior and Veyra's C++ integration layer around Unreal Gameplay Ability System (GAS).

- activation validation;
- cooldown/cost primitives;
- targeting;
- projectiles/areas where these are ability primitives;
- prediction contracts;
- reusable ability tasks/effects.

A Vanguard ability composes this system; it does not recreate it.

### VeyraEconomy

Canonical owner for gold and economy rules.

- gold balances;
- grants/spending;
- purchase affordability;
- kill/assist/CS/objective reward calculations;
- economy transactions and audit/debug events.

UI and items request transactions; they do not mutate gold directly.

**Progression** (XP balances, levels, level-up stat increments and skill points) lives in this module for now as a **separate owner** with its own state, per the Economy & Progression Bible. It shares the module, not code paths: Gold and XP are never mixed in one class.

### VeyraItems

- inventory ownership;
- item definitions;
- recipes and combination rules;
- Tier 1/2/3/4 rules;
- item actives;
- Tier 3 Attunement attachment/configuration;
- consumable state;
- shop-facing item queries.

Depends on combat/abilities/economy through approved contracts. It does not own the underlying damage or gold formulas.

### VeyraFlux

- authoritative shared team Flux;
- Flux gain/loss rules if loss ever exists;
- threshold state;
- Flux Spell definitions/loadouts/unlocks;
- Fluxborn-strength progression inputs;
- notifications when thresholds change.

Flux Spell unlocks are validated against **permanent Team Flux only**; temporary Flux must not contribute to spell-slot unlock state. The threshold values are data owned by the Battleground Bible (§14); do not copy them here or into code.

**How Team Flux is used.** The design rules live in the Battleground Bible; this module holds the runtime state.

- **Sources:** destroyed lane Spires and base-defense towers grant *permanent* Flux; secured Flux Wells and destroyed inhibitors grant *temporary* Flux, each grant expiring on its own timer. Flux is never spent: Flux Spell casts do not consume it.
- **State:** per team, the permanent total plus a list of active temporary grants with their expiry times. *Active* Flux is permanent plus unexpired temporary.
- **Readers:** Fluxborn strength (active Flux, all lanes); Flux Spell slot unlocks (permanent Flux only); Economy's Fluxborn farm-reward bonus (active Flux at the Fluxborn's death); HUD and statistics presentation.
- **Dependency direction:** world objectives report destruction/capture through Core contracts or match orchestration, and the Flux system grants the data-defined reward; it does not reach into world actors. Economy sits below Flux, so it never queries Team Flux: the Fluxborn death event carries the value the farm-reward rule needs. Fluxborn scaling reacts to Flux-changed notifications rather than polling.

A Flux Spell cast does not consume shared Flux under the current game design. Swapping Flux Spells at the shop costs gold and should use the economy transaction API rather than mutating gold in the Flux module.

### VeyraWorld

- Flux Wells and other world objectives;
- Spires and Prime Well world actors;
- jungle camps and wildlife systems;
- lane/Fluxway world behavior;
- objective capture state and world interactions;
- runtime navigation changes from ability-created terrain (Battleground Bible §2).

World actors report outcomes to the authoritative owning systems rather than reaching directly into UI or champion code.

### VeyraVision

- ordinary map vision and fog of war;
- Dense Fog volumes, authored and ability-created (Vision Bible §2);
- wards, the vision-tool slot, Sweeper and Quick Sight;
- stealth detection and True Sight reveal;
- presence-sensor events.

Combat still owns targetability and hit validation; Vision supplies what each team can see. Sits at the same layer as World.

### VeyraMatch

- teams;
- match phases;
- spawn/respawn orchestration;
- score and victory state;
- match start/end;
- high-level coordination between otherwise independent systems.

Use this layer to orchestrate systems when direct peer-to-peer dependencies would create cycles.

### VeyraVanguards

Champion-specific gameplay content.

Suggested internal pattern:

```text
VeyraVanguards/
├── Shared/
├── Raska/
├── Kade/
├── Silt/
└── ...
```

Vanguard code may compose Combat and Ability primitives. It must not fork or duplicate their rules.

### VeyraUI

Presentation only.

- HUD;
- shop presentation;
- scoreboard;
- draft/loadout presentation;
- menus and settings;
- accessibility presentation.

UI observes/queries gameplay state and emits user intent. No gameplay module depends on UI.

### VeyraDeveloper

Non-shipping or development-facing utilities.

- automation tests;
- debug commands;
- headless match harnesses;
- asset/data validation;
- gameplay inspection tools;
- bot/test drivers.

Developer tooling may depend on production systems. Production systems must never require developer tooling.

## 2. Dependency direction

Conceptually:

```text
VeyraCore
   ↓
Combat / Economy
   ↓
Abilities / Items / Flux / World / Vision
   ↓
Match / Vanguards
   ↓
UI

Developer tooling may observe/use all layers.
```

This is a guide, not a license for arbitrary sideways dependencies. Prefer contracts/messages when peer systems need to cooperate.

### Hard dependency rules

- `Core` depends on no Veyra gameplay module.
- `Combat` never depends on a Vanguard, item, Flux, world objective, or UI.
- `Economy` never depends on shop widgets or specific items.
- `UI` may depend on read/query contracts from gameplay; gameplay never depends on UI.
- `Vanguards` never become a dependency of reusable core gameplay systems.
- `Developer` is a leaf from the perspective of production code: everything may be tested by it, nothing production-critical depends on it.
- Circular module dependencies are prohibited.

### Backend (outside Unreal)

The Go backend from [`ADR-005`](Docs/ADR/ADR-005-launcher-session-handoff-and-local-first-hosting.md) lives in [`Backend/`](Backend/README.md), with the local Docker stack in `compose.yaml` at the repository root. It is one service with one internal package per trusted domain (identity now; party, matchmaking, match allocation and results later). Domain packages own their rules and depend on storage interfaces; storage and HTTP transport depend on domains, never the reverse. Unreal modules never link to backend code; they talk to it over HTTP.

## 3. Content directory

A likely content organization:

```text
Content/Veyra/
├── Vanguards/
│   ├── Shared/
│   ├── Raska/
│   ├── Kade/
│   └── ...
├── Items/
├── Flux/
│   ├── Spells/
│   ├── Fluxborn/
│   └── Objectives/
├── World/
│   ├── MeridianCrucible/
│   ├── Jungle/
│   └── Structures/
├── UI/
├── VFX/
├── Audio/
└── Developer/
```

Do not create cross-project junk drawers such as `Misc`, `Stuff`, or `Temp` as permanent homes. Temporary work should have an explicit cleanup path.

## 4. Content versus code

Use **code** when the behavior is reusable logic or a rule.

Use **data** when the thing primarily describes tunable content.

Use **Blueprint/presentation assets** when the thing primarily assembles or visualizes content.

Example: Razorwheel Prime should not own a bespoke damage formula in a widget or Blueprint. Its data references the stats/effects it grants; the reusable cleave and movement-steal behaviors execute through gameplay systems.

## 5. Naming and ownership

Before adding a class, be able to complete this sentence:

> `X` belongs in `Y` because `Y` is the authoritative owner of `Z`.

If the sentence is awkward, the class probably belongs somewhere else or the boundary needs clarification.

Avoid generic names such as `Manager` when a more precise owner exists. Prefer domain terms such as `InventoryComponent`, `TeamFluxState`, `DamageExecution`, or `ObjectiveCaptureComponent` once the actual Unreal design is decided.

## 6. Initial scaffolding rule

Do not create ten empty modules merely because this document lists them. The first Unreal scaffold should establish the minimum clean dependency graph required for the first vertical slice, while preserving the boundaries described here.

When a module becomes too broad or creates unwanted dependencies, split it deliberately and record major changes in an ADR.


## 7. Unified client presentation and Test Skin — architectural checkpoint (2026-09-23)

A **single installed Unreal client** contains the ordinary pre-game UI, Store/Collection, social/party and matchmaking presentation, interactive Test Skin, committed champion select, live gameplay, verified results and Replay/Spectator/Reconnect-only modes. The website and launcher are separate; the dedicated match server is still authoritative. See [ADR-004](Docs/ADR/ADR-004-unified-unreal-client-states.md). Earlier diagrams' `VeyraUI` is a **presentation responsibility**, not an independent pre-game executable or gameplay authority.

- **Client-state coordinator:** explicit legal state transitions, active UI/input focus and map/resource ownership; subscribes to authoritative match/party/queue/session state through contracts. No giant universal `GameInstance`, PlayerController or persistent level owns all gameplay, Shop, preview and match truth.
- **Ordinary shell:** Home, Play, Shop, Vanguards, profiles, Match History, persistent independently collapsible party/friends/chat surfaces and queue status. UI sends intents, never grants entitlements, money, matchmaking eligibility or progression.
- **Isolated Test Skin world:** entered from Shop inside the same application, reuses actual Vanguard assets and C++/GAS ability primitives with test-only dummies, ability resets/resources and cosmetic comparison. A preview does not act as a live match or second implementation of combat; it cannot commit authoritative rewards or purchases. Deactivate it immediately for Match Found, release unneeded resources on selection entry and return to original Shop listing on normal exit.
- **Committed state:** Match Found blocking acceptance → champion select (no ordinary page navigation or party management) → truthful loading/connect → live HUD. If the assigned match is still live after process restart or connection failure, only Reconnect is offered in pre-game. Verified completion transitions into results and restores ordinary shell. Replay/Spectator are separate nonparticipant modes with their own permissions.
- **Performance:** versioned installed assets and budgeted caching/preloading, no mandatory on-demand download for installed skins, optional loads yield to time-critical match transitions. Separate permanent backend/cache records from ephemeral world/widget state.
- **Required tests:** transition priority while testing/loading skins; no party/queue mutation from test map; no extra Shop/Settings/social entry in committed select or Reconnect-only; ability presentation parity Base/Skin; restart recovery to assigned match; valid/results-only shell restoration; stable memory bounds under repeated test-map enter/exit. Exact module names, map/world travel and Unreal implementation strategy remain provisional.

**Current pre-game client design pause: after UX-92; wait for author “continue” before UX-93.**
