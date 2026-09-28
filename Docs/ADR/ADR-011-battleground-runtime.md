# ADR-011: The battleground runtime: lanes, Fluxborn, structures, Team Flux, rewards and victory

**Status:** Proposed. The author reviews it before M7's implementation begins (gate G1). It becomes Accepted when the author merges the M7a pull request that adds it.  
**Date:** 2026-09-28  
**Approved in:** Author decisions for M7 (2026-09-28): a winnable match on a three-lane grey-box battleground; Gold is earned now and spent from M8; open values are provisional data the author reviews here; Custom practice runs on the full battleground with no victory condition.  
**Related:** [ADR-003](ADR-003-owned-field-entities.md) (runtime terrain), [ADR-006](ADR-006-unreal-project-scaffold.md) (§3 modules and layers, §4 GAS placement, §5 networking and the lane population study, §7 movement, §8 pause), [ADR-007](ADR-007-match-join-contract.md) (§7 result, §8 how matches end), [ADR-008](ADR-008-vanguard-definitions-and-ability-composition.md) (Vanguard definitions, provenance), [ADR-009](ADR-009-runtime-combat-primitives.md) (statuses, attribution, projectiles, basic attacks), [ADR-010](ADR-010-play-flow.md) (§7 practice, §11.9), [Battleground Bible](../Design/Veyra_Battleground_Bible_v0.9.md) §2–5, §9–10, §12, §16–19, [Combat Bible](../Design/Veyra_Combat_Bible_v0.5.md) §18, §24, §33, §55, [Economy & Progression Bible](../Design/Veyra_Economy_Progression_Bible_v0.1.md) §1–9, §14, §16, [Architecture Constitution](../../ARCHITECTURE.md) §1.3, §3, [Project Structure](../../PROJECT_STRUCTURE.md) §2 (VeyraFlux, VeyraWorld, VeyraEconomy).

## Context

After M6 a player can reach a match through the launcher, practice or matchmaking, and champion select. The match cannot be won.

- `L_Greybox` is one straight lane: no structures, no Fluxborn.
- `AVeyraGameMode::EndMatch` never sets a winner. The end reasons are developer request, abandoned and host-ended.
- Gold does not exist. XP comes only from developer commands, and is stored as an integer, against Economy §1's fractional precision.
- Death uses one flat respawn delay.
- VeyraFlux and VeyraWorld are planned modules (Project Structure §2) with no code.

M7 builds the battleground that makes a standard match winnable. Canon settles the rules — three Fluxways, three Spires per lane, inhibitors, two base-defense towers, the passive Prime Well, tower aggro, Team Flux and the reward rules. It gives almost no numbers and no base geometry (Battleground §16 lists both as open). This record settles how the systems are built, and proposes provisional values for the author to review.

Several facts in the code shape the design:

1. **The planned layer for Flux and World cannot hold them.** `ModuleLayers.json` puts World and Flux beside VeyraAbilities in the Abilities layer. Fluxborn need `UVeyraBasicAttackComponent`, and tower shots need `AVeyraProjectile`, which both live in VeyraAbilities, and a module may not depend on a module in its own layer.
2. **Combat records attribution only between Vanguards.** Kill credit for a death the environment finishes (Combat §18) is not implemented, and a Vanguard's takedown effects fire when it kills anything.
3. **"Structure Attack" and "Structure Projectile"** (Combat §55) are on Combat §2's open descriptive-tag list, and Project Structure §5 forbids adding tags from an open list.
4. **No Vanguard declares a Primary Damage Type** (Combat §33). All four basic attacks are Physical, and every base Magic Power is 0.
5. **Healing does not exist yet** (ADR-009 open item). The Prime Well's regeneration needs a minimal form of it.
6. **In-process play sessions replicate no map-placed actor** (ADR-006 §8), so anything the network tests must see is spawned at runtime.

## Decision

### 1. Scope and delivery

- **M7a (one pull request): the battleground and victory.**
  - The three-lane map and its structures.
  - Tower attacks with aggro and ramp.
  - Invulnerability and inhibitor rebuilds.
  - Team Flux from structures.
  - Victory, reported to the backend and shown as Victory or Defeat.
  - A player wins by sieging with basic attacks; there are no Fluxborn yet.
