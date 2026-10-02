# Item icons

One icon per item the [Item Bible](../../Docs/Design/Veyra_Item_Bible_v0.3.md) names, 64 in all:
- **The author's 37** (supplied 2026-09-29): 8 Tier 1 components, 16 Tier 2 items (boots included), 11 Tier 3 Masterworks and 2 consumables.
- **19 placeholders** for the bible's 2026-09-30 revision (ADR-025 §8.9): 4 components, 8 assemblies, 4 Masterworks, the 2 Quest Items (`Quest_Items/`) and The Last Harbor (`Tier_4_Mythicals/`).
- **2 placeholders** for its 2026-10-02 revision (ADR-050): Echo Lens and The Second Self.
- **6 placeholders** for the items ADR-051 adds: Veil Needle, Blank Sigil, Cutline Mantle, Oathpiercer, Witnessless Edge and Memoryglass Reliquary.

[`scripts/placeholder_item_icons.py`](../../scripts/placeholder_item_icons.py) draws the 27 placeholders: a glyph for the item's identity and small glyphs for its other stats. The author's art replaces a placeholder file for file; then its entry leaves the script's `PLACEHOLDERS` table.

- **Files:** `<tier folder>/T_<item_id>_Icon.png`, 256 × 256 px, opaque RGB on a dark charcoal ground. The author keeps the 512 px and original exports; these are the size the UI needs.
- **No UI baked in:** borders, selection, text, prices and cooldowns are drawn by the game.
- **Index:** `contact_sheet.jpg` is a labelled preview; the labels exist only there.
- **Provenance:** `manifest.csv` and `manifest.json` cover the author's 37 and list each item's ID, name, tier and whether `Game/Tuning/Items.json` defines it yet. Their `file` and `sha256` fields describe the author's 512 px exports. `generation_prompts.json` records how each icon was made.
- **Status:** these are newly generated designs from the item names, recipes and stat identities, not previously approved visual canon.

## Into the game

`Game/Scripts/BuildIconArt.ps1 -Kind Items` imports them as UI textures, `/Game/Veyra/UI/Items/T_<item_id>_Icon`, which the shop's tiles and the HUD's item bar show (`VeyraShellArt::ItemIconOf`). An item with no icon shows its initials.

The 11 icons for items `Items.json` does not define yet (Keensteel, Deadeye Edge, Sovereign Edge, Siegeheart Core, Impact Aegis, Razorwheel Prime, Starfall Prism, Nullglass Lens, Gravitic Seal, Arcane Boots and Flux Flask) are imported with the rest, ready for their data.
