# Ability icons

Icons for the five Flux Spells on the current roster (Blink, Mend, Scorch, Enfeeble, Wildstrike) and for the passive and Q, W, E and R of Cairn, Qazharr, Oriel and Bryn: 25 in all. The author supplied them on 2026-09-29.

- **Files:** `Flux_Spells/` and `Vanguards/<Name>/`, each `T_<ability_id>_Icon.png`, where the ID is the runtime one in `Game/Tuning/Abilities.json` and `Vanguards.json`. They are 256 × 256 px, opaque RGB on a dark charcoal ground. Bryn's passive is displayed as "Every Shot Counts" but its ID is `bryn_breach`.
- **No UI baked in:** slot keys, cooldowns, selection and disabled states are drawn by the game.
- **Index:** `contact_sheet.jpg` is a labelled preview; the labels exist only there.
- **Provenance:** `manifest.csv` and `manifest.json` list each ability's ID, name and slot; their `file` and `sha256` fields describe the author's 512 px exports, which the author keeps. `generation_prompts.json` records how each icon was made, from the Character Bible and the Vanguards' approved hero art. `validation.json` is the pack's coverage check.
- **Status:** these are newly generated designs, not previously approved visual canon.

## Into the game

`Game/Scripts/BuildIconArt.ps1 -Kind Abilities` imports them as UI textures, `/Game/Veyra/UI/Abilities/T_<ability_id>_Icon`, which the HUD's deck, champion select and the shop's Flux Spells show (`VeyraShellArt::AbilityIconOf`). An ability with no icon shows its initials, as the other six Vanguards' do until their icons are drawn.
