# ADR-069: Production Vanguard models sculpted in code

**Status:** Proposed. The author's standing instruction (2026-09-28) is to keep working unreviewed toward a viable game, choosing a provisional answer where canon is silent. The closing section lists every such answer for the author to overturn. This ADR becomes Accepted when the author merges the pull request that adds it.
**Date:** 2026-10-07
**Related:**
- [ADR-064](ADR-064-generated-animated-vanguards.md): generated bodies, the kit, the skeleton and its animations, and the generator owning its output. This record extends it.
- [ADR-068](ADR-068-toon-characters-and-graphic-combat-effects.md): the toon body material and the ink, which production models wear.
- [Art Direction](../Design/Art_Direction_v0.1.md) as ADR-068 §1 amends it: toon characters in a painterly world.
- The Character Bible's art precedence (approved artwork supplied by the author is canon).

## Context
The author's roadmap (2026-10-06) gives M59 to "high fidelity models": "AAA indie studio level 3d models and assets". They rejected a first sculpt-over-primitives try as programmer art ("dont take shortcuts. think of these as the production level models we need").

Asked how models should be made, the author first chose procedural modelling with no generative AI. They then tried a generative 3D service themselves and set it aside (2026-10-07): "maybe just use the image of the a pose as a reference and go procedurally". They supply a full-body reference image of a Vanguard on request, or, where they have none, its full-body splash art: "the splash art is a good enough reference to get a decent model into the game" (2026-10-07).

A first production Kade then copied his reference closely: 24,000 triangles, 2048² baked textures, a painted face, quilting, laces and a tattoo. The author set the target (2026-10-07):
- **Silhouette, not detail.** A Vanguard's silhouette from the game camera should resemble its splash art as closely as possible. The splash art's detail is not the target, and 24,000 triangles are not needed.
- **Low poly, stylised.** A simple low-poly model in the toon style.
- **Loose parts move.** Hair, cloth and anything like them are dynamic.
- **Believable states.** A body with a gun does not run or walk with it at its face, nor die sighting down the barrel. The first Kade's aim was sculpted into his rest pose, so every clip inherited it.

## Decision

### 1. A production model is a script that sculpts
- **The script.** Each production model is a Python script, `VanguardBodies/models/<id>.py`, named by its kit entry's `model`.
- **What it does.** It sculpts the body as signed distance fields with the sculpt toolkit, `VanguardBodies/sculpt/`:
  - lofted cross-sections for torso, limbs and head;
  - primitives blended with a C2 smooth minimum;
  - garments over the body, cut by regions;
  - frames of their own for the head and the hands, so fingers curl round a grip.
- **What it models.** The big forms that make the Vanguard's outline from the game camera, and its colour regions. Nothing the camera cannot see.
- **Fit.** It is fitted to its archetype's layout, skeleton and animations, as every body is (ADR-064): a humanoid's or a colossus's. A humanoid rests in the A pose, empty-handed: a stance is the clips' (§6), never sculpted in.
- **References.** The author's reference images guide it: the splash art for the silhouette, a full-body reference (or the splash art, where it shows the whole figure) for proportions, measured in pixels against the figure's height, and the palette, sampled. The reference is never traced into the asset. Where it disagrees with canon or the kit (a held lantern the art omits, a forelimb canon sets on the ground), canon and the kit win; where its proportions would not read from the game camera (a head a twelfth of the height), the model fits the camera.
- **Provenance.** No generative or third-party content enters a model: every form and colour is written in its script.

### 2. The pipeline
The generator runs it in the pinned Blender, with the numpy and OpenVDB it bundles:
1. **Mesh.** The field is evaluated brick by brick, only where its surface may pass, into an OpenVDB level set, which OpenVDB meshes.
2. **Reduce.** The mesh is reduced to the entry's triangle budget, less its cloth sheets and a triangle for each bone's anchor, keeping the hands, face and weapon denser. Specks the reduction leaves are dropped, and it is shaded smooth.
3. **Skin.** Each vertex takes the bones of the part it lies on; a garment's vertex takes those of the body part beneath it. The weights are then smoothed across the surface, so joints bend in a blend.
4. **Colour.** Each face takes its material's colour flat (the author's ruling, 2026-10-07): the material of the part nearest its centre, or its sheet's. The vertex colour carries it, and its alpha marks what glows; the toon material shades it. There is no texture to bake.
5. **Export.** The skeleton, animations and FBX export are ADR-064's.

- **Workers.** The field is evaluated in worker processes, each rebuilding the sculpt from the model's script (every random draw is seeded, so each holds the generator's sculpt). Meshing and labelling spread across them.
  - A point's result depends only on its own brick or cell, so the output is the same however many workers share it.
  - The worker count is the build machine's setting (`VEYRA_SCULPT_PROCESSES`), half its logical cores by default.
- **Speed.** A low-poly model builds in about 20 seconds.

