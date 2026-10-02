# Veyra Project Structure

**Status:** Provisional structure; architecture direction is locked, exact module names may evolve.  
**Engine target:** Unreal Engine 5.8.3, source build (`ADR-001`); Windows client, Linux dedicated server (`ADR-005`)  
**Ability framework:** Unreal Gameplay Ability System (GAS), per `ADR-002`  
**Application architecture:** Single Unreal client with controlled states, per [`ADR-004`](Docs/ADR/ADR-004-unified-unreal-client-states.md)  
**Project location:** `Game/Veyra.uproject`. The `Source/`, `Content/` and `Config/` paths in this document are relative to `Game/` ([`ADR-006`](Docs/ADR/ADR-006-unreal-project-scaffold.md) §1).  
**Read first:** [`ARCHITECTURE.md`](ARCHITECTURE.md)

The purpose of this document is to make ownership and dependency direction obvious before the Unreal project becomes large. It is not permission to create every listed module immediately. Start with the smallest useful set and split modules when boundaries become valuable.

## 1. Intended source domains

A likely long-term shape is:

```text
Source/
├── VeyraCore/
├── VeyraSettings/
├── VeyraCombat/
├── VeyraAbilities/
├── VeyraEconomy/
├── VeyraItems/
├── VeyraFlux/
├── VeyraWorld/
├── VeyraVision/
├── VeyraMatch/
├── VeyraBots/
├── VeyraVanguards/
├── VeyraServices/
├── VeyraUI/
└── VeyraDeveloper/
```

The modules that exist so far, and their enforced layers, are in `Game/Source/ModuleLayers.json` (ADR-006 §3); the others are the intended split. The domain responsibilities below matter more than the exact spelling.

### VeyraCore

Lowest-level Veyra-owned foundation.

Appropriate responsibilities:

- shared identifiers and lightweight value types;
- central Gameplay Tags/vocabulary;
- stable interfaces and message contracts;
- common utilities that are genuinely domain-neutral;
- shared serialization/version helpers where needed.

Must not depend on higher gameplay modules.

### VeyraSettings

