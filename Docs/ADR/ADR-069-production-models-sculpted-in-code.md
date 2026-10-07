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

Asked how models should be made, the author first chose procedural modelling with no generative AI. They then tried a generative 3D service themselves and set it aside (2026-10-07): "maybe just use the image of the a pose as a reference and go procedurally". They supply a full-body reference image of a Vanguard on request.

What exists:
- Every body is built by `GenerateVanguardBodies.py` from parts (tubes, balls and panes) weighted rigidly to the humanoid skeleton.
- Bodies are coloured by vertex colours and fitted to the capsule within a 9,000-triangle humanoid budget.
- The kit and its manifest make every body reproducible and checked in CI.

## Decision

### 1. A production model is a script that sculpts
- **The script.** Each production model is a Python script, `VanguardBodies/models/<id>.py`, named by its kit entry's `model`.
- **What it does.** It sculpts the body as signed distance fields with the sculpt toolkit, `VanguardBodies/sculpt/`:
  - lofted cross-sections for torso, limbs and head;
  - primitives blended with a C2 smooth minimum;
  - garments as shells standing off the body, cut by regions;
  - frames of their own for the head and the hands, so a head bows to a sight and fingers curl round a grip.
- **Fit.** It is fitted to the same humanoid layout, skeleton and animations as every body (ADR-064). Its stance is sculpted in, as the layout's stance is.
- **References.** The author's reference images guide it. Proportions are measured from the reference, in pixels against the figure's height, and so is the palette, sampled and converted to linear light. The reference is never traced into the asset.
- **Provenance.** No generative or third-party content enters a model: every form and colour is written in its script.

### 2. The pipeline
The generator runs it in the pinned Blender, with the numpy and OpenVDB it bundles:
1. **Mesh.** The field is evaluated brick by brick, only where its surface may pass, into an OpenVDB level set, which OpenVDB meshes. A part marked detail is left out of this mesh.
2. **Reduce.** The mesh is reduced to the entry's triangle budget, keeping the face and hands denser. It is shaded smooth, with hard edges where it folds sharply.
3. **Unwrap.** The mesh is unwrapped at one texel density, the face and hands at twice it.
4. **Bake.** The textures are baked from the field itself, detail parts included. Each texel's point on the game mesh is moved onto the full surface along the field's gradient. That gives:
   - its true normal, as a tangent-space normal map in MikkTSpace, as the engine computes tangents on import;
   - its material, which paints the base colour (sRGB): a stylised material model of grain, occluded crevices darkened and worn edges lightened;
   - its occlusion, glow and opacity, in a mask texture.
5. **Skin.** Each vertex takes the bones of the part it lies on; a garment's vertex takes those of the body part beneath it. The weights are then smoothed across the surface, so joints bend in a blend.
6. **Export.** The skeleton, animations and FBX export are ADR-064's. The three textures are written as PNGs beside the FBX.

- **Workers.** The field is evaluated in worker processes, each rebuilding the sculpt from the model's script (every random draw is seeded, so each holds the generator's sculpt). Meshing, labelling and baking spread across them.
  - A point's result depends only on its own brick or cell, so the output is the same however many workers share it.
  - A full build of every body fell from 28 to 7 minutes on the author's machine (Kade's own from about 20 to 4).
  - The worker count is the build machine's setting (`VEYRA_SCULPT_PROCESSES`), half its logical cores by default.

