# ADR-075: Effects carry the area

**Status:** Proposed. On 2026-10-12 the author reviewed the first rollout batch's recordings (Qazharr, Bryn, Kade, Vera and Mimzi). Their verdict: the skills "all kinda look basic", and the telegraph is what really shows instead of the effects. A telegraph "doesn't need to be there for every skill", and "the effects themselves should be able to indicate what's gonna happen". The author was asked three questions and ruled:
- the player's own aiming indicator stays, and so does a faint outline under big delayed threats;
- an accessibility setting brings the full outlines back, off by default;
- each Vanguard gets signature effects drawn from its own theme.

This ADR becomes Accepted when the author merges the pull request that adds it.
**Date:** 2026-10-12
**Related:**
- [ADR-068](ADR-068-toon-characters-and-graphic-combat-effects.md): the telegraph gate. Every telegraph that threatens or aims keeps its outline and gains a shaded fill. This record amends that gate.
- [ADR-072](ADR-072-skill-animations-and-staged-skill-effects.md): §4 (staged effects) and §9 (basic attacks in flight). This record extends both with a stage that draws an area at its true shape.
- [ADR-041](ADR-041-casting-modes-and-targeting-aids.md): §2, the player's own indicator, which this record leaves as it is.
- [ADR-055](ADR-055-accessibility-settings-and-hud-warnings.md): accessibility settings. This record adds one.
- Combat Bible §1 and its visual fidelity rule: "Visual telegraphs and VFX must closely represent real gameplay hitboxes, ranges, and danger areas."

## Context

ADR-068's gate shaded every telegraph, and ADR-072 gave skills their own motion and effects, but those effects knew nothing of the area a cast lands in. A windup effect gathers on the caster's hands. A delayed area's mark is a ring sized by the ability's reach, and it isn't turned toward the aim. So the shaded telegraph was the only thing that showed where a skill would land, and it outshone the effects. The Combat Bible asks only that what is seen matches the true hit area. It doesn't ask for a telegraph in particular, so an effect drawn at the true shape can do the same job.

## Decision

### 1. A telegraph thins only beside an effect that shows its area

- Each ability's effects (`FVeyraAbilityEffects`) gain `Telegraph`, which takes one of three values:
  - **Full:** the outline and its shaded fill. This is the default.
  - **Outline:** the outline alone. This is a big threat's safety net, kept faint under its effect.
  - **None:** no telegraph.
- **The rule (`VeyraTelegraphFill::ShowOf`):** a telegraph is thinned only where an effect of the same ability shows that same area at its true shape:

  | Telegraph | Effect that must show the area |
  |---|---|
  | A windup's or a channel's | `Area` |
  | A delayed area's | `Area` or its `Mark` |
  | A lingering area's | its `Linger` |
  | A projectile's lane | its `Travel` |