### 2a. Garments, cloth and hair
- **Layers.** A garment is solid down into what it covers. Its outer face stands its thickness (and its folds) out from the layer beneath, and none of that layer's surface remains under it.
  - A hollow skin would leave air thinner than a voxel between layers. The mesher tunnels that air into handles, and the reduction spends triangles on surface no one sees.
- **Regions keep to limbs.** A garment's region names the limbs it covers, and a point belongs to it where it lies nearer those limbs' surfaces than any other limb's.
  - So a sleeve stays on its arm and trousers on the legs in every pose. A height band alone wraps whatever limb passes through it.
- **Drapes.** Cloth bunched over the body, such as a mantle's folds round the neck, is laid as folds. A fold is a ridge along a curve placed on the surface, tapering toward both ends and solid beneath, its crest hanging below its line as cloth folds over its own weight.
- **Sheets.** A cloak or a coat's tails is a single sheet, not a solid: a grid surface the model drapes by a function, built at the density it keeps.
  - It joins the game mesh after the sculpt is skinned, weighted by its builder down its spring chains (§7).
  - Its hem is torn in its geometry: each strip ends at its own length, pointed.
  - A sheet is pushed clear of the limbs it would pass through.
- **Hair.** Hair is a few big solid locks, the mass and spikes the silhouette shows. Where the kit gives hair springs, each lock passes from the head to the nearest hair chain (§7) the farther it lies beyond the skull; the cap over the scalp stays with the head.

### 3. Budget and records
- **Budget.** A model entry carries its own triangle budget and voxel size. Kade's is 5,000 triangles.
- **Staleness.** The generator's hash covers the whole `VanguardBodies` package except the model scripts, so changing the sculpt toolkit makes every body stale until rebuilt.
  - A model script is an input of the bodies that name it (its text's hash joins their input hash, a status body's included), so changing it makes only those stale, and a script no body names yet (a draft) makes none.
  - A model script imports only the toolkit, never another model, since its hash covers its own text alone.
- **A status body's model.** A status body may carry its own `model` settings (a larger budget for a ride that carries its mount, or `null` to stay generated). The script receives the status body's spec, so one script can build both: Raska's builds Hound only under her Ride body's rider archetype.

### 4. The material
- A model body wears the body material every body wears (ADR-068 §2), coloured by its vertex colour.
- **Two-sided.** The material is two-sided, so cloth can be a single sheet; the normal turns with the side shown.

### 5. The layout
The humanoid layout gains kit fields, each optional:
- `legShare`: the leg's share of the height.
- `hipShare`: the hip joints' spacing.
- `aimBore`: a weapon held in both hands at its sight: the bore line's height and offset, where each hand holds it, which way each elbow bends, how the head bows to the sight, and the weapon's `carry` (§6). A generated body's entry may give only the head's bow and the carry: it aims where its stance puts the weapon, and its holds carry it as a model's do.
- `springs`: the loose parts that hang on spring chains (§7), each with its spring, drag, damping and turn (`stiffness`, `drag`, `damping`, `maxAngle`).

A body that sets none is laid out as before.

### 6. Holds and limb inverse kinematics
The author's direction (2026-10-07): production models come with IK rigs and animations, IK used where a limb must hold to something fixed and FK everywhere else.
- **Holds.** A weapon held in both hands is a hold, not a pose sculpted in.
  - The body rests empty-handed in the A pose. Its weapon hangs from the right hand's prop bone, and its hands are built as they hold it.
  - Each clip reaches both arms to where the weapon is: carried low across the body as it stands, runs, casts, recalls or is struck; raised to the sight only through the attack's windup and strike, the head bowing to it; let go as the body dies, the weapon tipping out of the hand to lie flat.
  - The arms are two-bone solves in the generator, each elbow hinged as it bends at rest so a forearm never twists. The carry rides the chest; the sight holds where the layout aims it.
  - Each hand and its prop bone rest as they are at the aim, moved unturned to the rest wrist, so the weapon built on them is the one the holds carry, and a dropped weapon tips about its grip.