### 2a. Cloth, hair and faces
- **Layers.** A garment is solid down into what it covers. Its outer face stands its thickness (and its folds) out from the layer beneath, and none of that layer's surface remains under it.
  - It rests on that layer as meshed. Relief baked into the layer beneath (a coat's quilting) does not show through the cloth or belt over it.
  - A hollow skin would leave air thinner than a voxel between layers. The mesher tunnels that air into handles that no unwrap can flatten, and the reduction spends triangles on surface no one sees.
- **Regions keep to limbs.** A garment's region names the limbs it covers, and a point belongs to it where it lies nearer those limbs' surfaces than any other limb's.
  - So a sleeve stays on its arm and trousers on the legs in every stance. A height band alone wraps whatever limb passes through it, such as an arm hanging at the hip or raised to a sight.
- **Drapes.** Cloth bunched over the body, such as a mantle's folds round the neck, is laid as folds. A fold is a soft ridge along a curve placed on the surface, tapering toward both ends and solid beneath.
- **Sheets.** A cloak or a coat's tails is a single sheet, not a solid: a grid surface the model drapes by a function, built at the density it keeps.
  - It joins the game mesh after the sculpt is skinned, weighted by its builder: a cloak down the cape bones, tails half after each thigh.
  - The bake paints it by its own material over its own (u, v). That material tears its hem into strips and holes through the mask's opacity, and it takes its occlusion from the sculpt around it.
  - A sheet is pushed clear of the limbs it would pass through.
- **Hair.** Hair is a solid core of locks under hair cards: ribbons laid along each lock, carrying a strand paint whose strands fray at their own lengths through the opacity.
  - Ruling (the author, 2026-10-07): cards rather than strand grooms. The engine's strand hair is production-ready, but it renders realistic rather than toon and costs GPU time for every Vanguard on screen.
- **Faces.** Eyes, brows, lips and stubble are painted onto the skin in the head's own frame, so the paint stays on the face however the head turns. A toon face is painted, not sculpted.
- **Unwrapping.** The game mesh is cut into charts: each face's material, split by the way it faces (normals smoothed). Each sheet stays whole, fragments merge into their neighbours, and specks the reduction left are dropped. Then each chart is unwrapped angle-based. A smart projection left thousands of islands, and their margins wasted most of the texture.

### 3. Budget and records
- **Budget.** A model entry carries its own triangle budget, texture size and voxel size. Kade's is 24,000 triangles and 2048² textures.
- **Records.** The manifest records each model body's textures and their SHA-256 values. The content hash covers them, so a texture change is a change. CI checks committed textures as it checks committed FBX.
- **Staleness.** The generator's hash covers the whole `VanguardBodies` package, its sculpt toolkit and models included, so changing either makes the bodies stale until rebuilt.

### 4. The material
- **New inputs.** `M_VeyraVanguardBody` (ADR-068 §2) gains three texture parameters, with neutral defaults so a vertex-coloured body renders exactly as before:
  - `BaseMap` multiplies the vertex colour;
  - `MaskMap` holds occlusion (R), what glows (G, times the vertex alpha) and opacity (B, times the veil);
  - `NormalMap` bends the toon bands and rim.
- **Occlusion strength.** `aoStrength` (kit data) sets how much of the occlusion darkens the bands.
- **Instances.** A model body wears its own instance, `MI_<Body>`, which sets its textures.
- **Two-sided.** The material is two-sided, so cloth can be a single sheet; the normal turns with the side shown.

### 5. The layout
The humanoid layout gains three kit fields, each optional:
- `legShare`: the leg's share of the height.
- `hipShare`: the hip joints' spacing.
- `aimBore`: the aim stance fitted to the model's rifle. The bore line's height and offset, where each hand holds it, and which way each elbow bends.

A body that sets none is laid out as before.

### 6. Limb inverse kinematics
The author's direction (2026-10-07): production models come with IK rigs and animations, IK used where a limb must hold to something fixed and FK everywhere else.
- **What holds.**
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
  - **Off hand.** The off hand returns to where it rests on the weapon hand in the rest pose. It is held as fully as its clip keeps it near there, so a drift is corrected while a clip that takes the hand away (a gesture) is let go.
- **Pure rules.** `VeyraLimbIK` holds the solve, the plant weight, the hold weight, the pelvis drop and the speed share, each tested.
- **Data.** Every value is a presentation setting (`Vanguards|Limb IK`).
- **Presentation only.** Nothing reads it back; the capsule stays the unit's place.

## Consequences
- Production models are reproducible from source and reviewable as code. A model is regenerated, never hand-edited.
- A model's quality is the quality of its script. The toolkit grows with each Vanguard: garments, hard-surface pieces and hair families become shared modules.
- Bodies cost more: 24,000 triangles and three 2048² textures a Vanguard where they had 9,000 triangles and none. ADR-068 §5's measurement is repeated once the first models are in.

## Provisional answers for the author
1. Production models are sculpted in code from the author's references, without generative content (§1, the author's ruling of 2026-10-07).
2. A 24,000-triangle budget and 2048² textures for a model Vanguard (§3).
3. Textures baked from the field rather than painted (§2).
4. One two-sided body material for skin, garments and cloth (§4).
5. Cloth as sheets and hair as cards over a solid core, the latter the author's ruling (§2a).
6. Runtime IK for planted feet and a two-handed weapon's off hand only, eased by plant and speed (§6).