The player's settings ([ADR-024](Docs/ADR/ADR-024-player-settings.md)), in the Preferences layer directly above VeyraCore:
- the registry of every setting the Settings screen offers (`Game/Settings/Settings.json`, outside the tuning hash);
- `FVeyraSettingsStore` and the account document;
- `UVeyraSettingsSubsystem`, which keeps device settings in `UVeyraUserSettings` (the engine's `GameUserSettings.ini`) and account settings in a per-account cache the services sync.

Presentation only: it never decides a match. The systems that apply a setting read the store and listen to its change event; the developer defaults in `Default*.ini` stay the source of every default until a setting replaces one.

### VeyraCombat

Owns reusable combat truth.

- attributes;
- damage/healing/shield pipeline;
- mitigation/resistance calculations;
- status effects and crowd-control primitives;
- displacement primitives;
- movement modes, including ride states (Combat Bible §56: a ride state is a movement mode on an ordinary pathing agent, not a separate system);
- combat event data;
- death-trigger inputs (not full match respawn policy).

Must never special-case named Vanguards or items.

It links the engine's Gameplay Ability System because it owns the Attribute Sets, the damage execution and the modifier policy for Combat §41 stacking (ADR-006 §4). The damage math itself is plain C++ that the execution calls.

It also owns each combatant's life state and the death event that other domains react to, such as Match's respawn (ADR-006 §4, M3 amendment).

M5 added the runtime primitives the first kits use ([ADR-009](Docs/ADR/ADR-009-runtime-combat-primitives.md)): the status ledger, the movement component with its displacement and dash modes, shields with identity and caps, Combat State and assist attribution, and damage prepared at Commit.

M17 added critical strikes and combat's random source ([ADR-023](Docs/ADR/ADR-023-crit-and-the-full-item-catalog.md) §1, §4, §10):
- `Attacks/VeyraCrit` is the crit rule. `UVeyraCombatRollSubsystem` keeps one outcome bag (`Random/VeyraOutcomeBag`) per unit and channel, so chance is drawn rather than rolled. Any chance-based mechanic draws through it, never `FMath::FRand`.
- `OnDamageDealt` reports what each damage instance cost an enemy, by type.

### VeyraAbilities

Owns reusable ability execution behavior and Veyra's C++ integration layer around Unreal Gameplay Ability System (GAS).

- activation validation;
- cooldown/cost primitives;
- targeting;
- projectiles/areas where these are ability primitives;
- prediction contracts;
- reusable ability tasks/effects.

A Vanguard ability composes this system; it does not recreate it.

It arrived in M3 with:

- the base ability class, whose one validator every cast passes;
- the first archetype, targeted damage;
- the cooldown ledger, which lives on the PlayerState;
- the loadout, which maps ability slots to content IDs;
- `VeyraAbilities::TryCast`, the single entry point for casting.

M5 added ([ADR-008](Docs/ADR/ADR-008-vanguard-definitions-and-ability-composition.md) §3–§4, ADR-009):

- the archetypes a kit's data composes (skillshot, area, self-buff, empowered attack and dash);
- cast phases and Cast IDs;
- projectiles, delayed areas and shapes;
- basic attacks;
- the telegraphs presentation draws for a cast.

M20 added ([ADR-026](Docs/ADR/ADR-026-reactive-kit-primitives.md)): reactions, which let an effect depend on the statuses its target holds; lingering areas that hit at their pulses and as they end, and delayed areas that land sooner inside one; and `Statuses/`, whose `UVeyraStackConversionSubsystem` turns a status at its most stacks into the one its tuning names.

M20b added ([ADR-027](Docs/ADR/ADR-027-mobile-attacks-and-ally-casts.md)):

- basic attacks whose windup the attacker walks through, and empowerments that span several attacks;
- a self-buff's timed attack impact, its ally recipient and the zones that land on that recipient;
- delayed areas that linger where they land, their lingering area prepared in `VeyraAreaDelivery`;
- `Delivery/VeyraShieldRewardSubsystem`, which rewards a shield's holder once the shield has absorbed its share.

M22 added ([ADR-028](Docs/ADR/ADR-028-blind-grounding-and-collision.md)): blinded attacks that miss; areas that land on their caster's own lingering area and end it; end payloads that push. Combat's movement gives a displacement's collision statuses when terrain, a Vanguard or a structure stops it.

M24a added ([ADR-030](Docs/ADR/ADR-030-stealth-markers-and-marked-follow-ups.md)), for Tavi:
- Combat's Invisible and Untargetable statuses: Vision gates an Invisible unit as it gates a ward, and `VeyraTargeting::CanAcquire` and `CanHitEnemy` keep an Untargetable one from enemies' orders, casts, attacks, areas, skillshots and tethers;
- `VeyraCombat::Blink`, and cooldown refunds (`VeyraCooldowns::Reduce`, `VeyraAbilities::RefundCooldown(s)`);
- `Entities/AVeyraPlacedMarker` in Combat: ADR-003's placed marker, the first owned entity. It belongs to its owner's side and credits its owner, counts hits as a ward does, ends with a reason on `OnMarkerEnded`, and can present as its owner, which the HUD's bars and the minimap follow;
- recast windows that open on a held mark or a takedown, casts that take only a marked target, dashes through a target, skillshots that return to their caster, self-buffs that leave a marker, and the ambush archetype (`UVeyraAmbushAbility`);
- the quarry passive (`UVeyraQuarryPassive`).

M24b added ([ADR-031](Docs/ADR/ADR-031-stances-focus-shadows-and-cross-marks.md)), for Angeru:
- stances: the loadout stows a slot's own ability, keeping its grant and cooldown, when a stance puts another there (`SwapOwn`), and a follow-up belongs to the own ability whose cast opened it, waiting unseen in another stance; the stance archetype (`UVeyraStanceAbility`) toggles a set;
- the placement archetype (`UVeyraPlacementAbility`), a marker at a point whose follow-up ends with it; the blink archetype (`UVeyraBlinkAbility`), beside an enemy or to its caster's marker, with a swap; a skillshot its caster's marker throws too, the shots sharing what they strike;
- dashes that take over a dash under way (`duringDash`), while any other move waits; casts that refuse a target locked out by an earlier one (`targetMustNotHold`); cooldowns a takedown refunds (`takedownRefund`, `UVeyraTakedownRefundSubsystem`), the participants found once by `VeyraKillCredit::TakedownParticipants`;
- in Combat, `AVeyraPlacedMarker::FindStanding` and `Relocate`, `VeyraCombat::BlinkBeside` and `NearestGround`;
- the disciplines passive (`UVeyraDisciplinesPassive`).

M25a added ([ADR-032](Docs/ADR/ADR-032-movement-punishment-shield-holds-and-walls.md)), for Varkesh:
- Combat's `OnUnitMoved`, announced by the movement component as a unit's own dash or blink ends, and the stress temper passive (`UVeyraStressTemperPassive`) that strikes a coated mover where it lands;
- self-buffs whose shield holds statuses and bursts as it ends (`Delivery/VeyraShieldHoldSubsystem`, watching the shield's effect);
- walls: runtime terrain is the battleground's (`Terrain/UVeyraTerrainSubsystem` and `AVeyraTerrainWall` in World), reached through Combat's `IVeyraRuntimeTerrain` contract and registry; a wall blocks units, forced moves and line projectiles, cuts the navigation mesh and moves out the units it would trap; a placed marker holds one as its ownership and lifetime link, a skillshot leaves one where its flight ends (`endWall`), and the grey-box draws the terrain as a block;
- follow-ups that arm after their cast and end with their opening ability's marker, both in `Loadout/VeyraFollowUpSubsystem` (the placement archetype's own watch moved there), and areas at the caster's marker (`CastersMarker`).

M25b added ([ADR-033](Docs/ADR/ADR-033-charge-attack-spent-statuses-movement-fields-and-grids.md)), for Relay:
- Charge, a kept resource (`VeyraCombat::KeepResource`: it starts empty, and initialization, revival and the fountain leave it be), and the charger passive (`UVeyraChargerPassive`) fed by nearby Fluxborn deaths;
- casts that cost a share of the current resource and need a least of it, priced by `UVeyraGameplayAbility::CostFor` with the ResourceCostReduction status kind; statuses spent by attacks (`attackCharges`), which the basic attack component reports to Combat;
- Combat's movement fields (`Movement/UVeyraMovementFieldSubsystem`), which bend enemy dashes and displacements toward their centre, held by lingering areas that name one;
- auras on allied Fluxborn, the AttackShortensCooldown status kind (`VeyraAbilities::ShortenSoonestCooldown`), and self-buffs that drain their caster's resource and end as it runs out.

M26a added ([ADR-034](Docs/ADR/ADR-034-companions-for-marek-and-nix.md)), ADR-003's combat entity:
- in Combat, owned units (`Entities/IVeyraOwnedUnit`, which the placed marker implements) and `VeyraCombat::ResponsibleFor`: an owned unit's hostile actions, contributions and kills are its owner's (the death event's `Killer`, with the striking unit as `LethalUnit`, and hostile damage's `Responsible`), while dealt damage keeps the unit that hit, so item effects never fire from it; `VeyraCombat::Withdraw`, a death at nobody's hand with nothing announced; and the MaxHealth status kind;
- the Companion unit kind, ranked with Fluxborn by Fluxborn, drawn as a unit on the minimap and seen through Vision's `sight.companion`;
- in Abilities, `Companions/`: `AVeyraCompanion` (a character with its own Health and basic attack, on its owner's side, inheriting only `ownerMagicPowerShare`), its server controller (follow its owner and fight what the owner fought lately, or hold a point and fight what comes near), the pure `VeyraCompanionRules`, and `UVeyraCompanionSubsystem`, which forms a companion beside its owner, banishes it with its owner or when it is killed, reforms it, and keeps it grown to its owner's Level; Abilities.json's `companions` map.

M26b added ([ADR-034](Docs/ADR/ADR-034-companions-for-marek-and-nix.md)), for Marek:
- the command archetype (`Abilities/UVeyraCommandAbility`: a companion leaps to hold a point, its landing its own hit; a recall follow-up), blinks to the caster's own companion with departure zones for each, reaction bursts, self-buffs' companion statuses, chains and an end with the companion, and casts' `needsCompanion` and `cooldownWhile`;
- the Accord passive (`UVeyraAccordPassive`), which summons Marek's companion, marks what it bites and strikes an enemy both of them hurt.

M27a added ([ADR-035](Docs/ADR/ADR-035-sea-states-crashing-rides-and-summoned-companions.md)), for Neris:
- casts' `cooldownOf`, which holds a cooldown under another ability's ID (read through `UVeyraAbilityLoadoutComponent::CooldownIdOf`), and `refusedWhile`, which refuses a cast as `HeldBack` while its caster holds a status;
- rides' crash zones, which erupt where the rider is on every end but death, and the dismount archetype (`Abilities/UVeyraDismountAbility`), a mounted action that ends its ride;
- zones' ally effects: the caster's heal and statuses for the allied Vanguards in a zone, each once (`VeyraAreaDelivery::Resolve`).

M27b added ([ADR-035](Docs/ADR/ADR-035-sea-states-crashing-rides-and-summoned-companions.md)), for Neris:
- summoned companions: the command orders Summon and Redirect, `UVeyraCompanionSubsystem::SummonFor` and `Redirect` (a companion for a while, bound to an ally it escorts or an enemy it hunts, gone for good when its time runs out or it is killed), the Escort and Hunt modes, and companions' escort pulse and attack statuses;
- rides' contact (the rider's body strikes each enemy and helps each ally it meets once a ride) and trail (an area ability laid along its path), with `VeyraAreaDelivery::PrepareAllyEffects`, `Reaches` and `HelpAlly` shared with zones;
- stances that may hold R, never the slot they sit in;
- Neris, The Tidebound, and her Waterling: Calm and Storm variants of Breaking Wave, Little Current and Tidebreaker that share cooldowns, and Change the Weather.

M28a added ([ADR-036](Docs/ADR/ADR-036-runtime-dense-fog-sounded-waymarks-and-the-mist-trail.md)), for Sylra:
- in Combat, the visibility contract's `AddDenseFog` (a circle or a corridor, for a while) and `FogVolumeAt`, and the Sounded status kind, read through `VeyraCombat::HasStatusKindFromSide`;
- in Vision, Dense Fog that abilities lay: each cast's `Fog/AVeyraDenseFogBank`, replicated to every player and joined to the map's fog into volumes while it lasts, and presence pings for a Sounded enemy in fog to the side that sounded it;
- in Abilities, areas' `fog` and lingering areas' `shieldTopUp` (a merging shield built on the caster and allies who stay, held back a while after damage), and `VeyraAreaDelivery::LayAt`, which the ride trail and the Mist Trail share;
- in Vanguards, the Mist Trail passive (`UVeyraMistTrailPassive`).

M28b added Sylra, The Mistwarden: Harbor Bell, Lay the Mist, Waymark, Through the White and Follow the Bell, all data.

M29a added ([ADR-037](Docs/ADR/ADR-037-deployables-postures-cover-and-designation.md)), for Eudora:
- in Combat, the Cover status kind and the cover rule in `VeyraCombat::DealPreparedDamage` (an ally's frontal cover takes a share of a projectile's damage out of its capacity, passing some to its holder; damage carries `ProjectileFrom`, where its projectile was launched, which cover judges the shot from), and the Designated status kind, read through `VeyraCombat::HasStatusKindFrom`;
- in Abilities, the command orders Deploy, ChangePosture and Unanchor; the Anchored mode (`AVeyraCompanion::Anchor`), which never walks and stands through its owner's death; companions' postures, moving statuses and moving aura (`UVeyraCompanionSubsystem::Deploy` and `Move`); and companions' preference for their owner's Designated enemy;
- in Vanguards, the All Hands passive (`UVeyraAllHandsPassive`).

M29b added Eudora Blackbridge, The Fieldwright, and Picket: Drive Rivet, Set the Picket, Raise the Bulwark, MOVE THE LINE! and All Hands, all data.

M30 added ([ADR-039](Docs/ADR/ADR-039-weekly-rotation-and-co-op-vs-ai.md)), Co-op vs AI and the weekly free rotation:
- in the backend, the catalog's seeded weekly rotation; the `coop` matchmaking kind, which fills one side with humans; a co-op select that seats its enemy AI team; and Standard matches that carry it;
- in Bots, role preferences per Vanguard, by which a team deals its seats' places (`VeyraBotRoles::Deal`);
- in Match, the host's acceptance of a co-op match's enemy team;
- in the client, co-op cards and queues, the Play page's four categories (Ranked, Casual, AI, Customs) from each mode's `category`, and `Smoke.ps1 -Flow Coop`;
- in the scripts, `Set-VeyraBackendConfig`, which sizes a script's queue to its clients in `Backend/config/scripted.json`.

Abilities are server-only, with no client prediction (ADR-006 §4 and §7, M3 amendments; ADR-009 §6).

### VeyraEconomy

Canonical owner for gold and economy rules.

- gold balances;
- grants/spending;
- purchase affordability;
- kill/assist/CS/objective reward calculations;
- economy transactions and audit/debug events.

UI and items request transactions; they do not mutate gold directly.

**Progression** (XP balances, levels, level-up stat increments and skill points) lives in this module for now as a **separate owner** with its own state, per the Economy & Progression Bible. It shares the module, not code paths: Gold and XP are never mixed in one class.

Progression arrived first, in M5 (ADR-008 §6). It sits in its own Economy layer, above Combat, whose verbs apply level-up growth, and below Abilities, which reads ranks.

M24b added rank shapes ([ADR-031](Docs/ADR/ADR-031-stances-focus-shadows-and-cross-marks.md) §2): `Progression.json` names documented exceptions to the standard ranks, each spending the standard total of skill points, and the progression component holds and replicates its unit's shape, an innate R starting at rank 1.

Gold and the rewards arrived in M7 ([ADR-011](Docs/ADR/ADR-011-battleground-runtime.md) §11):

- `Gold/`: `UVeyraGoldComponent` on the PlayerState, a fractional balance replicated to its owner, changed only by explained grants.
- `Rewards/`: `UVeyraRewardSubsystem` decides who qualifies for each death's Gold and XP and pays through the Gold and progression components; the arithmetic is the pure `VeyraRewards` functions, and the values are `Game/Tuning/Economy.json`'s. It follows Combat's deaths for Vanguard kills; World reports Fluxborn deaths, with their team's active Flux, and fallen structures, so Economy never reads Flux or World.
- `Buyback/`: `UVeyraBuybackComponent` on the PlayerState counts a participant's buybacks and holds its cooldown; the pure `VeyraBuyback` rules price and refuse the next one (Economy & Progression Bible §15; ADR-020 §3). Economy takes the Gold; Match respawns the Vanguard.

### VeyraItems

- inventory ownership;
- item definitions;
- recipes and combination rules;
- Tier 1/2/3/4 rules, and the one-Mythical lock;
- item actives;
- Tier 3 and Tier 4 Attunements, and what they keep on the slot (Current, Reserve);
- Quest Items: quests, progress and evolution (`Quests/`);
- consumable state;
- shop-facing item queries.

Depends on combat/abilities/economy through approved contracts. It does not own the underlying damage or gold formulas.

VeyraItems arrived in M8 ([ADR-012](Docs/ADR/ADR-012-items-and-shop.md) §2) in its own **Items** layer, above Abilities, whose archetypes run item Actives, and below Battleground. Its catalog is `Game/Tuning/Items.json`; `VeyraItems::Validate` holds the tier rules the schema cannot (Item Bible §2, §11). It spends and refunds Gold through Economy and applies equipment through `VeyraCombat::SetEquipmentStats`; Match routes the fountain and the player's shop requests to it. Since M19 ([ADR-025](Docs/ADR/ADR-025-mythicals-quest-items-and-spell-shields.md)) it also listens to Combat's deaths, for quests and Current, and to the damage its holders take; Spell Shields are Combat's.

Where the Attunements live ([ADR-023](Docs/ADR/ADR-023-crit-and-the-full-item-catalog.md) §3–§4):
- Static ones fold into `VeyraEquipment::StatsFor`.
- Stacking buffs live in the shop subsystem.
- The ones a hit or nearness sets off live in `Attunements/UVeyraAttunementSubsystem`, on Combat's `OnDamageDealt`: Reprisal Guard, Drag, Convergence, Fracture, Endless Cleave and Tempered by Conflict.

### VeyraFlux

- authoritative shared team Flux;
- Flux gain/loss rules if loss ever exists;
- threshold state;
- Flux Spell definitions/loadouts/unlocks;
- Fluxborn-strength progression inputs;
- notifications when thresholds change.

Flux Spell unlocks are validated against **permanent Team Flux only**; temporary Flux must not contribute to spell-slot unlock state. The threshold values are data owned by the Battleground Bible (§14); do not copy them here or into code.

**How Team Flux is used.** The design rules live in the Battleground Bible; this module holds the runtime state.

- **Sources:** destroyed lane Spires and base-defense towers grant *permanent* Flux; secured Flux Wells and destroyed inhibitors grant *temporary* Flux, each grant expiring on its own timer. Flux is never spent: Flux Spell casts do not consume it.
- **State:** per team, the permanent total plus a list of active temporary grants with their expiry times. *Active* Flux is permanent plus unexpired temporary.
- **Readers:** Fluxborn strength (active Flux, all lanes); Flux Spell slot unlocks (permanent Flux only); Economy's Fluxborn farm-reward bonus (active Flux at the Fluxborn's death); HUD and statistics presentation.
- **Dependency direction:** world objectives report destruction/capture through Core contracts or match orchestration, and the Flux system grants the data-defined reward; it does not reach into world actors. Economy sits below Flux, so it never queries Team Flux: the Fluxborn death event carries the value the farm-reward rule needs. Fluxborn scaling reacts to Flux-changed notifications rather than polling.

