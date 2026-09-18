# Veyra Project Structure

**Status:** Provisional structure; architecture direction is locked, exact module names may evolve.  
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
- combat event data;
- death-trigger inputs (not full match respawn policy).

Must never special-case named Vanguards or items.

### VeyraAbilities

Owns reusable ability execution behavior and, if adopted, Veyra's integration layer around Unreal Gameplay Ability System.

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

A Flux Spell cast does not consume shared Flux under the current game design. Swapping Flux Spells at the shop costs gold and should use the economy transaction API rather than mutating gold in the Flux module.

### VeyraWorld

- Flux Wells and other world objectives;
- Spires and Prime Well world actors;
- jungle camps and wildlife systems;
- lane/Fluxway world behavior;
- objective capture state and world interactions.

World actors report outcomes to the authoritative owning systems rather than reaching directly into UI or champion code.

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
Abilities / Items / Flux / World
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
