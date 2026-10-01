# ADR-028: Blind, grounding, collision stuns and restoring passives, for Silt and Torr

**Status:** Proposed. The author's standing instruction (2026-09-28) is to keep working unreviewed toward a viable game, choosing a provisional answer where canon is silent. §9 lists every such answer for the author to overturn. This ADR becomes Accepted when the author merges the pull request that adds it.
**Date:** 2026-09-30
**Related:**
- [ADR-008](ADR-008-vanguard-definitions-and-ability-composition.md): kits as data, passives as classes.
- [ADR-018](ADR-018-kit-primitives.md): self-buff options, recasts and lingering areas.
- [ADR-026](ADR-026-reactive-kit-primitives.md): Root, reactions and lingering areas that act.
- [ADR-027](ADR-027-mobile-attacks-and-ally-casts.md): the M20b primitives.
- [Roster Bible](../Design/Veyra_Initial_Roster_Character_Bible_v0.6.md): §3 (Silt), §9 (Torr).
- [Combat Bible](../Design/Veyra_Combat_Bible_v0.5.md): §4 (basic attacks), §8 (crowd control), §9 (displacement), §7 (Temporary Health).

## Context

M20 released fifteen Vanguards. The 2026-09-30 survey ranked Silt and Torr next. What their kits need and the engine lacks:

1. **Blind.** Silt's Sandstorm blinds: basic attacks miss. No status stops an attack landing.
2. **An attack that takes a mark to heal.** Reclaim: a basic attack on a Sandy enemy consumes Sandy and heals Silt, scaling with Magic Power. Mimzi's Pocket Hex consumes a mark on attack, but for a bolt, not a heal.
3. **A stun on collision.** Battering Mass pushes enemies and stuns one that hits terrain, a Spire or another Vanguard. Displacement plans its path clear of terrain and passes through units, and reports no collision.
4. **Disrupting dashes in a field.** Anchor's field shortens or disrupts enemy movement abilities crossing it. Root blocks dashes but also walking.
5. **Ripping up a field.** Anchor's recast rips it free for a small knockup, where it stands. A recast opens a follow-up ability, but no area can be placed on a lingering area the caster left.
6. **A knockback as a buff ends.** Overcapacity ends with a venting knockback. A self-buff's end payload gives only a status.
7. **Self-restoration out of combat.** Unreturned restores a share of missing Health once Torr is out of Vanguard combat, and grants benefits as Health falls.

## Decision

### 1. Blind

Combat gains the status kind `Blind`. A blinded unit's basic attacks miss. Each still counts as an attack: it spends its windup and interval, fires the on-attack event and spends any empowerment. It lands nothing: no damage, no on-hit and no secondary impact. Since nothing it adds would land, no attack modifier acts on it; the miss is decided before them, as some act at once. It is crowd control, and Tenacity shortens it. Structures' attacks are never blinded, since they are not basic attacks.

### 2. Grounded

Combat gains the status kind `Grounded`. The unit cannot cast an ability that moves it (a dash, a leap or an attach), and may walk, attack and cast the rest: Root's `Dash` block without its `Move` block. It is crowd control, and Tenacity shortens it. As with Root, a dash already under way finishes (ADR-026 §3).

### 3. Collision statuses

An effect's displacement gains `collisionStatuses`. The displacement stops early, and the unit takes those statuses from the displacing source, when either:
- terrain shortens its path; or
- its body meets another Vanguard or a structure on the way.

Wards, Fluxborn and wildlife do not stop it. The movement component detects the collision and applies the statuses, so Combat keeps the whole rule.

### 4. The Reclaim passive

A `reclaim` passive map names:
- a mark, a status its owner's damaging abilities apply;
- a heal: an amount, an amount per Level and a Magic Power ratio;
- `lockoutSeconds`.

Each of its owner's basic attacks that lands on an enemy Vanguard holding the owner's mark consumes it and heals the owner. It heals once per `lockoutSeconds` per target: Silt's "internal timing rule". The mark itself may be reapplied at any time. A basic attack that misses (§1) consumes nothing. The passive reads the mark as the attack lands, before its damage, so a killing attack still consumes it and heals: death clears the victim's statuses.

### 5. Areas on the caster's lingering area

An area's `origin` gains `CastersLingeringArea`, with `originAbility`. The area lands at the caster's own lingering area of that ability, facing as it did, and that lingering area ends at once without its end effects. The cast is refused if the caster has no such area. Anchor's recast is such an area: its knockup lands where the anchor stands.

### 6. An end payload that pushes

A self-buff's end payload gains `displacement`, at most one, away from its recipient. Each enemy the payload reaches is pushed as well as given the status. Overcapacity's vent is such a payload.

### 7. The Unreturned passive

An `unreturned` passive map names:
- `checkSeconds`;
- `restoreFractionPerSecond`, of missing Health, which it restores while its owner is out of Vanguard combat (Combat State);
- `thresholds`: each a `healthFraction` and `statuses`, which the owner holds, given again at each check, while below that fraction.

Torr's poor ordinary regeneration is plain data: his Health Regeneration stat.

### 8. The two Vanguards

- **Silt** (magic, control mage):
  - Reclaim with the Sandy mark;
  - Scattershot, a delayed area where the sand lands;
  - Sink, a delayed area, stunning in its centre and slowing at its edge;
  - Sandstorm, a lingering area that blinds and coats in Sandy;
  - Buried Alive, a lingering mire that slows and damages, erupts Sandy at its pulses, and ends in a knockup.
- **Torr** (magic, tank):
  - Unreturned;
  - Battering Mass, a frontal slam that pushes and stuns on collision;
  - Fortify, a self-buff with directional damage reduction and slowed movement, ending in a slow around him;
  - Anchor, a lingering field that makes Torr immune to displacement, gives allies displacement resistance and grounds enemies, with a recast that rips it up for a knockup;
  - Overcapacity: body scale, displacement immunity and Temporary Health, ending in a vent that pushes enemies away.

Every number is provisional tuning.

## 9. Provisional answers where canon is open

1. **A blinded attack is still an attack**: on-attack effects fire and an empowered attack is spent, while on-hit effects and damage are lost.
2. **Anchor's "shortened/disrupted" movement abilities are grounding.** Enemies inside the field cannot start a dash; a dash begun outside finishes.
3. **Collision counts terrain, Vanguards and structures only**; being pushed into Fluxborn or wildlife does not stun.
4. **Reclaim's timing rule is a per-target lockout on the heal**, not on the mark.
5. **Overcapacity's Temporary Health does not drain** during the state; it lasts until the state ends. A steady drain waits on a Combat primitive for decaying Temporary Health.
6. **Overcapacity's larger Q and E are variants** held while it lasts (ADR-018 §1), with larger shapes.

## Consequences

- Blind and Grounded are Combat statuses any later kit can use. Grounded also serves Tavi's and Sylra's backlogs.
- Collision statuses serve every later push-into-wall kit.
- The lingering-area origin lets any recast act on the field it left.

## Tests

- **Combat:** Blind makes an attack miss and spend its interval; Grounded blocks a dash cast and not a walk; a push stops at terrain or a Vanguard and stuns.
- **Abilities:** an area on the caster's lingering area ends it; an end payload pushes.
- **Vanguards:** Reclaim and Unreturned; each kit on the archetype test world.
- **Smokes:** a bot match with both.
