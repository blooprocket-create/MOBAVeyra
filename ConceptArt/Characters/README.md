# Veyra Character Concept Art

This directory is the repository home for current Vanguard character-sheet concept art.

## These sheets are reference, not specification

Character sheets are **visual development references**. Designs, ability layouts, costume details, wording and presentation may all evolve during production.

**Canonical text design lives in [`Docs/Design/`](../../Docs/Design/). Where a sheet and a design bible disagree, the bible wins.**

This matters more than it sounds. A finished-looking sheet is far more persuasive to an implementer than a paragraph of Markdown, so a stale sheet quietly becomes the spec unless the disagreement is written down. Several sheets in this directory currently contradict canon on ability names, Dense Fog behaviour, displacement rules, passives and character origins.

Every known conflict is catalogued in
**[`Docs/Design/Sheet_Canon_Discrepancy_Register_v0.1.md`](../../Docs/Design/Sheet_Canon_Discrepancy_Register_v0.1.md)** —
read it before using any sheet in this directory as input to modelling, VFX, ability implementation or balance work.

Specifically, do not take from a sheet:

- ability names or ability behaviour;
- role or lane assignments (the roster is deliberately **not** organised around mandatory role slots);
- vision, stealth, displacement or crowd-control rules — the Vision and Combat Bibles own those;
- lore facts, origins, species or regional attribution;
- **any numeric value.** Tuning belongs in validated designer-editable data under `ARCHITECTURE.md` §1.3, never in artwork.

## Third-party material — action required

Three sheets in this directory contain third-party branding and third-party character/item artwork, and this repository is **public**:

- `Raska_The_Redline_Character_Sheet_v2.png`
- `Kade_Dead_Reckoning_Character_Sheet.png`
- `Angeru_The_Housebreaker_Character_Sheet.png`

These are **withdrawn from the index below** and must not be used as reference, published, or presented as Veyra material in their current form. Details of exactly what each contains are in section A of the discrepancy register.

Required, in order:

1. Regenerate all three using Veyra-owned visual material only.
2. Remove the originals **from Git history**, not just from the working tree. Deleting a file in a new commit leaves it retrievable in every existing clone and fork of a public repository; history rewriting (for example `git-filter-repo`) plus a coordinated force-push is required.

Until step 2 completes, the files remain publicly retrievable regardless of what the working tree contains.

## Current sheet set

22 sheets remain in active reference use, one each for 22 of the 25 Vanguards in the current design roster. Raska, Kade and Angeru have no usable sheet pending regeneration. Moro's filename carries a `_v2` suffix from an earlier revision pass.

- [Aurelisse — The Open Sky](./Aurelisse_The_Open_Sky_Character_Sheet.png)
- [Cairn — The River's Grasp](./Cairn_The_Rivers_Grasp_Character_Sheet.png)
- [Celandrine — The Wayrunner](./Celandrine_The_Wayrunner_Character_Sheet.png)
- [Eudora Blackbridge — The Fieldwright](./Eudora_Blackbridge_The_Fieldwright_Character_Sheet.png)
- [Gorraveth — The Slagmaw](./Gorraveth_The_Slagmaw_Character_Sheet.png)
- [Korruk — The Splinterbeast](./Korruk_The_Splinterbeast_Character_Sheet.png) — *origin contradicts canon, see register §B1*
- [Marek — The Black Accord (+ Nix)](./Marek_The_Black_Accord_Character_Sheet.png)
- [Mavra — The Spillwright](./Mavra_The_Spillwright_Character_Sheet.png)
- [Mimzi — The Pocket Hex](./Mimzi_The_Pocket_Hex_Character_Sheet.png)
- [Moro — The Wildspark](./Moro_The_Wildspark_Character_Sheet_v2.png) — *passive incomplete, see register §B5*
- [Neris — The Tidebound](./Neris_The_Tidebound_Character_Sheet.png)
- [Patch — The Last Hug](./Patch_The_Last_Hug_Character_Sheet.png) — *passive contradicts canon, see register §B4*
- [Qazharr — The Harbor Wolf](./Qazharr_The_Harbor_Wolf_Character_Sheet.png)
- [Relay — The Last Conductor](./Relay_The_Last_Conductor_Character_Sheet.png)
- [Silt](./Silt_Character_Sheet.png)
- [Sylra — The Mistwarden](./Sylra_The_Mistwarden_Character_Sheet.png)
- [Tavi — The Playmate](./Tavi_The_Playmate_Character_Sheet.png)
- [Tavra — The Harbor Gun](./Tavra_The_Harbor_Gun_Character_Sheet.png) — *Dense Fog behaviour contradicts canon, see register §B2*
- [Torr — The Unreturned](./Torr_The_Unreturned_Character_Sheet.png)
- [Varkesh — The Forgeheart](./Varkesh_The_Forgeheart_Character_Sheet.png)
- [Vera — The Last Volley](./Vera_The_Last_Volley_Character_Sheet.png)
- [Vitra — The Last Light](./Vitra_The_Last_Light_Character_Sheet.png)

### Withdrawn pending clean regeneration

- Angeru — The Housebreaker
- Kade — Dead Reckoning
- Raska — The Redline

## Sheet template

Three layout generations are currently in circulation. The most recent — used by Mimzi, Celandrine, Gorraveth, Aurelisse and Eudora Blackbridge — is the **standard template for new and regenerated sheets**. It is the only generation carrying a species/region taxonomy, an explicit weaknesses list, in-game view mockups and a colour palette.

Older sheets should be retrofitted to it as art budget allows. Sub-brand lines and difficulty scales currently vary sheet to sheet and should be unified at the same time; see register §F.