- Anywhere else the telegraph stays whole, whatever the data says. That covers an ability with no effects of its own, a source no cast owns (the player's own indicator, the attack range, the selection ring), and an effect that shows a different stage.
- **An opponent is never shown less than an effect shows them.** Hiding a telegraph can't hide information an enemy is owed.
- **Validation:** at start, a recipe whose `Telegraph` isn't Full must have at least one effect that shows an area (`Area`, `Mark`, `Linger` or `Travel`).
- The player's own indicator (ADR-041 §2) is untouched.

### 2. Show Skill Outlines

- A new accessibility setting, `accessibility_skill_outlines` ("Show Skill Outlines"), is off by default.
- On, every telegraph shows whole: outline and fill, as ADR-068's gate drew them.
- It's an account setting in `Game/Settings/Settings.json`, with its words in `Game/Text`, and the preferences resolve it as ADR-055's other options.

### 3. The Area stage

- `Area` is a looping effect drawn on the ground over each area a cast lands in:
  - from its windup through its channel;
  - over each delayed area of the ability until it lands.
- **One source for its shape:** the shapes come from Abilities.json through the view the telegraph already uses:
  - `VeyraCastTelegraphs::ForCast` for a cast;
  - the delayed area's replicated shapes.
  
  Nothing is typed twice into the effects.
- **Parameters:** it's placed on the ground at the area's origin, turned the cast's way, and given six parameters. All six names are settings, validated at start:
  - `Shape`: 0 a circle, 1 a sector, 2 a rectangle running from its origin;
  - `Radius`;
  - `HalfArc`, in degrees (180 for a circle);
  - `Length` and `Width`, for a rectangle (a circle's or a sector's breadth both ways);
  - `Landing`, from 0 to 1, as the telegraph's fill lands over its last `TelegraphLandingSeconds`.
- **Placement:** a caster who steps or turns while winding up carries the effect with them, as the telegraph would.
- **On real ground:** the effect is drawn on the plane that best fits the ground under its whole footprint, so it tilts with a slope rather than sinking into it. The footprint is sampled over its rim and middle, or its length and breadth, on the playable surface.
- **Uneven ground:** where the ground strays from that plane by more than `AreaGroundTolerance` (a ridge, a step), one plane cannot show the area true. A telegraph the effect hid then keeps its outline, whose segments follow the ground (`VeyraTelegraphFill::ShowOverGround`).
- **Recipe changes:** an effect is restarted when a different recipe or scale lands where another was drawn.
- **Generated systems:** an Area system places its particles over the shape from these parameters alone. Its particles' positions are expressions of the parameters and a hash of each particle's ID, in the effect's local space. So one system draws any circle, sector or lane its ability's tuning gives, and the shape can never drift from the hit area.

### 4. Signature effects

- Effects are drawn from each Vanguard's own theme in the character bible, not from the shared family alone. The pilot's shared systems remain building blocks.
- Every effect still takes its side's colour (ADR-068), so a signature never hides who cast it.
- **Batch 1:**

  | Vanguard | Theme |
  |---|---|
  | Qazharr | Sea spray and wave arcs off a boarding blade |
  | Bryn | Blue Flux shells, muzzle blasts and artillery smoke |
  | Kade | Teal sightlines and precise light |
  | Vera | Spectral firing ranks and a hot red line |
  | Mimzi | Brass rings and star sparks |

- The effects are generated from `Effects.json`, as ADR-072's are, and the batch is reviewed by the author before the rollout goes on.
- **Clipping:** a camera-facing flare or ring played on the ground was cut in half by it and read as a half-dome. The author asked for this to be fixed. Each such emitter in a skill effect that plays on the ground is lifted by 0.3 of its own size, so its lower edge clears the ground at the match camera's 60-degree pitch, where the edge drops a quarter of the size. Effects that fly through the air aren't lifted.

### 5. A volley lane held open shows where its shots can go

On 2026-10-12 the author asked for Kade's ultimate to "show where he can shoot".

- **What used to show:** a volley's lane (UVeyraVolleySubsystem) showed only during its windup. Once open, it lived on the server alone, though its shots stay inside it for as long as it lasts.
- **The open lane replicates:** it now replicates beside its caster's cast state as `FVeyraVolleyLaneState`, which holds:
  - which volley opened it;
  - the way it faces;
  - when it closes at the latest.
- **Who writes it:** the server's lane writes it as it opens and clears it as it closes. Presentation reads it; the server's lane still decides where every shot goes.
- **Cast presentation state:** like the cast state it sits beside, the lane replicates push-model behind the fog. Only the caster, its teammates and those who see it receive it (ADR-016 §3), and it is never authority. `Veyra.Net.Vision.AnEnemysOpenLaneReachesOnlyThoseWhoSeeIt` checks the gate.
- **One shape:** `VeyraCastTelegraphs::ForLane` gives its shape from the volley's tuning. That's the same cone the windup's telegraph shows, from the caster as far as a shot reaches, so the two can't drift apart.
- **How it's drawn:**
  - The open lane is a telegraph source of its own (`VolleyLane`), thinned by §1's rule as a windup is: only beside an `Area` effect.
  - The volley's `Area` effect is drawn over the lane until it closes.
  - Show Skill Outlines brings its outline back.

## Consequences

- **What players see:** a skill's own effects show where it lands, so its look is its warning. The shaded shapes remain only:
  - as the player's own aim;
  - under big delayed threats, as a faint outline;
  - for players who turn Show Skill Outlines on.
- **Rollout effort:** each ability that drops its telegraph needs an `Area`, `Mark`, `Linger` or `Travel` effect that really shows the area. Batch 2 onward carries that cost.
- **Testing:** the rule is a plain function and is tested alone. The settings, the stage's parameters and the greybox's thinning are covered by tests in `Veyra.UI`.
