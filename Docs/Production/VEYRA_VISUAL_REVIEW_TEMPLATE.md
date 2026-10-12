# Veyra Visual Review — reusable task/PR template

> Copy this into the task or PR description for **substantial UI, HUD, Vanguard, animation, material, Niagara, environment, lighting, rendering, and audio-visual** work. See [Visual Production Standard](VEYRA_VISUAL_PRODUCTION_STANDARD.md) and [Visual Upgrade Roadmap](VEYRA_VISUAL_UPGRADE_ROADMAP.md). Do not describe a missing capture or test as complete.

## Scope and ownership

- **Feature / region / screen / Vanguard / ability:**
- **Proposed presentation change:**
- **Authoritative canon:** (specific Veyra bible/section, approved art, accepted ADRs)
- **Existing source/generator/module being modified:**
- **Generated/shipping assets affected:**
- **Explicit non-goals and preserved behavior:** (including authoritative gameplay, approved UI flow and accessibility)
- **Proposed visual treatment or already approved?**
- **Reviewer for signature appearance decisions:**

## Before implementation

- [ ] Confirmed `AGENTS.md`, `ARCHITECTURE.md`, relevant canon, `Docs/Production/README.md` and applicable ADRs.
- [ ] Read current implementation and available shared styles, generators, clips and VFX; no duplicate owner introduced.
- [ ] Verified asset provenance/AI-use permissions where third-party material is involved.
- [ ] Captured baseline **from the actual engine** (or recorded why capture tooling is missing).
- [ ] Defined comparison environment and profiling scene.
- [ ] Identified gameplay correctness, vision/readability and accessibility invariants.
- [ ] Any needed engine/plugin/UI-architecture change has explicit decision before work.

## Visual acceptance criteria (write observable evidence)

1. **Identity and cohesion:**
2. **Composition / hierarchy at intended camera or UI size:**
3. **Interaction states / animation / timing:**
4. **Competitive clarity, truthful telegraphs and no information leakage:**
5. **Accessibility / scaling / colour and motion variants:**
6. **Performance budget and quality-tier fallback:**
7. **Reproduction/source and rollback:**

## Controlled capture table

| Capture ID | Build/commit | Map/screen/state | Camera/zoom | Resolution, render scale, preset | Actors/ability phase | Before | After |
|---|---|---|---|---|---|---|---|
| baseline-01 | | | | | | | |
| stress-01 | | | | | | | |
| accessibility-01 | | | | | | | |

Record device/driver/OS and lighting or world seed when relevant. For motion, include timestamped clip or timeline and the committed ability phase. Do not compare different resolutions, scripts or graphics presets as though they are the same test.

## Verification report

| Check | Result | Artifact / command / measurement / exception |
|---|---|---|
| C++ build / generated asset import | Not run / Pass / Fail | |
| Relevant automated tests / architecture checks | Not run / Pass / Fail | |
| World/nav/vision/ability timing preserved | Not applicable / Pass / Fail | |
| Client states / HUD model / input preserved | Not applicable / Pass / Fail | |
| Text size, safe area, contrast, reduced motion | Not applicable / Pass / Fail | |
| Packaged/cooked validation | Not run / Pass / Fail | |
| GPU/CPU/Slate/Niagara profile and p95/p99 times | Not run / Pass / Fail | |
| 5v5 or worst UI density | Not run / Pass / Fail | |
| Matched before/after screenshots and motion | Missing / Present | |

**Performance comparison:** hardware, resolution, render scale, build type, driver, preset, scene and duration; CPU/GPU median + tail frame-time before/after; largest regression and suspected owner; scalable fallback and measured effect.

**Known defects:** classify each **Blocker / Major / Minor** under the Visual Production Standard. Provide a screenshot/repro and owner. Do not close a blocker or major defect without a documented approved exception.

**Regeneration:** exact scripts/parameters, generated asset paths, version/seed, asset provenance.  
**Rollback:** branch/revision or generator inputs to restore, and impact on generated output.  
**Technical reviewer:** name and decision.  
**Author art-direction reviewer:** **Approved / Changes requested / Pending** (include dated feedback).  

## Final state (select one, truthfully)

- [ ] Source/generator implemented, **not yet integrated**
- [ ] In-engine integrated, **not yet visually reviewed**
- [ ] Reviewed with captures, **awaiting author approval**
- [ ] Author-approved and passed required production gates
- [ ] Rejected/deferred, with next owner and reason

**Never use "production-ready" as a substitute for these explicit states.**
