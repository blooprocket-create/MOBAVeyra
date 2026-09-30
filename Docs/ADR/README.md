# Architecture Decision Records

This directory records major technical choices that should not be casually reversed by future contributors or coding agents.

## Accepted decisions

- [`ADR-001-unreal-version-policy.md`](ADR-001-unreal-version-policy.md) — Unreal Engine 5.8, pinned to 5.8.3 built from source; deliberate version-pinned upgrades.
- [`ADR-002-gameplay-ability-system.md`](ADR-002-gameplay-ability-system.md) — Adopt GAS behind Veyra-owned C++ integration and combat semantics.
- [`ADR-003-owned-field-entities.md`](ADR-003-owned-field-entities.md) — Three owned-entity primitives (combat entity, placed marker, world volume owned by its ruling system); Raska's ride state scoped separately; implementation sequenced from the nine Vanguards needing no entity. Build order only — all 25 Vanguards remain in the first-playable roster.
- [`ADR-004-unified-unreal-client-states.md`](ADR-004-unified-unreal-client-states.md) — One Unreal client application with controlled, isolated ordinary client, Test Skin, champion-select, gameplay, results and reconnect states.
- [`ADR-005-launcher-session-handoff-and-local-first-hosting.md`](ADR-005-launcher-session-handoff-and-local-first-hosting.md) — Tauri launcher, single-use launch-code session handoff, Windows client/Linux server, local-first Docker hosting, Go backend, Git LFS.
- [`ADR-006-unreal-project-scaffold.md`](ADR-006-unreal-project-scaffold.md) — Unreal project in `Game/`, four targets, the module set with an enforced layer map, ASC on the PlayerState, Iris with fog of war enforced per player at the data boundary, text JSON tuning, Git LFS patterns and locking, CQTest and PowerShell build/test scripts. Accepted when the author merged M1 (#12).
- [`ADR-007-match-join-contract.md`](ADR-007-match-join-contract.md) — How a client joins its assigned match: backend-derived join tickets (a key per match, erased at its end), the server's roster delivered on its standard input at start, a per-match server credential for ready and result reports, the minimum M4 result, and the local Docker allocator. Accepted when the author merged M4 (#15).
- [`ADR-008-vanguard-definitions-and-ability-composition.md`](ADR-008-vanguard-definitions-and-ability-composition.md) — Vanguard definitions in tuning data; abilities composed from data onto six archetypes with per-rank values, cast phases and Cast IDs; unique passives behind a content-owned registry; Progression in a new `VeyraEconomy` module; the `VeyraVanguards` and `VeyraUI` modules and layers; the provisional-values marker; developer Vanguard selection; the open canon questions for the first four kits. Accepted when the author merged M5 (#17, #18).
- [`ADR-009-runtime-combat-primitives.md`](ADR-009-runtime-combat-primitives.md) — The Combat status ledger with one native Gameplay Effect; the movement component for displacement and dashes; shields with identity and caps, Combat State and assist attribution; projectiles and delayed areas with damage prepared at Commit; shapes; basic attacks; no client prediction for these categories. Accepted when the author merged M5 (#17, #18).
- [`ADR-010-play-flow.md`](ADR-010-play-flow.md) — The play flow: a client-state coordinator in `VeyraServices` replacing the linear handoff; a generated front-end map with absolute travel and verified results; menus as UMG built in C++; the Tauri launcher with a stdout/stdin launch handshake; a stubbed tutorial with an owned starter and a stand-in rotation; solo Custom practice ended by the host; champion select as a backend session that creates the match once; assignment v2 with mode, rules and Vanguards; M6b's queue and Match Found; provisional answers where canon is silent. Accepted when the author merged M6 (#21, #22).

## Proposed decisions

- [`ADR-011-battleground-runtime.md`](ADR-011-battleground-runtime.md) — The battleground runtime: VeyraFlux and VeyraWorld as peers in a new Battleground layer, routed through Match; structures as spawned pawns with Combat §33's rules through damage delivery kinds; kill credit and attribution on any victim; Fluxborn on CharacterMovement with a server AI controller; tower aggro and ramp; invulnerability, inhibitor rebuilds, Prime Well regeneration and backdoor protection; live Team Flux scaling; Gold, fractional XP, the reward rules and the respawn curve; one layout file for the generated map and the server's spawning; victory as `prime_well_destroyed` with a winner; practice without victory; every value provisional data. It becomes Accepted when the author merges the M7a pull request that adds it.
- [`ADR-012-items-and-shop.md`](ADR-012-items-and-shop.md) — Items and the shop: VeyraItems in a new Items layer above Abilities; item definitions as validated data with tier rules; Gold transactions kept in Economy; the inventory and remote purchase queue as Items' pure rules; equipment stats through one Combat verb, with Ability Haste and additive bonus Attack Speed; the fountain, delivery and shop requests routed by Match; Recall as a Match order; League's answers where canon is silent. It becomes Accepted when the author merges the M8 pull request that adds it.
- [`ADR-013-ai-vanguards.md`](ADR-013-ai-vanguards.md) — AI Vanguards: VeyraBots in a new Autonomy layer above Match; Match announces bots and never names their brain; bots order through the players' paths by participant; a brain that senses, decides by pure rules and acts; Beginner and Intermediate as data, with builds, skill priorities and ability uses per Vanguard; bot seats carry their difficulty; League's answers where canon is silent. It becomes Accepted when the author merges the M9 pull request that adds it.
- [`ADR-014-jungle-and-flux-wells.md`](ADR-014-jungle-and-flux-wells.md) — The jungle and Flux Wells: wildlife and Wells as neutral units, hostile to Vanguards and what they cast but never to Fluxborn or structures; wildlife in the Fluxborn pattern with camp aggro, leash and reset; camps spawned, cleared and respawned by a jungle subsystem; traits as statuses, with two new status kinds; Economy §7's wildlife rewards; Wells as neutral objectives drained by damage and capped, contested presence, secured by the last hit, granting temporary Team Flux and a Gold pool; the jungle bot seat; League's answers where canon is silent. It becomes Accepted when the author merges the M10a pull request that adds it.
- [`ADR-015-flux-spells.md`](ADR-015-flux-spells.md) — Flux Spells: two spell slots in the one ability loadout, with fixed cooldowns; the roster as ordinary archetype entries, with small reusable options (targeted statuses and unit kinds, a heal, Level-scaled amounts) and two status kinds (`DamageOverTime`, `Weaken`); slots unlocked by permanent Team Flux, routed by Match; server-authoritative preselection with a saved loadout per Vanguard and assignment v4; Gold swaps at the fountain; bots' spells; League's answers where canon is silent. It becomes Accepted when the author merges the M10b pull request that adds it.
- [`ADR-016-vision.md`](ADR-016-vision.md) — Vision: VeyraVision as a peer in the Battleground layer; one visibility contract beside Combat's targeting, written only by Vision and read by targeting, orders and bots; team vision on a server timer with Dense Fog's same-volume rule and stealth against True Sight; the Iris fog gate made real for units and PlayerState data; Dense Fog as layout data and a runtime API; presence pings and Sweeper outlines as their own channels; the vision tools as Vision's actions in a slot of their own, with wards as placed markers; League's answers where canon is silent. It becomes Accepted when the author merges the M11 pull requests that add it.
- [`ADR-017-match-statistics.md`](ADR-017-match-statistics.md) — Match statistics: Combat reports resolved damage (with each shield's provider), restored Health and applied statuses; Economy reports every Gold grant; Match's one statistics service keeps each player's record, bots too; K/D/A and last hits public on the PlayerState for the in-match scoreboard; the result carries a scoreboard the backend stores and returns; results show Scoreboard, Detailed Statistics and team summary; Match History lists and reopens completed matches; League's answers where canon is silent. It becomes Accepted when the author merges the M12 pull requests that add it.
- [`ADR-018-kit-primitives.md`](ADR-018-kit-primitives.md) — Kit primitives for six more Vanguards (Kade, Vera, Mimzi, Patch, Gorraveth, Raska): slot overrides (recast windows, variants, next-cast overrides, replacement sets); new status kinds, source-relative statuses and one-stack decay; cast, displacement and camp-cleared events; Camouflage enforced by Vision; lingering areas and shaped reveals as the first world volume; volleys, tethers and attach; ride states per Combat §56; League's answers where canon is silent. It becomes Accepted when the author merges the M13 pull requests that add it.
- [`ADR-019-match-flow.md`](ADR-019-match-flow.md) — Match flow (Match Flow Bible §3–§11): rejoining gives back the same Vanguard; movement-only autopilot behind a tower, then home; activity, AFK and absence on the match clock; personal loss and its forgiveness; remake, surrender and pause votes with a real-time intermission; surrender and remake results; League's answers for AI seats in votes. It becomes Accepted when the author merges the M14 pull requests that add it.
- [`ADR-020-camera-minimap-kill-economy.md`](ADR-020-camera-minimap-kill-economy.md) — The local camera (Free by default, Locked, Semi-Locked, Hold to Center) as presentation; the minimap drawn from the layout and what the fog gate lets a client have, with its clicks through a hook the UI sets; the kill-streak bounty, death-streak devaluation and buyback; League's answers for bounty and buyback values. It becomes Accepted when the author merges the M15 pull requests that add it.
- [`ADR-021-custom-lobbies.md`](ADR-021-custom-lobbies.md) — Invite-only custom lobbies as their own backend object: the host places humans and bots (any released Vanguard, a difficulty each) on either side; launch through champion select; `custom` rules end to end with victory on or off and starting Gold per session (assignment schema 5); friends and invites in the client; League's answers where Custom Matches §7 is open. It becomes Accepted when the author merges the M16 pull requests that add it.
- [`ADR-022-installer-and-game-patching.md`](ADR-022-installer-and-game-patching.md) — Veyra Setup and the launcher's install, update and repair: a hand-written, branded NSIS Setup installs the launcher per user, with no administrator prompt, and never carries the game; the launcher installs, updates, repairs and uninstalls the game from a static release store (a channel file, manifests addressed by hash, FastCDC chunks named by SHA-256 and stored as zstd frames); the install record and staged commit; launcher configuration schema 2; the local file server; a fully custom setup window as the stated final goal. It becomes Accepted when the author merges the M17 pull request that adds it.
- [`ADR-023-crit-and-the-full-item-catalog.md`](ADR-023-crit-and-the-full-item-catalog.md) — Critical strikes (Combat §5): the Crit Chance and Crit Damage Bonus stats, the tuning, the pure rule and a server roll on basic attacks. The Item Bible's eleven remaining items (Keensteel, Deadeye Edge, Sovereign Edge, Arcane Boots, Flux Flask and six Attunement Masterworks). The dealt-damage event and a Magic Resist Reduction status. League's answers where the bibles are open. It becomes Accepted when the author merges the M17 pull request that adds it.

**Renumbering note (2026-09-23):** The unified-client record was first filed as a second `ADR-003` and has been renumbered to `ADR-004`; its content and acceptance are unchanged. Historical records such as [`Pull_Request_Record_v0.1.md`](../Pull_Request_Record_v0.1.md) that say "ADR-003" refer to owned field entities. Every ADR must use a unique number; `scripts/check_doc_context.py --check` enforces this. The next new ADR is `ADR-024`.

**Renumbering note (2026-09-30):** The record of critical strikes and the rest of the item catalog was filed as `ADR-022` in its M17 commits while the installer record took that number on `main`; it is `ADR-023`, its content unchanged.

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
