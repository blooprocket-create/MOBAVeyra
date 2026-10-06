# Presentation kit: combat readability

The generated assets the grey-box presentation shows a fight with ([ADR-063](../../../Docs/ADR/ADR-063-combat-readability-cues-effects-sound-and-the-fountain-shop.md)):
a hit flash, the hover outline, four Niagara effects and five placeholder sounds; and the smoke Nix's body is made of ([ADR-064](../../../Docs/ADR/ADR-064-generated-animated-vanguards.md) §1). Each comes from a spec in this
folder and a seeded generator, so it is regenerated rather than hand-edited. The imported assets live under
`/Game/Veyra/UI/Presentation`, which is always cooked.

| Spec | Generator | Assets |
|---|---|---|
| `PresentationMaterials.json` | `Game/Scripts/BuildPresentationMaterials.ps1` (Unreal Python, `BuildPresentationMaterials.py`; the spec is checked by `PresentationMaterials/spec.py`, in CI too) | `M_VeyraHitFlash`, an additive overlay for static and skinned bodies; `PP_VeyraHoverOutline`, a post-process outline whose stencils are its own parameter defaults; `M_VeyraSmoke`, a masked, sunlit puff of stylized smoke that erodes as its particle's alpha fades and glows independently of exposure; `M_VeyraEffectPuff` and `M_VeyraEffectStrand`, the combat effects' sprites and ribbons: solid and ragged, white-hot at birth, cooling to their colour, then darkening and eroding with the particle's age |
| `Effects.json` | `Game/Scripts/BuildEffects.ps1 [-Effects <name>]` (the `VeyraEffects` commandlet) | `Effects/NS_VeyraImpact`, `NS_VeyraCastFlash`, `NS_VeyraDeathBurst` (engine system templates) and `NS_VeyraTrail`, `NS_VeyraSmokeBody`, `NS_VeyraSpray`, `NS_VeyraDrip`, `NS_VeyraMist`, `NS_VeyraEmbers` and `NS_VeyraWildstorm` (engine emitter templates), every particle's base colour linked to `User.Color` and every sprite and ribbon drawn with a generated material. An input is set to a value, to an expression, or to an enum entry by its display name as authored, never a translation (a static switch, listed before the inputs it shows, as `Ribbon Width Mode` before `Ribbon Width`), on the emitter it names or on every one that has it; `User.Scale` (default 1) sizes a body's effect to its body |
| `CueSounds.json` | `Game/Scripts/BuildCueSounds.ps1` (`GenerateCueSounds.py` synthesises, `ImportCueSounds.py` imports) | `Audio/S_VeyraImpact`, `S_VeyraSwing`, `S_VeyraCast`, `S_VeyraDeath`, `S_VeyraClick` |

## Rules

- **Presentation only.** None of these decides anything; gameplay state stays with its owners (ADR-063 §7).
- **Glow ignores exposure.** The Crucible is lit physically, under a manual exposure that shows an unlit emissive of 1
  as black. Every generated glow is scaled by the inverse of the scene's exposure, and its strength (`glowGain`) is data
  in its spec, in multiples of what the exposure maps to white (ADR-063, 2026-10-06 amendment).
- **No engine default materials.** The engine's default sprite and ribbon materials are unlit, and vanish under that
  exposure. Every effect names its generated `material` (sprites) and `ribbonMaterial` (ribbons), and the effects
  generator refuses a system with a sprite or ribbon renderer that would keep a default. Build the materials before the effects.
- **One look.** Combat effects draw with `M_VeyraEffectPuff` and `M_VeyraEffectStrand`; a new effect uses them, or
  another `particleEffect` material, rather than an engine template's own.
- **Judge it in the lit Crucible.** `Game/Scripts/CapturePresentation.ps1` stands the hit flash, the hover outline,
  every effect at three moments of its life, the click marker and a swing in L_Battleground, and captures them from
  the gameplay camera into `Game/Saved/PresentationReview`.
- **Regenerate, don't hand-fix.** Change a spec or its generator, then run its script. Each generator refuses to
  overwrite an asset Git LFS has checked out read-only: acquire the lock first (ADR-006 §9).
- **Rebuild only what changed.** `BuildPresentationMaterials.ps1 -Materials <name>` rebuilds one material, `BuildEffects.ps1 -Effects <name>` one effect.
  `BuildEffects.ps1 -Describe` writes each template's topology to `Game/Saved/Effects`, to write a spec against.
- **Determinism.** The sounds are byte-identical for the same spec, seed and generator version.
  `CueSounds.manifest.json` records their hashes, and the import checks them.
- **The Niagara editing utilities** are Experimental in the engine. ADR-063 §4 approves them for `BuildEffects.ps1`
  only, at authoring time.
- **Shaders compile in the cook.** Unreal Python runs without a renderer, so a material's shader code is checked
  when a client is packaged.
- **Placeholders.** The sounds are synthesised stand-ins until authored audio replaces them.
