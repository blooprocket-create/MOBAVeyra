# Veyra agent context map

**Purpose:** Route an agent to the *smallest authoritative source* for a task. This page is a navigation aid, not a replacement for the linked design documents, the architecture constitution, or accepted decisions.

**Current scope:** All active design bibles in `Docs/Design/` (not `Archives/`). Client UX approvals are recorded through **UX-92**; discussion is paused until the author says **“continue.”** Do not extrapolate or approve UX-93.

## Minimal-read protocol

1. Read this page first. Identify the **rule owner** in the table below; do not load every bible or the entire roster into context.
2. For substantial gameplay or engine work, read [`AGENTS.md`](../AGENTS.md), [`ARCHITECTURE.md`](../ARCHITECTURE.md), and the relevant ownership/dependency part of [`PROJECT_STRUCTURE.md`](../PROJECT_STRUCTURE.md). A routing summary never overrides those documents.
3. Read only the relevant *current* bible section. For a large bible, consult its generated [section locator](Index/README.md) first; fetch a bounded range around the matching heading, including nearby qualifiers and linked cross-domain rules.
4. Inspect the relevant [accepted ADRs](ADR/README.md) before choosing an implementation strategy. Confirm whether the decision is accepted, provisional, open, rejected, or superseded in the actual source.
5. Search narrowly for exact mechanic terms and cross-references, including negative rules and exceptions; read the *owning* document, not just a mention elsewhere. If authorities disagree or an important decision is open, report that and seek a ruling instead of inventing one.
6. When changing a decision, update its owning document, dependent routes and structured data/assets, and regenerate section locators. Run `python3 scripts/check_doc_context.py --write` then `--check`; report any design conflict explicitly.

**Authority boundaries:** Architecture rules and accepted ADRs govern technical decisions. Within design, each bible governs its own domain; cross-references are pointers, not duplicated authority. The roster bible owns Vanguard kit/lore/appearance prose; individual YAML files are checkable structural subsets, not tuning data. Approved *base* hero art has the appearance precedence documented in the character/art bibles; cosmetic skins never supersede base appearance. Historical archives and older proposal checkpoints are not active canon.

## Active bibles — task-to-owner router

Open the relevant row only. Every current `Veyra_*_Bible_v*.md` at the top level of `Docs/Design/` must appear here; CI checks that inventory.

