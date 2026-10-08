# ADR-065: First-match feedback: waves, Spires, reward and level feedback, ability numbers, and Patch

**Status:** Proposed. The author's standing instruction (2026-09-28) is to keep working unreviewed toward a viable game, choosing a provisional answer where canon is silent. The closing section lists every such answer for the author to overturn. This ADR becomes Accepted when the author merges the pull request that adds it.
**Date:** 2026-10-06
**Related:**
- [ADR-011](ADR-011-battleground-runtime.md): §7 Fluxborn, §12 layout, §17 values. This record amends the leash and the Spire distances.
- [ADR-040](ADR-040-crucible-world-authoring-toolchain.md): half-turn symmetry and smaller bases (2026-10-05).
- [ADR-010](ADR-010-play-flow.md): the string table holds no numbers. This record keeps that rule and adds numbers drawn from tuning beside the text.
- [ADR-052](ADR-052-combat-text-health-bars-zoom-and-attack-range.md) §1: combat text. [ADR-063](ADR-063-combat-readability-cues-effects-sound-and-the-fountain-shop.md): cues, effects and sounds from replicated state.
- [ADR-018](ADR-018-kit-primitives.md): tethers and auras.
- [ADR-024](ADR-024-player-settings.md) §2: a setting is a data entry, a text row, and the code that applies it.
- [Roster Bible](../Design/Veyra_Initial_Roster_Character_Bible_v0.6.md) §5 (Patch); [Combat Bible](../Design/Veyra_Combat_Bible_v0.5.md) §43 (tethers), §50 (damage at Commit).

## Context
The author played the hosted 0.2.0 build on 2026-10-06: a practice match as Patch, alone against four bots, for 18 minutes. The match result records 1 kill, 5 deaths, 20 Fluxborn last hits and 3,115 Gold. Of the 12,578 damage Patch took, the enemy Vanguards dealt about 3,400; Fluxborn and towers dealt the rest. The bots last-hit 47 to 79 Fluxborn each.

The author's notes:
- the lane Spires stand too close together;
- a Fluxborn followed them into the jungle;
- ability descriptions should show the damage an ability deals, before the target's defences;
- Patch has little to survive with. His tether should siphon Health from its target, and his ultimate felt useless;
- last-hitting was hard, and once the waves overwhelmed them there was no coming back;
- they want to see "+X" Gold as they earn it, and to notice when they level up.

The code:
- **Waves on the wrong lane.** Team B's waves spawn at Team A's spawn point turned half a turn. Since the battleground became its own half-turn rotation (ADR-040, 2026-10-05), that rotation carries the top lane onto the bottom. So Team B's top and bottom waves appeared on the other side lane and crossed Team B's jungle to reach their own. A jungler met Fluxborn there that seemed to have chased them. Structures and waypoints already walked each lane from the team's own end; only the spawn did not.
- **The leash** bounded only the target. A target counted while it stood within 900 of the lane's path, which is 550 into the jungle. The Fluxborn itself could stray without limit, and a call for help renewed the chase at every hit.
- **Spires.** Mid's Spires stood 1,200 apart with 750 range, so their reach overlapped.
- **Missing feedback.** No Gold grant, level gain or killing blow reached the client as a moment. The bots predict a last hit on the server; the player had no such cue.
- **Text.** Ability text is prose with no numbers (ADR-010).
- **Patch.**
  - His tether deals nothing and heals nothing.
  - His ultimate grants 200/350/500 Temporary Health plus 10% of Max Health, and a 20% slowing aura. It deals no damage.

## Decision

### 1. Each team's waves spawn on the lane they walk
`VeyraLayout::FluxbornSpawnPoint(Lane, Team)` is the lane's spawn distance along it from the team's own end, as its structures stand. The server spawns there. No placement on a lane is ever Team A's point rotated, because the rotation maps a side lane onto the other. A layout test checks every lane and team: the spawn lies on the lane the wave walks, and nearer its own inhibitor than the enemy's.

