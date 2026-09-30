# Item icons

One icon per item the [Item Bible](../../Docs/Design/Veyra_Item_Bible_v0.3.md) names: 8 Tier 1 components, 16 Tier 2 items (boots included), 11 Tier 3 Masterworks and 2 consumables, 37 in all. The author supplied them on 2026-09-29.

- **Files:** `<tier folder>/T_<item_id>_Icon.png`, 256 × 256 px, opaque RGB on a dark charcoal ground. The author keeps the 512 px and original exports; these are the size the UI needs.
- **No UI baked in:** borders, selection, text, prices and cooldowns are drawn by the game.
- **Index:** `contact_sheet.jpg` is a labelled preview; the labels exist only there.
- **Provenance:** `manifest.csv` and `manifest.json` list each item's ID, name, tier and whether `Game/Tuning/Items.json` defines it yet. Their `file` and `sha256` fields describe the author's 512 px exports. `generation_prompts.json` records how each icon was made.
- **Status:** these are newly generated designs from the item names, recipes and stat identities, not previously approved visual canon.

## Into the game

`Game/Scripts/BuildIconArt.ps1 -Kind Items` imports them as UI textures, `/Game/Veyra/UI/Items/T_<item_id>_Icon`, which the shop's tiles and the HUD's item bar show (`VeyraShellArt::ItemIconOf`). An item with no icon shows its initials.

The 11 icons for items `Items.json` does not define yet (Keensteel, Deadeye Edge, Sovereign Edge, Siegeheart Core, Impact Aegis, Razorwheel Prime, Starfall Prism, Nullglass Lens, Gravitic Seal, Arcane Boots and Flux Flask) are imported with the rest, ready for their data.