A Flux Spell cast does not consume shared Flux under the current game design. Swapping Flux Spells at the shop costs gold and should use the economy transaction API rather than mutating gold in the Flux module.

VeyraFlux arrived in M7 ([ADR-011](Docs/ADR/ADR-011-battleground-runtime.md) §2) in the **Battleground** layer, a peer of VeyraWorld: the two meet only through Match, which passes World's structure destructions to Flux and Flux's changes back to World. Its tuning is `Game/Tuning/Flux.json`.

### VeyraWorld

- Flux Wells and other world objectives;
- Spires and Prime Well world actors;
- jungle camps and wildlife systems;
- lane/Fluxway world behavior;
- objective capture state and world interactions;
- runtime navigation changes from ability-created terrain (Battleground Bible §2).

World actors report outcomes to the authoritative owning systems rather than reaching directly into UI or champion code.

VeyraWorld arrived in M7 ([ADR-011](Docs/ADR/ADR-011-battleground-runtime.md) §2, §12) in the **Battleground** layer, above Abilities, whose attacks and projectiles its units use. `Game/Tuning/World.json` holds the battleground's layout (Team A's half; Team B's is its mirror across the river's diagonal) and its structures; `VeyraLayout` turns the layout into lanes, waypoints, structure placements and jungle terrain (ADR-026 §5) for the map commandlet and the server alike.