- **What holds at runtime.**
  - A body's feet are held to the ground under them, so a Vanguard stands on the battleground's slopes and steps rather than floating or sinking.
  - A weapon carried in both hands keeps the off hand on it, however the carrying hand moves.
  - Every free motion (a run's swings, a strike, a cast) stays the clips' forward kinematics.
- **The art names its limbs.** The body art's `FootChains` (root, joint, end), `OffHand` and `OffHandAnchor`. The generator writes them for an archetype that names them: the humanoid's legs, and its left arm when its stance holds a weapon in both hands.
- **The ground.** The animation instance traces the ground under each foot from where the feet stood the frame before: world-static objects only, within a band above and below the floor the capsule stands on. It eases each foot's offset and the ground's slope toward what it finds. It traces only for a body drawn lately.
- **The solve.** In the native proxy, over the blended clips and in the skin's space:
  - **Planted feet only.** A foot is held as far as it is planted in its clip, freed as it lifts above its rest. A moving body keeps a share of the hold that falls with its speed, so IK eases in and out and never sits at full weight.
  - **Pelvis.** The pelvis lowers so the lower foot reaches its ground.
  - **Joints.** Each leg is a two-bone solve whose knee keeps the plane it bends in now, so it never flips. A bone stretches by at most `LimbMaxStretch` of its length.
  - **Slope.** A planted foot tilts toward the ground's slope.
  - **Off hand.** The off hand returns to where it holds the weapon hand in the idle clip (the rest only for a body without one). It is held as fully as its clip keeps it near there, so a drift is corrected while a clip that takes the hand away (a gesture, a death) is let go.
- **Pure rules.** `VeyraLimbIK` holds the solve, the plant weight, the hold weight, the pelvis drop and the speed share, each tested.
- **Data.** Every value is a presentation setting (`Vanguards|Limb IK`).
- **Presentation only.** Nothing reads it back; the capsule stays the unit's place.

### 7. Loose parts
Hair, cloaks, coat tails and sashes move as such things do (the author, 2026-10-07).
- **Chains.** A loose part hangs on spring chains: short bone chains from the bone it hangs from to a tip. The generator adds them for the parts a kit's `springs` names, and the body's art lists them (`SpringChains`), with the capsules they hang outside (`SpringColliders`: torso and legs).
  - A cloak hangs on three chains (down the back and behind each arm), a coat's tails on two, hair on four (down the back, over each ear, over the brow), a long skirt on four round the hips, each arm's drape on one from its forearm, and a lantern on one from the hand that holds it.
  - **Hanging parts.** An arm's drape and a held lantern hang plumb in every clip but a death: the clips turn each chain's first bone back against the limb it hangs from, so it falls straight down however that limb is raised. In a death it follows the limb to the ground.
- **The motion.** In the native proxy, after the limbs, each chain trails where the clips put it, in the world. Each joint is a mass on a spring to where its clip has it:
  - `stiffness` is the spring (per second squared, its natural frequency squared);
  - `drag` is the speed it loses through the air, so a running body's cloak streams behind it;
  - `damping` is the speed it loses relative to its clip, so a lock of hair bounces and settles without streaming;
  - each bone keeps its length, turns at most `maxAngle` from its clip's bone, and stays outside the colliders.
  - There is no gravity: the clips already hang the cloth, and a spring to them would only pull it back.
  - So a running body drags its cloak behind it, and a body that stops lets it swing forward, back and settle.
- **Time.** The motion is stepped in substeps of at most `SpringSubstepSeconds`, the clip moving evenly across them, so a chain moves alike at any frame rate. A long frame counts as `SpringMaxStepSeconds`; a body that jumps farther than `SpringTeleportDistance` (a respawn, a blink) settles its chains on its clips.
- **The clips.** A body whose cloak hangs on chains does not sway it in its clips: its chains do.
- **Pure rules.** `VeyraSpringChain` holds the step, tested (including the same trail at 30 and 144 frames a second).
- **Data.** Each part's motion is its kit data; the timing is presentation settings (`Vanguards|Loose Parts`).
- **Presentation only.** Nothing reads it back.

## Consequences
- Production models are reproducible from source and reviewable as code. A model is regenerated, never hand-edited.
- A model's quality is the quality of its silhouette. The toolkit grows with each Vanguard: garments, hard-surface pieces and hair families become shared modules.
- Bodies stay cheap: about 5,000 triangles each, flat colour, no textures. ADR-068 §5's measurement is repeated once the first models are in.

## Provisional answers for the author
1. Production models are sculpted in code from the author's references, without generative content (§1, the author's ruling of 2026-10-07).
2. A 5,000-triangle budget for a model Vanguard (§3).
3. Flat colour regions, no baked textures (§2, the author's ruling of 2026-10-07).
4. One two-sided body material for skin, garments and cloth (§4).
5. Cloth as sheets torn in their geometry, hair as a few big solid locks (§2a).
6. Runtime IK for planted feet and a two-handed weapon's off hand only, eased by plant, speed and distance; weapons held by holds the clips reach, never sculpted in (§6).
7. Loose parts on spring chains solved at runtime, rather than cloth simulation (§7); Kade's cloak, coat tails and hair, and Sylra's cloak, skirt, arm drapes and lantern (springs and turns are provisional art values).
8. Sylra and Silt modelled from their splash art, the author's supplied references (§1). Sylra's head is fitted to her hood at a size the game camera reads, and her lantern kept from canon though her art omits it; Silt's kit build and leg share follow his art (a normal build, legs 0.34 of his height), his forelimbs stay on the ground as canon sets them, and his drying sediment is cut into long plates by a cellular pattern so his wet interior shows in the cracks.