### 2. The leash bounds the Fluxborn as well as its target
- **The Fluxborn itself:** at each thought, a Fluxborn farther than `fluxborn.ai.leashRange` from its lane's path lets its target go and walks back. This holds even when the target stands nearer the lane; a call for help cannot hold it out there.
- **The target:** a target farther out is let go, as before.
- **The value:** the leash shortens from 900 to **550**. Lanes are 700 wide, so a Fluxborn pursues at most 200 past the lane's edge.

### 3. Spires stand farther apart
| Lane | Distances from the inhibitor (inner, middle, outer) | Gaps | Outer Spire to the enemy's, along the lane |
|---|---|---|---|
| Top, Bottom | 900, 3,900, 7,300 (were 2,600, 4,600, 6,900) | 900 · 3,000 · 3,400 | 5,800 (was 6,600) |
| Mid | 800, 2,700, 4,600 (were 2,400, 3,600, 4,800) | 800 · 1,900 · 1,900 | 3,584 (was 3,184) |

- **The inner Spire** now guards its inhibitor from just in front of it.
- **Beyond it**, each gap is wider than two tower ranges, so no two Spires cover each other.
- **What else changes:** these are data. Spawning, navigation, the minimap and the bots read them live. The Crucible map's review cameras, which stand at the outer Spires, are rebaked by `BuildBattlegroundMap.ps1`.

### 4. Gold shows as it is earned
- **Economy:** a Gold grant can carry where it was earned. The reward routing passes the fallen unit's place for last hits, participation, wildlife and wards.
- **Match:** the combat-text link (ADR-052 §1) also watches each participant's Gold and sends the owner a Gold line:
  - at that place when there is one;
  - otherwise over the owner's own Vanguard, for kills, assists, bounties, First Blood, structures and Flux Wells.
- **Silent grants:** passive income, starting Gold, sales, undo and developer grants show nothing.
- **Merging:** grants that arrive at one moment on one anchor merge into one figure, so a kill and its bounty read as one number.
- **The client** draws "+X" in the Gold colour. It is a new combat-text kind, with its own setting (Interface, on by default).
- **The place is sent with the line:** a fallen Fluxborn's body lasts a second, shorter than the number.

### 5. Levelling up is a moment
- **The cue:** the cue subsystem (ADR-063 §1) raises a level-up cue when a seen Vanguard's Level rises. Level already replicates to every client.
- **Everyone sees:** a body burst on that Vanguard (a generated Niagara effect).
- **The player who levels also gets:**
  - a level-up sound;
  - a banner under the top strip, "Level N", with a skill-point line when one is unspent;
  - the rank-up marks over the abilities pulse while a point is unspent.
- **Reduce UI Animation** stops the pulse; Reduce Flashing softens the burst ([ADR-055](ADR-055-accessibility-settings-and-hud-warnings.md)).

### 6. A last-hit cue
- **One formula:** `VeyraBasicAttacks::ExpectedHit` gives the damage a unit's basic attack deals a target before critical hits and on-hit effects. It takes the attack's profile and the attacker's power, and reads the target's resistances and the mitigation constant. The bots' last-hit judgement uses it too, so there is one formula.
- **The cue:** a damaged enemy Fluxborn's or creature's bar carries a mark at the player's expected hit: the Health one basic attack would finish. The moment its Health falls to the mark, the attack kills, and its Health turns gold.
- **The inputs reach the client already:** the player's Vanguard's power (owner-only), the target's Health and resistances, and the profile from tuning.
- **The setting:** "Last-Hit Cue" (Interface, on by default).
- **It is a hint:** it ignores critical hits, other attackers and projectile travel, and never decides anything.
- **Amended by [ADR-071](ADR-071-playtest-readability-strands-kit-rings-and-a-ready-last-hit.md) §5:** a Ready stage before gold now counts the attack's windup, its projectile's flight and the Health the target is losing to others.

