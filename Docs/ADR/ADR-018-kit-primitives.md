# ADR-018: Kit primitives for six more Vanguards: Kade, Vera, Mimzi, Patch, Gorraveth, Raska

**Status:** Proposed. The author's standing instruction (2026-09-28) is to keep working unreviewed toward a viable game, taking League of Legends' answer where canon is silent. §8 lists every such answer for the author to overturn. It becomes Accepted when the author merges the M13 pull requests that add it.  
**Date:** 2026-09-29  
**Related:**
- ADRs: [ADR-003](ADR-003-owned-field-entities.md) (owned field entities, world volumes, the ride decision), [ADR-008](ADR-008-vanguard-definitions-and-ability-composition.md) (definitions and ability composition), [ADR-009](ADR-009-runtime-combat-primitives.md) (runtime combat primitives, the status ledger), [ADR-013](ADR-013-ai-vanguards.md) (bots), [ADR-014](ADR-014-jungle-and-flux-wells.md) (camps), [ADR-016](ADR-016-vision.md) (vision, stealth, True Sight).
- Bibles: [Initial Roster Character Bible](../Design/Veyra_Initial_Roster_Character_Bible_v0.6.md) §1, §2, §5, §7, §21, §23; [Combat Bible](../Design/Veyra_Combat_Bible_v0.5.md) §7–§11, §22–§24, §26, §43, §56; [Vision Bible](../Design/Veyra_Vision_Bible_v0.1.md) §4–§5.
- [Architecture Constitution](../../ARCHITECTURE.md) §1.3.

## Context

Four Vanguards are playable. Canon picks are globally unique, so a 5v5 match needs 10, Co-op needs 5 and Draft needs 16. ADR-003 orders the six with no owned field entity first: Kade, Vera, Mimzi, Patch, Gorraveth and Raska. The Roster Bible gives their kits as intent, not numbers, and each kit needs something the four shipped kits never did.

Surveyed 2026-09-29, what already exists:
- Piercing skillshots, backward dashes and area knockback.
- Secondary impacts.
- Next-attack empowerment with attack and hit hooks.
- Homing passive projectiles.
- Takedown extension.
- The visibility contract with True Sight.
- A Temporary Health ledger.
- Cooldowns keyed by ability ID, with ranks read from the slot.

What is missing:
- **Status kinds** stop at Weaken. There is no Root, Fear, Knockup, Unstoppable, Camouflage or immunity kind.
- **Attack range** is read straight from the attack profile.
- **Events:** no event announces a cast, a displacement or a cleared camp.
- **Slot changes:** nothing lets an ability change what its slot holds for a while.

## Decision

### 1. Slot overrides (amends ADR-008 §3)

The ability loadout can override what a slot holds, in four shapes. Each is data on the ability that causes it:
- **Recast window:** after a cast, the slot holds a follow-up for a time (Kade R's shots, Patch Q's throw, Raska Q's second pass). It may **auto-fire** at expiry (Last Exit, Roadhouse's canon guarantee).
- **Variant:** while a status lasts, a slot holds a different ability, such as Vera W during R or Gorraveth Q and W during R. The variant shares its slot's rank. It shares the base cooldown or keeps its own, as its data says.
- **Next-cast override:** the next cast of any basic ability runs its **Redlined** form (Raska).
- **Replacement set:** a ride state replaces Q, W and E together. The mounted set keeps completely independent cooldowns, and both sets keep counting down (Combat §56).

An override ends when its window, status or state ends, and the slot shows its own ability again.

