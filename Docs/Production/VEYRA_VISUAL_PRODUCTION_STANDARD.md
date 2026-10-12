# Veyra Visual Production Standard

**Version:** 1.0 (initial production standard)  
**Date:** 2026-10-09  
**Status:** Production process and proposed visual-quality gates; not a new gameplay or art-canon ruling  
**Engine:** Unreal Engine 5.8.3, project-pinned Epic source build  
**Read with:** [Production index](README.md), [Art Direction](../Design/Art_Direction_v0.1.md), [Pre-Game Client UX Bible](../Design/Veyra_Pre_Game_Client_UX_Bible_v0.1.md), [Settings & Accessibility Bible](../Design/Veyra_Settings_Accessibility_Bible_v0.1.md), [Asset & VFX Pipeline](VEYRA_ASSET_AND_VFX_PIPELINE.md), [World Production](VEYRA_WORLD_PRODUCTION_BIBLE.md), [World Validation](VEYRA_WORLD_VALIDATION_STANDARD.md), [Architecture](../../ARCHITECTURE.md), [Visual Upgrade Roadmap](VEYRA_VISUAL_UPGRADE_ROADMAP.md).

## 1. Purpose and definition of done

Veyra must look and feel like one intentionally art-directed, commercially presentable game in **the actual shipped Unreal client**: ordinary menus, committed Vanguard selection, the Meridian Crucible, Vanguards, abilities, HUD, results, and their transitions.

A feature is not visually done because code compiles, Blender exports, Niagara plays, a UMG widget renders, a screenshot is attractive at one zoom, or an agent describes it as "AAA." Production acceptance requires:

1. **Fidelity:** follows approved Veyra artwork, silhouette, palette, and the surface-specific design brief.
2. **Consistency:** uses shared materials, typography, spacing, icons, motions, sound language, and effect families; justified exceptions are documented.
3. **Readability:** works from the real gameplay camera, at competitive combat density, without implying false ranges, occluding critical states, or leaking fog/stealth information.
4. **Responsiveness:** effects and animation match the authoritative event/cast state; controls and client interactions provide prompt, consistent feedback.
5. **Accessibility:** approved display, motion, contrast, colour, text, and input preferences remain respected.
6. **Performance:** a repeatable measured configuration meets its approved budget with quality/scalability fallbacks.
7. **Reproducibility:** source/generator, parameters, capture recipe, review evidence, and rollback path are recorded.
8. **Human review:** the author explicitly approves signature visual changes; automation may flag regressions but does not declare artistic sign-off.

**This is a process standard**, not permission to approve a redesigned Vanguard, new UX feature, gameplay-map layout, engine fork, renderer dependency, UI-navigation rule, or new player setting. Open decisions require author approval and, where applicable, an ADR.

## 2. Canon, ownership, and what must not change

| Concern | Authoritative owner | Visual production responsibility |
|---|---|---|
| Engine/source boundary, modules, server authority | `ARCHITECTURE.md`, `PROJECT_STRUCTURE.md`, accepted ADRs | Presentation observes; never adjudicates gameplay |
| Base Vanguard identity and silhouette | Approved base hero art; Character Bible; Art Direction | Match approved silhouette at gameplay camera, do not silently redesign |
| Overall look | Art Direction, including ADR-068's hybrid ruling | Painterly-realistic world; toon Vanguards, companions, Echoes, Fluxborn and creatures; graphic combat effects |
| Client navigation, social, party, selection, priority overlays | Pre-Game Client UX, Client & Platform, Parties/Chat/Match Flow | Reskin and refactor the visual layer **without changing UX-1–92 behavior** |
| HUD preferences, contrast, scaling, safe area, motion, graphics | Settings & Accessibility Bible | Preserve approved behavior and setting semantics; do not invent new options |
| World geometry, vision, range and objective access | Battleground, Vision, Combat; `Game/Tuning/World.json` | Dress world without changing gameplay topology or information permissions |
| Visual source and generation | Asset & VFX Pipeline; World Production | Blender source/generators; Unreal shipping materials, Niagara, lighting, UI and integration |
| Map/world acceptance | World Validation Standard | Apply its A–D validation levels in addition to this standard |

The engine checkout remains read-only by default (ADR-040). Do not enable an Experimental UE feature as a shipping dependency without the ADR-001 review. Evaluate rather than silently mandate Common UI, Nanite, Lumen, virtualized shadows, TSR, retainer render targets, or any other optional renderer/UI technology.

