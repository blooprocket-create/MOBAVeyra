# CLAUDE.md

Claude Code should treat this repository as an architecture-first Unreal project.

## Find the right context first

Open [`Docs/CONTEXT_MAP.md`](Docs/CONTEXT_MAP.md) and identify the authoritative owner of the task. For a large bible, consult the dedicated [section locator](Docs/Index/README.md) and read a bounded section; do not ingest every current or archived design document. The route and locator are navigation only, never replacements for canon. When editing docs, regenerate the maps with `python3 scripts/check_doc_context.py --write` and run `--check`.

## Read before work

Always begin substantial gameplay tasks by reading:

- `ARCHITECTURE.md`
- `PROJECT_STRUCTURE.md`
- the relevant owning sections in `Docs/Design/` selected by the context map
- relevant records in `Docs/ADR/`

`ARCHITECTURE.md` is the source of truth for technical boundaries. Do not duplicate or reinterpret its rules here.

## Operating model

The intended workflow is:

> inspect → identify authoritative owner → implement through reusable systems → build → test → architecture check → report

Do not optimize for the fewest edited files if doing so creates the wrong ownership or dependency. Prefer a small clean primitive over a local hack that future features will duplicate.

## No hardcoded tuning — mandatory project rule

Follow the project-wide **no hardcoded gameplay tuning / no magic numbers** rule in `ARCHITECTURE.md` §1.3 and `AGENTS.md`. Put **all** gameplay tuning—including prototype wave spawn schedules, phase boundaries, AFK timers, Gold/XP rewards, costs, cooldowns, radii, movement speeds, caps, and thresholds—in validated, designer-editable data rather than literal values in C++ or Blueprint gameplay logic. A named C++ constant is not a substitute for editable data.

When implementing a feature, identify its authoritative configuration owner, validate required settings, and cover data-driven behavior with tests. Treat values borrowed from another game's map as provisional Veyra playtest settings, not hardcoded assumptions. Only genuine, justified mathematical/algorithmic invariants may remain code constants; do not confuse those with balance values.

## Hard prohibitions

Do not:

- make clients authoritative for gameplay outcomes;
- put core gameplay logic in UI, animation Blueprints, or arbitrary level Blueprints;
- duplicate damage, economy, Flux, inventory, cooldown, or status calculations;
- introduce circular module dependencies;
- hard-reference unrelated domains when an interface/event/message is appropriate;
- add `if Raska`, `if ItemX`, or equivalent named-content branches to reusable core systems;
- silently change architecture or design to get a task over the line;
- hardcode gameplay tuning values or scatter unexplained numeric literals through gameplay code or Blueprints;
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