- `Structures/`: `AVeyraStructure`, a pawn with its own Ability System Component, and the tower attack.
- `Fluxborn/`: `AVeyraFluxborn` and its server-only `AVeyraFluxbornController`, which follows its lane's waypoints and fights by `VeyraFluxbornRules` (ADR-011 §7).
- `Wildlife/` (M10a, [ADR-014](Docs/ADR/ADR-014-jungle-and-flux-wells.md) §2): `AVeyraWildlife`, a neutral creature in the Fluxborn pattern; its server-only `AVeyraWildlifeController`, which answers its camp's attacker, leashes and heals at home; and `UVeyraJungleSubsystem`, which spawns, clears and respawns the camps, reports each creature's death to Economy and grants the cleared camp's trait.
- `Wells/` (M10a, ADR-014 §4): `AVeyraFluxWell`, a neutral objective pawn, and `UVeyraFluxWellSubsystem`, which opens the Wells on the match clock, drains an open one by capped, contested presence in a present Vanguard's name, heals one left alone, and reports each secure to Match and its Gold pool to Economy.
- `Rules/`: pure rules over data — tower targeting and ramp, structure vulnerability and backdoor protection, Fluxborn targeting, the wave schedule, where wildlife stands and how far it fights, and a Well's presence drain.
- `UVeyraBattlegroundSubsystem` spawns the structures and waves on the server, runs their timers, routes hostile damage to the towers and Fluxborn nearby, and reports deaths only it can describe to Economy's rewards.

