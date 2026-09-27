# ADR-008: Vanguard definitions and ability composition

**Status:** Proposed. It becomes Accepted when the author merges the M5 pull request that adds it.  
**Date:** 2026-09-26  
**Approved in:** Author decisions for M5 (2026-09-26): Vanguards before the play flow; provisional values drafted by the implementer; levels, ranks and skill points with a developer XP command; the Vision parts deferred; grey-box presentation.  
**Related:** [ADR-002](ADR-002-gameplay-ability-system.md) (GAS behind Veyra-owned integration), [ADR-003](ADR-003-owned-field-entities.md) (implementation order), [ADR-006](ADR-006-unreal-project-scaffold.md) (§3 modules, §4 GAS placement, §6 tuning), [ADR-009](ADR-009-runtime-combat-primitives.md) (the runtime primitives these definitions use), [Character Bible](../Design/Veyra_Initial_Roster_Character_Bible_v0.6.md) §13, §18, §19, §20, [Combat Bible](../Design/Veyra_Combat_Bible_v0.5.md), [Economy & Progression Bible](../Design/Veyra_Economy_Progression_Bible_v0.1.md) §1, §9, §16, [Architecture Constitution](../../ARCHITECTURE.md) §1.3, §1.9–§1.10, §3.

## Context

M5 builds the first four Vanguards in ADR-003's order: Cairn, Qazharr, Oriel and Bryn. Today the game has one targeted-damage test ability on Q, a cooldown ledger, resource costs, shields and the damage pipeline. It has no Vanguard data, no way to express a kit other than one C++ class per ability, and no levels or ranks.

Three constraints shape how kits are expressed:

- **Canon gives no numbers.** Every cooldown, cost, range, ratio and duration in these four kits, and every base stat, is open (Character Bible "prototype tuning values"; Combat §58). All of them must be data (Architecture §1.3).
- **No named-content branches in core** (Architecture §1.9), and reusable verbs over rediscovered logic (§1.10). Twenty-five Vanguards will share the same systems.
- **One owner per truth** (Architecture §3). Progression is its own owner in the Economy domain; ability execution belongs to Abilities; statuses and movement to Combat.

## Decision

### 1. New modules and layers

Three modules arrive with M5, each in a new layer of `Game/Source/ModuleLayers.json`:

| Module | Layer (position) | Owns |
|---|---|---|
| `VeyraEconomy` | Economy, between Rules and Abilities | Progression only for now: XP, levels, skill points, ranks (Economy §16). Gold arrives with its first feature. |
| `VeyraVanguards` | Content, between Abilities and Orchestration | Vanguard definitions and the extension point for unique passives. |
| `VeyraUI` | Presentation, above Services | Grey-box presentation and the HUD. Client only; no gameplay class references it. |

- The layer rule (a module depends only on lower layers) forces Economy into its own layer: Progression applies stat growth through Combat verbs, and Abilities reads ranks from Progression.
- Vanguards sit below Match, because Match prepares each participant's Vanguard; Vanguards never depend on Match.
- `EVeyraAbilitySlot` moves to `VeyraCore`, since Progression, below Abilities, needs it.

### 2. Vanguard definitions

`Game/Tuning/Vanguards.json` holds one record per Vanguard, bound through the tuning framework:

- **Identity:** the Vanguard's content ID and its resource family (only Standard, Mana, in M5; all four kits use it).
- **Body:** capsule radius and half-height, turn rate.
- **Base stats and per-level growth:** Health, resource, resource regeneration, Armor, Magic Resist, Physical Power, Magic Power, Attack Speed, Move Speed.
- **Basic attack profile:** range, damage type and ratio, windup fraction, attack-move acquisition radius, an optional personal minimum interval (Bryn), and a projectile when ranged.
- **Kit:** the content IDs of Q, W, E and R, and the passive.

`VeyraVanguards::PrepareCombatant` applies a definition to a participant: it initialises stats, grants the four abilities, hands Progression the growth table, sets the basic-attack profile and starts the passive. It replaces the M3 developer loadout in `Match.json`.

### 3. Abilities are data composed onto a few archetypes

Each ability is a record in `Game/Tuning/Abilities.json`, in exactly one archetype map. An archetype is a C++ `UVeyraGameplayAbility` subclass with its own validation and delivery; a record composes shared sub-records (cast phases, effect bundle, shape, projectile, statuses, shields) with per-rank values.

| Archetype | Delivers | First users |
|---|---|---|
| `targetedDamage` | damage to a unit (M3) | `test_bolt` |
| `skillshot` | a line projectile that stops at the first eligible unit or pierces | Cairn Q, Oriel Q |
| `area` | shapes at the caster or a ground point, optionally delayed or channelled, in zones ordered innermost first | Cairn W and R, Qazharr R, Oriel W and R, Bryn W and R |
| `selfBuff` | statuses and shields on the caster, optionally an aura on nearby allies, optionally ended early by recasting | Cairn E, Qazharr W, Oriel E |
| `empoweredAttack` | an empowerment consumed by the next basic attack (Combat §17) | Qazharr Q, Bryn Q |
| `dash` | self-movement forward or backward, with effects at start and impact | Qazharr E, Bryn E |

- **Per-rank values** are arrays: one value means constant, otherwise one value per rank (5 for basic abilities, 3 for ultimates).
- **Which archetype runs an ability** is read from the tuning maps, replacing the hard-wired choice in `UVeyraAbilityLoadoutComponent`.
- A new kit is new data when its mechanics fit the archetypes. A genuinely new delivery is a new archetype, reviewed like any core change.

### 4. Cast phases and Cast IDs

`UVeyraGameplayAbility` gains data-driven phases: windup (movement locked or free), Commit, delivery, an optional stationary channel with ticks, and recovery.

