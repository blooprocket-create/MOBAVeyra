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

- Initial Roster Character Bible v0.6
- World Bible v0.5
- Battleground Bible v0.9
- Item Bible v0.3
- Combat Bible v0.4
- Vision & Reconnaissance Bible v0.1
- Economy & Progression Bible v0.1
- Match Flow Bible v0.1
- Account, Collection & Mastery Bible v0.1
- Modes & Access Bible v0.1
- Custom Matches & Private Lobbies Bible v0.1
- Launcher, Pre-Game Client & Platform Bible v0.1
- Parties, Social & Matchmaking Bible v0.1
- Chat & Communication Bible v0.1
- Player Profiles, Identity & Website Bible v0.1
- Moderation, Reporting & Telemetry Bible v0.1
- Replay, Saved Matches & Live Spectating Bible v0.1

Invite-only custom matches support host-adjustable gameplay settings, human and individually configured AI on either team, and fewer than ten Vanguards—even one human practicing alone. Human Vanguard selection still requires ownership or weekly rotation; custom games grant no Account XP or Mastery. See the [Custom Matches Bible](Docs/Design/Veyra_Custom_Matches_Bible_v0.1.md).

The current planned roster contains **25 designed Vanguards**, including Mimzi, Celandrine, Gorraveth, Aurelisse, and Eudora Blackbridge. This is the current first-playable design target, not a fixed cap; it can grow as more Vanguards are designed. Twenty-five designs exceed the draft's arithmetic minimum of 16 distinct Vanguards for six bans plus ten globally unique picks. **Ranked is intentionally deferred from initial launch** and still requires Account Level 30, **20 permanently owned, released Vanguards** per eligible player, and further Ranked-system design; having 25 character concepts does not make Ranked immediately playable. The account, mode, client, party, profile, moderation and replay bibles are working design checkpoints; matchmaking implementation, account security, commerce, telemetry/replay architecture and other backend details remain to be decided before implementation.

Veyra has **no built-in voice chat**. Live text supports Team and default-on All Chat (which players may disable in Settings), continuous Party Chat and friend DMs while in a match; both teams may optionally use post-match chat without delaying requeue. See the [Chat & Communication Bible](Docs/Design/Veyra_Chat_Communication_Bible_v0.1.md). The broader Settings catalog is an upcoming design task.

Persistent account currencies are **Flux** (earned; Vanguard unlocks and paid display-name changes) and **Refined Flux** (premium; Vanguards, skins and paid display-name changes). Both are **separate from in-match Team Flux**. Replay design targets a non-video authoritative gameplay recording for every match, ten server-saved matches per account and delayed live friend spectating; those are product requirements, not an approved recording/storage implementation.

These documents describe working game design, not immutable implementation contracts. When an implementation decision conflicts with a design document, do not silently choose one: raise the mismatch and resolve it deliberately.

### Concept art

Character-sheet visual development belongs under [`ConceptArt/Characters/`](ConceptArt/Characters/). The repository currently contains **20 uploaded character-sheet images**; the five newest Vanguards have visual sheets from design, but those image files have **not yet been uploaded into this GitHub directory**. The [character-sheet index](ConceptArt/Characters/README.md) links to existing images and identifies pending uploads.

## Status

Veyra targets **Unreal Engine 5.8** and has formally adopted Unreal's **Gameplay Ability System (GAS)** as its ability-framework foundation. Final module boundaries, detailed GAS ownership/prediction policy, backend services, and other unresolved infrastructure choices remain deliberate architecture decisions. Do not invent unresolved choices merely to finish a task; record major choices through an ADR.

## License

Source is publicly viewable but is **not open source**. See [`LICENSE.md`](LICENSE.md).
