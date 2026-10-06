# ADR-043: Map walls, and terrain that blocks sight

**Status:** Proposed. The author's standing instruction (2026-09-28) is to keep working unreviewed toward a viable game, choosing a provisional answer where canon is silent. §7 lists every such answer for the author to overturn. This ADR becomes Accepted when the author merges the pull request that adds it.
**Date:** 2026-10-01
**Related:**
- [Battleground Bible](../Design/Veyra_Battleground_Bible_v0.9.md) §2 (map topology and macro shape, ability-created terrain), §7 (jungle geometry), §16 (guardrails).
- [Vision Bible](../Design/Veyra_Vision_Bible_v0.1.md): sight and Dense Fog. It is silent on terrain.
- [ADR-011](ADR-011-battleground-runtime.md): the layout, its mirror and the battleground's setup.
- [ADR-016](ADR-016-vision.md): its open item "Terrain line of sight", which this record closes.
- [ADR-032](ADR-032-movement-punishment-shield-holds-and-walls.md) §8.8: a wall an ability raises blocks units, dashes and line projectiles, not sight. That stands.

## Context

The battleground is a flat floor. Its lanes, river, base pads and Dense Fog are markings that every machine draws from the layout. Nothing on it is terrain: every route is open, and there are no chokepoints, jungle paths, gank entrances or base walls.

The Battleground Bible sets the macro shape: three lanes, bases in opposite corners, a river on the other diagonal, and inner and outer jungle. It leaves walls and routes to Veyra: "Walls, jungle routes and fog placement stay Veyra's own."

Runtime terrain already exists for Varkesh's Forge Divide (ADR-032):
- a replicated wall box on the world-static channel;
- a navigation obstacle on a navmesh generated at runtime;
- terrain that stops dashes, displacement and line projectiles, and moves out units caught inside.

Every mover already paths on that navmesh. Vision has no line-of-sight test.

## Author clarification (2026-10-02)

The existing greybox wall shapes and layout are not approved canon. They are prototype geometry, not an artistic or level-design target for the production Crucible. Terrain sight blocking and fog of war remain required gameplay capabilities. The production terrain pass must validate both sides for fairness and keep the distinction between sight-blocking terrain and gameplay Dense Fog explicit.

## Decision

### 1. Walls are layout data (World: layout)

- **`layout.walls` lists oriented boxes:** `{center, facing, length, thickness}`, in Team A's half.
  - Team B's are their mirror across y = −x, as every layout feature's is. (Amended 2026-10-05 by [ADR-040](ADR-040-crucible-world-authoring-toolchain.md): their rotation half a turn about the centre, facing turned too.)
  - The grey-box geometry is Veyra's own and Provisional.
- **Validation:**
  - Each wall lies on the floor and wholly in Team A's half.
  - Each wall keeps `wallClearance` (data) from:
    - every lane strip;
    - every camp's spawn ring;
    - every Flux Well's radius;
    - every structure's body;
    - the fountain and the starts.

  Straight-line data (lane waypoints, camp centres, hold points) therefore never sits in a wall.

### 2. They are terrain (World: terrain)

- **The battleground raises the layout's walls as permanent terrain when it sets up,** through the terrain subsystem that raises Forge Divide.
- So they are the same kind of thing:
  - **for the server,** a navigation obstacle;
  - **for clients,** a replicated body their predicted movement collides with;
  - **for everyone,** a stop for dashes, displacement and line projectiles.
- Blinks cross them, as they cross Forge Divide.
- They never move, change or end; their one replicated property is push-based, so after their first replication they send nothing more.
- The map asset is not rebaked: walls are data, and every test battleground built from a layout has them.

### 3. Map walls block sight (Vision)

- **A unit is seen by a sight source only if the segment between them crosses no map wall.** This applies to Vanguards, Fluxborn, structures, wards and companions alike.
- **The same test applies:**
  - to a ward's presence sensing;
  - to sight inside Dense Fog.
- **Ability walls still do not block sight** (ADR-032 §8.8).
- **Vision learns the walls from Match,** as it learns Dense Fog. Vision and World are peers in the Battleground layer.
- **The test is a pure 2D rule:** a segment against an oriented box. Candidate walls are pre-filtered on a grid.

### 4. Presentation