| Task / question | Current owning document | Read with / boundary |
|---|---|---|
| Individual Vanguard identity, kit, lore, base appearance | [Initial Roster Character Bible](Design/Veyra_Initial_Roster_Character_Bible_v0.6.md) | Find **one** numbered Vanguard in its [section locator](Index/sections/Veyra_Initial_Roster_Character_Bible_v0.6.md); use matching [Vanguard YAML](Design/Vanguards/README.md); combat and vision rules remain with their owners. |
| Regions, setting, histories and geography | [World Bible](Design/Veyra_World_Bible_v0.5.md) | Character Bible governs each Vanguard's identity and individual history. |
| Map, lanes, jungle, wildlife, towers, Flux Wells, Team Flux sources and Fluxborn scaling, wave schedule, map-spawned objectives, ability-created terrain | [Battleground Bible](Design/Veyra_Battleground_Bible_v0.9.md) | [Section locator](Index/sections/Veyra_Battleground_Bible_v0.9.md); Combat owns damage/targeting; Vision owns reveal/detection. |
| Damage, CC, status, attack rules, movement, targeting, towers' combat behavior, ride states | [Combat Bible](Design/Veyra_Combat_Bible_v0.5.md) | [Section locator](Index/sections/Veyra_Combat_Bible_v0.5.md); generic ride states live in §56, not solely in Raska's kit. |
| Fog of war, Dense Fog (including ability-created fog), wards, stealth detection, acquisition | [Vision Bible](Design/Veyra_Vision_Bible_v0.1.md) | Battleground owns map fog placement; Combat owns general targetability and hit validity. |
| Gold, XP, economy, leveling, match shop/delivery and buyback | [Economy & Progression Bible](Design/Veyra_Economy_Progression_Bible_v0.1.md) | Account currency is separate; Combat owns damage and kill rules. |
| Item tiers, recipes, attunements, item abilities and consumables | [Item Bible](Design/Veyra_Item_Bible_v0.3.md) | Economy owns gold transactions; Combat owns shared effects; all tuning is validated designer-editable engine data. |
| Match stages, champion-select cancellation, AFK/disconnect, surrender, pause and result adjudication | [Match Flow Bible](Design/Veyra_Match_Flow_Bible_v0.1.md) | Parties owns queue/readiness; Client & Platform owns application state; UX owns presentation. |
| Queues, mode access, future Ranked rules and weekly rotation | [Modes & Access Bible](Design/Veyra_Modes_Access_Bible_v0.1.md) | Ranked is deferred from initial launch; matchmaking and selection UI are separate owners. |
| Invite-only custom lobby, AI participants and practice-session rules | [Custom Matches Bible](Design/Veyra_Custom_Matches_Bible_v0.1.md) | Modes owns mode eligibility; Match Flow owns authoritative match outcomes. |
| Account XP, ownership, Collection, persistent currencies and Mastery | [Account, Collection & Mastery Bible](Design/Veyra_Account_Collection_Mastery_Bible_v0.1.md) | Persistent account Flux / Refined Flux are **not** in-match Team Flux. |
| Launcher, authentication entry, unified Unreal application, Test Skin and state transitions | [Client & Platform Bible](Design/Veyra_Client_Platform_Bible_v0.1.md) | [Section locator](Index/sections/Veyra_Client_Platform_Bible_v0.1.md); the accepted [unified-client ADR](ADR/ADR-004-unified-unreal-client-states.md) governs architecture and [ADR-005](ADR/ADR-005-launcher-session-handoff-and-local-first-hosting.md) governs launcher technology, session handoff and hosting; UX owns visible screen behavior. A match takes the screen from Match Starting to its end (ruled 2026-09-28, §2 step 6). |
| Home, Shop, ordinary client shell, Match Found overlays, selection and results UI, Test Skin presentation | [Pre-Game Client UX Bible](Design/Veyra_Pre_Game_Client_UX_Bible_v0.1.md) | [Section locator](Index/sections/Veyra_Pre_Game_Client_UX_Bible_v0.1.md); UX-1–92 are recorded, **paused after UX-92**; champion select's League-style arrangement is the author's 2026-09-28 ruling in §1. Parties, Chat, Match Flow and Client & Platform retain their rule authority. |
| Parties, friends, presence, invites, queue readiness and matchmaking | [Parties, Social & Matchmaking Bible](Design/Veyra_Parties_Social_Matchmaking_Bible_v0.1.md) | UX controls presentation, not authoritative permission. |
| Team/All/Party/DM/post-match text, chat mute, channel permission | [Chat & Communication Bible](Design/Veyra_Chat_Communication_Bible_v0.1.md) | No built-in voice chat; moderation owns reports. |
| Identity, profile surfaces, player names, account creation entry | [Profiles & Identity Bible](Design/Veyra_Profiles_Identity_Bible_v0.1.md) | Account Bible owns progression and unlocks. |
| Reports, appeals, staff review, logging and telemetry | [Moderation & Telemetry Bible](Design/Veyra_Moderation_Telemetry_Bible_v0.1.md) | Chat owns conversation permissions; results/statistics data stays with trusted owners. |
| Replay recording, saves, delayed live spectating, replay privacy | [Replay & Spectator Bible](Design/Veyra_Replay_Spectator_Bible_v0.1.md) | Client & Platform governs state transitions; Custom Matches owns invite-only custom spectator options. |
| Player controls, camera, HUD, graphics, audio, language and accessibility | [Settings & Accessibility Bible](Design/Veyra_Settings_Accessibility_Bible_v0.1.md) | [Section locator](Index/sections/Veyra_Settings_Accessibility_Bible_v0.1.md); approved settings through SET-168, not permission to invent UI or server rules. The Display Mode is the match's (ruled 2026-09-28 under 166: Borderless Fullscreen by default; results return to the window). |
| Server-recorded match fields, scoreboard and detailed results/statistics | [Match Statistics Bible](Design/Veyra_Match_Statistics_Bible_v0.1.md) | Match Flow owns outcome; Combat/Economy/Flux own the facts recorded; UX presents verified results. |

## Other task routes

