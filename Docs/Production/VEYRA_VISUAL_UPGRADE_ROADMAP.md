# Veyra Visual Upgrade Roadmap

**Version:** 1.0 | **Date:** 2026-10-09  
**Status:** Proposed execution plan, **not** authorization to change engine strategy, canon, or approved UX  
**Read first:** [Visual Production Standard](VEYRA_VISUAL_PRODUCTION_STANDARD.md), [Production index](README.md), [Art Direction](../Design/Art_Direction_v0.1.md), [Architecture](../../ARCHITECTURE.md), [Context Map](../CONTEXT_MAP.md).

## Objective

Bring Veyra's entire Unreal presentation from functional prototype to consistent production-release quality **without throwing away the server-authoritative gameplay implementation or sacrificing competitive clarity**. Do not attempt to "AAA polish everything" in one PR. Establish a real-world visual quality bar with a small integrated slice, verify it, and replicate **its reusable systems**.

A stage is complete only after in-engine evidence, performance and UX regression tests, plus the author's visual approval where art direction changes. Existing design bibles control actual rules; this plan controls sequence.

## Verified starting point (repo audit, 2026-10-09)

| Surface | Existing foundation | Primary gap to assess |
|---|---|---|
| Client | `VeyraUI/Private/Shell/` C++-assembled UMG, shared `VeyraShellStyle` / `VeyraShellLook`, client-intent boundary | Art-directable components, consistent surface treatments, typography, animation and complete state coverage |
| Match HUD | `VeyraUI/Private/Hud/`, `VeyraGreyboxHud` Canvas and dedicated HUD models | Composable production presentation that preserves current HUD data and player settings |
| Vanguard appearance | `VanguardBodies` generators; ADR-064, ADR-068 and ADR-069 model/shader system | Silhouette/animation quality and staged detail, as judged at match zoom |
| Skills/VFX | ADR-063/068/071/072, staged effects introduced via kit presentation | Personal ability signatures, truthful impact, consistent shared effect art and 5v5 readability |
| World | `VeyraWorldTools`, `World.json`, `L_Battleground`, World Production/Validation guides | Cohesive finished region with terrain, water, composition, atmospheric lighting and tested scalability |
| Renderer | project config includes custom-depth and motion-blur policy; other choices need a runtime audit | Evidence-based renderer/scalability profiles rather than undocumented quality switches |

These are **code/documentation observations**, not an assertion that their current visual result has been photographed and approved. Proposed ADRs must not be presented as merged/accepted merely because a file exists.

## Stage 0 — Inventory, baseline and defects

**Output:** an actionable baseline report with reproducible captures; no reskin.

1. Inventory client screen implementations, HUD drawing paths, UI materials/fonts/styles, asset/generator families, world lighting/render configuration, Niagara/VFX and animation status. Distinguish authored assets, generated assets, placeholders and missing assets.
2. Capture: Home/Play/selection/shop/results; normal and expanded social/party; gameplay lane/jungle/river/Well/base; 5v5 stress; representative Vanguards, Patch's tether, Cairn/Oriel/Varkesh skill stages. Include reduced-motion/high-contrast/large-text states.
3. Collect packaged-build CPU/GPU/Slate/particle and streaming traces on the existing first-pass author PC. Record resolution, preset, render scale and workload.
4. Record a **visual defect backlog** with severity, scope, root owner, screenshots and recommended remediation. Separate polish defects from gameplay/readability defects.
5. Catalog only **approved** reference art. Do not introduce outside assets without provenance/AI-rights checks.

**Exit:** the author can see exactly what is placeholder, visually inconsistent, technically broken, and expensive; reference captures and an approved first slice are selected.

## Stage 1 — Shared production presentation foundation

**Output:** reusable systems, not a broad client rewrite.

- UI: define visual tokens (palette roles, type ramp, spacing, radii, focus, surface elevation, reusable brushes, nine-slice/textures and component states) in art-directable settings/assets. Integrate with the existing `VeyraShellStyle` and client models.
- HUD: define reusable bar/icon/counter/tooltip primitives; preserve the current HUD models, individual scales and fixed anchors. Do not copy game-calculation logic into widgets.
- Characters/effects: stabilize shader/effect master materials and parameter naming; stage-specific skill VFX recipe; ability animation review and comparison harness.
- World: shared environment material functions and a small material test scene; define lighting/exposure approach and foliage/stone/water sample set.
- Capture: one repeatable before/after harness for a selected client screen and gameplay area, reusing existing world/presentation capture scripts.
- Renderer: evaluate performance/value of existing rendering options, Nanite candidates, illumination/reflections and UI invalidation. No blanket Lumen/Nanite/Common UI enabling.

