# Art direction and the art pipeline

**Version:** 0.1
**Date:** 2026-09-20
**Supersedes:** `Sheet_Generation_Prompt_v0.1.md`, which prompted for a whole sheet with its text baked in. [`Vanguards/render_sheet.py`](Vanguards/render_sheet.py) now owns the text, so only the artwork needs generating.

## Where the prompts are

**Not in this file.** They are generated, so they cannot go stale:

```
python3 Docs/Design/Vanguards/render_sheet.py --prompts bryn
python3 Docs/Design/Vanguards/render_sheet.py --prompts --all
python3 Docs/Design/Vanguards/render_sheet.py --missing
```

Each prompt is emitted with that Vanguard's appearance paragraph, signature colour, region and antagonist direction already injected from the Character Bible and their YAML. A rename or a rewrite changes the prompts automatically. A prompt pasted into a document would not.

`--missing` lists the art each Vanguard still needs, as a work order.

## House style

**Painterly realism with one saturated signature hue** — the rendering of Patch's sheet with the colour discipline of Tavi's.

Painterly and heavily rendered; not photoreal, not flat cel, not anime. Materials read physically. Every image for a Vanguard is built around **one saturated signature colour** that drives the key light and the accents, with everything else desaturated so that hue carries the frame. Strong directional key plus an opposing rim, deep shadows, high contrast, mid-to-dark value key. Silhouette readable at thumbnail size — this is a MOBA, and the shape identifies the character before the detail does.

Never in the artwork: text, lettering, logos, UI, watermarks, signatures, lens flare, stat bars, or numbers of any kind. Tuning belongs in data per `ARCHITECTURE.md` §1.3, and sheets that printed `Tracked for 4 seconds` into their pixels are why this is written down.

### Signature colours

Assigned by region so no two sheets collide in a draft grid. Held in `HUE` in `render_sheet.py` and injected into every prompt.

| Region | Hue | | Region | Hue |
|---|---|---|---|---|
| Iron March | `#c4552b` | | Ember Basin | `#ff7a1a` |
| Drowned Cantons | `#2e8b9e` | | Bramblehollow | `#7fa650` |
| Merrin | `#c8536f` | | Wildwood | `#a8c64a` |
| Buried Riverlands | `#c19a4b` | | Reed Provinces | `#c0392b` |
| Shatterdeep | `#6fc3d4` | | Flux Network / Fluxborn | `#4a8fd4` |

## Workflow

**Generate `hero` first, approve one variation, then use that image as a reference for the other twelve.** This is the whole trick to consistency. Without a reference the twelve slots produce twelve different people wearing similar clothes.

Use a text-to-image model for the hero and a reference-driven model for everything after it. Save results to `ConceptArt/Vanguards/<id>/<stem>.png` and re-render the sheet; slots fill automatically when the file appears, and show a labelled work order when it does not.

## The thirteen slots

| Stem | Aspect | Purpose |
|---|---|---|
| `hero` | 3:4 | Full-figure illustration, signature key light, region behind the subject |
| `front` `back` `side` | 1:2 | Orthographic A-pose turnaround, flat even light, plain background — modelling reference, not illustration |
| `scale` | 1:2 | Flat silhouette beside a 1.8 m human silhouette |
| `idle` `move` `cast` `ult` | 1:1 | Three-quarter top-down gameplay camera, silhouette readability first |
| `portrait` `weapon` `material` `signature` | 1:1 | Close crops |

**That is 325 images for the full roster.** If that is too much, the tiering that keeps the sheets useful is: `hero` alone makes a sheet presentable; `hero` + turnaround makes it buildable; the in-game views matter most for characters whose readability is in question; the detail crops are the first thing to cut.

## Before generating anything — a blocker

**Only 8 of 25 Vanguards have an appearance paragraph in canon.** Cairn, Bryn, Oriel, Mimzi, Celandrine, Gorraveth, Aurelisse and Eudora carry a `Visual` or `Visual language` field in the Character Bible. The other **seventeen do not**, and for them the prompt says so rather than inventing a look.

For those seventeen, the only record of how the character looks is the concept sheet itself. That is the same failure as text baked into an image, in a more expensive form: **the design exists only as pixels, and nothing can validate, diff or correct it.**

Three of them — **Raska, Kade and Angeru** — have no surviving sheet either, since those were withdrawn for third-party content. Their appearance is currently recorded nowhere at all.

Two consequences worth deciding deliberately:

1. **For the fourteen with surviving sheets**, write the appearance paragraph into the Character Bible *before* regenerating, reading it off the existing art. That captures the design into text while the art still exists to read it from.
2. **For Raska, Kade and Angeru**, reconstructing the old look would carry forward designs developed against third-party reference material — the reason those sheets were withdrawn. A clean redesign is the safer path, and it is also the honest one.
