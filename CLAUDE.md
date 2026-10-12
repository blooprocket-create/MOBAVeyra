# CLAUDE.md

Claude Code should treat this repository as an architecture-first Unreal project.

## Find the right context first

Open [`Docs/CONTEXT_MAP.md`](Docs/CONTEXT_MAP.md) and identify the authoritative owner of the task. For a large bible, consult the dedicated [section locator](Docs/Index/README.md) and read a bounded section; do not ingest every current or archived design document. The route and locator are navigation only, never replacements for canon. When editing docs, regenerate the maps with `python3 scripts/check_doc_context.py --write` and run `--check`.

## Read before work

Always begin substantial gameplay tasks by reading:

- `ARCHITECTURE.md`
- `PROJECT_STRUCTURE.md`
- the relevant owning sections in `Docs/Design/` selected by the context map
- for client UI/HUD, world/environment/Blender/asset/material/PCG/water/lighting/rendering/animation/VFX tasks: `Docs/Production/README.md` and the relevant production guide
- relevant records in `Docs/ADR/`

`ARCHITECTURE.md` is the source of truth for technical boundaries. Do not duplicate or reinterpret its rules here.

## Operating model

The intended workflow is:

> inspect → identify authoritative owner → implement through reusable systems → build → test → architecture check → report

Do not optimize for the fewest edited files if doing so creates the wrong ownership or dependency. Prefer a small clean primitive over a local hack that future features will duplicate.

## Production-quality visual work

For UI/HUD, character, animation, combat effect, world, lighting and renderer polish, read [Visual Production Standard](Docs/Production/VEYRA_VISUAL_PRODUCTION_STANDARD.md), [Visual Upgrade Roadmap](Docs/Production/VEYRA_VISUAL_UPGRADE_ROADMAP.md), and use [Visual Review Template](Docs/Production/VEYRA_VISUAL_REVIEW_TEMPLATE.md). Reuse existing C++ view models, client intents, shared generators/materials and Niagara recipes; don't replace approved client navigation or accessibility semantics while reskinning. Gather matched in-engine captures, record frame-time/Slate/particle costs where relevant, and seek author approval for distinctive visual changes before scaling across screens/the roster. A passed generator/build does not certify appearance.

## World, environment, asset, and VFX tasks

For production Crucible work, also read [ADR-040](Docs/ADR/ADR-040-crucible-world-authoring-toolchain.md). Treat the Epic UE 5.8.3 source checkout as **read-only by default**: reading, building, running and debugging engine source are allowed; editing `Engine/Source`, `Engine/Plugins`, engine build files or engine-owned content is not allowed without explicit author approval plus a dedicated ADR.

World generation/editor automation belongs in `Game/Plugins/VeyraWorldTools/`. Runtime/gameplay modules must not depend on that editor plugin. The production battleground is genuinely 3D: do not preserve or introduce Z=0 spawn assumptions when touching structures, Fluxborn, wildlife, Flux Wells, team starts or other terrain-bound actors.

Use the production loop in `Docs/Production/`: identify the authoritative gameplay source and visual source, edit the source/generator, regenerate only the affected scope, validate in Unreal, capture repeatable gameplay/debug views, and iterate from the actual in-engine result.

Do not:

- duplicate `Game/Tuning/World.json` gameplay layout inside Blender, PCG, Blueprints, or hand-authored map actors;
- let decorative generation alter protected competitive topology;
- hand-fix generated output when its generator/source is the real owner;
- treat Blender particle/simulation behavior as shipped runtime gameplay;
- put gameplay authority in Niagara, materials, lighting, water rendering, or presentation Blueprints;
- enable Experimental engine features as shipping dependencies without the decision required by ADR-001;
- inspect/transform third-party visual content with an agent unless its provenance and AI-use rights permit it.

For reusable visual families, prefer deterministic generators with explicit seeds and metadata over unrelated one-off meshes.

## No hardcoded tuning — mandatory project rule

Follow the project-wide **no hardcoded gameplay tuning / no magic numbers** rule in `ARCHITECTURE.md` §1.3 and `AGENTS.md`. Put **all** gameplay tuning—including prototype wave spawn schedules, phase boundaries, AFK timers, Gold/XP rewards, costs, cooldowns, radii, movement speeds, caps, and thresholds—in validated, designer-editable data rather than literal values in C++ or Blueprint gameplay logic. A named C++ constant is not a substitute for editable data.

When implementing a feature, identify its authoritative configuration owner, validate required settings, and cover data-driven behavior with tests. Treat values borrowed from another game's map as provisional Veyra playtest settings, not hardcoded assumptions. Only genuine, justified mathematical/algorithmic invariants may remain code constants; do not confuse those with balance values.

## Repository-facing reference policy — mandatory

Everything Claude writes into the repository must use **Veyra-native terminology**.

Do not name or reference third-party game titles, publishers, studios, characters, items, maps, branded mechanics, or other external game IP in pull request titles/descriptions/comments, commit messages, issues, changelogs, documentation, filenames, test names, code comments, generated artifacts, or any other repository-visible text.

Comparisons to other games are permitted only in private conversation with the user. Before creating a repository artifact, translate the comparison into a self-contained Veyra requirement and cite Veyra's authoritative documentation when one exists. If outside material informed provisional tuning or an implementation idea, record only the resulting Veyra behavior/data, not the outside reference.

If repository-facing wording still depends on naming an outside game or property, rewrite it in Veyra terms before committing, commenting, or opening a pull request.

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

A substantial change is complete only when it builds, relevant tests pass, architecture boundaries remain intact, and the summary names any intentionally deferred work. Visual/world changes must also satisfy the applicable `Docs/Production/VEYRA_WORLD_VALIDATION_STANDARD.md` gates and report source/generator, seed/profile, affected region, gameplay-geometry impact, screenshots, and performance/cook checks where relevant.
