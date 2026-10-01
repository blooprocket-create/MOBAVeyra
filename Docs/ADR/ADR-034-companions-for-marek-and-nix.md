# ADR-034: Companions, the combat entity, for Marek and Nix

**Status:** Proposed. The author's standing instruction (2026-09-28) is to keep working unreviewed toward a viable game, choosing a provisional answer where canon is silent. §11 lists every such answer for the author to overturn. This ADR becomes Accepted when the author merges the pull request that adds it.
**Date:** 2026-10-01
**Related:**
- [ADR-003](ADR-003-owned-field-entities.md): the combat entity, the first of its three owned-entity primitives, which this ADR builds.
- [Initial Roster Character Bible](../Design/Veyra_Initial_Roster_Character_Bible_v0.6.md) §10: Marek, The Black Accord, and Nix.
- [Combat Bible](../Design/Veyra_Combat_Bible_v0.5.md):
  - §18: kill credit and assists;
  - §28: Combat State;
  - §32: summons and companions;
  - §33: tower aggression from owned companions.
- [ADR-030](ADR-030-stealth-markers-and-marked-follow-ups.md) §5: the placed marker, whose owner base the companion shares.
- [ADR-031](ADR-031-stances-focus-shadows-and-cross-marks.md) §5: blinks to an own marker, with a swap.

## Context

Marek fights from two positions at once, with Nix.
- **Nix** is a shadow creature with its own Health and battlefield presence. It attacks independently, can be killed and later reforms.
- **Passive, Bound Together:** when Marek and Nix both damage the same enemy within a short window, **Accord** deals bonus magic damage. It has a per-target cooldown.
- **Q, Witchfire:** a magic bolt. Against a target Nix damaged lately, it triggers Accord and an explosive interaction.
- **W, Hunt:** Nix leaps to an area, damages and slows there, remains there and attacks independently. Recast recalls Nix.
- **E, Cross the Chain:** Marek and Nix swap places. Both departure points erupt. Hitting one target from both sides can trigger Accord, and successful Accords can cut the cooldown.
- **R, Hell on a Leash:** Nix takes a larger true form. The bible's list of what it *may* include: more Health, more movement speed, a shorter Hunt cooldown, two Cross the Chain charges, a stronger Accord. A visible chain joins the two; enemies crossing it take damage, with a per-enemy internal cooldown. If Nix is killed, the ultimate ends.

ADR-003 named the combat entity's job: Health, autonomous behaviour, destructible. It serves Nix, Picket and Waterling, and nothing of it existed yet. The survey for this ADR found that:
- Combat records attribution (Combat State, contributions, kill credit) only when a Vanguard is the source, so an owned unit's damage would count for nobody.
- Economy and statistics match a killer's Ability System Component exactly against the participants', so a companion's last hit would pay no Gold and count no minion kill.
- Item effects need an inventory on the unit whose hit it was. Owned units therefore already inherit no item effects, as Combat §32 requires.

## Decision

### 1. Owned units and attribution (Core, Combat)

**`EVeyraUnitKind::Companion`** is a new unit kind: an owned combat entity of a Vanguard's.

**`IVeyraOwnedUnit`** (Combat) is a unit that belongs to a Vanguard. It answers its owner's Ability System Component. The placed marker and the companion implement it.

**`VeyraCombat::ResponsibleFor(Source)`** is the unit a source answers to: its owner for an owned unit, followed to a unit that is owned by none. Ownership resolves to one root; a summon's summon makes no new root (Combat §32).

Combat applies it wherever it attributes:
- **A hostile action** by an owned unit counts as its owner's for Combat State (§28) and for the target's contributions (assists, participation and structure rewards).
- **The death event:**
  - `Killer` becomes the responsible unit of the lethal source. Last-hit Gold, minion-kill statistics and quest progress therefore go to the owner of a companion that last-hits.
  - The new `LethalUnit` keeps the unit whose hit it was.
  - Kill credit resolves from the responsible unit.
