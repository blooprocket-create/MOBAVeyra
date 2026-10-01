# ADR-032: Movement punishment, shield-held statuses and temporary walls, for Varkesh

**Status:** Proposed. The author's standing instruction (2026-09-28) is to keep working unreviewed toward a viable game, choosing a provisional answer where canon is silent. §8 lists every such answer for the author to overturn. This ADR becomes Accepted when the author merges the pull request that adds it.
**Date:** 2026-10-01
**Related:**
- [Initial Roster Character Bible](../Design/Veyra_Initial_Roster_Character_Bible_v0.6.md) §14: Varkesh, The Forgeheart.
- [Combat Bible](../Design/Veyra_Combat_Bible_v0.5.md):
  - §7: shields;
  - §9: dashes, blinks and terrain;
  - §16: On Ability Hit.
- [ADR-003](ADR-003-owned-field-entities.md): owned field entities, of which a wall is one.
- [ADR-018](ADR-018-kit-primitives.md): recast windows.
- [ADR-028](ADR-028-blind-grounding-and-collision.md): an area at the caster's lingering area.
- [ADR-030](ADR-030-stealth-markers-and-marked-follow-ups.md): placed markers.
- [ADR-031](ADR-031-stances-focus-shadows-and-cross-marks.md): placements, and follow-ups that end with their marker.

## Context

Varkesh is a medium-range magic battlemage who punishes movement.
- **Passive, Stress Temper:** his damaging abilities coat enemy Vanguards in **Heated Metal**. A coated enemy that dashes or blinks is struck where it lands: magic damage and a brief root. The coating is spent, with a per-target cooldown.
- **Q, Slagshot:** an area at a point.
- **W, Tempered Shell:**
  - a shield, with resistance to slows and control while it holds;
  - a burst of heat as it breaks or runs out.
- **E, Molten Ground:** a strip of molten ground that slows and coats.
- **R, Forge Divide:**
  - A rolling wave pushes enemies away from its centre line and coats them.
  - At the end of its path it cools into a temporary black-iron wall that blocks both teams.
- **R recast, Shatterforge:** after an arming delay, it detonates the wall.

Not yet possible:
- seeing a unit's own dash or blink end;
- statuses that last only while a shield holds;
- a burst when a shield ends;
- a wall in the world;
- a follow-up that arms after a delay;
- an area at the caster's marker.

## Decision

### 1. A unit's own movement, announced (Combat)

Combat announces **OnUnitMoved** when a unit's own movement ends: a dash that arrives, stops at an enemy or is replaced, and every blink.
- Each announcement gives the unit, the kind, and where the move began and ended.
- A displacement is not the unit's own movement, so it is not announced, and neither is an interrupted dash.

### 2. The stress temper passive (Vanguards)

**The `stressTemper` passive map** names a coating status, the strike's effects, and a lockout status.
- **Coating:** each hit of one of its owner's damaging abilities on an enemy Vanguard puts the coating on it (On Ability Hit, §16).
- **Strike:** when a unit holding the coating from the owner moves on its own (§1), and holds no lockout from the owner:
  1. the coating is spent;
  2. the strike's effects land on the unit where it ended;
  3. the unit takes the lockout.

### 3. Statuses a shield holds, and a burst as it ends (Abilities)

A self-buff may name:
- **`shieldHolds`:** statuses its recipient keeps while the buff's shield holds. They are removed as the shield breaks or runs out.
- **`shieldEndZones`:** zones that land around the recipient at that moment.

Both follow the shield's own effect, which Combat removes when it empties or expires.

### 4. Walls (Combat, Abilities)

**A placed marker may be a wall**, with a length and a thickness.
- Its body is world-static and blocks units of both teams, as terrain does. So dashes stop at it, blinks land beside it, and line projectiles end at it (Combat Bible §9).
- It cuts the navigation mesh as a dynamic obstacle, so paths go round it.
- No one can target or destroy it; it ends with its time, or when its owner's ability ends it.

**A skillshot may name an `endWall`:** where its projectile's flight ends, at its range or at terrain, it leaves a wall across its path.

### 5. Follow-ups that arm, and end with their marker (Abilities)

