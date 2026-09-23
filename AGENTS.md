# AGENTS.md

This repository is designed to be worked on heavily by coding agents. Speed is useful; architecture violations are not.

## Task-specific context routing

Start with [`Docs/CONTEXT_MAP.md`](Docs/CONTEXT_MAP.md), **not** the entire `Docs/Design/` directory. Use its task-to-owner table to select the smallest current bible and the relevant [section locator](Docs/Index/README.md) for large bibles. Read just the necessary source range and any cross-domain owner it cites; a locator is not canon. Never treat historical `Archives/` or an older proposal checkpoint as current rules.

For documentation changes, update the owning bible and routing links, run `python3 scripts/check_doc_context.py --write`, and verify with `python3 scripts/check_doc_context.py --check`. The CI check catches missing active bibles and stale heading/line maps.

## Mandatory first reads

Before modifying gameplay code, read:

1. [`ARCHITECTURE.md`](ARCHITECTURE.md)
2. [`PROJECT_STRUCTURE.md`](PROJECT_STRUCTURE.md)
3. The relevant *current owning sections*, selected using [`Docs/CONTEXT_MAP.md`](Docs/CONTEXT_MAP.md)
4. Relevant ADRs under [`Docs/ADR/`](Docs/ADR/)

If a task conflicts with those documents, do not silently choose a side. Surface the conflict.

## Non-negotiable working rules

- Preserve server authority.
- C++ owns reusable systems/rules; data owns tuning; Blueprints stay thin.
- UI never owns gameplay state.
- Use one authoritative owner for each rule/state domain.
- Do not duplicate calculations.
- Do not introduce circular dependencies.
- Do not add hard references between unrelated systems when an interface/event/message/orchestrator is appropriate.
- Do not add champion- or item-name special cases to core systems.
- Do not expand a convenient class into a god object.
- **Never hardcode gameplay tuning values or leave magic numbers in gameplay C++/Blueprints.** Every prototype and final balance/timing/cost/range/cap value must come from validated, designer-editable data; a named C++ constant is not a substitute. Treat other games' numbers as provisional data, especially map-dependent wave and objective timings. Allow only justified true algorithmic invariants (e.g. mathematical identities), not concealed balance literals.
- Do not bypass architecture merely to make a task compile.

## Task workflow

For substantial work:

1. **Locate the owner.** Identify which domain owns the requested state/rule.
2. **Inspect before editing.** Find existing primitives, interfaces, tests, and data definitions before adding new ones.
3. **Plan the dependency path.** State which modules/classes will change and why.
4. **Implement the smallest coherent change.** Prefer extending reusable primitives over duplicating them.
5. **Build early.** Do not stack large amounts of uncompiled Unreal C++.
6. **Test the rule.** Add/update automation coverage where practical.
7. **Run architecture review.** Check the list in `ARCHITECTURE.md` before declaring completion.
8. **Report clearly.** Summarize files changed, behavior added, tests run, and any remaining risks/open decisions.

## Stop conditions

Stop and ask for/record a design or architecture decision instead of guessing when:

- a requested feature requires changing an architecture rule;
- authoritative ownership is ambiguous;
- a new circular dependency seems necessary;
- a system needs to know about a named Vanguard/item to function;
- Blueprint/UI would need to own gameplay logic;
- the task requires choosing an engine/plugin/backend strategy not yet decided;
- design documentation contradicts current implementation and neither is clearly superseded.

## Unreal-specific expectations

Once the Unreal project exists:

- Treat generated files and build outputs as generated; do not hand-edit them.
- Prefer command-line builds/tests for repeatability.
- Keep server/headless compatibility in mind for gameplay systems.
- Use editor scripting/automation for repetitive asset work where practical.
- Never assume a Blueprint graph is harmless merely because it is fast to create.
- When an asset-side change is required, document what was changed and how it can be reproduced.

## Pull request completion checklist

For substantial PRs, include:

- scope/intent;
- authoritative owner(s) touched;
- new dependencies introduced;
- data/schema changes and confirmation that every changed gameplay tuning value is editable data (no magic numbers);
- networking/replication implications;
- tests/builds run;
- architecture checklist result;
- screenshots/video only when visual verification is relevant.

A passing build is necessary but not sufficient. The implementation must also preserve architectural boundaries.
