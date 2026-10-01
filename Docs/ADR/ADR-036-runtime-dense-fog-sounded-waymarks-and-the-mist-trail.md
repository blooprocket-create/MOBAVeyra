# ADR-036: Runtime Dense Fog, Sounded, Waymarks and the Mist Trail, for Sylra

**Status:** Proposed. The author's standing instruction (2026-09-28) is to keep working unreviewed toward a viable game, choosing a provisional answer where canon is silent. §9 lists every such answer for the author to overturn. This ADR becomes Accepted when the author merges the pull request that adds it.
**Date:** 2026-10-01
**Related:**
- [Initial Roster Character Bible](../Design/Veyra_Initial_Roster_Character_Bible_v0.6.md) §16: Sylra, The Mistwarden.
- [Vision Bible](../Design/Veyra_Vision_Bible_v0.1.md):
  - §2: Dense Fog, and ability-created Dense Fog (ruled 2026-09-23);
  - §4 and §6: presence sensors;
  - fog of war at the data boundary (ruled 2026-09-25).
- [Combat Bible](../Design/Veyra_Combat_Bible_v0.5.md) §7: shields.
- [ADR-003](ADR-003-owned-field-entities.md): owned field entities.
- [ADR-016](ADR-016-vision.md):
  - §2: the visibility contract;
  - §4: runtime fog volumes, which this ADR builds;
  - §5: presence.
- [ADR-018](ADR-018-kit-primitives.md) §5: lingering areas.
- [ADR-035](ADR-035-sea-states-crashing-rides-and-summoned-companions.md) §6: an area ability laid along a path.

## Context

Sylra makes Dense Fog and guides her side through it.
- **Passive, Follow the Bell:** entering Dense Fog, she leaves a short Mist Trail.
  - Allies moving along it gain Movement Speed.
  - An ally who follows it into the same fog volume gains a shield.
  - It grants no vision.
- **Q, Harbor Bell:** a pulse that strikes the first enemy Vanguard, dealing magic damage and Sounding it.
  - Sounded reveals nothing.
  - While a Sounded enemy is inside Dense Fog, the fog pings its presence to Sylra's side.
- **W, Lay the Mist:** true Dense Fog at a point, under every Dense Fog rule.
- **E, Waymark:** a beacon.
  - Outside fog it gives ward-style vision; inside fog it is a presence sensor, which reports at once an enemy already inside its coverage.
  - Allies near it gain a shield that builds while they stay, and rebuilds after a delay once damaged.
- **R, Through the White:** a long corridor of Dense Fog for several seconds. "It is not team invisibility."

The survey for this ADR found that:
- **Vision** keeps Dense Fog as circles, and joins overlapping circles into volumes (`VeyraVisionRules::ConnectVolumes`). ADR-016 §4 named a runtime API for ability fog but did not build it. Clients draw only the authored fog, from the layout.
- **An area's `reveal`** (`IVeyraVisibility::RevealArea`) already gives ordinary vision outside fog, and is a presence sensor over fog, pinging at once and then at the cadence. That is the Waymark's vision.
- **Shields** live in the absorption ledger, granted by source. Nothing tops one up to a cap, or holds back after damage.
- **Laying an area ability along a path** exists for rides (ADR-035 §6), not for a passive.

## Decision

### 1. Runtime Dense Fog (Combat contract; Vision)

- **The contract** (`IVeyraVisibility`) gains two calls:
  - **`AddDenseFog(Shape, DurationSeconds)`**: Dense Fog for a while. A shape is a circle, or a corridor (a start, a direction, a length and a width).
  - **`FogVolumeAt(Point)`**: the fog volume holding a point, or none.

  In a world without vision, fog does nothing and no point is in fog.
- **Vision builds it as ADR-016 §4 named.** This amends ADR-016 §4, which named the call `AddFogVolume(centre, radius, lifetime)`:
  - A corridor becomes overlapping circles, of half its width, spaced along its length.
  - Each cast's circles are one **fog bank**, a replicated actor every player receives. The fog itself is always seen (Vision Bible §2), so the fog gate lets it through.
  - Vision's fog is the authored circles plus every live bank's. Volumes are reconnected as a bank comes and as it goes. Overlapping fog is one volume, and an expiry splits it at once.
  - Concealment, the same-volume rule, wards and sensors apply to it unchanged: it is the same construct (Vision Bible §2).