- **Arming:** a recast window may name `armingSeconds`. The follow-up then opens that long after the cast commits, rather than at once.
- **Ending with the marker:** a follow-up ends when the marker of the ability that opened it ends. The rule now lives in one place, a follow-up subsystem, instead of in the placement archetype (ADR-031 §4). It covers placements, walls and self-buff markers alike.

### 6. An area at the caster's marker (Abilities)

**A new area origin, `CastersMarker`,** puts an area on the caster's standing marker of the ability `originAbility` names, facing as the marker does.
- That marker ends at once, as a lingering area ends under `CastersLingeringArea` (ADR-028 §5).
- With no such marker standing, the cast is refused.

### 7. Varkesh's kit

| Slot | Ability | Built from |
|---|---|---|
| Passive | **Stress Temper** | `stressTemper` (§2) |
| Q | **Slagshot** | an area at a point |
| W | **Tempered Shell** | a self-buff shield with `shieldHolds` (Tenacity and Slow Resistance) and `shieldEndZones` (§3) |
| E | **Molten Ground** | a lingering rectangle at a point, whose pulses slow and coat |
| R | **Forge Divide** | a piercing skillshot that pushes enemies aside from its path, with an `endWall` (§4) and an armed recast (§5) |
| R recast | **Shatterforge** | an area at the caster's marker (§6), which ends the wall |

## 8. Provisional answers where canon is open

1. **Heated Metal** lasts 4 s. The strike deals 40 + 12 per level magic damage, plus 30% of Magic Power, and roots for 0.75 s. Each target's lockout lasts 6 s.
2. **Which moves count:**
   - dashes that arrive, stop at an enemy or are replaced;
   - blinks, including a swap with one's own marker.
   - Displacements and interrupted dashes do not count.
3. **Coating:** the passive coats with every damaging ability's hit on an enemy Vanguard. The abilities need not name the coating themselves. Molten Ground deals no damage, so its pulses apply the coating.
4. **Tempered Shell:**
   - the shield lasts 3 s;
   - while it holds, 30% Tenacity and 40% Slow Resistance;
   - the burst has a 300 radius, for 40–120 magic damage plus 20% of Magic Power.
5. **Molten Ground:** a 650 × 220 strip that lasts 4 s, pulsing every 0.5 s with a 25% slow.
6. **Forge Divide:**
   - the wave is 300 wide and travels 1000;
   - it pushes enemies 100 aside from its path;
   - the wall is 600 long and 80 thick, and stands for 4 s.
7. **Shatterforge:** arms after 1 s. It explodes in a 400 radius around the wall, for 150–300 magic damage plus 50% of Magic Power, with a 40% slow for 1.5 s.
8. **The wall** blocks units, dashes and line projectiles of both teams, but not sight. Canon calls it a solid wall; whether walls block vision is open in the Vision Bible.

## Consequences

- Vanguards.json gains the `stressTemper` map.
- Abilities.json:
  - self-buffs gain `shieldHolds` and `shieldEndZones`;
  - skillshots gain `endWall`;
  - recast windows gain `armingSeconds`;
  - areas gain the `CastersMarker` origin.
- `UVeyraPlacementAbility` drops its own marker watch for the shared follow-up rule (§5).
- A wall is the first marker that blocks movement. Later terrain-making kits reuse it.

## Tests

- **OnUnitMoved:** announced for a dash that arrives and for a blink, not for a displacement.
- **Stress Temper:**
  - a coated enemy that dashes is struck and rooted where it lands;
  - the lockout blocks a second strike;
  - an uncoated dash is spared.
- **Shield holds:** the statuses go and the burst lands as the shield breaks, and as it runs out.
- **Walls:**
  - a skillshot leaves a wall at its end;
  - the wall blocks a unit's dash;
  - it ends with its time.
- **Arming:** the follow-up opens only after the arming delay, and ends with its wall.
- **CastersMarker:** the area lands at the wall and ends it, and the cast is refused with no wall.
- **Varkesh from the committed tuning:** his kit casts, and Shatterforge detonates the wall Forge Divide left.
- **Packaged smokes:** Practice, CasualVictory, and `-Vanguards varkesh,torr`.
