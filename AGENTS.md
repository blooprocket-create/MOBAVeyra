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
4. For UI/HUD, world, environment, Blender, asset, material, PCG, water, lighting, rendering, animation, or VFX work: [`Docs/Production/README.md`](Docs/Production/README.md) and the relevant production guide it indexes
5. Relevant ADRs under [`Docs/ADR/`](Docs/ADR/)

If a task conflicts with those documents, do not silently choose a side. Surface the conflict.

## Repository-facing reference policy — mandatory

All repository-visible work must use **Veyra-native terminology** and Veyra's canonical documentation.

- Do not name or reference third-party game titles, publishers, studios, characters, items, maps, branded mechanics, or other external game IP in pull request titles/descriptions/comments, commit messages, issues, changelogs, documentation, filenames, test names, code comments, generated artifacts, or other repository-visible text.
- Third-party games may be used privately with the user as comparative design or implementation context, but those references must be translated into Veyra terminology before anything is written to the repository.
- When Veyra already defines a rule or behavior, cite the authoritative Veyra document/section instead of using comparative shorthand.
- If outside material informed an early prototype or tuning value, record only the resulting provisional Veyra behavior/data in repository artifacts; do not preserve the outside reference as provenance or shorthand.
- If a requested repository change would require an external-IP reference to explain it, stop and rewrite the explanation in self-contained Veyra terms before committing or opening a PR.

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

## World, asset, and VFX production

- Crucible world-authoring architecture is governed by [ADR-040](Docs/ADR/ADR-040-crucible-world-authoring-toolchain.md). The pinned Epic source checkout is **read-only by default**: agents may inspect/build/run it, but may not modify `Engine/Source`, `Engine/Plugins`, engine build files, or engine-owned content unless the author explicitly approves an engine fork and a separate ADR.
- Production Crucible generation belongs in the project-owned `Game/Plugins/VeyraWorldTools/` editor plugin plus the existing Veyra runtime/layout contracts. Packaged gameplay code may not depend on the editor plugin.
- Do not assume battleground Z=0. World/runtime spawn work on the production Crucible must use the Veyra-owned playable-surface resolution path required by ADR-040 rather than ad-hoc traces or per-system height assumptions.

For substantial visual/world work:

- `Game/Tuning/World.json` is the current machine-readable gameplay-layout source under ADR-011. Do not create a second private map layout in Blender, Blueprints, PCG, or hand-edited map actors.
- Protected competitive geometry may not be changed by ordinary art/PCG work.
- Blender owns reproducible source geometry/generators; Unreal owns runtime materials, Niagara, lighting, gameplay integration, and final visual validation.
- Edit the authoritative source/generator instead of hand-patching generated output.
- Record deterministic seeds/inputs for procedural production content.
- Validate important visual work from the actual Unreal gameplay camera and use the repeatable screenshot/debug views defined by `Docs/Production/`.
- Presentation systems may observe authoritative gameplay state but never own gameplay outcomes.
- Experimental UE features are not shipping dependencies unless approved through the repository's ADR process.
- Record provenance and AI-use restrictions for third-party visual content before allowing agents to inspect or transform it.

## Visual polish and production acceptance

For substantial client UI, HUD, Vanguard, animation, VFX, environment, lighting or renderer changes, follow [the Visual Production Standard](Docs/Production/VEYRA_VISUAL_PRODUCTION_STANDARD.md), [Visual Upgrade Roadmap](Docs/Production/VEYRA_VISUAL_UPGRADE_ROADMAP.md) and [Review Template](Docs/Production/VEYRA_VISUAL_REVIEW_TEMPLATE.md). Preserve the approved client UX and Settings behavior, remain presentation-only, build the smallest approved slice, and record in-engine before/after captures **and** a reproducible performance comparison. Generator success and compilation are not visual acceptance. Do not declare new styling or a renderer/plugin strategy approved without the owner's decision.

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
- for generated world/asset work: generator/source, seed/profile, affected region, whether `World.json` or protected gameplay geometry changed, and regeneration steps;
- screenshots/video when visual verification is relevant, using standard validation cameras where defined.

A passing build is necessary but not sufficient. The implementation must also preserve architectural boundaries.