### VeyraVision

- ordinary map vision and fog of war;
- Dense Fog volumes, authored and ability-created (Vision Bible §2);
- wards, the vision-tool slot, Sweeper and Quick Sight;
- stealth detection and True Sight reveal;
- presence-sensor events.

Combat still owns targetability and hit validation; Vision supplies what each team can see. Sits at the same layer as World.

### VeyraMatch

- teams;
- match phases;
- spawn/respawn orchestration;
- score and victory state;
- match start/end;
- high-level coordination between otherwise independent systems.

Use this layer to orchestrate systems when direct peer-to-peer dependencies would create cycles.

`AVeyraPlayerState` owns each Vanguard's Ability System Component and Attribute Sets, so they survive death, respawn and reconnect (ADR-006 §4). Since M3 the module also holds:

- the GameMode, which admits players, assigns sides, runs the phases and respawns the dead;
- the GameState, which replicates the phase, the match clock and the pause;
- the PlayerController, which sends the player's intents;
- the Vanguard character, and the server-only controller that moves it (ADR-006 §7).

Since M4 it also owns how a hosted match admits and ends (ADR-007):

- the roster, which admits each participant by join ticket and gives them their side;
- `UVeyraMatchHostSubsystem`, whose input is the server's assignment and whose outputs are "accepting players" and "match ended";
- the Ended phase, the developer end-match request, and abandonment.

Since M6 it adds an assigned practice match's bots (ADR-010 §7). Since M9 it seats them as playing bots with their difficulty and announces each through `UVeyraMatchEvents::OnBotAdded` (`Bots/`), so VeyraBots can give it a brain; its order paths take the participant, so a bot orders as a player does ([ADR-013](Docs/ADR/ADR-013-ai-vanguards.md) §2–§3).

Since M8 it routes the shop and holds Recall ([ADR-012](Docs/ADR/ADR-012-items-and-shop.md) §7–§8): the fountain check tells `UVeyraShopSubsystem` who stands at their fountain, deaths deliver the queue, and the player controller forwards buy, sell, undo, cancel and item-slot requests. `Recall/` holds the channel on each PlayerState; the game mode starts it (B), ends it on every order the Vanguard takes, and brings the Vanguard home.

Since M12 `Statistics/` holds the match's one statistics service ([ADR-017](Docs/ADR/ADR-017-match-statistics.md) §3). `UVeyraMatchStatisticsSubsystem` records every participant, bots too, from the events Combat, Economy, World and Vision report, and never computes what they decide. Pure rules (`VeyraStatisticsRules`) hold the crowd-control union and which Gold counts as earned. Each PlayerState's `UVeyraScoreComponent` carries the public part, K/D/A and last hits, to every client.

Since M31 `Input/` decides how the player's keys cast ([ADR-041](Docs/ADR/ADR-041-casting-modes-and-targeting-aids.md)):
- `FVeyraCastInput`, the casting modes (Quick, Quick with Indicator, Normal) and Show Cast Range as pure transitions;
- `VeyraControlPreferences`, the control settings;
- `VeyraCursorPicks`, which unit under the cursor an order or cast names, for Target Vanguards Only and Smart Self-Cast.

The controller publishes the waiting cast for VeyraUI's indicator (`GetCastIndicator`). Attack-move orders carry their target preference to the Vanguard controller.

Since M23 `Chat/` holds in-match Team and All Chat ([ADR-029](Docs/ADR/ADR-029-in-match-chat.md)). `UVeyraChatSubsystem` validates each message on the server (`VeyraChatRules`: cleaning, length, rate, who receives which channel) and hands it to each recipient's controller, keeping mutes and the All Chat preference at delivery. No replicated actor carries chat, so spectators and replays never see it. The player controller holds the client's chat log, its own notices included, capped by `chat.keepMessages`.

It knows nothing about the backend; `VeyraServices` connects the two.

### VeyraBots

AI Vanguards ([ADR-013](Docs/ADR/ADR-013-ai-vanguards.md)), in their own Autonomy layer above Match; nothing depends on them.

- `Brain/`: what a bot knows (`VeyraBotSenses` into `FVeyraBotView`, plain data), how it decides (`VeyraBotRules`, pure and tested per priority: shop, rank, retreat and recall, fight, last-hit, siege, hold in lane), how each ability is aimed from its archetype (`VeyraBotAbilities`), where it stands in its lane (`VeyraBotLane`), and the brain component that thinks on a world-time timer and orders through the game mode;
- `UVeyraBotSubsystem`, which gives each bot Match announces its brain;
- `Tuning/`: `Game/Tuning/Bots.json`, the Beginner and Intermediate behaviours, the lane of each seat, and each released Vanguard's build, skill priority and ability uses.

Bots think only on the server.

### VeyraVanguards

Champion-specific gameplay content.

A kit is data: `Game/Tuning/Vanguards.json` defines each Vanguard, and its abilities are records in `Abilities.json` that the shared archetypes run (ADR-008 §2–§3). Code exists only for passives the shared systems cannot express, so the module is organised by kind, not by Vanguard (ADR-008 §5):

```text
VeyraVanguards/
├── Passives/   one class per unique passive (Deep Foundation, Gathering Light, Breach, Wild Dominion,
│               Never Break Stride, Slipstream, Reclaim, Unreturned), and the kit-statuses passive,
│               which runs nothing (Embedded, Hazard Exposure)
├── Shared/     generic passives any Vanguard's data can use (the hit chain)
└── Tuning/     the Vanguards.json binding and its rules
```