**Presentation boundary:** `VeyraUI` and authored UI assets read client snapshots, replicated data, and approved events; all actions travel through existing intents. UI, Niagara, animation, materials and visual-only Blueprints never own HP, Flux, Gold, vision, hit testing, capture state, damage, or permission.

## 3. Visual reference and approval package

Before any broad visual change, prepare a small reference package:

- **Target:** exact screen, scene, asset family, Vanguard, ability, camera and the expected viewer state.
- **Canon:** link approved Veyra art and the relevant bible section; note whether the new look is already approved or is a proposed treatment.
- **Composition:** hierarchy, focal point, layering, negative space, saturation and contrast, with specific threats to visual clarity.
- **Interaction/motion:** hover, focus, active, disabled, waiting, success/failure, error, in/out transitions, reduced-motion equivalent.
- **Constraints:** target platform/hardware, resolution/aspect, UI scale/safe area, in-world pixel footprint, worst-case screen density.
- **Source:** exact owning generator, material family, widget style asset, data file, and output; no hand-edit of generated artifacts.
- **Acceptance:** screenshot/movie shot list, checkable criteria, profile/trace cases, reviewer and rollback.

Create an **as-built** reference after approval: capture the real in-game result, not just a concept mockup. Store lightweight reviews in the PR; keep heavyweight captures/traces in an agreed artifact location, referenced by links/identifiers. Protect confidential recordings and user data.

## 4. Shared visual language

**Character/world/effect separation.** The world is painterly-realistic with materially credible stone, metal, soil, plants and water; characters are stylized/toon with clear shade bands and local-colour ink; ability effects use deliberately graphic bursts, rings, slashes, beams and trails. Do not blend these into one undifferentiated emissive/noise style.

**Hierarchy.** From the gameplay camera, prioritize: (1) actionable danger/targeting, (2) Vanguards and essential state, (3) objectives/Fluxborn, (4) traversable terrain, (5) ambience. Art can be intricate in close presentation and restrained at match zoom. No important meaning communicated by hue alone.

**Reusable style system.** Establish shared palette roles, typography roles, nine-slice/button/surface assets, icon geometry/weight, border/elevation levels, material functions, effect masks, transition timing categories, and audio cue families. Values live in designer-editable **presentation** assets/settings and are validated; do not scatter literal style values or per-screen copies through C++.

**Distinctiveness.** Canonical Vanguard silhouette wins over decorative detail. The Crucible remains one ancient Flux-network installation, not an invented alliance of political factions. Team side and Flux state must be legible independently of any Vanguard's regional signature hue.

**Effects as information.** VFX may embellish a committed outcome, not manufacture an uncommitted hit, hidden target, hidden timer, or apparently larger collision zone.

## 5. Production client UI standard

### 5.1 Architecture and migration

Current `Game/Source/VeyraUI/Private/Shell/` assembles many UMG widget trees in C++; `VeyraShellStyle` holds shared presentation. **Preserve those client models and authoritative intent boundaries.** Build a reusable *art-directable* layer of UMG Widget Blueprints/style assets/material instances where it improves iteration speed; keep C++ for state orchestration, validation, shared lower-level behavior, and data adapters.

- Use view-model/snapshot-driven, event-based updates. Avoid polling UI attributes every tick; use Slate/UMG invalidation appropriately and mark truly continuously changing elements intentionally.
- Separate **content and state** from **layout and skin**. One authoritative view model must feed the old and new presentation while migrating; do not duplicate purchase/queue/party rules in widgets.
- Keep ordinary client **friends/chat sidebar** and **party/matchmaking bottom panel** independent and collapsible. During committed selection, preserve the approved sidebar minimization and party controls being hidden. Match Found and reconnect retain their priority and must not be obscured by decoration.
- Preserve the approved selection composition (roster bench top, countdown beneath, teams to the sides, large Vanguard art center, loadout and Lock In bottom).
- Cross-screen components: navigation, cards, portrait frames, buttons, tabs, counters, badges, notifications, dialogs, tooltips, chat composition, loading/error/empty states.
- Implement complete interaction states (normal, hovered, focused, pressed, unavailable, loading, error) and keyboard/controller semantics where supported; never convey availability by colour alone.
- Common UI may be **evaluated** for layered focus/input routing. Adoption needs a technical decision, spike, regression tests and migration plan; do not replace the current client-state coordinator or silently change inputs.

### 5.2 Screens that require production treatment