- **M7b (a stacked pull request): lanes and rewards.**
  - Fluxborn and their waves, targeting and backdoor protection.
  - Gold and XP rewards.
  - The respawn curve.
  - The HUD for Gold, Team Flux and respawn.
  - Scale evidence.
- **Deferred:**
  - the shop and items (M8); AI Vanguards that fight (M9);
  - jungle wildlife, Flux Wells, vision, fog and Dense Fog;
  - Flux Spells and their permanent-Flux slot unlocks;
  - recall; buyback; kill-streak bounties and death-streak devaluation;
  - surrender; statistics beyond the winner;
  - the fountain barrier that holds Vanguards in during preparation (ADR-006 §7 deviation stands).

### 2. Modules and the Battleground layer

**Amends ADR-006 §3 and Project Structure §2.** A new **Battleground** layer sits directly above Abilities and below Content. It holds **VeyraFlux** and **VeyraWorld** as peers; VeyraVision joins it when it arrives, as Project Structure already places Vision "at the same layer as World".

The layer order becomes Foundation → Rules → Economy → Abilities → **Battleground** → Content → Orchestration → Services → Presentation → Composition (sealed), Developer (sealed).

- **VeyraCore** gains:
  - `EVeyraUnitKind::Structure`;
  - `EVeyraStructureKind {LaneSpire, BaseTower, Inhibitor, PrimeWell}`;
  - `EVeyraLane {Top, Mid, Bottom}`.
- **VeyraCombat** gains:
  - the damage delivery kind (§5) and the structure rules;
  - invulnerability as a verb;
  - a minimal Health restore;
  - kill credit, attribution on any victim, and a hostile-damage event (§6).
- **VeyraEconomy** gains:
  - **Gold** (`Gold/`): a per-player ledger and the pure reward rules.
  - **Rewards** (`Rewards/`): the input events `FVeyraFluxbornDeath` and `FVeyraStructureDestroyed`, and a subsystem that routes deaths and destructions to the Gold and Progression owners. It holds no formulas of its own.
  - **Progression:** XP becomes fractional, plus the XP-sharing rules.
  - Gold and XP stay in separate classes, as Project Structure requires.
- **VeyraFlux**:
  - the Team Flux ledger (permanent total plus independent temporary grants on world-time timers);
  - Fluxborn strength;
  - a change event;
  - a replicated state actor for the HUD;
  - `Game/Tuning/Flux.json`.
- **VeyraWorld**:
  - the battleground subsystem (layout, spawning, the structure graph);
  - structures and their attacks;
  - Fluxborn and their controller;
  - waves;
  - the aggro router;
  - backdoor protection;
  - `Game/Tuning/World.json`.
  - Rules are pure functions with unit tests.
- **VeyraMatch** gains `FVeyraBattlegroundLink`, owned by the GameMode so the GameMode does not grow. It:
  - starts waves when the match goes live;
  - holds loading until the battleground is ready;
  - routes between World and Flux;
  - decides victory;
  - stops the battleground and rewards when the match ends.

### 3. How the systems talk

Calls go down the layers, events go up, and the two peers meet only through Match. This is the route Project Structure names: "world objectives report destruction/capture through Core contracts or match orchestration".