- **Engineering:** [Architecture](../ARCHITECTURE.md) → [Project Structure](../PROJECT_STRUCTURE.md) → relevant [ADR index](ADR/README.md) → owning design section. Do not create a giant manager or make UI, Blueprints, or the client authoritative.
  - The Unreal project is `Game/Veyra.uproject` ([ADR-006](ADR/ADR-006-unreal-project-scaffold.md) §1); `Source/`, `Content/` and `Config/` paths are relative to `Game/`.
  - Generate project files, build and test with `Game/Scripts/GenerateProjectFiles.ps1`, `Game/Scripts/Build.ps1` and `Game/Scripts/Test.ps1`.
  - Package with `Game/Scripts/Package.ps1`. `Game/Scripts/Smoke.ps1` plays a scripted two-client match against the containerised Linux server ([ADR-006](ADR/ADR-006-unreal-project-scaffold.md) §10); with `-Handoff`, the clients reach it through the backend's session handoff ([ADR-007](ADR/ADR-007-match-join-contract.md)).
  - The module layer map is `Game/Source/ModuleLayers.json`, enforced by `scripts/check_module_layers.py`.
  - The Gameplay Tag naming convention lives in [Project Structure §5](../PROJECT_STRUCTURE.md#gameplay-tag-vocabulary).
  - Gameplay tuning is text data in `Game/Tuning/` ([rules](../Game/Tuning/README.md)). `scripts/check_tuning.py` checks it in CI, and the game checks it again when it loads.
  - The Go backend is in `Backend/` ([README](../Backend/README.md)). How a client joins its assigned match, and how the backend starts match servers and records results, is [ADR-007](ADR/ADR-007-match-join-contract.md); the launcher and session handoff are [ADR-005](ADR/ADR-005-launcher-session-handoff-and-local-first-hosting.md).
  - The launcher is in `Launcher/` ([README](../Launcher/README.md)). The play flow (the client-state coordinator in `VeyraServices`, the menus in `VeyraUI`, the launch handshake, onboarding, champion select and Custom practice) is [ADR-010](ADR/ADR-010-play-flow.md). `Game/Scripts/Play.ps1` plays it through the launcher; `Smoke.ps1 -Flow Practice` checks it without a window.
  - The battleground runtime (lanes, Fluxborn, structures, Team Flux, Gold and XP rewards, respawn and victory) is [ADR-011](ADR/ADR-011-battleground-runtime.md); its rules belong to the Battleground, Combat and Economy bibles.
  - Items, the shop and Recall are [ADR-012](ADR/ADR-012-items-and-shop.md); their rules belong to the Item and Economy bibles.
  - AI Vanguards (bots, their difficulties and behaviour data) are [ADR-013](ADR/ADR-013-ai-vanguards.md); who may seat them belongs to the Custom Matches and Modes & Access bibles.
  - The jungle (wildlife camps, traits, leashing), the Flux Wells, and neutral units as a targeting category are [ADR-014](ADR/ADR-014-jungle-and-flux-wells.md); their canon is the Battleground Bible §6–§8 and §17 and the Economy & Progression Bible §7 and §8.2.
  - Flux Spells (the two spell slots, permanent-Flux unlocks, preselection in champion select, fountain swaps) are [ADR-015](ADR/ADR-015-flux-spells.md); their canon is the Battleground Bible §14, the Economy & Progression Bible §13.2, the Combat Bible §21 and the Pre-Game Client UX Bible 36–40.
  - Vision (team fog at the data boundary, Dense Fog, stealth and True Sight, the vision tools and wards) is [ADR-016](ADR/ADR-016-vision.md); its canon is the Vision Bible, the Battleground Bible §11 and the Combat Bible §10–§12.
- **Vanguard editing:** one character's section of the Character Bible → `Docs/Design/Vanguards/<nn>-<name>.yaml` → [Vanguard validation instructions](Design/Vanguards/README.md) → specific base [hero art](../ConceptArt/Vanguards/README.md). Do **not** interpret YAML as engine balance data.
- **Cosmetics:** [skin gallery](../ConceptArt/Vanguards/skins/README.md) and [asset index](../ConceptArt/Vanguards/skins/index.json) → the particular `ConceptArt/Vanguards/<id>/skins/<collection>/hero.webp`; use base character/art bible only for identity and silhouette.
- **Art direction:** [Art Direction](Design/Art_Direction_v0.1.md) and [canon discrepancy register](Design/Sheet_Canon_Discrepancy_Register_v0.1.md). [Ride-state question history](Design/Ride_State_Open_Questions_v0.1.md) is *resolved history*, not an open-rules source.
- **Historical comparison only:** [Design Archives](Design/Archives/README.md) and [Concept Art Archives](../ConceptArt/Archives/README.md). Do not use archived versions to override an active bible.

## Accepted and proposed ADRs

Every ADR number is unique, and every record, accepted or proposed, is routed here (CI checks both). Full summaries are in the [ADR index](ADR/README.md).

- [ADR-001-unreal-version-policy.md](ADR/ADR-001-unreal-version-policy.md) — Unreal Engine 5.8, pinned to 5.8.3 (source build).
- [ADR-002-gameplay-ability-system.md](ADR/ADR-002-gameplay-ability-system.md) — GAS behind Veyra-owned C++ integration and combat semantics.
- [ADR-003-owned-field-entities.md](ADR/ADR-003-owned-field-entities.md) — owned combat units, markers and world volumes.
- [ADR-004-unified-unreal-client-states.md](ADR/ADR-004-unified-unreal-client-states.md) — one Unreal application and isolated client states.
- [ADR-005-launcher-session-handoff-and-local-first-hosting.md](ADR/ADR-005-launcher-session-handoff-and-local-first-hosting.md) — launcher, session handoff, local-first hosting, Go backend, Git LFS.
- [ADR-006-unreal-project-scaffold.md](ADR/ADR-006-unreal-project-scaffold.md) — Unreal project in `Game/`, targets, modules and the layer check, ASC placement, Iris and the per-player fog gate, text JSON tuning, LFS, CQTest and build/test scripts.
- [ADR-007-match-join-contract.md](ADR/ADR-007-match-join-contract.md) — join tickets, the server's roster at start, server reports and results, the local Docker allocator.
- [ADR-008-vanguard-definitions-and-ability-composition.md](ADR/ADR-008-vanguard-definitions-and-ability-composition.md) — Vanguard definitions, abilities composed onto archetypes, the passive registry, Progression, the new modules, provisional values, open canon questions for the first four kits.
- [ADR-009-runtime-combat-primitives.md](ADR/ADR-009-runtime-combat-primitives.md) — statuses, displacement and dashes, shields with caps, projectiles and areas, shapes, basic attacks, no prediction for these categories.
- [ADR-010-play-flow.md](ADR/ADR-010-play-flow.md) — the client-state coordinator, front-end map and travel, UMG menus in C++, the launcher and its launch handshake, onboarding and available Vanguards, Custom practice, champion select, assignment v2, queue and Match Found.
- [ADR-011-battleground-runtime.md](ADR/ADR-011-battleground-runtime.md) — **Proposed** (accepted when the M7a pull request merges): the Battleground layer with VeyraFlux and VeyraWorld, structures and their combat rules, kill credit, Fluxborn and waves, tower aggro, invulnerability and the Prime Well, Team Flux, Gold and XP rewards, the respawn curve, the layout and generated map, victory, and the provisional values.
- [ADR-012-items-and-shop.md](ADR/ADR-012-items-and-shop.md) — **Proposed** (accepted when the M8 pull request merges): the Items layer and VeyraItems, item data and tier rules, Gold transactions, the inventory and purchase queue, equipment stats with Ability Haste and additive bonus Attack Speed, fountain routing, Recall, and League's answers where canon is silent.
- [ADR-013-ai-vanguards.md](ADR/ADR-013-ai-vanguards.md) — **Proposed** (accepted when the M9 pull request merges): the Autonomy layer and VeyraBots, the bot-added event, order paths by participant, the sense–decide–act brain, `Bots.json` difficulties and per-Vanguard behaviour, bot seats with difficulty, and League's answers where canon is silent.
- [ADR-014-jungle-and-flux-wells.md](ADR/ADR-014-jungle-and-flux-wells.md) — **Proposed** (accepted when the M10a pull request merges): neutral wildlife and Flux Wells as a hostility category; `AVeyraWildlife`, its camp-aggro and leash controller and the jungle subsystem; traits as statuses; wildlife and Well rewards; `AVeyraFluxWell`, presence drain, contest and securing; the Flux Well source; the jungle bot seat; League's answers where canon is silent.
- [ADR-015-flux-spells.md](ADR/ADR-015-flux-spells.md) — **Proposed** (accepted when the M10b pull request merges): `Spell1` and `Spell2` in the ability loadout with fixed cooldowns; the roster as archetype entries; `DamageOverTime`, `Weaken` and the `Periodic` delivery; slots unlocked by permanent Team Flux through Match; preselection, the saved loadout per Vanguard and assignment v4; fountain swaps for Gold; bots' spells; League's answers where canon is silent.
- [ADR-016-vision.md](ADR/ADR-016-vision.md) — **Proposed** (accepted when the M11 pull requests merge): VeyraVision in the Battleground layer; the Core visibility contract; team vision, Dense Fog and stealth rules; the Iris fog gate for units and PlayerState data; Dense Fog layout and runtime volumes; presence pings and outlines; the vision-tool slot, wards, Sweeper and Quick Sight; League's answers where canon is silent.

## Keeping the maps current

The [section locator index](Index/README.md) links to generated heading/line maps for the biggest bibles. Rebuild after editing their headings **or any earlier lines**, since line-number references shift. The [CI documentation check](../.github/workflows/docs-context-index.yml) fails when a current bible is missing from this route table, a referenced active bible disappears, or a committed section map is stale. A new domain needs an explicit owner and route; adding a cross-reference never silently changes who owns a rule.
