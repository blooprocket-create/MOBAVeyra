# Item icon replacement pass — 2026-10-13

27 equipment illustrations replace the active placeholder slots. Item identity
comes from the Item Bible sections 4, 6, 8, 9 and 11; source conventions and
consumption belong to the parent item-art README and BuildIconArt.ps1.

Generated with the built-in image_gen tool using the original Iron Grip and
Crown of the Crucible artwork as style references. No third-party source art
was used. Exact prompts, source-sheet names, crop boundaries and output hashes
are in generation.json. Generation is stochastic; retained source sheets make
the crop/export stage reproducible. Run prepare_icons.py with Python and Pillow,
then BuildIconArt.ps1 -Kind Items -Ids with the IDs in generation.json.

Before/ holds the original glyph placeholders under filenames excluded from
the recursive icon importer. The 37 existing finished source icons are unchanged.

Validation: all 27 exported icons are opaque RGB, 256 x 256; the contact sheet
was visually inspected and crops corrected at the detected sheet separators.
Unreal import saved all 27 textures with zero warnings/errors. No gameplay data,
C++, module dependencies, replication, protected geometry or world layout changed.
The placeholder renderer has no active entries and cannot overwrite this art.

In-engine shop/HUD before/after captures and a controlled performance comparison
have not been recorded. Artwork remains proposed pending those checks and the
author's appearance review; successful generation/import is not visual acceptance.