**A stance's forms.** When a slot's own ability and its variant are both stances their recast ends (Vera's Dig In and its volley form), they are one stance: taking either form ends the other's statuses, recasting either ends every form, and a form the variant holds lasts no longer than the variant.

### 2. Status kinds (amends ADR-009 §1)

New kinds in Combat's one status ledger. Each is defined by its canon section, with its magnitude as data:

| Kind | Meaning | Crowd control |
|---|---|---|
| **AttackRange** | Adds to basic-attack range. | no |
| **AttackSpeedCap** | Raises the Attack Speed cap. Overflow is still measured from the ordinary cap (§22). | no |
| **SlowResistance** | Reduces Slows. | no |
| **Planted** | The unit's own choice to stand still, as in a firing stance: it cannot move, and may attack and cast. Not crowd control; Tenacity and Cleanse ignore it. | no |
| **Dormant** | The unit's own choice to take no action, as Patch's Play Dead: it cannot move, attack or cast. Not crowd control. | no |
| **Camouflage** | Hidden beyond the status's detection radius (§11; §4 here). | no |
| **DisplacementImmunity** | §9, as ruled 2026-09-25. | no |
| **Unstoppable** | Ordinary crowd control cannot affect the unit; Suppression still can (§8, §9). | no |
| **Fear** | The unit moves away from the source and cannot attack or cast (§8). | yes |
| **Knockup** | Airborne forced displacement (§8, §9). | yes |
| **Ghosted** | Ignores unit collision, never terrain (§24). | no |
| **BodyScale** | Scales the gameplay hitbox (§13). | no |
| **Counter** | A replicated meter with no stat effect, such as Momentum. | no |
| **DirectionalDamageReduction** | Reduction against damage from within an arc of the unit's facing. | no |
| **AttackDamageAmplification** | Amplifies basic-attack damage, optionally only against a unit kind. | no |

**Source-relative statuses.** Some statuses apply only to their source: Kade's and Vera's range against their marked target, and Gorraveth's speed toward his.

**Decay modes.** Stacking statuses may decay **one stack at a time** after a delay (Vera's Cadence), as well as all at once.

### 3. Events (amends ADR-009 §5)

Combat and Abilities announce, server only:
- **Cast started** and **cast committed**, each with whether the cast is offensive. An offensive cast is any ability or Flux Spell with an effect on enemies.
- **`OnDisplaced`:** source, target and distance, for each forced displacement.
- The attack plan names **what empowered it**, so a passive can tell its own empowerment apart.

World announces **`OnCampCleared`**: the camp, and the Vanguards who damaged any of its creatures and are alive when it clears.

### 4. Stealth: Camouflage (amends ADR-016)

Vision enforces Camouflage as it does wards' Invisibility, with these rules:
- **Hidden:** an enemy sees a Camouflaged Vanguard only when it is within the status's detection radius of one of that enemy side's Vanguards or standing structures, or under that side's True Sight (§11; Vision Bible §5).
- **Wards and Dense Fog:** wards never detect it (Vision Bible §4). Dense Fog's rules still come first.
- **What breaks it:** attacking or an offensive cast ends it (§11). Damage does not, unless the effect says so.
- **Targeting:** a Camouflaged unit is not Untargetable. A targeted projectile already launched keeps going, and area effects still hit it.

### 5. Lingering areas and shaped reveals (amends ADR-003 and ADR-016)

- A **lingering area** is a delivered area that lasts. For its lifetime it keeps applying statuses to the units inside it, chosen by side. This is the smallest form of ADR-003's world volume, and later world volumes build on it (Kade's Sightline first).
- **`RevealArea` accepts a rectangle** as well as a circle. A reveal remains ordinary vision: no True Sight, and no sight into Dense Fog. Sightline therefore reveals nothing, as its 2026-09-20 ruling requires.

### 6. New delivery and archetype options

- **Volley (a new archetype).** A cast locks a lane and roots the caster. Each shot is a recast, fired at intervals, up to a count that events may raise.
- **Tether (a new archetype).** A tether links source and target and, beyond its break range, pulls once and ends (§43). Combat keeps the tether ledger and judges it on a world timer (`Combat.json tethers.checkSeconds`): death, its time or stretching ends it; losing sight of the target or its Camouflage does not. A newer tether of the same ability from the same source replaces the older. Statuses the tether declares are held on the target while it lasts. Vision gives the source's side sight of a tethered target, which Dense Fog still overrides (§43).
- **Attach (a new archetype).** The caster leaps at its target and, ending within reach, holds on to its back for a duration, owning none of the target's movement; a leap that ends out of reach does nothing more. It is a movement mode of the caster's body: it passes through units meanwhile, cannot attack and may cast. It ends with its time, either unit's death, a displacement or Fear of the caster, or its release. Statuses the attach declares are held on the host while it lasts. A dash may throw its caster straight back from its host (`AwayFromHost`), with effects on the host, as Bear Hug's recast; the recast ends early if the hold does.
- **Options on existing archetypes:**
  - a caster recoil on skillshots;
  - consuming the caster's statuses;
  - movement during a channel, and a zone per tick;
  - damage multipliers by unit kind;
  - capped healing from hits;
  - Temporary Health from a self-buff;
  - enemy statuses from an aura;
  - an empowerment on leaving stealth;
  - an end payload scaled by hits taken;
  - a lunge on an empowered attack, with damage from missing and bonus Health;
  - **one displacement per target per cast**, which implements Raska R's canon.

### 7. Ride states (ADR-003's separate decision, made here; Combat §56)

A ride state is a movement mode on `UVeyraMovementComponent`, owned by Combat:
- **Movement:** Movement Speed is **set**, never added. Soft caps do not apply; Slows and the floor do. The heading turns at a limited rate, which the server owns: the rider keeps its speed through the widest permitted arc, never slowing to pivot. A move order to a point inside its turning circle ends at the closest approach, as soon as it starts moving away again (Match's controller). After the ride its speed falls linearly to ordinary across the ride's decay window.
- **Presence:** the rider passes through units as a Ghosted unit does; its combined, larger hitbox is a BodyScale status the ride holds.
- **Blocked actions:** attacks (attack-move becomes move), damaging structures, recall and the fountain shop. Match refuses those.
- **Kept:** an earned empowered attack survives until after the ride.
- **The ability set:** the replacement set of §1.
- **The vehicle:** it exists only from the entry cast to the end of any separated phase. On every exit, death included, it continues as a projectile along the rider's heading, and its damage is still the rider's.
- **Leaving on landing:** a dash that leaves the ride holds it until it lands, then ends it, so its landing's effects come before the vehicle goes on. A dash's landing is held by the caster, not by the ability that set off, since a used-once follow-up's ability may be removed mid-dash.
- **The ride archetype** (`ride` map) enters it: set speed, turn rate, duration, decay, statuses held on the rider, mounted actions by slot and the vehicle's skillshot. A dash may leave the ride first (`rideExit: Leave`), as Bail Out and Last Exit do. As the ride's time runs out, a recast that fires at expiry fires first, so Last Exit's payoff never loses a race with the ride's own end.

Combat also counts the **distance a unit moves itself** (Raska's Momentum), excluding forced movement.

### 8. League answers where canon is silent (for the author to overturn)

Every value is Provisional data, a League stand-in:
- **The second status kinds:**
  - Unstoppable refuses an enemy's Stun, Slow, Fear and Knockup, and every displacement; debuffs that are not crowd control, such as Weaken, still land;
  - DisplacementImmunity refuses Knockups and displacements, but not Stuns;
  - a Knockup holds the unit where it stands for its time, and Tenacity does not shorten it (League's airborne); an ability that also moves the unit pairs it with a displacement;
  - a Fear walks the unit straight away from its source for the Fear's time, under the Fear's Slow; terrain ends the walk, a displacement replaces it, and a Fear cleansed early ends it;
  - a directional reduction judges the attacker from where it stands as the damage lands, projectiles included; several multiply;
  - an attack amplification scales every component of the attack, including what passives add to it;
  - the match statistics count a Fear or a Knockup as stun time, since each is hard crowd control.
- **Kade:**
  - range 600;
  - Q in Jhin W's range;
  - E is Caitlyn E, which is how his Reposition "interacts with trajectory control";
  - R is Jhin R's volley: 3 shots, up to 2 more when allies displace a Tracked target;
  - **Tracked** comes only from forced displacement, not dashes;
  - **Dead Reckoning** banks each displacement's distance, up to 1500; at 600 or more, his next attack on a Tracked target spends it all for +40 and +0.1 Physical Power ratio per 100 banked;
  - Sightline's Attack Speed is one value, +45%, since statuses do not yet scale by rank (canon asks only for "an attack-speed benefit").
- **Vera:**
  - range 575, Physical attacks (her design sheet);
  - Cadence: 8 stacks of +7% Attack Speed, decaying one at a time;
  - her Firing Line echo is **proc damage without On-Hit** (Combat §16; League's Rageblade phantom hit does apply On-Hit);
  - R's "every third attack" counts every attack;
  - Dig In plants her: movement blocked, not crowd control;
  - "attacking the target refreshes Cadence" (Range Found): an attack on a Ranged target adds a second stack;
  - Dig In's "slower Cadence decay": each stack lasts twice as long while she is dug in;
  - during R, W is Dig In's volley form, which reaches +225 and gives +45% Attack Speed instead of +150 and +30%.
- **Mimzi:**
  - range 550, Magic attacks (her design sheet);
  - Camouflage's detection radius is 400;
  - her R bolts target only enemies she could legally acquire, and apply no Hex;
  - each bolt goes to the nearest other enemy Vanguard within 600 of the proc's target, or to the target itself when none is;
  - Hex is a Counter status from her, lapsing after 4 s; her abilities add two stacks by listing it twice;
  - "the first basic attack after emerging" is her first attack from the moment she is Camouflaged until 3 s after it ends, including the attack that ends it.
- **Patch:**
  - he cannot attack while attached but may cast;
  - a missed Bear Hug has no effect and goes on cooldown;
  - the throw lands behind him, away from the target;
  - recasting Play Dead does not end it early.
  - the Haunt comes only from an enemy Vanguard's damage, and the lash only when a Haunted enemy damages an allied Vanguard within 800 of a living Patch;
  - Bear Hug and Don't Leave Me target enemy Vanguards only;
  - Play Dead's pulse counts every hostile hit Patch takes while it lasts, damage over time included, and fears every enemy unit within its radius;
  - The Thing Inside's aura slows every enemy unit near him, Fluxborn and wildlife included; during it, Bear Hug holds longer and slows harder.
- **Gorraveth:**
  - "helping finish a camp" means he damaged any creature of it and is alive when it clears;
  - Ravine Bound's landing need not be visible, and it crosses no walls (the map has none yet).
  - a camp he helped clear, or a takedown on its 20-second cooldown, restores 6% of his Max Health and 40, with +25% Movement Speed for 2 seconds;
  - Rip Through's area is its whole path, and Q and W deal 1.5 times their damage to wildlife;
  - Furnace Rake's two sweeps are circles around him wherever he stands; each wildlife hit restores 4% of his Max Health, at most 12% per cast;
  - during R, a Q or W hit on an enemy Vanguard gives +30% Movement Speed toward enemy Vanguards, not that one alone, for 2 seconds, refreshed; the frenzy's statuses gain 2 seconds per takedown up to 6, while its Q and W forms keep their 6 seconds.
- **Raska:**
  - Momentum counts her own dashes, not forced movement, and resets on death;
  - canon gives Redlined forms only for Q and W, so E and the mounted set have none yet;
  - a destination inside her turning circle is approached at the closest point.
  - Momentum is the stacks of a Counter status: 1 per 40 units she moves herself, dashes included, +5 per attack and +8 per cast, to 100;
  - at full, Q and W hold their Redlined forms, used once and cooling down as the slots' own abilities; a ride holds Redline off until she leaves it, since the mounted set has none;
  - Roadhouse: +250 reach while it waits, and the attack adds 30, 10% of the target's missing Health and 3% of her bonus Health, lunging to the target;
  - Redlined Breakneck knocks back what it stops at; Countersteer's counter is a Slow, and Redlined its Stun, in a circle of 350 around her, and comes only if a hostile hit landed while she braced;
  - Powerslide is a cone Slow; its turn is left for later;
  - a dash that leaves the ride keeps the ride until it lands: its landing resolves first, then Hound goes on from there, sparing what Last Exit knocked up (one displacement per target).

The values live in `Game/Tuning/Vanguards.json`, `Abilities.json` and `Bots.json`.

### 9. Delivery

- **M13a:** Kade, Vera and Mimzi, with slot overrides, the first status kinds, Camouflage, lingering areas, volleys and their options.
- **M13b:** Patch, Gorraveth and Raska, with the second status kinds, tethers, attach, camp-cleared, ride states and their options.
- Each Vanguard is released with bot data, its portrait, its text and the backend's released list.

## Consequences

- The roster grows to 10, enough for 5v5 with unique picks.
- Combat's status vocabulary and events grow. Later Vanguards compose from them rather than adding one-off code.
- Vision enforces a second kind of stealth. Wards' Invisibility and Camouflage share one rule path.
- ADR-003's world volume starts as the lingering area, and its ride decision is made.

## Amendments to earlier records

- **ADR-003:** Kade's Sightline is a lingering area, the smallest world volume. Ride states are decided in §7.
- **ADR-008:** slots may be overridden (§1).
- **ADR-009:** the new status kinds, source-relative statuses, the decay mode, and the events of §3.
- **ADR-016:** Camouflage (§4) and shaped reveals (§5).

## Open items

- Spell Shields, and intercepting Hound.
- Hound damaging structures.
- Redlined forms beyond canon.
- Terrain gaps for Gorraveth's leap.
- Mirroring the turn rate on clients (ADR-009 §6 keeps no prediction).
- A displacement a newer one cuts short still reports the path it resolved, so Dead Reckoning may count more than the unit travelled; reporting on arrival would delay Tracked until the knockback ends.
- HUD meters for Cadence, Hex and Momentum beyond status stacks.
- Tethers breaking on Untargetability (§43): Combat has no Untargetable state yet.
- Bots that leave a stance before they move: until then Bots.json marks Vera's Dig In `Never`, so bots do not cast it.
