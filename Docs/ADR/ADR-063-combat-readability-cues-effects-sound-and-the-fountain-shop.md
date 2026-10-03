# ADR-063: Combat readability: cues, body feedback, outlines, effects, sound and the fountain shop

**Status:** Proposed. The author's standing instruction (2026-09-28) is to keep working unreviewed toward a viable game, choosing a provisional answer where canon is silent. §9 lists every such answer for the author to overturn. This ADR becomes Accepted when the author merges the pull request that adds it.
**Date:** 2026-10-03
**Related:**
- [Asset & VFX Pipeline](../Production/VEYRA_ASSET_AND_VFX_PIPELINE.md) §11–§12: Niagara is the runtime effect system; gameplay reports what happened and presentation resolves how it looks; the exact effect-event API is open (§25), and this record decides it.
- [Settings & Accessibility Bible](../Design/Veyra_Settings_Accessibility_Bible_v0.1.md):
  - §3.3: optional click markers; high-contrast outlines;
  - §4.1: ally, enemy and neutral colours for outlines;
  - §4.2: Reduce Flashing, reduced particles; danger zones, projectiles and CC stay readable;
  - §4.3: independent gameplay-effect volume.
- [ADR-001](ADR-001-unreal-version-policy.md): no Experimental engine feature as a shipping dependency.
- [ADR-008](ADR-008-vanguard-definitions-and-ability-composition.md) §1: the grey-box presentation.
- [ADR-016](ADR-016-vision.md) §3: the fog gate on replicated state.
- [ADR-062](ADR-062-side-based-unit-collision-local-avoidance-and-hosted-diagnostics.md) §6: the click mark.

## Context
The first outside playtest (2026-10-03) found combat unreadable: with no animation, players couldn't tell when an attack or an ability fired or landed. Nobody found the shop without being told.

The code:
- **No client events.** Nothing is multicast. A client knows a fight only from replicated state:
  - each attacker's attack phase (windup, backswing), target and phase end;
  - each unit's Health;
  - each caster's cast state (ability, phase, aim).
- **Missing moments:** a cast that commits with no channel or recovery looks like a cancelled windup, and one with no windup never shows at all. A hit a shield absorbs leaves Health unchanged.
- **The presentation:** cylinders in side colours, spheres for projectiles, and line outlines for telegraphs. There is no motion, flash, outline, particle or sound.
- **The shop** opens only from its key. Nothing stands at the fountain.

## Decision

### 1. Cues come from replicated state
- **The cue owner:** a client-only `UVeyraCombatCueSubsystem` (VeyraUI) watches the units this client sees. It raises typed cues for the presentation's views:
  - attack windup and attack commit;
  - hit (a drop in Health plus shields);
  - cast windup and cast commit;
  - death.
- **Why state, not new messages:** the fog gate already filters this state per player (ADR-016 §3), so a cue can never show what a player isn't entitled to see. It also costs no new network traffic, and no gameplay class knows the cues exist.
- **First sight is not an event:** a unit coming out of the fog records its state without raising cues.
- **The one gameplay addition:** the cast state gains a committed-cast record (a serial number, the ability, its aim). Every commit then replicates, including casts with no windup.
- **Projectiles** already replicate and are drawn from their launch data. Their trail and impact are drawn from the projectile and the hit cue.

### 2. Bodies show what happens to them
- **A generated unit material** replaces the engine's basic shape material for bodies: side colour, a rim light, and an emissive hit flash.
- **Motion:** during a windup the body leans toward its target, it snaps forward at the commit, it recoils from a hit, and it collapses at death.
- **Presentation only:** these are offsets of the drawn mesh. The capsule, collision and movement never change.
- **Accessibility:** Reduce Flashing turns the hit flash into a steady tint that fades (§4.2).

### 3. Hover outlines
- **The outline:** the unit under the cursor is outlined in its side's colour from the player's colour vision (§4.1). Allies, enemies and neutral units differ by colour, and enemies also by a thicker line.
- **The technique:** custom-depth stencil plus a post-process outline material on the local camera.
- **The cursor** changes to the attack cursor over an enemy the player can attack.

### 4. Effects
- **Niagara is enabled.** A few systems, parameterised by colour, scale and count, cover the cues: impact burst, cast flash, death burst, projectile trail and a shop beacon.
- **A generator builds them:** a commandlet in VeyraDeveloper's editor build, as the map and art commandlets are. It is deterministic and seeded, so the systems are regenerated rather than hand-edited.
- **ADR-001: a narrow approval.** The generator uses the engine's Niagara editing utilities, which Epic marks Experimental.
  - They run only at authoring time, in the editor. The systems they write are ordinary Niagara assets, and no client or server build loads the utilities.
  - This record approves them for this generator only. Any other use of an Experimental feature needs its own decision.
  - If an engine update breaks them, the generated systems remain valid and only the generator is repaired. Nothing at runtime changes.
  - If they can't build an effect the cues need, that effect falls back to meshes with dynamic materials.
- **Reduced particles** (§4.2) lowers counts. It never removes what a projectile, a danger zone or crowd control needs to read.

### 5. Sound
- **A seeded generator** synthesises the cue sounds: swings, impacts, casts, deaths and a click. Unreal imports them as sound assets.
- **Playback:** the cue subsystem plays them at their location, so nothing is heard that isn't seen (§4.3).
- **Volume:** a gameplay-effect volume joins the Audio settings.

### 6. The fountain shop
- **A shop stands at each side's fountain:** a structure drawn by the presentation at the team start with a presentation offset. It needs no layout change.
- **Its look:** a mesh from the structure-kit generator, a Niagara beacon and a label.
- **Use:** hovering outlines it, and clicking it opens the shop screen, as the shop key does. Buying stays fountain-only on the server.

### 7. Ownership
- **Gameplay** owns the state cues are read from, and the committed-cast record.
- **VeyraUI** owns cues, body feedback, outlines, effects and sound.
- **The generators** own the assets: materials and sounds through Unreal Python, Niagara through the commandlet, meshes through Blender. Each records its version and seed, and its output is regenerated rather than hand-fixed (Asset & VFX Pipeline §4).

### 8. Not in scope
Skeletal characters and animation, the map's lighting and environment, and every gameplay rule.

### 9. Provisional answers where canon is open
1. Cues are inferred from replicated state; no gameplay event channel is added (§1).
2. The committed-cast record is the one replicated addition.
3. The hit flash, lean, snap, recoil and collapse, and their timings, are provisional presentation.
4. Enemies are outlined thicker than allies.
5. The cue sounds are synthesised placeholders, to be replaced by authored audio.
6. The shop is a presentation structure at the team start, not a layout entry.
7. The Niagara editing utilities are approved for the effect generator only (§4).