**Exit:** a coherent style dictionary can be applied to new assets/screens without creating a second implementation of gameplay state or an unmaintainable proliferation of materials.

## Stage 2 — First vertically integrated reference slice

**Suggested pilot (requires author's selection):**
- **Client:** Home/Play with intact friends/chat sidebar and party strip; full modal/queue/expanded state.
- **Selection:** approved champion-select layout with one featured Vanguard, countdown, loadout and accessibility cases.
- **Match HUD:** selected target, ability bar, items/Flux Spells, minimap, health/resource/Team Flux, warnings, status/cooldown rules.
- **Crucible:** one river-to-jungle/objective approach, preserving `World.json` layout and collision/nav/vision.
- **Vanguards:** Cairn, Oriel and Varkesh already used for staged-skill animation; use a small subset first if resources demand. Include a readable 5v5 with Fluxborn to ensure the treatment scales.

Deliver **real Unreal captures and short motion clips**, not solely concept art. Compare the same gameplay states and settings before/after. Make author sign-off an explicit gate for the look, not just compile success.

**Exit criteria:**
- Production-quality material, UI and VFX treatments are consistent in one scene.
- Selection and social/party behavior matches approved UX; in-match HUD semantics unchanged.
- No visual misinformation under fog, Dense Fog, occlusion or high effect overlap.
- Baseline and candidate frame-time traces identify any cost and show a documented optimization/scalability response.
- Reproducible source files, regeneration and rollback documented.

## Stage 3 — Controlled horizontal rollout

Convert the signed-off reference slice into reusable **asset family + UI component + animation/effect** coverage. Work one bounded family at a time:

| Workstream | Order | Independent review |
|---|---|---|
| Client | Home/Play → selection → Collection/Shop → profile/history/results/loading/settings | standard/exceptional/empty/error/transient states and priority dialogs |
| HUD | bar/shell treatment → individual HUD components → shop/minimap → combat text/warnings | 5v5, high text scale, colour variations, safe-area |
| World | one source-region family at a time: river/bridges, lanes, jungle, objectives, bases | topology and sightline validation, then visual/performance |
| Vanguards | small groups based on silhouette/rig/archetype | individual kit signature, game zoom, motion, 5v5 compatibility |
| Abilities | reusable windup/commit/travel/impact families followed by per-skill detail | cue truthfulness, sound timing, clutter/scalability |
| Performance | measured profile matrix at every integration boundary | frame-time/memory/streaming snapshots, fallback visuals |

Do not mass regenerate all Vanguards, rebuild every UMG screen, or reseed the whole map until the respective pilot survives both visual and automated checks.

## Stage 4 — Production hardening and ship-readiness

1. Exhaustive screen state review and keyboard/gamepad/focus where supported; long localization strings, text size extremes, reduced motion, contrast and safe areas.
2. Shader/PSO stutter audit, loading transitions, animation event sync, audio mixing, and texture streaming/memory pressure.
3. Gameplay camera visibility sweeps at typical and worst zoom; night/dark/bright material contexts; adversarial colour palettes; 5v5 stress.
4. Quality tiers tested on a defined hardware and render-path matrix. Decorative effects may scale; critical telemetry, combat indicators and fog correctness may not.
5. Cooked/packaged QA plus regression capture comparison and rollback rehearsals.

**Exit:** no blocker/major visual defects without documented author exception; reference frames, tests, quality profiles, and the signed-off production target are versioned.

## Proposed task structure

Make small, independently reviewable tasks, e.g.:

- `Visual/BaselineClient`: enumerate screens/states, record captures and defects.
- `Visual/SharedTokens`: create presentation style assets and bridge them to C++.
- `Visual/HomeSlice`: transform Home while preserving existing view-model and shell panel behavior.
- `Visual/HudSlice`: migrate one HUD component from Canvas to reusable renderer and verify identical state.
- `Visual/CrucibleRiverSlice`: improve material/landscape/water/art in a protected geography with navigation screenshots.
- `Visual/VanguardSkillSlice`: polish one Vanguard's clips and staged FX with gameplay evidence.
- `Visual/ScalabilityAudit`: capture GPU/CPU tradeoffs per graphics mode on repeatable stress cases.

For each task follow [Visual Review Template](VEYRA_VISUAL_REVIEW_TEMPLATE.md), link the exact relevant bible/ADR, and complete the owning test suite. **No broad exploratory art-production PR can claim completion without a reviewed in-engine result.**

## Explicit decisions still pending

The author needs to choose the first slice(s), visual token look, whether larger interface technology changes are worthwhile, and hardware/visual profile tradeoffs. Treat these as review gates, **not** as already decided by this plan.

**Never trade server correctness, champion identity, protected gameplay geometry, approved client behavior or accessibility for visual polish.**