- Phases run on world time, so they freeze during a pause (ADR-006 §8).
- **Commit** is the single point where cost is paid and the cooldown starts (Combat §48, §54). A cast interrupted before Commit spends nothing and enters cooldown at 20% of its normal cooldown (Combat §26), a data value.
- Every cast gets a server **Cast ID** (Combat §45). "Once per cast" rules (Oriel's stacks, Cairn's triggers) key on it.
- A rank of 0 refuses the cast with `NotLearned`.

### 5. Unique passives

A passive that the shared systems cannot express is a `UVeyraPassive` class owned by the content domain:

- `VeyraVanguards` keeps a registry from a passive archetype key in data (`deepFoundation`, `gatheringLight`, `breach`) to its class, and `Vanguards.json` holds each passive's parameters.
- Generic passives live in `VeyraVanguards/Shared/`. Qazharr's Sea Dog uses one (a status that grows with consecutive hits on one target), so he needs no C++ of his own.
- Passives react only to events and hooks the core systems publish (casts, ability hits, basic-attack hits, takedowns, statuses). No core system names a Vanguard (Architecture §1.9).

### 6. Progression

`VeyraEconomy` owns in-match progression (Economy §16):

- levels 1–18 on a data-driven XP curve, with carry-over through several levels and XP past the cap discarded (Economy §9);
- one skill point per level; ranks up to 5/5/5/3; ultimate ranks open at levels 6, 11 and 16; no respec during a match (Economy §1, §9);
- per-level stat growth, handed down by the Vanguard definition and applied through a Combat verb that adds the same flat amount to current Health and resource (Economy §9), not the "keep the percentage" default.

Abilities reads the slot's rank at cast start. Until minions give XP, a development-only command grants XP and levels; Shipping refuses it.

### 7. Tuning format

- **Local `$ref`** to `#/definitions/…` lets shared sub-records be declared once, in both validators.
- **Cross-file references** gain wildcards and union targets: for example, every `vanguards/*/abilities/q` must name a key in one of the archetype maps.
- **Provenance:** every record whose values the implementer drafted carries `"provenance": "Provisional"`; canon values are marked `Canon`, and the author flips provisional records to `Reviewed`. `check_tuning.py` reports how many remain provisional.
- Tests compute their expectations from the loaded tuning, so retuning never breaks them.

### 8. Choosing a Vanguard before champion select

Until champion select (M6), developer data chooses Vanguards: `Match.json developerMatch.vanguards` assigns them by join order, and a development-only `-VeyraVanguard=` option overrides it for one client. Shipping refuses the option. Hosted matches (ADR-007) follow the same developer order; the match-join contract does not change.

### 9. Open canon questions and provisional answers

Canon leaves these open. M5 builds the answer on the right as provisional data or behaviour, for the author to confirm or change:

1. **Cairn Q's hook** passes through units that are not Vanguards ("the first enemy Vanguard struck"). **Oriel Q** stops at the first enemy unit of any kind.
2. **Shield categories** the kits leave unspecified: Universal for Cairn's passive and R shields, Qazharr W and Oriel E.
3. **Oriel's basic attack** is Physical (Combat §4's default, and her YAML), although her kit's damage is Magic.
4. **Bryn E** counts as a Dash (Combat §9): displacement can interrupt it, and terrain stops it without crossing.
5. **Terrain blocks skillshots** (Cairn Q states it) but not homing projectiles.
6. **No idle auto-attack acquisition** in M5: attacks come from explicit attack orders and attack-move.
7. **Oriel R** damages in channel ticks; one tick is a valid setting.
8. **Cairn E's aura** re-evaluates nearby allies on a short interval; recasting E ends it early.
9. **Qazharr:** R's knockback direction is data (perpendicular to the swing by default); E's impact deals minor Physical damage and no control; Q's slow also applies to cleaved targets.
10. **Bryn:** explosions behind the target exclude the primary target; R's range is long but finite; Q's empowerment expires after a data duration.
11. **A ground cast beyond range** is clamped to the maximum range. Walking into range to cast is deferred.
12. **Regeneration:** Mana regeneration is in; Health regeneration waits for healing.
13. **Cairn's "increased maximum Health"** is a higher base stat, not a percentage passive.
14. **Gathering Light and Breach stacks** reset on death (Combat §44).
15. **Cairn E's self-slow** is a speed modifier, not crowd control, so Tenacity does not shorten it; the slow floor still applies.

## Consequences

- A kit that fits the archetypes is reviewable data, and its numbers can be retuned without code.
- Twenty-five Vanguards share six archetypes and the Combat verbs; nothing in core names one of them.
- The numbers M5 ships are provisional playtest settings, marked in data, not balance decisions.
- Abilities now depends on Progression for ranks, and Match on Vanguards for definitions. The layer check enforces the direction.
- Adding an archetype or a passive class is a core change with its own tests, not content.

## Open items

- Champion select replaces developer Vanguard selection (M6).
- XP from minions, kills and objectives, and Gold (the rest of Economy).
- The Focus, Charge and no-resource families (their first Vanguards).
- Ability Haste as a stat, and item-driven modifiers.
- The canon answers to §9.

## Alternatives considered

- **One C++ class per ability:** simple at four Vanguards, but a hundred classes that each rediscover targeting, shapes and effects break Architecture §1.10, and every number change becomes a code change.
- **Abilities authored as Blueprints or Data Assets:** binary assets agents cannot review or edit, against ADR-006 §6.
- **Progression inside Combat:** mixes two owners that Architecture §3 keeps apart; Gold, XP and levels will grow together in Economy.
- **Vanguard definitions in `Match.json`:** Match would own content it only orchestrates.