Vanguard code may compose Combat and Ability primitives. It must not fork or duplicate their rules. It sits in the Content layer, below Match, which prepares each participant's Vanguard.

### VeyraServices

The trusted-services client (ADR-007 §12): the only module that talks to the backend.

- the client-state coordinator (`Client/`, [ADR-010](Docs/ADR/ADR-010-play-flow.md) §2): `FVeyraClientFlow` is plain C++ that owns the game session, the client's state (signing in, starter choice, shell with the party and its queue, Match Found, champion select, match, results, Reconnect-only and the rest) and the player's intents, and reaches the backend and the engine only through injected interfaces. `UVeyraClientFlowSubsystem` hosts it in a client's GameInstance. The UI observes its snapshot and asks through `IVeyraClientIntents`; the coordinator and the backend decide;
- the game's side of the session handoff: it reads the launch code from standard input once it has said it is ready (the launch handshake, `Contracts/LaunchHandshake.json`), and redeems it;
- the front end (`FrontEnd/`, ADR-010 §3): `AVeyraShellGameMode`, the pawnless game mode of the generated `L_FrontEnd` map where the shell runs;
- the account settings sync (`Settings/`, [ADR-024](Docs/ADR/ADR-024-player-settings.md) §1): `FVeyraAccountSettingsSync` reads the player's account settings on sign-in, sends their changes once they settle, and raises the choice between this device's and the account's when both changed. The coordinator owns it, and `VeyraSettings` keeps the values behind `IVeyraAccountSettingsCache`;
- the match server's side: it reads the assignment from standard input, hands the roster to `VeyraMatch`, and reports ready and the result;
- the backend's address, waits and polling, as validated settings.

It plugs into `VeyraMatch`'s contracts, so no gameplay module depends on it or on HTTP. It sits in its own Services layer, above Orchestration.

### VeyraUI

Presentation only.

- HUD;
- shop presentation;
- scoreboard;
- draft/loadout presentation;
- menus and settings;
- accessibility presentation.

UI observes/queries gameplay state and emits user intent. No gameplay module depends on UI.

It arrived in M5 with grey-box presentation (`Greybox/`: engine shapes for bodies, projectiles and cast telegraphs) and a Canvas HUD (`Hud/`), both placeholders (ADR-008 §1). It is `ClientOnly`, so servers neither build nor load it, and the layer check enforces that.

The Settings screen (`Settings/`, [ADR-024](Docs/ADR/ADR-024-player-settings.md) §4–§5) is `UVeyraSettingsScreen` over `VeyraSettingsModels`, which reads the registry's layout and the player's store. It opens from the shell's top bar and the results, over the shell, and from the in-match menu, over the live match (`UVeyraMatchMenuSubsystem`). It never opens in champion select, Match Found or Reconnect-only. `UVeyraDisplayApplier` applies the Graphics & Display settings to the engine as they change (frame caps in front and behind, VSync, render scale, quality groups under their preset, the client's window size), and holds a disruptive display change until the player keeps it (SET-92); its rules are `VeyraDisplayRules`, apart from the engine. `UVeyraMatchDisplaySubsystem` gives a match the screen in the player's Display Mode. `VeyraInterfacePreferences` puts the player's HUD, minimap, ping, readout, scoreboard and cursor settings over the Greybox developer settings, and the HUD and `UVeyraMatchMenuSubsystem` read it; `VeyraCameraPreferences` (VeyraMatch) does the same for the camera. Key bindings reach the controller as a copy of `UVeyraInputSettings` with the player's keys over the developer's (`AVeyraPlayerController::GetKeys`), remapped when one changes; the menu, shop and scoreboard keys do the same in `UVeyraMatchMenuSubsystem`.

M6 added the menus, UMG widgets built entirely in C++ with no widget Blueprints (ADR-010 §4):

- `Shell/`: the screen for each client state (starter choice, Home, Play, champion select, results, Reconnect-only, problems), built from pure view models over the coordinator's snapshot. Buttons ask the coordinator's intents and are enabled only when it allows them;
- `Match/`: the in-match menu, with Resume, and End Custom Match for a practice match's host;
- `Text/`: what players read about Vanguards, abilities and passives, from the string table `Game/Text/VeyraText.csv` (text, not tuning);
- the style and the menu key, as validated settings in `DefaultGame.ini` and `DefaultInput.ini`.

The grey-box HUD draws through an overlay actor the local player's HUD renders (`Hud/`), so the menus cover it.

M8 added `Shop/` ([ADR-012](Docs/ADR/ADR-012-items-and-shop.md) §11): the shop screen, which P opens beside the game, and its model, which prices every item by the inventory rule the server uses. The HUD gained the item bar (keys 1–6) and Recall's channel bar, and the string table gained item names and descriptions.

M12 added `Shell/VeyraMatchReportModel`, a match's saved Scoreboard, team summary and Detailed Statistics (ADR-017 §6). The results screen shows it from the verified result, and so does Match History (`Shell/VeyraMatchHistoryModel`), a shell page that lists the player's completed matches newest first, filtered by Vanguard, mode and outcome, with Load More. It also added `Scoreboard/` ([ADR-017](Docs/ADR/ADR-017-match-statistics.md) §4): the in-match scoreboard, open while Tab is held. It has both teams, the viewer's first, and each player's Vanguard, level, K/D/A, creep score and items. Its model reads only what every client receives: each PlayerState's public score, level and inventory.

M23 added `Chat/` ([ADR-029](Docs/ADR/ADR-029-in-match-chat.md) §5): `UVeyraChatComposer`, the line chat is typed into, which the chat key opens through `UVeyraMatchMenuSubsystem` and which keeps typed keys from the game, and `VeyraChatCommands`, which reads `/all`, `/mute` and `/unmute`. The HUD's chat log (`Hud/VeyraChatLogModel`) draws the newest lines above the composer, faded unless it is open, in the player's chat text size and background from the Communication settings.

### VeyraDeveloper

Non-shipping or development-facing utilities.