- **A structure is destroyed** (Combat's death event) → World:
  - updates invulnerability and inhibitor timers;
  - calls Economy's structure reward (a downward call);
  - raises its structure-destroyed event.

  → Match grants the data-defined Team Flux, and for a Prime Well decides victory (§13).
- **Team Flux changes**, including a temporary grant's expiry: Flux raises its change event → Match passes the team's active Flux and Fluxborn strength to World. World caches them and rescales every living Fluxborn of that team.
- **A Fluxborn dies** → World writes its cached active Team Flux into `FVeyraFluxbornDeath` and calls Economy. Economy never queries Flux: "the Fluxborn death event carries the value" (Project Structure, VeyraFlux; Economy §16).
- **Vanguard kills** reach Economy from Combat's death event directly, since Economy sits above Combat.

### 4. Structures are units

- **`AVeyraStructure` is an `APawn`**, so the existing unit gathering, grey-box bodies and HUD bars, which all iterate pawns with a unit kind, include it without new paths.
- Each structure carries:
  - its own ASC in Minimal replication (ADR-006 §4);
  - the combat components;
  - the vitals, offence and defence sets;
  - its kind and lane;
  - a replicated state: Standing, Invulnerable, Destroyed or Rebuilding, with the rebuild time.
- Its capsule is a dynamic navigation obstacle.
- A destroyed structure keeps its actor. Inhibitors rebuild, and destroyed Spires stay on the map as wrecks.
- **Structures are spawned on the server from the layout while the match loads** (§12), not placed in the map, so network tests see them and navigation settles before preparation.

### 5. Structure combat rules (Combat §33, §55)

- **Delivery kinds instead of structure tags.** A prepared hit carries `EVeyraDamageDelivery {BasicAttack, Ability, StructureAttack, Proc, Developer}` and whether it is structure-enabled.
  - Canon's Structure Attack and Structure Projectile are expressed by the `StructureAttack` delivery. No descriptive tags are added (Project Structure §5).
  - Combat §2's tag list stays open; if the author adds these tags to canon later, the delivery kind maps onto them.
- **Structures take damage only** from a basic attack, a tower attack, or a structure-enabled ability (Combat §33: "Normal abilities do not damage structures"). Empowered basic attacks are basic attacks.
- **Structure defences:** structures have their own Armor and Magic Resist, and Vanguard penetration and resistance reduction do not apply to them.
- **No crowd control or debuffs on structures**, unless a status is explicitly allowed.
- **Targeting and area effects.**
  - Targeting an enemy structure with an ability that is not structure-enabled is refused with its own reason.
  - Areas, skillshots, cleaves and secondary impacts do not gather structures.
- **Structure Effectiveness** (`Combat.json`, Canon 50%) scales secondary riders on a basic attack against a structure: crit bonus, on-hit damage, empowered bonus damage, and lifesteal. The converted basic-attack damage stays at full effectiveness.
- **Primary Damage Type** (amends ADR-008 §2).
  - Every Vanguard declares one in `Vanguards.json`, and structure basic attacks use its matching power and the structure's matching defence. The engine never guesses it.
  - Provisionally all four are **Physical**: their basic attacks are Physical and their base Magic Power is 0. Oriel's kit reads as magic, so the author should confirm her type before her Magic Power grows.
- **Tower attacks** (Combat §55):
  - Physical;
  - the `StructureAttack` delivery;
  - not basic attacks;
  - no crit.
  - Shields absorb them. Physical immunity and full invulnerability still work.
  - Untargetability or stasis before impact makes the projectile fail.
  - Spell Shields and projectile blockers, which do not exist yet, will not stop them.

### 6. Kill credit and attribution (amends ADR-009 §3)

- **Attribution is recorded for any victim** that carries an attribution component (Vanguards, Fluxborn, structures), with the time of each contribution. Economy needs it for Fluxborn participation (Economy §3.2) and structure pools (§8.1).
- **Combat decides the credited killer** (Combat §18):
  - the source of the lethal damage, when it traces to an enemy Vanguard;
  - otherwise the latest enemy Vanguard that contributed within `Combat.json attribution.killCreditWindowSeconds`;
  - otherwise an Execution with no killer.
  - A tower that finishes a Vanguard therefore credits the enemy who fought them.
- **Takedowns, kills and assists are Vanguard-victim terms** (Combat §18). A Vanguard's takedown effects (for example, Qazharr's ultimate extension) fire only on Vanguard victims, and Combat State stays between Vanguards.
- **Combat raises a server-side hostile-damage event** (source, target, delivery). World's aggro router is its one subscriber (§7, §8).

### 7. Fluxborn (amends ADR-006 §7)

- **Movement.** Fluxborn use CharacterMovementComponent through `UVeyraMovementComponent`, so slows and displacement work on them. A server-only AI controller moves each one. There is no client prediction, and Mass is not used (ADR-006 §7 already rules it out).
- **`AVeyraFluxborn`** has:
  - its own ASC in Minimal replication;
  - the combat components;
  - a basic attack whose profile comes from `World.json`, using the same attack component and profile type as Vanguards.
- **`AVeyraFluxbornController`**:
  - thinks on a world-time timer, so it freezes during a pause;
  - walks its lane's waypoints (reversed for Team B);
  - after a chase, resumes at the first waypoint ahead of it along the lane, never behind;
  - holds its order while crowd control locks movement, like the Vanguard controller.