**Home:** clear live-feature hero, Vanguard or event identity from authorized account/content state, appropriately restrained ambient motion, discoverable Play/Collection/Shop, persistent social/party surfaces.

**Play/party/social:** durable queue/status communication; legible invited/ready/locked transitions; presence and chat with no accidental overlay of blocking accept dialogs.

**Vanguard select/Collection:** full-bleed approved art or controlled 3D showcase with legible team/pick state; readable countdown under stress; no navigation escape during committed state; retain ownership/eligibility logic.

**Loading, results, profiles, shop:** consistent structure, backgrounds and empty/error/progress states; no invented ownership/currency or unapproved content.

**Review at minimum:** 16:9 1920x1080 and another supported desktop aspect/resolution, the interface's supported text-size extremes, expanded/collapsed panels, longest localized strings in available content, keyboard focus, opacity/high-contrast options, all blocking/transitional states. Additional resolutions are tracked as coverage decisions, not new player settings.

## 6. In-match HUD standard

The existing HUD uses `VeyraGreyboxHud`/Canvas and model classes; production migration should **retain HUD state calculations/models** (`VeyraHudModel`, minimap, bars, target frame, text, warnings) while replacing presentation incrementally with composable UI/material layers. Avoid maintaining two separate gameplay-facing calculations.

- Production-quality glyph/font selection, scalable icon treatment, legible bars, consistent edge alignment, and predictable layering.
- Health, shields, resources, cooldowns, item actives, Flux Spell states, Team Flux, selected target, minimap, chat, alerts and kill feed must remain immediately understandable.
- Preserve approved **independent component scaling**, safe-area axes, fixed anchor design (no draggable layout editor), high-contrast states, status icon meaning, cooldown modes and combat-text density.
- Fog/stealth/readability invariant: if a unit/status is not visible or its timer is not authorized, HUD/minimap must not hint it.
- Use controlled update frequency; reserve per-frame work for what truly changes each frame, not labels and layouts. Profile Slate, overdraw and any world-space bar/indicator meshes.
- Avoid cover-ups: decorative frames must not occlude threats, objective entrances, projectiles, cast signals or minimap information.

**Gate:** a side-by-side before/after playable 5v5 capture must show improved hierarchy without losing any existing HUD semantics or accessibility behavior.

## 7. Vanguard models, animation and personal presentation

Base models must match approved **in-game silhouette**, not the painting's brush detail. ADR-069's script-sculpted, stylized low-poly approach is valid; higher quality comes from correct primary volumes, confident proportions, material/lighting quality, motion, and secondary movement—not simply more triangles or texture resolution.

- Derive model from author-approved base artwork and script source; capture front/side/three-quarter, game zoom, and silhouette-only review.
- Distinct resting, moving, striking, casting, hit, idle, death and recall poses; bodies do not permanently hold a cast/aim pose at rest.
- Skill-specific clips/ability staging as in ADR-072. Anticipation/release/follow-through follow authoritative cast phase/commit, cancellation, and channel state; never move damage authority to animation.
- Root motion and character movement remain consistent with server/client movement contracts; test scale and stride against drawn size to prevent sliding (ADR-071).
- Loose hair/cloth/body parts may use secondary motion when affordable and collision-safe. Fall back cleanly when disabled or off-camera.
- Toon shading must remain readable against both bright and dark biomes; custom-depth ink and hover outlines must not interfere. Test occlusion, fog, stealth, impact overlays and each side colour.
- Prefer one clear signature on a Vanguard's cast over simultaneous full-screen bursts.

**Gate:** an author-approved, correctly posed, recognizable Vanguard at competitive zoom, with all relevant skill stages readable in motion, varied lighting, and measured multi-Vanguard cost.

## 8. Combat VFX, world VFX and audiovisual feedback

Use Niagara and shared presentation materials under the Asset & VFX Pipeline; parameterize by ability, effect stage, side, surface, size, density and scalability. Avoid one-off near-duplicate effects.

- Separate pre-commit telegraphs from committed impact. Graphic silhouette, directional flow, time progression and sound must communicate ability identity.
- Ability stages: windup, channel, commit, travel and impact where relevant, plus status, shield, death, recall and structure/Flux Well reactions. Reuse ADR-072's staged effect/event integration rather than bypassing it.
- The visual edge of dangerous areas must agree with the actual authored range/shape and remain visible over water, riverbanks, shadows and spell clutter. Reproject ground geometry to real terrain where needed.
- Set budgets for translucent overdraw, spawn count, light emitters, audio voices, post-process cost and concurrent effects **in measured quality profiles**. Avoid persistent screenwide bloom, thick fog over combat, or unbounded particle emitters.
- Effects must stop cleanly on cancellation, despawn, fog/relevancy changes and scene transitions. Pool only when lifetimes and reset behavior are proven.
- Coordinated audio: differentiated ready/commit/hit/blocked/kill states with no audible confirmation of an unseen or uncommitted outcome; intelligible at dense 5v5 mix, with accessibility and user volume preferences respected.
- Restrained environmental life (flowing water, Flux pulses, particles, foliage movement) reinforces setting without fighting targets for contrast.

