# Superseded character sheets — historical reference

**Status as of 2026-09-21: these sheets are no longer current reference.**

Current Vanguard artwork lives in [`../Vanguards/`](../Vanguards/), one directory per Vanguard, and
all 25 now carry an authored hero image whose appearance paragraph has been reconciled with it.
The pipeline is described in [`Docs/Design/Art_Direction_v0.1.md`](../../Docs/Design/Art_Direction_v0.1.md).

This directory is kept because the sheets are the design's visual history and because the
[discrepancy register](../../Docs/Design/Sheet_Canon_Discrepancy_Register_v0.1.md) still
catalogues them entry by entry. **Do not use them as input to modelling, VFX, ability
implementation or balance work.**

## Why they were retired

These sheets baked their text into the artwork — ability names, descriptions, and in at least one
case a tuning value. That is the failure the current pipeline exists to prevent: a finished-looking
sheet is far more persuasive to an implementer than a paragraph of Markdown, so a stale sheet
quietly becomes the spec. Text that cannot be diffed, validated or corrected will drift from canon,
and several of these did.

[`Vanguards/render_sheet.py`](../../Docs/Design/Vanguards/render_sheet.py) now composes every sheet
from canon at render time and fills each image slot from a file, so the artwork and the text are
produced separately and the text can never go stale against the bible.

Two of the filenames here also carry **superseded character names** — `Tavra_The_Harbor_Gun` is
Bryn, and `Vitra_The_Last_Light` is Oriel. They are left as-is rather than renamed, because the
register and the purge runbook both reference these exact paths.

## Third-party material — mostly resolved, one step outstanding

Three sheets contained third-party branding and artwork, and this repository is **public**:

- `Raska_The_Redline_Character_Sheet_v2.png`
- `Kade_Dead_Reckoning_Character_Sheet.png`
- `Angeru_The_Housebreaker_Character_Sheet.png`

All three were deleted from the working tree and **removed from Git history**. The rewrite was
executed and verified against the live remote on 2026-09-20; the procedure and its verification
steps are recorded in
[`Docs/Runbooks/purge-third-party-sheets.md`](../../Docs/Runbooks/purge-third-party-sheets.md).

**One step remains and it is not something a client can do.** GitHub owns `refs/pull/*`
server-side and read-only, so a force-push does not remove them. Until GitHub Support
garbage-collects the unreachable objects and drops the stale pull-request refs, the content is
still reachable by anyone who knows the ref. That request is step 6 of the runbook and is the
repository owner's to file.

All three Vanguards now have clean authored art in [`../Vanguards/`](../Vanguards/), built from
written canon with no third-party material. Their YAML entries record `sheet.status: withdrawn`.

## The sheets

22 sheets, one each for the Vanguards that had one before the withdrawal. Register references mark
the conflicts that were found; every one of them has since been resolved against the authored art.

| Vanguard | File | Noted conflict |
|---|---|---|
| Aurelisse | `Aurelisse_The_Open_Sky_Character_Sheet.png` | |
| Cairn | `Cairn_The_Rivers_Grasp_Character_Sheet.png` | |
| Celandrine | `Celandrine_The_Wayrunner_Character_Sheet.png` | |
| Eudora | `Eudora_Blackbridge_The_Fieldwright_Character_Sheet.png` | |
| Gorraveth | `Gorraveth_The_Slagmaw_Character_Sheet.png` | |
| Korruk | `Korruk_The_Splinterbeast_Character_Sheet.png` | origin contradicts canon — register §B1 |
| Marek | `Marek_The_Black_Accord_Character_Sheet.png` | |
| Mavra | `Mavra_The_Spillwright_Character_Sheet.png` | |
| Mimzi | `Mimzi_The_Pocket_Hex_Character_Sheet.png` | |
| Moro | `Moro_The_Wildspark_Character_Sheet_v2.png` | passive incomplete — register §B5 |
| Neris | `Neris_The_Tidebound_Character_Sheet.png` | |
| Patch | `Patch_The_Last_Hug_Character_Sheet.png` | passive contradicts canon — register §B4 |
| Qazharr | `Qazharr_The_Harbor_Wolf_Character_Sheet.png` | |
| Relay | `Relay_The_Last_Conductor_Character_Sheet.png` | |
| Silt | `Silt_Character_Sheet.png` | pronouns contradict canon — register §B8 |
| Sylra | `Sylra_The_Mistwarden_Character_Sheet.png` | |
| Tavi | `Tavi_The_Playmate_Character_Sheet.png` | |
| Bryn | `Tavra_The_Harbor_Gun_Character_Sheet.png` | Dense Fog behaviour — register §B2; former name |
| Torr | `Torr_The_Unreturned_Character_Sheet.png` | |
| Varkesh | `Varkesh_The_Forgeheart_Character_Sheet.png` | |
| Vera | `Vera_The_Last_Volley_Character_Sheet.png` | |
| Oriel | `Vitra_The_Last_Light_Character_Sheet.png` | former name and title — register §C |
