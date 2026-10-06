# Fluxborn greybox mesh kit

Six placeable static meshes: active and collapsed poses of **Strider**, **Spark**
and **Breaker**. This is a provisional mesh study for review, not approved final
character art. The imported assets are under `/Game/Veyra/Flux/Fluxborn/Greybox`.

| Runtime ID | Current role | Provisional silhouette |
|---|---|---|
| `strider` | Frontline melee | Compact biped, broad shoulders and paired striking gauntlets. |
| `spark` | Ranged | Light tripod, narrow Flux chamber, high resonator vanes and a focused emitter. |
| `breaker` | Siege | Broad four-legged carriage, heavy carapace and a forward siege bore. |

The body sizes come directly from `World.json`. The generator also checks that
its inventory and role labels match that file. Team Flux changes combat strength
through its existing owner; this kit creates no extra unit tiers. Current
inhibitor reinforcements reuse `breaker` and therefore reuse the Breaker mesh.

## Production owners

- [Art Direction: Fluxborn greybox meshes](../../../Docs/Design/Art_Direction_v0.1.md#fluxborn-greybox-meshes)
  owns this provisional visual brief.
- [Battleground Bible](../../../Docs/Design/Veyra_Battleground_Bible_v0.9.md)
  sections 4, 17, 18 and 19 own the three archetypes, collapse, waves, reinforcement
  and targeting rules.
- [World Bible](../../../Docs/Design/Veyra_World_Bible_v0.5.md) sections 3 and 13
  identify Fluxborn as temporary network constructs; these are not living wildlife.
- [ADR-011](../../../Docs/ADR/ADR-011-battleground-runtime.md) section 7 and
  [World.json](../../Tuning/World.json) own movement, collision, unit definitions,
  strength, attacks and corpse lifetime.

Stone shells, iron joints, alloy trim and geometric Flux conduits connect the
kit visually to the Crucible structures. Both sides share the meshes. The blue
Flux material is a neutral review default, not a team-color or strength-state
decision. Its `FluxTint` and `FluxStrength` parameters support later material
instances. Collapsed meshes use dark iron in place of luminous surfaces. The
Flux glow ignores the scene's exposure, so it reads the same under the
Crucible's physical sun as anywhere: the kit's `emission` (the `FluxStrength`
default) is in multiples of what the exposure maps to white, and 0 means the
surface does not glow. The surfaces share the structure kit's builder
(`kit_material` in `Game/Scripts/veyra_material_graph.py`), and
`Game/Scripts/KitMaterials/spec.py` checks their values, in CI too.

## Reproduce

Run from the repository root with the project's editor built:

```powershell
./Game/Scripts/BuildFluxbornArt.ps1 -Blender '<path to blender.exe>'
```

Use `-ImportOnly` to validate and reimport the existing exports. Blender 5.2.1 LTS
and Unreal 5.8.3 were used for this pass. Blender runs in its own background
process. PythonScriptPlugin is enabled only for the Unreal import process; no
runtime plugin or project setting changes are required.
Use `-VerifyOnly` to check the saved Unreal meshes without changing any assets;
its report is `Game/Saved/FluxbornKit/unreal-verify.json`. Use
`-Materials M_FluxbornFlux` to rebuild only the named materials, without
importing a mesh. The manifest hashes the whole kit, materials included (the
generator writes them into its Blender scene and exports), so after a change to a
material's values run `GenerateFluxbornMeshes.py` first: its manifest then
matches the kit, and the meshes need no reimport when their triangles and
dimensions are unchanged.

- `FluxbornKit.json`: editable palette, art proportions, geometry budgets and
  preview settings; no gameplay tuning.
- `Game/Scripts/GenerateFluxbornMeshes.py`: procedural assembly source, with
  named parts for the body, limbs, armor and emitters.
- `FBX/`: one triangulated static mesh per variant and pose.
- `manifest.json`: actual geometry statistics, source hashes and tuning-derived
  footprint snapshot. This reports art output and does not own gameplay values.
- `Game/Scripts/ImportGreyboxMeshKit.py`: the validated editor-only importer.
- `Game/Saved/FluxbornKit/FluxbornKit.blend`: inspection scene with an initially
  hidden `EditableSourceParts` collection containing separate active body parts.
- `Game/Saved/FluxbornKit/FluxbornKit.png`: orthographic Blender render of the
  actual meshes, not a capture from the game.
- `Game/Saved/FluxbornKit/unreal-validation.json`: Unreal import measurements.

The inspection scene can be edited for exploration, but rebuilding uses the
Python assembly and JSON as source. Generated Unreal base materials and meshes
are rebuilt from that source. Acquire Git LFS locks before replacing tracked
`.uasset` files, per ADR-006 section 9. The existing LFS rules cover the FBX files.

## Placement, verification and remaining work

Use scale 1, a ground-level pivot and +X forward in Unreal. Each mesh includes a
`Facing` socket so the importer can verify axis conversion. UV0 is a unique
smart-projected atlas, with a separate UV1 lightmap channel. Every component is
closed geometry, although separate mechanical components intersect by design.
The generator validates manifold edges, finite UVs, nondegenerate triangles,
triangle budgets and capsule-radius footprint containment. The importer checks
source hashes, material mapping, centimetre bounds, ground pivot, forward socket,
two UV channels and disabled collision.

The grey-box presentation draws them in play. Their asset references live in the
Data Asset `DA_FluxbornArt` (a `UVeyraUnitArtSet`), keyed by each kind's content
ID, as Architecture section 1.3 and ADR-006 section 6 require;
`UVeyraGreyboxSettings::FluxbornArt` names it, and `Game/Scripts/BuildUnitArtSets.ps1`
writes it from this manifest after an import. The presentation shows the active
mesh in place of each Fluxborn's capsule body, then the collapsed one once its
replicated life state says it died. The
`M_FluxbornFlux` slot's `FluxTint` takes the body colour a capsule would show:
the viewer-relative side colour, tinted while crowd controlled, as the structure
kit's Flux does (provisional, for legibility). A kind without art keeps its
capsule body. The meshes have no skeletal rig, animation, animation Blueprint,
projectile VFX or network-return death VFX. They have one LOD and use solid
greybox materials rather than final textures. Rigging, animation, additional LODs
and gameplay-camera review follow the silhouette review.

The binding observes the replicated kind and life state only; all rules stay with
their current owners. The existing character capsule remains the
sole gameplay collision and movement owner. The meshes have no simple collision,
their default collision is disabled, and complex collision uses the empty simple
shape. Do not add placed gameplay minions to the generated battleground: its
existing wave system already spawns the authoritative units. The collapsed pose
is only a possible visual during the existing corpse lifetime; it introduces no
persistent wreck or timer.

No gameplay C++, tuning, replication, wave composition, map or authoritative state
is changed by this kit or its binding.
