# Large-document section locators

Start with [Veyra's agent context map](../CONTEXT_MAP.md), select the single owning bible, then open **only that bible's map** below. Each locator is generated from the current source headings and gives the line at which the relevant section begins. It is *not* a second copy of the rules.

- [Initial Roster Character Bible — find one Vanguard and its individual abilities](sections/Veyra_Initial_Roster_Character_Bible_v0.6.md)
- [Combat Bible — find individual combat semantics and ride-state rules](sections/Veyra_Combat_Bible_v0.5.md)
- [Battleground Bible — find map, jungle, waves, objectives and towers](sections/Veyra_Battleground_Bible_v0.9.md)
- [Pre-Game Client UX Bible — find proposal groups 1–92 and current client behavior](sections/Veyra_Pre_Game_Client_UX_Bible_v0.1.md)
- [Settings & Accessibility Bible — find a particular settings category and proposal](sections/Veyra_Settings_Accessibility_Bible_v0.1.md)
- [Client & Platform Bible — find unified Unreal state transitions and ownership](sections/Veyra_Client_Platform_Bible_v0.1.md)

For other bibles, read the short introductory scope and search headings in the selected file, e.g. `rg -n '^#{1,3} ' Docs/Design/<selected-file>.md` (quote the filename), then read only the needed range.

**Before committing documentation edits:** `python3 scripts/check_doc_context.py --write` regenerates all six locators; `python3 scripts/check_doc_context.py --check` checks routes and that committed maps exactly match source headings and line numbers. The repository CI runs the check. When introducing a new active bible or superseding a version, update `Docs/CONTEXT_MAP.md`, the generator's mapped filenames if applicable, and the links on this page.

Line links use GitHub's `?plain=1#L<number>` source viewer. In a local checkout, use the listed line and fetch only a bounded range. Do not infer the entire rule from a heading alone.
