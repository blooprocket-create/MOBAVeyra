# Concept art archives

This directory preserves superseded Veyra concept art.

These files are **historical reference only**. They are useful for tracing how a design looked
before it was replaced, and for checking what an earlier decision was actually based on. They are
**not** current reference and must not be used as input to modelling, VFX, ability implementation
or balance work.

Current Vanguard artwork lives one level up in [`../Vanguards/`](../Vanguards/), one directory per
Vanguard. The rendered sheets that use it live in
[`../../Docs/Design/Vanguards/sheets/`](../../Docs/Design/Vanguards/sheets/), all 25 committed.

This mirrors [`Docs/Design/Archives/`](../../Docs/Design/Archives/), which does the same job for
superseded design bibles.

## Contents

- [`Vanguards/<id>/`](Vanguards/) — superseded hero illustrations, one directory per Vanguard,
  mirroring `../Vanguards/`. Named `superseded_gen<N>[_<reason>].webp`; the generation number
  orders them. `gen1_a/b/c` are the original machine-generated variations every authored hero
  replaced; the four later supersessions and their reasons are tabled in
  [`../Vanguards/README.md`](../Vanguards/README.md#superseded-images). Moved here from beside
  each `hero.webp` on 2026-09-23 so the current-art directories hold only current work.
  `render_sheet.py --missing` reads `superseded_gen1_a.webp` here to list which heroes are
  authored replacements.

The 22 baked-text character sheets that preceded the current pipeline are **not** here: they were
deleted on 2026-09-21, at the author's instruction, once every Vanguard carried authored hero art
and every conflict the sheets raised had been resolved against it. See below.

Superseded concept art belongs here, beside [`Docs/Design/Archives/`](../../Docs/Design/Archives/)
for superseded bibles, and not next to current work.

## What happened to them, and what survives

Each Vanguard's YAML records `sheet.status: superseded` with `sheet.file: null`, and keeps its
`register_refs`. So the record of which sheet carried which conflict is intact even though the
image is not — the
[discrepancy register](../../Docs/Design/Sheet_Canon_Discrepancy_Register_v0.1.md) still describes
every one of them, section by section.

`superseded` is a distinct status from `withdrawn` on purpose. **Withdrawn** is the three sheets
pulled for third-party content — Raska, Kade and Angeru — which were also stripped from Git
history. **Superseded** is these twenty-two, retired because better art replaced them. Both end
with `file: null`, and collapsing them into one label would lose why the file is absent.

## Deleting them did not reclaim the space, and that is expected

Git keeps every blob any commit referenced, so the roughly 55 MB stays in the pack. Only a history
rewrite reclaims it, and one is not justified here: the first rewrite existed to remove genuine
third-party material, and a second one for material Veyra owns outright would invalidate every
clone to save space that costs nothing to carry.

What the deletion achieves is a working tree that contains only current work.
