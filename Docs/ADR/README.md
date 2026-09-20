# Architecture Decision Records

This directory records major technical choices that should not be casually reversed by future contributors or coding agents.

## Accepted decisions

- [`ADR-001-unreal-version-policy.md`](ADR-001-unreal-version-policy.md) — Unreal Engine 5.8; deliberate version-pinned upgrades.
- [`ADR-002-gameplay-ability-system.md`](ADR-002-gameplay-ability-system.md) — Adopt GAS behind Veyra-owned C++ integration and combat semantics.
- [`ADR-003-owned-field-entities.md`](ADR-003-owned-field-entities.md) — Three owned-entity primitives (combat entity, placed marker, world volume owned by its ruling system); Raska's ride state scoped separately; first playable built from the nine Vanguards needing no entity.

## When to create an ADR

Create an ADR when a decision materially affects multiple systems, establishes a long-lived dependency, chooses an engine/plugin/infrastructure strategy, or changes an architecture rule.

Examples:

- Unreal Engine version policy;
- Gameplay Ability System adoption strategy;
- dedicated-server authority implementation;
- item/Vanguard data definition format;
- gameplay messaging/event system;
- persistence/backend boundaries;
- replay/determinism strategy;
- asset/LFS/source-control policy.

## Format

Use sequential names such as:

```text
ADR-001-unreal-version-policy.md
ADR-002-gameplay-ability-system.md
```

Suggested template:

```markdown
# ADR-###: Decision title

**Status:** Proposed | Accepted | Superseded
**Date:** YYYY-MM-DD

## Context
What problem or constraint requires a decision?

## Decision
What are we choosing?

## Consequences
What becomes easier, harder, required, or prohibited?

## Alternatives considered
What other reasonable options were considered and why were they not selected?
```

Keep ADRs concise. Their job is to preserve *why* a major choice exists.