- The greybox already draws terrain boxes.
- The minimap draws the walls and the river too.

### 5. Tests

- Layout validation of walls.
- On the runtime battleground: routes along every lane, from each fountain to each lane, and to every camp and Well, with the walls up.
- Walls replicate to clients.
- A wave paths around a wall.
- The sight rule: a unit behind a wall unseen and around it seen, for each kind of source.
- The minimap model.
- The smokes: Practice, and eight playing bots on the real layout.

### 6. Performance

- Sight is checked every update, for every source and target pair in range, against the walls the grid offers.
- The eight-bot smoke records the server's frame time.

### 7. Provisional answers where canon is open

1. **Walls block sight.** This is the familiar MOBA rule, and the Vision Bible is silent on it.
2. **Ability-created walls do not block sight** (ADR-032 §8.8 unchanged).
3. **The grey-box geometry:**
   - Base walls with a mouth for each lane.
   - Inner-jungle blocks with camp pockets and river entrances.
   - Outer-jungle banks along top and bot with gank gaps.

   Team A's half is symmetric across the mid diagonal, and the mirror gives Team B's.
4. **`wallClearance`.**

## Consequences

- The map gains routes, chokepoints and gank entrances, and line of sight becomes part of play.
- Every test battleground built from a layout carries its walls.
- The compact test layout has none unless a test adds them.

## Amendments to earlier records

- **ADR-016:** its open item "Terrain line of sight" is decided here.
- **ADR-011:** the layout gains `walls` and `wallClearance`.

## Amendment (2026-10-05): natural walls

The author ruled on 2026-10-05 that the walls must be "natural shaped walls that aren't all the same size, that flow well and provide actual competitive usage": impassable terrain as it would occur in a jungle, not placed rectangles. §1 changes as follows. §2–§6 keep working unchanged on the boxes a wall derives.

- **A wall is a ridge along a curve.** `layout.walls` lists, for Team A's half, `{name, points}`. Each point is `{x, y, width}`. The spine is a Catmull-Rom curve through the points, sampled `layout.wallSamplesPerSegment` times a span. Its thickness changes linearly between points, and its ends are rounded. Walls may overlap, so one massif can bend, branch and swell. Team B's walls are the rotation. `name` is for designers only.
- **One curve, two readings** (`FVeyraWallShape`, VeyraWorld):
  - The ridge is the band's rounded signed distance (`VeyraWidthCurve`, which the river's channels share). The terrain field raises the cliff within it, and the dressing follows it.
  - What blocks and hides is a chain of boxes (`Boxes`), one per sample span. Each is as thick as the span's thicker end, mitred where the spine turns so the outside of every bend stays closed, and squared past each tip so the rounded end stands inside.
  - Collision, navigation obstacles, sight blocking and the minimap consume the boxes through `VeyraLayout::Walls`, as before. A test checks that every point inside a ridge lies in some box, and that no box corner reaches further past the ridge than a square end does.
- **Validation.** It refuses a wall with fewer than two distinct points or a width that isn't above 0. It also refuses a spine that turns a right angle or more between samples, where the mitres would no longer close. Every box keeps the existing clearances from lanes, camps, Wells, structures, the fountain, Dense Fog, the river and the dividing line.
- **The design.** Each of Team A's two jungle quadrants is built from:
  - lane-side massifs broken by three gates, with brush in the gate mouths;
  - a central massif with lobes, splitting a lane corridor from a mid corridor;
  - a pocketed inner den;
  - a mid-side ridge with gank gaps;
  - outcrops in the outer strips.

  Widths run from 240 at Blink-able tips (Blink's range is 400) to 980. The skittermaw camp moved inward into a den, and three Dense Fog circles moved to sit in the gate mouths. All of it is provisional and checked by `ValidateBattleground.ps1`: equivalent routes for both teams differ by at most 1 unit.
- **The look.** The ridge crest is 620 high on 260 jungle ground, low enough that the gameplay camera sees over it. Slate cliff faces stand along both edges and close round the ends, the terrain's moss shows between them, boulders lie at the foot, and shrubs and small trees grow on the crown where it is broad. Nothing is placed where walls overlap.
- **Known gap.** A tip's square collision corner reaches up to (√2 − 1) × half the tip's width past the rounded cliff: about 50 units at the narrowest tips.