- **Target choice** (Battleground §19, Combat §33), in order:
  1. responding to aggression: an enemy Vanguard that damaged a nearby allied Vanguard, until it leaves the engagement range;
  2. for siege Fluxborn, a structure already in attack range;
  3. the nearest enemy Fluxborn;
  4. an enemy structure;
  5. an enemy Vanguard.

  Ties break by distance, then by a stable ID.
- **The aggro router.** One World subscriber to Combat's hostile-damage event notifies the towers and Fluxborn near the victim, rather than each unit subscribing.
- **Collision.** Blocking collision (Combat §24), with the movement component's avoidance among Fluxborn; the radius and weight are data.
- **Replication** (amends ADR-006 §5):
  - Fluxborn and structures replicate at whole fractions of the 30 Hz server tick, set in `World.json` as "every N server ticks";
  - push model;
  - Iris filter exemptions until Vision brings the fog gate, like Vanguards.
- **Expected population:** about 60–80 Fluxborn in steady state and about 110 at peak, plus 30 structures. That is within the M3 study's measured range (72–162 stand-ins). M7b measures it again with real Fluxborn.

### 8. Towers

Lane Spires and base-defense towers share one attack component and one set of rules.

- **Normal targeting:** hostile Fluxborn first, keeping the current one while it stays valid; otherwise a Vanguard in range.
- **Vanguard priority:**
  - An enemy Vanguard that damages a defending Vanguard while both are in the tower's range takes priority. Owned damage, damage over time included, counts when its owner is in range at the moment of the tick.
  - A valid priority target keeps priority; a second attacker does not steal it.
  - Priority ends when that Vanguard leaves range, dies or becomes untargetable. Nothing lingers.
- **Ramp:** consecutive shots on the same Vanguard gain a step per shot up to a cap (Canon illustration: +20% per shot, five stacks). The ramp resets on target loss or switch, leaving range, death or untargetability.
- **Isolation:** each tower owns its target and ramp; nothing is shared, and there are no named-unit cases.
- **Shots are homing `AVeyraProjectile`s.**

### 9. Invulnerability, inhibitors, the Prime Well and backdoor protection (Battleground §18–19)

- **Invulnerability** is applied through Combat's invulnerability verb, from a pure World rule over the structure graph:
  - base towers are invulnerable while all of their team's inhibitors stand;
  - the Prime Well is vulnerable only while both base towers are destroyed and at least one of its team's inhibitors is down;
  - damage already dealt stays.
- **Inhibitors:**
  - a destroyed inhibitor rebuilds at full Health after its rebuild time, on a world timer, so a pause holds it;
  - it grants its temporary Team Flux through Match (§3);
  - newly spawned waves in its lane gain extra units while it is down (M7b).
- **Prime Well regeneration:**
  - The Prime Well alone regenerates missing Health, and only while all of its team's inhibitors stand.
  - It stops at once when any inhibitor falls.
  - Spires and base towers never regenerate.
  - This is the first use of Combat's minimal Health restore, a Veyra execution on Combat §41's allow-list.
- **Backdoor protection** (M7b):
  - Lane Spires, base towers and the Prime Well gain damage reduction while no Fluxborn of the attacking team is within the protection radius.
  - It ramps toward its maximum after the last one leaves or dies, and drops to zero the moment one enters.
  - It is a self-sourced damage-reduction status, so the normal pipeline applies it.
  - It never overrides invulnerability.
- **No outer-before-inner gating.** Canon makes only base towers and the Prime Well conditionally invulnerable. Lane Spires are gated by position and backdoor protection alone (Battleground §5, §10). This is provisional answer 3 in §16.

### 10. Team Flux

- **Sources** (Battleground §5, §10, §18; `Flux.json`):
  - lane Spires and base towers grant permanent Flux;
  - inhibitors grant temporary Flux, each grant expiring on its own timer;
  - Flux is never spent.
- **Active Flux** is permanent plus unexpired temporary. It sets Fluxborn strength for the whole team: each step of Flux adds a fraction of Health and damage (Canon prototype: every 25 adds +5% of each).
- **Live scaling.** World applies the strength to each Fluxborn at spawn and again to every living one on each change. Canon says temporary Flux "falls away when its source expires". It is one infinite native effect per unit with multiplicative Max Health and damage lines, and Max Health keeps its current percentage (Combat §41).
- **Permanent Flux** is kept separate for Flux Spell slot unlocks, which are deferred.
- **HUD:** each team's active and permanent Flux, the Fluxborn bonus, and the countdown of each temporary grant.

