# ADR-006: Unreal project scaffold

**Status:** Proposed. It becomes Accepted when the author merges the M1 pull request that adds it.
**Date:** 2026-09-25
**Author decisions already given (2026-09-25):**
- the project lives in `Game/`;
- tuning format delegated to the recommendation (text JSON, §6);
- fog of war is enforced at the data boundary (§5);
- implementation runs in Claude Code on the author's Windows machine.
**Related:** [Architecture Constitution](../../ARCHITECTURE.md) §1–§12, [Project Structure](../../PROJECT_STRUCTURE.md), [ADR-001](ADR-001-unreal-version-policy.md), [ADR-002](ADR-002-gameplay-ability-system.md), [ADR-003](ADR-003-owned-field-entities.md), [ADR-004](ADR-004-unified-unreal-client-states.md), [ADR-005](ADR-005-launcher-session-handoff-and-local-first-hosting.md).

## Context

ADR-005 makes "scaffold the 5.8.3 project and Windows client/Linux server targets" the first build step. Several decisions were deliberately left to scaffolding time and cannot be avoided once the first file is created:

- where the Unreal project lives in this repository and what it is called (Architecture §12: "exact final module names/count");
- which modules exist first and how their dependency direction is enforced (Project Structure §6: "the minimum clean dependency graph required for the first vertical slice");
- where the Ability System Component lives and how Attribute Sets are split (ADR-002: "chosen during scaffolding based on respawn/persistence requirements");
- how gameplay tuning is authored, given that coding agents cannot edit binary assets (ADR-005: "a scaffolding decision still to be made");
- the Git LFS file patterns and locking convention (Architecture §12);
- the replication system, movement component and test framework the first networked build uses.

The design canon adds hard constraints that shape these choices:

- **Persistence through death.** Cooldowns keep ticking through death and respawn, permanent match progression persists, and a dead source can still receive kill or assist credit (Combat §18, §44; Economy §14–15).
- **Same Vanguard on reconnect.** A disconnected player's Vanguard stays in the world on movement-only autopilot, and reconnecting restores control of that same Vanguard (Match Flow §4).
- **Pause freezes everything.** An approved pause stops all gameplay time, including ability cooldowns, respawn, buyback and spawn timers and the match clock, while networking, chat, reconnect and resume voting keep working (Match Flow §10.2).
- **Multiplicative stacking.** Percentage modifiers stack multiplicatively (Combat §41). This differs from GAS's default additive aggregation.
- **Presence only, not position.** Vision requires separate explicit channels for vision, Dense Fog, presence pings, outlines and stealth reveal. Wards inside fog report presence, "not exact enemy coordinates" (Vision §2, §4, §9).
- **Controls.** Mouse-order controls throughout: point-and-click movement, attack-move, and Normal/Quick/Quick-with-Indicator casting (Settings §1).
- **Server-produced recordings.** Every match is recorded by the server as authoritative gameplay data, not video. The recording format waits for its own design pass (Replay §9).

UE 5.8 facts relied on here (Epic 5.8 documentation and release notes, 2026-09):

- **Iris** is described as "production-ready for licensees in UE 5.8" in the release notes. The plugin is still flagged Beta, and it is off by default. Iris *group filters* are Epic's stated mechanism for team-based visibility, and Iris cannot be combined with Replication Graph.
- **Mover** is still Experimental in 5.8. CharacterMovementComponent is the production path.
- **Mass** replication is Experimental and is not recommended for replicated crowds in 5.8.
- **CQTest** is Epic's recommended framework for engine-level automation tests. **Gauntlet** (Beta) runs a server with clients for end-to-end tests.
- **Data Validation** is enabled by default and can run headless (`-run=DataValidation`).

## Decision

### 1. Location and name

The Unreal project is **`Game/Veyra.uproject`**, a subdirectory beside `Backend/`, `Docs/` and `ConceptArt/`, not at the repository root.

