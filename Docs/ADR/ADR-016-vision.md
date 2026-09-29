# ADR-016: Vision: team fog at the data boundary, Dense Fog, and the vision tools

**Status:** Proposed. The author's standing instruction (2026-09-28) is to keep working unreviewed toward a viable game, taking League of Legends' answer where canon is silent; §11 lists every such answer for the author to overturn. It becomes Accepted when the author merges the M11 pull requests that add it.  
**Date:** 2026-09-29  
**Related:** [ADR-003](ADR-003-owned-field-entities.md) (placed markers, world volumes), [ADR-006](ADR-006-unreal-project-scaffold.md) §5 (Iris and the fog gate), [ADR-009](ADR-009-runtime-combat-primitives.md) §7 (PlayerState data behind the gate), [ADR-011](ADR-011-battleground-runtime.md) (the Battleground layer, Match routing, the layout), [ADR-012](ADR-012-items-and-shop.md) (the fountain shop), [ADR-013](ADR-013-ai-vanguards.md) (bot senses), [ADR-015](ADR-015-flux-spells.md) (a slot beside the spells), [Vision Bible](../Design/Veyra_Vision_Bible_v0.1.md), [Battleground Bible](../Design/Veyra_Battleground_Bible_v0.9.md) §11, [Combat Bible](../Design/Veyra_Combat_Bible_v0.5.md) §10–§12, [Initial Roster Character Bible](../Design/Veyra_Initial_Roster_Character_Bible_v0.6.md) §19 (Bryn's Sounding Flare), [Architecture Constitution](../../ARCHITECTURE.md) §1.3, §4.

## Context

A match today has no fog: every client receives every unit, and every Vanguard can target anything in range. That is the largest gap between the game and League, whose laning, ganks and objectives all turn on what each side can see. Canon settles the rules; it leaves every number open:
- **Team fog of war** with shared ordinary vision (Vision §1). There is no brush; **Dense Fog** volumes conceal instead (§2; Battleground §11).
  - An observer outside a volume can neither see nor target an enemy Vanguard inside it, even when a teammate is inside.
  - An observer inside the same volume can.
  - Connected volumes are one volume, and abilities may create volumes at runtime.
- **Vision, detection and targetability are distinct** (§1). Knowing where a unit is does not permit targeting it. Skillshots, areas and other non-targeted effects hit what they touch, seen or not (Combat §11, §12).
- **Stealth** (Combat §11): Camouflage (hidden beyond a detection radius) and Invisibility (hidden until revealed).
  - Stealth is not Untargetable.
  - **True Sight** reveals both, and Sweeper is its only source.
  - Ordinary vision and wards never reveal Invisibility.
- **One vision-tool slot**, separate from items and spells (§3):
  - **Persistent Ward:** 3 carried charges that recharge. No cap on active wards. A ward is invisible to enemies and has its own lifetime. Inside fog it is a **presence sensor** that pings its team, never a camera.
  - **Sweeper:** True Sight in an area; revealed wards are targetable by the whole team; an enemy inside fog shows only an **outline**, which lingers briefly.
  - **Quick Sight:** ordinary vision in an area for a short time; inside fog, a presence sensor.
  - Swaps happen at the fountain for a flat Gold cost. Persistent Ward refills at the fountain and on respawn. Cooldowns survive swaps and death, and a swap never removes a ward.
- **Ward destruction** pays the destroyer a small fixed Gold, with no XP and no share (§8).
- **Fog is enforced at the data boundary, per player** (§9; ADR-006 §5):
  - A client receives an enemy unit or enemy-owned entity only while it is visible to *that player*.
  - Channels such as pings and outlines carry only what they allow.
  - Vision is the only writer of the visibility groups.
- **Bryn's Sounding Flare** (Character Bible §19) reports an enemy Vanguard present in a Dense Fog zone it overlaps, and reveals nothing.

What the code has (surveyed 2026-09-28):
1. **The M3 spike proved the gate** (`Veyra.Net.FogGate`, ADR-006 §5):
   - Units use the engine's filter-out filter, opened by per-side and per-observer inclusion groups.
   - PlayerState attribute sets use `COND_NetGroup`.
   - Nothing of it is in production. Every unit class is exempted from filtering in `DefaultEngine.ini`, and four PlayerState components carry an "until Vision gates" note.
2. **A targeting hook exists but is unused.** `VeyraCombat::CanAcquire(Acquirer, Target)` returns true.
   - Towers, Fluxborn, Oriel's passive and the movement speed toward Vanguards already call it.
   - `CheckEnemyTarget`, which basic attacks and targeted abilities use, does not.
   - Attack orders and attack-move check hostility only.
3. **Bots see by distance**, and a jungler senses every enemy within its gank range.
4. **The Battleground layer** holds Flux and World as peers; Match routes between them (ADR-011). Its description already says "Vision later".

## Decision

### 1. VeyraVision, a peer in the Battleground layer

A new module, **VeyraVision**, joins Flux and World in the Battleground layer. It depends downward on Core, Combat, Abilities and Economy. Match routes what crosses peers, as it does for Flux and World.

### 2. Visibility is one contract beside Combat's targeting, written only by Vision

- **The contract:** Combat declares `IVeyraVisibility` beside its targeting rules: `CanSee(Observer, Target)`, `IsVisibleToTeam(Team, Target)`, and a presence query over a fog zone. A world holds at most one implementation, registered by Vision's server subsystem in a world subsystem, `UVeyraVisibilityRegistry`. Core cannot hold it: the Foundation layer has no Engine dependency, and Combat already owns targetability (PROJECT_STRUCTURE §VeyraVision).
- **Callers below Vision ask Combat:**
  - `VeyraCombat::CanAcquire` returns `CanSee`.
  - `CheckEnemyTarget` gains `EVeyraTargetValidity::NotVisible`.
  - Attack orders, attack-move and bot senses use the same query.
  - A world with no implementation (unit tests, `L_Greybox` development matches) sees everything, as today.
- **What "see" means** (pure rules in `VeyraVisionRules`):
  - A target is visible to a team when one of the team's sight sources has it within its sight radius. Sources are Vanguards, Fluxborn, structures, wards and sight areas.
  - **Stealth:** an Invisible target also needs True Sight covering it; a Camouflaged one needs a source within its detection radius or True Sight.
  - **Dense Fog:** an enemy Vanguard inside a fog volume is visible only to an observer inside the same connected volume who would otherwise see it. Team vision never carries it outside. Fluxborn, wildlife and wards follow ordinary vision; canon's fog rules name Vanguards.
  - **Neutral units** (wildlife) are seen like enemies by both sides. The side's own and allied units are always visible to it.
- **Timing:** Vision recomputes on a server timer (`Vision.json` `updateSeconds`) and records, per team and per observer, what is seen. `CanSee` reads that record, so targeting and replication always agree.

### 3. The Iris gate, as the spike proved it

- **Gated classes:** Vanguards, Fluxborn, wildlife, projectiles, delayed areas and wards use the engine's filter-out filter. Their `DynamicFilterName=None` exemptions go.
- **Groups:** one inclusion group per side holds that side's own units. One per observer holds the enemy and neutral units that player currently sees. Vision is their only writer.
- **Always visible:** structures and Flux Wells, as League shows towers and objective state; the Team Flux state is public.
- **PlayerState data:**
  - The attribute sets and the status, combat-state, cast-state, basic-attack, shield (absorption) and Recall components replicate with `COND_NetGroup`.
  - What League's scoreboard shows stays public: whether a participant is alive, its level and its items. Cooldowns, the loadout and Gold are its owner's alone.
  - An ability-system component subclass registers the sets before their first send.
  - Each participant's group holds its teammates and its current observers. An observer who loses sight keeps the last value it saw.
  - Gated subobjects also join the owner's group, so the participant always has its own data, and the replay group, since the replay driver ignores Iris filters.
  - Iris learns a subobject's groups only once the subobject replicates. The PlayerState applies them as it starts replicating (`OnReplicationStartedForIris`); until then gated data reaches nobody, so a mistake fails closed.

### 4. Dense Fog volumes are layout data and a runtime API

- **Authored volumes:** `World.json`'s layout gains `denseFog`: circles in Team A's half, with Team B's derived by the layout's mirror. Placement is Battleground §11's grey-box decision; §11 of this record lists the League-like spots.
- **Runtime volumes:** Vision owns an API, `AddFogVolume(centre, radius, lifetime)`, for abilities that make fog (Sylra, later; ADR-003).
- **Connected volumes:** overlapping circles are one volume while they touch, recomputed when a volume comes or goes.
- **Presentation:** the grey box draws each volume as a dark translucent disc. Fogged enemies vanish because they are no longer replicated; darkening unseen ground waits for real presentation.

### 5. Presence is a channel, not vision

- **Sensors:** a ward's sensor inside fog, Quick Sight's area over fog, and Bryn's Sounding Flare each report "an enemy Vanguard is present in this fog zone".
  - A sensor covers its own area: a ward, its `sensorRadius` inside the fog it stands in; a lit area, its radius. Never the whole fog volume (Vision Bible §4).
  - A ping names the fog circle the enemy stands in, and a time, never a unit or a position. A new sensor pings at once, then at the data's cadence while the enemy stays.
  - Pings travel on `AVeyraVisionTeamState`, one per side, which the fog gate lets reach that side alone.
- **Sweeper's outline** is its own channel on the same team state: where an enemy Vanguard inside fog stands while True Sight covers it, then where it was last covered until it fades after the data's linger. It grants no targeting.
- **One primitive lights an area:** a timed sight area, ordinary vision for its side while it lasts, and over fog a presence sensor. Quick Sight places one. Abilities below Vision ask for one through Combat's contract, `IVeyraVisibility::RevealArea`: the area archetype's `reveal` (radius and duration) calls it as the cast commits, and Bryn's Sounding Flare has one.

### 6. The vision tools are Vision's actions, in a slot of their own

- **The slot:** `EVeyraAbilitySlot::VisionTool`, on its own key.
- **The keys:** League players use 4 for their trinket, so the item keys become 1 2 3 5 6 7 and the tool takes 4, all as input data.
- **Not GAS abilities.** Their charges refill at the fountain, their cooldowns persist across swaps, and their every effect is Vision's (wards, True Sight, sight areas). So they are Vision's actions:
  - `UVeyraVisionToolComponent` on the PlayerState holds the equipped tool, the ward charges and their recharge, and each tool's cooldown. It replicates to its owner.
  - A use order goes controller → GameMode → Vision, which checks that the Vanguard is alive and not action-blocked (Combat's blocks, as for item actives) before acting.
- **`AVeyraWard`** is a placed marker (ADR-003) owned by Vision:
  - its owner and team, its sight radius, its lifetime, Invisibility, and its sensor radius;
  - destroyed by a number of hits from enemy Vanguards' basic attacks, and by nothing else;
  - its destroyer is paid through Economy's `RewardWardDestroyed`.
- **A ward is a unit of its own kind** (`EVeyraUnitKind::Ward`), so Combat can hold its rules without naming it:
  - **It counts hits, not damage.** Only a basic attack from a Vanguard reaches it, and each lands as one point of its Health, whatever the attack's damage and modifiers. Its Max Health is the data's hit count, and its death is an ordinary death, so the credited killer is its destroyer.
  - **Abilities pass it by.** Statuses ignore it, and area and skillshot gathering skip it, as League's wards stop no skillshot. Fluxborn and towers never choose it.
  - **It stops no one.** Its body is on its own collision object channel, which every other body ignores. It still blocks the cursor's unit trace, so a player can click a ward they see.
  - **Enemies see it only under True Sight** (§5, M11b G8). Its own side always receives it.
- **Tools:**
  - **Persistent Ward:** places a ward within range, spending a charge.
  - **Sweeper:** grants True Sight around its owner for its duration.
  - **Quick Sight:** places a timed sight area within range.
- **The swap:** a shop row. Match's controller asks. The shop takes the Gold (`UVeyraShopSubsystem::ChargeAtFountain`, the fountain rule its own purchases follow; `Economy.json` `visionTools.swapCost`), and Vision equips. The tool already in the slot is refused, and so is anything away from the fountain unless its owner is dead.
- **The HUD** (§8): the tool line names the tool, with its charges or cooldown. Presence pings are rings over their fog circle, fading until the next, and outlines are small rings, both drawn from the viewer's side's team state.

### 7. Bots see what their team sees

- Bot senses read `IVeyraVisibility`, so a bot never reacts to what its team cannot see. The jungler's gank range still applies, but to seen enemies only.
- A jungler still knows which of its own camps are up, as League players keep camp timers. It walks to a camp whose creatures it cannot see yet, and attacks only those its side sees.
- The warding seats (`Bots.json` `warding.seats`: League's jungler and support) ward the Dense Fog patches they pass, League's bushes. A patch qualifies within `spotReach` of its centre, when no ward of their side stands within `spotSpacing`, with a charge in hand and no enemy Vanguard near.

### 8. The HUD

- A vision-tool slot showing its tool, charges and cooldown.
- Presence pings as a ring over the fog zone.
- Sweeper outlines as a marker.

### 9. Values are data

- `Vision.json` v3: sight radii, the update cadence, the ward and its charges, Sweeper, Quick Sight, the ping cadence.
- `Abilities.json` v5: an area's `reveal`.
- `World.json` v3: `denseFog`.
- `Economy.json` v5: the ward reward and the swap cost.
- `Bots.json` v4: `warding` (seats, reach and spacing).
- Input keys.

Canon gives the three carried charges; every other value is Provisional.

### 10. Delivery

- **M11a, the fog:** the module, the contract, targeting, the gate, Dense Fog, Bryn's presence, and the PlayerState data.
- **M11b, the tools:** the slot, wards, Sweeper, Quick Sight, pings, outlines, swaps, rewards, the HUD, and bots that ward.

### 11. League answers where canon is silent (for the author to overturn)

1. **Sight radii (League's):** Vanguards and structures 1350, Fluxborn 1100, wards 900. Vision updates every 0.2 s.
2. **Persistent Ward (League's Stealth Ward):**
   - 3 charges (canon), each recharging in 120 s;
   - each ward lasts 120 s;
   - placed within 600;
   - its fog sensor covers 500;
   - destroyed by 3 basic attacks from Vanguards.
3. **Sweeper (Oracle Lens):** True Sight within 600 of its owner for 10 s; 90 s cooldown; outlines linger 1 s.
4. **Quick Sight (Farsight Alteration, as temporary vision):** within 3500, a 500-radius area for 4 s; 120 s cooldown.
5. **Economy:** a swap costs 50 Gold; destroying a ward pays 30 Gold (League's).
6. **Presence pings** every 2 s while an enemy Vanguard stays.
7. **Keys:** the tool on 4, the items on 1 2 3 5 6 7 (League's).
8. **Dense Fog spots, League's brush:** both river entrances on each side, the tri-brush spots beside top and bottom lanes, the brush where each of those lanes meets the river, and the two brushes flanking mid on each side. The author confirmed (2026-09-29) that Dense Fog is Veyra's replacement for League's bush.
9. **Structures and Wells stay visible to both sides**, as League shows towers and the objective timers.
10. **Only Vanguards hide in fog.** Canon's fog rules name enemy Vanguards (Vision Bible §2), so Fluxborn and wildlife inside fog stay under ordinary vision. League's bush hides minions too; the author may choose that instead.

## Consequences

- **What changes:**
  - Every unit class but structures and Wells becomes fog-gated.
  - Targeting gains a visibility check; bots lose omniscience.
  - The PlayerState gains an ability-system component subclass.
  - The item keys move.
- **Bandwidth falls:** a client no longer receives the enemy's Fluxborn and Vanguards it cannot see (ADR-006 §5's second lever).
  - **Measured (M11a gate, 2026-09-29):** the packaged Linux server with 8 load-test bots and 2 clients on the battleground, the waves and jungle running, sent each client 7.8–10.2 KB/s at steady state (peak 11.4), with a server frame of 3–4 ms on average. ADR-011 §7 measured 9.6–13.6 KB/s without the fog, before the jungle and its Wells added their units.
  - **Measured again with the vision tools (M11b gate, 2026-09-29):** 9.6–11.7 KB/s per client (peak 12.7), a server frame of 3–4 ms. That is within ADR-011's range and a little above M11a's run; the difference between runs is the waves' timing. In the 8-bot match, bots placed 5 wards, and both junglers levelled and cleared 16 camps.
- **Tests:** Vision runs in every match. Network tests that are not about fog widen every unit's sight past any test map (`FScopedMatchTuning`), so they keep their meaning; fog tests restore the committed sight.

## Amendments to earlier records

- **ADR-006 §5:** the gate, made real; its exemptions removed.
- **ADR-009 §7:** the four components move behind `COND_NetGroup`.
- **ADR-011:** the Battleground layer gains Vision; the layout gains `denseFog`.
- **ADR-012:** the vision-tool swap.
- **ADR-013:** senses through vision; ward spots.

## Open items

- Darkening unseen ground on the client (presentation).
- Terrain line of sight: the grey-box map has no walls.
- Camouflage and Invisibility statuses for Vanguards (Tavi, Mimzi) use §2's rules when those Vanguards arrive.
- A minimap, and pings on it.
- Replicating fog that abilities create, so clients draw it; the map's own fog is drawn from the layout every machine has.