- automation tests;
- debug commands;
- headless match harnesses;
- asset/data validation;
- gameplay inspection tools;
- bot/test drivers.

Developer tooling may depend on production systems. Production systems must never require developer tooling.

The debug commands are the `Veyra.Dev.*` console commands in `DevCommands/`, one catalog of them; `Veyra.Dev.Help` lists it, and a test checks that no `Veyra.Dev.*` command lives anywhere else.

- A **server command** is typed on a player's machine and runs on the server for that player's own participant. It travels through `AVeyraPlayerController::RequestDeveloperCommand`, and the reply prints in the player's console.
- The match knows only `VeyraDeveloperCommandRoute`, the handler this module installs at startup, so no production module depends on this one.
- Shipping servers refuse every command, and Shipping builds leave this module out.
- Each command acts through its owner's verbs, never by writing that owner's state: Developer Gold through Economy, damage through Combat's pipeline, items through the shop's rules. Amounts come from the typed arguments or the tuning.

Its scripted players drive the game from the command line for `Game/Scripts/Smoke.ps1`: one plays a match's script, one plays a Vanguard's whole kit, and since M6 one plays the play flow by clicking the same shell and menu buttons a player would. It plays practice alone (`-Flow Practice`), and a matchmade 1v1 in two games at once (`-Flow Casual`, `-Flow CasualDecline`). The same scripted player is also a sparring partner for a person playing the matchmade path (`Game/Scripts/Play.ps1 -Opponent`).

### VeyraWorldTools project plugin

[ADR-040](Docs/ADR/ADR-040-crucible-world-authoring-toolchain.md) places production Crucible authoring in `Game/Plugins/VeyraWorldTools/`, a project-owned **editor/developer plugin** rather than an engine fork or a packaged gameplay dependency.

- It may read VeyraWorld/VeyraCore layout contracts and `Game/Tuning/World.json`, create/update Landscape, drive the river presentation layer, invoke constrained editor-time PCG, regenerate scoped world regions, create review cameras/captures and emit generation metadata.
- Production modules do not depend on this plugin. If runtime gameplay needs a capability discovered during authoring — notably resolving a 2D gameplay placement onto the real terrain surface — that capability belongs in the appropriate production module (for the Crucible surface contract, VeyraWorld), and the plugin consumes it.
- The Epic UE 5.8.3 source checkout is read-only by default for project work. Engine changes require explicit author approval and their own ADR.
- `L_Battleground.umap` remains generated output. The plugin/tooling plus reviewed source data must make its terrain/water/art layers reproducible.

## 2. Dependency direction

Conceptually:

```text
VeyraCore
   ↓
Combat
   ↓
Economy
   ↓
Abilities / Items / Flux / World / Vision
   ↓
Vanguards (content)
   ↓
Match
   ↓
Bots (AI Vanguards)
   ↓
Services (the backend client)
   ↓
UI (client only)

Developer tooling may observe/use all layers.
```

This is a guide, not a license for arbitrary sideways dependencies. Prefer contracts/messages when peer systems need to cooperate.

### Hard dependency rules

- `Core` depends on no Veyra gameplay module.
- `Combat` never depends on a Vanguard, item, Flux, world objective, or UI.
- `Economy` never depends on shop widgets or specific items.
- `UI` may depend on read/query contracts from gameplay; gameplay never depends on UI.
- `Vanguards` never become a dependency of reusable core gameplay systems.
- `Developer` is a leaf from the perspective of production code: everything may be tested by it, nothing production-critical depends on it.
- Circular module dependencies are prohibited.

### Backend (outside Unreal)