- Unreal, Go, documentation and art stay separable for CI path filters and reviews.
- The Windows path stays short (`C:\Dev\MOBAVeyra\Game\`).
- Paths in Project Structure (`Source/`, `Content/Veyra/`) are read as relative to `Game/`.
- The `.uproject` is associated with the locally registered 5.8.3 source engine. The association identifier is machine-specific; build scripts resolve the engine from it rather than hard-coding a path.

### 2. Targets and build settings

- Four targets: `Veyra` (Game), `VeyraEditor`, `VeyraClient` and `VeyraServer`.
- Supported platforms: Win64 for Editor and Client; Linux and Win64 for Server. The Win64 server is a local debugging convenience only.
- Build settings: `DefaultBuildSettings = BuildSettingsVersion.V7` and `IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_8`. Both were verified as `Latest` in the 5.8.3-release engine source on 2026-09-25.
- Nothing may force a *unique build environment* on the Editor target. On a source engine that silently rebuilds the whole engine (measured on this machine: several hours).
- **Amendment (2026-09-26, M3): one known engine rebuild.**
  - `Game/Scripts/Build.ps1` builds with `-NoEngineChanges`, which stops any build that would change an existing engine file.
  - Whenever UnrealBuildTool regenerates the project's makefile (a plugin enabled, or a source file added or removed), UnrealHeaderTool can rewrite the engine's generated `NetCore.init.gen.cpp`. Only the package checksum changes, but NetCore then looks out of date.
  - `Build.ps1` recognises exactly that case: every engine file the VeyraEditor build would change is NetCore's object, library or DLL, the version file, or a module manifest. It then builds again with the guard lifted and says so, which takes about 30 seconds. Any other engine change still stops the build.
  - Why UnrealHeaderTool does this is not yet understood. The author asked for it to be investigated after M3.

### 3. Modules

Modules are created **only when they receive real content**, as Project Structure §6 requires. Each milestone below adds the modules it fills.

| Module | Type | Arrives | Owns |
|---|---|---|---|
| `Veyra` | Runtime (primary) | M1 | Composition root only: module startup and default-class wiring. Nothing depends on it. |
| `VeyraCore` | Runtime | M1 | Log categories, native Gameplay Tag vocabulary, stable ID types, the tuning-data framework (§6) and domain-neutral contracts. |
| `VeyraDeveloper` | DeveloperTool | M1 | Automation tests, debug commands and test harnesses. It is a leaf that production modules never depend on. |
| `VeyraCombat` | Runtime | M2 | Attributes, the damage/healing/shield pipeline, statuses and movement primitives. It links the engine's GameplayAbilities module because it owns the Attribute Sets and the damage execution. |
| `VeyraAbilities` | Runtime | M3 | GAS integration: ASC subclass, base ability, costs, cooldowns, targeting, projectiles. |
| `VeyraMatch` | Runtime | M2 (PlayerState), M3 (the rest) | GameMode, GameState and PlayerState, teams, phases, spawn/respawn orchestration. |

- **Amendment (2026-09-25, M2):**
  - `VeyraMatch` arrives in M2 holding only `AVeyraPlayerState`, so the ASC lives on the PlayerState from the start (§4). The GameMode and GameState follow in M3.
  - `VeyraAbilities` moves to M3, with the first ability. M2 gives it no real content: a stock ASC suffices, and the one Veyra rule the ASC needs (§4's modifier policy) installs as an application query.
  - The layer map is Foundation (`VeyraCore`) → Rules (`VeyraCombat`) → Orchestration (`VeyraMatch`) → Composition (`Veyra`, sealed) → Developer (`VeyraDeveloper`, sealed). M3 inserts a layer for `VeyraAbilities` between Rules and Orchestration.
- **Amendment (2026-09-26, M3):**
  - `VeyraAbilities` arrives in its own Abilities layer. It needs no ASC subclass. A stock ASC is enough, because Veyra's base ability class supplies costs, cooldowns and validation through GAS's virtual hooks.
  - It holds:
    - the base ability class and the first archetype (targeted damage);
    - the cooldown ledger (§4) and the loadout component, which maps each ability slot to a content ID;
    - `VeyraAbilities::TryCast`, the one entry point for a cast;
    - the Abilities tuning domain.
  - Projectiles and target shapes arrive with their first abilities.
  - `VeyraMatch` gains:
    - the GameMode, GameState and PlayerController;
    - the Vanguard character and the server-only controller that moves it (§7);
    - team starts;
    - the runtime input and camera settings.
- Later modules (Economy, Items, Flux, World, Vision, Vanguards, UI, a trusted-services client) follow Project Structure as their first feature lands.
- **The layer graph is enforced by a check, not just by convention.** A repository script reads every `*.Build.cs`, compares the dependencies against a declared layer map, and fails on an upward, sideways or circular edge. It runs in GitHub Actions without Unreal, alongside the existing documentation check.

### 4. Gameplay Ability System placement

- **Vanguards.**
  - The Ability System Component and Attribute Sets live on **`AVeyraPlayerState`**, with replication mode **Mixed**. The pawn is the avatar.
  - Cooldowns, permanent effects and attribution therefore survive death, respawn and reconnect without special cases.
  - AI-controlled Vanguards in Co-op and custom matches get a PlayerState too, so bots and players share one path.
- **Other GAS-using units** (Fluxborn, wildlife, structures, owned combat entities) carry their own ASC with replication mode **Minimal**, and only if they receive effects.
- **Projectiles never carry an ASC.** They carry a payload snapshotted at Commit (Combat §45, §50).
- **Attribute Sets are split by concern.** The draft split is vitals, resource, offence, defence and mobility, plus meta attributes for the damage and healing pipeline.
  - The resource family (standard, Focus, Charge, none) is data, not a class per Vanguard.
  - The exact split, and how multiplicative percentage stacking is implemented (a GAS modifier operation or a custom aggregator), are finalised in M2 with tests. That outcome is recorded here as an amendment.
- **Amendment (2026-09-25, M2): the split and the stacking.**
  - **Sets, all in `VeyraCombat`:**
    - `UVeyraVitalsSet`: Health, Max Health, and the meta attributes `IncomingPhysicalDamage`, `IncomingMagicDamage` and `IncomingTrueDamage`.
    - `UVeyraOffenceSet`: damage amplification, and flat and percentage penetration against each resistance.
    - `UVeyraDefenceSet`: Armor, Magic Resist, flat and percentage reduction of each, and damage reduction.
    - The resource and mobility sets, and Physical and Magic Power, arrive with their first consumer.
  - **Stacking (Combat §41) needs no custom aggregator.**
    - Flat changes use `AddBase`. Every percentage is a `MultiplyCompound` factor (1 + x or 1 − x), which the 5.8 aggregator multiplies.
    - Each attribute has one rule: no modifiers, flat only, percentage only, or both.
    - An application query on every combatant rejects any other operation, instant or periodic modifiers on stats, and non-Veyra executions. A test checks every Veyra effect definition against the same rules.
  - **Resistance reduction** is its own pair of attributes, applied in Combat §3's order rather than as a modifier on Armor or Magic Resist, so percentage bonuses never scale it.
  - **Shields and Temporary Health** are not attributes. They are a Combat-owned replicated ledger kept in step with their effects, because canon orders them by category and age (Combat §7).
  - **Health** is written only by the vitals set. The damage execution computes §25 steps 2–6 through Combat's resolver and outputs the meta attributes; the vitals set passes them through shields and Temporary Health before Health.
- **Gold, XP, levels, skill points, inventory and Team Flux are not GAS attributes.** They stay with their own owners (ADR-002).
- **Amendment (2026-09-26, M3): cooldowns, costs, death and casting.**
  - **Cooldowns are a Veyra ledger, not Gameplay Effects.**
    - `UVeyraCooldownComponent` (`VeyraAbilities`) sits on the PlayerState. For each ability content ID it records when the ability is ready (in world time) and the base duration the cooldown started with.
    - It replicates to the owner and to replays.
    - It plugs into GAS through `CheckCooldown`, `ApplyCooldown` and `GetCooldownTimeRemainingAndDuration`, so `CommitAbility` stays the single commit point.
    - **Why:** these rules are simpler on a ledger than on active effects:
      - rescaling running cooldowns in proportion when Ability Haste changes, with separate haste pools and refunds (Combat §21);
      - the 20% cooldown on an interrupted cast (§26);
      - death cleanup (§44).
    - Living on the PlayerState, the ledger survives death, and it freezes during a pause (§8).
  - **Costs.**
    - `UVeyraResourceSet` (`VeyraCombat`) holds `Resource`, `MaxResource` and a `ResourceSpend` meta attribute.
    - `Resource` is written only by its set, like Health. Spending goes through a Veyra execution on the §41 allow-list.
    - Only the standard family (Mana, Combat §58) is data-backed so far. Focus, Charge and no-resource arrive with their first Vanguard.
  - **Death.**
    - Combat owns a replicated life state, `UVeyraLifeComponent` (Alive or Dead), on the PlayerState. There is no `Status.Dead` tag, because canon defines no such status and tags come only from closed canon lists (Project Structure §5).
    - Lethal damage finalizes the death, removes temporary effects (§44) and broadcasts an `FVeyraDeathEvent` through `UVeyraCombatEventSubsystem`.
    - `VeyraMatch` subscribes to that event. It removes the body, and after a placeholder delay from `Match.json` it revives the participant and spawns a new body at its side's start. The canon respawn curve is still open (Battleground §16).
    - The Vanguard's controller outlives each body. The engine would otherwise destroy it with the body, because it has no PlayerState of its own.
  - **Casting.**
    - The client sends a cast intent: a slot and a target actor.
    - The match checks the phase and the pause, then calls `VeyraAbilities::TryCast`.
    - `TryCast` runs the ability's validator: the ability is known, the caster is alive, it is off cooldown, the caster can pay, and the target is valid.
    - It then activates the ability through a gameplay event whose target the server fills in. The ability checks its target again, and `CommitAbility` pays the cost and starts the cooldown.
    - Refusals go back to the owning client with a reason.
    - No client target data is involved (§7), so the target-data spike (§5) concerns later ability kinds.

### 5. Networking

- **Iris is enabled from the first networked build.** The alternative is the legacy system plus Replication Graph.
  - Epic presents Iris as its replacement for relevancy and Replication Graph.
  - Its group filters map directly onto per-team visibility.
  - Starting on it avoids a migration later.
- **Fog of war is enforced at the data boundary** (author ruling, 2026-09-25).
  - The server replicates an enemy unit or enemy-owned entity to a player's client only while it is visible to that player's team.
  - Presence pings and outlines carry only the information their channel allows (Vision §2, §4), never the hidden unit's exact state.
  - The Vision system maintains one visibility group per team and is the only writer of that membership.
  - The Vision Bible gains this rule in the same pull request that adds this ADR.
  - **Amendment (2026-09-25): the gate is per player.** An enemy is replicated to a player's client only while it is visible to *that player*. Shared team vision reaches every teammate, but a Dense Fog sighting reaches only observers inside the same fog volume (Vision §2; Battleground §11). Vision is the only writer of the team visibility groups and of each observer's fog-volume membership. How Iris expresses the per-player part is decided in M3.
- **Push-model replication** is enabled.
- **Required spikes in M3 before Iris is locked.** Each outcome is recorded here as an amendment.
  1. Record and play back a server-side replay while gameplay runs on Iris.
     - The 5.8.3 `BaseEngine.ini` sets `+IrisNetDriverConfigs=(NetDriverName=DemoNetDriver, bCanUseIris=false)`. The replay driver therefore records through the legacy replication path, in parallel with Iris on the game driver.
     - The spike proves recording and playback work in that mixed setup, and measures the extra server cost of replicating twice.
  2. Confirm the known GAS target-data issue UE-365455 and its workaround.
  3. Measure bandwidth and server cost with a full lane population.
  - If a spike fails, the fallback is the legacy replication system with Replication Graph. Choosing it needs a deliberate amendment.
- **Amendment (2026-09-26, M3): how Iris expresses the per-player fog gate.** The evidence is `Veyra.Net.FogGate`, which uses three players so that one side has two.
  - **Units are hidden by default.** Every fog-gated unit uses the engine's filter-out dynamic filter (`NotRouted`), so no client receives it. Inclusion groups, which Iris applies after dynamic filters, open it up:
    - one group per side, allowed for that side's connections, holding the side's own units;
    - one group per observer, allowed only for that observer's connection, holding the enemy units that player currently sees.
  - **Vision writes both kinds of group.**
    - Shared team vision adds a sighting to every teammate's observer group.
    - A Dense Fog sighting goes only into the groups of observers inside the same fog volume.
    - A unit leaving a player's groups is destroyed on that player's client. In the test, the observer's teammate never received the sighted enemy.
  - **Data on the always-relevant PlayerState is hidden the same way.** The attribute sets, and later any other per-participant data a fogged enemy must not reveal, replicate with `COND_NetGroup`.
    - Iris sends a `COND_NetGroup` subobject to a connection only through a group that allows it, so this too is deny-by-default.
    - Each participant has one net condition group. Its teammates and current observers are members (`APlayerController::IncludeInNetConditionGroup`), and its owner receives the data through the engine's owner group.
    - An observer who loses sight keeps the value it last saw. GAS on the clients raised no warnings.
  - **Why not one exclusion group per observer.** Exclusion groups allow everything by default, so a unit Vision forgot would leak. Filter-out plus inclusion groups deny by default.
  - **What production needs, with Vision:**
    - **Register before the first send.** GAS registers attribute sets for every connection. The spike re-registered them while replicating; production registers them with `COND_NetGroup` before the first send, through an ASC subclass hook.
    - **Push memberships to Iris explicitly.** A subobject's net condition groups reach Iris only when game code calls `FReplicationSystemUtil::UpdateSubObjectGroupMemberships` after registering it.
    - **Replays.** The replay driver uses legacy replication and ignores Iris filters. Gated subobjects also join the replay group so replays record them.
    - **Subobjects need net condition groups.** Iris ignores subobjects in inclusion groups, so subobject gating always uses net condition groups.
  - Presence pings and outlines remain separate channels (Vision §2, §4).
- **Amendment (2026-09-26, M3): GAS target data under Iris.** The evidence is `Veyra.Net.TargetData`. UE-365455 does not appear in the 5.8.3 source, so the spike tested the behaviour directly.
  - **A client cannot start a GAS ability under §7's control model.**
    - GAS silently refuses to activate an ability on a machine where the avatar is a simulated proxy.
    - A Vanguard possessed by its server-side controller is a simulated proxy on its owning client.
    - So neither GAS prediction nor GAS client target data is available until a prediction ruling for some ability category changes who owns the pawn on that client.
    - Veyra's casts do not need either: intents travel through the PlayerController, and the server fills the gameplay event (§4).
  - **Target data made of reflected properties crosses Iris intact**, sent through the ASC's server RPC.
  - **Iris ignores a struct's own `NetSerialize`.**
    - Without an Iris NetSerializer, Iris sends the struct's reflected properties and warns that it is "generating descriptor for struct … that has custom serialization".
    - A field only `NetSerialize` wrote arrived as zero.
    - **Rule:** every Veyra replicated struct, target data included, is plain reflected properties, or it gets an Iris NetSerializer. The spike's `NetSerialize` struct was removed after the run, because its warning appeared in every development build.
  - **Modules that load after replication starts.**
    - GAS rebuilds its polymorphic target-data type table whenever modules finish loading. Iris warns when that happens while a replication system exists, because a client and server could then disagree on type indices.
    - The packaged server loads its map during engine start-up. `AutomationWorker` and `AutomationController` (non-Shipping only) and `PerfCounters` load after that, so every server logs the warning.
    - None of those modules registers replicated types, so the warning is harmless today, but it would hide a real case.
    - **Recommended, not done yet:** the composition root loads those modules before the map, so the warning appears only when something real happens.

### 6. Tuning data

**Source of truth.** Gameplay tuning is **text data in the repository**: versioned JSON files under `Game/Tuning/`, one file per owning domain.

- Each file validates against a schema and is loaded at startup by the owning domain through the `VeyraCore` tuning framework into typed, reflected structures.
- Validation checks required fields, ranges and cross-references, and **fails explicitly**. There are no silent defaults (Architecture §1.3).

**How the data behaves at runtime**

- The files are staged into packaged Client and Server builds.
- The server is authoritative. The client uses its copy only for presentation and prediction.
- Client and server compare a content hash on connection so a mismatched build is refused.
- Custom-match overrides are layered on the server: defaults, then mode, then session (Custom Matches §4).

**Asset references** (meshes, animations, VFX, audio) stay in Data Assets keyed by stable IDs. **No gameplay number lives in a binary asset or in code.**

**Checks without Unreal.** A schema check runs in GitHub Actions. Agents and reviewers edit and diff tuning like code.

**Why not the alternatives.** Binary Data Assets and DataTables imported from CSV both leave the authoritative numbers in binary files that agents cannot edit, and that need an editor-side reimport step.

**Amendment (2026-09-25, M2): the framework.**

- **Files.**
  - `Game/Tuning/<Domain>.json`, with its schema in `Game/Tuning/Schemas/<Domain>.schema.json`.
  - UTF-8 without a byte-order mark, with LF line endings (`.gitattributes`).
  - Each document has a root `schemaVersion`, which the owning domain checks against the version its code reads.
  - `Game/Tuning/README.md` holds the rules.
- **Schema dialect.** A strict subset of JSON Schema draft-04: every field is required, objects forbid extra fields, and every number declares a minimum.
- **In the game.**
  - `VeyraTuning::ValidateAndBind` (`VeyraCore`) parses strictly and walks the schema, the document and the reflected struct together.
  - It collects every error with a JSON pointer, and binds only a fully valid document.
  - The schema and the struct must describe exactly the same fields.
  - The engine's JSON-to-struct converter is not used for validation, because it accepts unknown fields and falls back to defaults on wrong types.
- **Loading.**
  - Each owning domain loads its file once at startup; for Combat that is `UVeyraCombatTuningSubsystem`.
  - A failed load is fatal outside the editor and an error inside it.
- **Hash.** BLAKE3 of each document's exact bytes. The connect-time comparison arrives with networking in M3.
- **Staging.** Each owning module declares its files as runtime dependencies (UFS), so they ship with every build that runs that domain. M3 verifies them in the packaged Linux server.
- **CI.** `scripts/check_tuning.py` applies the same rules with a pinned `jsonschema`. A shared corpus runs in both validators, so they cannot drift apart.
- **Content IDs.** Lowercase ASCII snake_case (`^[a-z][a-z0-9]*(_[a-z0-9]+)*$`), matching the Vanguard and backend identifiers. The `FVeyraContentId` type arrives with the first tuning that references content.
- **First schema.** Combat's resistance mitigation constant (Combat §3), which the author ruled to be tuning data.

**Amendment (2026-09-26, M3): content in tuning, the connect-time hash, and staging verified.**

- **The dialect grows for content.** Both validators implement these additions, and the shared corpus covers them.
  - **String enums** bind to `enum class` UENUMs by their short C++ names. The schema's `enum` must list exactly the UENUM's values. `EVeyraDamageType` is now a UENUM; its true-damage value is spelled `TrueDamage` because UnrealHeaderTool forbids `True`.
  - **Content IDs.** `FVeyraContentId` (`VeyraCore`) wraps a name checked against the canonical pattern. A string schema whose `pattern` is exactly that pattern binds to it.
  - **Content-keyed maps.** An object with a single `patternProperties` entry (the canonical pattern) and `additionalProperties: false` binds to `TMap<FVeyraContentId, FStruct>`. `Abilities.json` uses one: `targetedDamage` is keyed by ability ID.
  - **Cross-file references**, which a schema cannot express, are checked in two places:
    - the loading domain checks them in the game (Match checks that its developer loadout's `abilityQ` is defined in `Abilities.json`);
    - `scripts/check_tuning.py` keeps a small reference table.
- **The hash, compared on connect.**
  - Each domain logs its BLAKE3 hash when it loads.
  - The composite hash is BLAKE3 over the sorted `domain=hash` lines of every loaded domain, so no domain can be left out.
  - A client sends it as a login option (`?VeyraTuning=`). `AVeyraGameMode::PreLogin` refuses a missing or different hash.
  - An editor-build client and the packaged Linux server matched in the container smoke test.
- **Staging verified.** `Game/Scripts/Package.ps1` lists the packaged server's pak and fails unless every `Game/Tuning` file is in it.

### 7. Movement

- Vanguards use **CharacterMovementComponent**, driven by server-validated move orders with pathfinding. Mover remains Experimental in 5.8 and is not approved under ADR-001.
- The client prediction policy for movement and each ability category is decided in M3 and later milestones, not here (Architecture §12).
- Movement for Fluxborn and other high-count units is decided when lanes are built. Mass replication is not a candidate in 5.8.
- **Amendment (2026-09-26, M3): server-only movement and casting, with no client prediction** (author ruling, 2026-09-25).
  - **Intents.** A client sends intents: a move destination, or a cast slot and target. The server validates each one and acts on it. Every client, the owner included, shows the smoothed replicated result.
  - **Who moves the Vanguard.** A server-only `AVeyraVanguardController` (an AI controller) possesses each Vanguard and moves it with pathfinding. It stays the pawn's owner, so no client can send CharacterMovement moves for it.
  - **The PlayerController possesses nothing.** It sends the player's intents and views the Vanguard. While pawn-less it is hardened:
    - no default or spectator pawn;
    - no restart and no spectating;
    - on the server, its view point is the Vanguard, because Iris uses the view point.
  - **After each possession**, the pawn carries the human's PlayerState, so the ASC's avatar is the pawn.
  - **Order checks** run in this order:
    - a rate limit from `Match.json`, whose held-click repeat stays below it;
    - the match phase and the pause;
    - for moves, a destination that projects onto the navmesh within a tuned distance.
  - **Preparation refuses every order** until base geometry exists. This is a recorded deviation from Match Flow §1.3, which allows movement inside the fountain.
  - **Later ability categories** decide their prediction one by one (Architecture §12).

### 8. Pause and the gameplay clock

- Match Flow §10.2 requires every gameplay timer to freeze while the network, chat and votes keep running.
- M3 prototypes Unreal's world pause, which stops world time and the timers GAS durations use, against an automated test covering every timer category the bible lists.
- If world pause cannot meet the rule, a Veyra-owned gameplay clock is introduced instead. That choice is recorded as an amendment.
- **Amendment (2026-09-26, M3): world pause meets the rule, so there is no Veyra gameplay clock.**
  - **The test.** `Veyra.Net.MatchPause` pauses a live match on a dedicated server with two clients, under Iris.
  - **What stops:** Gameplay Effect durations (a shield), world timers, world time and the match clock, and movement. Every timer category in Match Flow §10.2 runs on one of these:
    - cooldowns and buffs on effect durations or world time;
    - respawn, buyback, spawn and penalty clocks on world timers;
    - the match clock on world time;
    - movement, regeneration and combat on actor ticks.
  - **What keeps running:** replication (an actor spawned and changed during the pause reaches both clients), real time, and the server's refusal of orders while paused.
  - **Resuming:** every clock continues from its saved value.
  - **The rule that follows.** Every gameplay timer uses world time or the world's timer manager, never real time. The only exception is the real-time intermission countdown.
  - **How clients learn of it.** `AVeyraGameState` replicates the pause and holds the client's match clock still.
  - **In-process tests cannot show a client's own world pausing.** The engine carries that through the map's WorldSettings, and in-process play sessions replicate no map-placed actor, under Iris or the legacy system.
  - **Update (2026-09-26): covered across processes.**
    - `Game/Scripts/Smoke.ps1` has a client request a pause and checks that its own world stops, then starts again on resume.
    - It passed against the containerised Linux server and against a local editor-build server.
    - `Veyra.Net.MatchPause` now also checks that the cooldown ledger freezes, on the server and in the owner's view.

### 9. Source control

- `.gitattributes` routes Unreal binary assets under `Game/` through Git LFS:
  - `*.uasset` and `*.umap` as **lockable**;
  - common source binaries (for example `.fbx`, `.wav`, `.png`, `.tga`, `.exr`, `.psd`) as ordinary LFS files.
- Rules are scoped to `Game/`. Existing `ConceptArt/` and `Docs/` files stay as they are, with no history rewrite.
- **Locking convention:** lock a binary asset before editing it and release the lock when the change merges.
- The `.gitignore` comment that still calls the LFS policy open is corrected.
- GitHub Free includes 10 GiB of LFS storage and 10 GiB of bandwidth per month. Pushes stop when the quota is exceeded without a payment method. That budget is reviewed before art production begins; ADR-005 already names Perforce as the fallback if LFS stops scaling.

### 10. Tests and builds

- **Automation tests use CQTest** in `VeyraDeveloper`, named `Veyra.<Module>.<Feature>`.
  - They run headless from the command line: `-nullrhi -unattended`, `Automation RunTest Veyra`.
  - Network and headless-match tests use CQTest's PIE networking first and Gauntlet later (Architecture §7).
- **Builds and tests are driven by versioned PowerShell scripts** in `Game/Scripts/`, so a human, a coding agent and the future self-hosted runner run the same commands (AGENTS.md: "Prefer command-line builds/tests").
- **Code follows Epic's C++ coding standard** with a `Veyra` class prefix, include-what-you-use and zero compiler warnings in Veyra modules.
- **Amendment (2026-09-26, M3): packaging, the container and the smoke test.**
  - **Packaging.** `Game/Scripts/Package.ps1` wraps `RunUAT BuildCookRun` for a target already built by `Build.ps1`. It never passes `-build`, so the `-NoEngineChanges` guard (§2) still holds.
  - **The match server image** (ADR-005 step 2) is `Game/Docker/Server/Dockerfile`:
    - it builds from the packaged Linux server;
    - a `debian:12-slim` stage fails the build if `ldd` reports a missing library;
    - the final image is distroless `cc-debian12:nonroot`.
  - **The container** is the `match-server` service in the root `compose.yaml`. It runs only with its compose profile and publishes `127.0.0.1:7777/udp`.
  - **Direct connect is for development only.** Shipping builds refuse every login until M4's match-join contract.
  - **The smoke test.** `Game/Scripts/Smoke.ps1` starts the server and two headless clients with `-VeyraSmoke`. Each client moves its Vanguard and casts its Q ability at the other. The first client also pauses and resumes the match.
    - A client's result is the verdict line it logs. On Windows a clean engine exit always returns 0, so a client's exit code only catches crashes.
    - `-Server Editor` swaps the container for a local editor-build server when Docker is unavailable.
  - **Docker Desktop must forward UDP both ways.** Version 4.48.0 on this machine delivered packets into the container but dropped its replies, so clients timed out. Version 4.92 works.

## Milestones

Each milestone is one branch and one pull request. It is built on the Windows machine, which is required because cloud agent sessions cannot compile Unreal (ADR-005), and it is merged only by the author.

1. **M1 — Skeleton.**
   - Scope: project, targets, the `Veyra`, `VeyraCore` and `VeyraDeveloper` modules, Gameplay Tag vocabulary, `.gitattributes`, build/test scripts, the module-layer check and its CI job.
   - Done when Editor Win64, Client Win64 and Server Linux all build, the automation tests pass headless, and both repository checks pass.
2. **M2 — Tuning and GAS foundation.**
   - Scope: the tuning framework with its first schemas, `VeyraCombat`, the ASC on the PlayerState (in `VeyraMatch`), the Attribute Sets, and the canonical damage pipeline for Combat §3 and §25. `VeyraAbilities` moved to M3 (§3 amendment).
   - Done when the mitigation, penetration, shield and stacking rules pass automated tests, and Editor Win64, Client Win64 and Server Linux build with zero warnings.
3. **M3 — Match and network.**
   - Scope: `VeyraAbilities`, the rest of `VeyraMatch`, Iris, click-to-move Vanguards, a grey-box test map, the Linux server in Docker with dev-only direct connect (ADR-005 step 2), the spikes in §5 and §8, and deciding how Iris expresses the per-player fog gate.
   - Done when two clients play against the containerised server and a server-validated test ability passes an automated network test.
4. **M4 onward.**
   - Session handoff from the game side (ADR-005 step 3), which needs the backend's match-join contract first.
   - Then the first Vanguards in ADR-003 order: Cairn, Qazharr, Oriel, Bryn.

## Consequences

- Every scaffolding decision Architecture §12 leaves open is either decided above or given a named milestone and test that decides it.
- Tuning is reviewable, diffable and editable by agents from day one. The cost is a small Veyra-owned loading and validation framework instead of engine DataTable tooling.
- The ASC-on-PlayerState model makes PlayerState replication frequency part of the performance budget, and AI Vanguards must have PlayerStates.
- Committing to Iris early concentrates risk in three named spikes, rather than in a late migration.
- Nothing ships to players from M1–M3. They exist to prove boundaries, authority and the build pipeline before any content scales.
- Epic has described 5.8 as the last planned major UE5 release and signalled a Verse-based gameplay model for UE6. Keeping rules in plain, tested C++ behind narrow engine seams keeps any future migration (a separate decision under ADR-001) tractable.

## Alternatives considered

- **Project at the repository root.** This is conventional for Unreal-only repositories. It was rejected because it mixes `Content/`, `Config/` and `Source/` with the Go backend and design documents, and complicates CI path filters.
- **Scaffold all twelve modules now.** Rejected by Project Structure §6. Empty modules imply boundaries nobody has tested.
- **ASC on the pawn for Vanguards.** Death and respawn would then need custom persistence for cooldowns, permanent effects and attribution. That duplicates what the PlayerState model gives for free.
- **Legacy replication with Replication Graph.** Mature and not deprecated, and kept as the fallback. It was not chosen because it is the older path, while Iris is Epic's stated direction.
- **Mover.** Experimental in 5.8.
- **Mass for lane units.** Mass replication is experimental and not recommended for replicated crowds in 5.8.
- **DataTables imported from CSV, or Data Assets, as the tuning source of truth.** Rejected because the authoritative values would end up in binary assets that agents cannot edit or review (ADR-005).
- **Automation Spec or Low-Level Tests as the primary framework.** Both remain available. CQTest is Epic's current recommendation, and it covers world and network tests that Low-Level Tests cannot.