### 11. Gold, XP and respawn

- **Gold:**
  - `UVeyraGoldComponent` on the PlayerState holds a fractional balance, replicated to its owner only;
  - every grant carries a reason and is logged for audit;
  - starting Gold is granted when preparation begins;
  - there is no passive Gold (Economy §1);
  - the UI rounds only for display.
- **Rewards** follow Economy §3–§8:
  - **Fluxborn:**
    - 100% of listed Gold to the credited last hitter, or unclaimed;
    - 10% participation Gold to each other living nearby ally when an allied Vanguard damaged it recently;
    - XP to living nearby allies: solo 100%, or one 120% pool shared, with Level 18 excluded;
    - the listed values rise 1% per 25 active Team Flux of the Fluxborn's team, up to 10%, evaluated at death.
  - **Vanguard kills:**
    - base kill Gold to the killer, plus one 50% pool split among assistants;
    - First Blood +50% once, with a deterministic tie-break;
    - kill XP by victim level, pool × (1 + 20% × (participants − 1));
    - the higher-level multiplier applies once;
    - Level 18 counts toward the bonus but is excluded from the split.
  - **Structures:**
    - Spires and base towers pay a fixed pool split among recent contributors, even dead or distant ones;
    - the first Spire or base tower destroyed in the match adds a bonus to every member of the destroying team;
    - inhibitors and the Prime Well pay nothing, and structures give no XP.
  - **Stop at victory:** nothing is paid after the match ends (Economy §8.2).
- **XP becomes fractional** (Economy §1); thresholds and carry-over are unchanged.
- **Respawn** (Economy §14):
  - The timer grows with the Vanguard's level and the elapsed match time, from a data curve in `Match.json`.
  - A replicated respawn time drives the HUD countdown.
  - Death never costs Gold, XP or levels.
- **Fountain recovery** (Battleground §12): a living Vanguard at their own fountain recovers Health and resource at a data rate, through the same Health restore. This is provisional answer 8.

### 12. The battleground layout and map

- **One data file drives both the map and the server's spawning.** `Game/Tuning/World.json` `layout` holds:
  - the floor;
  - three lane polylines from Team A's base to Team B's;
  - each lane's inhibitor as a distance along the lane from its owning end, and its Spires as distances beyond that inhibitor;
  - Team A's Prime Well, base towers and fountain.

  Team B's positions follow from a declared mirror (a reflection across the diagonal that maps each lane onto itself), so distances are balanced by construction, as Battleground §2 asks. Travel distance is gameplay, so the layout is tuning: hashed, staged and compared at login.
- **`UVeyraBattlegroundMapCommandlet`** (VeyraDeveloper, following the `L_Greybox` and `L_FrontEnd` commandlets) bakes `Content/Veyra/World/Maps/L_Battleground`:
  - the floor, lane strips and base pads;
  - the fountains as team starts;
  - navigation bounds and light;
  - a marker that tells the server to spawn the battleground.

  `Game/Scripts/BuildBattlegroundMap.ps1` runs it, and the map is never edited by hand.
- **Runtime spawning.** The server spawns structures from the layout when it finds the marker, during loading. Network tests spawn a compact test layout the same way.
- **Map selection:**
  - `ServerDefaultMap` becomes `L_Battleground`.
  - The backend names a map per kind of match: each mode, Custom practice, and development matches. It passes the map to the server it starts.
  - Development matches stay on `L_Greybox`, so M3–M6's smoke runs keep their meaning.
  - `Play.ps1 -Direct` and `Smoke.ps1` choose the map.
  - `L_Greybox` stays.

### 13. Victory and the result (amends ADR-007 §7)

- **The Prime Well's destruction ends a Standard match.** Match calls `EndMatch` with the new reason `PrimeWellDestroyed` and the winner, the Prime Well's opposing side. A winner is required with that reason and forbidden with every other.
- **The backend:**
  - accepts `prime_well_destroyed` only with a winner and only for Standard rules;
  - adds a migration whose check keeps the reason and the winner together.
- **The client's results screen** shows Victory or Defeat from the verified result.
- **Results record the winner only.** Gold and other statistics come with the statistics work (Match Statistics Bible).
- **The backend's maximum match duration stays a failure safety net**, far above canon's 20–45-minute pacing. It is not a match-length cutoff, which canon forbids (Battleground §18).

