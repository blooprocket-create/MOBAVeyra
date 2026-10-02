# Ability icons

Icons for every Vanguard's passive and Q, W, E and R, the recasts and other abilities a slot comes to hold, and the five Flux Spells on the current roster (Blink, Mend, Scorch, Enfeeble, Wildstrike): 157 in all. The author supplied the first 25 on 2026-09-29 and the updated set, which redraws those and adds the rest, on 2026-10-01.

- **Files:** `Flux_Spells/` and `Vanguards/<Name>/`, each `T_<ability_id>_Icon.png`, where the ID is the runtime one in `Game/Tuning/Abilities.json` and `Vanguards.json`. They are 256 × 256 px, opaque RGB on a dark charcoal ground.
- **Names that differ from the art's:** the pack names some icons for what they show, and the files take the runtime ID instead:
  - Bryn's passive is shown as "Every Shot Counts", but its ID is `bryn_breach`;
  - "Don't Leave Me" and "You're It!" are `patch_dont_leave_me` and `tavi_youre_it`;
  - Tavi's "Again" and "Next Playmate" recasts are `tavi_tag_again` and `tavi_ready_or_not_again`;
  - Torr's "Rip Free" is `torr_anchor_rip`;
  - Neris's Calm Crash is `neris_wave_crash`, and her Little Current redirects are `neris_little_current_redirect` and `neris_little_current_storm_redirect`.
- **State art:** `../SkillStates/<Name>/T_<art_id>_Icon.png` holds 15 icons for states of an ability that has one runtime ID, kept for when the HUD can show an ability's state:
  - Neris's Calm forms, the Storm Crash, both Change the Weather switches and Sea State;
  - Angeru's Blade and Veil;
  - Raska's Redlined riding moves and Roadhouse;
  - Tavi's Hide burst;
  - Eudora's Gun Platform.

  They stay outside this folder so that `BuildIconArt.ps1` never imports them.
- **No UI baked in:** slot keys, cooldowns, selection and disabled states are drawn by the game.
- **Index:** `Contact_Sheets/` holds a labelled preview per Vanguard and one for the Flux Spells; the labels exist only there.
- **Provenance:**
  - `manifest.csv` and `manifest.json` list each icon's runtime ID (empty for state art), art ID, name, slot, variant, file and SHA-256.
  - `generation_prompts.json` records how each icon was made, from the Character Bible and the Vanguards' approved hero art.
  - `validation.json` counts the icons and names any kit ability or Flux Spell without one; only the test fixture `test_bolt` has none.
- **Status:** these are newly generated designs, not previously approved visual canon.

## Into the game

`Game/Scripts/BuildIconArt.ps1 -Kind Abilities` imports them as UI textures, `/Game/Veyra/UI/Abilities/T_<ability_id>_Icon`, which the HUD's deck, champion select and the shop's Flux Spells show (`VeyraShellArt::AbilityIconOf`). The committed textures are lockable Git LFS files: lock one before importing a new version of it.
