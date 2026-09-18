# ADR-002: Adopt Unreal Gameplay Ability System

**Status:** Accepted  
**Date:** 2026-09-18

## Context

Veyra requires a networked ability framework capable of supporting a large Vanguard roster, cooldowns, resource costs, status effects, gameplay tags, replicated state, asynchronous ability execution, and client prediction without duplicating those foundations champion by champion.

Unreal Engine provides the Gameplay Ability System (GAS) for abilities, Attributes, Gameplay Effects, Gameplay Tags, Ability Tasks, replication, and prediction.

Veyra also has a deliberately strict architecture: the server remains authoritative, the Combat Bible defines canonical combat semantics, and core rules must not become opaque Blueprint graphs or content-specific hacks.

## Decision

Veyra will **adopt Unreal Gameplay Ability System (GAS)** as a foundational gameplay framework.

GAS is infrastructure, not the owner of Veyra's game design. Veyra will build a thin, well-defined C++ integration layer around GAS so engine facilities serve the rules documented in the Veyra design bibles.

The intended use includes:

- Ability System Components for ability-capable actors where appropriate.
- Gameplay Abilities for Vanguard abilities and other compatible active gameplay actions.
- Attribute Sets for suitable replicated gameplay attributes.
- Gameplay Effects for compatible buffs, debuffs, costs, cooldowns, and stat modification.
- Gameplay Tags as the central semantic vocabulary for combat states and effect categories.
- Ability Tasks and custom Veyra Ability Tasks for asynchronous/multi-stage execution.
- GAS networking and prediction facilities where they fit Veyra's server-authoritative model.

### Veyra remains authoritative over semantics

GAS must not replace or contradict the canonical Veyra combat rules.

In particular:

- The **Veyra combat system** owns canonical damage, healing, shield, mitigation, death, and other rules defined by the Combat Bible.
- GAS effects/abilities may invoke those systems through reusable interfaces/executions; they must not create parallel damage formulas.
- Champion-specific abilities compose reusable GAS/Veyra primitives rather than implementing private copies of targeting, mitigation, death, status, or cooldown logic.
- Gameplay Effect stacking behavior must map intentionally to Veyra's documented status-stacking policies.
- Prediction must never grant the client final authority over damage, resources, inventory, Flux, objectives, or match state.
- Core GAS extension code belongs in C++. Blueprints/Data Assets may configure content and presentation over stable primitives.

### Initial setup expectation

The first gameplay scaffold should establish GAS correctly before champion implementation expands:

1. Enable and link the GameplayAbilities, GameplayTags, and GameplayTasks modules/plugins required by GAS.
2. Establish the Veyra Ability System Component ownership model.
3. Establish initial Attribute Set ownership and replication.
4. Establish centrally defined Gameplay Tags.
5. Create Veyra base Gameplay Ability / Ability Task / Gameplay Effect extension points only where they provide real shared value.
6. Prove server authority and replication with automated/networked test abilities before building the roster on top.
7. Document prediction policies as they become concrete rather than assuming all abilities predict identically.

The exact ASC placement (for example PlayerState versus Pawn for specific actor categories), detailed prediction policy, and final Attribute Set decomposition should be chosen during scaffolding based on respawn/persistence requirements and documented when locked.

## Consequences

- Veyra gains a mature framework for replicated abilities, tags, effects, costs, cooldowns, and prediction.
- AI coding agents can build against documented Unreal primitives rather than inventing a bespoke ability framework.
- GAS concepts become an implementation dependency that contributors must understand.
- We must resist using Gameplay Effects as a shortcut that bypasses Veyra's canonical combat pipeline.
- Automated tests are especially important around custom executions, stacking, prediction, cancellation, and server correction.
- Some Veyra systems such as economy, Team Flux, inventory, objectives, and match orchestration remain separate domain systems even when abilities interact with them.

## Alternatives considered

### Build a fully custom ability framework
Rejected because it would duplicate mature Unreal networking, prediction, tagging, effect, and ability infrastructure without a demonstrated Veyra-specific need.

### Avoid deciding until champion implementation
Rejected because ability architecture affects the initial character, attribute, networking, module, and content scaffolding.

### Put most ability logic directly in Blueprints
Rejected because it conflicts with Veyra's architecture constitution and would make authority, testing, reuse, and large-roster maintenance harder.