### 14. Practice (amends ADR-010 §7)

- Custom practice runs on the battleground, with waves and structures, and **no victory condition**; the host has switched normal victory off (Custom Matches Bible).
- A destroyed Prime Well stays destroyed, the waves keep coming, and End Custom Match ends the match as before.
- The practice bots stay.

### 15. Testing and the developer siege

- **Pure rules have unit tests:**
  - the wave schedule across phase boundaries;
  - lane resume;
  - tower targeting and ramp;
  - structure invulnerability;
  - backdoor protection;
  - Fluxborn targeting;
  - the Flux ledger;
  - Gold and XP rewards, with the bible's worked examples.
- **Network tests** use a compact layout (one short lane holding every structure kind) and short timers through scoped tuning. No test waits on the real schedule.
- **The developer siege.**
  - `Veyra.Dev.Siege` destroys the requester's next enemy structure in lane order, with a lethal developer hit through the pipeline. Invulnerability, Team Flux, reward pools and victory therefore run for real.
  - Shipping refuses it, like the other developer commands.
  - `Smoke.ps1 -Flow CasualVictory` uses it to reach Victory and Defeat end to end in minutes.

### 16. Provisional answers where canon is silent

1. **Layer placement:** Flux and World are peers in a Battleground layer, routed through Match (§2–§3).
2. **Structures are pawns**, spawned from data (§4).
3. **No outer-before-inner invulnerability** for lane Spires (§9).
4. **All four Vanguards are Physical-primary** for now; Oriel is to be confirmed (§5).
5. **Temporary Flux changes Fluxborn live**, not only at spawn (§10).
6. **Kill-credit window** of 10 s, matching the assist window; takedown effects only on Vanguard victims (§6).
7. **Results record the winner only** (§13).
8. **Fountain recovery exists** at provisional rates (§11). Without it, Health returns only on respawn, since there is no recall.
9. **Practice after its Prime Well falls:** the match continues (§14).
10. **Per-kind server maps**, with development matches on `L_Greybox` (§12).
11. **Reward values live with their owner.** Gold values are in Economy's tuning and XP values in Progression's, keyed by Fluxborn ID. A test keeps them in step with `World.json`'s Fluxborn list.
12. **Fluxborn target order** among equals (§7) and **tower "normal targeting"** keeping its current Fluxborn (§8): canon gives priorities, not tie-breaks.
13. **The grey-box layout and every number in §17.**

### 17. Values are data

Every value below is designer-editable data; none is a constant in code. Each record is Provisional unless marked Canon, and the author flips reviewed records to Reviewed (ADR-008 §7). The numbers are starting points for playtesting, not borrowed truths.

| Area | Owner | Values |
|---|---|---|
| Layout | `World.json` | Floor 14000 × 14000; bases in opposite corners about 5000 from the centre, mirrored; side lanes about 16 800 long and mid about 11 000 |
| Spire positions | `World.json` | Distance from the owning inhibitor: side lanes 1200 / 3200 / 5500; mid 1000 / 2200 / 3400. Three per lane (Canon) |
| Lane Spire and base tower | `World.json` | Health 3500 / 3000; Armor 60, Magic Resist 60; 150 Physical damage every 1.0 s; range 750; projectile speed 1200; ramp +20% per shot, five stacks (Canon illustration) |
| Inhibitor | `World.json` | Health 3000; rebuild 180 s (Canon); +1 Breaker per new wave in its lane while down |
| Prime Well | `World.json` | Health 5500; regenerates 0.5% of Max Health per second while all inhibitors stand |
| Backdoor protection | `World.json` | Radius 1100; maximum 66% damage reduction; ramp over 5 s; checked every 0.5 s |
| Fluxborn | `World.json` | Strider: Health 450, Physical Power 12, 1.25 attacks/s, range 110. Spark: 290, 23, 0.67/s, range 550, projectile speed 650. Breaker: 900, 40, 0.5/s, range 300, Armor 30. Move speed 325 for all |
| Fluxborn AI | `World.json` | Think every 0.25 s; acquisition 700; aggression response 700; leash 900; waypoint acceptance 150; avoidance radius and weight |
| Waves | `World.json` | First wave at 0:30 (Canon); every 30 s, 25 s from 14:00, 20 s from 30:00 (Canon); 3 Striders + 3 Sparks; a Breaker every 3rd wave, every 2nd from 30:00 |
| Replication | `World.json` | Fluxborn every 3 server ticks (10 Hz); structures every 6 |
| Team Flux | `Flux.json` | Lane Spire and base tower +25 permanent (Canon); inhibitor +25 for 180 s (Canon); every 25 active Flux gives +5% Health and +5% damage (Canon) |
| Gold | `Economy.json` (new) | Starting 500; Strider 21, Spark 14, Breaker 60; base kill 300; assist pool 50% (Canon); First Blood +50% (Canon); participation 10% (Canon); Flux reward bonus 1% per 25, up to 10% (Canon); Spire and base-tower pool 250; first-Spire team bonus 100; participation radius 1400; participation window 10 s; structure contribution window 15 s |
| XP | `Progression.json` (v2) | Strider 60, Spark 30, Breaker 93; base kill XP 60 + 30 × (victim level − 1); higher-level victim ×1.2; radius 1400; shared pool 120% and +20% per extra participant (Canon) |
| Kill credit | `Combat.json` | Kill-credit window 10 s |
| Structure Effectiveness | `Combat.json` | 50% (Canon) |
| Respawn | `Match.json` (v4) | Level 1: 6 s, rising to 45 s at Level 18; +2% per minute after 15:00, at most +50% |
| Fountain recovery | `Match.json` | Radius and Health and resource per second at the fountain |
| Maps | backend configuration | The server map for each mode, Custom practice and development matches |

