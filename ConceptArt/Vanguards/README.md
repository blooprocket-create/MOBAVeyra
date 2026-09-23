# Vanguard artwork

One directory per Vanguard, keyed by the same id used by
[`Docs/Design/Vanguards/<nn>-<id>.yaml`](../../Docs/Design/Vanguards/). All 25 Vanguards have an
authored hero image, and each one's `**Visual language:**` paragraph in the Character Bible has been
reconciled against it.

**This directory holds artwork only.** The rendered sheets that use it live in [`Docs/Design/Vanguards/sheets/`](../../Docs/Design/Vanguards/sheets/), all 25 committed. No text is baked into these images. Sheets are composed from
canon at render time by [`render_sheet.py`](../../Docs/Design/Vanguards/render_sheet.py), so a
rename or a rewrite in the bible reaches the sheet on the next render and artwork can never carry a
stale ability name or a tuning value. The superseded baked-text sheets are what happened without that separation; they were deleted on
2026-09-21, and [`../Archives/`](../Archives/) records what they were.

## Alternate cosmetic concept art

The canonical `hero.webp` in each Vanguard directory remains the base appearance and modelling reference. Alternate cosmetic looks live separately under `<id>/skins/<collection-slug>/hero.webp` and are indexed in [`skins/README.md`](skins/README.md) and [`skins/index.json`](skins/index.json). A skin illustration is visual exploration only: it does not replace canon appearance, introduce a gameplay effect, or signal a released/owned cosmetic. The skin catalog and the base hero sheet are intentionally separate.

## File names

A slot is filled the moment its file appears; the renderer picks it up with no configuration.

| Name | What it is |
|---|---|
| `hero.webp` | The hero illustration. The only slot currently filled for most Vanguards. |
| `front` `back` `side` `scale` | Orthographic turnaround and scale silhouette — modelling reference, flat even light. |
| `portrait` `weapon` `material` `signature` | Close detail crops. |
| `idle` `move` `cast` `ult` | In-game views. **Captured from the running game, never generated** — a generated picture of a gameplay camera cannot answer the one question these slots exist for. |

`.png`, `.jpg` and `.webp` all resolve. WebP is what the repository stores: the heroes were 152 MB
as PNG and are 14 MB as WebP.

### Superseded images

A replaced image is **moved, never overwritten**, to
[`../Archives/Vanguards/<id>/`](../Archives/Vanguards/) as `superseded_gen<N>[_<reason>].webp`.
Overwriting would lose the ability to see what was rejected and why, which matters when the same
character gets regenerated more than once; archiving keeps this directory to current work only.
`gen1_a/b/c` are the original machine-generated variations that every authored hero replaced.
Beyond those, four images have been superseded with a reason worth recording (paths relative to
`ConceptArt/Archives/Vanguards/`):

| File | Superseded because |
|---|---|
| `varkesh/superseded_gen2_blade.webp` | Gave him a two-handed blade while his kit is entirely ranged and zone. Regenerated weaponless. |
| `neris/superseded_gen2_rock.webp` | Put her on wet rock, and had converged almost exactly with Sylra's silhouette. Regenerated riding the wave her Q is built on. |
| `tavi/superseded_gen2_anime.webp` | Replaced by a stronger composition. |
| `tavi/superseded_gen3.webp` | Replaced again; the cross mark lands far harder in the current image. |

The reason suffix is optional — the generation number is what orders them.

## Outstanding work

```
python3 Docs/Design/Vanguards/render_sheet.py --missing
```

reports what each Vanguard still needs and counts generated artwork separately from the in-game
views awaiting a build to capture from.

**Generate the hero first and use it as the reference for the other eight slots.** That is the whole
trick to consistency; without a reference the eight slots produce eight different people wearing
similar clothes.

Prompts are written by hand, outside this repository. Nothing here generates them: the renderer
owns the sheet's text and fills a slot from whatever image file is present.

```
python3 Docs/Design/Vanguards/render_sheet.py --all       # re-render the sheets
python3 Docs/Design/Vanguards/render_sheet.py --missing    # which slots have no art yet
```

House style, the signature-colour table, the per-character rendering idiom and the measured
reasoning behind all of it live in
[`Docs/Design/Art_Direction_v0.1.md`](../../Docs/Design/Art_Direction_v0.1.md).
