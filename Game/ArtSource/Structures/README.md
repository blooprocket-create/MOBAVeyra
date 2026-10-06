# Crucible structure greybox kit

Eight static meshes: standing and destroyed versions of Lane Spire, Base Tower,
Inhibitor and Prime Well. This is a **provisional mesh study**, not approved final
environment art. The current build imports them into
`/Game/Veyra/World/Structures/Greybox/Meshes` with four shared materials under
`/Game/Veyra/World/Structures/Greybox/Materials`.

## Owners and scope

- [Art Direction](../../../Docs/Design/Art_Direction_v0.1.md#crucible-structure-greybox-meshes)
  owns the visual brief and its provisional status.
- [Battleground Bible](../../../Docs/Design/Veyra_Battleground_Bible_v0.9.md)
  sections 1, 3, 5, 10 and 18 own the structures' identities and states.
- [ADR-011](../../../Docs/ADR/ADR-011-battleground-runtime.md) section 4 owns
  server-spawned structures and their collision. These assets add no gameplay.
- [World.json](../../Tuning/World.json) owns the footprint and capsule dimensions.
  The generator reads them; neither the art JSON nor the manifest replaces tuning.

The kit has no neutral Flux Well or fountain mesh. Those are separate objectives
and sites, not any of these four runtime structure kinds.

## Reproduce

From the repository root, with the project's editor already built and Blender on
PATH (or pass its executable):

```powershell
./Game/Scripts/BuildStructureArt.ps1 -Blender '<path to blender.exe>'
```

Blender 5.2.1 LTS and Unreal 5.8.3 were used for this pass. The script runs Blender
in a separate background process and enables Unreal's PythonScriptPlugin only for
the import process. It does not change the project's enabled runtime plugins.

- `StructureKit.json`: editable palette, visual height proportions, mesh detail
  and preview settings. These are art parameters, not gameplay tuning.
- `Game/Scripts/GenerateStructureMeshes.py`: reproducible procedural source.
- `FBX/`: one triangulated, ground-origin static mesh per file, in FBX centimetres.
- `manifest.json`: source fingerprint, bounds, triangle counts, UV and material
  inventory; generated from the actual meshes.
- `Game/Saved/StructureKit/StructureKit.blend`: editable inspection scene.
- `Game/Saved/StructureKit/StructureKit.png`: render of the actual meshes.
- `Game/Saved/StructureKit/unreal-validation.json`: measured Unreal import result.

The saved Blender scene and render are reproducible review outputs, not source
authority. The render is a Blender studio view, not a gameplay-camera capture.
`-ImportOnly` reuses existing FBX files after verifying their source fingerprint.
Before replacing tracked `.uasset` files, acquire their Git LFS locks per ADR-006
section 9. Source FBX files already match the repository's existing LFS patterns.
The generated Unreal meshes and base materials are rebuilt from source; make
palette changes in `StructureKit.json`, and use material instances for later
runtime variations instead of hand-editing generated base materials.

## Placement and later runtime integration

Drag a mesh from the Unreal content folder into an editor scene for art review.
Use scale 1 and place its origin at ground level. Imported visual geometry has
no simple collision; its default collision is disabled, and complex collision is
configured to use the empty simple shape. It must not change pathing or targeting.

The current battleground's gameplay structures are spawned by `VeyraWorld` while
the match loads. **Do not place duplicate gameplay structures into a generated
map.** The grey-box presentation draws these meshes in play. Their asset
references live in the Data Asset `DA_StructureArt` (a `UVeyraUnitArtSet`), keyed
by each kind's manifest ID (`laneSpire`, `baseTower`, `inhibitor`, `primeWell`), as
Architecture section 1.3 and ADR-006 section 6 require; `UVeyraGreyboxSettings`
names the set. `Game/Scripts/BuildUnitArtSets.ps1` writes the set from this
manifest after an import. The presentation observes the replicated structure
state (standing, or its wreck once destroyed), keeps the capsule as the sole
collision/navigation owner, and attaches the visual at the capsule's foot rather
than its centre. `M_CrucibleFlux`'s `FluxTint` takes the viewer-relative side
colour (provisional, for legibility).

Both sides share geometry and physical materials. `M_CrucibleFlux` exposes
`FluxTint` and `FluxStrength` for material instances; blue is a review default,
not a new faction or team-color rule. Its glow ignores the scene's exposure, so
it reads the same under the Crucible's physical sun as anywhere: the kit's
`emission` (the `FluxStrength` default) is in multiples of what the exposure maps
to white, and 0 means the surface does not glow. Every kit surface comes from one
shared builder (`kit_material` in `Game/Scripts/veyra_material_graph.py`, as the
Fluxborn kit's do), and `Game/Scripts/KitMaterials/spec.py` checks the kit's
values, in CI too. `BuildStructureArt.ps1 -Materials M_CrucibleFlux` rebuilds only
the named materials from the kit, without Blender and without importing a mesh. Rebuilding and invulnerability need their
own presentation treatment during integration. Cracking, corruption venting and
energy release during destruction still require animation/VFX. The wreck meshes
provide only the resulting static geometry.

## Checks and limits

Generation checks ground origin, footprint containment, nondegenerate triangles
and two UV channels. Import checks source hashes, mesh type, material slot names,
centimetre bounds, ground origin, both UV channels and absence of simple collision.
UV0 is a unique smart-projected atlas; UV1 duplicates it for a separate lightmap
channel. No texture maps are required by the four greybox materials.

This pass has one LOD per mesh, no destruction animation, no VFX and no final
surface textures. It still needs gameplay-camera review. No gameplay C++,
tuning, replication, map layout or authoritative state is changed.
