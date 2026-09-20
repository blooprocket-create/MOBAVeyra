# Concept art archives

This directory preserves superseded Veyra concept art.

These files are **historical reference only**. They are useful for tracing how a design looked
before it was replaced, and for checking what an earlier decision was actually based on. They are
**not** current reference and must not be used as input to modelling, VFX, ability implementation
or balance work.

Current Vanguard artwork lives one level up in [`../Vanguards/`](../Vanguards/), one directory per
Vanguard. The rendered sheets that use it live in
[`../../Docs/Design/Vanguards/sheets/`](../../Docs/Design/Vanguards/sheets/).

This mirrors [`Docs/Design/Archives/`](../../Docs/Design/Archives/), which does the same job for
superseded design bibles.

## Contents

- [`Characters/`](Characters/) — the 22 baked-text character sheets that preceded the current
  pipeline. Their README explains why they were retired and lists the conflicts each one carried.

## Why they were moved here rather than deleted

Two reasons, and the second is the important one.

They are the design's visual history, and the
[discrepancy register](../../Docs/Design/Sheet_Canon_Discrepancy_Register_v0.1.md) still catalogues
them entry by entry, so deleting them would strand every reference in it.

More practically: removing them from the repository does not remove them from it. Git keeps every
blob any commit ever referenced, so a deletion commit reclaims nothing — only a history rewrite
does, and one was already performed on this repository to remove genuine third-party material. A
second rewrite for material Veyra owns outright would invalidate every clone again to save space
that a shallow clone does not pay for anyway.

**What moving them does achieve** is the thing a README could not. `Characters/` used to sit
directly beside `Vanguards/`, two sibling directories with no way to tell which was current without
opening a file. That is the same failure these sheets were retired for: a finished-looking artifact
is more persuasive than a paragraph of Markdown, so the wrong one gets used. The path now says
which is which.
