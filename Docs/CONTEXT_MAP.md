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
| Launcher, authentication entry, unified Unreal application, Test Skin and state transitions | [Client & Platform Bible](Design/Veyra_Client_Platform_Bible_v0.1.md) | [Section locator](Index/sections/Veyra_Client_Platform_Bible_v0.1.md); the accepted [unified-client ADR](ADR/ADR-004-unified-unreal-client-states.md) governs architecture; UX owns visible screen behavior. |
| Home, Shop, ordinary client shell, Match Found overlays, selection and results UI, Test Skin presentation | [Pre-Game Client UX Bible](Design/Veyra_Pre_Game_Client_UX_Bible_v0.1.md) | [Section locator](Index/sections/Veyra_Pre_Game_Client_UX_Bible_v0.1.md); UX-1–92 are recorded, **paused after UX-92**. Parties, Chat, Match Flow and Client & Platform retain their rule authority. |
| Parties, friends, presence, invites, queue readiness and matchmaking | [Parties, Social & Matchmaking Bible](Design/Veyra_Parties_Social_Matchmaking_Bible_v0.1.md) | UX controls presentation, not authoritative permission. |
| Team/All/Party/DM/post-match text, chat mute, channel permission | [Chat & Communication Bible](Design/Veyra_Chat_Communication_Bible_v0.1.md) | No built-in voice chat; moderation owns reports. |
| Identity, profile surfaces, player names, account creation entry | [Profiles & Identity Bible](Design/Veyra_Profiles_Identity_Bible_v0.1.md) | Account Bible owns progression and unlocks. |
| Reports, appeals, staff review, logging and telemetry | [Moderation & Telemetry Bible](Design/Veyra_Moderation_Telemetry_Bible_v0.1.md) | Chat owns conversation permissions; results/statistics data stays with trusted owners. |
| Replay recording, saves, delayed live spectating, replay privacy | [Replay & Spectator Bible](Design/Veyra_Replay_Spectator_Bible_v0.1.md) | Client & Platform governs state transitions; Custom Matches owns invite-only custom spectator options. |
| Player controls, camera, HUD, graphics, audio, language and accessibility | [Settings & Accessibility Bible](Design/Veyra_Settings_Accessibility_Bible_v0.1.md) | [Section locator](Index/sections/Veyra_Settings_Accessibility_Bible_v0.1.md); approved settings through SET-168, not permission to invent UI or server rules. |
| Server-recorded match fields, scoreboard and detailed results/statistics | [Match Statistics Bible](Design/Veyra_Match_Statistics_Bible_v0.1.md) | Match Flow owns outcome; Combat/Economy/Flux own the facts recorded; UX presents verified results. |

## Other task routes

- **Engineering:** [Architecture](../ARCHITECTURE.md) → [Project Structure](../PROJECT_STRUCTURE.md) → relevant [ADR index](ADR/README.md) → owning design section. Do not create a giant manager or make UI, Blueprints, or the client authoritative.
- **Vanguard editing:** one character's section of the Character Bible → `Docs/Design/Vanguards/<nn>-<name>.yaml` → [Vanguard validation instructions](Design/Vanguards/README.md) → specific base [hero art](../ConceptArt/Vanguards/README.md). Do **not** interpret YAML as engine balance data.
- **Cosmetics:** [skin gallery](../ConceptArt/Vanguards/skins/README.md) and [asset index](../ConceptArt/Vanguards/skins/index.json) → the particular `ConceptArt/Vanguards/<id>/skins/<collection>/hero.webp`; use base character/art bible only for identity and silhouette.
- **Art direction:** [Art Direction](Design/Art_Direction_v0.1.md) and [canon discrepancy register](Design/Sheet_Canon_Discrepancy_Register_v0.1.md). [Ride-state question history](Design/Ride_State_Open_Questions_v0.1.md) is *resolved history*, not an open-rules source.
- **Historical comparison only:** [Design Archives](Design/Archives/README.md) and [Concept Art Archives](../ConceptArt/Archives/README.md). Do not use archived versions to override an active bible.

## Accepted ADRs

Every ADR number is unique, and every accepted record is routed here (CI checks both). Full summaries are in the [ADR index](ADR/README.md).

- [ADR-001-unreal-version-policy.md](ADR/ADR-001-unreal-version-policy.md) — Unreal Engine 5.8, version-pinned.
- [ADR-002-gameplay-ability-system.md](ADR/ADR-002-gameplay-ability-system.md) — GAS behind Veyra-owned C++ integration and combat semantics.
- [ADR-003-owned-field-entities.md](ADR/ADR-003-owned-field-entities.md) — owned combat units, markers and world volumes.
- [ADR-004-unified-unreal-client-states.md](ADR/ADR-004-unified-unreal-client-states.md) — one Unreal application and isolated client states.

## Keeping the maps current

The [section locator index](Index/README.md) links to generated heading/line maps for the biggest bibles. Rebuild after editing their headings **or any earlier lines**, since line-number references shift. The [CI documentation check](../.github/workflows/docs-context-index.yml) fails when a current bible is missing from this route table, a referenced active bible disappears, or a committed section map is stale. A new domain needs an explicit owner and route; adding a cross-reference never silently changes who owns a rule.
