# ADR-037: Deployables, postures, cover and designation, for Eudora and Picket

**Status:** Proposed. The author's standing instruction (2026-09-28) is to keep working unreviewed toward a viable game, choosing a provisional answer where canon is silent. §9 lists every such answer for the author to overturn. This ADR becomes Accepted when the author merges the pull request that adds it.
**Date:** 2026-10-01
**Related:**
- [Initial Roster Character Bible](../Design/Veyra_Initial_Roster_Character_Bible_v0.6.md) §25: Eudora Blackbridge, The Fieldwright, and Picket.
- [Combat Bible](../Design/Veyra_Combat_Bible_v0.5.md):
  - §20: projectile interception, by directional guards among others;
  - §32: summons and companions.
- [ADR-034](ADR-034-companions-for-marek-and-nix.md): companions and commands.
- [ADR-035](ADR-035-sea-states-crashing-rides-and-summoned-companions.md) §5: summoned companions, the Escort mode.

## Context

Eudora fields Picket, a four-legged rivet-cannon-and-bulwark platform. It is **equipment, not an aware companion, and never Fluxborn**.
- **Passive, All Hands:** when an allied Vanguard, Eudora included, damages an enemy Vanguard within Picket's operating area, Picket earns bounded Work.
  - Each contributor has an internal cooldown, so multi-hit spells, damage over time and procs cannot farm it.
  - At a threshold, Work repairs Picket's Health by a capped amount.
  - No Picket, no Work.
- **Q, Drive Rivet:** a physical linear rivet that damages the first enemy hit.
  - A struck enemy Vanguard is briefly **Designated**: Picket in Gun Platform prefers it for its next otherwise legal shot.
  - Designation grants no reveal, no range and no Dense Fog acquisition.
- **W, Set the Picket:** after a readable setup, one Picket at a nearby valid point, in Gun Platform: slow physical shots at legal targets, with controlled bonus damage against Fluxborn.
  - It has finite Health, lifetime and range, and can be destroyed.
  - There is no second while one stands, and redeploying needs W.
- **E, Raise the Bulwark:** the existing Picket switches to Bulwark.
  - Its gun stops, and a directional frontal shield gives allied Vanguards and Fluxborn behind its covered side partial, capacity-limited protection against ranged attacks and projectiles.
  - A bounded share of what it intercepts goes to Picket's Health.
  - Uncovered angles, melee and ground effects pass.
  - A recast after the mode cooldown returns to Gun Platform. It never fires at full and guards at full at once.
- **R, MOVE THE LINE!:** the existing operational Picket unanchors and follows a chosen allied Vanguard at limited speed for a short time.
  - Meanwhile it has reduced gunfire and reduced frontal protection, facing the cast's direction, and nearby allied Fluxborn gain a small temporary shield.
  - At the end it anchors where it is, in its earlier mode.
  - If Picket is destroyed, the ultimate ends.

The survey for this ADR found that:
- **Companions** (ADR-034, ADR-035) are owned combat entities with Health, a basic attack, modes (Follow, Hold, Escort, Hunt), summons for a while that never reform, and command orders.
  - Nothing places one at a point for good, keeps it from walking, or switches what it does in place.
- **Combat Bible §20** makes projectile interception canon, by "directional guards attached to a Vanguard" among others, but Veyra implements none. Picket's Bulwark is a partial, capacity-limited form of it.
- **Statuses** have no information-only mark that a unit's own attacks prefer.

## Decision

### 1. Deployed companions (Abilities)

- **The command order `Deploy`:**
  - forms its caster's companion, the command's `companion`, at the target point (the nearest ground there), **anchored**, for `lifetimeSeconds`;
  - casting it again while that companion lives moves it to the new point, keeping its Health and starting its time again;
  - a deployed companion never reforms: destroyed or out of time, it is gone until the next cast (as ADR-035 §5's summons).
- **The companion mode `Anchored`:**
  - it never walks;
  - it attacks the legal enemies within its basic attack's range, never chasing;
  - its owner's distance does not end it, nor does its owner's death: it stands until its time runs out or it is destroyed;
  - it faces away from its caster, the way the cast pointed.
- **Its target:** a legal enemy its owner Designated (§5), else one its owner fought lately, else the nearest.

### 2. Postures (Abilities)

- **A companion definition gains `postures`**, in order, the first its starting posture. Each:
  - `attacks`: `Attacks` or `Holds`;
  - `statuses` it holds while it stands in the posture.
- **The command order `ChangePosture`:** the caster's living companion takes its next posture, its statuses replacing the last one's. The ability's cooldown is the mode cooldown.
- **A companion definition gains `movingStatuses`**, which it holds instead of its posture's while an order moves it (§3).

### 3. Moving a deployed companion (Abilities)

- **The command order `Unanchor`:** the caster's living anchored companion escorts the allied Vanguard the cast names (ADR-035's Escort) for `lifetimeSeconds`.
  - It holds its moving statuses instead of its posture's, facing the cast's direction.
  - It then anchors where it is, in its earlier posture.
  - If the companion is destroyed, the order ends.