- **Hostile damage** (`OnHostileDamage`) gains `Responsible`. The battleground's aggression router reads it, so an owned unit's damage to a defending Vanguard draws tower and Fluxborn priority to its owner while the owner is in range (§33).
- **Dealt damage** (`OnDamageDealt`) keeps its source, the unit whose hit it was. Item effects, which read the source's inventory, therefore never fire from a companion's hit: nothing is inherited unless declared (§32).
- **Match statistics** add an owned unit's dealt damage to its owner's record.

Killing an owned unit is no kill of its owner and no takedown (§32). Its death is announced like any death, and every reward and statistic reads its kind and ignores it.

### 2. A status that raises Max Health (Combat)

**The MaxHealth status kind** multiplies its holder's Max Health by one plus its magnitude while it holds. Health keeps its share of the maximum as it changes, as for every Max Health change.

### 3. Companions (Abilities)

**Abilities.json's `companions` map** defines a companion:
- its body;
- its stats at its owner's first level, and their growth per owner level;
- its explicit inheritance, `ownerMagicPowerShare`: the share of its owner's Magic Power it holds as its own;
- its basic attack profile;
- its behaviour distances: `followDistance`, `leashRange`, `acquireRange`, and `ownerTargetSeconds` (how lately its owner must have contributed against a unit for the companion to prefer it);
- `reformSeconds` and `thinkSeconds`.

**`AVeyraCompanion`** is a character, like a Fluxborn:
- it walks on the Veyra movement component, so crowd control and forced moves work on it;
- it owns its Ability System Component (Minimal replication), the combat components a unit needs and a basic attack component;
- it stands on its owner's side and is an owned unit.

It inherits nothing else. Items, critical strikes, on-hit effects, lifesteal and its owner's buffs stay its owner's (§32).

**`UVeyraCompanionSubsystem`** (a world subsystem) keeps each owner's companion:
- `Summon(Owner, Id)` asks for one. The subsystem forms it beside its owner once the owner has a living body.
- On a world-time timer, so a pause holds it, the subsystem:
  - banishes the companion when its owner dies;
  - reforms it beside its owner, at full Health, `reformSeconds` after it is killed, or as its owner revives, whichever is later;
  - keeps its grown stats and inherited Magic Power current.
- A banished companion keeps its actor: dead, hidden, without collision, as Vision and every gatherer then pass it by.

### 4. A companion's behaviour (Abilities)

**`AVeyraCompanionController`** is server-only and thinks on a world-time timer.
- **Follow:**
  - It keeps within `followDistance` of its owner.
  - It attacks an enemy its owner contributed against within `ownerTargetSeconds`, while that enemy is within its `acquireRange` and its owner's `leashRange`.
  - Otherwise it returns to its owner's side.
- **Hold** (an ability sends it, §5):
  - It stands at a point and attacks enemies within `acquireRange` of it, in this order:
    1. one its owner contributed against lately;
    2. an enemy Vanguard;
    3. the nearest other enemy unit.
  - It returns to the point between fights.
  - The hold ends when its time runs out, when it is recalled, when its owner moves beyond `leashRange`, or when it is banished.
- **Crowd control:** while crowd control locks its movement it holds, as a Fluxborn does.

**Others' view of it:**
- **Towers and Fluxborn** rank a companion with the Fluxborn: before structures and Vanguards.
- **Wildlife** fights the companion that hurts its camp, as it fights any attacker.
- **Vision:**
  - it is a gated unit;
  - it sees for its side by `sight.companion` (Vision.json);
  - a banished one sees nothing.
- **The HUD** gives it a bar and the minimap a unit dot. Bots perceive Vanguards, Fluxborn, structures and camps, and pass companions by (§11).

### 5. Commands (Abilities)