### 7. Ability numbers beside the text
- **The text table still holds no numbers** (ADR-010).
- **Gameplay supplies the numbers:** `VeyraAbilityRules::DamageParts(Tuning, Ability)` walks an ability's tuning with the validator's own walker. It returns each damage list with the role its field plays: on hit, on contact, each pulse, at the end, each second, and so on. `VeyraEffectDelivery::DamageAmount` is the formula.
- **In a match:** the ability's tooltip lists each part at the slot's rank with the player's current power, before defences. For example, "On hit: 180 magic damage (130 + 50% Magic Power)". It also shows the cooldown and cost at that rank.
- **In champion select:** the ability list shows each part per rank, for example "60 / 95 / 130 / 165 / 200 (+50% Magic Power) magic damage".
- **Role names and labels** are text-table rows with no numbers in them.

### 8. Patch survives and his ultimate matters
**Two general mechanisms**, which any Vanguard's data may use:
- **A tether siphon.** A tether may declare a siphon. While the tether holds, every interval it deals its damage to the target, prepared at the cast (Combat Bible §50). Its source then restores the declared share of the Health the target actually lost: measured before and after the pulse, so shields and resistances count, and never above Max Health.
- **Aura damage.** A self-buff's aura may deal damage each second to the enemy units it reaches, prepared at the cast and dealt at each refresh in proportion.

**Patch's tether, Don't Leave Me** (author ruling, 2026-10-06): it siphons 12 / 17 / 22 / 27 / 32 magic damage every 0.5 s, returning all the Health it takes. That is up to 72–192 over its 3 seconds.

**Patch's ultimate, The Thing Inside:**
- Temporary Health rises to 300 / 475 / 650 plus 15% of Max Health (was 200 / 350 / 500 plus 10%).
- The aura's slow rises to 30% (was 20%).
- The aura deals 25 / 40 / 55 magic damage each second (+10% of Magic Power) to enemies within 350.
- A moment after he grows, enemies within 350 are shoved 200 away and slowed for 1 second. This uses the self-buff's timed payload.
- It keeps the stronger Bear Hug, the larger body and the footing.

The Roster Bible's Patch is a protector who holds an enemy close. The siphon makes holding on keep him standing, and the ultimate now claims the ground around him, as his canon's "stronger zone control" asks.

**Text:** both abilities' descriptions say what they now do, without numbers.

### 9. A fallen body lies where it fell
The author's second match found that kills vanished. A dead Vanguard's body left the map on the next tick, so no client saw it fall: its death animation and collapse never played.

Now the body stays where it fell until its Vanguard returns:
- **Out of play:** it doesn't move, collides with nothing, gives no sight, and is no target.
- **Seen falling:** every client that sees it sees it fall and lie there.
- **At respawn:** the game mode removes it as the Vanguard returns in a new body.

The fallen of Fluxborn and wildlife stay for 2.5 seconds (`corpseSeconds`, was 1), so a Gold number and the fall finish before the body goes.

### 10. A kill feed
**The feed.** Every player gets a line for each fall: Vanguards and structures. A fall is no secret, as the scoreboard's kills are not. The match's link sends it, mirroring combat text (ADR-052 §1).
- **Down the top right:**
  - the killer's face and name, the number of assists, then the fallen's face and name, in the colours of their sides;
  - "Executed" when no Vanguard is credited; the structure's name for a structure;
  - First Blood outlined in gold.
- **Announcements**, under the top strip, for:
  - the player's own takedown ("You slew Oriel") and death ("Oriel slew you", "You were executed");
  - First Blood, whoever drew it;
  - a structure of either side ("Enemy top outer Spire destroyed").

