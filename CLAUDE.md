# CLAUDE.md

Claude Code should treat this repository as an architecture-first Unreal project.

## Read before work

Always begin substantial gameplay tasks by reading:

- `ARCHITECTURE.md`
- `PROJECT_STRUCTURE.md`
- relevant files in `Docs/Design/`
- relevant records in `Docs/ADR/`

`ARCHITECTURE.md` is the source of truth for technical boundaries. Do not duplicate or reinterpret its rules here.

## Operating model

The intended workflow is:

> inspect → identify authoritative owner → implement through reusable systems → build → test → architecture check → report

Do not optimize for the fewest edited files if doing so creates the wrong ownership or dependency. Prefer a small clean primitive over a local hack that future features will duplicate.

## Hard prohibitions

Do not:

- make clients authoritative for gameplay outcomes;
- put core gameplay logic in UI, animation Blueprints, or arbitrary level Blueprints;
- duplicate damage, economy, Flux, inventory, cooldown, or status calculations;
- introduce circular module dependencies;
- hard-reference unrelated domains when an interface/event/message is appropriate;
- add `if Raska`, `if ItemX`, or equivalent named-content branches to reusable core systems;
- silently change architecture or design to get a task over the line;
- create god classes or permanent `Misc`/`Helpers` dumping grounds;
- bypass the accepted GAS strategy in `ADR-002` or invent unresolved project-level decisions such as backend vendor during unrelated work.

## When blocked

If the clean solution needs a new architecture decision, explain:

1. the concrete problem;
2. the smallest decision required;
3. reasonable options and tradeoffs;
4. which existing rule/document is affected.

Then wait for the decision or, if explicitly authorized, add an ADR before implementing the new direction.

## Completion

A substantial change is complete only when it builds, relevant tests pass, architecture boundaries remain intact, and the summary names any intentionally deferred work.