**The `command` archetype** (Abilities.json `commands`) sends its caster's companion to a ground point within cast range:
- the companion dashes there at `speed`;
- where it arrives, its `landingZones` hit as the companion's own hit;
- it then holds for `holdSeconds`.
- A cast's recast window opens a follow-up command with `order: Recall`, which ends the hold at no cost. The hold's end closes that window.

**Blinks gain `To: OwnCompanion`.** The blink goes to its caster's living companion, within cast range.
- With `Swap`, the two exchange places.
- **`departureZones`** erupt at both departure points:
  - the caster's eruption is the caster's hit;
  - the companion's eruption is the companion's hit.
  - Both are prepared from the caster's rank and power.

### 6. Reactions that burst (Abilities)

**A reaction may name `burstZones`.** These are zones centred on the reacting target that hit the caster's other enemies around it, as Witchfire's explosion against a target Nix bit.

### 7. Self-buffs for a companion (Abilities)

**A self-buff may name `companionStatuses`:** statuses its caster's companion holds while the buff lasts, as Nix's true form.

**`companionDeath: Ends`:** the buff ends when its companion is banished.

**A `chain`** (at most one):
- while the buff lasts, enemies on the line between caster and companion, within `width`, take its effects at each pulse;
- each enemy at most once per `perEnemySeconds`;
- the effects are the caster's hit.

The companion replicates whether it is chained, so presentation draws the line.

### 8. Casts that need a companion, and cooldowns while a status holds (Abilities)

- **`needsCompanion`** (at most one companion ID) refuses the cast, as `NoCompanion`, while its caster has no living companion of that ID.
- **`cooldownWhile`** entries scale the cooldown a cast starts while its caster holds a status, as Hell on a Leash shortens Hunt's.

### 9. Accord (Vanguards)

**The `accord` passive map** names:
- the companion it summons as it starts;
- a window and a per-target cooldown;
- Accord's damage: magic, by level, plus a Magic Power ratio;
- the mark status the companion's hits leave, which Witchfire reacts to;
- the slot whose cooldown each Accord shortens, and by how much;
- a boost status (Hell on a Leash's) and the multiplier Accord's damage takes while its owner holds it.

**How it triggers:** when the owner and the companion have each dealt damage to one enemy within the window, and that enemy's cooldown has passed, Accord deals its damage from the owner, as a proc.
- A proc's damage starts nothing new.
- Cross the Chain's two eruptions are the two hits, so they trigger Accord without a special case.

### 10. Marek

Marek joins the roster with Bound Together, Witchfire, Hunt, Cross the Chain and Hell on a Leash, as the sections above express them. Hell on a Leash grants Nix more Health, more speed and a larger body, shortens Hunt's cooldown, strengthens Accord, and raises the chain. It ends if Nix is banished.

### 11. Provisional answers where canon is open

1. **Nix's Health and attack grow with Marek's level.** Nix holds a share of Marek's Magic Power as its own; nothing else is inherited.
2. **Following,** Nix attacks only what Marek has fought lately. Holding a point, it attacks what is near.
3. **Hunt's hold lasts a fixed time;** Nix then returns.
4. **Nix reforms** a fixed time after it is killed, or when Marek revives.
5. **When Marek dies,** Nix is banished until he revives.
6. **Killing Nix** pays nothing and is no takedown.
7. **Towers and Fluxborn** treat Nix as they treat Fluxborn.
8. **Nix's last hits** pay Marek, and its damage counts in his statistics.
9. **Bots ignore companions** for now; Marek's bot plays his kit.
10. **Hell on a Leash** leaves out the second Cross the Chain charge, one of the bible's optional benefits.
11. **The values** in Abilities.json and Vanguards.json are Provisional playtest settings.

## Consequences

- Picket and Waterling build on the companion: their own behaviours (a gun and shield mode; following an ally or hunting) become companion options rather than new actors.
- A companion is a gated unit with its own Iris filter entry, like a Fluxborn.
- Tuning schemas list the new unit kind wherever they list kinds.
