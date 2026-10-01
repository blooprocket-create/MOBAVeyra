# ADR-033: Charge, attack-spent statuses, movement fields and power grids, for Relay

**Status:** Proposed. The author's standing instruction (2026-09-28) is to keep working unreviewed toward a viable game, choosing a provisional answer where canon is silent. §9 lists every such answer for the author to overturn. This ADR becomes Accepted when the author merges the pull request that adds it.
**Date:** 2026-10-01
**Related:**
- [Initial Roster Character Bible](../Design/Veyra_Initial_Roster_Character_Bible_v0.6.md) §4: Relay, The Last Conductor.
- [Combat Bible](../Design/Veyra_Combat_Bible_v0.5.md):
  - §9: forced movement;
  - §17: empowered attacks;
  - §27: costs, including a share of the current resource.
- [ADR-027](ADR-027-mobile-attacks-and-ally-casts.md): self-buffs cast on an ally.
- [ADR-031](ADR-031-stances-focus-shadows-and-cross-marks.md): resource families (Focus).

## Context

Relay is a utility support built around a battery.
- **Passive, Charger:** nearby Fluxborn deaths, allied or enemy, generate **Charge**. Maximum Charge grows with level. Jungle wildlife gives none.
- **Q, Rapid Discharge:** on himself or an ally. It spends a share of current Charge for a number of **Overclocked Attacks**: more Attack Speed until those attacks are used.
- **W, Overcharge:** for a while, Fluxborn deaths give more Charge.
- **E, Magnetic Field:**
  - a field where allies resist slows;
  - enemy dashes and displacements inside it bend toward its centre.
- **R, Full Grid:**
  - Relay anchors while his Charge drains.
  - Connected allies' basic attacks shorten the cooldown of their next basic ability.
  - Allied Fluxborn inside are overclocked.
  - Rapid Discharge costs less meanwhile.

Not yet possible:
- a resource that starts empty and never refills by itself;
- a cost that is a share of the current resource;
- statuses spent by attacks;
- movement fields that bend forced moves;
- auras on allied Fluxborn;
- statuses that shorten a cooldown with each attack;
- self-buffs that drain their caster's resource.

## Decision

### 1. Charge (Vanguards, Combat)

**`Charge` is a new resource family.** A Charge unit starts with none and never regenerates.
- Its definition's regeneration is 0: validated.
- Respawning does not refill it; Combat keeps a unit's resource refill policy, and Charge's is **Kept**.
- Only effects restore it, as the charger passive does.
- Its maximum grows with level through the ordinary growth.
- The HUD shows it in its own colour.

### 2. The charger passive (Vanguards)

**The `charger` map** names:
- a radius;
- Charge per Fluxborn death;
- a boost status (Overcharge's) and the multiplier the Charge given takes while its owner holds it.

Any Fluxborn's death within the radius of its living owner gives Charge, whatever its side. A death of wildlife, a Vanguard or a structure gives none.

### 3. A share of the current resource (Abilities)

**A cast may name `currentResourceFraction`:** it then costs that share of its caster's current resource, paid at Commit, as well as its ordinary cost by rank.
- **`minimumResource`** refuses the cast while the caster holds less.
- **The ResourceCostReduction status kind** lowers its holder's costs by its magnitude.

### 4. Statuses spent by attacks (Combat, Abilities)

**A status may name `attackCharges`:** each of its holder's basic attacks that commits spends one, and the status ends with the last. The basic attack component tells Combat each Commit.

### 5. Movement fields (Combat, Abilities)

**A lingering area may name a `movementField`:** while it stands, a forced move of an enemy of its caster's bends toward its centre, by up to `pull` units.
- This covers a dash or a displacement that starts inside the field or crosses it.
- The bend is applied to the move's end as the move is planned; terrain then shortens the bent path as ever.
- Combat keeps the fields, so the movement component reads them without knowing Abilities.

### 6. Grids (Abilities)

- **Auras reach allied Fluxborn too:** a self-buff's aura may name `allyFluxbornStatuses`.
- **The AttackShortensCooldown status kind:** each of its holder's committed basic attacks shortens, by the status's magnitude in seconds, the remaining cooldown of whichever of its Q, W and E comes off cooldown soonest ("the next basic ability").
- **A self-buff may name `drain`:**
  - its caster loses that much resource each second while the buff lasts;
  - the buff ends early when the resource runs out.

### 7. Relay's kit

| Slot | Ability | Built from |
|---|---|---|
| Passive | **Charger** | `charger` (§2) |
| Q | **Rapid Discharge** | a self-buff cast on Relay or an ally, costing a share of current Charge (§3), giving an attack-spent Attack Speed status (§4) |
| W | **Overcharge** | a self-buff giving the charger's boost status |
| E | **Magnetic Field** | a lingering area at a point, with ally Slow Resistance and a movement field (§5) |
| R | **Full Grid** | a self-buff that plants Relay and drains Charge, with an aura of attack cooldown cuts for allied Vanguards, overclocking for allied Fluxborn, and a cost reduction for Relay (§6) |

## 8. Consequences

- Vanguards.json gains the `Charge` family and the `charger` map.
- Abilities.json gains:
  - `currentResourceFraction` and `minimumResource` on casts;
  - `attackCharges` on statuses;
  - `movementField` on lingering areas;
  - `allyFluxbornStatuses` on auras;
  - `drain` on self-buffs;
  - the status kinds ResourceCostReduction and AttackShortensCooldown.

## 9. Provisional answers where canon is open

1. **Charge:**
   - maximum 100 at Level 1, plus 10 each Level;
   - 6 Charge per Fluxborn death within 1100;
   - kept through death.
2. **Rapid Discharge:**
   - costs 50% of current Charge, and needs at least 20;
   - gives 4 Overclocked Attacks of 40/50/60/70/80% Attack Speed, for at most 6 s.
3. **Overcharge:** triples Charge from deaths for 8 s.
4. **Magnetic Field:**
   - a 350-radius field for 4 s;
   - allies inside gain 35% Slow Resistance;
   - enemy forced moves bend up to 150 toward its centre.
5. **Full Grid:**
   - drains 12 Charge a second, for at most 8 s, with Relay Planted;
   - allied Vanguards within 800: each basic attack takes 0.5/0.75/1 s off their next basic ability;
   - allied Fluxborn: 30% Attack Speed and 15% Move Speed;
   - Relay's costs fall by 50%.