**Gate:** player's intent, threatening area, impact/result and counterplay remain distinguishable in screenshots **and** motion with 5v5 ability overlap; no false information.

## 9. Crucible environment and rendering strategy

The map's layout authority and generation chain remain **Battleground Bible → `Game/Tuning/World.json` → commandlet/script → `L_Battleground.umap`**. Produce a vertical slice before broad dressing: a river crossing, lane–jungle junction or objective approach that represents terrain, water, cover, structure, foliage, character and combat together.

- Landscape for continuous ground, Blender for authored formations, constrained PCG for dressing; follow the World Production Bible's protected topology.
- Large/mid/small shape hierarchy; landmark silhouettes and navigable negative space come before micro-detail. River is a shaped part of the terrain, not a flat decorative line.
- Shared material families should express consistent albedo value, roughness variation, normal intensity, macro breakup, grime/erosion and wetness without distracting noise or illegible ground.
- Lighting: stable game-time exposure, coherent sun/sky direction, controlled ambient and shadows, intentional atmosphere. Check bright and dark terrain and all side palettes at **match camera distance**.
- Optional features (Nanite for suitable meshes, Lumen/reflection methods, shadow methods, TSR/upscalers, foliage/rendering paths) need side-by-side quality and cost tests. Feature availability is not an art-direction mandate. No feature is "free" solely because UE 5.8.3 supports it.
- Different quality tiers may remove decorative detail, reflection cost, ambient effects or distant foliage, **never** remove an essential telegraph, combat readability, authorized vision rule or threat cue.

**Gate:** all paths/clearances/sightlines and original gameplay rules survive, while the finished slice is recognizable, coherent and performant.

## 10. Performance, profiling and scalability

**Existing first-pass author target:** 120 FPS on Intel Core i7-12700KF / GeForce RTX 3070 Ti / 32 GB (World Validation §23.1); per-frame budget is about **8.33 ms**, not a blanket approval of any individual renderer feature. Record resolution, internal render scale, graphics preset, screen state, duration and scene seed. This does **not** define release minimum specs.

**Baseline before changes:** capture cold/warm shader conditions separately; compare packaged/cooked builds where possible instead of relying solely on PIE. Profile with `stat unit`, `stat gpu`, Unreal Insights (CPU/GPU/Slate as applicable), GPU visualizer, Niagara and texture/memory views. State which hardware/driver/build was used.

**Measurement matrix (minimum for substantial changes):**

| Situation | Why it exists |
|---|---|
| Home, roster, selection, shop with panels open/closed | layout/Slate/scene-capture cost |
| Quiet lane and jungle | baseline materials/shadows/foliage |
| River crossing or Flux Well | water/reflections/lighting/VFX |
| Full 5v5 with waves and overlapping abilities | worst-case game and particle frame time |
| Targeted scalability reduction | ensures essential visibility is not tied to luxury features |

Record CPU game/render and GPU times where available, median and tail frame times (prefer p95/p99), frame hitches, peak memory, draw calls/overdraw where diagnostic, and shader-compilation stalls separately. Compare identical seeds/camera, resolution, combat sequence and render settings. Regressions must be quantified with a cause and an accepted tradeoff or optimization.

**Quality profiles:** decide approved presets per target hardware; make renderer choices a tested profile matrix, not a single global switch. A setting may be unavailable on some paths; fall back predictably. No silent resolution changes during benchmark comparisons.

