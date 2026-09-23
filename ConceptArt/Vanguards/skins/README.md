# Vanguard cosmetic concepts

This catalog tracks **eight current alternate cosmetic concepts**, separately from each Vanguard's canonical `hero.webp`: three Living Paint, three Neon Circuit, and two Amazonian. All eight referenced WebP illustrations are committed on `main`. They are **concepts only**; no in-game skin, pricing, rarity, effects, animations or release status is implied.

The structured catalog is [`index.json`](index.json). Each skin lives under its Vanguard at `ConceptArt/Vanguards/<id>/skins/<collection-slug>/hero.webp`. The art must not overwrite the base Vanguard `hero.webp`.

| Collection | Vanguard | Skin concept | Art |
|---|---|---|---|
| Living Paint | Neris | Living Paint Neris | [View hero](../neris/skins/living-paint/hero.webp) |
| Living Paint | Moro | Living Paint Moro | [View hero](../moro/skins/living-paint/hero.webp) |
| Living Paint | Tavi | Living Paint Tavi | [View hero](../tavi/skins/living-paint/hero.webp) |
| Neon Circuit | Raska | Neon Circuit Raska | [View hero](../raska/skins/neon-circuit/hero.webp) |
| Neon Circuit | Kade | Neon Circuit Kade | [View hero](../kade/skins/neon-circuit/hero.webp) |
| Neon Circuit | Patch | Neon Circuit Patch | [View hero](../patch/skins/neon-circuit/hero.webp) |
| Amazonian | Raska | Amazonian Raska | [View hero](../raska/skins/amazonian/hero.webp) |
| Amazonian | Marek | Amazonian Marek | [View hero](../marek/skins/amazonian/hero.webp) |

## Art direction notes

The **Living Paint** collection uses thick, tactile pigment, palette-knife strokes and dripping splashes as the characters' physical material. **Neon Circuit** uses dark cybernetic hardware and bright cyan energy accents, while preserving each Vanguard's recognizable silhouette and props. **Amazonian Marek** pairs the Amazonian warrior with Nix as a demonic masked panther; he is a companion, not a mount. **Amazonian Raska** replaces Hound's motorcycle silhouette with a cheetah and uses close-combat gauntlets rather than a spear; the base combat identity stays intact. These are visual concepts, not rules for gameplay effects.

Every gallery link above points to an existing `hero.webp` on `main`; the structured index records the matching asset paths and checksums. Current cosmetic concepts live in the Vanguard `skins/` directories, **not** in `ConceptArt/Archives/`, which is reserved for superseded historical artwork.

For a new variant, add an image to its Vanguard directory and an entry to `index.json`; add a catalog row here. Keep variants outside the base hero and model-reference slots so that a new costume never silently redefines the canon appearance.
