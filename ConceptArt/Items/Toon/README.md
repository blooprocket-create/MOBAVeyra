# Toon item icon pass — 2026-10-13

## Direction

The author requested all 64 item icons be redesigned to remain distinguishable
and indicate their function, in a toon style compatible with Veyra. The selected
emphasis is **bold equipment shapes with integrated effect cues**.

Item rules remain the Item Bible sections 4–12 and Items.json. Band shading,
cool painted shadows, local-color ink and graphic effects draw from Art Direction
and ADR-068. This pass extends that rendering language to inventory artwork;
final artwork approval and in-engine acceptance remain pending.

## Visual grammar

Shape comes first: components, assemblies and Masterworks have distinct contours,
interior openings, proportions and focal positions. Related upgrades retain a
motif but change their silhouette, rather than adding more metal detail.

Broad secondary color cues: red heart/core for Health; green woven return current
for regeneration; orange force/strike for Physical Power; violet power for Magic
Power; blue-gray layered plates for Armor; cyan shell/deflection for Magic
Resistance; yellow directional repetition for Attack Speed; teal circular timing
for Ability Haste; a precise gold glint for critical strikes. These are proposed
presentation conventions, never replacements for tooltips or actual stat glyphs.

Masterworks show their primary Attunement with one integrated visual metaphor:
a wound through a shield, a repeated reflection, converging beams, stored healing
current, attack motion or a slowing weight. No miniature stat-badge rows, text,
UI borders or tuning numbers are baked into an icon. Color does not carry the
identity alone. Effects never imply unsupported healing, shielding or abilities.

## Source and acceptance

generation.json records all exact prompts and purposes. Sources/ holds one
built-in image_gen result per item. Iron Grip is the shared toon style reference;
each later prompt identifies it as a rendering reference only, never as its subject.
The output is stochastic; retained images make exports reproducible.

export_icons.py uses Pillow to preflight all 64 sources, preserve Before/,
resize to opaque RGB 256 x 256, hash outputs, and build large, 48 px, 32 px and
grayscale contact sheets. Sources/ and Before/ deliberately use filenames that
the recursive Unreal icon importer ignores. Import the changed IDs using
Game/Scripts/BuildIconArt.ps1 -Kind Items after obtaining the LFS asset locks.

Review at 48 and 32 px, grayscale, and in the actual shop and HUD. Confirm
distinctness, true mechanical cues and clean edges. Record controlled before/after
client captures and performance measurements before declaring visual acceptance.
No gameplay rules, data tuning, UI state ownership or module dependencies change.