### 11. Bodies drawn larger
The author found the bodies too small for the battleground.
- **Sizes:** Vanguards' animated bodies (and their companions' and Echoes') are drawn **1.65 times** their capsule, structures' art **1.32 times**, each from its foot.
- **Presentation only:** the capsule, and so collision, reach, ranges and hit areas, keeps its size. This matches how a body's look commonly reaches past its hitbox.
- **Following the bodies:** health bars and combat numbers stand over the drawn body.
- **Clicking:** the cursor finds a unit within 30 units of its line (a sphere sweep), so clicking a body where it is drawn picks it.

### 12. Drawn bodies ease between updates
The author found movement rough. ADR-062 §6 relied on the engine's smoothing for other machines' units. That smoothing eases only a character's mesh, and every Veyra body hung from the capsule. So on every client:
- each unit stepped to each update the server sent, 30 times a second for Vanguards and 10 for Fluxborn;
- each turn came in steps of 16–24°;
- each stop overshot by a few units and snapped back.

The fix:
- **What hangs from the mesh.** A body's drawn parts (the animated skin, the grey-box shape or disc, the art) hang from the character's mesh.
- **What follows it.** Health bars, combat numbers and a camera that follows the player's body read that place (`VeyraDrawnBody`, which movement owns). The camera ticks after that body's movement.
- **Ease times.** A Vanguard's, companion's or Echo's body eases to each new place and facing over **0.1 s**. A Fluxborn's or creature's eases over **0.15 s**, since their updates come less often. These are client presentation settings (`DefaultGame.ini`). **Amended by ADR-067 §1:** 0.04 s and 0.05 s for Vanguards, companions and Echoes, and 0.1 s for Fluxborn and creatures, after a hosted match showed the ease holding back turns and stops.
- **What stays the same.** The server's movement and every rule stay as they were: nothing the server decides reads a drawn body.

Not changed here: the server's tick rate (30), the avoidance settings, and the server's frame time in a long siege. The author's second match shows that frame time rising as Fluxborn pile up in a falling base: past 700 replicated actors, 13–20 ms on average, with spikes to 200 ms. That needs profiling first.

## Consequences
- Team B's side-lane waves arrive on time, from their own inhibitors, and no wave crosses a jungle. Lane pressure is even again.
- Fluxborn stay with their lane: a Vanguard who steps 200 into the jungle loses them.
- Lane fights happen farther from the next Spire back; a fallen outer Spire exposes more lane before the middle one.
- Players see their Gold, their level-ups, their killing blows and their abilities' numbers as they play.
- Patch can trade and sustain while he holds a target, and his ultimate threatens and shoves.

## Verification
- Layout: `EachTeamsWavesSpawnOnTheLaneTheyWalkBeforeTheirOwnInhibitor`.
- Fluxborn: `BeyondItsLeashItLetsItsTargetGoThoughTheTargetStandsWithin`.
- Committed World.json validation (Spire rules).
- Rewards and text: Gold lines reach the owner with their place (routing and network tests); the level-up cue from a Level change; the expected-hit formula against the bots' former judgement; the damage parts of known abilities.
- Patch: siphon and aura-damage tests (a tether heals its source by the Health its target lost; an aura's damage each second).
- The full suite, the Editor, Client and Linux Server builds, and the flow smokes.

## Provisional answers for the author
1. Leash 550 from the lane's path, for the Fluxborn and its target (§2).
2. Spire distances (§3).
3. Which Gold grants show, and where (§4).
4. The level-up burst, sound, banner and pulse (§5).
5. The last-hit cue ignores critical hits and on-hit effects, and is on by default (§6).
6. Tooltip format and role names (§7).
7. Patch's siphon values and his ultimate's new values (§8).
8. Fluxborn and wildlife bodies last 2.5 seconds (§9).
9. The kill feed's rows, timing, and which moments are announced (§10).
10. The body scales 1.65 and 1.32, and the 30-unit cursor pick (§11).
11. Ease times of 0.1 s for Vanguards, companions and Echoes, and 0.15 s for Fluxborn and creatures (§12).