- **The grey box** draws a live bank as it draws authored fog.

### 2. Sounded (Combat; Vision)

- **A status kind, `Sounded`:** it blocks nothing and changes no stat.
- **Vision reads it:** while an enemy Vanguard holds Sounded from a source of side S and stands inside Dense Fog, S receives a presence ping for the fog circle it is in, at the presence cadence.
- It reveals nothing, outlines nothing and grants no targeting (Vision Bible §5's channels).

### 3. Areas that lay fog (Abilities)

- **An area ability gains `fog`**, at most one, beside its `reveal`:
  - a shape: `Circle` with its radius, or `Corridor` with its length and width;
  - `durationSeconds`.
- It asks the contract for the fog as the cast commits, where the area lands. A corridor starts there and runs along the cast's direction.
- An area may lay fog with no zones, as an area may reveal with none.
- **Lay the Mist** is a circle at the target point. **Through the White** is a corridor from Sylra along her aim.

### 4. Shields that build while an ally stays (Abilities)

- **A lingering area gains `shieldTopUp`**, at most one, for its caster and the allied Vanguards inside, on each pulse:
  - `amount` per pulse, toward a `cap`, by rank and with a Magic Power ratio, as other shields are;
  - `delayAfterDamageSeconds`: an ally damaged within that time gets no top-up from this pulse.
- The shield is the caster's grant in the absorption ledger, so damage spends it as any shield. A top-up raises the caster's existing grant to that ally, never past the cap.
- **Waymark** is an area ability at a point:
  - its `reveal` lights its area for its lifetime, which over fog is a presence sensor, pinging at once for an enemy already inside;
  - its lingering area holds the shield top-up.
### 5. The Mist Trail (Abilities)

- **The `mistTrail` passive map:**
  - when its holder steps from no fog into a fog volume, it lays an area ability behind her every `spacing` units for `laySeconds`, at her rank, as ADR-035 §6 lays a trail;
  - the trail's ally statuses include a `followStatus` from her;
  - an allied Vanguard holding her follow status who stands in the volume she entered gains `followShield` from her, once per trail.
- The trail's area grants no vision.

### 6. Sylra (Vanguards)

- **Her kit is data:**
  - Harbor Bell, a skillshot striking the first enemy Vanguard;
  - Lay the Mist and Through the White, areas that lay fog;
  - Waymark, an area that reveals and lingers;
  - Follow the Bell, the Mist Trail;
  - her statuses and text.
- **Bots:**
  - Q, damage;
  - W, an escape at herself, or an engage at the enemy;
  - E, a defend at herself;
  - R, an engage.

### 7. Presentation

- A fog bank is drawn as authored fog is.
- A Waymark is drawn as a lingering area.
- Sounded shows as a status on its holder to those who see it; its pings are drawn as other presence pings.

### 8. Vision and data

- Runtime fog's numbers are the casts' data.
- The fog bank's replication is a Vision.json value, as other Vision actors' are.

### 9. Provisional answers where canon is open

1. **A corridor** is overlapping circles; the fog's edge is their union.
2. **Runtime fog has no side:** it hides either side's Vanguards, as authored fog does.
3. **Sounded** pings at the ordinary presence cadence, only while its holder is in fog.
4. **The Waymark** is a lingering area: seen by both sides, never destroyed, lasting its lifetime.
5. **The Waymark's shield** builds only while the ally stays in its radius, and only after `delayAfterDamageSeconds` without damage.
6. **The Mist Trail's shield** is given once per trail to each ally.
7. **"Entering" fog** means stepping from no fog into a volume; moving within fog, or from one volume straight into a touching one, does not count.
8. **The values** in Abilities.json and Vanguards.json are Provisional playtest settings.

## Consequences

- Fog becomes something abilities make. Any later fog-maker uses the same two calls.
- Sounded is a reusable information status: it marks without revealing.
- A shield that builds while an ally holds ground can serve later beacons and zones.
