# ADR-068: Toon characters and graphic combat effects in a painterly world

**Status:** Proposed. The author's standing instruction (2026-09-28) is to keep working unreviewed toward a viable game, choosing a provisional answer where canon is silent. The closing section lists every such answer for the author to overturn. This ADR becomes Accepted when the author merges the pull request that adds it.
**Date:** 2026-10-06
**Related:**
- [Art Direction](../Design/Art_Direction_v0.1.md): "painterly-realistic", "physically believable materials", per-character faces. This record amends it for characters and combat effects.
- [ADR-063](ADR-063-combat-readability-cues-effects-sound-and-the-fountain-shop.md): cues, effects, the hit flash and outlines from replicated state; generated Niagara.
- [ADR-064](ADR-064-generated-animated-vanguards.md): generated bodies, the kit, and the generator owning its output.
- [ADR-040](ADR-040-crucible-world-authoring-toolchain.md): the frame budget and the world's painterly production.
- [ADR-001](ADR-001-unreal-version-policy.md): Experimental engine features need their own decision.

## Context
After 0.2.2 the author asked for the next pass to be animation and combat effects:
> "we're going for a stylized game, anime leaning, but... not anime(toon?). remember we're using unreal engine 5.8 we can do some amazing things if we do it right. give this a AAA indie game release level first pass."

The written canon says "painterly-realistic everything else" with "physically believable materials". That conflicts with a toon look, so the author was asked to choose (2026-10-06): painterly-stylized, cel-shaded toon, or a hybrid. **They chose the hybrid:** a painterly world, with toon characters and combat effects.

What exists:
- Every Vanguard body wears one generated, default-lit material coloured by its vertices.
- No outline, except the hover outline.
- Four generic cue effects and a projectile trail, drawn from one ragged-puff look.
- Telegraphs are lines.
- Animation is procedural, with one cast clip for every ability.
- ADR-040 measured a 99th-percentile frame of 8.60 ms against the 8.33 ms target. Every addition here must pay for itself.

## Decision

### 1. Art direction: a painterly world, toon characters and effects
**Author ruling (2026-10-06).** It amends Art Direction v0.1 for characters and combat effects.
- **The world stays painterly-realistic.** Landscape, ruins, water and dressing keep their canon and their production.
- **Characters are toon:** Vanguards, companions, Echoes, Fluxborn and creatures.
  - Banded light and shadow with a soft edge, a cool painted shadow tint, and a rim of light.
  - A thin ink outline, darker than the local colour.
  - Anime-leaning faces and proportions; "not anime".
- **Combat effects are graphic:** hard-edged shapes, white-hot cores and bold side colours. The shape language is bursts, rings, slashes and streaks, readable at the gameplay camera.
- **Presentation never decides anything.** As ADR-063 holds, nothing the server decides reads a drawn effect, and an effect never shows a hit before its commit (Combat Bible §48).

### 2. Toon character shading
- **The material.** The generated body material `M_VeyraVanguardBody` (ADR-064's kit, `bodyMaterial`) becomes a toon material, built by `ImportVanguardBodies.py`:
  - **Unlit, with its own light.** The sun's direction and colour come from the material parameter collection `MPC_VeyraToonLight` (`ToSun`, `SunColor`), generated with the bodies. The presentation sets both from the map's brightest directional light each frame; the collection's defaults, the kit's `toSun` and `sunColor`, light a world without one.
  - **Two bands with a soft edge:** the lit side is the vertex colour × `litTint` × the sun's colour; the side away is the vertex colour × `shadowTint`, a cool painted shadow. They meet where the surface turns from the sun past `bandThreshold`, over `bandSoftness`.
  - **A rim of the sun's light** along the lit side's silhouette: a fresnel of `rimExponent`, from `rimStart` to `rimEnd`, at `rimStrength`.
  - **Exposure.** The whole is `brightness` × what the scene's exposure maps to white (`unexposed`), so a body reads the same under the Crucible's physical sun as in a preview. The kit's glow (`glowGain`) lies on top, as before.
- **Its numbers are kit data**, checked by `KitMaterials/spec.py` in CI.
- **Shadows.** Bodies still cast shadows onto the world; they do not take the world's shadows on themselves.
- **The hit flash** stays the overlay material (ADR-063 §2): the ink below needs no overlay.

### 3. Ink outlines
A post-process pass, `PP_VeyraToonInk`, draws the characters' ink. `BuildPresentationMaterials.py` generates it from `PresentationMaterials.json`. It runs after depth of field and before the temporal upscaler, at the rendering's own resolution, so the upscaler smooths its lines as it smooths the scene's edges; drawn after tonemapping, the lines stair-stepped.
- **What is a character.** Every character's drawn body writes its custom depth with the ink's own stencil: a generated body, and a creature's art. A structure's art is part of the painted world and writes none. A hovered unit takes its hover stencil instead, so the hover outline (ADR-063 §3) replaces its ink while the cursor is on it. The hover outline looks only for its own stencils, so ink and hover never mistake each other.
- **Where a line falls.** A pixel takes ink where a neighbour within reach is a character's visible surface lying nearer than the pixel by more than `depthGap`. That inks both the silhouette against the world and one part of a body in front of a farther part. A character hidden behind the world draws no ink.
- **The line.** `thicknessPixels` wide at `referenceHeight`, growing with the screen. Its colour is the nearer surface's own colour × `darken` × `inkTint`: a deeper, slightly cooler tone of what it outlines, not black. The pass reads the scene before exposure and tonemapping, so `darken` is in linear light (0.1 there is about 0.35 on screen). The line is solid where `solidNeighbours` of the eight neighbours see the nearer surface and fainter where fewer do, which softens its edge.
- **Its cost.** One custom-depth pass over the characters and one screen pass of eight neighbours. §5 measures it.

### 4. What follows in this milestone
In this order, each in its own gate. Each settles its own values and adds them to this record:
- **Graphic effect materials and systems**, generated as ADR-063's are:
  - an impact burst, a cast ring, a slash, a projectile core and trail;
  - status effects: stunned, slowed, shielded, healed;
  - death, recall and respawn.
  - Coloured by side, and for a Vanguard's own effects by its kit's hue.
- **Ground telegraphs** drawn as shaded shapes rather than lines.
- **Hit feel:** a brief hold on a struck body (hitstop), and a small camera shake on the player's own heavy hits. Both follow Screen Shake and the reduced-motion settings.

Animation is not part of this milestone. The author's roadmap (2026-10-06) gives high-fidelity models and assets to M59, and character and environment animation to M60.

### 5. Budget
Every gate records the frame time and draw cost it adds at the gameplay camera, in a ten-Vanguard fight. An effect family that cannot fit is cut back before it ships.

## Consequences
- Characters read off the painterly ground by their shading and outline as well as their side colour.
- The toon look lives in generators and kit data, so a body or an effect is regenerated, never hand-edited.
- Bodies no longer take the world's shadows. A toon look accepts that, and it is cheaper.
- The canon text of Art Direction v0.1 needs the amendment recorded here, so artists read the ruling there too.

## Provisional answers for the author
1. Characters include Fluxborn and creatures as well as Vanguards, companions and Echoes (§1).
2. Unlit toon shading with its own sun, receiving no world shadow (§2).
3. A screen-space ink pass over the characters' custom depth, in a darkened local colour, rather than an inverted hull: one pass for every character, with no second draw of each body (§3).
4. The effect families and hit feel listed in §4.
5. Fluxborn and creatures are inked now but keep their kit materials until M59 remodels them (§3).