- **A companion definition gains `movingAura`**, at most one: statuses for the allied units of the given kinds within a radius, at a pulse, while it moves (Picket's shield for nearby Fluxborn).

### 4. Cover (Combat; Combat Bible §20)

- **A status kind, `Cover`:**
  - magnitude: the share of eligible damage it prevents;
  - its `arcDegrees`: the arc it faces;
  - a new status field, `cover`: its `reach` behind it, its `capacity`, and its `transferShare`;
  - its `unitKinds`: the kinds of ally it shelters, every kind when it names none (Picket's are Vanguards and Fluxborn).
- **The cover rule:** a projectile's damage on a unit is reduced when an allied unit holding Cover, other than the unit itself, stands between them:
  - the unit is behind the cover, within its reach, against its facing;
  - the projectile's source lies within its arc, in front.

  Projectiles are a basic attack's projectile, a skillshot's and a targeted ability's.
- **What it prevents** is its share of the hit's damage before mitigation. It comes out of its capacity, which refills each time the status is given.
- **`transferShare` of what it prevents** is dealt to the cover's holder, as a proc from the source.
- **What passes:** melee attacks, areas, effects over time and anything outside the arc.
- **One cover:** the strongest cover that applies answers for a hit.

### 5. Designation (Combat; Abilities)

- **A status kind, `Designated`:** it blocks nothing and changes no stat.
- **A companion prefers** a legal enemy holding Designated from its owner over other candidates. Designation reveals nothing and extends no range.

### 6. All Hands (Vanguards)

- **The `allHands` passive map:**
  - when an allied Vanguard, its owner included, damages an enemy Vanguard within `radius` of its owner's living companion, the passive earns `work`, at most once per `perContributorSeconds` for each contributor;
  - at `threshold` it spends that much Work, restoring `repair` Health to the companion, never past its maximum.
- **No living companion, no Work.**

### 7. Eudora (Vanguards)

- **Her kit is data:**
  - Drive Rivet, a skillshot whose first enemy hit is struck and, if a Vanguard, Designated;
  - Set the Picket (`Deploy`), Raise the Bulwark (`ChangePosture`) and MOVE THE LINE! (`Unanchor`);
  - Picket, a companion with Gun Platform and Bulwark postures and its moving statuses and aura;
  - All Hands.
- **Picket's bonus against Fluxborn** is a Gun Platform status, `AttackDamageAmplification` for Fluxborn.

### 8. Vision and presentation

- **Picket is a companion:** gated, seeing by `sight.companion`, and drawn as a unit with a bar.
- **Its posture shows** as its statuses do.

### 9. Provisional answers where canon is open

1. **Picket is a companion mechanically.** Killing it pays nothing (ADR-034 §1).
2. **Cover applies to projectiles only**, never melee, areas or effects over time (Combat Bible §20).
3. **Cover's capacity** refills each time its status is given: each switch to Bulwark, and each end of a move.
4. **Redeploying** moves Picket, keeps its Health, and starts its lifetime again.
5. **A Designation** lasts a few seconds and only sways Picket's choice among targets it may attack anyway.
6. **All Hands counts damage dealt**, by any delivery, from each allied Vanguard at most once per its cooldown.
7. **A deployed companion outlives its owner's death**, as equipment left standing does, until its time runs out or it is destroyed.
8. **The values** in Abilities.json and Vanguards.json are Provisional playtest settings.

## Consequences

- A deployable is a companion that never walks unless ordered. Later turrets and totems can reuse it.
- Cover is Combat §20's first implementation; full interception, reflection and stationary barriers can extend it.
- Postures give any companion modes without new code.
