# Item icons

One icon per item the [Item Bible](../../Docs/Design/Veyra_Item_Bible_v0.3.md) names, 64 in all. The current full-set direction is [Art Direction: item inventory illustrations](../../Docs/Design/Art_Direction_v0.1.md#item-inventory-illustrations): **bold equipment shapes with integrated effect cues**, using Veyra's toon shade bands and graphic effects. All 64 icons are redesigned together, including the original finished icons, so silhouette and function take priority over surface detail.

## Current source

[`Toon/README.md`](Toon/README.md) records the visual grammar, production method and acceptance boundaries. [`Toon/generation.json`](Toon/generation.json) records every exact prompt, purpose and exported hash; [`Toon/repairs.json`](Toon/repairs.json) records targeted review corrections. `Toon/Sources/` retains individual built-in image_gen outputs, and `Toon/Before/` preserves the prior set. These directories use filenames excluded from the recursive importer.

Run [`Toon/export_icons.py`](Toon/export_icons.py) with Python and Pillow to preflight all sources and regenerate the 256 px icons, `contact_sheet.jpg`, and the [48 px / 32 px / grayscale review](Toon/readability_48_32_gray.png). Final author artwork review and in-engine visual acceptance remain pending; the tool's success does not grant artistic approval.

## Earlier source history

- The initial source set comprised:
- **The author's 37** (supplied 2026-09-29): 8 Tier 1 components, 16 Tier 2 items (boots included), 11 Tier 3 Masterworks and 2 consumables.
- **19 placeholders** for the bible's 2026-09-30 revision (ADR-025 §8.9): 4 components, 8 assemblies, 4 Masterworks, the 2 Quest Items (`Quest_Items/`) and The Last Harbor (`Tier_4_Mythicals/`).
- **2 placeholders** for its 2026-10-02 revision (ADR-050): Echo Lens and The Second Self.
- **6 placeholders** for the items ADR-051 adds: Veil Needle, Blank Sigil, Cutline Mantle, Oathpiercer, Witnessless Edge and Memoryglass Reliquary.

**Earlier 2026-10-13 replacement pass:** The 27 placeholder slots were replaced with equipment illustrations matching the original painterly icons. The subsequent author-directed toon pass supersedes that treatment for all 64 items.

[`Generated/generation.json`](Generated/generation.json) records that superseded pass. [`Generated/contact_sheet.jpg`](Generated/contact_sheet.jpg) previews it; `Generated/Before/` preserves its former placeholders. Its exporter requires an explicit `--restore-previous-pass` option to overwrite the current icons. The historical placeholder generator has no active entries.

- **Files:** `<tier folder>/T_<item_id>_Icon.png`, 256 × 256 px, opaque RGB on a dark charcoal ground. High-resolution toon sources are retained under `Toon/Sources/`.
- **No UI baked in:** borders, selection, text, prices and cooldowns are drawn by the game.
- **Index:** `contact_sheet.jpg` is a labelled preview; the labels exist only there.
- **Legacy provenance:** `manifest.csv`, `manifest.json` and `generation_prompts.json` describe the original 37 images, not the current toon exports. Their `file` and `sha256` fields describe the original 512 px exports. Current provenance is `Toon/generation.json`.
- **Status:** these are newly generated designs from the item names, recipes and stat identities, not previously approved visual canon.

## Into the game

`Game/Scripts/BuildIconArt.ps1 -Kind Items` imports them as UI textures, `/Game/Veyra/UI/Items/T_<item_id>_Icon`, which the shop's tiles and the HUD's item bar show (`VeyraShellArt::ItemIconOf`). An item with no icon shows its initials.

All 64 current runtime catalog items have a corresponding source and imported texture. Item-art automation checks catalog coverage. No item tuning or gameplay behavior changes in this art pass.