Official engine reference notes: Lumen's Epic/High scalability profiles are designed around substantially lower frame-rate targets than Veyra's first-pass 120 FPS machine goal; measure before enabling at scale. Slate/UMG invalidation is often preferable to Retainer Panels unless measured draw-call savings justify render-target and repaint cost. See:
- [UE 5.8 Lumen Performance Guide](https://dev.epicgames.com/documentation/en-us/unreal-engine/lumen-performance-guide-for-unreal-engine)
- [UE 5.8 UI Optimization](https://dev.epicgames.com/documentation/en-us/unreal-engine/optimization-guidelines-for-umg-in-unreal-engine)
- [UE 5.8 Invalidation](https://dev.epicgames.com/documentation/en-us/unreal-engine/invalidation-in-slate-and-umg-for-unreal-engine)
- [UE 5.8 Unreal Insights Trace Quick Start](https://dev.epicgames.com/documentation/en-us/unreal-engine/trace-quick-start-guide-in-unreal-engine)
- [UE 5.8 Nanite](https://dev.epicgames.com/documentation/en-us/unreal-engine/nanite-in-unreal-engine)

## 11. Screenshot/motion regression coverage

Standardize scene/camera state and store each baseline with: code/content revision, map, build type, camera transform/zoom, viewport resolution, render scale, graphics profile, device, time/lighting, selected Vanguard(s), side, status, ability/event timeline, world seed and recording identifier.

**Required capture set per scope:**

| Surface | Capture moments | Checks |
|---|---|---|
| Client Home/Play/social | panel collapsed/expanded, queue active, modal on top | layout, focus, priority overlay, text |
| Select / Collection | selected Vanguard, locked/waiting, timer near expiry | approved arrangement, art, visual state |
| In-match HUD | quiet lane, selected target, shop, multi-status, full 5v5 | bars, clutter, read order, safe area |
| Vanguard | neutral/locomotion/specific ability/impact/death | silhouette, secondary motion, ink/shadows |
| Combat | windup, commit, travel, impact, cancellation | truthful timing, team colour, counterplay |
| Crucible | lane, junction, river, Well, jungle, base, dense effects | readability, water/materials, paths |
| Accessibility | high contrast, alternate side palette, reduced motion, larger text | functional equivalence |

Minimum practical evidence: paired same-camera before/after frames **and** a short in-engine motion capture for animated work; add an actual packaged-build capture for wide changes. Use existing `Game/Scripts/CapturePresentation.ps1` and World Validation capture tools when applicable; extend rather than create unrelated manual image workflows. If an automation/capture mode does not yet exist, record it as a follow-up; do not fabricate screenshots or claim the run happened.

**Review grades:**
- **Blocker:** inaccurate gameplay/vision, unreadable threat, broken client state, accessibility control missing, reproducibility or major perf regression.
- **Major:** silhouette/visual identity drift, pervasive contrast or scale failure, competing effects that obscure play, unexplained severe hitches.
- **Minor:** local alignment, restrained polish, secondary ambience or optional motion mismatch.

No unresolved blocker or major defect may pass production visual acceptance without a named, explicitly approved exception. Pixel-perfect screenshot diffs are advisory on animated/rasterized scenes; use comparable regions, art review, and measurement as well.

## 12. Agent handoff and change template

Before touching files, an agent must report the **scope, owner, reference, intended visual change, preservation rules, tools, measurement plan and affected generators**. Build only the smallest reviewable vertical slice; do not mass-produce a whole roster from an unapproved treatment.

For each production PR, use [Visual Review Template](VEYRA_VISUAL_REVIEW_TEMPLATE.md). Include:
- author-approved visual reference and any unresolved art decisions;
- source files/assets/generators, generated outputs, exact regeneration steps;
- before/after captures under matched settings; motion review where applicable;
- build/cook/test outcomes and reproducible profiler/trace evidence;
- UX/accessibility/vision/terrain integrity checks;
- rollback plan, ownership, reviewer decision and defects.

**Acceptance rule:** generated =/= integrated =/= visually reviewed =/= approved. Use those as separate status labels. An agent may say "ready for author review" only after the test/capture evidence is present; only the author can sign off on signature art direction.

## 13. Next actions and unresolved choices

Follow the [Visual Upgrade Roadmap](VEYRA_VISUAL_UPGRADE_ROADMAP.md), beginning with a baseline audit of current UI, HUD, rendering and Vanguard presentation, then one client/HUD/world/Vanguard slice.

Open decisions to obtain explicitly when work reaches them:
- which client screen and Crucible region will be the first production reference slice;
- final typography, UI visual token/palette values and presentation asset source formats;
- whether to adopt Common UI, and whether to stage a larger UMG migration from C++ widget-tree assembly;
- hardware/spec matrix and shipping frame-time requirements beyond the existing author-PC first pass;
- any default rendering feature/profile changes with material architecture or performance implications;
- approved human visual sign-off on the first slice, before mass rollout.

Do **not** infer approval for these decisions from this standard.