The Go backend from [`ADR-005`](Docs/ADR/ADR-005-launcher-session-handoff-and-local-first-hosting.md) lives in [`Backend/`](Backend/README.md), with the local Docker stack in `compose.yaml` at the repository root. It is one service with one internal package per trusted domain (identity, social, party, matchmaking with Match Found, the Vanguard catalog, accounts with onboarding and entitlements, champion select, match allocation and results, and the account's settings document). Domain packages own their rules and depend on storage interfaces; storage and HTTP transport depend on domains, never the reverse. Unreal modules never link to backend code; only `VeyraServices` talks to it, over HTTP.

### Launcher (outside Unreal)

The launcher from ADR-005 L1–L5, [ADR-010](Docs/ADR/ADR-010-play-flow.md) §5 and [ADR-022](Docs/ADR/ADR-022-installer-and-game-patching.md) lives in [`Launcher/`](Launcher/README.md): a Tauri v2 app built with plain `cargo`. It installs, updates and repairs the game, signs the player in, starts the game and hands it a launch code, then closes; it never links to the game. `Game/Scripts/Play.ps1` opens it.

`core/` holds everything it does, so the window and the headless tools are thin:
- its configuration and the build manifest;
- the backend client and the launch handshake;
- the release format and the install engine.

Around it:
- **The window:** `app/` and `ui/`.
- **Headless tools** (`cli/`): `veyra-launch-cli` launches and `veyra-install` installs.
- **`publish/`:** `veyra-publish`, which turns a packaged client into a release (`Game/Scripts/Publish.ps1`).
- **`setup/`:** Veyra Setup, the NSIS installer for the launcher, built by `Launcher/Package.ps1`.

### Website (outside Unreal)

[`Website/`](Website/README.md) holds a placeholder Cloudflare Worker, which `wrangler.jsonc` at the repository root names. The repository is connected to Cloudflare's Workers Builds, and the placeholder gives that build something to deploy. The account-facing website (Profiles & Identity Bible) will replace it, after its own architecture decision.

## 3. Content directory

A likely content organization:

```text
Content/Veyra/
├── Vanguards/
│   ├── Shared/
│   ├── Raska/
│   ├── Kade/
│   └── ...
├── Items/
├── Flux/
│   ├── Spells/
│   ├── Fluxborn/
│   └── Objectives/
├── World/
│   ├── MeridianCrucible/
│   ├── Jungle/
│   └── Structures/
├── FrontEnd/
├── UI/
├── VFX/
├── Audio/
└── Developer/
```

`Developer/` holds development-only content, such as the grey-box test map `Developer/Maps/L_Greybox`. That map is generated from `Source/VeyraDeveloper/Greybox/Greybox.json` by `Game/Scripts/BuildGreyboxMap.ps1`, never edited by hand.

`FrontEnd/Maps/L_FrontEnd`, the client's default map where the shell runs (ADR-010 §3), is generated the same way by `Game/Scripts/BuildFrontEndMap.ps1`: an empty world with the shell's game mode.

`World/Maps/L_Battleground`, the server's default map where every player-made match plays (ADR-011 §12), is generated from the layout in `Game/Tuning/World.json` by `Game/Scripts/BuildBattlegroundMap.ps1`: the floor, each team's start at its fountain, navigation bounds, a sun, and the marker that has the server spawn the structures. Development matches keep `L_Greybox`.

`UI/Vanguards/T_<id>_Hero`, each Playable Vanguard's champion-select art (ADR-010, amendment of 2026-09-28), is imported from its `ConceptArt/Vanguards/<id>/hero.webp` by `Game/Scripts/BuildVanguardArt.ps1`, never edited by hand; portraits are crops of it, placed by the shell style's data.

Do not create cross-project junk drawers such as `Misc`, `Stuff`, or `Temp` as permanent homes. Temporary work should have an explicit cleanup path.

## 4. Content versus code

Use **code** when the behavior is reusable logic or a rule.

Use **data** when the thing primarily describes tunable content.

Use **Blueprint/presentation assets** when the thing primarily assembles or visualizes content.

Example: Razorwheel Prime should not own a bespoke damage formula in a widget or Blueprint. Its data references the stats/effects it grants; the reusable cleave and movement-steal behaviors execute through gameplay systems.

## 5. Naming and ownership

Before adding a class, be able to complete this sentence:

> `X` belongs in `Y` because `Y` is the authoritative owner of `Z`.

If the sentence is awkward, the class probably belongs somewhere else or the boundary needs clarification.

Avoid generic names such as `Manager` when a more precise owner exists. Prefer domain terms such as `InventoryComponent`, `TeamFluxState`, `DamageExecution`, or `ObjectiveCaptureComponent` once the actual Unreal design is decided.

### Gameplay Tag vocabulary

Gameplay Tags are Veyra's central typed vocabulary (Architecture §1.12). Native tags follow these rules:

- **Form.** Each tag is a dotted path of PascalCase ASCII segments, rooted at the canon concept and spelled as the owning bible spells it, with no project prefix: `Damage.Type.Physical`.
- **Declaration.** Tags are declared only in `VeyraCore/Public/Tags/`, one header per tag family, as `VEYRACORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN` inside `namespace VeyraTags`. The C++ symbol is the tag path with `.` replaced by `_`: `VeyraTags::Damage_Type_Physical`.
- **Definition.** Each tag is defined in the matching `VeyraCore/Private/Tags/` source file with `UE_DEFINE_GAMEPLAY_TAG_COMMENT`. The comment cites the canon section that defines the tag.
- **Nowhere else.** DeveloperTool modules never define tags, and gameplay code never builds tags from free-form strings.
- **Canon first.** A tag is added only once its owning bible closes the list it belongs to. Combat §2's descriptive damage-event tags are an open list ("such as") and wait until canon closes it; the Combat §8 crowd-control types arrive with the first status that uses them.

The `Veyra.Core.TagConvention` automation tests check every tag `VeyraCore` registers against the form rule and require each one to carry a comment.

## 6. Initial scaffolding rule

Do not create ten empty modules merely because this document lists them. The first Unreal scaffold should establish the minimum clean dependency graph required for the first vertical slice, while preserving the boundaries described here.

When a module becomes too broad or creates unwanted dependencies, split it deliberately and record major changes in an ADR.


## 7. Unified client presentation and Test Skin — architectural checkpoint (2026-09-23)

A **single installed Unreal client** contains the ordinary pre-game UI, Store/Collection, social/party and matchmaking presentation, interactive Test Skin, committed champion select, live gameplay, verified results and Replay/Spectator/Reconnect-only modes. The website and launcher are separate; the dedicated match server is still authoritative. See [ADR-004](Docs/ADR/ADR-004-unified-unreal-client-states.md). Earlier diagrams' `VeyraUI` is a **presentation responsibility**, not an independent pre-game executable or gameplay authority.

- **Client-state coordinator:** explicit legal state transitions, active UI/input focus and map/resource ownership; subscribes to authoritative match/party/queue/session state through contracts. No giant universal `GameInstance`, PlayerController or persistent level owns all gameplay, Shop, preview and match truth.
- **Ordinary shell:** Home, Play, Shop, Vanguards, profiles, Match History, persistent independently collapsible party/friends/chat surfaces and queue status. UI sends intents, never grants entitlements, money, matchmaking eligibility or progression.
- **Isolated Test Skin world:** entered from Shop inside the same application, reuses actual Vanguard assets and C++/GAS ability primitives with test-only dummies, ability resets/resources and cosmetic comparison. A preview does not act as a live match or second implementation of combat; it cannot commit authoritative rewards or purchases. Deactivate it immediately for Match Found, release unneeded resources on selection entry and return to original Shop listing on normal exit.
- **Committed state:** Match Found blocking acceptance → champion select (no ordinary page navigation or party management) → truthful loading/connect → live HUD. If the assigned match is still live after process restart or connection failure, only Reconnect is offered in pre-game. Verified completion transitions into results and restores ordinary shell. Replay/Spectator are separate nonparticipant modes with their own permissions.
- **Performance:** versioned installed assets and budgeted caching/preloading, no mandatory on-demand download for installed skins, optional loads yield to time-critical match transitions. Separate permanent backend/cache records from ephemeral world/widget state.
- **Required tests:** transition priority while testing/loading skins; no party/queue mutation from test map; no extra Shop/Settings/social entry in committed select or Reconnect-only; ability presentation parity Base/Skin; restart recovery to assigned match; valid/results-only shell restoration; stable memory bounds under repeated test-map enter/exit. Exact module names, map/world travel and Unreal implementation strategy remain provisional.

**Current pre-game client design pause: after UX-92; wait for author “continue” before UX-93.**