## Consequences

- A standard match can be won and lost, and the verified result says who won.
- The battleground's rules are pure, tested functions over data, so tuning and layout changes need no code.
- Two modules and one layer are added. Match gains a routing class rather than growth in the GameMode.
- **Combat's changes ripple:**
  - XP becomes fractional through the HUD and tests;
  - takedown effects stop firing on non-Vanguard kills;
  - abilities stop damaging structures.
- **Replicated population grows several times over.** Bandwidth is the limit ADR-006 §5 found; M7b measures it and tunes update rates.
- **`L_Battleground` is a binary map that needs an LFS lock.** It is generated, so its source of truth is text.

## Amendments to earlier records

- **ADR-006:**
  - §3: the Battleground layer, VeyraFlux and VeyraWorld;
  - §5: Fluxborn and structure update rates and Iris filters;
  - §7: Fluxborn movement.
- **ADR-007 §7:** the `prime_well_destroyed` end reason with a winner.
- **ADR-008 §2:** every Vanguard declares a Primary Damage Type.
- **ADR-009 §3:** attribution on any victim, the credited killer, the hostile-damage event, and takedowns on Vanguard victims only.
- **ADR-010:**
  - §7: practice on the battleground without victory;
  - §11.9: the Prime Well now ends standard matches; End Match (Developer) stays for development.

## Open items

- Final base geometry, Fluxborn scaling formula, respawn curve and every value in §17 (Battleground §16).
- The fountain barrier during preparation, which would let Vanguards move inside their fountain (ADR-006 §7 deviation).
- Recall, the shop and buyback (M8 and later).
- Kill-streak bounties and death-streak devaluation (Economy §5.3–5.4).
- Oriel's Primary Damage Type.
- Whether "Structure Attack" and "Structure Projectile" become canon tags.
- The Vision fog gate for Fluxborn and structures.

## Alternatives considered

- **Flux below World in stacked layers.** World could then call Flux directly and subscribe to its changes, with no routing in Match. It contradicts Project Structure's rule that objectives report "through Core contracts or match orchestration", and it puts two systems canon treats as peers in an order that later features (Flux Spells acting on the world) would fight.
- **Structures as plain actors with their own gathering and HUD paths.** It would duplicate unit gathering, presentation and bars for 30 units that behave like units.
- **Map-placed structures.** In-process network tests would not see them (ADR-006 §8), and the map would become a second source of truth beside the data.
- **A separate geometry file in VeyraDeveloper, like `Greybox.json`.** Lane lengths and structure positions are gameplay, so they belong in hashed tuning that the server and clients compare.
- **Mass for Fluxborn.** Its replication is not a candidate in 5.8 (ADR-006 §7). The measured population is within what characters handle.
- **Fluxborn strength applied only at spawn.** It is simpler, but temporary Flux would not "fall away" from units already on the field.
