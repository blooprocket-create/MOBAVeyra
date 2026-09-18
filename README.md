# Veyra

Veyra is an original 5v5 competitive MOBA in development by Wayfinder Studios.

The project is currently in pre-production. The repository is being established around a server-authoritative Unreal Engine architecture, data-driven gameplay content, and strict boundaries intended to keep the codebase maintainable as the roster and systems grow.

## Current design pillars

- **Structured battlefield, unstructured strategy.** The map provides lanes, jungle, objectives, gold, XP, and opportunity costs; the rules do not force a top/jungle/mid/carry/support composition.
- **Flux drives macro play.** Shared team Flux strengthens lane pressure and unlocks tactical Flux Spells rather than directly becoming a generic champion-stat ladder.
- **Readable first, spectacular second.** Systems should be understandable under competitive pressure even when their presentation is dramatic.
- **Familiar to learn, distinct to master.** Veyra uses proven MOBA language where it helps players, while its objectives, roster, Flux systems, and world establish their own identity.
- **Architecture is a feature.** Gameplay rules, content data, presentation, networking, and UI are intentionally separated so rapid iteration does not become spaghetti code.

## Repository documentation

### Engineering

- [`ARCHITECTURE.md`](ARCHITECTURE.md) - non-negotiable architecture rules and ownership principles.
- [`PROJECT_STRUCTURE.md`](PROJECT_STRUCTURE.md) - intended Unreal module/content organization and dependency direction.
- [`AGENTS.md`](AGENTS.md) - mandatory instructions for Codex and other coding agents.
- [`CLAUDE.md`](CLAUDE.md) - Claude Code entrypoint and repository working rules.
- [`Docs/ADR/`](Docs/ADR/) - Architecture Decision Records for major technical choices.

### Design bibles

Repository-native Markdown exports of the current working design documents live in [`Docs/Design/`](Docs/Design/):

- Initial Roster Character Bible v0.3
- World Bible v0.2
- Battleground Bible v0.3
- Item Bible v0.3

These documents describe working game design, not immutable implementation contracts. When an implementation decision conflicts with a design document, do not silently choose one: raise the mismatch and resolve it deliberately.

### Concept art

Character-sheet visual development belongs under [`ConceptArt/Characters/`](ConceptArt/Characters/). The directory index tracks the current Vanguard sheet set and intended filenames.

## Status

The Unreal project has not yet established all implementation details. In particular, exact Unreal Engine version, final module boundaries, final Gameplay Ability System adoption strategy, backend services, and detailed client prediction rules remain open decisions. Do not invent those decisions merely to finish a task; record major choices through an ADR.

## License

Source is publicly viewable but is **not open source**. See [`LICENSE.md`](LICENSE.md).
