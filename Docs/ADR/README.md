# Architecture Decision Records

This directory records major technical choices that should not be casually reversed by future contributors or coding agents.

## Accepted decisions

- [`ADR-001-unreal-version-policy.md`](ADR-001-unreal-version-policy.md) — Unreal Engine 5.8; deliberate version-pinned upgrades.
- [`ADR-002-gameplay-ability-system.md`](ADR-002-gameplay-ability-system.md) — Adopt GAS behind Veyra-owned C++ integration and combat semantics.
- [`ADR-003-owned-field-entities.md`](ADR-003-owned-field-entities.md) — Three owned-entity primitives (combat entity, placed marker, world volume owned by its ruling system); Raska's ride state scoped separately; implementation sequenced from the nine Vanguards needing no entity. Build order only — all 25 Vanguards remain in the first-playable roster.
- [`ADR-004-unified-unreal-client-states.md`](ADR-004-unified-unreal-client-states.md) — One Unreal client application with controlled, isolated ordinary client, Test Skin, champion-select, gameplay, results and reconnect states.

**Renumbering note (2026-09-23):** The unified-client record was first filed as a second `ADR-003` and has been renumbered to `ADR-004`; its content and acceptance are unchanged. Historical records such as [`Pull_Request_Record_v0.1.md`](../Pull_Request_Record_v0.1.md) that say "ADR-003" refer to owned field entities. Every ADR must use a unique number; `scripts/check_doc_context.py --check` enforces this. The next new ADR is `ADR-005`.

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
