# Crucible environment kit

Veyra-authored source geometry for an overgrown highland ruin (ADR-040). `CrucibleKit.json` owns each family's parameters and seed and the kit's look (stone tints, moss, glyphs, foliage, wind); it contains no map coordinates and no gameplay collision. The generator is `Game/Scripts/GenerateEnvironmentMeshes.py` (Blender 5.2, background). No third-party mesh or texture content is incorporated: the kit's materials use the terrain's own generated textures (`Terrain/`).

Run `Game/Scripts/BuildEnvironmentArt.ps1 -Blender <executable>` from the repository root to regenerate the FBX files and source manifest and import them. Use `-ImportOnly` to import unchanged FBX; a change to `CrucibleKit.json` needs the full run, since the manifest hashes it. Pass `-- --only Cliff,Tree --preview` to the generator directly for a contact sheet in `Game/Saved/EnvironmentKit/kit_preview.png`.

Families: slate cliffs and river boulders (cut into facets, ledged and weathered), fluted pillars broken off, masonry blocks, steles with raised Flux glyphs on their own `Glyph` slot, trees, shrubs, ferns, reeds and grass. Material slots are named (`Rock`, `Ruin`, `Glyph`, `Bark`, `Leaves`, `Grass`) and the importer assigns materials by name. Foliage carries its tint in vertex colour and its height share in vertex alpha, for the wind.

The importer builds the kit's materials (they must compile, or it fails), imports each mesh fresh, gives stone Nanite, and checks dimensions, the base pivot and the absence of any collision; its report is `Game/Saved/EnvironmentKit/unreal-validation.json`. Placement is `VeyraDressingBuild` in VeyraWorldTools, from the style profile's `dressingProfile`.

Generated assets belong in `Game/Content/Veyra/World/Environment`. Committed binary assets need the repository's LFS lock before they are regenerated. Never edit FBX or UAssets as an independent authoring source.
